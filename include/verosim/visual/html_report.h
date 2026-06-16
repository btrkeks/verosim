#pragma once

#include <string>

#include "verosim/visual/visual_report.h"

namespace verosim {

bool WriteHtmlReport(const VisualReport &report, const std::string &out_path, std::string &error);

} // namespace verosim
