#include "pdf.h"

static std::string Num(double v) {
  char b[48];
  snprintf(b, sizeof b, "%.3f", v);
  std::string s = b;
  while (!s.empty() && s.back() == '0') s.pop_back();
  if (!s.empty() && s.back() == '.') s.pop_back();
  if (s.empty() || s == "-0") s = "0";
  return s;
}

static std::string PdfString(const ws& t) {
  std::string r = "(";
  for (wchar_t c : t) {
    unsigned char b = '?';
    BOOL used = FALSE;
    if (c < 0x80) b = (unsigned char)c;
    else {
      char out = '?';
      if (WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, &c, 1, &out, 1, "?", &used) == 1 && !used) b = (unsigned char)out;
      else b = '?';
    }
    if (b == '(' || b == ')' || b == '\\') { r += '\\'; r += (char)b; }
    else if (b < 0x20 || b > 0x7e) { char o[8]; snprintf(o, sizeof o, "\\%03o", b); r += o; }
    else r += (char)b;
  }
  r += ")";
  return r;
}

bool SavePdf(const ws& path, const std::vector<Page>& pages, const std::vector<int>& idx,
             const RenderStyle& st, double pw, double ph) {
  double W = pw * 72, H = ph * 72;
  std::string out = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
  std::vector<size_t> off(1, 0);
  auto beginObj = [&](int n) {
    if ((int)off.size() <= n) off.resize(n + 1);
    off[n] = out.size();
    out += std::to_string(n) + " 0 obj\n";
  };
  int nPages = (int)idx.size();
  int firstPage = 7;            // 1 catalog, 2 pages, 3-6 fonts, then page/content pairs
  beginObj(1);
  out += "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
  beginObj(2);
  out += "<< /Type /Pages /Count " + std::to_string(nPages) + " /Kids [";
  for (int i = 0; i < nPages; i++) out += std::to_string(firstPage + i * 2) + " 0 R ";
  out += "] >>\nendobj\n";
  const char* fnames[4] = {"Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique"};
  for (int f = 0; f < 4; f++) {
    beginObj(3 + f);
    out += std::string("<< /Type /Font /Subtype /Type1 /BaseFont /") + fnames[f] + " /Encoding /WinAnsiEncoding >>\nendobj\n";
  }
  for (int k = 0; k < nPages; k++) {
    const Page& pg = pages[idx[k]];
    std::string c;
    std::vector<Prim> prims;
    for (auto& r : pg.runs) {
      double rowTop = st.marginT + r.y;
      double base = rowTop + r.lineH * 0.80;
      if (r.flags & RF_SUP) base -= r.lineH * 0.30;
      if (r.flags & RF_SUB) base += r.lineH * 0.10;
      bool bold = (r.flags & RF_BOLD) || st.bold, ital = (r.flags & RF_ITALIC) || st.italic;
      double sz = st.sizePt;
      if (r.flags & (RF_SUP | RF_SUB)) sz *= 0.66;
      if (r.flags & RF_DHEIGHT) sz *= 2;
      double scale = r.adv / r.baseAdv;
      double th = scale * 100.0;
      double advPt = r.adv * 72;
      double tc = advPt / (th / 100.0) - 0.6 * sz;
      ws txt;
      int cell = 0;
      for (wchar_t ch : r.text) {
        int cells = CharCells(ch);
        if (CellPrims(ch, r.adv * cells, r.lineH, prims)) {
          txt += L' ';
          for (auto& p : prims) {
            double x0 = (st.marginL + r.x + cell * r.adv + p.x0) * 72;
            double x1 = (st.marginL + r.x + cell * r.adv + p.x1) * 72;
            double y0 = H - (rowTop + p.y1) * 72;
            double y1 = H - (rowTop + p.y0) * 72;
            c += Num(p.gray) + " g " + Num(x0) + " " + Num(y0) + " " + Num(x1 - x0) + " " + Num(y1 - y0) + " re f 0 g\n";
          }
        } else
          txt += ch;
        cell += cells;
      }
      double xs = (st.marginL + r.x) * 72, ys = H - base * 72;
      int fi = (bold ? 1 : 0) + (ital ? 2 : 0);
      bool onlySpaces = txt.find_first_not_of(L' ') == ws::npos;
      if (!onlySpaces) {
        c += "BT /F" + std::to_string(fi + 1) + " " + Num(sz) + " Tf " + Num(th) + " Tz " + Num(tc) + " Tc " + Num(xs) + " " +
             Num(ys) + " Td " + PdfString(txt) + " Tj ET\n";
      }
      double len = cell * advPt;
      if ((r.flags & RF_UNDERLINE) || st.underline)
        c += Num(xs) + " " + Num(ys - sz * 0.15) + " " + Num(len) + " " + Num(std::max(0.4, sz * 0.05)) + " re f\n";
      if ((r.flags & RF_STRIKE) || st.strike)
        c += Num(xs) + " " + Num(ys + sz * 0.28) + " " + Num(len) + " " + Num(std::max(0.4, sz * 0.05)) + " re f\n";
    }
    for (auto& g : pg.gfx) {
      double gx = st.marginL + g.x, gy = st.marginT + g.y;
      for (int col = 0; col < g.cols; col++)
        for (int by = 0; by < g.bytesPerCol; by++) {
          unsigned char b = g.data[(size_t)col * g.bytesPerCol + by];
          for (int bit = 0; bit < 8 && b; bit++)
            if (b & (0x80 >> bit)) {
              double x = (gx + col * g.dotW) * 72;
              double y = H - (gy + (by * 8 + bit + 1) * g.dotH) * 72;
              c += Num(x) + " " + Num(y) + " " + Num(g.dotW * 72 + 0.05) + " " + Num(g.dotH * 72 + 0.05) + " re f\n";
            }
        }
    }
    int pobj = firstPage + k * 2, cobj = pobj + 1;
    beginObj(pobj);
    out += "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + Num(W) + " " + Num(H) + "] /Contents " + std::to_string(cobj) +
           " 0 R /Resources << /Font << /F1 3 0 R /F2 4 0 R /F3 5 0 R /F4 6 0 R >> >> >>\nendobj\n";
    beginObj(cobj);
    out += "<< /Length " + std::to_string(c.size()) + " >>\nstream\n" + c + "endstream\nendobj\n";
  }
  size_t xref = out.size();
  int total = firstPage + nPages * 2;     // objects 1..total-1
  out += "xref\n0 " + std::to_string(total) + "\n0000000000 65535 f \n";
  for (int i = 1; i < total; i++) {
    char b[32];
    snprintf(b, sizeof b, "%010u 00000 n \n", (unsigned)off[i]);
    out += b;
  }
  out += "trailer\n<< /Size " + std::to_string(total) + " /Root 1 0 R /Info << /Producer (OpenDOSPRN) >> >>\nstartxref\n" +
         std::to_string(xref) + "\n%%EOF\n";
  return util::WriteFileBytes(path, out);
}

ws ExpandPdfPattern(const ws& pat, const ws& src, const SYSTEMTIME& t) {
  ws r;
  ws base = src, ext;
  size_t dot = base.find_last_of(L'.');
  if (dot != ws::npos) { ext = base.substr(dot + 1); base = base.substr(0, dot); }
  auto monthName = [&](bool longName) {
    wchar_t b[64];
    GetDateFormatW(LOCALE_USER_DEFAULT, 0, &t, longName ? L"MMMM" : L"MMM", b, 64);
    return ws(b);
  };
  size_t i = 0;
  ws low = util::Lower(pat);
  auto starts = [&](const wchar_t* s) { return low.compare(i, wcslen(s), s) == 0; };
  while (i < pat.size()) {
    wchar_t c = pat[i];
    if (c == L'(') {
      size_t e = pat.find(L')', i + 1);
      if (e == ws::npos) e = pat.size();
      r += pat.substr(i + 1, e - i - 1);
      i = e + 1;
    } else if (starts(L"<fname>")) { r += base; i += 7; }
    else if (starts(L"<fext>")) { r += ext; i += 6; }
    else if (starts(L"<uname>")) { r += util::UserName(); i += 7; }
    else if (starts(L"yyyy")) { r += util::Fmt(L"%04d", t.wYear); i += 4; }
    else if (starts(L"yy")) { r += util::Fmt(L"%02d", t.wYear % 100); i += 2; }
    else if (starts(L"mmmm")) { r += monthName(true); i += 4; }
    else if (starts(L"mmm")) { r += monthName(false); i += 3; }
    else if (starts(L"mm")) { r += util::Fmt(L"%02d", t.wMonth); i += 2; }
    else if (starts(L"m")) { r += util::Fmt(L"%d", t.wMonth); i += 1; }
    else if (starts(L"dd")) { r += util::Fmt(L"%02d", t.wDay); i += 2; }
    else if (starts(L"d")) { r += util::Fmt(L"%d", t.wDay); i += 1; }
    else if (starts(L"hh")) { r += util::Fmt(L"%02d", t.wHour); i += 2; }
    else if (starts(L"nn")) { r += util::Fmt(L"%02d", t.wMinute); i += 2; }
    else if (starts(L"ss")) { r += util::Fmt(L"%02d", t.wSecond); i += 2; }
    else { r += c; i++; }
  }
  for (auto& ch : r) if (wcschr(L"\\/:*?\"<>|", ch)) ch = L'_';
  ws lr = util::Lower(r);
  if (lr.size() < 4 || lr.compare(lr.size() - 4, 4, L".pdf") != 0) r += L".pdf";
  return r;
}
