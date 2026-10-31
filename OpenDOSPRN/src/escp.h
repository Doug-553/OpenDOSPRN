#pragma once
#include "common.h"
#include "codepage.h"
#include "settings.h"

// ---- page model (all coordinates in inches, relative to the printable area) ----
enum RunFlags {
  RF_BOLD = 1, RF_ITALIC = 2, RF_UNDERLINE = 4, RF_STRIKE = 8, RF_SUP = 16, RF_SUB = 32, RF_DHEIGHT = 64
};
struct Run {
  double x = 0, y = 0;        // y = top of the text row
  double adv = 0.1;           // advance per cell
  double baseAdv = 0.1;       // advance before condensed/double-width scaling
  double lineH = 1.0 / 6;     // row height
  unsigned flags = 0;
  ws text;
};
struct Gfx {
  double x = 0, y = 0, dotW = 1.0 / 60, dotH = 1.0 / 72;
  int cols = 0, bytesPerCol = 1;
  std::vector<unsigned char> data;
};
struct Page {
  std::vector<Run> runs;
  std::vector<Gfx> gfx;
  bool Empty() const { return runs.empty() && gfx.empty(); }
};

struct EmuOptions {
  double contentW = 8, contentH = 11;   // printable area, inches
  double cpi = 10, lpi = 6;
  bool wrap = false, useFF = true, lfAsCrLf = true, crAsCrLf = false;
  bool interpretEsc = true;             // false: "No sequences"
  const SeqScheme* scheme = nullptr;    // user scheme (null = built-in Epson ESC/P)
  CodePage cp;
};

struct SeqFunc { const wchar_t* name; std::string epson; };
const SeqFunc* SeqFuncs();                       // kSeqFuncCount entries
std::string ParseEscNotation(const ws& s);       // "\e@", "\x1b", "^[" ...
int CharCells(wchar_t c);                        // 1 or 2 (wide)

std::vector<Page> Interpret(const std::string& data, const EmuOptions& o);
