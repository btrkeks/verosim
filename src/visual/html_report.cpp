#include "verosim/visual/html_report.h"

#include <fstream>
#include <ostream>
#include <sstream>

#include "verosim/cli/json_util.h"
#include "verosim/visual/visual_mark.h"

namespace verosim {
namespace {

std::string HtmlEscape(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string FormatDouble(double value)
{
    std::ostringstream out;
    WriteDouble(value, out);
    return out.str();
}

void WriteCss(std::ostream &out)
{
    out << R"CSS(
    :root {
      --paper: #f6f1e8;
      --ink: #16211f;
      --muted: #66736e;
      --rule: #d7d0c4;
      --panel: #fffaf0;
      --pred: #304b5f;
      --gt: #445239;
      --inserted: #087f8c;
      --deleted: #d34f45;
      --changed: #b97900;
      --shadow: 0 18px 50px rgba(42, 38, 30, 0.16);
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      color: var(--ink);
      background:
        linear-gradient(90deg, rgba(22,33,31,0.04) 1px, transparent 1px),
        linear-gradient(0deg, rgba(22,33,31,0.035) 1px, transparent 1px),
        var(--paper);
      background-size: 28px 28px;
      font-family: Georgia, "Times New Roman", serif;
    }
    header {
      padding: 28px 32px 18px;
      border-bottom: 1px solid var(--rule);
      background: rgba(246, 241, 232, 0.92);
      position: sticky;
      top: 0;
      z-index: 3;
      backdrop-filter: blur(8px);
    }
    h1 {
      margin: 0 0 12px;
      font-size: clamp(26px, 3.2vw, 42px);
      line-height: 1;
      letter-spacing: 0;
    }
    .meta {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(145px, 1fr));
      gap: 10px;
      max-width: 1120px;
    }
    .metric {
      border-left: 3px solid var(--ink);
      padding: 5px 0 5px 10px;
      min-width: 0;
    }
    .metric b {
      display: block;
      font-family: ui-monospace, "Cascadia Mono", "SFMono-Regular", monospace;
      font-size: 18px;
    }
    .metric span {
      color: var(--muted);
      font-size: 12px;
      text-transform: uppercase;
    }
    main {
      padding: 24px 32px 42px;
    }
    .summary {
      display: grid;
      grid-template-columns: minmax(0, 1fr) minmax(260px, 380px);
      gap: 18px;
      align-items: start;
      margin-bottom: 22px;
    }
    .panel {
      background: rgba(255, 250, 240, 0.88);
      border: 1px solid var(--rule);
      border-radius: 8px;
      box-shadow: var(--shadow);
      padding: 16px;
    }
    h2 {
      margin: 0 0 12px;
      font-size: 16px;
      text-transform: uppercase;
      color: var(--muted);
      letter-spacing: 0;
    }
    .paths {
      font-family: ui-monospace, "Cascadia Mono", "SFMono-Regular", monospace;
      font-size: 12px;
      line-height: 1.5;
      overflow-wrap: anywhere;
    }
    .category {
      display: grid;
      grid-template-columns: minmax(0, 1fr) auto;
      gap: 12px;
      padding: 6px 0;
      border-bottom: 1px solid rgba(215, 208, 196, 0.72);
      font-size: 14px;
    }
    .category:last-child { border-bottom: 0; }
    .legend {
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
      margin-top: 12px;
      color: var(--muted);
      font-size: 13px;
    }
    .swatch {
      display: inline-block;
      width: 12px;
      height: 12px;
      margin-right: 6px;
      vertical-align: -1px;
      border-radius: 2px;
    }
    .s-inserted { background: var(--inserted); }
    .s-deleted { background: var(--deleted); }
    .s-changed { background: var(--changed); }
    .page-pair {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(min(100%, 1100px), 1fr));
      gap: 18px;
      align-items: start;
      margin-top: 18px;
    }
    .score {
      background: #fffdf8;
      border: 1px solid var(--rule);
      border-radius: 8px;
      overflow: auto;
      box-shadow: var(--shadow);
    }
    .score h2 {
      position: sticky;
      top: 0;
      z-index: 2;
      margin: 0;
      padding: 10px 12px;
      color: #fff;
      background: var(--pred);
    }
    .score.gt h2 { background: var(--gt); }
    .svg-wrap {
      padding: 10px;
      min-width: 0;
    }
    .svg-wrap svg {
      width: 100%;
      height: auto;
      display: block;
      overflow: visible;
    }
    .verosim-mark.verosim-role-inserted,
    .verosim-mark.verosim-role-inserted * {
      color: var(--inserted) !important;
      fill: var(--inserted) !important;
      stroke: var(--inserted) !important;
    }
    .verosim-mark.verosim-role-deleted,
    .verosim-mark.verosim-role-deleted * {
      color: var(--deleted) !important;
      fill: var(--deleted) !important;
      stroke: var(--deleted) !important;
    }
    .verosim-mark.verosim-role-changed,
    .verosim-mark.verosim-role-changed * {
      color: var(--changed) !important;
      fill: var(--changed) !important;
      stroke: var(--changed) !important;
    }
    .verosim-mark.verosim-kind-measure {
      opacity: 0.78;
      filter: drop-shadow(0 0 5px rgba(185, 121, 0, 0.42));
    }
    .warnings {
      margin-top: 16px;
      color: #7a2e29;
      font-size: 13px;
    }
    @media (max-width: 900px) {
      header, main { padding-left: 16px; padding-right: 16px; }
      .summary, .page-pair { grid-template-columns: 1fr; }
    }
)CSS";
}

void WriteCategoryTable(const std::map<std::string, long> &dict, std::ostream &out)
{
    for (const auto &[name, value] : dict) {
        out << "<div class=\"category\"><span>" << HtmlEscape(name) << "</span><b>" << value
            << "</b></div>\n";
    }
}

void WriteWarnings(const VisualReport &report, std::ostream &out)
{
    if (report.warnings.empty() && report.unresolved_marks.empty()) return;
    out << "<div class=\"warnings panel\"><h2>Warnings</h2>";
    for (const std::string &warning : report.warnings) {
        out << "<p>" << HtmlEscape(warning) << "</p>";
    }
    if (!report.unresolved_marks.empty()) {
        out << "<p>" << report.unresolved_marks.size()
            << " visual marks could not be matched to rendered SVG elements.</p>";
    }
    out << "</div>\n";
}

void WriteScorePage(const VisualizedScore &score, std::size_t index, const std::string &extra_class,
    std::ostream &out)
{
    out << "<section class=\"score " << extra_class << "\"><h2>" << HtmlEscape(score.title)
        << " page " << (index + 1) << "</h2><div class=\"svg-wrap\">";
    if (index < score.pages.size()) out << score.pages[index].svg;
    out << "</div></section>\n";
}

} // namespace

bool WriteHtmlReport(const VisualReport &report, const std::string &out_path, std::string &error)
{
    std::ofstream out(out_path);
    if (!out) {
        error = "cannot write " + out_path;
        return false;
    }

    out << "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
           "<title>VeroSim OMR-NED Visualization</title><style>";
    WriteCss(out);
    out << "</style></head><body>\n<header><h1>VeroSim OMR-NED Visualization</h1><div class=\"meta\">";
    out << "<div class=\"metric\"><b>" << FormatDouble(report.omr_ned)
        << "</b><span>OMR-NED</span></div>";
    out << "<div class=\"metric\"><b>" << report.distance << "</b><span>Edit Distance</span></div>";
    out << "<div class=\"metric\"><b>" << report.n_pred << "</b><span>Prediction Symbols</span></div>";
    out << "<div class=\"metric\"><b>" << report.n_gt << "</b><span>Ground Truth Symbols</span></div>";
    out << "<div class=\"metric\"><b>" << report.unresolved_marks.size()
        << "</b><span>Unresolved Marks</span></div>";
    out << "</div></header><main>\n";

    out << "<section class=\"summary\"><div class=\"panel\"><h2>Inputs</h2><div class=\"paths\"><b>Prediction</b><br>"
        << HtmlEscape(report.pred_path) << "<br><br><b>Ground Truth</b><br>"
        << HtmlEscape(report.gt_path) << "</div><div class=\"legend\">"
        << "<span><i class=\"swatch s-inserted\"></i>Inserted</span>"
        << "<span><i class=\"swatch s-deleted\"></i>Deleted</span>"
        << "<span><i class=\"swatch s-changed\"></i>Changed</span>"
        << "</div></div><div class=\"panel\"><h2>Categories</h2>";
    WriteCategoryTable(report.edit_distances, out);
    out << "</div></section>\n";

    WriteWarnings(report, out);

    const std::size_t page_count = std::max(report.pred.pages.size(), report.gt.pages.size());
    for (std::size_t i = 0; i < page_count; ++i) {
        out << "<section class=\"page-pair\">";
        WriteScorePage(report.pred, i, "pred", out);
        WriteScorePage(report.gt, i, "gt", out);
        out << "</section>\n";
    }

    out << "</main></body></html>\n";
    return true;
}

} // namespace verosim
