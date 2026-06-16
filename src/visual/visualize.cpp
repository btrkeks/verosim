#include "verosim/visual/visualize.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "verosim/engine/compare.h"
#include "verosim/extraction/extract.h"
#include "verosim/extraction/source_format_util.h"
#include "verosim/extraction/vrv_bridge.h"
#include "verosim/visual/html_report.h"
#include "verosim/visual/score_renderer.h"
#include "verosim/visual/svg_annotator.h"
#include "verosim/visual/visual_plan.h"

namespace verosim {
namespace {

CompareOptions EngineCompareOptions(const CompareCliOptions &options)
{
    return CompareOptions{ .note_position_policy = options.note_position_policy };
}

struct LoadedVisualScore {
    SymScore score;
    std::vector<std::string> warnings;
    bool ok = false;
};

bool LoadAndExtractVisualScore(VrvBridge &bridge, const std::string &path,
    const ExtractOptions &options, LoadedVisualScore &loaded, std::string &error)
{
    try {
        if (!bridge.LoadScoreFile(path)) {
            error = "failed to load " + path;
            return false;
        }
        ExtractResult result = ExtractSymScore(bridge.GetDoc(), SourceFormatFromBridge(bridge), options);
        loaded.score = std::move(result.score);
        loaded.warnings = std::move(result.warnings);
        loaded.ok = true;
        return true;
    }
    catch (const std::exception &e) {
        error = "exception loading " + path + ": " + e.what();
        return false;
    }
}

bool LoadRenderBridge(VrvBridge &bridge, const std::string &path, std::string &error)
{
    if (!ConfigureScoreRenderOptions(bridge, true, error)) return false;
    if (!bridge.LoadScoreFile(path)) {
        error = "failed to load " + path;
        return false;
    }
    return true;
}

std::vector<VisualMark> MarksForSide(const std::vector<VisualMark> &marks, VisualSide side)
{
    std::vector<VisualMark> filtered;
    for (const VisualMark &mark : marks) {
        if (mark.side == side) filtered.push_back(mark);
    }
    return filtered;
}

struct AnnotatedPages {
    std::vector<RenderedPage> pages;
    std::vector<VisualMark> unresolved;
    std::vector<std::string> warnings;
};

AnnotatedPages AnnotatePages(const RenderedScore &rendered, const std::vector<VisualMark> &marks)
{
    AnnotatedPages annotated;
    std::vector<bool> resolved(marks.size(), false);
    for (const RenderedPage &page : rendered.pages) {
        SvgAnnotationResult result = AnnotateSvg(page.svg, marks);
        if (!result.parse_ok) {
            annotated.warnings.push_back("could not parse SVG page " + std::to_string(page.page_no)
                + ": " + result.error);
        }
        for (std::size_t i = 0; i < result.resolved.size(); ++i) {
            resolved[i] = resolved[i] || result.resolved[i];
        }
        annotated.pages.push_back(RenderedPage{ .page_no = page.page_no, .svg = std::move(result.svg) });
    }
    for (std::size_t i = 0; i < marks.size(); ++i) {
        if (!resolved[i]) annotated.unresolved.push_back(marks[i]);
    }
    return annotated;
}

void PrefixWarnings(
    std::vector<std::string> &out, const std::string &prefix, const std::vector<std::string> &warnings)
{
    for (const std::string &warning : warnings) out.push_back(prefix + warning);
}

} // namespace

bool BuildVisualComparison(const std::string &pred_path, const std::string &gt_path,
    const CompareCliOptions &options, VisualReport &report, std::string &error)
{
    report = VisualReport{};

    VrvBridge pred_bridge;
    VrvBridge gt_bridge;
    const ExtractOptions extract_options{ .detail = options.detail };

    LoadedVisualScore pred;
    LoadedVisualScore gt;
    if (!LoadAndExtractVisualScore(pred_bridge, pred_path, extract_options, pred, error)) return false;
    if (!LoadAndExtractVisualScore(gt_bridge, gt_path, extract_options, gt, error)) return false;

    const CompareResult compare = CompareScores(pred.score, gt.score, EngineCompareOptions(options));
    const long n_pred = pred.score.notation_size();
    const long n_gt = gt.score.notation_size();
    const std::map<std::string, long> edit_distances = EditDistancesDict(compare.op_list);
    const VisualPlan plan = BuildVisualPlan(compare.op_list);

    RenderedScore pred_rendered;
    RenderedScore gt_rendered;
    VrvBridge pred_render_bridge;
    VrvBridge gt_render_bridge;
    if (!LoadRenderBridge(pred_render_bridge, pred_path, error)) {
        error = "prediction render failed: " + error;
        return false;
    }
    if (!LoadRenderBridge(gt_render_bridge, gt_path, error)) {
        error = "ground-truth render failed: " + error;
        return false;
    }
    if (!RenderScoreToSvgPages(pred_render_bridge, pred_rendered, error)) {
        error = "prediction render failed: " + error;
        return false;
    }
    if (!RenderScoreToSvgPages(gt_render_bridge, gt_rendered, error)) {
        error = "ground-truth render failed: " + error;
        return false;
    }

    const AnnotatedPages pred_pages = AnnotatePages(pred_rendered, MarksForSide(plan.marks, VisualSide::kPred));
    const AnnotatedPages gt_pages = AnnotatePages(gt_rendered, MarksForSide(plan.marks, VisualSide::kGt));

    report.pred_path = pred_path;
    report.gt_path = gt_path;
    report.pred = VisualizedScore{ .title = "Prediction", .pages = pred_pages.pages };
    report.gt = VisualizedScore{ .title = "Ground Truth", .pages = gt_pages.pages };
    report.distance = compare.cost;
    report.n_pred = n_pred;
    report.n_gt = n_gt;
    report.omr_ned = OmrNed(compare.cost, n_pred, n_gt);
    report.edit_distances = edit_distances;
    report.unresolved_marks = pred_pages.unresolved;
    report.unresolved_marks.insert(
        report.unresolved_marks.end(), gt_pages.unresolved.begin(), gt_pages.unresolved.end());
    PrefixWarnings(report.warnings, "pred: ", pred.warnings);
    PrefixWarnings(report.warnings, "gt: ", gt.warnings);
    PrefixWarnings(report.warnings, "pred render: ", pred_rendered.warnings);
    PrefixWarnings(report.warnings, "gt render: ", gt_rendered.warnings);
    PrefixWarnings(report.warnings, "", pred_pages.warnings);
    PrefixWarnings(report.warnings, "", gt_pages.warnings);

    return true;
}

bool VisualizePairToHtml(const std::string &pred_path, const std::string &gt_path,
    const std::string &out_path, const CompareCliOptions &options, std::string &error)
{
    VisualReport report;
    if (!BuildVisualComparison(pred_path, gt_path, options, report, error)) return false;
    return WriteHtmlReport(report, out_path, error);
}

} // namespace verosim
