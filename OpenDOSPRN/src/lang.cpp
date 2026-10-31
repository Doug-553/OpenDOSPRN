#include "lang.h"

namespace lang {
static std::map<std::string, ws> g_def, g_cur;

static void D(const char* k, const wchar_t* v) { g_def[k] = v; }

void Init() {
  D("TabMain", L"Main");                 D("TabFonts", L"Fonts");
  D("Help", L"Help");                    D("SpoolParam", L"Spool parameters");
  D("TimeOut", L"Job timeout");          D("Sec", L"sec.");
  D("AutoPrint", L"Print automatically, timeout %u sec");
  D("PrintJob", L"Print");               D("PrintFile", L"Print File");
  D("ClearJob", L"Clear job");           D("FileName", L"File name");
  D("Preview", L"Preview");              D("Sequences", L"Sequences emulation");
  D("Orientation", L"Orientation");      D("Portrait", L"&Portrait");
  D("Landscape", L"&Landscape");         D("TextParam", L"Text parameters");
  D("WordWrap", L"Wrap long lines");     D("FormFeed", L"Use form feed (FF)");
  D("LineFeed", L"LF as CR/LF");         D("CRasCRLF", L"CR as CR/LF");
  D("Minimize", L"Run minimized");       D("Printer", L"Printer");
  D("Select", L"Select");                D("MonoSpaceOnly", L"Show monospace fonts only");
  D("FontStyle", L"Style");              D("Bold", L"Bold");
  D("Italic", L"Italic");                D("Underline", L"Underline");
  D("Strikeout", L"Strikeout");          D("Size", L"Size");
  D("PageParam", L"Page margins, mm");   D("TopMargin", L"Top");
  D("Leftmargin", L"Left");              D("BottomMargin", L"Bottom");
  D("RightMargin", L"Right");            D("FontPreview", L"Sample");
  D("Font", L"Font settings");           D("MenuOpen", L"&Open");
  D("MenuCancel", L"&Cancel");           D("MenuExit", L"E&xit");
  D("ExitTitle", L"Confirmation");
  D("ExitLine1", L"If you exit the program,");
  D("ExitLine2", L"DOS jobs will not be printed. Exit?");
  D("Key", L"Key");                      D("Esc", L"Sequence");
  D("Add", L"&Add");                     D("Delete", L"&Delete");
  D("Codes", L"Codes");                  D("Apply", L"Apply");
  D("Cancel", L"Cancel");                D("Close", L"Close");
  D("Capture", L"Capture port");         D("Initialize", L"Initialization");
  D("TrayIconHint", L"DOS printing support");
  D("NoPrinters", L"There are no printers");
  D("FileNotFound", L"File not found");
  D("CodePageEmulation", L"Code page emulation");
  D("EdSpoolName", L"To change the spool file name you must disable 'Capture port' first.");
  D("LimitedAccount", L"This action needs elevated rights, but your account does not have them. Run the program as administrator.");
  D("Papersize", L"Paper size");         D("Printpreview", L"Print preview");
  D("Openthelastpreview", L"Open the last preview");
  D("SaveintoPDF", L"Save into PDF");    D("SetupPDFsettings", L"Set up PDF settings");
  D("Selectprinterandsetupparameters", L"Select printer and set up parameters");
  D("DOSPRNwillprintreportsintotheprinter", L"Reports will be printed on the printer");
  D("DOSPRNwillsavereportsintoPDF", L"Reports will be saved into PDF");
  D("DOSPRNwillshowpreviewwindowsbeforeprinitng", L"A preview window is shown before printing");
  D("Service", L"Service");              D("Savesettingstofile", L"Save settings to file");
  D("Loadsettingsfromthefile", L"Load settings from file");
  D("PDFsettings", L"PDF settings");
  D("Savereportstoautomatically", L"Save reports to PDF automatically");
  D("Savereportstopdfautomatically", L"Save reports to PDF automatically");
  D("SavePDFtofolder", L"Save PDF to folder");
  D("PDFnametemplate", L"PDF name template");
  D("OpenAfterCreation", L"Open file after creation");
  D("ZoomOut", L"Zoom Out");             D("Decreasezoom", L"Decrease zoom");
  D("Zoom", L"Zoom");                    D("ZoomIn", L"Zoom In");
  D("Increaseszoom", L"Increase zoom");  D("FirstPage", L"First Page");
  D("PriorPage", L"Prior Page");         D("NextPage", L"Next Page");
  D("LastPage", L"Last Page");           D("Stop", L"Stop");
  D("Print", L"Print");                  D("ActualSize", L"Actual size");
  D("PageWidth", L"Page width");         D("PageHeight", L"Page height");
  D("WholePage", L"Whole page");         D("ViewLastPreview", L"View last preview");
  D("DontAskAgain", L"Don't ask again"); D("Copies", L"Copies");
  D("RawPrintingMode", L"Raw printing mode");
  D("RawPrintingModeHint", L"Data is sent to the printer without any processing");
  D("RawPrintingModeAlert", L"In raw mode the data goes to the printer unchanged: preview, PDF and sequence emulation are not used. Enable raw mode?");
  D("PageRange", L"Page range (e.g. 1,3,5-9)");
  D("InterfaceLanguage", L"Interface language");
  D("Margins", L"Margins");              D("Settings", L"Settings");
  D("CPI", L"CPI:");                     D("LPI", L"LPI:");
  D("ASCIITable", L"ASCII table");       D("Lines", L"lines");
  D("Chars", L"chars");                  D("Scheme", L"Sequence scheme");
  D("Function", L"Function");            D("Name", L"Name");
  D("Ok", L"OK");                        D("Reset", L"Reset");
  D("Browse", L"...");                   D("Sample", L"Sample");
  D("Open", L"Open");                    D("Layout", L"Layout");
  D("CancelJob", L"Cancel job");         D("Exit", L"Exit");
  D("PrintDelayedTask", L"Print delayed task");
  D("NothingToPrint", L"Nothing to print");
  D("PrintError", L"The report could not be printed.");
  D("PdfSaved", L"PDF saved.");          D("PdfError", L"The PDF file could not be saved.");
  D("NoOutput", L"Select at least one output: Printer or PDF.");
  D("CaptureFailed", L"Could not capture the port. ");
  D("SavePdfTitle", L"Save As PDF");     D("PrinterNotAccessible", L"Printer %s is not accessible");
  D("SpoolFile", L"Name (mask) of spool file");
  D("Page", L"Page");                    D("Of", L"of");
  D("ConfirmDeleteScheme", L"Delete this scheme?");
  D("SchemeName", L"Scheme name");       D("BuiltIn", L"Built-in scheme (read-only)");
  D("PortSelect", L"Port settings (each port has its own)");
  D("CaptureOn", L"Capture port");
  D("Dummy", L"");
  g_cur = g_def;
}

static ws Strip(const ws& s) { return s; }

bool Select(const ws& name) {
  g_cur = g_def;
  if (_wcsicmp(name.c_str(), L"English") == 0) return true;
  std::string raw;
  if (!util::ReadFileBytes(util::AppDir() + name + L".lng", raw)) return false;
  ws text = util::Utf8ToW(raw);
  size_t p = 0;
  if (!text.empty() && text[0] == 0xFEFF) p = 1;
  while (p < text.size()) {
    size_t e = text.find_first_of(L"\r\n", p);
    if (e == ws::npos) e = text.size();
    ws line = text.substr(p, e - p);
    p = e;
    while (p < text.size() && (text[p] == L'\r' || text[p] == L'\n')) p++;
    size_t eq = line.find(L'=');
    if (eq == ws::npos || eq == 0) continue;
    std::string key = util::WToUtf8(line.substr(0, eq));
    ws val = Strip(line.substr(eq + 1));
    if (key == "Title") continue;               // product name is not translated
    if (key == "Register" || key == "Terms" || key == "RegOK" || key == "RegFail") continue;
    if (g_def.count(key)) g_cur[key] = val;
  }
  return true;
}

std::vector<ws> Available() {
  std::vector<ws> r;
  r.push_back(L"English");
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW((util::AppDir() + L"*.lng").c_str(), &fd);
  if (h != INVALID_HANDLE_VALUE) {
    do {
      ws n = fd.cFileName;
      if (n.size() > 4) n.resize(n.size() - 4);
      if (_wcsicmp(n.c_str(), L"English") != 0) r.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
  }
  std::sort(r.begin() + 1, r.end());
  return r;
}

const ws& T(const char* key) {
  auto it = g_cur.find(key);
  if (it != g_cur.end()) return it->second;
  static ws tmp;
  tmp = util::Utf8ToW(key);
  return tmp;
}

ws Tu(const char* key, unsigned v) {
  ws t = T(key);
  size_t p = t.find(L"%u");
  if (p == ws::npos) return t;
  return t.substr(0, p) + util::Fmt(L"%u", v) + t.substr(p + 2);
}
}  // namespace lang
