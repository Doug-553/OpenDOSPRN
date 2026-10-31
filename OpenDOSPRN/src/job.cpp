#include "ui.h"

static std::vector<Page> g_lastPages;
static ws g_lastSource;
static Settings g_lastSettings;
static bool g_haveLast = false;

static void MsgBox(HWND o, const ws& text, UINT icon = MB_ICONINFORMATION) {
  MessageBoxW(o, text.c_str(), L"OpenDOSPRN", MB_OK | icon);
}

static void EnsureDir(const ws& dir) {
  ws cur;
  for (size_t i = 0; i < dir.size(); i++) {
    cur += dir[i];
    if ((dir[i] == L'\\' || i + 1 == dir.size()) && cur.size() > 3) CreateDirectoryW(cur.c_str(), nullptr);
  }
}

static bool Output(HWND owner, const Settings& s, const std::vector<Page>& pages, const std::vector<int>& sel, const ws& src) {
  if (!s.toPrinter && !s.toPdf) { MsgBox(owner, lang::T("NoOutput"), MB_ICONWARNING); return false; }
  bool ok = true;
  if (s.toPrinter) {
    ws err;
    PrinterSel ps;
    ps.name = s.printerName;
    ps.devmode = s.devmode;
    if (!PrintPages(s, ps, pages, sel, src.empty() ? L"DOS job" : src, err)) { MsgBox(owner, err, MB_ICONERROR); ok = false; }
  }
  if (s.toPdf) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    ws name = ExpandPdfPattern(s.pdfPattern, src.empty() ? L"prnspl" : src, t);
    ws folder = s.pdfFolder.empty() ? util::DocumentsDir() : util::ExpandEnv(s.pdfFolder);
    ws path;
    if (s.pdfAutoSave) {
      EnsureDir(folder);
      if (!folder.empty() && folder.back() != L'\\') folder += L'\\';
      path = folder + name;
    } else {
      wchar_t buf[MAX_PATH * 2] = {};
      wcsncpy(buf, name.c_str(), MAX_PATH * 2 - 1);
      OPENFILENAMEW of = {};
      of.lStructSize = sizeof of;
      of.hwndOwner = owner;
      static const wchar_t filt[] = L"PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0";
      of.lpstrFilter = filt;
      of.lpstrFile = buf;
      of.nMaxFile = MAX_PATH * 2;
      ws title = lang::T("SavePdfTitle");
      of.lpstrTitle = title.c_str();
      of.lpstrDefExt = L"pdf";
      ws initDir = folder;
      of.lpstrInitialDir = initDir.c_str();
      of.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
      if (!GetSaveFileNameW(&of)) return ok;
      path = buf;
    }
    double pw, ph;
    PageDims(s, pw, ph);
    if (SavePdf(path, pages, sel, MakeRenderStyle(s), pw, ph)) {
      if (s.pdfOpen) ShellExecuteW(owner, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
      MsgBox(owner, lang::T("PdfError") + L"\n" + path, MB_ICONERROR);
      ok = false;
    }
  }
  return ok;
}

void RunJob(HWND owner, const std::string& data, const ws& src, const Settings& s) {
  if (data.empty()) { MsgBox(owner, lang::T("NothingToPrint")); return; }
  if (s.rawMode && s.toPrinter) {
    ws pr = s.printerName.empty() ? DefaultPrinterName() : s.printerName;
    if (pr.empty()) { MsgBox(owner, lang::T("NoPrinters"), MB_ICONWARNING); return; }
    if (!PrintRaw(pr, data, s.copies, src.empty() ? L"DOS job" : src))
      MsgBox(owner, util::Fmt(lang::T("PrinterNotAccessible").c_str(), pr.c_str()), MB_ICONERROR);
    return;
  }
  std::vector<Page> pages = BuildPages(s, data);
  if (pages.empty() || (pages.size() == 1 && pages[0].Empty())) { MsgBox(owner, lang::T("NothingToPrint")); return; }
  g_lastPages = pages;
  g_lastSource = src;
  g_lastSettings = s;
  g_haveLast = true;
  if (s.preview) {
    PreviewIO io;
    io.pages = &g_lastPages;
    io.set = &g_lastSettings;
    io.sourceName = src;
    if (!ShowPreview(owner, io)) return;
    Output(owner, g_lastSettings, g_lastPages, io.sel, src);
  } else {
    std::vector<int> all;
    for (int i = 0; i < (int)pages.size(); i++) all.push_back(i);
    Output(owner, s, pages, all, src);
  }
}

bool HaveLastPreview() { return g_haveLast; }

void ShowLastPreview(HWND owner) {
  if (!g_haveLast) return;
  PreviewIO io;
  io.pages = &g_lastPages;
  io.set = &g_lastSettings;
  io.sourceName = g_lastSource;
  if (ShowPreview(owner, io)) Output(owner, g_lastSettings, g_lastPages, io.sel, g_lastSource);
}

// ASCII table: the 256 characters of the selected code page, printed through the same pipeline
void ShowAsciiTable(HWND owner) {
  std::string raw;
  if (!util::ReadFileBytes(util::AppDir() + L"ASCII.TBL", raw)) {
    raw.clear();
    for (int row = 0; row < 16; row++) {
      for (int col = 0; col < 16; col++) {
        int c = row * 16 + col;
        raw += (c < 32 || c == 127) ? ' ' : (char)c;
      }
      raw += "\r\n";
    }
  }
  RunJob(owner, raw, L"ASCII.TBL", g_set);
}
