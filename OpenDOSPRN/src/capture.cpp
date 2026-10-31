#include "capture.h"

Capture::Capture() {}
Capture::~Capture() { Stop(); }

ws Capture::PortName(int i) {
  if (i >= 0 && i < 9) return util::Fmt(L"LPT%d", i + 1);
  if (i >= 9 && i < 12) return util::Fmt(L"COM%d", i - 8);
  return L"LPT1";
}

bool Capture::Map(bool on, ws& error) {
  ws target = L"\\Device\\NamedPipe\\" + pipeName_.substr(9);   // skip "\\.\pipe\"
  BOOL ok;
  if (on) ok = DefineDosDeviceW(DDD_RAW_TARGET_PATH, port_.c_str(), target.c_str());
  else ok = DefineDosDeviceW(DDD_RAW_TARGET_PATH | DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE, port_.c_str(), target.c_str());
  if (!ok) {
    DWORD e = GetLastError();
    wchar_t* msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, e, 0, (LPWSTR)&msg, 0, nullptr);
    error = util::Fmt(L"DefineDosDevice(%s) error %lu", port_.c_str(), e);
    if (msg) { error += L": "; error += msg; LocalFree(msg); }
    if (e == ERROR_ACCESS_DENIED) error += L" (ACCESS_DENIED)";
    return false;
  }
  return true;
}

bool Capture::Start(const ws& port, const ws& spoolPath, HWND notify, ws& error) {
  Stop();
  port_ = port;
  spool_ = spoolPath;
  notify_ = notify;
  pipeName_ = L"\\\\.\\pipe\\OpenDOSPRN_" + port;
  stopEv_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  running_ = true;
  lastTick_ = 0;
  thread_ = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
  Sleep(50);   // let the first pipe instance come up before the device is mapped
  if (!Map(true, error)) { Stop(); return false; }
  mapped_ = true;
  return true;
}

void Capture::Stop() {
  if (!thread_ && !mapped_) return;
  ws dummy;
  if (mapped_) { Map(false, dummy); mapped_ = false; }
  running_ = false;
  if (stopEv_) SetEvent(stopEv_);
  if (thread_) { WaitForSingleObject(thread_, 3000); CloseHandle(thread_); thread_ = nullptr; }
  if (stopEv_) { CloseHandle(stopEv_); stopEv_ = nullptr; }
}

DWORD WINAPI Capture::ThreadProc(LPVOID p) { ((Capture*)p)->Loop(); return 0; }

void Capture::Loop() {
  // pipe open to every local user (the DOS program may run in another session/account)
  SECURITY_DESCRIPTOR sd;
  InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
  SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE);
  SECURITY_ATTRIBUTES sa = {sizeof sa, &sd, FALSE};
  HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  std::vector<char> buf(65536);
  while (running_) {
    HANDLE pipe = CreateNamedPipeW(pipeName_.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 4096,
                                   (DWORD)buf.size(), 0, &sa);
    if (pipe == INVALID_HANDLE_VALUE) {
      if (WaitForSingleObject(stopEv_, 500) == WAIT_OBJECT_0) break;
      continue;
    }
    OVERLAPPED ov = {};
    ov.hEvent = ev;
    ResetEvent(ev);
    HANDLE waitFor[2] = {ev, stopEv_};
    BOOL conn = ConnectNamedPipe(pipe, &ov);
    DWORD err = GetLastError();
    bool connected = conn || err == ERROR_PIPE_CONNECTED;
    if (!connected && err == ERROR_IO_PENDING) {
      DWORD w = WaitForMultipleObjects(2, waitFor, FALSE, INFINITE);
      if (w != WAIT_OBJECT_0) { CancelIo(pipe); CloseHandle(pipe); break; }
      connected = true;
    }
    if (!connected) { CloseHandle(pipe); continue; }

    HANDLE file = CreateFileW(spool_.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    for (;;) {
      ResetEvent(ev);
      DWORD got = 0;
      BOOL rd = ReadFile(pipe, buf.data(), (DWORD)buf.size(), nullptr, &ov);
      if (!rd && GetLastError() == ERROR_IO_PENDING) {
        DWORD w = WaitForMultipleObjects(2, waitFor, FALSE, INFINITE);
        if (w != WAIT_OBJECT_0) { CancelIo(pipe); break; }
        if (!GetOverlappedResult(pipe, &ov, &got, FALSE)) break;
      } else if (rd) {
        GetOverlappedResult(pipe, &ov, &got, FALSE);
      } else
        break;
      if (got == 0) break;
      if (file != INVALID_HANDLE_VALUE) {
        DWORD wr;
        WriteFile(file, buf.data(), got, &wr, nullptr);
      }
      lastTick_ = GetTickCount64();
      if (notify_) PostMessageW(notify_, WM_CAPTURE_DATA, got, (LPARAM)(index_ * 2));
    }
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    lastTick_ = GetTickCount64();
    if (notify_) PostMessageW(notify_, WM_CAPTURE_DATA, 0, (LPARAM)(index_ * 2 + 1));   // client closed
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
  }
  CloseHandle(ev);
}
