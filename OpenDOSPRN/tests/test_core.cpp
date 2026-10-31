#include "../src/render.h"
#include "../src/pdf.h"
int wmain() {
  Settings s;
  s.codepage = L"437";
  std::string d;
  d += "\x1b@Hello \x1b" "EBOLD\x1b" "F normal \x1b-\x01underline\x1b-" "0\r\n";
  d += "\x0f" "condensed text here\x12\r\n\x0e" "WIDE\x14\r\n";
  d += "Box: \xDA\xC4\xC4\xBF\r\n     \xB3  \xB3\r\n     \xC0\xC4\xC4\xD9\r\n";
  d += "Acentos: \x87\x84\x94 \x81\r\n";
  d += std::string("\x1b*\x00\x08\x00\xff\x81\x81\xff\xff\x81\x81\xff\r\n", 18);
  d += "page1\x0cpage2 \x1b" "M12cpi\r\n";
  auto pages = BuildPages(s, d);
  printf("pages=%zu\n", pages.size());
  for (size_t i = 0; i < pages.size(); i++) {
    printf(" page %zu: runs=%zu gfx=%zu\n", i, pages[i].runs.size(), pages[i].gfx.size());
    for (auto& r : pages[i].runs) {
      std::string t;
      for (wchar_t c : r.text) t += c < 128 ? (char)c : '?';
      printf("   x=%.2f y=%.2f adv=%.4f fl=%u '%s'\n", r.x, r.y, r.adv, r.flags, t.c_str());
    }
  }
  double pw, ph;
  PageDims(s, pw, ph);
  std::vector<int> idx;
  for (int i = 0; i < (int)pages.size(); i++) idx.push_back(i);
  bool ok = SavePdf(L"test_out.pdf", pages, idx, MakeRenderStyle(s), pw, ph);
  printf("pdf=%d\n", ok);
  SYSTEMTIME t = {2026, 10, 5, 30, 14, 5, 9, 0};
  wprintf(L"name=%ls\n", ExpandPdfPattern(s.pdfPattern, L"prnspl", t).c_str());
  wprintf(L"name2=%ls\n", ExpandPdfPattern(L"<uname>_<fname>_mmmm_d", L"job.txt", t).c_str());
  // render page 0 and last page to BMP
  for (int pgi : {0, (int)pages.size() - 1}) {
    int W = (int)(pw * 100), H = (int)(ph * 100);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24;
    void* bits; HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(dc, bmp);
    RECT rc = {0, 0, W, H}; FillRect(dc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
    RenderPage(dc, pages[pgi], MakeRenderStyle(s), 100, 100, 0, 0);
    size_t stride = (W * 3 + 3) & ~3u; size_t sz = stride * H;
    BITMAPFILEHEADER fh = {0x4D42, (DWORD)(54 + sz), 0, 0, 54};
    bi.bmiHeader.biHeight = -H;
    std::string out((char*)&fh, 14); out.append((char*)&bi.bmiHeader, 40); out.append((char*)bits, sz);
    // BMP with negative height is top-down; fine for PIL
    std::wstring fn = L"page" + std::to_wstring(pgi) + L".bmp";
    util::WriteFileBytes(fn, out);
  }
  return 0;
}
