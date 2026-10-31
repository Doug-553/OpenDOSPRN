// OpenDOSPRN - DOS printing support utility (clean re-implementation)
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winspool.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

typedef std::wstring ws;

namespace util {
ws Utf8ToW(const std::string& s);
std::string WToUtf8(const ws& s);
ws ExpandEnv(const ws& s);
ws AppDir();                       // directory of the exe, with trailing backslash
ws Fmt(const wchar_t* fmt, ...);
ws Trim(const ws& s);
ws Lower(ws s);
bool FileExists(const ws& path);
bool ReadFileBytes(const ws& path, std::string& out);
bool WriteFileBytes(const ws& path, const std::string& data);
ws FileBaseName(const ws& path);   // name without dir
ws FileExt(const ws& path);        // extension without dot
ws UserName();
ws DocumentsDir();
ws Mm(double v);                   // formatting helper
double ParseDouble(const ws& s, double def);
}  // namespace util
