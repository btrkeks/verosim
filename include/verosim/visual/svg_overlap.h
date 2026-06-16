#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace verosim {

struct SvgBBox {
    std::string object_id;
    std::string group_id;
    std::string source_id;
    std::string kind;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    bool content = false;
};

struct SvgOverlapPair {
    std::string first_id;
    std::string second_id;
    std::string first_kind;
    std::string second_kind;
    double area = 0.0;
    double ratio = 0.0;
};

struct SvgOverlapSummary {
    int page_no = 1;
    std::size_t candidate_count = 0;
    std::size_t overlap_count = 0;
    std::vector<SvgOverlapPair> worst_pairs;
};

struct SvgOverlapOptions {
    double min_overlap_ratio = 0.15;
    std::size_t max_reported_pairs = 3;
    std::vector<std::string> target_kinds = { "note", "rest" };
    bool ignore_same_source = true;
    bool ignore_shared_notehead_columns = true;
};

struct SvgOverlapResult {
    bool parse_ok = true;
    std::string error;
    SvgOverlapSummary summary;
};

struct SvgBBoxExtractionResult {
    bool parse_ok = true;
    std::string error;
    std::vector<SvgBBox> boxes;
};

SvgBBoxExtractionResult ExtractVerovioSvgBBoxes(const std::string &svg);

SvgOverlapSummary DetectBBoxOverlaps(
    const std::vector<SvgBBox> &boxes, int page_no, const SvgOverlapOptions &options = {});

SvgOverlapResult DetectSvgBBoxOverlaps(
    const std::string &svg, int page_no, const SvgOverlapOptions &options = {});

std::string FormatSvgOverlapWarning(const SvgOverlapSummary &summary);

} // namespace verosim
