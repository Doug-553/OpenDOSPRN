#include "codepage.h"

CodePage::CodePage() {
  for (int i = 0; i < 256; i++) { map[i] = (wchar_t)i; lead[i] = false; }
}

static bool ParseNum(const std::string& s, long& v) {
  if (s.empty()) return false;
  char* e = nullptr;
  if (s[0] == '$') v = strtol(s.c_str() + 1, &e, 16);
  else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) v = strtol(s.c_str() + 2, &e, 16);
  else v = strtol(s.c_str(), &e, 10);
  return e && *e == 0;
}

static std::vector<std::string> Lines(const std::string& d) {
  std::vector<std::string> r;
  size_t p = 0;
  while (p < d.size()) {
    size_t e = d.find('\n', p);
    if (e == std::string::npos) e = d.size();
    std::string l = d.substr(p, e - p);
    if (!l.empty() && l.back() == '\r') l.pop_back();
    r.push_back(l);
    p = e + 1;
  }
  return r;
}

// .cp format: "<byte>=<value>" or "<byte>..<byte>=<value>..<value>"; value decimal or $hex.
static void ParseCp(const std::string& data, CodePage& cp) {
  for (auto& l : Lines(data)) {
    if (l.size() < 3) continue;
    unsigned lo = (unsigned char)l[0], hi = lo;
    size_t eq = 1;
    if (l.size() >= 5 && l[1] == '.' && l[2] == '.') { hi = (unsigned char)l[3]; eq = 4; }
    if (l[eq] != '=') continue;
    std::string v = l.substr(eq + 1);
    size_t dd = v.find("..");
    long a = 0, b = 0;
    if (dd != std::string::npos) {
      if (!ParseNum(v.substr(0, dd), a) || !ParseNum(v.substr(dd + 2), b)) continue;
    } else {
      if (!ParseNum(v, a)) continue;
      b = a + (hi - lo);
    }
    for (unsigned c = lo; c <= hi; c++) cp.map[c] = (wchar_t)(a + (long)(c - lo));
  }
}

// .cpx format: "0xKK<TAB>0xUUUU" (KK may be a two-byte code)
static void ParseCpx(const std::string& data, CodePage& cp) {
  for (auto& l : Lines(data)) {
    size_t t = l.find('\t');
    if (t == std::string::npos) continue;
    long k = 0, u = 0;
    if (!ParseNum(l.substr(0, t), k) || !ParseNum(l.substr(t + 1), u)) continue;
    if (k < 0x100) cp.map[k] = (wchar_t)u;
    else { cp.dbcs[(uint16_t)k] = (wchar_t)u; cp.lead[(k >> 8) & 0xFF] = true; }
  }
}

bool CodePage::Load(const ws& name, CodePage& out) {
  out = CodePage();
  out.name = name;
  ws dir = util::AppDir();
  std::string raw;
  if (util::ReadFileBytes(dir + name + L".cp", raw)) { ParseCp(raw, out); return true; }
  if (util::ReadFileBytes(dir + name + L".cpx", raw)) { ParseCpx(raw, out); return true; }
  // fall back to the Windows code page of the same number
  int id = _wtoi(name.c_str());
  if (id > 0) {
    for (int c = 0; c < 256; c++) {
      char b = (char)c;
      wchar_t w = 0;
      if (MultiByteToWideChar(id, 0, &b, 1, &w, 1) == 1) out.map[c] = w;
    }
    return true;
  }
  return true;   // identity (Latin-1)
}

std::vector<std::pair<ws, ws>> LoadCodePageList() {
  std::vector<std::pair<ws, ws>> r;
  std::string raw;
  if (util::ReadFileBytes(util::AppDir() + L"codepages.lst", raw)) {
    for (auto& l : Lines(raw)) {
      size_t eq = l.find('=');
      if (eq == std::string::npos || eq == 0) continue;
      r.push_back({util::Utf8ToW(l.substr(0, eq)), util::Utf8ToW(l.substr(eq + 1))});
    }
  }
  if (r.empty()) {
    r.push_back({L"437", L"USA (DOS OEM)"});
    r.push_back({L"850", L"Western European (DOS OEM)"});
    r.push_back({L"860", L"Portuguese (DOS OEM)"});
    r.push_back({L"1252", L"Western European (Windows ANSI)"});
  }
  return r;
}
