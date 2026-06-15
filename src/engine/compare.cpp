#include "verosim/engine/compare.h"

#include <iterator>

#include "verosim/engine/block_diff.h"
#include "verosim/engine/interner.h"
#include "verosim/engine/myers.h"

// Memoization note (the one deliberate deviation from a 1:1 port): Python
// memoizes every recursive diff on repr() keys cleared per comparison. Those
// reprs embed unique object ids, so within one comparison only
// _block_diff_lin's own suffix memo ever hits — and it is exactly an
// index-keyed (i, j) DP per non-common block, which is how block_diff.cpp
// implements it. The other memos (_notes_set_distance, the within-note
// Levenshteins, ...) never see a repeated key in the live call graph and are
// not ported. Should a content-keyed cache ever be added (e.g. inside-bar
// cost for repeated identical measure pairs), it must cache costs only —
// op lists carry object identity and must be built from the concrete
// measures at hand.

namespace verosim {

std::string_view OpNameStr(OpName name)
{
    switch (name) {
        case OpName::kInsPart: return "inspart";
        case OpName::kDelPart: return "delpart";
        case OpName::kInsBar: return "insbar";
        case OpName::kDelBar: return "delbar";
        case OpName::kNoteIns: return "noteins";
        case OpName::kNoteDel: return "notedel";
        case OpName::kInsPitch: return "inspitch";
        case OpName::kDelPitch: return "delpitch";
        case OpName::kPitchNameEdit: return "pitchnameedit";
        case OpName::kPitchTypeEdit: return "pitchtypeedit";
        case OpName::kAccidentIns: return "accidentins";
        case OpName::kAccidentDel: return "accidentdel";
        case OpName::kAccidentEdit: return "accidentedit";
        case OpName::kTieIns: return "tieins";
        case OpName::kTieDel: return "tiedel";
        case OpName::kHeadEdit: return "headedit";
        case OpName::kDotIns: return "dotins";
        case OpName::kDotDel: return "dotdel";
        case OpName::kGraceEdit: return "graceedit";
        case OpName::kGraceSlashEdit: return "graceslashedit";
        case OpName::kInsBeam: return "insbeam";
        case OpName::kDelBeam: return "delbeam";
        case OpName::kEditBeam: return "editbeam";
        case OpName::kInsTuplet: return "instuplet";
        case OpName::kDelTuplet: return "deltuplet";
        case OpName::kEditTuplet: return "edittuplet";
        case OpName::kInsArticulation: return "insarticulation";
        case OpName::kDelArticulation: return "delarticulation";
        case OpName::kEditArticulation: return "editarticulation";
        case OpName::kInsExpression: return "insexpression";
        case OpName::kDelExpression: return "delexpression";
        case OpName::kEditExpression: return "editexpression";
        case OpName::kInsSpace: return "insspace";
        case OpName::kDelSpace: return "delspace";
        case OpName::kEditSpace: return "editspace";
        case OpName::kExtraIns: return "extrains";
        case OpName::kExtraDel: return "extradel";
        case OpName::kExtraContentEdit: return "extracontentedit";
        case OpName::kExtraSymbolEdit: return "extrasymboledit";
        case OpName::kExtraInfoEdit: return "extrainfoedit";
        case OpName::kExtraOffsetEdit: return "extraoffsetedit";
        case OpName::kExtraDurationEdit: return "extradurationedit";
        case OpName::kExtraStyleEdit: return "extrastyleedit";
    }
    return "?";
}

CompareResult CompareScores(const SymScore &pred, const SymScore &gt)
{
    StringInterner interner; // shared: content equality across scores == id equality
    const PreparedScore pred_prep = PrepareScore(pred, interner);
    const PreparedScore gt_prep = PrepareScore(gt, interner);

    CompareResult result;

    // Differing part counts: assume same order, the extra trailing parts of
    // the larger score are whole-part deletions/insertions.
    const std::size_t n_of_parts = std::min(pred.parts.size(), gt.parts.size());
    for (std::size_t p = gt.parts.size(); p < pred.parts.size(); ++p) {
        const long size = pred.parts[p].notation_size();
        result.op_list.push_back(EditOp{ .name = OpName::kDelPart,
            .a = OpSide::Part(&pred.parts[p]),
            .b = OpSide::None(),
            .cost = size });
        result.cost += size;
    }
    for (std::size_t p = pred.parts.size(); p < gt.parts.size(); ++p) {
        const long size = gt.parts[p].notation_size();
        result.op_list.push_back(EditOp{ .name = OpName::kInsPart,
            .a = OpSide::None(),
            .b = OpSide::Part(&gt.parts[p]),
            .cost = size });
        result.cost += size;
    }

    for (std::size_t p = 0; p < n_of_parts; ++p) {
        const PreparedPart &pred_part = pred_prep.parts[p];
        const PreparedPart &gt_part = gt_prep.parts[p];
        std::vector<std::int32_t> pred_ids, gt_ids;
        pred_ids.reserve(pred_part.measures.size());
        for (const PreparedMeasure &m : pred_part.measures) pred_ids.push_back(m.content_id);
        gt_ids.reserve(gt_part.measures.size());
        for (const PreparedMeasure &m : gt_part.measures) gt_ids.push_back(m.content_id);

        for (const NcsBlock &block : NonCommonSubsequences(pred_ids, gt_ids)) {
            DiffResult block_diff
                = BlockDiffLin(pred.parts[p], pred_part, gt.parts[p], gt_part, block);
            result.cost += block_diff.cost;
            result.op_list.insert(result.op_list.end(),
                std::make_move_iterator(block_diff.ops.begin()),
                std::make_move_iterator(block_diff.ops.end()));
        }
    }

    // Staff groups and metadata items: their DetailLevel bits (StaffDetails,
    // Metadata) are off at every v1 tier, so both lists are empty and the set
    // distances are no-ops. num_syntax_errors_fixed is 0 on this side (D4
    // deferred) — the oracle's distance can include it for kern preds.
    return result;
}

namespace {

// Visualization._HEADER_NAME_OF_EDIT_NAME (visualization.py:3234+), restricted
// to edit names this engine can emit. The Voicing-only pitch entries are NOT
// in the table (Voicing is off at v1), so pitch ops fall back to
// 'directionins' -> 'wrong direction OMR-ED' exactly like Python.
std::string HeaderName(const std::string &edit_name)
{
    static const std::map<std::string, std::string> kTable = {
        { "noteins", "wrong note OMR-ED" },
        { "notedel", "wrong note OMR-ED" },
        { "insspace", "wrong note OMR-ED" },
        { "delspace", "wrong note OMR-ED" },
        { "editspace", "wrong note OMR-ED" },
        { "headedit", "wrong note head OMR-ED" },
        { "insbeam", "wrong flag/beam OMR-ED" },
        { "delbeam", "wrong flag/beam OMR-ED" },
        { "editbeam", "wrong flag/beam OMR-ED" },
        { "dotdel", "wrong dot OMR-ED" },
        { "dotins", "wrong dot OMR-ED" },
        { "instuplet", "wrong tuplet OMR-ED" },
        { "deltuplet", "wrong tuplet OMR-ED" },
        { "edittuplet", "wrong tuplet OMR-ED" },
        { "accidentins", "wrong accidental OMR-ED" },
        { "accidentdel", "wrong accidental OMR-ED" },
        { "accidentedit", "wrong accidental OMR-ED" },
        { "graceedit", "wrong graceness OMR-ED" },
        { "graceslashedit", "wrong graceness OMR-ED" },
        { "tiedel", "wrong tie OMR-ED" },
        { "tieins", "wrong tie OMR-ED" },
        { "insarticulation", "wrong articulation OMR-ED" },
        { "delarticulation", "wrong articulation OMR-ED" },
        { "editarticulation", "wrong articulation OMR-ED" },
        { "insexpression", "wrong expression OMR-ED" },
        { "delexpression", "wrong expression OMR-ED" },
        { "editexpression", "wrong expression OMR-ED" },
        { "clefins", "wrong clef OMR-ED" },
        { "clefdel", "wrong clef OMR-ED" },
        { "clefcontentedit", "wrong clef OMR-ED" },
        { "clefsymboledit", "wrong clef OMR-ED" },
        { "clefinfoedit", "wrong clef OMR-ED" },
        { "clefoffsetedit", "wrong clef OMR-ED" },
        { "clefdurationedit", "wrong clef OMR-ED" },
        { "clefstyleedit", "wrong clef OMR-ED" },
        { "timesigins", "wrong timesig OMR-ED" },
        { "timesigdel", "wrong timesig OMR-ED" },
        { "timesigcontentedit", "wrong timesig OMR-ED" },
        { "timesigsymboledit", "wrong timesig OMR-ED" },
        { "timesiginfoedit", "wrong timesig OMR-ED" },
        { "timesigoffsetedit", "wrong timesig OMR-ED" },
        { "timesigdurationedit", "wrong timesig OMR-ED" },
        { "timesigstyleedit", "wrong timesig OMR-ED" },
        { "keysigins", "wrong keysig OMR-ED" },
        { "keysigdel", "wrong keysig OMR-ED" },
        { "keysigcontentedit", "wrong keysig OMR-ED" },
        { "keysigsymboledit", "wrong keysig OMR-ED" },
        { "keysiginfoedit", "wrong keysig OMR-ED" },
        { "keysigoffsetedit", "wrong keysig OMR-ED" },
        { "keysigdurationedit", "wrong keysig OMR-ED" },
        { "keysigstyleedit", "wrong keysig OMR-ED" },
        { "slurins", "wrong slur OMR-ED" },
        { "slurdel", "wrong slur OMR-ED" },
        { "slurcontentedit", "wrong slur OMR-ED" },
        { "slursymboledit", "wrong slur OMR-ED" },
        { "slurinfoedit", "wrong slur OMR-ED" },
        { "sluroffsetedit", "wrong slur OMR-ED" },
        { "slurdurationedit", "wrong slur OMR-ED" },
        { "slurstyleedit", "wrong slur OMR-ED" },
        { "dynamicins", "wrong dynamic OMR-ED" },
        { "dynamicdel", "wrong dynamic OMR-ED" },
        { "dynamiccontentedit", "wrong dynamic OMR-ED" },
        { "dynamicsymboledit", "wrong dynamic OMR-ED" },
        { "dynamicinfoedit", "wrong dynamic OMR-ED" },
        { "dynamicoffsetedit", "wrong dynamic OMR-ED" },
        { "dynamicdurationedit", "wrong dynamic OMR-ED" },
        { "dynamicstyleedit", "wrong dynamic OMR-ED" },
        { "crescendoins", "wrong crescendo OMR-ED" },
        { "crescendodel", "wrong crescendo OMR-ED" },
        { "crescendocontentedit", "wrong crescendo OMR-ED" },
        { "crescendosymboledit", "wrong crescendo OMR-ED" },
        { "crescendoinfoedit", "wrong crescendo OMR-ED" },
        { "crescendooffsetedit", "wrong crescendo OMR-ED" },
        { "crescendodurationedit", "wrong crescendo OMR-ED" },
        { "crescendostyleedit", "wrong crescendo OMR-ED" },
        { "diminuendoins", "wrong diminuendo OMR-ED" },
        { "diminuendodel", "wrong diminuendo OMR-ED" },
        { "diminuendocontentedit", "wrong diminuendo OMR-ED" },
        { "diminuendosymboledit", "wrong diminuendo OMR-ED" },
        { "diminuendoinfoedit", "wrong diminuendo OMR-ED" },
        { "diminuendooffsetedit", "wrong diminuendo OMR-ED" },
        { "diminuendodurationedit", "wrong diminuendo OMR-ED" },
        { "diminuendostyleedit", "wrong diminuendo OMR-ED" },
        { "insbar", "entire measure insert/delete OMR-ED" },
        { "delbar", "entire measure insert/delete OMR-ED" },
        { "inspart", "entire staff insert/delete OMR-ED" },
        { "delpart", "entire staff insert/delete OMR-ED" },
    };
    const auto it = kTable.find(edit_name);
    if (it == kTable.end()) return "wrong direction OMR-ED"; // 'directionins' fallback
    return it->second;
}

std::string CategoryEditName(const EditOp &op)
{
    std::string edit_name(OpNameStr(op.name));
    if (edit_name.rfind("extra", 0) == 0) {
        const SymExtra *extra = op.a.kind == OpSide::Kind::kExtra ? op.a.extra : op.b.extra;
        if (extra != nullptr) {
            // re.sub('extra', kind, edit_name)
            edit_name = std::string(ExtraKindName(extra->kind)) + edit_name.substr(5);
        }
    }
    return edit_name;
}

} // namespace

std::string EditOpCategory(const EditOp &op)
{
    return HeaderName(CategoryEditName(op));
}

std::map<std::string, long> EditDistancesDict(const std::vector<EditOp> &ops)
{
    std::map<std::string, long> dict;
    for (const EditOp &op : ops) {
        dict[EditOpCategory(op)] += op.cost;
    }
    dict["bad kern syntax OMR-ED"] = 0; // D4 deferred; always present like Python
    return dict;
}

double OmrNed(long cost, long n_pred, long n_gt)
{
    const long total = n_pred + n_gt;
    if (total == 0) return 0.0; // both empty == identical
    return static_cast<double>(cost) / static_cast<double>(total);
}

} // namespace verosim
