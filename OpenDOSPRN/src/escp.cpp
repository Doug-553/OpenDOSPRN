#include "escp.h"

static std::string S(const char* p, size_t n) { return std::string(p, n); }
#define LIT(x) S(x, sizeof(x) - 1)

static const SeqFunc g_funcs[kSeqFuncCount] = {
  {L"Reset printer",         LIT("\x1b" "@")},
  {L"Bold on",               LIT("\x1b" "E")},
  {L"Bold off",              LIT("\x1b" "F")},
  {L"Double-strike on",      LIT("\x1b" "G")},
  {L"Double-strike off",     LIT("\x1b" "H")},
  {L"Italic on",             LIT("\x1b" "4")},
  {L"Italic off",            LIT("\x1b" "5")},
  {L"Underline on",          LIT("\x1b" "-" "\x01")},
  {L"Underline off",         LIT("\x1b" "-" "\x00")},
  {L"Condensed on",          LIT("\x0f")},
  {L"Condensed off",         LIT("\x12")},
  {L"Double width on",       LIT("\x0e")},
  {L"Double width off",      LIT("\x14")},
  {L"10 CPI (pica)",         LIT("\x1b" "P")},
  {L"12 CPI (elite)",        LIT("\x1b" "M")},
  {L"15 CPI",                LIT("\x1b" "g")},
  {L"6 lines per inch",      LIT("\x1b" "2")},
  {L"8 lines per inch",      LIT("\x1b" "0")},
  {L"Form feed",             LIT("\x0c")},
};

const SeqFunc* SeqFuncs() { return g_funcs; }

int CharCells(wchar_t c) {
  if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
      (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE6F) || (c >= 0xFF00 && c <= 0xFF60) ||
      (c >= 0xFFE0 && c <= 0xFFE6))
    return 2;
  return 1;
}

// "\e" "\x1b" "\n" "\r" "\t" "\\" "^X" and plain characters
std::string ParseEscNotation(const ws& s) {
  std::string r;
  for (size_t i = 0; i < s.size(); i++) {
    wchar_t c = s[i];
    if (c == L'\\' && i + 1 < s.size()) {
      wchar_t n = s[++i];
      switch (n) {
        case L'e': case L'E': r += '\x1b'; break;
        case L'n': r += '\n'; break;
        case L'r': r += '\r'; break;
        case L't': r += '\t'; break;
        case L'f': r += '\f'; break;
        case L'\\': r += '\\'; break;
        case L'x': case L'X': {
          int v = 0, k = 0;
          while (k < 2 && i + 1 < s.size() && iswxdigit(s[i + 1])) {
            wchar_t h = s[++i];
            v = v * 16 + (h <= L'9' ? h - L'0' : (towlower(h) - L'a' + 10));
            k++;
          }
          r += (char)v;
          break;
        }
        default: r += (char)n; break;
      }
    } else if (c == L'^' && i + 1 < s.size() && s[i + 1] >= L'@' && s[i + 1] <= L'_') {
      r += (char)(s[++i] - L'@');
    } else {
      r += (char)(c & 0xFF);
    }
  }
  return r;
}

// Replace user-defined sequences by the matching Epson ESC/P commands.
static std::string TranslateUser(const std::string& in, const SeqScheme& sc) {
  struct Pair { std::string from, to; };
  std::vector<Pair> v;
  for (int f = 0; f < kSeqFuncCount; f++) {
    if (sc.codes[f].empty()) continue;
    std::string from = ParseEscNotation(sc.codes[f]);
    if (!from.empty()) v.push_back({from, g_funcs[f].epson});
  }
  if (v.empty()) return in;
  std::stable_sort(v.begin(), v.end(), [](const Pair& a, const Pair& b) { return a.from.size() > b.from.size(); });
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size();) {
    bool hit = false;
    for (auto& p : v) {
      if (in.compare(i, p.from.size(), p.from) == 0) {
        out += p.to;
        i += p.from.size();
        hit = true;
        break;
      }
    }
    if (!hit) out += in[i++];
  }
  return out;
}

namespace {
const double kCondFactor = 10.0 / 17.14;

class Emu {
 public:
  explicit Emu(const EmuOptions& o) : o_(o) { Reset(); pages_.emplace_back(); }

  std::vector<Page> Run(const std::string& d) {
    d_ = &d;
    n_ = d.size();
    p_ = 0;
    while (p_ < n_) Step();
    while (pages_.size() > 1 && pages_.back().Empty()) pages_.pop_back();
    return pages_;
  }

 private:
  const EmuOptions& o_;
  const std::string* d_ = nullptr;
  size_t n_ = 0, p_ = 0;
  std::vector<Page> pages_;
  // state
  double cpi_, lineH_, pageLen_, lm_, rm_, x_, y_;
  bool cond_, dw_, dh_, bold_, dstrike_, ital_, ul_, st_, sup_, sub_;
  std::vector<double> htabs_, vtabs_;

  void Reset() {
    cpi_ = o_.cpi;
    lineH_ = 1.0 / o_.lpi;
    pageLen_ = o_.contentH;
    lm_ = 0;
    rm_ = o_.contentW;
    x_ = lm_;
    y_ = 0;
    cond_ = dw_ = dh_ = bold_ = dstrike_ = ital_ = ul_ = st_ = sup_ = sub_ = false;
    htabs_.clear();
    vtabs_.clear();
  }

  double BaseAdv() const { return 1.0 / cpi_; }
  double Adv() const { return BaseAdv() * (cond_ ? kCondFactor : 1.0) * (dw_ ? 2.0 : 1.0); }
  unsigned Flags() const {
    unsigned f = 0;
    if (bold_ || dstrike_) f |= RF_BOLD;
    if (ital_) f |= RF_ITALIC;
    if (ul_) f |= RF_UNDERLINE;
    if (st_) f |= RF_STRIKE;
    if (sup_) f |= RF_SUP;
    if (sub_) f |= RF_SUB;
    if (dh_) f |= RF_DHEIGHT;
    return f;
  }
  Page& Cur() { return pages_.back(); }

  void NewPage() {
    pages_.emplace_back();
    y_ = 0;
  }
  void LineFeed() {
    y_ += lineH_;
    if (y_ + lineH_ > pageLen_ + 1e-4) NewPage();
  }
  void CR() { x_ = lm_; }

  void PutChar(wchar_t ch) {
    int cells = CharCells(ch);
    double adv = Adv();
    double w = adv * cells;
    if (x_ + w > rm_ + 1e-4) {
      if (o_.wrap) { CR(); LineFeed(); }
      else return;     // cut off
    }
    Page& pg = Cur();
    unsigned fl = Flags();
    bool merged = false;
    if (!pg.runs.empty()) {
      ::Run& r = pg.runs.back();
      double end = r.x;
      for (wchar_t c : r.text) end += r.adv * CharCells(c);
      if (r.flags == fl && fabs(r.adv - adv) < 1e-9 && fabs(r.y - y_) < 1e-9 && fabs(r.lineH - lineH_) < 1e-9 &&
          fabs(end - x_) < 1e-6) {
        r.text += ch;
        merged = true;
      }
    }
    if (!merged) {
      ::Run r;
      r.x = x_; r.y = y_; r.adv = adv; r.baseAdv = BaseAdv(); r.lineH = lineH_; r.flags = fl;
      r.text = ws(1, ch);
      pg.runs.push_back(r);
    }
    x_ += w;
  }

  bool Have(size_t k) const { return p_ + k <= n_; }
  int Byte() { return (unsigned char)(*d_)[p_++]; }

  void HTab() {
    double a = BaseAdv();
    for (double t : htabs_) if (t > x_ + 1e-6) { x_ = std::min(t, rm_); return; }
    if (!htabs_.empty()) return;   // explicit tab list exhausted
    double step = 8 * a;
    double nx = (floor((x_ + 1e-6) / step) + 1) * step;
    x_ = std::min(nx, rm_);
  }
  void VTab() {
    for (double t : vtabs_) if (t > y_ + 1e-6) {
      y_ = t;
      if (y_ + lineH_ > pageLen_ + 1e-4) NewPage();
      return;
    }
    LineFeed();
  }

  void Graphics(int mode, int nl, int nh, bool dummyModeIs8pinHeader) {
    (void)dummyModeIs8pinHeader;
    int cols = nl + 256 * nh;
    int bpc = 1;
    double dpiX = 60;
    double dotH = 1.0 / 72;
    switch (mode) {
      case 0: dpiX = 60; break;
      case 1: dpiX = 120; break;
      case 2: dpiX = 120; break;
      case 3: dpiX = 240; break;
      case 4: dpiX = 80; break;
      case 5: dpiX = 72; break;
      case 6: dpiX = 90; break;
      case 7: dpiX = 144; break;
      case 32: dpiX = 60; bpc = 3; dotH = 1.0 / 180; break;
      case 33: dpiX = 120; bpc = 3; dotH = 1.0 / 180; break;
      case 38: dpiX = 90; bpc = 3; dotH = 1.0 / 180; break;
      case 39: dpiX = 180; bpc = 3; dotH = 1.0 / 180; break;
      case 40: dpiX = 360; bpc = 3; dotH = 1.0 / 180; break;
      case 71: dpiX = 180; bpc = 3; dotH = 1.0 / 180; break;
      case 72: dpiX = 360; bpc = 3; dotH = 1.0 / 180; break;
      case 73: dpiX = 360; bpc = 3; dotH = 1.0 / 180; break;
      default: break;
    }
    size_t bytes = (size_t)cols * bpc;
    if (!Have(bytes)) bytes = n_ - p_;
    Gfx g;
    g.x = x_; g.y = y_;
    g.dotW = 1.0 / dpiX; g.dotH = dotH;
    g.cols = (int)(bytes / bpc);
    g.bytesPerCol = bpc;
    g.data.assign(d_->begin() + p_, d_->begin() + p_ + bytes);
    p_ += bytes;
    Cur().gfx.push_back(g);
    x_ += g.cols * g.dotW;
  }

  void Esc() {
    if (!Have(1)) { p_ = n_; return; }
    int c = Byte();
    switch (c) {
      case '@': Reset(); break;
      case '0': lineH_ = 1.0 / 8; break;
      case '1': lineH_ = 7.0 / 72; break;
      case '2': lineH_ = 1.0 / 6; break;
      case '3': if (Have(1)) lineH_ = Byte() / 180.0; break;
      case 'A': if (Have(1)) lineH_ = Byte() / 60.0; break;
      case '+': if (Have(1)) lineH_ = Byte() / 360.0; break;
      case 'J': if (Have(1)) { y_ += Byte() / 180.0; if (y_ + lineH_ > pageLen_ + 1e-4) NewPage(); } break;
      case 'j': if (Have(1)) { y_ = std::max(0.0, y_ - Byte() / 180.0); } break;
      case 'C':
        if (Have(1)) {
          int n = Byte();
          if (n == 0 && Have(1)) pageLen_ = std::min<double>(Byte(), 22) * 1.0;
          else if (n > 0) pageLen_ = n * lineH_;
          pageLen_ = std::min(pageLen_, o_.contentH);
          if (pageLen_ < lineH_) pageLen_ = lineH_;
        }
        break;
      case 'N': if (Have(1)) Byte(); break;
      case 'O': break;
      case 'l': if (Have(1)) { lm_ = std::min(Byte() * BaseAdv(), o_.contentW); if (x_ < lm_) x_ = lm_; } break;
      case 'Q': if (Have(1)) { double r = Byte() * BaseAdv(); rm_ = (r > lm_ && r <= o_.contentW) ? r : o_.contentW; } break;
      case 'D': {
        htabs_.clear();
        double last = -1;
        for (int k = 0; k < 32 && Have(1); k++) {
          int t = Byte();
          if (t == 0 || t < last) break;
          last = t;
          htabs_.push_back(t * BaseAdv());
        }
        break;
      }
      case 'B': {
        vtabs_.clear();
        double last = -1;
        for (int k = 0; k < 16 && Have(1); k++) {
          int t = Byte();
          if (t == 0 || t < last) break;
          last = t;
          vtabs_.push_back(t * lineH_);
        }
        break;
      }
      case 'M': cpi_ = 12; break;
      case 'P': cpi_ = 10; break;
      case 'g': cpi_ = 15; break;
      case 'E': bold_ = true; break;
      case 'F': bold_ = false; break;
      case 'G': dstrike_ = true; break;
      case 'H': dstrike_ = false; break;
      case '4': ital_ = true; break;
      case '5': ital_ = false; break;
      case '-': if (Have(1)) ul_ = (Byte() & 1) != 0; break;
      case '_': if (Have(1)) ul_ = (Byte() & 1) != 0; break;
      case 'S': if (Have(1)) { int n = Byte() & 1; sup_ = (n == 0); sub_ = (n == 1); } break;
      case 'T': sup_ = sub_ = false; break;
      case 'W': if (Have(1)) dw_ = (Byte() & 1) != 0; break;
      case 'w': if (Have(1)) dh_ = (Byte() & 1) != 0; break;
      case 0x0e: dw_ = true; break;
      case 0x0f: cond_ = true; break;
      case '!':
        if (Have(1)) {
          int n = Byte();
          cpi_ = (n & 1) ? 12 : 10;
          cond_ = (n & 4) != 0;
          bold_ = (n & 8) != 0;
          dstrike_ = (n & 16) != 0;
          dw_ = (n & 32) != 0;
          ital_ = (n & 64) != 0;
          ul_ = (n & 128) != 0;
        }
        break;
      case '$': if (Have(2)) { int l = Byte(), h = Byte(); x_ = std::min(lm_ + (l + 256 * h) / 60.0, rm_); } break;
      case '\\': if (Have(2)) { int l = Byte(), h = Byte(); int v = (short)(l + 256 * h); x_ = std::max(lm_, std::min(x_ + v / 120.0, rm_)); } break;
      case '*': if (Have(3)) { int m = Byte(), l = Byte(), h = Byte(); Graphics(m, l, h, true); } break;
      case 'K': if (Have(2)) { int l = Byte(), h = Byte(); Graphics(0, l, h, true); } break;
      case 'L': if (Have(2)) { int l = Byte(), h = Byte(); Graphics(1, l, h, true); } break;
      case 'Y': if (Have(2)) { int l = Byte(), h = Byte(); Graphics(2, l, h, true); } break;
      case 'Z': if (Have(2)) { int l = Byte(), h = Byte(); Graphics(3, l, h, true); } break;
      case '(':
        if (Have(3)) {
          Byte();                       // command letter
          int l = Byte(), h = Byte();
          size_t len = (size_t)(l + 256 * h);
          p_ = std::min(n_, p_ + len);  // extended commands are skipped
        }
        break;
      // one-parameter commands that do not affect the layout model
      case 'x': case 'p': case 'k': case 'a': case ' ': case 'U': case 'm': case 't': case 'R': case 'r':
      case '%': case 'I': case 'i': case 's': case 'e': case 'f': case 'u': case 'q': case 'X':
        if (Have(1)) Byte();
        break;
      case ':': if (Have(3)) p_ += 3; break;
      default: break;   // unknown / no-parameter command
    }
  }

  void Step() {
    int b = Byte();
    switch (b) {
      case 0x1b:
        if (o_.interpretEsc) Esc(); else if (Have(1)) {}   // ESC ignored in "No sequences" mode
        return;
      case 0x07: case 0x00: case 0x11: case 0x13: case 0x18: case 0x7f: return;
      case 0x08: x_ = std::max(lm_, x_ - Adv()); return;
      case 0x09: HTab(); return;
      case 0x0a: if (o_.lfAsCrLf) CR(); LineFeed(); return;
      case 0x0b: VTab(); return;
      case 0x0c: if (o_.useFF) { NewPage(); if (o_.lfAsCrLf) CR(); } return;
      case 0x0d: CR(); if (o_.crAsCrLf) LineFeed(); return;
      case 0x0e: if (o_.interpretEsc) dw_ = true; return;
      case 0x0f: if (o_.interpretEsc) cond_ = true; return;
      case 0x12: if (o_.interpretEsc) cond_ = false; return;
      case 0x14: if (o_.interpretEsc) dw_ = false; return;
      default: break;
    }
    if (b < 0x20 && o_.cp.map[b] == (wchar_t)b) return;   // unmapped control code
    // text
    wchar_t ch;
    if (o_.cp.lead[b] && Have(1)) {
      int b2 = Byte();
      auto it = o_.cp.dbcs.find((uint16_t)((b << 8) | b2));
      ch = it != o_.cp.dbcs.end() ? it->second : L'?';
    } else {
      ch = o_.cp.map[b];
    }
    PutChar(ch);
  }
};
}  // namespace

std::vector<Page> Interpret(const std::string& data, const EmuOptions& o) {
  std::string work = data;
  if (o.interpretEsc && o.scheme) {
    std::string init = ParseEscNotation(o.scheme->init);
    work = TranslateUser(data, *o.scheme);
    work = init + work;
  }
  Emu e(o);
  return e.Run(work);
}
