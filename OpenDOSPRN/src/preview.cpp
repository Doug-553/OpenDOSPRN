#include "ui.h"

namespace {
enum { ID_ZOUT = 201, ID_ZIN, ID_ZCOMBO, ID_FIRST, ID_PRIOR, ID_NEXT, ID_LAST, ID_RANGE, ID_PRINT, ID_CANCEL };

struct PV {
  PreviewIO* io = nullptr;
  HWND wnd = nullptr, canvas = nullptr, status = nullptr, zcombo = nullptr, range = nullptr;
  int cur = 0;
  double zoom = 1.0;
  int zmode = 3;          // 0 custom, 1 width, 2 height, 3 whole page
  double pw = 8.27, ph = 11.69;
  bool closed = false, printed = false;
};

const int kPad = 14;
const int kToolH = 34, kStatH = 22;

int Total(PV* p) { return (int)p->io->pages->size(); }
double Dpi(PV* p) { return 96.0 * p->zoom; }

void ComputeZoom(PV* p) {
  RECT rc;
  GetClientRect(p->canvas, &rc);
  double cw = std::max(50, (int)(rc.right - 2 * kPad)), chh = std::max(50, (int)(rc.bottom - 2 * kPad));
  if (p->zmode == 1) p->zoom = cw / (p->pw * 96.0);
  else if (p->zmode == 2) p->zoom = chh / (p->ph * 96.0);
  else if (p->zmode == 3) p->zoom = std::min(cw / (p->pw * 96.0), chh / (p->ph * 96.0));
  p->zoom = std::max(0.1, std::min(p->zoom, 6.0));
}

void UpdateScroll(PV* p) {
  RECT rc;
  GetClientRect(p->canvas, &rc);
  int paperW = (int)(p->pw * Dpi(p)), paperH = (int)(p->ph * Dpi(p));
  SCROLLINFO si = {sizeof si, SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL};
  si.nMin = 0;
  si.nMax = paperW + 2 * kPad;
  si.nPage = rc.right;
  SetScrollInfo(p->canvas, SB_HORZ, &si, TRUE);
  si.nMax = paperH + 2 * kPad;
  si.nPage = rc.bottom;
  SetScrollInfo(p->canvas, SB_VERT, &si, TRUE);
}

void UpdateStatus(PV* p) {
  ws t = util::Fmt(L" %s %d %s %d", lang::T("Page").c_str(), p->cur + 1, lang::T("Of").c_str(), Total(p));
  t += util::Fmt(L"    %d%%", (int)(p->zoom * 100 + 0.5));
  SetText(p->status, t);
}

void Refresh(PV* p) {
  if (p->zmode) ComputeZoom(p);
  UpdateScroll(p);
  UpdateStatus(p);
  InvalidateRect(p->canvas, nullptr, TRUE);
}

void GoPage(PV* p, int n) {
  n = std::max(0, std::min(n, Total(p) - 1));
  if (n != p->cur) {
    p->cur = n;
    SetScrollPos(p->canvas, SB_VERT, 0, TRUE);
  }
  UpdateStatus(p);
  InvalidateRect(p->canvas, nullptr, TRUE);
}

void SetZoomMode(PV* p, int mode, double z = 1.0) {
  p->zmode = mode;
  if (!mode) p->zoom = z;
  SetScrollPos(p->canvas, SB_HORZ, 0, FALSE);
  SetScrollPos(p->canvas, SB_VERT, 0, FALSE);
  Refresh(p);
}

void Paint(PV* p, HDC hdc) {
  RECT rc;
  GetClientRect(p->canvas, &rc);
  int W = rc.right, H = rc.bottom;
  if (W <= 0 || H <= 0) return;
  HDC mem = CreateCompatibleDC(hdc);
  HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
  HGDIOBJ old = SelectObject(mem, bmp);
  HBRUSH bg = CreateSolidBrush(RGB(128, 128, 128));
  FillRect(mem, &rc, bg);
  DeleteObject(bg);
  double dpi = Dpi(p);
  int pwPx = (int)(p->pw * dpi), phPx = (int)(p->ph * dpi);
  int sx = GetScrollPos(p->canvas, SB_HORZ), sy = GetScrollPos(p->canvas, SB_VERT);
  int px = pwPx + 2 * kPad <= W ? (W - pwPx) / 2 : kPad - sx;
  int py = phPx + 2 * kPad <= H ? (H - phPx) / 2 : kPad - sy;
  RECT shadow = {px + 4, py + 4, px + pwPx + 4, py + phPx + 4};
  HBRUSH sb = CreateSolidBrush(RGB(90, 90, 90));
  FillRect(mem, &shadow, sb);
  DeleteObject(sb);
  RECT paper = {px, py, px + pwPx, py + phPx};
  FillRect(mem, &paper, (HBRUSH)GetStockObject(WHITE_BRUSH));
  if (p->cur >= 0 && p->cur < Total(p)) {
    int sv = SaveDC(mem);
    IntersectClipRect(mem, std::max(0, (int)paper.left), std::max(0, (int)paper.top), std::min(W, (int)paper.right),
                      std::min(H, (int)paper.bottom));
    RenderStyle st = MakeRenderStyle(*p->io->set);
    RenderPage(mem, (*p->io->pages)[p->cur], st, dpi, dpi, px, py);
    RestoreDC(mem, sv);
  }
  BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
}

void Scroll(PV* p, int bar, int code, int pos) {
  SCROLLINFO si = {sizeof si, SIF_ALL};
  GetScrollInfo(p->canvas, bar, &si);
  int np = si.nPos;
  switch (code) {
    case SB_LINEUP: np -= 30; break;
    case SB_LINEDOWN: np += 30; break;
    case SB_PAGEUP: np -= (int)si.nPage; break;
    case SB_PAGEDOWN: np += (int)si.nPage; break;
    case SB_THUMBTRACK: case SB_THUMBPOSITION: np = pos; break;
    case SB_TOP: np = 0; break;
    case SB_BOTTOM: np = si.nMax; break;
  }
  SetScrollPos(p->canvas, bar, np, TRUE);
  InvalidateRect(p->canvas, nullptr, FALSE);
}

LRESULT CALLBACK CanvasProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  PV* p = (PV*)GetWindowLongPtrW(h, GWLP_USERDATA);
  switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      if (p) Paint(p, dc);
      EndPaint(h, &ps);
      return 0;
    }
    case WM_SIZE: if (p) Refresh(p); return 0;
    case WM_VSCROLL: if (p) Scroll(p, SB_VERT, LOWORD(w), HIWORD(w)); return 0;
    case WM_HSCROLL: if (p) Scroll(p, SB_HORZ, LOWORD(w), HIWORD(w)); return 0;
    case WM_LBUTTONDOWN: SetFocus(h); return 0;
    case WM_MOUSEWHEEL: {
      if (!p) return 0;
      int d = GET_WHEEL_DELTA_WPARAM(w);
      if (GET_KEYSTATE_WPARAM(w) & MK_CONTROL) SetZoomMode(p, 0, d > 0 ? p->zoom * 1.15 : p->zoom / 1.15);
      else Scroll(p, SB_VERT, d > 0 ? SB_LINEUP : SB_LINEDOWN, 0), Scroll(p, SB_VERT, d > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
      return 0;
    }
    case WM_KEYDOWN:
      if (!p) break;
      switch (w) {
        case VK_NEXT: GoPage(p, p->cur + 1); return 0;
        case VK_PRIOR: GoPage(p, p->cur - 1); return 0;
        case VK_HOME: GoPage(p, 0); return 0;
        case VK_END: GoPage(p, Total(p) - 1); return 0;
        case VK_DOWN: Scroll(p, SB_VERT, SB_LINEDOWN, 0); return 0;
        case VK_UP: Scroll(p, SB_VERT, SB_LINEUP, 0); return 0;
        case VK_LEFT: Scroll(p, SB_HORZ, SB_LINEUP, 0); return 0;
        case VK_RIGHT: Scroll(p, SB_HORZ, SB_LINEDOWN, 0); return 0;
        case VK_ESCAPE: PostMessageW(p->wnd, WM_CLOSE, 0, 0); return 0;
      }
      break;
    case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTCHARS;
  }
  return DefWindowProcW(h, m, w, l);
}

void Layout(PV* p) {
  RECT rc;
  GetClientRect(p->wnd, &rc);
  MoveWindow(p->status, 0, rc.bottom - kStatH, rc.right, kStatH, TRUE);
  MoveWindow(p->canvas, 0, kToolH, rc.right, rc.bottom - kToolH - kStatH, TRUE);
}

LRESULT CALLBACK PreviewProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  PV* p = (PV*)GetWindowLongPtrW(h, GWLP_USERDATA);
  switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); break;
    case WM_SIZE: if (p && p->canvas) { Layout(p); Refresh(p); } return 0;
    case WM_COMMAND: {
      if (!p) break;
      int id = LOWORD(w);
      if (id == ID_ZCOMBO && HIWORD(w) == CBN_SELCHANGE) {
        int i = (int)SendMessageW(p->zcombo, CB_GETCURSEL, 0, 0);
        static const double z[] = {1, 0, 0, 0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0};
        if (i == 0) SetZoomMode(p, 0, 1.0);
        else if (i >= 1 && i <= 3) SetZoomMode(p, i);
        else if (i >= 4) SetZoomMode(p, 0, z[i]);
        SetFocus(p->canvas);
        return 0;
      }
      switch (id) {
        case ID_ZOUT: SetZoomMode(p, 0, p->zoom / 1.25); break;
        case ID_ZIN: SetZoomMode(p, 0, p->zoom * 1.25); break;
        case ID_FIRST: GoPage(p, 0); break;
        case ID_PRIOR: GoPage(p, p->cur - 1); break;
        case ID_NEXT: GoPage(p, p->cur + 1); break;
        case ID_LAST: GoPage(p, Total(p) - 1); break;
        case ID_PRINT: {
          std::vector<int> sel;
          if (!ParsePageRange(GetText(p->range), Total(p), sel)) {
            MessageBoxW(h, lang::T("PageRange").c_str(), L"OpenDOSPRN", MB_OK | MB_ICONWARNING);
            return 0;
          }
          p->io->sel = sel;
          p->printed = true;
          p->closed = true;
          return 0;
        }
        case ID_CANCEL: p->closed = true; return 0;
      }
      return 0;
    }
    case WM_CLOSE: if (p) p->closed = true; return 0;
  }
  return DefWindowProcW(h, m, w, l);
}
}  // namespace

bool ShowPreview(HWND owner, PreviewIO& io) {
  static bool reg = false;
  if (!reg) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = PreviewProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ODP_Preview";
    RegisterClassW(&wc);
    WNDCLASSW cc = {};
    cc.lpfnWndProc = CanvasProc;
    cc.hInstance = g_hInst;
    cc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    cc.lpszClassName = L"ODP_Canvas";
    RegisterClassW(&cc);
    reg = true;
  }
  PV pv;
  pv.io = &io;
  PageDims(*io.set, pv.pw, pv.ph);
  int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
  int w = std::min(sw - 40, 1000), h = std::min(sh - 80, 780);
  HWND d = CreateWindowExW(WS_EX_APPWINDOW, L"ODP_Preview", (L"OpenDOSPRN - " + lang::T("Printpreview")).c_str(),
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, (sw - w) / 2, (sh - h) / 2 - 10, w, h, nullptr, nullptr,
                           g_hInst, &pv);
  if (!d) return false;
  pv.wnd = d;
  int x = 6;
  auto btn = [&](const ws& t, int id, int bw) {
    HWND b = MakeCtl(d, L"BUTTON", t, WS_TABSTOP | BS_PUSHBUTTON, x, 4, bw, 26, id);
    x += bw + 4;
    return b;
  };
  btn(L"\u2212", ID_ZOUT, 30);
  btn(L"+", ID_ZIN, 30);
  pv.zcombo = MakeCtl(d, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, x, 5, 120, 200, ID_ZCOMBO);
  x += 128;
  const ws zi[] = {lang::T("ActualSize"), lang::T("PageWidth"), lang::T("PageHeight"), lang::T("WholePage"),
                   L"25%", L"50%", L"75%", L"100%", L"150%", L"200%", L"400%"};
  for (auto& s : zi) SendMessageW(pv.zcombo, CB_ADDSTRING, 0, (LPARAM)s.c_str());
  SendMessageW(pv.zcombo, CB_SETCURSEL, 3, 0);
  x += 8;
  btn(L"|\u25C0", ID_FIRST, 34);
  btn(L"\u25C0", ID_PRIOR, 34);
  btn(L"\u25B6", ID_NEXT, 34);
  btn(L"\u25B6|", ID_LAST, 34);
  x += 10;
  MakeCtl(d, L"STATIC", lang::T("PageRange"), SS_CENTERIMAGE, x, 4, 160, 26, 0);
  x += 164;
  pv.range = MakeCtl(d, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, x, 6, 90, 22, ID_RANGE);
  x += 100;
  btn(lang::T("Print"), ID_PRINT, 80);
  btn(lang::T("Cancel"), ID_CANCEL, 80);
  pv.status = MakeCtl(d, L"STATIC", L"", SS_SUNKEN | SS_CENTERIMAGE, 0, 0, 100, kStatH, 0);
  pv.canvas = CreateWindowExW(WS_EX_CLIENTEDGE, L"ODP_Canvas", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, 0,
                              kToolH, 100, 100, d, nullptr, g_hInst, &pv);
  Layout(&pv);
  ShowWindow(d, SW_SHOW);
  if (owner) EnableWindow(owner, FALSE);
  Refresh(&pv);
  SetFocus(pv.canvas);
  MSG msg;
  while (!pv.closed && GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  if (owner) EnableWindow(owner, TRUE);
  DestroyWindow(d);
  if (owner) SetForegroundWindow(owner);
  return pv.printed;
}
