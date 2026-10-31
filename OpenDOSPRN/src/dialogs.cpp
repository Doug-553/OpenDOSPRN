#include "ui.h"

static void MsgBox(HWND o, const ws& t, UINT icon = MB_ICONINFORMATION) { MessageBoxW(o, t.c_str(), L"OpenDOSPRN", MB_OK | icon); }

static bool PickFile(HWND owner, bool save, ws& path) {
  wchar_t buf[MAX_PATH * 2] = {};
  OPENFILENAMEW of = {};
  of.lStructSize = sizeof of;
  of.hwndOwner = owner;
  static const wchar_t filt[] = L"INI Files (*.ini)\0*.ini\0All Files (*.*)\0*.*\0";
  of.lpstrFilter = filt;
  of.lpstrFile = buf;
  of.nMaxFile = MAX_PATH * 2;
  of.lpstrDefExt = L"ini";
  of.Flags = OFN_NOCHANGEDIR | (save ? (OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST) : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST));
  if (!(save ? GetSaveFileNameW(&of) : GetOpenFileNameW(&of))) return false;
  path = buf;
  return true;
}

// ------------------------------------------------------------------ Service
void ShowServiceDialog(HWND owner) {
  ModalCtx ctx;
  ctx.init = [&](HWND d) {
    MakeCtl(d, L"BUTTON", lang::T("Loadsettingsfromthefile"), WS_TABSTOP | BS_PUSHBUTTON, 14, 14, 330, 28, 401);
    MakeCtl(d, L"BUTTON", lang::T("Savesettingstofile"), WS_TABSTOP | BS_PUSHBUTTON, 14, 50, 330, 28, 402);
    MakeCtl(d, L"BUTTON", lang::T("Ok"), WS_TABSTOP | BS_DEFPUSHBUTTON, 268, 92, 76, 26, IDOK);
  };
  ctx.proc = [&](HWND d, UINT m, WPARAM w, LPARAM, LRESULT&) {
    if (m != WM_COMMAND) return false;
    ws p;
    switch (LOWORD(w)) {
      case 401:
        if (PickFile(d, false, p)) {
          if (g_set.LoadIni(p)) { g_set.Save(); lang::Select(g_set.language); if (g_refreshUi) g_refreshUi(); }
          else MsgBox(d, lang::T("FileNotFound"), MB_ICONWARNING);
        }
        return true;
      case 402:
        if (PickFile(d, true, p)) g_set.SaveIni(p);
        return true;
      case IDOK: case IDCANCEL: ctx.done = true; return true;
    }
    return false;
  };
  RunModal(owner, lang::T("Service"), 360, 130, ctx);
}

// ------------------------------------------------------------------ PDF settings
void ShowPdfSettings(HWND owner) {
  ModalCtx ctx;
  HWND eFolder = nullptr, ePat = nullptr, cbAuto = nullptr, cbOpen = nullptr;
  ctx.init = [&](HWND d) {
    MakeCtl(d, L"STATIC", lang::T("SavePDFtofolder"), 0, 12, 10, 430, 16, 0);
    eFolder = MakeCtl(d, L"EDIT", g_set.pdfFolder, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 12, 28, 390, 22, 501, WS_EX_CLIENTEDGE);
    MakeCtl(d, L"BUTTON", L"...", WS_TABSTOP | BS_PUSHBUTTON, 408, 27, 40, 24, 502);
    MakeCtl(d, L"STATIC", lang::T("PDFnametemplate"), 0, 12, 60, 430, 16, 0);
    ePat = MakeCtl(d, L"EDIT", g_set.pdfPattern, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 12, 78, 390, 22, 503, WS_EX_CLIENTEDGE);
    MakeCtl(d, L"BUTTON", lang::T("Reset"), WS_TABSTOP | BS_PUSHBUTTON, 408, 77, 40, 24, 504);
    ws help =
        L"yy\t= year, last 2 digits\r\nyyyy\t= year, 4 digits\r\nm\t= month number\r\nmm\t= month number, 2 digits\r\n"
        L"mmm\t= month name, short (Jan)\r\nmmmm\t= month name, long (January)\r\nd\t= day number\r\ndd\t= day number, 2 digits\r\n"
        L"hh\t= hour, 2 digits\r\nnn\t= minutes, 2 digits\r\nss\t= seconds, 2 digits\r\n"
        L"(text)\t= text in round brackets is used as is\r\n<fname>\t= name of the printed file\r\n"
        L"<fext>\t= extension of the printed file\r\n<uname>\t= Windows user name";
    MakeCtl(d, L"EDIT", help, WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 12, 108, 436, 112, 505, WS_EX_CLIENTEDGE);
    cbAuto = MakeCtl(d, L"BUTTON", lang::T("Savereportstopdfautomatically"), WS_TABSTOP | BS_AUTOCHECKBOX, 12, 230, 436, 20, 506);
    cbOpen = MakeCtl(d, L"BUTTON", lang::T("OpenAfterCreation"), WS_TABSTOP | BS_AUTOCHECKBOX, 12, 252, 436, 20, 507);
    SendMessageW(cbAuto, BM_SETCHECK, g_set.pdfAutoSave ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(cbOpen, BM_SETCHECK, g_set.pdfOpen ? BST_CHECKED : BST_UNCHECKED, 0);
    MakeCtl(d, L"BUTTON", lang::T("Ok"), WS_TABSTOP | BS_DEFPUSHBUTTON, 372, 282, 76, 26, IDOK);
  };
  ctx.proc = [&](HWND d, UINT m, WPARAM w, LPARAM, LRESULT&) {
    if (m != WM_COMMAND) return false;
    switch (LOWORD(w)) {
      case 502: {
        wchar_t buf[MAX_PATH] = {};
        BROWSEINFOW bi = {};
        bi.hwndOwner = d;
        bi.pszDisplayName = buf;
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        if (PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi)) {
          if (SHGetPathFromIDListW(pidl, buf)) SetText(eFolder, buf);
          CoTaskMemFree(pidl);
        }
        return true;
      }
      case 504: SetText(ePat, L"(dosprn)_yyyy_mm_dd_hh_nn_ss(.pdf)"); return true;
      case IDOK:
        g_set.pdfFolder = util::Trim(GetText(eFolder));
        g_set.pdfPattern = util::Trim(GetText(ePat));
        if (g_set.pdfPattern.empty()) g_set.pdfPattern = L"(dosprn)_yyyy_mm_dd_hh_nn_ss(.pdf)";
        g_set.pdfAutoSave = SendMessageW(cbAuto, BM_GETCHECK, 0, 0) == BST_CHECKED;
        g_set.pdfOpen = SendMessageW(cbOpen, BM_GETCHECK, 0, 0) == BST_CHECKED;
        g_set.Save();
        ctx.done = true;
        return true;
      case IDCANCEL: ctx.done = true; return true;
    }
    return false;
  };
  RunModal(owner, lang::T("PDFsettings"), 460, 318, ctx);
}

// ------------------------------------------------------------------ Escape sequences editor
void ShowSequencesDialog(HWND owner) {
  ModalCtx ctx;
  std::vector<SeqScheme> work = g_set.schemes;
  HWND cmb = nullptr, eInit = nullptr, lv = nullptr, eSeq = nullptr, bDel = nullptr;
  bool busy = false;
  int curFn = -1;
  const SeqFunc* fn = SeqFuncs();

  auto userScheme = [&]() -> SeqScheme* {
    int i = (int)SendMessageW(cmb, CB_GETCURSEL, 0, 0) - 2;
    return (i >= 0 && i < (int)work.size()) ? &work[i] : nullptr;
  };
  auto fillList = [&]() {
    busy = true;
    SeqScheme* sc = userScheme();
    ListView_DeleteAllItems(lv);
    for (int i = 0; i < kSeqFuncCount; i++) {
      LVITEMW it = {};
      it.mask = LVIF_TEXT;
      it.iItem = i;
      it.pszText = (LPWSTR)fn[i].name;
      ListView_InsertItem(lv, &it);
      ListView_SetItemText(lv, i, 1, (LPWSTR)(sc ? sc->codes[i].c_str() : L""));
      ws rd = ToReadable(fn[i].epson);
      ListView_SetItemText(lv, i, 2, (LPWSTR)rd.c_str());
    }
    SetText(eInit, sc ? sc->init : ws());
    SetText(eSeq, L"");
    EnableWindow(eInit, sc != nullptr);
    EnableWindow(eSeq, FALSE);
    EnableWindow(bDel, sc != nullptr);
    curFn = -1;
    busy = false;
  };
  auto fillCombo = [&](int sel) {
    SendMessageW(cmb, CB_RESETCONTENT, 0, 0);
    SendMessageW(cmb, CB_ADDSTRING, 0, (LPARAM)L"Epson ESC/P");
    SendMessageW(cmb, CB_ADDSTRING, 0, (LPARAM)L"No sequences");
    for (auto& s : work) SendMessageW(cmb, CB_ADDSTRING, 0, (LPARAM)s.name.c_str());
    SendMessageW(cmb, CB_SETCURSEL, sel, 0);
    fillList();
  };

  ctx.init = [&](HWND d) {
    MakeCtl(d, L"STATIC", lang::T("Scheme"), 0, 12, 10, 300, 16, 0);
    cmb = MakeCtl(d, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 12, 28, 300, 200, 601);
    MakeCtl(d, L"BUTTON", lang::T("Add"), WS_TABSTOP | BS_PUSHBUTTON, 322, 27, 100, 24, 602);
    bDel = MakeCtl(d, L"BUTTON", lang::T("Delete"), WS_TABSTOP | BS_PUSHBUTTON, 430, 27, 100, 24, 603);
    MakeCtl(d, L"STATIC", lang::T("Initialize"), 0, 12, 60, 300, 16, 0);
    eInit = MakeCtl(d, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 12, 78, 518, 22, 604, WS_EX_CLIENTEDGE);
    lv = MakeCtl(d, WC_LISTVIEWW, L"", WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 12, 108, 518, 240, 605,
                 WS_EX_CLIENTEDGE);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    LVCOLUMNW c = {};
    c.mask = LVCF_TEXT | LVCF_WIDTH;
    c.cx = 190; c.pszText = (LPWSTR)lang::T("Function").c_str();
    static ws h1, h2, h3;
    h1 = lang::T("Function"); h2 = lang::T("Esc"); h3 = L"Epson ESC/P";
    c.pszText = (LPWSTR)h1.c_str(); ListView_InsertColumn(lv, 0, &c);
    c.cx = 170; c.pszText = (LPWSTR)h2.c_str(); ListView_InsertColumn(lv, 1, &c);
    c.cx = 140; c.pszText = (LPWSTR)h3.c_str(); ListView_InsertColumn(lv, 2, &c);
    MakeCtl(d, L"STATIC", lang::T("Esc") + L":", 0, 12, 360, 90, 16, 0);
    eSeq = MakeCtl(d, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 100, 357, 430, 22, 606, WS_EX_CLIENTEDGE);
    MakeCtl(d, L"STATIC", L"\\e = ESC, \\xNN = hex byte, ^X = control char", 0, 12, 386, 518, 16, 0);
    MakeCtl(d, L"BUTTON", lang::T("Apply"), WS_TABSTOP | BS_DEFPUSHBUTTON, 346, 414, 90, 26, IDOK);
    MakeCtl(d, L"BUTTON", lang::T("Cancel"), WS_TABSTOP | BS_PUSHBUTTON, 440, 414, 90, 26, IDCANCEL);
    int sel = 0;
    if (_wcsicmp(g_set.seqScheme.c_str(), L"No sequences") == 0) sel = 1;
    for (size_t i = 0; i < work.size(); i++) if (_wcsicmp(work[i].name.c_str(), g_set.seqScheme.c_str()) == 0) sel = (int)i + 2;
    fillCombo(sel);
  };

  ctx.proc = [&](HWND d, UINT m, WPARAM w, LPARAM l, LRESULT& res) {
    if (m == WM_NOTIFY) {
      NMHDR* nm = (NMHDR*)l;
      if (nm->idFrom == 605 && nm->code == LVN_ITEMCHANGED) {
        NMLISTVIEW* n = (NMLISTVIEW*)l;
        if ((n->uChanged & LVIF_STATE) && (n->uNewState & LVIS_SELECTED)) {
          curFn = n->iItem;
          SeqScheme* sc = userScheme();
          busy = true;
          SetText(eSeq, sc ? sc->codes[curFn] : ws());
          EnableWindow(eSeq, sc != nullptr);
          busy = false;
        }
        return true;
      }
      return false;
    }
    if (m != WM_COMMAND) return false;
    int id = LOWORD(w), code = HIWORD(w);
    if (id == 601 && code == CBN_SELCHANGE) { fillList(); return true; }
    if (id == 604 && code == EN_CHANGE && !busy) {
      if (SeqScheme* sc = userScheme()) sc->init = GetText(eInit);
      return true;
    }
    if (id == 606 && code == EN_CHANGE && !busy) {
      SeqScheme* sc = userScheme();
      if (sc && curFn >= 0) {
        sc->codes[curFn] = GetText(eSeq);
        ListView_SetItemText(lv, curFn, 1, (LPWSTR)sc->codes[curFn].c_str());
      }
      return true;
    }
    if (id == 602) {
      ws name;
      if (PromptText(d, lang::T("Add"), lang::T("SchemeName"), name)) {
        bool bad = name.find(L'|') != ws::npos || _wcsicmp(name.c_str(), L"Epson ESC/P") == 0 || _wcsicmp(name.c_str(), L"No sequences") == 0;
        for (auto& s : work) if (_wcsicmp(s.name.c_str(), name.c_str()) == 0) bad = true;
        if (bad) { MsgBox(d, lang::T("Name"), MB_ICONWARNING); return true; }
        SeqScheme sc;
        sc.name = name;
        if (SeqScheme* cur = userScheme()) { sc.init = cur->init; sc.codes = cur->codes; }
        work.push_back(sc);
        fillCombo((int)work.size() + 1);
      }
      return true;
    }
    if (id == 603) {
      int i = (int)SendMessageW(cmb, CB_GETCURSEL, 0, 0) - 2;
      if (i >= 0 && i < (int)work.size() &&
          MessageBoxW(d, lang::T("ConfirmDeleteScheme").c_str(), L"OpenDOSPRN", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        work.erase(work.begin() + i);
        fillCombo(0);
      }
      return true;
    }
    if (id == IDOK) {
      g_set.schemes = work;
      int sel = (int)SendMessageW(cmb, CB_GETCURSEL, 0, 0);
      g_set.seqScheme = sel == 0 ? L"Epson ESC/P" : sel == 1 ? L"No sequences" : work[sel - 2].name;
      g_set.Save();
      if (g_refreshUi) g_refreshUi();
      ctx.done = true;
      return true;
    }
    if (id == IDCANCEL) { ctx.done = true; return true; }
    (void)res;
    return false;
  };
  RunModal(owner, lang::T("Sequences"), 544, 454, ctx);
}

// ------------------------------------------------------------------ Print file
void ShowPrintFileDialog(HWND owner) {
  ModalCtx ctx;
  Settings tmp = g_set;
  HWND eFile, lblPrn, eCopies, rbP, rbL, cFont, eSize, eCpi, eLpi, eT, eB, eL, eR;
  auto pull = [&]() {
    tmp.copies = GetSpin(eCopies, 1);
    tmp.landscape = SendMessageW(rbL, BM_GETCHECK, 0, 0) == BST_CHECKED;
    int fi = (int)SendMessageW(cFont, CB_GETCURSEL, 0, 0);
    wchar_t fb[LF_FACESIZE + 1] = {};
    if (fi >= 0) SendMessageW(cFont, CB_GETLBTEXT, fi, (LPARAM)fb); else GetWindowTextW(cFont, fb, LF_FACESIZE);
    if (fb[0]) tmp.fontName = fb;
    tmp.fontSize = GetSpin(eSize, tmp.fontSize);
    tmp.cpi = GetFloat(eCpi, tmp.cpi);
    tmp.lpi = GetFloat(eLpi, tmp.lpi);
    tmp.mTop = GetSpin(eT, tmp.mTop); tmp.mBottom = GetSpin(eB, tmp.mBottom);
    tmp.mLeft = GetSpin(eL, tmp.mLeft); tmp.mRight = GetSpin(eR, tmp.mRight);
  };
  auto showPrinter = [&]() {
    ws n = tmp.printerName.empty() ? DefaultPrinterName() : tmp.printerName;
    SetText(lblPrn, n.empty() ? lang::T("NoPrinters") : n);
  };
  ctx.init = [&](HWND d) {
    MakeCtl(d, L"STATIC", lang::T("FileName"), 0, 12, 12, 120, 16, 0);
    eFile = MakeCtl(d, L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 12, 30, 440, 22, 701, WS_EX_CLIENTEDGE);
    MakeCtl(d, L"BUTTON", L"...", WS_TABSTOP | BS_PUSHBUTTON, 458, 29, 50, 24, 702);
    lblPrn = MakeCtl(d, L"STATIC", L"", SS_CENTERIMAGE | SS_PATHELLIPSIS, 12, 64, 300, 22, 0);
    MakeCtl(d, L"BUTTON", lang::T("Select"), WS_TABSTOP | BS_PUSHBUTTON, 318, 63, 190, 24, 703);
    MakeCtl(d, L"STATIC", lang::T("Copies") + L":", 0, 12, 100, 70, 16, 0);
    eCopies = MakeSpin(d, 704, 84, 97, 60, 22, 1, 999, tmp.copies);
    MakeCtl(d, L"BUTTON", lang::T("Orientation"), BS_GROUPBOX, 12, 128, 240, 62, 0);
    rbP = MakeCtl(d, L"BUTTON", lang::T("Portrait"), WS_TABSTOP | BS_AUTORADIOBUTTON | WS_GROUP, 26, 148, 100, 20, 705);
    rbL = MakeCtl(d, L"BUTTON", lang::T("Landscape"), BS_AUTORADIOBUTTON, 136, 148, 100, 20, 706);
    SendMessageW(tmp.landscape ? rbL : rbP, BM_SETCHECK, BST_CHECKED, 0);
    MakeCtl(d, L"BUTTON", lang::T("Font"), BS_GROUPBOX, 12, 198, 496, 90, 0);
    cFont = MakeCtl(d, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL, 26, 220, 220, 220, 707);
    for (auto& f : EnumFonts(tmp.monoOnly)) SendMessageW(cFont, CB_ADDSTRING, 0, (LPARAM)f.c_str());
    if (SendMessageW(cFont, CB_SELECTSTRING, -1, (LPARAM)tmp.fontName.c_str()) == CB_ERR) SetText(cFont, tmp.fontName);
    MakeCtl(d, L"STATIC", lang::T("Size") + L":", 0, 262, 224, 40, 16, 0);
    eSize = MakeSpin(d, 708, 304, 220, 54, 22, 4, 72, tmp.fontSize);
    MakeCtl(d, L"STATIC", lang::T("CPI"), 0, 26, 256, 40, 16, 0);
    eCpi = MakeFloatSpin(d, 709, 66, 252, 80, 22, tmp.cpi, 0.5, 5, 60);
    MakeCtl(d, L"STATIC", lang::T("LPI"), 0, 170, 256, 40, 16, 0);
    eLpi = MakeFloatSpin(d, 711, 210, 252, 80, 22, tmp.lpi, 0.5, 1, 30);
    MakeCtl(d, L"BUTTON", lang::T("Margins") + L", mm", BS_GROUPBOX, 12, 296, 496, 56, 0);
    MakeCtl(d, L"STATIC", lang::T("TopMargin"), 0, 22, 312, 60, 16, 0);
    eT = MakeSpin(d, 712, 22, 328, 56, 20, 0, 200, tmp.mTop);
    MakeCtl(d, L"STATIC", lang::T("BottomMargin"), 0, 142, 312, 70, 16, 0);
    eB = MakeSpin(d, 713, 142, 328, 56, 20, 0, 200, tmp.mBottom);
    MakeCtl(d, L"STATIC", lang::T("Leftmargin"), 0, 262, 312, 70, 16, 0);
    eL = MakeSpin(d, 714, 262, 328, 56, 20, 0, 200, tmp.mLeft);
    MakeCtl(d, L"STATIC", lang::T("RightMargin"), 0, 382, 312, 70, 16, 0);
    eR = MakeSpin(d, 715, 382, 328, 56, 20, 0, 200, tmp.mRight);
    MakeCtl(d, L"BUTTON", L"Save Settings", WS_TABSTOP | BS_PUSHBUTTON, 12, 366, 130, 26, 716);
    MakeCtl(d, L"BUTTON", lang::T("Print"), WS_TABSTOP | BS_DEFPUSHBUTTON, 332, 366, 84, 26, IDOK);
    MakeCtl(d, L"BUTTON", lang::T("Cancel"), WS_TABSTOP | BS_PUSHBUTTON, 424, 366, 84, 26, IDCANCEL);
    showPrinter();
  };
  ctx.proc = [&](HWND d, UINT m, WPARAM w, LPARAM l, LRESULT& res) {
    if (m == WM_NOTIFY) return FloatSpinNotify(d, l, res);
    if (m != WM_COMMAND) return false;
    switch (LOWORD(w)) {
      case 702: {
        wchar_t buf[MAX_PATH * 2] = {};
        OPENFILENAMEW of = {};
        of.lStructSize = sizeof of;
        of.hwndOwner = d;
        static const wchar_t filt[] = L"All Files (*.*)\0*.*\0";
        of.lpstrFilter = filt;
        of.lpstrFile = buf;
        of.nMaxFile = MAX_PATH * 2;
        of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&of)) SetText(eFile, buf);
        return true;
      }
      case 703: {
        PrinterSel sel;
        sel.name = tmp.printerName;
        sel.devmode = tmp.devmode;
        if (SelectPrinterDialog(d, sel)) { tmp.printerName = sel.name; tmp.devmode = sel.devmode; showPrinter(); }
        return true;
      }
      case 716:
        pull();
        g_set.copies = tmp.copies; g_set.landscape = tmp.landscape; g_set.fontName = tmp.fontName;
        g_set.fontSize = tmp.fontSize; g_set.cpi = tmp.cpi; g_set.lpi = tmp.lpi;
        g_set.mTop = tmp.mTop; g_set.mBottom = tmp.mBottom; g_set.mLeft = tmp.mLeft; g_set.mRight = tmp.mRight;
        g_set.printerName = tmp.printerName; g_set.devmode = tmp.devmode;
        g_set.Save();
        if (g_refreshUi) g_refreshUi();
        return true;
      case IDOK: {
        pull();
        ws path = util::Trim(GetText(eFile));
        std::string data;
        if (path.empty() || !util::ReadFileBytes(path, data)) { MsgBox(d, lang::T("FileNotFound") + L": " + path, MB_ICONWARNING); return true; }
        ws base = util::FileBaseName(path);
        ctx.done = true;
        RunJob(d, data, base, tmp);
        return true;
      }
      case IDCANCEL: ctx.done = true; return true;
    }
    return false;
  };
  RunModal(owner, lang::T("PrintFile"), 520, 404, ctx);
}
