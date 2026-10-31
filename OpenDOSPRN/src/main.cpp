#include "ui.h"

#define WM_TRAY (WM_APP + 2)
enum {
  ID_TAB = 1000, ID_LANG, ID_RUNMIN,
  ID_HELP, ID_PRINTFILE, ID_SERVICE,
  ID_SPOOL, ID_TRACK, ID_AUTOPRINT, ID_PRINTJOB, ID_CLEARJOB, ID_ORIENT,
  ID_PORTRAIT, ID_LANDSCAPE, ID_WRAP, ID_FF, ID_LFCRLF, ID_CRCRLF,
  ID_PRINTERBTN, ID_COPIES, ID_RAW, ID_PORT0 = 1100, /* 12 ports */ ID_CAPTURE = 1120,
  ID_SEQEMU, ID_SEQCOMBO, ID_SEQEDIT, ID_PAPER, ID_PREVIEW, ID_LASTPREVIEW, ID_TOPRINTER, ID_TOPDF, ID_PDFSET,
  ID_FONTLIST, ID_MONO, ID_BOLD, ID_ITALIC, ID_UNDER, ID_STRIKE, ID_SIZE, ID_CPI, ID_LPI, ID_RESET,
  ID_MTOP, ID_MBOT, ID_MLEFT, ID_MRIGHT, ID_CPCOMBO, ID_ASCII, ID_SAMPLE,
  ID_M_OPEN = 1300, ID_M_PRINTFILE, ID_M_PRINT, ID_M_PORTRAIT, ID_M_LANDSCAPE, ID_M_CAPTURE, ID_M_CANCELJOB, ID_M_EXIT
};

static HWND g_wnd, g_tab;
static std::vector<HWND> g_page[2];
static Capture g_caps[12];
static bool g_loading = false, g_busy = false, g_quitting = false;
static NOTIFYICONDATAW g_nid;
static HFONT g_sampleFont = nullptr;
static std::vector<std::pair<ws, ws>> g_cps;

static HWND C(int id) { return GetDlgItem(g_wnd, id); }
static const Settings& Prof(int i) { return i == g_set.portIndex ? g_set : g_profiles[i]; }
static ws SpoolPathFor(int i) { return util::ExpandEnv(Prof(i).spoolName); }
static ws SpoolPath() { return SpoolPathFor(g_set.portIndex); }
static bool SpoolHasDataFor(int i) {
  WIN32_FILE_ATTRIBUTE_DATA fa;
  return GetFileAttributesExW(SpoolPathFor(i).c_str(), GetFileExInfoStandard, &fa) && (fa.nFileSizeLow || fa.nFileSizeHigh);
}
static bool SpoolHasData() { return SpoolHasDataFor(g_set.portIndex); }
static void Check(int id, bool v) { SendMessageW(C(id), BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0); }
static bool Checked(int id) { return SendMessageW(C(id), BM_GETCHECK, 0, 0) == BST_CHECKED; }

static void Add(int page, HWND h) { if (h) g_page[page].push_back(h); }
static HWND Ctl(int page, const wchar_t* cls, const ws& t, DWORD st, int x, int y, int w, int h, int id, DWORD ex = 0) {
  HWND c = MakeCtl(g_wnd, cls, t, st, x + 14, y + 38, w, h, id, ex);
  Add(page, c);
  return c;
}

// ------------------------------------------------------------------ state <-> UI
static void UpdateButtons() {
  bool has = SpoolHasData();
  EnableWindow(C(ID_PRINTJOB), has && !g_busy);
  EnableWindow(C(ID_CLEARJOB), has && !g_busy);
  EnableWindow(C(ID_SPOOL), !g_caps[g_set.portIndex].Running());
  SetText(C(ID_AUTOPRINT), lang::Tu("AutoPrint", g_set.timeoutSec));
  for (int i = 0; i < 12; i++) {   // mark captured ports and ports with a pending job
    ws t = PortLabel(i);
    if (g_caps[i].Running()) t += L" *";
    if (SpoolHasDataFor(i)) t += L" !";
    if (GetText(C(ID_PORT0 + i)) != t) SetText(C(ID_PORT0 + i), t);
  }
  SetText(C(ID_CAPTURE), lang::T("CaptureOn") + L" " + PortLabel(g_set.portIndex));
}

static void ShowPage(int p) {
  for (int k = 0; k < 2; k++) for (HWND h : g_page[k]) ShowWindow(h, k == p ? SW_SHOW : SW_HIDE);
}

static void UpdateSample() {
  if (g_sampleFont) DeleteObject(g_sampleFont);
  LOGFONTW lf = {};
  lf.lfHeight = -MulDiv(g_set.fontSize, 96, 72);
  lf.lfWeight = g_set.bold ? FW_BOLD : FW_NORMAL;
  lf.lfItalic = g_set.italic; lf.lfUnderline = g_set.underline; lf.lfStrikeOut = g_set.strike;
  lf.lfCharSet = DEFAULT_CHARSET;
  wcsncpy(lf.lfFaceName, g_set.fontName.c_str(), LF_FACESIZE - 1);
  g_sampleFont = CreateFontIndirectW(&lf);
  HWND s = C(ID_SAMPLE);
  if (s) {
    SendMessageW(s, WM_SETFONT, (WPARAM)g_sampleFont, TRUE);
    SetText(s, util::Fmt(L"%s %d  -  AaBbYyZz 0123456789", g_set.fontName.c_str(), g_set.fontSize));
  }
}

static void LoadUi() {
  g_loading = true;
  SetText(C(ID_SPOOL), g_set.spoolName);
  SendMessageW(C(ID_TRACK), TBM_SETPOS, TRUE, g_set.timeoutSec);
  Check(ID_AUTOPRINT, g_set.autoPrint);
  Check(ID_PORTRAIT, !g_set.landscape); Check(ID_LANDSCAPE, g_set.landscape);
  Check(ID_WRAP, g_set.wrapLong); Check(ID_FF, g_set.useFF); Check(ID_LFCRLF, g_set.lfAsCrLf); Check(ID_CRCRLF, g_set.crAsCrLf);
  ws pn = g_set.printerName.empty() ? DefaultPrinterName() : g_set.printerName;
  SetText(C(ID_PRINTERBTN), pn.empty() ? lang::T("NoPrinters") : pn);
  SetSpin(C(ID_COPIES), g_set.copies);
  Check(ID_RAW, g_set.rawMode);
  for (int i = 0; i < 12; i++) Check(ID_PORT0 + i, i == g_set.portIndex);
  Check(ID_CAPTURE, g_set.capture);
  Check(ID_SEQEMU, g_set.emulateSeq);
  HWND sc = C(ID_SEQCOMBO);
  SendMessageW(sc, CB_RESETCONTENT, 0, 0);
  SendMessageW(sc, CB_ADDSTRING, 0, (LPARAM)L"Epson ESC/P");
  SendMessageW(sc, CB_ADDSTRING, 0, (LPARAM)L"No sequences");
  for (auto& s : g_set.schemes) SendMessageW(sc, CB_ADDSTRING, 0, (LPARAM)s.name.c_str());
  if (SendMessageW(sc, CB_SELECTSTRING, -1, (LPARAM)g_set.seqScheme.c_str()) == CB_ERR) SendMessageW(sc, CB_SETCURSEL, 0, 0);
  HWND pc = C(ID_PAPER);
  SendMessageW(pc, CB_RESETCONTENT, 0, 0);
  int ps = 0, i = 0;
  for (auto& p : Papers()) {
    SendMessageW(pc, CB_ADDSTRING, 0, (LPARAM)p.name);
    if (p.id == g_set.paperSize) ps = i;
    i++;
  }
  SendMessageW(pc, CB_SETCURSEL, ps, 0);
  Check(ID_PREVIEW, g_set.preview); Check(ID_TOPRINTER, g_set.toPrinter); Check(ID_TOPDF, g_set.toPdf);
  Check(ID_MONO, g_set.monoOnly);
  Check(ID_BOLD, g_set.bold); Check(ID_ITALIC, g_set.italic); Check(ID_UNDER, g_set.underline); Check(ID_STRIKE, g_set.strike);
  HWND fl = C(ID_FONTLIST);
  SendMessageW(fl, LB_RESETCONTENT, 0, 0);
  for (auto& f : EnumFonts(g_set.monoOnly)) SendMessageW(fl, LB_ADDSTRING, 0, (LPARAM)f.c_str());
  if (SendMessageW(fl, LB_SELECTSTRING, -1, (LPARAM)g_set.fontName.c_str()) == LB_ERR) {}
  SetSpin(C(ID_SIZE), g_set.fontSize);
  SetFloat(C(ID_CPI), g_set.cpi); SetFloat(C(ID_LPI), g_set.lpi);
  SetSpin(C(ID_MTOP), g_set.mTop); SetSpin(C(ID_MBOT), g_set.mBottom); SetSpin(C(ID_MLEFT), g_set.mLeft); SetSpin(C(ID_MRIGHT), g_set.mRight);
  HWND cp = C(ID_CPCOMBO);
  SendMessageW(cp, CB_RESETCONTENT, 0, 0);
  int cs = 0;
  for (size_t k = 0; k < g_cps.size(); k++) {
    SendMessageW(cp, CB_ADDSTRING, 0, (LPARAM)(g_cps[k].first + L" - " + g_cps[k].second).c_str());
    if (g_cps[k].first == g_set.codepage) cs = (int)k;
  }
  SendMessageW(cp, CB_SETCURSEL, cs, 0);
  Check(ID_RUNMIN, g_set.runMinimized);
  UpdateSample();
  InvalidateRect(C(ID_ORIENT), nullptr, TRUE);
  g_loading = false;
  UpdateButtons();
}

// Starts/stops the capture of one port according to its profile. Returns an error text ("" = ok).
static ws ApplyCapture(int i) {
  g_caps[i].Stop();
  const Settings& p = Prof(i);
  if (!p.capture) return ws();
  g_caps[i].SetIndex(i);
  ws err;
  if (!g_caps[i].Start(PortLabel(i), SpoolPathFor(i), g_wnd, err)) {
    if (i == g_set.portIndex) g_set.capture = false;
    g_profiles[i].capture = false;
    return PortLabel(i) + L": " + err;
  }
  return ws();
}

static void ReportCaptureErrors(const std::vector<ws>& errs) {
  if (errs.empty()) return;
  ws m = lang::T("CaptureFailed") + L"\n\n";
  for (auto& e : errs) m += e + L"\n";
  for (auto& e : errs) if (e.find(L"ACCESS_DENIED") != ws::npos) { m += L"\n" + lang::T("LimitedAccount"); break; }
  MessageBoxW(g_wnd, m.c_str(), L"OpenDOSPRN", MB_OK | MB_ICONWARNING);
}

static void StartStopCapture(int i) {
  ws e = ApplyCapture(i);
  if (!e.empty()) { ReportCaptureErrors({e}); Check(ID_CAPTURE, false); }
  UpdateButtons();
}

static void StartAllCaptures() {
  std::vector<ws> errs;
  for (int i = 0; i < 12; i++) { ws e = ApplyCapture(i); if (!e.empty()) errs.push_back(e); }
  ReportCaptureErrors(errs);
  if (!errs.empty()) { g_set.Save(); }
  LoadUi();
}

// Switch the port shown in the UI: its profile becomes the edited one.
static void SelectPort(int n) {
  if (n < 0 || n > 11 || n == g_set.portIndex) return;
  g_profiles[g_set.portIndex].TakePortFields(g_set);
  g_set.portIndex = n;
  g_set.TakePortFields(g_profiles[n]);
  g_set.Save();
  LoadUi();
}

static void PullUi() {
  if (g_loading) return;
  g_set.spoolName = GetText(C(ID_SPOOL));
  g_set.timeoutSec = (int)SendMessageW(C(ID_TRACK), TBM_GETPOS, 0, 0);
  g_set.autoPrint = Checked(ID_AUTOPRINT);
  g_set.landscape = Checked(ID_LANDSCAPE);
  g_set.wrapLong = Checked(ID_WRAP); g_set.useFF = Checked(ID_FF); g_set.lfAsCrLf = Checked(ID_LFCRLF); g_set.crAsCrLf = Checked(ID_CRCRLF);
  g_set.copies = GetSpin(C(ID_COPIES), g_set.copies);
  g_set.emulateSeq = Checked(ID_SEQEMU);
  int si = (int)SendMessageW(C(ID_SEQCOMBO), CB_GETCURSEL, 0, 0);
  if (si >= 0) { wchar_t b[256]; SendMessageW(C(ID_SEQCOMBO), CB_GETLBTEXT, si, (LPARAM)b); g_set.seqScheme = b; }
  int pi = (int)SendMessageW(C(ID_PAPER), CB_GETCURSEL, 0, 0);
  if (pi >= 0 && pi < (int)Papers().size()) g_set.paperSize = Papers()[pi].id;
  g_set.preview = Checked(ID_PREVIEW); g_set.toPrinter = Checked(ID_TOPRINTER); g_set.toPdf = Checked(ID_TOPDF);
  g_set.monoOnly = Checked(ID_MONO);
  g_set.bold = Checked(ID_BOLD); g_set.italic = Checked(ID_ITALIC); g_set.underline = Checked(ID_UNDER); g_set.strike = Checked(ID_STRIKE);
  g_set.fontSize = GetSpin(C(ID_SIZE), g_set.fontSize);
  g_set.cpi = std::max(5.0, std::min(60.0, GetFloat(C(ID_CPI), g_set.cpi)));
  g_set.lpi = std::max(1.0, std::min(30.0, GetFloat(C(ID_LPI), g_set.lpi)));
  g_set.mTop = GetSpin(C(ID_MTOP), g_set.mTop); g_set.mBottom = GetSpin(C(ID_MBOT), g_set.mBottom);
  g_set.mLeft = GetSpin(C(ID_MLEFT), g_set.mLeft); g_set.mRight = GetSpin(C(ID_MRIGHT), g_set.mRight);
  int ci = (int)SendMessageW(C(ID_CPCOMBO), CB_GETCURSEL, 0, 0);
  if (ci >= 0 && ci < (int)g_cps.size()) g_set.codepage = g_cps[ci].first;
  g_set.runMinimized = Checked(ID_RUNMIN);
  UpdateSample();
  UpdateButtons();
  g_set.Save();
}

// ------------------------------------------------------------------ build UI
static void BuildUi() {
  g_loading = true;   // control creation fires EN_CHANGE; ignore until LoadUi finishes
  for (int k = 0; k < 2; k++) g_page[k].clear();
  g_tab = MakeCtl(g_wnd, WC_TABCONTROLW, L"", WS_CLIPSIBLINGS | WS_TABSTOP, 6, 6, 636, 508, ID_TAB);
  TCITEMW ti = {TCIF_TEXT};
  ws t1 = lang::T("TabMain"), t2 = lang::T("TabFonts");
  ti.pszText = (LPWSTR)t1.c_str(); TabCtrl_InsertItem(g_tab, 0, &ti);
  ti.pszText = (LPWSTR)t2.c_str(); TabCtrl_InsertItem(g_tab, 1, &ti);

  // ---- Main page
  Ctl(0, L"BUTTON", lang::T("Help"), WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 90, 26, ID_HELP);
  Ctl(0, L"BUTTON", lang::T("PrintFile"), WS_TABSTOP | BS_PUSHBUTTON, 96, 0, 110, 26, ID_PRINTFILE);
  Ctl(0, L"BUTTON", lang::T("Service"), WS_TABSTOP | BS_PUSHBUTTON, 212, 0, 90, 26, ID_SERVICE);

  Ctl(0, L"BUTTON", lang::T("SpoolParam"), BS_GROUPBOX, 0, 34, 304, 138, 0);
  Ctl(0, L"STATIC", lang::T("SpoolFile"), 0, 10, 52, 284, 16, 0);
  Ctl(0, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 10, 70, 284, 22, ID_SPOOL, WS_EX_CLIENTEDGE);
  HWND tr = Ctl(0, TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_AUTOTICKS | TBS_BOTH, 10, 96, 284, 30, ID_TRACK);
  SendMessageW(tr, TBM_SETRANGE, TRUE, MAKELONG(1, 30));
  Ctl(0, L"BUTTON", L"", WS_TABSTOP | BS_AUTOCHECKBOX, 10, 128, 284, 18, ID_AUTOPRINT);
  Ctl(0, L"BUTTON", lang::T("PrintJob"), WS_TABSTOP | BS_PUSHBUTTON, 10, 146, 138, 22, ID_PRINTJOB);
  Ctl(0, L"BUTTON", lang::T("ClearJob"), WS_TABSTOP | BS_PUSHBUTTON, 156, 146, 138, 22, ID_CLEARJOB);

  Ctl(0, L"BUTTON", lang::T("Orientation"), BS_GROUPBOX, 312, 34, 296, 138, 0);
  Ctl(0, L"STATIC", L"", SS_OWNERDRAW, 322, 54, 110, 108, ID_ORIENT);
  Ctl(0, L"BUTTON", lang::T("Portrait"), WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP, 450, 70, 140, 20, ID_PORTRAIT);
  Ctl(0, L"BUTTON", lang::T("Landscape"), BS_AUTORADIOBUTTON, 450, 100, 140, 20, ID_LANDSCAPE);

  Ctl(0, L"BUTTON", lang::T("TextParam"), BS_GROUPBOX, 0, 178, 304, 100, 0);
  Ctl(0, L"BUTTON", lang::T("WordWrap"), WS_TABSTOP | BS_AUTOCHECKBOX, 10, 196, 284, 18, ID_WRAP);
  Ctl(0, L"BUTTON", lang::T("FormFeed"), WS_TABSTOP | BS_AUTOCHECKBOX, 10, 215, 284, 18, ID_FF);
  Ctl(0, L"BUTTON", lang::T("LineFeed"), WS_TABSTOP | BS_AUTOCHECKBOX, 10, 234, 284, 18, ID_LFCRLF);
  Ctl(0, L"BUTTON", lang::T("CRasCRLF"), WS_TABSTOP | BS_AUTOCHECKBOX, 10, 253, 284, 18, ID_CRCRLF);

  Ctl(0, L"BUTTON", lang::T("Printer"), BS_GROUPBOX, 312, 178, 296, 100, 0);
  Ctl(0, L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON, 322, 196, 276, 26, ID_PRINTERBTN);
  Ctl(0, L"STATIC", lang::T("Copies") + L":", 0, 322, 233, 60, 16, 0);
  MakeSpin(g_wnd, ID_COPIES, 386 + 14, 229 + 38, 56, 22, 1, 999, 1);
  Add(0, C(ID_COPIES)); Add(0, C(ID_COPIES + 1000));
  Ctl(0, L"BUTTON", lang::T("RawPrintingMode"), WS_TABSTOP | BS_AUTOCHECKBOX, 322, 256, 276, 18, ID_RAW);

  Ctl(0, L"BUTTON", lang::T("PortSelect"), BS_GROUPBOX, 0, 284, 304, 118, 0);
  for (int i = 0; i < 12; i++)
    Ctl(0, L"BUTTON", Capture::PortName(i), WS_TABSTOP | BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0), 12 + (i % 4) * 72, 304 + (i / 4) * 22, 68, 20, ID_PORT0 + i);
  Ctl(0, L"BUTTON", lang::T("Capture"), WS_TABSTOP | BS_AUTOCHECKBOX, 12, 376, 280, 20, ID_CAPTURE);

  Ctl(0, L"BUTTON", lang::T("Sequences"), BS_GROUPBOX, 312, 284, 296, 54, 0);
  Ctl(0, L"BUTTON", lang::T("Sequences"), WS_TABSTOP | BS_AUTOCHECKBOX, 322, 304, 100, 20, ID_SEQEMU);
  Ctl(0, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 424, 303, 140, 200, ID_SEQCOMBO);
  Ctl(0, L"BUTTON", L"...", WS_TABSTOP | BS_PUSHBUTTON, 570, 302, 28, 24, ID_SEQEDIT);

  Ctl(0, L"BUTTON", lang::T("Papersize"), BS_GROUPBOX, 312, 342, 296, 50, 0);
  Ctl(0, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 322, 360, 276, 220, ID_PAPER);

  Ctl(0, L"BUTTON", lang::T("Printpreview"), WS_TABSTOP | BS_AUTOCHECKBOX, 0, 410, 130, 20, ID_PREVIEW);
  Ctl(0, L"BUTTON", lang::T("ViewLastPreview"), WS_TABSTOP | BS_PUSHBUTTON, 134, 406, 170, 26, ID_LASTPREVIEW);
  Ctl(0, L"BUTTON", lang::T("Printer"), WS_TABSTOP | BS_AUTOCHECKBOX, 312, 410, 80, 20, ID_TOPRINTER);
  Ctl(0, L"BUTTON", L"PDF", WS_TABSTOP | BS_AUTOCHECKBOX, 396, 410, 56, 20, ID_TOPDF);
  Ctl(0, L"BUTTON", lang::T("SaveintoPDF"), WS_TABSTOP | BS_PUSHBUTTON, 456, 406, 152, 26, ID_PDFSET);

  // ---- Fonts page
  Ctl(1, L"BUTTON", lang::T("Font"), BS_GROUPBOX, 0, 4, 304, 262, 0);
  Ctl(1, L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 10, 22, 284, 208, ID_FONTLIST, WS_EX_CLIENTEDGE);
  Ctl(1, L"BUTTON", lang::T("MonoSpaceOnly"), WS_TABSTOP | BS_AUTOCHECKBOX, 10, 238, 284, 20, ID_MONO);

  Ctl(1, L"BUTTON", lang::T("FontPreview"), BS_GROUPBOX, 312, 4, 296, 90, 0);
  Ctl(1, L"STATIC", L"", SS_CENTER | SS_CENTERIMAGE | SS_SUNKEN, 322, 22, 276, 62, ID_SAMPLE);

  Ctl(1, L"BUTTON", lang::T("FontStyle"), BS_GROUPBOX, 312, 100, 296, 66, 0);
  Ctl(1, L"BUTTON", lang::T("Bold"), WS_TABSTOP | BS_AUTOCHECKBOX, 322, 120, 130, 18, ID_BOLD);
  Ctl(1, L"BUTTON", lang::T("Italic"), WS_TABSTOP | BS_AUTOCHECKBOX, 460, 120, 130, 18, ID_ITALIC);
  Ctl(1, L"BUTTON", lang::T("Underline"), WS_TABSTOP | BS_AUTOCHECKBOX, 322, 142, 130, 18, ID_UNDER);
  Ctl(1, L"BUTTON", lang::T("Strikeout"), WS_TABSTOP | BS_AUTOCHECKBOX, 460, 142, 130, 18, ID_STRIKE);

  Ctl(1, L"BUTTON", lang::T("Settings"), BS_GROUPBOX, 0, 272, 304, 92, 0);
  Ctl(1, L"STATIC", lang::T("Size") + L":", 0, 10, 296, 40, 16, 0);
  MakeSpin(g_wnd, ID_SIZE, 52 + 14, 292 + 38, 56, 22, 4, 72, 11);
  Add(1, C(ID_SIZE)); Add(1, C(ID_SIZE + 1000));
  Ctl(1, L"STATIC", lang::T("CPI"), 0, 120, 296, 36, 16, 0);
  HWND ecpi = MakeFloatSpin(g_wnd, ID_CPI, 158 + 14, 292 + 38, 70, 22, 10, 0.5, 5, 60);
  Add(1, ecpi); Add(1, C(ID_CPI + 1000));
  Ctl(1, L"STATIC", lang::T("LPI"), 0, 120, 328, 36, 16, 0);
  HWND elpi = MakeFloatSpin(g_wnd, ID_LPI, 158 + 14, 324 + 38, 70, 22, 6, 0.5, 1, 30);
  Add(1, elpi); Add(1, C(ID_LPI + 1000));
  Ctl(1, L"BUTTON", lang::T("Reset"), WS_TABSTOP | BS_PUSHBUTTON, 10, 324, 80, 24, ID_RESET);

  Ctl(1, L"BUTTON", lang::T("PageParam"), BS_GROUPBOX, 312, 172, 296, 100, 0);
  Ctl(1, L"STATIC", lang::T("TopMargin"), 0, 322, 194, 90, 16, 0);
  MakeSpin(g_wnd, ID_MTOP, 412 + 14, 190 + 38, 60, 22, 0, 200, 5);
  Ctl(1, L"STATIC", lang::T("BottomMargin"), 0, 322, 222, 90, 16, 0);
  MakeSpin(g_wnd, ID_MBOT, 412 + 14, 218 + 38, 60, 22, 0, 200, 5);
  Ctl(1, L"STATIC", lang::T("Leftmargin"), 0, 322 + 150, 194, 60, 16, 0);
  MakeSpin(g_wnd, ID_MLEFT, 532 + 14 - 10, 190 + 38, 60, 22, 0, 200, 3);
  Ctl(1, L"STATIC", lang::T("RightMargin"), 0, 322 + 150, 222, 60, 16, 0);
  MakeSpin(g_wnd, ID_MRIGHT, 532 + 14 - 10, 218 + 38, 60, 22, 0, 200, 3);
  for (int id : {ID_MTOP, ID_MBOT, ID_MLEFT, ID_MRIGHT}) { Add(1, C(id)); Add(1, C(id + 1000)); }

  Ctl(1, L"BUTTON", lang::T("CodePageEmulation"), BS_GROUPBOX, 312, 278, 296, 86, 0);
  Ctl(1, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 322, 298, 276, 260, ID_CPCOMBO);
  Ctl(1, L"BUTTON", lang::T("ASCIITable"), WS_TABSTOP | BS_PUSHBUTTON, 322, 328, 276, 26, ID_ASCII);

  // ---- outside the tabs
  MakeCtl(g_wnd, L"BUTTON", lang::T("Minimize"), WS_TABSTOP | BS_AUTOCHECKBOX, 14, 524, 150, 20, ID_RUNMIN);
  MakeCtl(g_wnd, L"STATIC", lang::T("InterfaceLanguage"), SS_RIGHT, 300, 526, 160, 16, 0);
  HWND lc = MakeCtl(g_wnd, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 466, 521, 170, 200, ID_LANG);
  for (auto& l : lang::Available()) SendMessageW(lc, CB_ADDSTRING, 0, (LPARAM)l.c_str());
  if (SendMessageW(lc, CB_SELECTSTRING, -1, (LPARAM)g_set.language.c_str()) == CB_ERR) SendMessageW(lc, CB_SETCURSEL, 0, 0);
  SendMessageW(g_wnd, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
  LoadUi();
  ShowPage(TabCtrl_GetCurSel(g_tab));
}

static void RebuildUi() {
  int sel = g_tab ? TabCtrl_GetCurSel(g_tab) : 0;
  HWND c = GetWindow(g_wnd, GW_CHILD);
  std::vector<HWND> kids;
  while (c) { kids.push_back(c); c = GetWindow(c, GW_HWNDNEXT); }
  for (HWND k : kids) DestroyWindow(k);
  BuildUi();
  TabCtrl_SetCurSel(g_tab, sel);
  ShowPage(sel);
  SetWindowTextW(g_wnd, L"OpenDOSPRN");
}

// ------------------------------------------------------------------ jobs
static void PrintSpool(int i) {
  if (g_busy || !SpoolHasDataFor(i)) return;
  g_busy = true;
  UpdateButtons();
  ws sp = SpoolPathFor(i);
  ws tmp = sp + L".job";
  std::string data;
  DeleteFileW(tmp.c_str());
  if (MoveFileExW(sp.c_str(), tmp.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    util::ReadFileBytes(tmp, data);
    DeleteFileW(tmp.c_str());
  } else {
    util::ReadFileBytes(sp, data);
    DeleteFileW(sp.c_str());
  }
  g_profiles[g_set.portIndex].TakePortFields(g_set);
  Settings job = g_set.ForPort(i);
  RunJob(g_wnd, data, PortLabel(i) + L"_" + util::FileBaseName(sp), job);
  g_busy = false;
  UpdateButtons();
}
static void PrintSpool() { PrintSpool(g_set.portIndex); }

static void ClearSpool() {
  DeleteFileW(SpoolPath().c_str());
  UpdateButtons();
}

static void ShowMain() {
  ShowWindow(g_wnd, SW_SHOW);
  ShowWindow(g_wnd, SW_RESTORE);
  SetForegroundWindow(g_wnd);
}

static bool ConfirmExit() {
  if (!g_set.confirmExit) return true;
  ws t = lang::T("ExitLine1") + L"\n" + lang::T("ExitLine2");
  return MessageBoxW(g_wnd, t.c_str(), lang::T("ExitTitle").c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES;
}

static void TrayMenu() {
  HMENU m = CreatePopupMenu();
  HMENU lay = CreatePopupMenu();
  AppendMenuW(m, MF_STRING, ID_M_OPEN, lang::T("MenuOpen").c_str());
  AppendMenuW(m, MF_STRING, ID_M_PRINTFILE, lang::T("PrintFile").c_str());
  AppendMenuW(m, MF_STRING | (SpoolHasData() ? 0 : MF_GRAYED), ID_M_PRINT, lang::T("PrintJob").c_str());
  AppendMenuW(lay, MF_STRING | (!g_set.landscape ? MF_CHECKED : 0), ID_M_PORTRAIT, lang::T("Portrait").c_str());
  AppendMenuW(lay, MF_STRING | (g_set.landscape ? MF_CHECKED : 0), ID_M_LANDSCAPE, lang::T("Landscape").c_str());
  AppendMenuW(m, MF_POPUP, (UINT_PTR)lay, lang::T("Orientation").c_str());
  AppendMenuW(m, MF_STRING | (g_set.capture ? MF_CHECKED : 0), ID_M_CAPTURE, (lang::T("CaptureOn") + L" " + PortLabel(g_set.portIndex)).c_str());
  AppendMenuW(m, MF_STRING | (SpoolHasData() ? 0 : MF_GRAYED), ID_M_CANCELJOB, lang::T("ClearJob").c_str());
  AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(m, MF_STRING, ID_M_EXIT, lang::T("MenuExit").c_str());
  POINT pt;
  GetCursorPos(&pt);
  SetForegroundWindow(g_wnd);
  int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, g_wnd, nullptr);
  DestroyMenu(m);
  switch (cmd) {
    case ID_M_OPEN: ShowMain(); break;
    case ID_M_PRINTFILE: ShowMain(); ShowPrintFileDialog(g_wnd); break;
    case ID_M_PRINT: PrintSpool(); break;
    case ID_M_PORTRAIT: g_set.landscape = false; g_set.Save(); LoadUi(); break;
    case ID_M_LANDSCAPE: g_set.landscape = true; g_set.Save(); LoadUi(); break;
    case ID_M_CAPTURE: g_set.capture = !g_set.capture; g_set.Save(); LoadUi(); StartStopCapture(g_set.portIndex); break;
    case ID_M_CANCELJOB: ClearSpool(); break;
    case ID_M_EXIT: if (ConfirmExit()) { g_quitting = true; DestroyWindow(g_wnd); } break;
  }
}

static void DrawOrient(DRAWITEMSTRUCT* d) {
  FillRect(d->hDC, &d->rcItem, (HBRUSH)(COLOR_BTNFACE + 1));
  int w = d->rcItem.right - d->rcItem.left, h = d->rcItem.bottom - d->rcItem.top;
  int pw = g_set.landscape ? 96 : 68, ph = g_set.landscape ? 68 : 96;
  RECT r = {d->rcItem.left + (w - pw) / 2, d->rcItem.top + (h - ph) / 2, 0, 0};
  r.right = r.left + pw; r.bottom = r.top + ph;
  FillRect(d->hDC, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
  FrameRect(d->hDC, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
  HPEN pen = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
  HGDIOBJ old = SelectObject(d->hDC, pen);
  for (int y = r.top + 8; y < r.bottom - 6; y += 6) { MoveToEx(d->hDC, r.left + 6, y, nullptr); LineTo(d->hDC, r.right - 6, y); }
  SelectObject(d->hDC, old);
  DeleteObject(pen);
}

static void OpenHelp() {
  ws p = util::AppDir() + L"Help\\readme.htm";
  if (!util::FileExists(p)) p = util::AppDir() + L"README.md";
  ShellExecuteW(g_wnd, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_CREATE:
      g_wnd = h;
      BuildUi();
      return 0;
    case WM_NOTIFY: {
      NMHDR* nm = (NMHDR*)l;
      if (nm->idFrom == ID_TAB && nm->code == TCN_SELCHANGE) { ShowPage(TabCtrl_GetCurSel(g_tab)); return 0; }
      LRESULT r = 0;
      if (FloatSpinNotify(h, l, r)) return r;
      break;
    }
    case WM_DRAWITEM:
      if (w == ID_ORIENT) { DrawOrient((DRAWITEMSTRUCT*)l); return TRUE; }
      break;
    case WM_HSCROLL:
      if ((HWND)l == C(ID_TRACK)) PullUi();
      return 0;
    case WM_COMMAND: {
      int id = LOWORD(w), code = HIWORD(w);
      if (g_loading) return 0;
      if ((id == ID_SPOOL || id == ID_SIZE || id == ID_CPI || id == ID_LPI || id == ID_COPIES || (id >= ID_MTOP && id <= ID_MRIGHT)) &&
          code == EN_CHANGE) { PullUi(); return 0; }
      switch (id) {
        case ID_HELP: OpenHelp(); break;
        case ID_PRINTFILE: ShowPrintFileDialog(h); break;
        case ID_SERVICE: ShowServiceDialog(h); break;
        case ID_PRINTJOB: PrintSpool(); break;
        case ID_CLEARJOB: ClearSpool(); break;
        case ID_PORTRAIT: case ID_LANDSCAPE: PullUi(); InvalidateRect(C(ID_ORIENT), nullptr, TRUE); break;
        case ID_AUTOPRINT: case ID_WRAP: case ID_FF: case ID_LFCRLF: case ID_CRCRLF: case ID_SEQEMU:
        case ID_PREVIEW: case ID_TOPRINTER: case ID_TOPDF: case ID_BOLD: case ID_ITALIC: case ID_UNDER: case ID_STRIKE:
        case ID_RUNMIN: PullUi(); break;
        case ID_PAPER: case ID_SEQCOMBO: case ID_CPCOMBO: if (code == CBN_SELCHANGE) PullUi(); break;
        case ID_MONO: {
          PullUi();
          HWND fl = C(ID_FONTLIST);
          SendMessageW(fl, LB_RESETCONTENT, 0, 0);
          for (auto& f : EnumFonts(g_set.monoOnly)) SendMessageW(fl, LB_ADDSTRING, 0, (LPARAM)f.c_str());
          SendMessageW(fl, LB_SELECTSTRING, -1, (LPARAM)g_set.fontName.c_str());
          break;
        }
        case ID_FONTLIST:
          if (code == LBN_SELCHANGE) {
            int i = (int)SendMessageW(C(ID_FONTLIST), LB_GETCURSEL, 0, 0);
            if (i >= 0) { wchar_t b[LF_FACESIZE + 8]; SendMessageW(C(ID_FONTLIST), LB_GETTEXT, i, (LPARAM)b); g_set.fontName = b; g_set.Save(); UpdateSample(); }
          }
          break;
        case ID_RESET:
          g_set.fontSize = 11; g_set.cpi = 10; g_set.lpi = 6;
          LoadUi();
          g_set.Save();
          break;
        case ID_PRINTERBTN: {
          PrinterSel sel;
          sel.name = g_set.printerName;
          sel.devmode = g_set.devmode;
          if (SelectPrinterDialog(h, sel)) { g_set.printerName = sel.name; g_set.devmode = sel.devmode; g_set.Save(); LoadUi(); }
          break;
        }
        case ID_RAW:
          if (Checked(ID_RAW) && !g_set.rawAlertAsked) {
            if (MessageBoxW(h, lang::T("RawPrintingModeAlert").c_str(), L"OpenDOSPRN", MB_YESNO | MB_ICONQUESTION) != IDYES) { Check(ID_RAW, false); break; }
            g_set.rawAlertAsked = true;
          }
          g_set.rawMode = Checked(ID_RAW);
          g_set.Save();
          break;
        case ID_CAPTURE:
          g_set.capture = Checked(ID_CAPTURE);
          g_set.Save();
          StartStopCapture(g_set.portIndex);
          break;
        case ID_SEQEDIT: ShowSequencesDialog(h); break;
        case ID_LASTPREVIEW: ShowLastPreview(h); break;
        case ID_PDFSET: ShowPdfSettings(h); break;
        case ID_ASCII: ShowAsciiTable(h); break;
        case ID_LANG:
          if (code == CBN_SELCHANGE) {
            wchar_t b[128];
            int i = (int)SendMessageW(C(ID_LANG), CB_GETCURSEL, 0, 0);
            SendMessageW(C(ID_LANG), CB_GETLBTEXT, i, (LPARAM)b);
            g_set.language = b;
            g_set.Save();
            lang::Select(g_set.language);
            PostMessageW(h, WM_APP + 9, 0, 0);
          }
          break;
        default:
          if (id >= ID_PORT0 && id < ID_PORT0 + 12) SelectPort(id - ID_PORT0);
      }
      return 0;
    }
    case WM_APP + 9: RebuildUi(); return 0;
    case WM_CAPTURE_DATA: UpdateButtons(); return 0;
    case WM_TIMER:
      if (w == 1) {
        UpdateButtons();
        for (int i = 0; i < 12 && !g_busy; i++) {
          const Settings& p = Prof(i);
          if (p.autoPrint && g_caps[i].Running() && SpoolHasDataFor(i)) {
            ULONGLONG last = g_caps[i].LastActivity();
            if (last && GetTickCount64() - last >= (ULONGLONG)p.timeoutSec * 1000) PrintSpool(i);
          }
        }
      }
      return 0;
    case WM_TRAY:
      if (l == WM_LBUTTONDBLCLK) ShowMain();
      else if (l == WM_RBUTTONUP) TrayMenu();
      return 0;
    case WM_SYSCOMMAND:
      if ((w & 0xFFF0) == SC_MINIMIZE) { ShowWindow(h, SW_HIDE); return 0; }
      break;
    case WM_CLOSE:
      if (!g_quitting) { ShowWindow(h, SW_HIDE); return 0; }
      break;
    case WM_DESTROY:
      KillTimer(h, 1);
      for (auto& c : g_caps) c.Stop();
      Shell_NotifyIconW(NIM_DELETE, &g_nid);
      g_set.Save();
      PostQuitMessage(0);
      return 0;
    case WM_QUERYENDSESSION: return TRUE;
  }
  return DefWindowProcW(h, m, w, l);
}

static void RefreshAfterImport() {
  RebuildUi();
  StartAllCaptures();
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int) {
  g_hInst = hi;
  HANDLE mtx = CreateMutexW(nullptr, TRUE, L"OpenDOSPRN_SingleInstance");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND o = FindWindowW(L"OpenDOSPRN_Main", nullptr);
    if (o) { ShowWindow(o, SW_SHOW); SetForegroundWindow(o); }
    return 0;
  }
  CoInitialize(nullptr);
  lang::Init();
  InitUi();
  g_set.Load();
  lang::Select(g_set.language);
  g_cps = LoadCodePageList();
  g_refreshUi = RefreshAfterImport;

  WNDCLASSW wc = {};
  wc.lpfnWndProc = MainProc;
  wc.hInstance = hi;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = LoadIcon(hi, MAKEINTRESOURCE(1));
  if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszClassName = L"OpenDOSPRN_Main";
  RegisterClassW(&wc);
  DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
  RECT rc = {0, 0, 648, 556};
  AdjustWindowRect(&rc, style, FALSE);
  int W = rc.right - rc.left, H = rc.bottom - rc.top;
  HWND h = CreateWindowExW(0, L"OpenDOSPRN_Main", L"OpenDOSPRN", style, (GetSystemMetrics(SM_CXSCREEN) - W) / 2,
                           (GetSystemMetrics(SM_CYSCREEN) - H) / 2, W, H, nullptr, nullptr, hi, nullptr);
  g_nid = {};
  g_nid.cbSize = sizeof g_nid;
  g_nid.hWnd = h;
  g_nid.uID = 1;
  g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  g_nid.uCallbackMessage = WM_TRAY;
  HICON small = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
  SendMessageW(h, WM_SETICON, ICON_SMALL, (LPARAM)small);
  SendMessageW(h, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
  g_nid.hIcon = small ? small : wc.hIcon;
  wcsncpy(g_nid.szTip, lang::T("TrayIconHint").c_str(), 63);
  Shell_NotifyIconW(NIM_ADD, &g_nid);

  SetTimer(h, 1, 500, nullptr);
  StartAllCaptures();
  if (wcsstr(GetCommandLineW(), L"/fonts")) { TabCtrl_SetCurSel(g_tab, 1); ShowPage(1); }
  if (!g_set.runMinimized || wcsstr(GetCommandLineW(), L"/show")) ShowWindow(h, SW_SHOW);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(h, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
  CoUninitialize();
  CloseHandle(mtx);
  return 0;
}
