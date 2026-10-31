#pragma once
#include "common.h"
#include "render.h"

struct PrinterSel {
  ws name;
  std::vector<BYTE> devmode;     // optional, from the setup dialog
};

std::vector<ws> EnumPrinterNames();
ws DefaultPrinterName();
bool SelectPrinterDialog(HWND owner, PrinterSel& sel);                           // standard print-setup dialog
bool PrintRaw(const ws& printer, const std::string& data, int copies, const ws& doc);
// pageIdx: 0-based page indices to print
bool PrintPages(const Settings& s, const PrinterSel& sel, const std::vector<Page>& pages,
                const std::vector<int>& pageIdx, const ws& doc, ws& error);
// Parses "1,3,5-9" (1-based) against total pages; empty = all. Returns false on bad syntax.
bool ParsePageRange(const ws& text, int total, std::vector<int>& out);
