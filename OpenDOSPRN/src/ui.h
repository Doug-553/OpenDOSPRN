#pragma once
#include "common.h"
#include "settings.h"
#include "lang.h"
#include "render.h"
#include "printing.h"
#include "pdf.h"
#include "capture.h"

extern HINSTANCE g_hInst;
extern HFONT g_uiFont;
extern void (*g_refreshUi)();   // called after settings were replaced (INI import, scheme editor)

void InitUi();
HWND MakeCtl(HWND parent, const wchar_t* cls, const ws& text, DWORD style, int x, int y, int w, int h, int id, DWORD ex = 0);
void SetText(HWND h, const ws& t);
ws GetText(HWND h);
void SetFontTree(HWND root);

struct ModalCtx {
  std::function<void(HWND)> init;
  std::function<bool(HWND, UINT, WPARAM, LPARAM, LRESULT&)> proc;
  bool done = false;
  HWND hwnd = nullptr;
};
void RunModal(HWND parent, const ws& title, int clientW, int clientH, ModalCtx& ctx);
bool PromptText(HWND parent, const ws& title, const ws& label, ws& text);
ws ToReadable(const std::string& bytes);     // "ESC @", "SI" ...

// preview
struct PreviewIO {
  std::vector<Page>* pages = nullptr;
  const Settings* set = nullptr;
  ws sourceName;
  std::vector<int> sel;        // chosen pages when printing
};
bool ShowPreview(HWND owner, PreviewIO& io);   // true = the user pressed Print

// jobs
void RunJob(HWND owner, const std::string& data, const ws& sourceName, const Settings& s);
bool HaveLastPreview();
void ShowLastPreview(HWND owner);

// dialogs
void ShowServiceDialog(HWND owner);
void ShowPdfSettings(HWND owner);
void ShowSequencesDialog(HWND owner);
void ShowPrintFileDialog(HWND owner);
void ShowAsciiTable(HWND owner);

// numeric edit helpers
HWND MakeSpin(HWND parent, int id, int x, int y, int w, int h, int mn, int mx, int val);
HWND MakeFloatSpin(HWND parent, int id, int x, int y, int w, int h, double val, double step, double mn, double mx);
bool FloatSpinNotify(HWND parent, LPARAM nmhdr, LRESULT& result);   // call from WM_NOTIFY
double GetFloat(HWND edit, double def);
void SetFloat(HWND edit, double v);
int GetSpin(HWND edit, int def);
void SetSpin(HWND edit, int v);
