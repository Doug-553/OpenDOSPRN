#pragma once
#include "common.h"

// Number of translatable printer functions (see g_seqFuncs in escp.cpp)
static const int kSeqFuncCount = 19;

struct SeqScheme {
  ws name;
  ws init;                 // initialization string (escape notation: \e \xNN \n \r ^X)
  std::vector<ws> codes;   // one user sequence per function; empty = not mapped
  SeqScheme() : codes(kSeqFuncCount) {}
};

struct Archive {
  virtual ~Archive() {}
  virtual void s(const wchar_t* key, ws& v) = 0;
  virtual void i(const wchar_t* key, int& v) = 0;
  virtual void b(const wchar_t* key, bool& v) = 0;
  virtual void d(const wchar_t* key, double& v) = 0;
  virtual bool loading() const = 0;
};

struct Settings {
  // ---- global (shared by all ports)
  int portIndex = 0;            // port currently shown/edited in the UI (0..8 = LPT1..9, 9..11 = COM1..3)
  bool rawAlertAsked = false;
  bool runMinimized = true, confirmExit = true;
  ws language = L"English";
  bool monoOnly = true;
  std::vector<SeqScheme> schemes;   // user-defined schemes

  // ---- per-port profile (every port has its own copy)
  // spool / capture
  ws spoolName = L"%userprofile%\\prnspl";
  int timeoutSec = 3;
  bool autoPrint = true;
  bool capture = false;
  // layout
  bool landscape = false;
  int paperSize = 9;            // DMPAPER_A4
  // text
  bool wrapLong = false, useFF = true, lfAsCrLf = true, crAsCrLf = false;
  // printer
  ws printerName;
  int copies = 1;
  bool rawMode = false;
  std::vector<BYTE> devmode;    // printer setup from the dialog (session only)
  // sequences
  bool emulateSeq = true;
  ws seqScheme = L"Epson ESC/P";
  // output
  bool preview = true, toPrinter = true, toPdf = false;
  ws pdfFolder;
  ws pdfPattern = L"(dosprn)_yyyy_mm_dd_hh_nn_ss(.pdf)";
  bool pdfAutoSave = false, pdfOpen = false;
  // fonts / page
  ws fontName = L"Courier New";
  int fontSize = 11;
  bool bold = false, italic = false, underline = false, strike = false;
  double cpi = 10.0, lpi = 6.0;
  int mTop = 5, mBottom = 5, mLeft = 3, mRight = 3;   // mm
  ws codepage = L"850";

  void SerializeGlobal(Archive& a);
  void SerializePort(Archive& a);
  void TakePortFields(const Settings& profile);   // copy a profile into this (globals untouched)
  Settings ForPort(int i) const;                  // globals + profile of port i
  void Load();                      // registry
  void Save() const;                // registry
  bool LoadIni(const ws& file);
  bool SaveIni(const ws& file) const;
  const SeqScheme* FindScheme(const ws& name) const;
};

extern Settings g_set;             // globals + profile of the selected port (UI edits this)
extern Settings g_profiles[12];    // stored profiles (the selected port is refreshed on Save)
ws PortLabel(int i);               // LPT1..LPT9, COM1..COM3
void InitProfiles();
