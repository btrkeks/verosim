#include "verosim/visual/svg_bundle.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <ostream>

#include "verosim/cli/json_util.h"

namespace verosim {
namespace {

void WriteManifestSide(const SvgBundleSide &side, std::ostream &out)
{
    out << "{\"pages\":[";
    for (std::size_t i = 0; i < side.pages.size(); ++i) {
        const SvgBundlePage &page = side.pages[i];
        if (i > 0) out << ",";
        out << "{\"page\":" << page.page << ",\"path\":";
        WriteJsonString(page.path, out);
        out << "}";
    }
    out << "]}";
}

bool WriteSidePages(const VisualizedScore &score, const std::filesystem::path &root,
    const std::string &side_dir, SvgBundleSide &side, std::string &error)
{
    const std::filesystem::path dir = root / side_dir;
    std::filesystem::create_directories(dir);
    side.pages.clear();

    for (const RenderedPage &page : score.pages) {
        const std::string filename = "page-" + std::to_string(page.page_no) + ".svg";
        const std::filesystem::path relative = std::filesystem::path(side_dir) / filename;
        const std::filesystem::path path = root / relative;
        std::ofstream out(path);
        if (!out) {
            error = "cannot write " + path.string();
            return false;
        }
        out << page.svg;
        if (!out) {
            error = "failed to write " + path.string();
            return false;
        }
        side.pages.push_back(SvgBundlePage{
            .page = page.page_no,
            .path = relative.generic_string(),
        });
    }
    return true;
}

bool WriteManifest(const SvgAssetBundle &bundle, const std::filesystem::path &manifest_path,
    std::string &error)
{
    std::ofstream out(manifest_path);
    if (!out) {
        error = "cannot write " + manifest_path.string();
        return false;
    }
    out << "{\"schema_version\":1,\"prediction\":";
    WriteManifestSide(bundle.prediction, out);
    out << ",\"ground_truth\":";
    WriteManifestSide(bundle.ground_truth, out);
    out << "}\n";
    if (!out) {
        error = "failed to write " + manifest_path.string();
        return false;
    }
    return true;
}

} // namespace

bool WriteSvgAssetBundle(
    const VisualReport &report, const std::string &out_dir, SvgAssetBundle &bundle, std::string &error)
{
    bundle = SvgAssetBundle{};
    const std::filesystem::path root(out_dir);
    try {
        std::filesystem::create_directories(root);
        if (!WriteSidePages(report.pred, root, "prediction", bundle.prediction, error)) return false;
        if (!WriteSidePages(report.gt, root, "ground_truth", bundle.ground_truth, error)) return false;

        const std::filesystem::path manifest_path = root / "visualization.json";
        bundle.manifest_path = manifest_path.string();
        return WriteManifest(bundle, manifest_path, error);
    }
    catch (const std::exception &e) {
        error = "failed to write SVG asset bundle: " + std::string(e.what());
        return false;
    }
}

} // namespace verosim
