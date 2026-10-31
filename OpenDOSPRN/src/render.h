#pragma once
#include "common.h"
#include "escp.h"

struct RenderStyle {
  ws font = L"Courier New";
  int sizePt = 11;
  bool bold = false, italic = false, underline = false, strike = false;
  double marginL = 0, marginT = 0;     // inches, from the paper edge to the printable area
};

struct Prim { double x0, y0, x1, y1, gray; };   // rectangle in inches relative to the cell, gray 0..1
bool CellPrims(wchar_t c, double cw, double ch, std::vector<Prim>& out);   // box drawing / block chars

struct PaperSize { int id; double wmm, hmm; const wchar_t* name; };
const std::vector<PaperSize>& Papers();
void PageDims(const Settings& s, double& wIn, double& hIn);        // paper size with orientation
EmuOptions MakeEmuOptions(const Settings& s);                      // from user settings
RenderStyle MakeRenderStyle(const Settings& s);
std::vector<Page> BuildPages(const Settings& s, const std::string& data);

// Draws one page. (originX, originY) = device position of the paper's top-left corner.
void RenderPage(HDC dc, const Page& pg, const RenderStyle& st, double dpiX, double dpiY, double originX, double originY);

// Monospace font enumeration for the UI
std::vector<ws> EnumFonts(bool monoOnly);
