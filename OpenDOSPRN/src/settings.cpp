#include "settings.h"

Settings g_set;

static const wchar_t* kRegKey = L"Software\\OpenDOSPRN";

ws PortLabel(int i) {
  if (i >= 0 && i < 9) return util::Fmt(L"LPT%d", i + 1);
  if (i >= 9 && i < 12) return util::Fmt(L"COM%d", i - 8);
  return L"LPT1";
}

Settings g_profiles[12];

void InitProfiles() {
  for (int i = 0; i < 12; i++) {
    Settings p;
    p.spoolName = L"%userprofile%\\prnspl_" + PortLabel(i);
    p.capture = (i == 0);
    g_profiles[i] = p;
  }
}

void Settings::SerializeGlobal(Archive& a) {
  a.i(L"Port", portIndex);
  a.b(L"RawPrintingModeAlert", rawAlertAsked);
  a.b(L"Minimize", runMinimized);
  a.b(L"ConfirmExit", confirmExit);
  a.s(L"Language", language);
  a.b(L"MonoSpaceOnly", monoOnly);
  ws names;
  if (!a.loading())
    for (size_t k = 0; k < schemes.size(); k++) names += (k ? L"|" : L"") + schemes[k].name;
  a.s(L"SchemeList", names);
  if (a.loading()) {
    schemes.clear();
    size_t p = 0;
    while (p <= names.size() && !names.empty()) {
      size_t q = names.find(L'|', p);
      ws n = names.substr(p, q == ws::npos ? ws::npos : q - p);
      if (!n.empty()) { SeqScheme sc; sc.name = n; schemes.push_back(sc); }
      if (q == ws::npos) break;
      p = q + 1;
    }
  }
  for (auto& sc : schemes) {
    a.s((L"Seq." + sc.name + L".Init").c_str(), sc.init);
    for (int f = 0; f < kSeqFuncCount; f++) a.s(util::Fmt(L"Seq.%s.F%d", sc.name.c_str(), f).c_str(), sc.codes[f]);
  }
  if (portIndex < 0 || portIndex > 11) portIndex = 0;
}

void Settings::SerializePort(Archive& a) {
  a.s(L"SpoolName", spoolName);
  a.i(L"Timeout", timeoutSec);
  a.b(L"AutoPrint", autoPrint);
  a.b(L"Capture", capture);
  a.b(L"Landscape", landscape);
  a.i(L"PaperSize", paperSize);
  a.b(L"WordWrap", wrapLong);
  a.b(L"FormFeed", useFF);
  a.b(L"LFasCRLF", lfAsCrLf);
  a.b(L"CRasCRLF", crAsCrLf);
  a.s(L"Printer", printerName);
  a.i(L"Copies", copies);
  a.b(L"RawPrintingMode", rawMode);
  a.b(L"EmulateSequences", emulateSeq);
  a.s(L"Sequences", seqScheme);
  a.b(L"PrintPreview", preview);
  a.b(L"ToPrinter", toPrinter);
  a.b(L"ToPDF", toPdf);
  a.s(L"PdfFolder", pdfFolder);
  a.s(L"PdfPattern", pdfPattern);
  a.b(L"PdfAutoSave", pdfAutoSave);
  a.b(L"PdfOpen", pdfOpen);
  a.s(L"FontName", fontName);
  a.i(L"FontSize", fontSize);
  a.b(L"Bold", bold);
  a.b(L"Italic", italic);
  a.b(L"Underline", underline);
  a.b(L"Strikeout", strike);
  a.d(L"CPI", cpi);
  a.d(L"LPI", lpi);
  a.i(L"TopMargin", mTop);
  a.i(L"BottomMargin", mBottom);
  a.i(L"LeftMargin", mLeft);
  a.i(L"RightMargin", mRight);
  a.s(L"CodePage", codepage);
  if (cpi < 5 || cpi > 60) cpi = 10;
  if (lpi < 1 || lpi > 30) lpi = 6;
  if (copies < 1) copies = 1;
  if (timeoutSec < 1) timeoutSec = 1;
}

void Settings::TakePortFields(const Settings& s) {
  Settings keep = *this;
  *this = s;
  portIndex = keep.portIndex;
  rawAlertAsked = keep.rawAlertAsked;
  runMinimized = keep.runMinimized;
  confirmExit = keep.confirmExit;
  language = keep.language;
  monoOnly = keep.monoOnly;
  schemes = keep.schemes;
}

Settings Settings::ForPort(int i) const {
  if (i == portIndex) return *this;
  Settings r = *this;
  r.TakePortFields(g_profiles[i]);
  r.portIndex = i;
  return r;
}

const SeqScheme* Settings::FindScheme(const ws& name) const {
  for (auto& s : schemes) if (_wcsicmp(s.name.c_str(), name.c_str()) == 0) return &s;
  return nullptr;
}

// ---------- registry archive ----------
namespace {
struct RegArchive : Archive {
  HKEY key; bool load;
  RegArchive(HKEY k, bool l) : key(k), load(l) {}
  bool loading() const override { return load; }
  void s(const wchar_t* n, ws& v) override {
    if (load) {
      DWORD type = 0, sz = 0;
      if (RegQueryValueExW(key, n, nullptr, &type, nullptr, &sz) == ERROR_SUCCESS && type == REG_SZ) {
        ws t(sz / 2 + 1, L'\0');
        if (RegQueryValueExW(key, n, nullptr, &type, (LPBYTE)&t[0], &sz) == ERROR_SUCCESS) {
          t.resize(wcslen(t.c_str()));
          v = t;
        }
      }
    } else
      RegSetValueExW(key, n, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * 2));
  }
  void i(const wchar_t* n, int& v) override {
    if (load) {
      DWORD type = 0, sz = 4, d = 0;
      if (RegQueryValueExW(key, n, nullptr, &type, (LPBYTE)&d, &sz) == ERROR_SUCCESS && type == REG_DWORD) v = (int)d;
    } else {
      DWORD d = (DWORD)v;
      RegSetValueExW(key, n, 0, REG_DWORD, (const BYTE*)&d, 4);
    }
  }
  void b(const wchar_t* n, bool& v) override { int t = v; i(n, t); if (load) v = t != 0; }
  void d(const wchar_t* n, double& v) override {
    ws t = util::Fmt(L"%.4f", v);
    if (load) { ws tmp = t; s(n, tmp); if (tmp != t) v = util::ParseDouble(tmp, v); }
    else s(n, t);
  }
};

struct IniArchive : Archive {
  ws file, sec; bool load;
  IniArchive(const ws& f, const ws& s, bool l) : file(f), sec(s), load(l) {}
  bool loading() const override { return load; }
  void s(const wchar_t* n, ws& v) override {
    if (load) {
      wchar_t buf[4096];
      DWORD r = GetPrivateProfileStringW(sec.c_str(), n, L"\x01", buf, 4096, file.c_str());
      if (!(r == 1 && buf[0] == 1)) v = buf;
    } else {
      ws q = L"\"" + v + L"\"";   // keep leading/trailing blanks
      WritePrivateProfileStringW(sec.c_str(), n, q.c_str(), file.c_str());
    }
  }
  void i(const wchar_t* n, int& v) override {
    if (load) v = (int)GetPrivateProfileIntW(sec.c_str(), n, v, file.c_str());
    else { ws t = util::Fmt(L"%d", v); WritePrivateProfileStringW(sec.c_str(), n, t.c_str(), file.c_str()); }
  }
  void b(const wchar_t* n, bool& v) override { int t = v; i(n, t); if (load) v = t != 0; }
  void d(const wchar_t* n, double& v) override {
    if (load) {
      wchar_t buf[64];
      GetPrivateProfileStringW(sec.c_str(), n, L"", buf, 64, file.c_str());
      v = util::ParseDouble(buf, v);
    } else { ws t = util::Fmt(L"%.4f", v); WritePrivateProfileStringW(sec.c_str(), n, t.c_str(), file.c_str()); }
  }
};
}  // namespace

void Settings::Load() {
  InitProfiles();
  HKEY k;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &k) != ERROR_SUCCESS) { TakePortFields(g_profiles[portIndex]); return; }
  RegArchive a(k, true);
  SerializeGlobal(a);
  HKEY pk;
  if (RegOpenKeyExW(k, L"Ports", 0, KEY_READ, &pk) == ERROR_SUCCESS) {
    for (int i = 0; i < 12; i++) {
      HKEY sk;
      if (RegOpenKeyExW(pk, PortLabel(i).c_str(), 0, KEY_READ, &sk) == ERROR_SUCCESS) {
        RegArchive pa(sk, true);
        g_profiles[i].SerializePort(pa);
        RegCloseKey(sk);
      }
    }
    RegCloseKey(pk);
  } else {
    // settings of the single-port version: use them as the template of every port
    Settings base = g_profiles[0];
    base.SerializePort(a);
    bool wasCapturing = base.capture;
    ws oldSpool = base.spoolName;
    for (int i = 0; i < 12; i++) {
      g_profiles[i] = base;
      if (oldSpool == L"%userprofile%\\prnspl") g_profiles[i].spoolName = oldSpool + L"_" + PortLabel(i);
      g_profiles[i].capture = wasCapturing && i == portIndex;
    }
  }
  RegCloseKey(k);
  TakePortFields(g_profiles[portIndex]);
}
void Settings::Save() const {
  Settings* self = const_cast<Settings*>(this);
  g_profiles[portIndex].TakePortFields(*self);
  HKEY k;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
  RegArchive a(k, false);
  self->SerializeGlobal(a);
  HKEY pk;
  if (RegCreateKeyExW(k, L"Ports", 0, nullptr, 0, KEY_WRITE, nullptr, &pk, nullptr) == ERROR_SUCCESS) {
    for (int i = 0; i < 12; i++) {
      HKEY sk;
      if (RegCreateKeyExW(pk, PortLabel(i).c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &sk, nullptr) == ERROR_SUCCESS) {
        RegArchive pa(sk, false);
        g_profiles[i].SerializePort(pa);
        RegCloseKey(sk);
      }
    }
    RegCloseKey(pk);
  }
  RegCloseKey(k);
}
bool Settings::LoadIni(const ws& file) {
  if (!util::FileExists(file)) return false;
  IniArchive a(file, L"Settings", true);
  SerializeGlobal(a);
  for (int i = 0; i < 12; i++) {
    IniArchive pa(file, L"Port." + PortLabel(i), true);
    g_profiles[i].SerializePort(pa);
  }
  TakePortFields(g_profiles[portIndex]);
  return true;
}
bool Settings::SaveIni(const ws& file) const {
  Settings* self = const_cast<Settings*>(this);
  g_profiles[portIndex].TakePortFields(*self);
  IniArchive a(file, L"Settings", false);
  self->SerializeGlobal(a);
  for (int i = 0; i < 12; i++) {
    IniArchive pa(file, L"Port." + PortLabel(i), false);
    g_profiles[i].SerializePort(pa);
  }
  return true;
}
