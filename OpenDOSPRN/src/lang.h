#pragma once
#include "common.h"
namespace lang {
void Init();                         // load English defaults
bool Select(const ws& name);         // "English" or name of a .lng beside the exe
std::vector<ws> Available();
const ws& T(const char* key);        // translated text (falls back to English, then key)
ws Tu(const char* key, unsigned v);  // text with a %u placeholder
}  // namespace lang
