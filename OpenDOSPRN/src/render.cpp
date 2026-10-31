#include "render.h"

// ------------------------------------------------------------------ paper sizes
const std::vector<PaperSize>& Papers() {
  static const std::vector<PaperSize> v = {
    {DMPAPER_A4, 210, 297, L"A4"},
    {DMPAPER_LETTER, 215.9, 279.4, L"Letter"},
    {DMPAPER_LEGAL, 215.9, 355.6, L"Legal"},
    {DMPAPER_A3, 297, 420, L"A3"},
    {DMPAPER_A5, 148, 210, L"A5"},
    {DMPAPER_B4, 250, 353, L"B4"},
    {DMPAPER_B5, 176, 250, L"B5"},
    {DMPAPER_EXECUTIVE, 184.15, 266.7, L"Executive"},
    {DMPAPER_FOLIO, 215.9, 330.2, L"Folio"},
    {DMPAPER_STATEMENT, 139.7, 215.9, L"Statement"},
    {DMPAPER_10X14, 254, 355.6, L"10 x 14 in"},
    {DMPAPER_11X17, 279.4, 431.8, L"11 x 17 in"},
    {DMPAPER_FANFOLD_US, 377.825, 279.4, L"US Std Fanfold 14 7/8 x 11 in"},
    {DMPAPER_FANFOLD_STD_GERMAN, 215.9, 304.8, L"German Std Fanfold 8 1/2 x 12 in"},
    {DMPAPER_FANFOLD_LGL_GERMAN, 215.9, 330.2, L"German Legal Fanfold 8 1/2 x 13 in"},
  };
  return v;
}

void PageDims(const Settings& s, double& w, double& h) {
  double wm = 210, hm = 297;
  for (auto& p : Papers()) if (p.id == s.paperSize) { wm = p.wmm; hm = p.hmm; break; }
  w = wm / 25.4;
  h = hm / 25.4;
  if (s.landscape) std::swap(w, h);
}

EmuOptions MakeEmuOptions(const Settings& s) {
  EmuOptions o;
  double w, h;
  PageDims(s, w, h);
  o.contentW = std::max(0.5, w - (s.mLeft + s.mRight) / 25.4);
  o.contentH = std::max(0.5, h - (s.mTop + s.mBottom) / 25.4);
  o.cpi = s.cpi;
  o.lpi = s.lpi;
  o.wrap = s.wrapLong;
  o.useFF = s.useFF;
  o.lfAsCrLf = s.lfAsCrLf;
  o.crAsCrLf = s.crAsCrLf;
  o.interpretEsc = s.emulateSeq && _wcsicmp(s.seqScheme.c_str(), L"No sequences") != 0;
  o.scheme = s.FindScheme(s.seqScheme);
  CodePage::Load(s.codepage, o.cp);
  return o;
}

RenderStyle MakeRenderStyle(const Settings& s) {
  RenderStyle r;
  r.font = s.fontName;
  r.sizePt = s.fontSize;
  r.bold = s.bold; r.italic = s.italic; r.underline = s.underline; r.strike = s.strike;
  r.marginL = s.mLeft / 25.4;
  r.marginT = s.mTop / 25.4;
  return r;
}

std::vector<Page> BuildPages(const Settings& s, const std::string& data) {
  return Interpret(data, MakeEmuOptions(s));
}

// ------------------------------------------------------------------ box drawing / blocks
struct BoxDef { wchar_t c; signed char u, d, l, r; };
static const BoxDef kBox[] = {
  {0x2500,0,0,1,1},{0x2502,1,1,0,0},{0x250C,0,1,0,1},{0x2510,0,1,1,0},{0x2514,1,0,0,1},{0x2518,1,0,1,0},
  {0x251C,1,1,0,1},{0x2524,1,1,1,0},{0x252C,0,1,1,1},{0x2534,1,0,1,1},{0x253C,1,1,1,1},
  {0x2550,0,0,2,2},{0x2551,2,2,0,0},{0x2552,0,1,0,2},{0x2553,0,2,0,1},{0x2554,0,2,0,2},{0x2555,0,1,2,0},
  {0x2556,0,2,1,0},{0x2557,0,2,2,0},{0x2558,1,0,0,2},{0x2559,2,0,0,1},{0x255A,2,0,0,2},{0x255B,1,0,2,0},
  {0x255C,2,0,1,0},{0x255D,2,0,2,0},{0x255E,1,1,0,2},{0x255F,2,2,0,1},{0x2560,2,2,0,2},{0x2561,1,1,2,0},
  {0x2562,2,2,1,0},{0x2563,2,2,2,0},{0x2564,0,1,2,2},{0x2565,0,2,1,1},{0x2566,0,2,2,2},{0x2567,1,0,2,2},
  {0x2568,2,0,1,1},{0x2569,2,0,2,2},{0x256A,1,1,2,2},{0x256B,2,2,1,1},{0x256C,2,2,2,2},
};

bool CellPrims(wchar_t c, double cw, double ch, std::vector<Prim>& out) {
  out.clear();
  switch (c) {
    case 0x2580: out.push_back({0, 0, cw, ch / 2, 0}); return true;
    case 0x2584: out.push_back({0, ch / 2, cw, ch, 0}); return true;
    case 0x2588: out.push_back({0, 0, cw, ch, 0}); return true;
    case 0x258C: out.push_back({0, 0, cw / 2, ch, 0}); return true;
    case 0x2590: out.push_back({cw / 2, 0, cw, ch, 0}); return true;
    case 0x2591: out.push_back({0, 0, cw, ch, 0.75}); return true;
    case 0x2592: out.push_back({0, 0, cw, ch, 0.5}); return true;
    case 0x2593: out.push_back({0, 0, cw, ch, 0.25}); return true;
    default: break;
  }
  const BoxDef* b = nullptr;
  for (auto& e : kBox) if (e.c == c) { b = &e; break; }
  if (!b) return false;
  double t = 0.011, d = 0.016;                    // line thickness and double-line offset (inches)
  double cx = cw / 2, cy = ch * 0.55;             // junction: slightly below the middle of the row box
  auto horiz = [&](double xa, double xb, int w) {
    if (w == 1) out.push_back({xa, cy - t / 2, xb, cy + t / 2, 0});
    else if (w == 2) { out.push_back({xa, cy - d - t / 2, xb, cy - d + t / 2, 0}); out.push_back({xa, cy + d - t / 2, xb, cy + d + t / 2, 0}); }
  };
  auto vert = [&](double ya, double yb, int w) {
    if (w == 1) out.push_back({cx - t / 2, ya, cx + t / 2, yb, 0});
    else if (w == 2) { out.push_back({cx - d - t / 2, ya, cx - d + t / 2, yb, 0}); out.push_back({cx + d - t / 2, ya, cx + d + t / 2, yb, 0}); }
  };
  double ext = (b->u == 2 || b->d == 2 || b->l == 2 || b->r == 2) ? d + t / 2 : t / 2;
  if (b->l) horiz(0, cx + ((b->r || b->u || b->d) ? ext : 0), b->l);
  if (b->r) horiz(cx - ((b->l || b->u || b->d) ? ext : 0), cw, b->r);
  if (b->u) vert(0, cy + ((b->d || b->l || b->r) ? ext : 0), b->u);
  if (b->d) vert(cy - ((b->u || b->l || b->r) ? ext : 0), ch, b->d);
  return true;
}

// ------------------------------------------------------------------ GDI rendering
namespace {
struct FontKey {
  unsigned flags; int wq;
  bool operator<(const FontKey& o) const { return flags != o.flags ? flags < o.flags : wq < o.wq; }
};
}

static int Px(double inches, double dpi) { return (int)floor(inches * dpi + 0.5); }

void RenderPage(HDC dc, const Page& pg, const RenderStyle& st, double dpiX, double dpiY, double ox, double oy) {
  std::map<FontKey, HFONT> fonts;
  int saveDc = SaveDC(dc);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(0, 0, 0));
  SetTextAlign(dc, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);

  auto getFont = [&](unsigned fl, double wscale) -> HFONT {
    FontKey k{fl, (int)floor(wscale * 1000 + 0.5)};
    auto it = fonts.find(k);
    if (it != fonts.end()) return it->second;
    double sz = st.sizePt;
    if (fl & (RF_SUP | RF_SUB)) sz *= 0.66;
    if (fl & RF_DHEIGHT) sz *= 2;
    int h = (int)floor(sz * dpiY / 72.0 + 0.5);
    if (h < 1) h = 1;
    LOGFONTW lf = {};
    lf.lfHeight = -h;
    lf.lfWeight = ((fl & RF_BOLD) || st.bold) ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = ((fl & RF_ITALIC) || st.italic) ? TRUE : FALSE;
    lf.lfUnderline = ((fl & RF_UNDERLINE) || st.underline) ? TRUE : FALSE;
    lf.lfStrikeOut = ((fl & RF_STRIKE) || st.strike) ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = PROOF_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    wcsncpy(lf.lfFaceName, st.font.c_str(), LF_FACESIZE - 1);
    if (fabs(wscale - 1.0) > 0.01) {
      LOGFONTW l0 = lf;
      l0.lfWidth = 0;
      HFONT f0 = CreateFontIndirectW(&l0);
      HGDIOBJ old = SelectObject(dc, f0);
      TEXTMETRICW tm;
      GetTextMetricsW(dc, &tm);
      SelectObject(dc, old);
      DeleteObject(f0);
      lf.lfWidth = std::max(1, (int)floor(tm.tmAveCharWidth * wscale + 0.5));
    }
    HFONT f = CreateFontIndirectW(&lf);
    fonts[k] = f;
    return f;
  };

  for (auto& r : pg.runs) {
    double rowTop = st.marginT + r.y;
    double base = rowTop + r.lineH * 0.80;
    double wscale = r.adv / r.baseAdv;
    double bAdj = 0;
    if (r.flags & RF_SUP) bAdj = -r.lineH * 0.30;
    if (r.flags & RF_SUB) bAdj = r.lineH * 0.10;
    // split run into text segments and cell primitives
    ws seg;
    std::vector<INT> dx;
    int segStartCell = 0;
    int cell = 0;
    std::vector<Prim> prims;
    auto flush = [&]() {
      if (seg.empty()) return;
      HFONT f = getFont(r.flags, wscale);
      HGDIOBJ old = SelectObject(dc, f);
      int x0 = (int)floor(ox + (st.marginL + r.x + segStartCell * r.adv) * dpiX + 0.5);
      ExtTextOutW(dc, x0, (int)floor(oy + (base + bAdj) * dpiY + 0.5), 0, nullptr, seg.c_str(), (UINT)seg.size(), dx.data());
      SelectObject(dc, old);
      seg.clear();
      dx.clear();
    };
    for (size_t i = 0; i < r.text.size(); i++) {
      wchar_t c = r.text[i];
      int cells = CharCells(c);
      // cumulative rounding avoids drift along the row
      int adv = Px((cell + cells) * r.adv, dpiX) - Px(cell * r.adv, dpiX);
      if (CellPrims(c, r.adv * cells, r.lineH, prims)) {
        if (seg.empty()) segStartCell = cell + cells;
        else { seg += L' '; dx.push_back(adv); }
        // draw primitives
        for (auto& p : prims) {
          int l = (int)floor(ox + (st.marginL + r.x + cell * r.adv + p.x0) * dpiX + 0.5);
          int rr = (int)floor(ox + (st.marginL + r.x + cell * r.adv + p.x1) * dpiX + 0.5);
          int t = (int)floor(oy + (rowTop + p.y0) * dpiY + 0.5);
          int b = (int)floor(oy + (rowTop + p.y1) * dpiY + 0.5);
          if (rr <= l) rr = l + 1;
          if (b <= t) b = t + 1;
          if (p.gray <= 0.001) {
            PatBlt(dc, l, t, rr - l, b - t, BLACKNESS);
          } else {
            // halftone via dither pattern
            int g = (int)((1.0 - p.gray) * 255);
            HBRUSH br = CreateSolidBrush(RGB(255 - g, 255 - g, 255 - g));
            RECT rc = {l, t, rr, b};
            FillRect(dc, &rc, br);
            DeleteObject(br);
          }
        }
        cell += cells;
        continue;
      }
      if (seg.empty()) segStartCell = cell;
      seg += c;
      dx.push_back(adv);
      cell += cells;
    }
    flush();
  }

  // bit images
  for (auto& g : pg.gfx) {
    double gx = st.marginL + g.x, gy = st.marginT + g.y;
    int dw = std::max(1, Px(g.dotW, dpiX)), dh = std::max(1, Px(g.dotH, dpiY));
    for (int c = 0; c < g.cols; c++) {
      for (int by2 = 0; by2 < g.bytesPerCol; by2++) {
        unsigned char b = g.data[(size_t)c * g.bytesPerCol + by2];
        if (!b) continue;
        for (int k = 0; k < 8; k++) {
          if (b & (0x80 >> k)) {
            int x = (int)floor(ox + (gx + c * g.dotW) * dpiX + 0.5);
            int y = (int)floor(oy + (gy + (by2 * 8 + k) * g.dotH) * dpiY + 0.5);
            PatBlt(dc, x, y, dw, dh, BLACKNESS);
          }
        }
      }
    }
  }

  RestoreDC(dc, saveDc);
  for (auto& f : fonts) DeleteObject(f.second);
}

// ------------------------------------------------------------------ fonts
static int CALLBACK EnumProc(const LOGFONTW* lf, const TEXTMETRICW* tm, DWORD type, LPARAM lp) {
  auto* v = (std::pair<std::vector<ws>*, bool>*)lp;
  if (lf->lfFaceName[0] == L'@') return 1;
  if (type != TRUETYPE_FONTTYPE && type != DEVICE_FONTTYPE && type != 0) return 1;
  bool fixed = (lf->lfPitchAndFamily & 3) == FIXED_PITCH;
  if (v->second && !fixed) return 1;
  v->first->push_back(lf->lfFaceName);
  return 1;
}

std::vector<ws> EnumFonts(bool monoOnly) {
  std::vector<ws> r;
  std::pair<std::vector<ws>*, bool> ctx(&r, monoOnly);
  HDC dc = GetDC(nullptr);
  LOGFONTW lf = {};
  lf.lfCharSet = DEFAULT_CHARSET;
  EnumFontFamiliesExW(dc, &lf, (FONTENUMPROCW)EnumProc, (LPARAM)&ctx, 0);
  ReleaseDC(nullptr, dc);
  std::sort(r.begin(), r.end(), [](const ws& a, const ws& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
  r.erase(std::unique(r.begin(), r.end(), [](const ws& a, const ws& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }), r.end());
  return r;
}
