#include "ui.h"

HINSTANCE g_hInst = nullptr;
HFONT g_uiFont = nullptr;
void (*g_refreshUi)() = nullptr;

void InitUi() {
  NONCLIENTMETRICSW ncm = {sizeof ncm};
  SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
  g_uiFont = CreateFontIndirectW(&ncm.lfMessageFont);
  INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_WIN95_CLASSES | ICC_UPDOWN_CLASS | ICC_TAB_CLASSES | ICC_BAR_CLASSES |
                                              ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&icc);
}

HWND MakeCtl(HWND parent, const wchar_t* cls, const ws& text, DWORD style, int x, int y, int w, int h, int id, DWORD ex) {
  HWND c = CreateWindowExW(ex, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent, (HMENU)(INT_PTR)id,
                           g_hInst, nullptr);
  if (c) SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
  return c;
}

void SetText(HWND h, const ws& t) { SetWindowTextW(h, t.c_str()); }
ws GetText(HWND h) {
  int n = GetWindowTextLengthW(h);
  ws s(n + 1, L'\0');
  GetWindowTextW(h, &s[0], n + 1);
  s.resize(n);
  return s;
}

static BOOL CALLBACK FontEnum(HWND h, LPARAM) {
  SendMessageW(h, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
  return TRUE;
}
void SetFontTree(HWND root) { EnumChildWindows(root, FontEnum, 0); }

static LRESULT CALLBACK ModalProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  ModalCtx* c = (ModalCtx*)GetWindowLongPtrW(h, GWLP_USERDATA);
  if (m == WM_NCCREATE) {
    c = (ModalCtx*)((CREATESTRUCTW*)l)->lpCreateParams;
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)c);
  }
  if (c) {
    if (m == WM_CREATE) {
      c->hwnd = h;
      if (c->init) c->init(h);
      return 0;
    }
    LRESULT r = 0;
    if (c->proc && c->proc(h, m, w, l, r)) return r;
    if (m == WM_CLOSE) { c->done = true; return 0; }
  }
  return DefWindowProcW(h, m, w, l);
}

void RunModal(HWND parent, const ws& title, int cw, int ch, ModalCtx& ctx) {
  static bool reg = false;
  if (!reg) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = ModalProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ODP_Dialog";
    RegisterClassW(&wc);
    reg = true;
  }
  DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU, ex = WS_EX_DLGMODALFRAME;
  RECT rc = {0, 0, cw, ch};
  AdjustWindowRectEx(&rc, style, FALSE, ex);
  int w = rc.right - rc.left, h = rc.bottom - rc.top;
  RECT pr = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
  if (parent && IsWindowVisible(parent) && !IsIconic(parent)) GetWindowRect(parent, &pr);
  int x = pr.left + ((pr.right - pr.left) - w) / 2, y = pr.top + ((pr.bottom - pr.top) - h) / 2;
  ctx.done = false;
  HWND d = CreateWindowExW(ex, L"ODP_Dialog", title.c_str(), style, x, y, w, h, parent, nullptr, g_hInst, &ctx);
  if (!d) return;
  if (parent) EnableWindow(parent, FALSE);
  ShowWindow(d, SW_SHOW);
  UpdateWindow(d);
  MSG msg;
  while (!ctx.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(d, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  if (parent) EnableWindow(parent, TRUE);
  DestroyWindow(d);
  if (parent) SetForegroundWindow(parent);
}

bool PromptText(HWND parent, const ws& title, const ws& label, ws& text) {
  ModalCtx ctx;
  bool ok = false;
  HWND edit = nullptr;
  ctx.init = [&](HWND d) {
    MakeCtl(d, L"STATIC", label, 0, 12, 12, 276, 16, 0);
    edit = MakeCtl(d, L"EDIT", text, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 12, 32, 276, 22, 100, 0);
    MakeCtl(d, L"BUTTON", lang::T("Ok"), WS_TABSTOP | BS_DEFPUSHBUTTON, 128, 66, 76, 24, IDOK);
    MakeCtl(d, L"BUTTON", lang::T("Cancel"), WS_TABSTOP | BS_PUSHBUTTON, 212, 66, 76, 24, IDCANCEL);
    SetFocus(edit);
  };
  ctx.proc = [&](HWND d, UINT m, WPARAM w, LPARAM, LRESULT&) {
    if (m == WM_COMMAND) {
      if (LOWORD(w) == IDOK) { text = util::Trim(GetText(edit)); ok = !text.empty(); ctx.done = true; return true; }
      if (LOWORD(w) == IDCANCEL) { ctx.done = true; return true; }
    }
    return false;
  };
  RunModal(parent, title, 300, 102, ctx);
  return ok;
}

ws ToReadable(const std::string& b) {
  static const char* names[32] = {"NUL","SOH","STX","ETX","EOT","ENQ","ACK","BEL","BS","HT","LF","VT","FF","CR","SO","SI",
                                  "DLE","DC1","DC2","DC3","DC4","NAK","SYN","ETB","CAN","EM","SUB","ESC","FS","GS","RS","US"};
  ws r;
  for (size_t i = 0; i < b.size(); i++) {
    unsigned char c = (unsigned char)b[i];
    if (i) r += L' ';
    if (c < 32) r += util::Utf8ToW(names[c]);
    else if (c < 127) r += (wchar_t)c;
    else r += util::Fmt(L"0x%02X", c);
    if (c < 32 && c != 27) {}
  }
  return r;
}

// ------------------------------------------------------------------ numeric edits
HWND MakeSpin(HWND p, int id, int x, int y, int w, int h, int mn, int mx, int val) {
  HWND e = MakeCtl(p, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, x, y, w, h, id, WS_EX_CLIENTEDGE);
  HWND u = CreateWindowExW(0, UPDOWN_CLASSW, nullptr,
                           WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS, 0, 0, 0, 0,
                           p, (HMENU)(INT_PTR)(id + 1000), g_hInst, nullptr);
  SendMessageW(u, UDM_SETBUDDY, (WPARAM)e, 0);
  SendMessageW(u, UDM_SETRANGE32, mn, mx);
  SendMessageW(u, UDM_SETPOS32, 0, val);
  return e;
}
int GetSpin(HWND e, int def) {
  HWND u = GetDlgItem(GetParent(e), GetDlgCtrlID(e) + 1000);
  BOOL ok = FALSE;
  LRESULT r = SendMessageW(u, UDM_GETPOS32, 0, (LPARAM)&ok);
  return ok ? (int)r : def;
}
void SetSpin(HWND e, int v) {
  HWND u = GetDlgItem(GetParent(e), GetDlgCtrlID(e) + 1000);
  SendMessageW(u, UDM_SETPOS32, 0, v);
}

namespace {
struct FSpin { double step, mn, mx; };
std::map<HWND, FSpin>& FSpins() { static std::map<HWND, FSpin> m; return m; }
}
HWND MakeFloatSpin(HWND p, int id, int x, int y, int w, int h, double val, double step, double mn, double mx) {
  HWND e = MakeCtl(p, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, x, y, w - 16, h, id, WS_EX_CLIENTEDGE);
  HWND u = CreateWindowExW(0, UPDOWN_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | UDS_ARROWKEYS, x + w - 16, y, 16, h, p,
                           (HMENU)(INT_PTR)(id + 1000), g_hInst, nullptr);
  SendMessageW(u, UDM_SETRANGE32, 0, 32767);
  SendMessageW(u, UDM_SETPOS32, 0, 16000);
  FSpins()[u] = {step, mn, mx};
  SetFloat(e, val);
  return e;
}
double GetFloat(HWND e, double def) { return util::ParseDouble(GetText(e), def); }
void SetFloat(HWND e, double v) {
  ws t = util::Fmt(L"%.2f", v);
  while (t.size() > 1 && t.back() == L'0') t.pop_back();
  if (!t.empty() && t.back() == L'.') t += L'0';
  SetText(e, t);
}
bool FloatSpinNotify(HWND parent, LPARAM l, LRESULT& r) {
  NMHDR* nm = (NMHDR*)l;
  if (nm->code != UDN_DELTAPOS) return false;
  auto it = FSpins().find(nm->hwndFrom);
  if (it == FSpins().end()) return false;
  NMUPDOWN* ud = (NMUPDOWN*)l;
  HWND e = GetDlgItem(parent, (int)nm->idFrom - 1000);
  double v = GetFloat(e, it->second.mn) + (ud->iDelta > 0 ? it->second.step : -it->second.step);
  v = std::max(it->second.mn, std::min(it->second.mx, v));
  SetFloat(e, v);
  SendMessageW(parent, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(e), EN_CHANGE), (LPARAM)e);
  r = 1;   // we handled the change ourselves
  return true;
}
