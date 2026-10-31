#pragma once
#include "common.h"

#define WM_CAPTURE_DATA (WM_APP + 1)

// Redirects an LPT/COM device name to a named pipe and stores received data in a spool file.
class Capture {
 public:
  Capture();
  ~Capture();
  void SetIndex(int i) { index_ = i; }
  bool Start(const ws& port, const ws& spoolPath, HWND notify, ws& error);
  void Stop();
  bool Running() const { return running_; }
  ULONGLONG LastActivity() const { return lastTick_; }
  const ws& Port() const { return port_; }
  static ws PortName(int index);   // 0..8 LPT1..9, 9..11 COM1..3

 private:
  static DWORD WINAPI ThreadProc(LPVOID p);
  void Loop();
  bool Map(bool on, ws& error);
  int index_ = 0;
  ws port_, spool_, pipeName_;
  HWND notify_ = nullptr;
  HANDLE thread_ = nullptr, stopEv_ = nullptr;
  volatile bool running_ = false;
  volatile ULONGLONG lastTick_ = 0;
  bool mapped_ = false;
};
