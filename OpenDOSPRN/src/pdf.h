#pragma once
#include "common.h"
#include "render.h"

// Writes the selected pages (indices, 0-based) as a text PDF using the standard Courier fonts.
bool SavePdf(const ws& path, const std::vector<Page>& pages, const std::vector<int>& idx,
             const RenderStyle& st, double pageWIn, double pageHIn);

// Expands the file-name template (yy yyyy m mm mmm mmmm d dd hh nn ss (literal) <fname> <fext> <uname>).
ws ExpandPdfPattern(const ws& pattern, const ws& srcName, const SYSTEMTIME& t);
