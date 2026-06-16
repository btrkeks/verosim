#include "verosim/engine/set_distance.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

#include "verosim/engine/note_diff.h"

namespace verosim {

bool AreDifferentEnough(const Fraction &a, const Fraction &b)
{
    if (a == b) return false;
    Fraction diff = a - b;
    if (diff < Fraction(0)) diff = Fraction(0) - diff;
    return diff > Fraction(1, 10000);
}

bool AreDifferentEnough(const std::optional<Fraction> &a, const std::optional<Fraction> &b)
{
    if (a.has_value() != b.has_value()) return true; // one None, one not
    if (!a.has_value()) return false; // both None
    return AreDifferentEnough(*a, *b);
}

namespace {

enum class SeqChoice : std::uint8_t { kDel, kIns, kEdit };

// Shared greedy pairing skeleton of the two set distances: for each original
// element scan the not-yet-paired compare_to elements in order; the first one
// passing the required keys is remembered as fallback, the first one also
// passing the preferred keys is taken immediately (and removed positionally,
// like unpaired_comp_notes.pop(i)).
template <typename Required, typename Preferred>
struct Pairing {
    std::vector<std::pair<int, int>> paired; // (orig idx, comp idx) in pairing order
    std::vector<int> unpaired_orig; // orig indices in order
    std::vector<int> unpaired_comp; // comp indices still unpaired, in order

    Pairing(int n_orig, int n_comp, Required required, Preferred preferred)
    {
        unpaired_comp.resize(n_comp);
        std::iota(unpaired_comp.begin(), unpaired_comp.end(), 0);
        for (int o = 0; o < n_orig; ++o) {
            int fallback_pos = -1;
            bool found_it = false;
            for (int pos = 0; pos < static_cast<int>(unpaired_comp.size()); ++pos) {
                const int c = unpaired_comp[pos];
                if (!required(o, c)) continue;
                if (fallback_pos < 0) fallback_pos = pos;
                if (!preferred(o, c)) continue;
                paired.emplace_back(o, c);
                unpaired_comp.erase(unpaired_comp.begin() + pos);
                found_it = true;
                break;
            }
            if (found_it) continue;
            if (fallback_pos >= 0) {
                paired.emplace_back(o, unpaired_comp[fallback_pos]);
                unpaired_comp.erase(unpaired_comp.begin() + fallback_pos);
                continue;
            }
            unpaired_orig.push_back(o);
        }
    }
};

bool InfodictsEqual(const std::vector<std::pair<std::string, std::string>> &a,
    const std::vector<std::pair<std::string, std::string>> &b)
{
    // Python compares dicts (order-insensitive). Both sides come from the
    // same extraction code so orders coincide in practice, but match dict
    // semantics exactly: same keys, same values.
    if (a.size() != b.size()) return false;
    for (const auto &[k, v] : a) {
        bool found = false;
        for (const auto &[k2, v2] : b) {
            if (k == k2) {
                found = (v == v2);
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

long StringsNdiffDistance(const std::string &a, const std::string &b)
{
    const std::size_t m = a.size();
    const std::size_t n = b.size();
    std::vector<int> lcs((m + 1) * (n + 1), 0);
    const auto at = [&](std::size_t i, std::size_t j) -> int & { return lcs[i * (n + 1) + j]; };
    for (std::size_t i = m; i-- > 0;) {
        for (std::size_t j = n; j-- > 0;) {
            at(i, j) = a[i] == b[j] ? at(i + 1, j + 1) + 1 : std::max(at(i + 1, j), at(i, j + 1));
        }
    }

    long distance = 0;
    std::array<long, 2> counter{ 0, 0 }; // '-' then '+'
    std::size_t i = 0, j = 0;
    const auto flush_equal = [&] {
        distance += std::max(counter[0], counter[1]);
        counter = { 0, 0 };
    };
    while (i < m && j < n) {
        if (a[i] == b[j]) {
            flush_equal();
            ++i;
            ++j;
        }
        else if (at(i + 1, j) >= at(i, j + 1)) {
            ++counter[0];
            ++i;
        }
        else {
            ++counter[1];
            ++j;
        }
    }
    while (i++ < m) ++counter[0];
    while (j++ < n) ++counter[1];
    flush_equal();
    return distance;
}

EditOp NoteDelOp(const SymNote &n)
{
    return EditOp{ .name = OpName::kNoteDel,
        .a = OpSide::Note(&n),
        .b = OpSide::None(),
        .cost = n.notation_size(),
        .ids_kind = EditOp::IdsKind::kChordIdx,
        .ids0 = n.note_idx_in_chord };
}

EditOp NoteInsOp(const SymNote &n)
{
    return EditOp{ .name = OpName::kNoteIns,
        .a = OpSide::None(),
        .b = OpSide::Note(&n),
        .cost = n.notation_size(),
        .ids_kind = EditOp::IdsKind::kChordIdx,
        .ids0 = n.note_idx_in_chord };
}

bool SameVisualPitchAndGrace(const SymNote &orig, const SymNote &comp)
{
    // visual pitch position only (accidental ignored, so an accidental
    // change is a pitch edit, not a note remove/insert)
    return orig.pitches[0].step_octave == comp.pitches[0].step_octave
        && orig.note_is_grace == comp.note_is_grace;
}

long NotePairCost(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep, int o, int c)
{
    if (orig_prep.note_str_ids[o] == comp_prep.note_str_ids[c]) return 0;
    return AnnotatedNoteDiff(orig.notes[o], comp.notes[c]).cost;
}

void AppendNotePairOps(DiffResult &result, const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep, int o, int c)
{
    if (orig_prep.note_str_ids[o] == comp_prep.note_str_ids[c]) return;
    DiffResult sub = AnnotatedNoteDiff(orig.notes[o], comp.notes[c]);
    result.ops.insert(result.ops.end(), std::make_move_iterator(sub.ops.begin()),
        std::make_move_iterator(sub.ops.end()));
}

DiffResult NotesSetDistanceMusical(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep)
{
    const auto required = [&](int o, int c) {
        const SymNote &on = orig.notes[o];
        const SymNote &cn = comp.notes[c];
        if (!SameVisualPitchAndGrace(on, cn)) return false;
        return !AreDifferentEnough(on.note_offset, cn.note_offset);
    };
    const auto preferred = [&](int o, int c) {
        const SymNote &on = orig.notes[o];
        const SymNote &cn = comp.notes[c];
        return on.note_dur_type == cn.note_dur_type && on.note_dur_dots == cn.note_dur_dots;
    };
    const Pairing pairing(static_cast<int>(orig.notes.size()), static_cast<int>(comp.notes.size()),
        required, preferred);

    DiffResult result;
    for (const int o : pairing.unpaired_orig) {
        const SymNote &n = orig.notes[o];
        const long size = n.notation_size();
        result.cost += size;
        result.ops.push_back(NoteDelOp(n));
    }
    for (const int c : pairing.unpaired_comp) {
        const SymNote &n = comp.notes[c];
        const long size = n.notation_size();
        result.cost += size;
        result.ops.push_back(NoteInsOp(n));
    }
    for (const auto &[o, c] : pairing.paired) {
        const long cost = NotePairCost(orig, comp, orig_prep, comp_prep, o, c);
        result.cost += cost;
        AppendNotePairOps(result, orig, comp, orig_prep, comp_prep, o, c);
    }
    return result;
}

DiffResult NotesSetDistanceVisual(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep)
{
    const int m = static_cast<int>(orig.notes.size());
    const int n = static_cast<int>(comp.notes.size());
    std::vector<long> cost(static_cast<std::size_t>(m + 1) * (n + 1), 0);
    std::vector<SeqChoice> choice(
        static_cast<std::size_t>(m + 1) * (n + 1), SeqChoice::kEdit);
    const auto cost_at = [&](int i, int j) -> long & {
        return cost[static_cast<std::size_t>(i) * (n + 1) + j];
    };
    const auto choice_at = [&](int i, int j) -> SeqChoice & {
        return choice[static_cast<std::size_t>(i) * (n + 1) + j];
    };

    for (int i = m - 1; i >= 0; --i) {
        cost_at(i, n) = cost_at(i + 1, n) + orig.notes[i].notation_size();
        choice_at(i, n) = SeqChoice::kDel;
    }
    for (int j = n - 1; j >= 0; --j) {
        cost_at(m, j) = cost_at(m, j + 1) + comp.notes[j].notation_size();
        choice_at(m, j) = SeqChoice::kIns;
    }
    for (int i = m - 1; i >= 0; --i) {
        for (int j = n - 1; j >= 0; --j) {
            long best = cost_at(i + 1, j) + orig.notes[i].notation_size();
            SeqChoice pick = SeqChoice::kDel;
            const long ins = cost_at(i, j + 1) + comp.notes[j].notation_size();
            if (ins < best) {
                best = ins;
                pick = SeqChoice::kIns;
            }
            if (SameVisualPitchAndGrace(orig.notes[i], comp.notes[j])) {
                const long edit = cost_at(i + 1, j + 1)
                    + NotePairCost(orig, comp, orig_prep, comp_prep, i, j);
                if (edit < best) {
                    best = edit;
                    pick = SeqChoice::kEdit;
                }
            }
            cost_at(i, j) = best;
            choice_at(i, j) = pick;
        }
    }

    DiffResult result;
    result.cost = cost_at(0, 0);
    int i = 0;
    int j = 0;
    while (i < m || j < n) {
        switch (choice_at(i, j)) {
            case SeqChoice::kDel:
                result.ops.push_back(NoteDelOp(orig.notes[i]));
                ++i;
                break;
            case SeqChoice::kIns:
                result.ops.push_back(NoteInsOp(comp.notes[j]));
                ++j;
                break;
            case SeqChoice::kEdit:
                AppendNotePairOps(result, orig, comp, orig_prep, comp_prep, i, j);
                ++i;
                ++j;
                break;
        }
    }
    return result;
}

} // namespace

DiffResult NotesSetDistance(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep,
    const CompareOptions &options)
{
    switch (options.note_position_policy) {
        case NotePositionPolicy::kMusicalOnset:
            return NotesSetDistanceMusical(orig, comp, orig_prep, comp_prep);
        case NotePositionPolicy::kVisualEventOrder:
            return NotesSetDistanceVisual(orig, comp, orig_prep, comp_prep);
    }
    return NotesSetDistanceVisual(orig, comp, orig_prep, comp_prep);
}

DiffResult ExtrasSetDistance(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep)
{
    const auto required = [&](int o, int c) {
        const SymExtra &oe = orig.extras[o];
        const SymExtra &ce = comp.extras[c];
        if (oe.kind != ce.kind) return false;
        return !AreDifferentEnough(oe.offset, ce.offset);
    };
    const auto preferred = [&](int o, int c) {
        const SymExtra &oe = orig.extras[o];
        const SymExtra &ce = comp.extras[c];
        if (AreDifferentEnough(oe.duration, ce.duration)) return false;
        // kind-specific preferences (two simultaneous keysigs/timesigs/clefs
        // must not be cross-paired). 'slur' placement is Style-gated and
        // therefore vacuous at v1, exactly like musicdiff.
        switch (oe.kind) {
            case ExtraKind::kClef: return oe.symbolic == ce.symbolic;
            case ExtraKind::kCrescendo:
            case ExtraKind::kDiminuendo:
            case ExtraKind::kDynamic:
            case ExtraKind::kSlur:
                return true;
            case ExtraKind::kKeySig:
            case ExtraKind::kTimeSig: return InfodictsEqual(oe.infodict, ce.infodict);
        }
        return true;
    };
    const Pairing pairing(static_cast<int>(orig.extras.size()),
        static_cast<int>(comp.extras.size()), required, preferred);

    DiffResult result;
    for (const int o : pairing.unpaired_orig) {
        const SymExtra &e = orig.extras[o];
        const long size = e.notation_size();
        result.cost += size;
        result.ops.push_back(EditOp{
            .name = OpName::kExtraDel, .a = OpSide::Extra(&e), .b = OpSide::None(), .cost = size });
    }
    for (const int c : pairing.unpaired_comp) {
        const SymExtra &e = comp.extras[c];
        const long size = e.notation_size();
        result.cost += size;
        result.ops.push_back(EditOp{
            .name = OpName::kExtraIns, .a = OpSide::None(), .b = OpSide::Extra(&e), .cost = size });
    }
    for (const auto &[o, c] : pairing.paired) {
        if (orig_prep.extra_str_ids[o] == comp_prep.extra_str_ids[c]) continue; // AnnExtra.__eq__
        DiffResult sub = AnnotatedExtraDiff(orig.extras[o], comp.extras[c]);
        result.cost += sub.cost;
        result.ops.insert(result.ops.end(), std::make_move_iterator(sub.ops.begin()),
            std::make_move_iterator(sub.ops.end()));
    }
    return result;
}

DiffResult AnnotatedExtraDiff(const SymExtra &e1, const SymExtra &e2)
{
    DiffResult result;
    const auto extra_op = [&](OpName name, long cost) {
        return EditOp{
            .name = name, .a = OpSide::Extra(&e1), .b = OpSide::Extra(&e2), .cost = cost
        };
    };
    // content
    if (e1.content != e2.content) {
        const long content_cost = StringsNdiffDistance(e1.content.value_or(""), e2.content.value_or(""));
        result.cost += content_cost;
        result.ops.push_back(extra_op(OpName::kExtraContentEdit, content_cost));
    }

    // symbolic (cost 2: delete one symbol, add the other); optional<> equality
    // matches Python None semantics
    if (e1.symbolic != e2.symbolic) {
        result.cost += 2;
        result.ops.push_back(extra_op(OpName::kExtraSymbolEdit, 2));
    }
    // infodict
    if (!InfodictsEqual(e1.infodict, e2.infodict)) {
        long info_cost = 0;
        for (const auto &[k, v] : e1.infodict) {
            const auto it = std::find_if(e2.infodict.begin(), e2.infodict.end(),
                [&k](const auto &kv) { return kv.first == k; });
            if (it == e2.infodict.end()) info_cost += 1; // delete a symbol
            else if (it->second != v) info_cost += 2; // delete + add
        }
        for (const auto &[k, v] : e2.infodict) {
            const bool in_e1 = std::any_of(e1.infodict.begin(), e1.infodict.end(),
                [&k](const auto &kv) { return kv.first == k; });
            if (!in_e1) info_cost += 1; // add a symbol
        }
        result.cost += info_cost;
        result.ops.push_back(extra_op(OpName::kExtraInfoEdit, info_cost));
    }
    // offset
    if (AreDifferentEnough(e1.offset, e2.offset)) {
        result.cost += 1;
        result.ops.push_back(extra_op(OpName::kExtraOffsetEdit, 1));
    }
    // duration
    if (AreDifferentEnough(e1.duration, e2.duration)) {
        result.cost += 1;
        result.ops.push_back(extra_op(OpName::kExtraDurationEdit, 1));
    }
    // styledict: never set at v1 tiers (extrastyleedit unreachable).
    return result;
}

} // namespace verosim
