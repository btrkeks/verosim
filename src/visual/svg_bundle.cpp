#include "verosim/visual/svg_bundle.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>

#include "verosim/cli/json_util.h"

#include <pugixml.hpp>

namespace verosim {
namespace {

constexpr const char *kStandaloneAnnotationCss = R"CSS(
.verosim-mark.verosim-role-inserted,
.verosim-mark.verosim-role-inserted * {
  color: #087f8c !important;
  fill: #087f8c !important;
  stroke: #087f8c !important;
}
.verosim-mark.verosim-role-deleted,
.verosim-mark.verosim-role-deleted * {
  color: #d34f45 !important;
  fill: #d34f45 !important;
  stroke: #d34f45 !important;
}
.verosim-mark.verosim-role-changed,
.verosim-mark.verosim-role-changed * {
  color: #b97900 !important;
  fill: #b97900 !important;
  stroke: #b97900 !important;
}
.verosim-mark.verosim-kind-measure {
  opacity: 0.78;
  filter: drop-shadow(0 0 5px rgba(185, 121, 0, 0.42));
}
)CSS";

std::string SvgWithStandaloneAnnotationStyle(const std::string &svg)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result parsed = doc.load_string(svg.c_str(), pugi::parse_default);
    if (!parsed) return svg;

    pugi::xml_node root = doc.document_element();
    if (std::string(root.name()) != "svg") return svg;

    pugi::xml_node style = root.prepend_child("style");
    style.append_attribute("type").set_value("text/css");
    style.text().set(kStandaloneAnnotationCss);

    std::ostringstream out;
    doc.save(out, "  ", pugi::format_default | pugi::format_no_declaration);
    return out.str();
}

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
        out << SvgWithStandaloneAnnotationStyle(page.svg);
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
