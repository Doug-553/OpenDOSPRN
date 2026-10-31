#include "common.h"
#include <cstdarg>

namespace util {

ws Utf8ToW(const std::string& s) {
  if (s.empty()) return ws();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
  ws w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
  return w;
}
std::string WToUtf8(const ws& s) {
  if (s.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
  std::string r(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), &r[0], n, nullptr, nullptr);
  return r;
}
ws ExpandEnv(const ws& s) {
  DWORD n = ExpandEnvironmentStringsW(s.c_str(), nullptr, 0);
  if (!n) return s;
  ws r(n, L'\0');
  ExpandEnvironmentStringsW(s.c_str(), &r[0], n);
  if (!r.empty() && r.back() == L'\0') r.pop_back();
  return r;
}
ws AppDir() {
  wchar_t buf[MAX_PATH * 2];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
  ws p(buf, n);
  size_t k = p.find_last_of(L"\\/");
  return k == ws::npos ? ws() : p.substr(0, k + 1);
}
ws Fmt(const wchar_t* fmt, ...) {
  wchar_t buf[2048];
  va_list ap;
  va_start(ap, fmt);
  _vsnwprintf(buf, 2047, fmt, ap);
  va_end(ap);
  buf[2047] = 0;
  return buf;
}
ws Trim(const ws& s) {
  size_t a = 0, b = s.size();
  while (a < b && iswspace(s[a])) a++;
  while (b > a && iswspace(s[b - 1])) b--;
  return s.substr(a, b - a);
}
ws Lower(ws s) {
  for (auto& c : s) c = (wchar_t)towlower(c);
  return s;
}
bool FileExists(const ws& path) {
  DWORD a = GetFileAttributesW(path.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
bool ReadFileBytes(const ws& path, std::string& out) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  out.clear();
  char buf[65536];
  DWORD n;
  while (ReadFile(h, buf, sizeof buf, &n, nullptr) && n) out.append(buf, n);
  CloseHandle(h);
  return true;
}
bool WriteFileBytes(const ws& path, const std::string& data) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  size_t off = 0;
  bool ok = true;
  while (off < data.size()) {
    DWORD n = 0;
    DWORD chunk = (DWORD)std::min<size_t>(data.size() - off, 1 << 20);
    if (!WriteFile(h, data.data() + off, chunk, &n, nullptr) || !n) { ok = false; break; }
    off += n;
  }
  CloseHandle(h);
  return ok;
}
ws FileBaseName(const ws& path) {
  size_t k = path.find_last_of(L"\\/");
  return k == ws::npos ? path : path.substr(k + 1);
}
ws FileExt(const ws& path) {
  ws b = FileBaseName(path);
  size_t k = b.find_last_of(L'.');
  return k == ws::npos ? ws() : b.substr(k + 1);
}
ws UserName() {
  wchar_t buf[256];
  DWORD n = 256;
  if (GetUserNameW(buf, &n)) return buf;
  return L"user";
}
ws DocumentsDir() {
  wchar_t buf[MAX_PATH];
  if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, buf))) return buf;
  return L"C:\\";
}
double ParseDouble(const ws& s, double def) {
  ws t = Trim(s);
  for (auto& c : t) if (c == L',') c = L'.';
  if (t.empty()) return def;
  wchar_t* e = nullptr;
  double v = wcstod(t.c_str(), &e);
  if (e == t.c_str()) return def;
  return v;
}
}  // namespace util
