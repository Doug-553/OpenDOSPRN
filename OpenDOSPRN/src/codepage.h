#pragma once
#include "common.h"

struct CodePage {
  wchar_t map[256];
  std::map<uint16_t, wchar_t> dbcs;   // double-byte tables (.cpx)
  bool lead[256];
  ws name;
  CodePage();
  static bool Load(const ws& name, CodePage& out);      // never fails for numeric names
  bool IsLead(unsigned char b) const { return lead[b]; }
};

std::vector<std::pair<ws, ws>> LoadCodePageList();       // codepages.lst: id -> description
