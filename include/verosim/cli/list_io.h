#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace verosim {

struct PairRow {
    std::string pred;
    std::string gt;
};

std::string JoinBaseDir(const std::string &base_dir, const std::string &path);
std::vector<std::string> ReadFileList(const std::string &list_path);
std::vector<PairRow> ReadPairList(const std::string &list_path, std::ostream &err);

} // namespace verosim
