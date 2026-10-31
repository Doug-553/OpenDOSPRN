#include "printing.h"
#include "lang.h"

std::vector<ws> EnumPrinterNames() {
  std::vector<ws> r;
  DWORD need = 0, cnt = 0;
  EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 4, nullptr, 0, &need, &cnt);
  if (!need) return r;
  std::vector<BYTE> buf(need);
  if (!EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 4, buf.data(), need, &need, &cnt)) return r;
  auto* pi = (PRINTER_INFO_4W*)buf.data();
  for (DWORD i = 0; i < cnt; i++) r.push_back(pi[i].pPrinterName);
  return r;
}

ws DefaultPrinterName() {
  DWORD n = 0;
  GetDefaultPrinterW(nullptr, &n);
  if (!n) return ws();
  ws s(n, L'\0');
  if (!GetDefaultPrinterW(&s[0], &n)) return ws();
  s.resize(wcslen(s.c_str()));
  return s;
}

bool SelectPrinterDialog(HWND owner, PrinterSel& sel) {
  PRINTDLGW pd = {};
  pd.lStructSize = sizeof pd;
  pd.hwndOwner = owner;
  pd.Flags = PD_PRINTSETUP;
  if (!PrintDlgW(&pd)) return false;
  bool ok = false;
  if (pd.hDevNames) {
    auto* dn = (DEVNAMES*)GlobalLock(pd.hDevNames);
    if (dn) {
      sel.name = (wchar_t*)dn + dn->wDeviceOffset;
      GlobalUnlock(pd.hDevNames);
      ok = true;
    }
    GlobalFree(pd.hDevNames);
  }
  sel.devmode.clear();
  if (pd.hDevMode) {
    auto* dm = (DEVMODEW*)GlobalLock(pd.hDevMode);
    if (dm) {
      sel.devmode.assign((BYTE*)dm, (BYTE*)dm + dm->dmSize + dm->dmDriverExtra);
      GlobalUnlock(pd.hDevMode);
    }
    GlobalFree(pd.hDevMode);
  }
  return ok;
}

bool PrintRaw(const ws& printer, const std::string& data, int copies, const ws& doc) {
  HANDLE h = nullptr;
  if (!OpenPrinterW((LPWSTR)printer.c_str(), &h, nullptr)) return false;
  bool ok = true;
  for (int c = 0; c < std::max(1, copies) && ok; c++) {
    DOC_INFO_1W di = {};
    di.pDocName = (LPWSTR)doc.c_str();
    di.pDatatype = (LPWSTR)L"RAW";
    if (!StartDocPrinterW(h, 1, (LPBYTE)&di)) { ok = false; break; }
    StartPagePrinter(h);
    size_t off = 0;
    while (off < data.size()) {
      DWORD wr = 0, chunk = (DWORD)std::min<size_t>(data.size() - off, 32768);
      if (!WritePrinter(h, (LPVOID)(data.data() + off), chunk, &wr) || !wr) { ok = false; break; }
      off += wr;
    }
    EndPagePrinter(h);
    EndDocPrinter(h);
  }
  ClosePrinter(h);
  return ok;
}

bool ParsePageRange(const ws& text, int total, std::vector<int>& out) {
  out.clear();
  ws t = util::Trim(text);
  if (t.empty()) {
    for (int i = 0; i < total; i++) out.push_back(i);
    return true;
  }
  std::set<int> sel;
  size_t p = 0;
  while (p <= t.size()) {
    size_t q = t.find_first_of(L",;", p);
    ws part = util::Trim(t.substr(p, q == ws::npos ? ws::npos : q - p));
    if (!part.empty()) {
      size_t dash = part.find(L'-');
      int a, b;
      if (dash == ws::npos) {
        a = b = _wtoi(part.c_str());
      } else {
        ws l = util::Trim(part.substr(0, dash)), r = util::Trim(part.substr(dash + 1));
        a = l.empty() ? 1 : _wtoi(l.c_str());
        b = r.empty() ? total : _wtoi(r.c_str());
      }
      if (a < 1 || b < a) return false;
      for (int i = a; i <= b && i <= total; i++) sel.insert(i - 1);
    }
    if (q == ws::npos) break;
    p = q + 1;
  }
  out.assign(sel.begin(), sel.end());
  return !out.empty();
}

bool PrintPages(const Settings& s, const PrinterSel& sel, const std::vector<Page>& pages,
                const std::vector<int>& pageIdx, const ws& doc, ws& error) {
  ws name = sel.name.empty() ? DefaultPrinterName() : sel.name;
  if (name.empty()) { error = lang::T("NoPrinters"); return false; }

  std::vector<BYTE> dmbuf = sel.devmode;
  if (dmbuf.empty()) {
    HANDLE hp = nullptr;
    if (OpenPrinterW((LPWSTR)name.c_str(), &hp, nullptr)) {
      LONG sz = DocumentPropertiesW(nullptr, hp, (LPWSTR)name.c_str(), nullptr, nullptr, 0);
      if (sz > 0) {
        dmbuf.resize(sz);
        if (DocumentPropertiesW(nullptr, hp, (LPWSTR)name.c_str(), (DEVMODEW*)dmbuf.data(), nullptr, DM_OUT_BUFFER) != IDOK)
          dmbuf.clear();
      }
      ClosePrinter(hp);
    }
  }
  DEVMODEW* dm = dmbuf.empty() ? nullptr : (DEVMODEW*)dmbuf.data();
  if (dm) {
    dm->dmFields |= DM_ORIENTATION | DM_PAPERSIZE | DM_COPIES;
    dm->dmOrientation = s.landscape ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
    dm->dmPaperSize = (short)s.paperSize;
    dm->dmCopies = (short)std::max(1, s.copies);
  }
  HDC dc = CreateDCW(L"WINSPOOL", name.c_str(), nullptr, dm);
  if (!dc) { error = util::Fmt(lang::T("PrinterNotAccessible").c_str(), name.c_str()); return false; }

  int driverCopies = (int)DeviceCapabilitiesW(name.c_str(), nullptr, DC_COPIES, nullptr, dm);
  int loops = (s.copies > 1 && driverCopies <= 1) ? s.copies : 1;
  DOCINFOW di = {};
  di.cbSize = sizeof di;
  di.lpszDocName = doc.c_str();
  if (StartDocW(dc, &di) <= 0) { DeleteDC(dc); error = lang::T("PrintError"); return false; }
  double dpiX = GetDeviceCaps(dc, LOGPIXELSX), dpiY = GetDeviceCaps(dc, LOGPIXELSY);
  int offX = GetDeviceCaps(dc, PHYSICALOFFSETX), offY = GetDeviceCaps(dc, PHYSICALOFFSETY);
  RenderStyle st = MakeRenderStyle(s);
  bool ok = true;
  for (int l = 0; l < loops && ok; l++) {
    for (int idx : pageIdx) {
      if (idx < 0 || idx >= (int)pages.size()) continue;
      if (StartPage(dc) <= 0) { ok = false; break; }
      RenderPage(dc, pages[idx], st, dpiX, dpiY, -offX, -offY);
      if (EndPage(dc) <= 0) { ok = false; break; }
    }
  }
  if (ok) EndDoc(dc); else AbortDoc(dc);
  DeleteDC(dc);
  if (!ok) error = lang::T("PrintError");
  return ok;
}
