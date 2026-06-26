#include "extractor.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "verosim/extraction/notation_rules.h"

namespace verosim {

using namespace extract_detail;

namespace {

Fraction EventContentSpan(const std::vector<Event> &events)
{
    Fraction span(0);
    for (const Event &ev : events) {
        const Fraction relEnd = ev.offset + ev.dur_ql;
        if (relEnd > span) span = relEnd;
    }
    return span;
}

bool IsIgnoredRegularBoundaryBarline(const SymExtra &extra, const Fraction &contentSpan)
{
    return extra.kind == ExtraKind::kBarline && extra.symbolic.has_value()
        && *extra.symbolic == "regular"
        && (extra.offset == Fraction(0) || extra.offset == contentSpan);
}

std::vector<SymExtra> NormalizeMeasureExtras(
    std::vector<SymExtra> extras, const Fraction &contentSpan)
{
    // m21utils.py:877-904: sort by offset, drop consecutive equivalent clefs,
    // then apply the final kind/offset ordering used by the oracle.
    std::erase_if(extras,
        [&](const SymExtra &extra) { return IsIgnoredRegularBoundaryBarline(extra, contentSpan); });
    std::stable_sort(extras.begin(), extras.end(),
        [](const SymExtra &a, const SymExtra &b) { return a.offset < b.offset; });
    std::vector<SymExtra> dedupedExtras;
    std::optional<std::string> mostRecentClef; // by value: vector growth cannot invalidate it
    for (SymExtra &extra : extras) {
        if (extra.kind == ExtraKind::kClef) {
            if (mostRecentClef.has_value() && extra.symbolic == mostRecentClef) {
                continue;
            }
            mostRecentClef = extra.symbolic;
        }
        dedupedExtras.push_back(std::move(extra));
    }
    std::stable_sort(dedupedExtras.begin(), dedupedExtras.end(),
        [](const SymExtra &a, const SymExtra &b) {
            if (a.kind != b.kind) return ExtraKindSortRank(a.kind) < ExtraKindSortRank(b.kind);
            return a.offset < b.offset;
        });
    return dedupedExtras;
}

const vrv::Object *DurationCarrierForBeamMember(const vrv::Object *obj)
{
    if (const vrv::DurationInterface *di = obj->GetDurationInterface()) {
        if (!TypeNameFromDur(di->GetDur()).empty()) return obj;
    }
    if (const vrv::Object *chord = obj->GetFirstAncestor(vrv::CHORD)) return chord;
    return obj;
}

BeamMember BeamMemberFromObject(const vrv::Object *obj)
{
    BeamMember m;
    m.is_rest = obj->Is(vrv::REST) || obj->Is(vrv::MREST) || obj->Is(vrv::MULTIREST)
        || obj->Is(vrv::SPACE) || obj->Is(vrv::MSPACE);
    const vrv::Object *carrier = DurationCarrierForBeamMember(obj);
    if (!m.is_rest) {
        Fraction t;
        if (const vrv::DurationInterface *di = carrier->GetDurationInterface()) {
            t = TypeNumFromName(TypeNameFromDur(di->GetDur()));
        }
        while (t > Fraction(4)) {
            t = t / Fraction(2);
            ++m.n_beams;
        }
    }
    if (const vrv::DurationInterface *di = carrier->GetDurationInterface()) {
        // @breaksec lives on AttBeamSecondary, a DurationInterface base.
        if (di->HasBreaksec()) m.breaksec = di->GetBreaksec();
    }
    return m;
}

std::map<const vrv::Object *, std::size_t> CurrentEventObjects(const std::vector<Event> &events)
{
    std::map<const vrv::Object *, std::size_t> eventByObject;
    for (std::size_t i = 0; i < events.size(); ++i) {
        eventByObject.emplace(events[i].obj, i);
        for (const vrv::Object *child : events[i].obj->GetChildren()) {
            eventByObject.emplace(child, i);
        }
    }
    return eventByObject;
}

struct BeamSpanData {
    std::vector<const vrv::Object *> members;
    std::vector<BeamMember> beamMembers;
    std::vector<std::optional<std::size_t>> localIndexes;
    std::vector<std::vector<BeamValue>> derived;
    int parent = -1;
    std::size_t depthOffset = 0;
};

bool ContainsBeamSpan(const BeamSpanData &parent, const BeamSpanData &child)
{
    if (parent.members.size() <= child.members.size()) return false;
    for (const vrv::Object *member : child.members) {
        if (std::find(parent.members.begin(), parent.members.end(), member)
            == parent.members.end()) {
            return false;
        }
    }
    return true;
}

void EnsureBeamDepth(std::vector<BeamValue> &beamings, std::size_t depth)
{
    while (beamings.size() <= depth) beamings.push_back(BeamValue::kPartial);
}

bool HasBeamAtDepth(const BeamSpanData &span, std::size_t idx, std::size_t depth)
{
    return idx < span.derived.size() && depth < span.derived[idx].size();
}

std::vector<std::pair<std::size_t, std::size_t>> BeamRunsAtDepth(
    const BeamSpanData &span, std::size_t depth)
{
    std::vector<std::pair<std::size_t, std::size_t>> runs;
    std::size_t i = 0;
    while (i < span.derived.size()) {
        if (!HasBeamAtDepth(span, i, depth)) {
            ++i;
            continue;
        }

        const std::size_t first = i;
        std::size_t last = i;
        while (last + 1 < span.derived.size() && HasBeamAtDepth(span, last + 1, depth)
            && span.derived[last][depth] != BeamValue::kStop
            && span.derived[last][depth] != BeamValue::kPartial) {
            ++last;
        }
        runs.emplace_back(first, last);
        i = last + 1;
    }
    return runs;
}

std::vector<std::vector<BeamValue>> BuildRawBeamLists(
    const vrv::Object *beamSpanRoot, const std::vector<Event> &events)
{
    std::vector<std::vector<BeamValue>> rawBeams(events.size());

    std::map<const vrv::Object *, std::vector<std::size_t>> beamMembers;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events[i].beam) beamMembers[events[i].beam].push_back(i);
    }

    for (const auto &[beamObj, members] : beamMembers) {
        (void)beamObj;
        std::vector<BeamMember> bm;
        bm.reserve(members.size());
        for (const std::size_t idx : members) {
            bm.push_back(BeamMemberFromObject(events[idx].obj));
        }
        const std::vector<std::vector<BeamValue>> derived = DeriveBeamTypes(bm);
        for (std::size_t k = 0; k < members.size(); ++k) rawBeams[members[k]] = derived[k];
    }

    const std::map<const vrv::Object *, std::size_t> eventByObject = CurrentEventObjects(events);
    const vrv::ListOfConstObjects beamSpanObjects
        = beamSpanRoot->FindAllDescendantsByType(vrv::BEAMSPAN);
    std::vector<BeamSpanData> beamSpans;
    beamSpans.reserve(beamSpanObjects.size());
    for (const vrv::Object *obj : beamSpanObjects) {
        BeamSpanData data;
        const auto *beamSpan = vrv_cast<const vrv::BeamSpan *>(obj);
        const vrv::ArrayOfObjects &spanMembers = beamSpan->GetBeamedElements();
        data.members.reserve(spanMembers.size());
        data.beamMembers.reserve(spanMembers.size());
        data.localIndexes.reserve(spanMembers.size());
        for (const vrv::Object *member : spanMembers) {
            data.members.push_back(member);
            data.beamMembers.push_back(BeamMemberFromObject(member));
            const auto it = eventByObject.find(member);
            data.localIndexes.push_back(
                it == eventByObject.end() ? std::nullopt : std::optional<std::size_t>(it->second));
        }
        data.derived = DeriveBeamTypes(data.beamMembers);
        beamSpans.push_back(std::move(data));
    }
    std::stable_sort(beamSpans.begin(), beamSpans.end(),
        [](const BeamSpanData &a, const BeamSpanData &b) {
            return a.members.size() > b.members.size();
        });

    for (std::size_t i = 0; i < beamSpans.size(); ++i) {
        int parent = -1;
        for (std::size_t j = 0; j < i; ++j) {
            if (!ContainsBeamSpan(beamSpans[j], beamSpans[i])) continue;
            if (parent < 0
                || beamSpans[j].members.size()
                    < beamSpans[static_cast<std::size_t>(parent)].members.size()) {
                parent = static_cast<int>(j);
            }
        }
        beamSpans[i].parent = parent;
        beamSpans[i].depthOffset
            = parent < 0 ? 0 : beamSpans[static_cast<std::size_t>(parent)].depthOffset + 1;
    }

    std::vector<std::tuple<int, std::size_t, std::size_t, std::size_t>> initializedNestedRuns;
    for (const BeamSpanData &beamSpan : beamSpans) {
        if (beamSpan.parent < 0) {
            for (std::size_t k = 0; k < beamSpan.localIndexes.size(); ++k) {
                if (!beamSpan.localIndexes[k].has_value()) continue;
                const std::size_t idx = *beamSpan.localIndexes[k];
                if (events[idx].beam) continue;
                rawBeams[idx] = beamSpan.derived[k];
            }
            continue;
        }

        const std::size_t depth = beamSpan.depthOffset;
        const BeamSpanData &parent = beamSpans[static_cast<std::size_t>(beamSpan.parent)];
        std::size_t maxLocalDepth = 0;
        for (const auto &derived : beamSpan.derived) {
            maxLocalDepth = std::max(maxLocalDepth, derived.size());
        }

        for (std::size_t localDepth = 0; localDepth < maxLocalDepth; ++localDepth) {
            const std::size_t rawDepth = depth + localDepth;
            if (rawDepth < parent.depthOffset) continue;
            const std::size_t parentLocalDepth = rawDepth - parent.depthOffset;
            for (const auto &[first, last] : BeamRunsAtDepth(parent, parentLocalDepth)) {
                const auto begin = parent.members.begin() + static_cast<std::ptrdiff_t>(first);
                const auto end = parent.members.begin() + static_cast<std::ptrdiff_t>(last + 1);
                bool overlapsChild = false;
                for (std::size_t k = 0; k < beamSpan.members.size(); ++k) {
                    if (beamSpan.beamMembers[k].n_beams <= static_cast<int>(rawDepth)) continue;
                    if (!HasBeamAtDepth(beamSpan, k, localDepth)) continue;
                    if (std::find(begin, end, beamSpan.members[k]) != end) {
                        overlapsChild = true;
                        break;
                    }
                }
                if (!overlapsChild) continue;

                const auto nestedRun = std::make_tuple(beamSpan.parent, rawDepth, first, last);
                if (std::find(initializedNestedRuns.begin(), initializedNestedRuns.end(), nestedRun)
                    != initializedNestedRuns.end()) {
                    continue;
                }
                for (std::size_t k = first; k <= last; ++k) {
                    if (parent.beamMembers[k].n_beams <= static_cast<int>(rawDepth)) continue;
                    if (!parent.localIndexes[k].has_value()) continue;
                    const std::size_t idx = *parent.localIndexes[k];
                    if (events[idx].beam) continue;
                    EnsureBeamDepth(rawBeams[idx], rawDepth);
                    rawBeams[idx][rawDepth] = BeamValue::kPartial;
                }
                initializedNestedRuns.push_back(nestedRun);
            }
        }

        for (std::size_t k = 0; k < beamSpan.localIndexes.size(); ++k) {
            if (!beamSpan.localIndexes[k].has_value()) continue;
            const std::size_t idx = *beamSpan.localIndexes[k];
            if (events[idx].beam) continue;
            for (std::size_t localDepth = 0; localDepth < beamSpan.derived[k].size();
                ++localDepth) {
                const std::size_t rawDepth = depth + localDepth;
                if (beamSpan.beamMembers[k].n_beams <= static_cast<int>(rawDepth)) continue;
                EnsureBeamDepth(rawBeams[idx], rawDepth);
                rawBeams[idx][rawDepth] = beamSpan.derived[k][localDepth];
            }
        }
    }
    return rawBeams;
}

std::vector<RawEvent> BuildRawEvents(
    const std::vector<Event> &events, const std::vector<std::vector<BeamValue>> &rawBeams)
{
    std::map<const vrv::Tuplet *, std::pair<std::size_t, std::size_t>> tupletSpan;
    for (std::size_t i = 0; i < events.size(); ++i) {
        for (const vrv::Tuplet *t : events[i].tuplets) {
            auto it = tupletSpan.find(t);
            if (it == tupletSpan.end()) {
                tupletSpan[t] = { i, i };
            }
            else {
                it->second.second = i;
            }
        }
    }

    std::vector<RawEvent> raw(events.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        raw[i].is_rest = events[i].is_rest;
        raw[i].type_num = events[i].type_num;
        raw[i].raw_beams = rawBeams[i];
        for (const vrv::Tuplet *t : events[i].tuplets) {
            const auto &[first, last] = tupletSpan.at(t);
            std::optional<TupletValue> type;
            if (first == i && last == i) type = TupletValue::kStartStop;
            else if (first == i) type = TupletValue::kStart;
            else if (last == i) type = TupletValue::kStop;
            raw[i].raw_tuplets.push_back(type);
            raw[i].tuplet_nums.push_back(std::to_string(t->GetNum()));
        }
    }
    return raw;
}

std::vector<const vrv::Note *> SortedChordMembers(const vrv::Chord *chord)
{
    std::vector<const vrv::Note *> members;
    for (const vrv::Object *c : chord->GetChildren()) {
        if (c->Is(vrv::NOTE)) members.push_back(vrv_cast<const vrv::Note *>(c));
    }
    std::stable_sort(members.begin(), members.end(), [](const vrv::Note *a, const vrv::Note *b) {
        const int da = a->GetOct() * 7 + DiatonicIndex(StepFromPname(a->GetPname()));
        const int db = b->GetOct() * 7 + DiatonicIndex(StepFromPname(b->GetPname()));
        return da < db;
    });
    return members;
}

void FillSharedNoteFields(SymNote &sn, const Event &ev,
    const std::vector<std::vector<BeamValue>> &beamings,
    const std::vector<std::vector<TupletValue>> &tuplets,
    const std::vector<std::vector<std::string>> &tupletInfo, std::size_t eventIdx,
    bool includeCarrierArticulations)
{
    sn.note_head = NoteHeadFromTypeNum(ev.type_num);
    sn.dots = ev.dots;
    sn.beamings = beamings[eventIdx];
    sn.tuplets = tuplets[eventIdx];
    sn.tuplet_info = tupletInfo[eventIdx];
    if (includeCarrierArticulations) {
        sn.articulations.insert(
            sn.articulations.end(), ev.articulations.begin(), ev.articulations.end());
    }
    std::sort(sn.articulations.begin(), sn.articulations.end());
    sn.grace_type = ev.grace_type;
    sn.grace_slash = ev.grace_slash;
    sn.note_offset = ev.offset;
    sn.note_dur_type = ev.dur_type;
    sn.note_dur_dots = ev.dots;
    sn.note_is_grace = !ev.grace_type.empty();
}

} // namespace

void Extractor::EmitMeasure(const vrv::Measure *measure, const std::string &staffN,
    std::vector<Event> events, std::vector<SymExtra> extras, StaffState &state)
{
    const std::vector<std::vector<BeamValue>> rawBeams = BuildRawBeamLists(&doc_, events);
    const std::vector<RawEvent> raw = BuildRawEvents(events, rawBeams);
    const std::vector<std::vector<BeamValue>> beamings = EnhanceBeamings(raw);
    const std::vector<std::vector<TupletValue>> tuplets = CorrectTuplets(raw);
    const std::vector<std::vector<std::string>> tupletInfo = TupletInfo(raw);

    const int partIdx = staves_.At(staffN).part_idx;
    SymPart &part = output_.score().parts[static_cast<std::size_t>(partIdx)];
    const int visualMeasureIdx = state.measure_idx;
    const std::size_t emittedMeasureIdx = part.bar_list.size();

    SymMeasure symMeasure;
    symMeasure.vrv_id = measure->GetID();
    symMeasure.measure_n = measure->GetN();
    const SymbolLocator measureLocator{ .part_idx = partIdx,
        .staff_n = staffN,
        .measure_idx = visualMeasureIdx,
        .measure_vrv_id = symMeasure.vrv_id,
        .measure_n = symMeasure.measure_n,
        .offset = Fraction(0),
        .occurrence = 0 };
    symMeasure.locator = measureLocator;
    symMeasure.extras = NormalizeMeasureExtras(std::move(extras), EventContentSpan(events));
    std::map<ExtraKind, int> extraOccurrences;
    for (SymExtra &extra : symMeasure.extras) {
        extra.locator = measureLocator;
        extra.locator.offset = extra.offset;
        extra.locator.occurrence = extraOccurrences[extra.kind]++;
    }
    symMeasure.notes = BuildSymNotes(events, beamings, tuplets, tupletInfo, state);
    for (std::size_t i = 0; i < symMeasure.notes.size(); ++i) {
        SymNote &note = symMeasure.notes[i];
        note.locator = measureLocator;
        note.locator.offset = note.note_offset;
        note.locator.occurrence = static_cast<int>(i);
    }

    if (symMeasure.n_of_elements() > 0) {
        part.bar_list.push_back(std::move(symMeasure));
        RegisterEmittedExtras(partIdx, emittedMeasureIdx);
    }
}

std::vector<SymNote> Extractor::BuildSymNotes(const std::vector<Event> &events,
    const std::vector<std::vector<BeamValue>> &beamings,
    const std::vector<std::vector<TupletValue>> &tuplets,
    const std::vector<std::vector<std::string>> &tupletInfo, StaffState &state)
{
    std::vector<SymNote> notes;
    notes.reserve(events.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        const Event &ev = events[i];
        if (ev.obj->Is(vrv::CHORD)) {
            const vrv::Chord *chord = vrv_cast<const vrv::Chord *>(ev.obj);
            const std::vector<const vrv::Note *> members = SortedChordMembers(chord);
            int idx = 0;
            for (const vrv::Note *member : members) {
                SymNote sn = MakeSymNote(ev, member, chord, idx, state);
                FillSharedNoteFields(sn, ev, beamings, tuplets, tupletInfo, i, idx == 0);
                ++idx;
                notes.push_back(std::move(sn));
            }
        }
        else if (ev.is_rest) {
            SymNote sn;
            sn.vrv_id = ev.obj->GetID();
            sn.visual_id = sn.vrv_id;
            sn.pitches.push_back(SymPitch{ "R", "None", false, 0 });
            FillSharedNoteFields(sn, ev, beamings, tuplets, tupletInfo, i, true);
            notes.push_back(std::move(sn));
        }
        else {
            const vrv::Note *note = vrv_cast<const vrv::Note *>(ev.obj);
            SymNote sn = MakeSymNote(ev, note, note, -1, state);
            FillSharedNoteFields(sn, ev, beamings, tuplets, tupletInfo, i, true);
            notes.push_back(std::move(sn));
        }
    }
    return notes;
}

void Extractor::RegisterEmittedExtras(int partIdx, std::size_t measureIdx)
{
    const SymMeasure &measure
        = output_.score().parts[static_cast<std::size_t>(partIdx)].bar_list[measureIdx];
    for (std::size_t extraIdx = 0; extraIdx < measure.extras.size(); ++extraIdx) {
        controls_.RegisterEmittedExtra(
            measure.extras[extraIdx].vrv_id, partIdx, measureIdx, extraIdx);
    }
}

} // namespace verosim
