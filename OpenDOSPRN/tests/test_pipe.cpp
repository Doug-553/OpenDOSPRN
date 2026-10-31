#include <windows.h>
#include <string>
int wmain(int argc, wchar_t** argv) {
  std::wstring port = argc > 1 ? argv[1] : L"LPT1";
  std::string d = "Job da porta ";
  for (wchar_t c : port) d += (char)c;
  d += "\r\n\x1b" "EEm negrito\x1b" "F\r\nAcentua\x87\x84o\r\n";
  std::wstring name = L"\\\\.\\pipe\\OpenDOSPRN_" + port;
  HANDLE h = INVALID_HANDLE_VALUE;
  for (int i = 0; i < 20 && h == INVALID_HANDLE_VALUE; i++) {
    h = CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) Sleep(500);
  }
  if (h == INVALID_HANDLE_VALUE) return 2;
  DWORD n; WriteFile(h, d.data(), (DWORD)d.size(), &n, nullptr);
  CloseHandle(h);
  return 0;
}
