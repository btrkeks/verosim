#include "verosim/cli/list_io.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace verosim {

std::string JoinBaseDir(const std::string &base_dir, const std::string &path)
{
    return base_dir.empty() ? path : base_dir + "/" + path;
}

std::vector<std::string> ReadFileList(const std::string &list_path)
{
    std::ifstream list(list_path);
    if (!list) throw std::runtime_error("cannot read file list " + list_path);

    std::vector<std::string> files;
    std::string line;
    while (std::getline(list, line)) {
        if (line.empty() || line[0] == '#') continue;
        files.push_back(line);
    }
    return files;
}

std::vector<PairRow> ReadPairList(const std::string &list_path, std::ostream &err)
{
    std::ifstream list(list_path);
    if (!list) throw std::runtime_error("cannot read pairs list " + list_path);

    std::vector<PairRow> pairs;
    std::string line;
    while (std::getline(list, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            err << "verosim: skipping malformed pairs line (no tab): " << line << '\n';
            continue;
        }
        pairs.push_back({ line.substr(0, tab), line.substr(tab + 1) });
    }
    return pairs;
}

} // namespace verosim
