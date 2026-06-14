#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "verosim/cli/compare_cli.h"
#include "verosim/cli/list_io.h"

namespace verosim {

void CompareBatchToJson(const std::vector<PairRow> &pairs, const std::string &base_dir, int jobs,
    const CompareCliOptions &options, std::ostream &out);

} // namespace verosim
