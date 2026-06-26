#include "verosim/visual/visualize.h"

#include <string>
#include <vector>

#include "verosim/app/score_pipeline.h"
#include "verosim/extraction/vrv_bridge.h"
#include "verosim/visual/html_report.h"
#include "verosim/visual/score_renderer.h"
#include "verosim/visual/svg_annotator.h"
#include "verosim/visual/visual_plan.h"
#include "verosim/visual/visual_resolver.h"

namespace verosim {
namespace {

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
    int measure_idx_offset = 0;
    for (const RenderedPage &page : rendered.pages) {
        VisualResolveResult resolve = ResolveVisualMarks(page.svg, marks, measure_idx_offset);
        if (!resolve.parse_ok) {
            annotated.warnings.push_back("could not parse SVG page " + std::to_string(page.page_no)
                + ": " + resolve.error);
            annotated.pages.push_back(page);
            continue;
        }
        measure_idx_offset += resolve.measure_count;
        SvgAnnotationResult result = AnnotateSvg(page.svg, resolve.marks, marks.size());
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

} // namespace

bool BuildVisualComparison(const std::string &pred_path, const std::string &gt_path,
    const CompareRunOptions &options, VisualReport &report, std::string &error)
{
    report = VisualReport{};

    VrvBridge pred_bridge;
    VrvBridge gt_bridge;
    const ExtractOptions extract_options = ExtractOptionsForRun(options);

    const LoadedScore pred = LoadAndExtractScoreFile(pred_bridge, pred_path, extract_options);
    if (!pred.ok) {
        error = pred.error;
        return false;
    }
    const LoadedScore gt = LoadAndExtractScoreFile(gt_bridge, gt_path, extract_options);
    if (!gt.ok) {
        error = gt.error;
        return false;
    }

    const ScoreComparison comparison = CompareLoadedScores(pred.score, gt.score, options);
    const VisualPlan plan = BuildVisualPlan(comparison.diff.op_list);

    RenderedScore pred_rendered;
    RenderedScore gt_rendered;
    if (!RenderScoreToSvgPages(pred_bridge, pred_rendered, error)) {
        error = "prediction render failed: " + error;
        return false;
    }
    if (!RenderScoreToSvgPages(gt_bridge, gt_rendered, error)) {
        error = "ground-truth render failed: " + error;
        return false;
    }

    const AnnotatedPages pred_pages = AnnotatePages(pred_rendered, MarksForSide(plan.marks, VisualSide::kPred));
    const AnnotatedPages gt_pages = AnnotatePages(gt_rendered, MarksForSide(plan.marks, VisualSide::kGt));

    report.pred_path = pred_path;
    report.gt_path = gt_path;
    report.pred = VisualizedScore{ .title = "Prediction", .pages = pred_pages.pages };
    report.gt = VisualizedScore{ .title = "Ground Truth", .pages = gt_pages.pages };
    report.distance = comparison.diff.cost;
    report.n_pred = comparison.n_pred;
    report.n_gt = comparison.n_gt;
    report.omr_ned = comparison.omr_ned;
    report.edit_distances = comparison.edit_distances;
    report.unresolved_marks = pred_pages.unresolved;
    report.unresolved_marks.insert(
        report.unresolved_marks.end(), gt_pages.unresolved.begin(), gt_pages.unresolved.end());
    AppendPrefixedWarnings(report.warnings, "pred: ", pred.warnings);
    AppendPrefixedWarnings(report.warnings, "gt: ", gt.warnings);
    AppendPrefixedWarnings(report.warnings, "pred render: ", pred_rendered.warnings);
    AppendPrefixedWarnings(report.warnings, "gt render: ", gt_rendered.warnings);
    AppendPrefixedWarnings(report.warnings, "", pred_pages.warnings);
    AppendPrefixedWarnings(report.warnings, "", gt_pages.warnings);

    return true;
}

bool VisualizePairToHtml(const std::string &pred_path, const std::string &gt_path,
    const std::string &out_path, const CompareRunOptions &options, std::string &error)
{
    VisualReport report;
    if (!BuildVisualComparison(pred_path, gt_path, options, report, error)) return false;
    return WriteHtmlReport(report, out_path, error);
}

} // namespace verosim
