#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "verosim/model/fraction.h"

namespace verosim {

// Layer 2 representation: a 1:1 mirror of musicdiff's AnnScore hierarchy at
// the v1 tiers (Voicing always off, so measures hold a flat note list and
// chords are pre-split into per-member notes — annotation.py:1194-1245).
//
// Every field that participates in musicdiff's __str__/__eq__/notation_size
// is materialized here (strings/enums/Fractions); the comparison engine hashes
// and compares SymScore without re-touching the Verovio tree. Verovio is
// referenced only via copied @xml:id strings for triage output.

// musicdiff pitch tuple (m21utils.note2tuple): (step+octave | "R", visible
// accidental name | "None", tied). Accidental names are music21's.
struct SymPitch {
    std::string step_octave; // "C4"; "R" for rests
    std::string accid = "None"; // "sharp", "flat", "natural", "double-sharp", "double-flat"
    bool tie = false;
    // Effective-pitch resolver output (D13): sounding alter in semitones.
    // Not part of any metric comparison or count; recorded for cross-checks
    // (--per-measure triage) and the deferred D4 work.
    int sounding_alter = 0;
};

enum class BeamValue { kStart, kContinue, kStop, kPartial };
enum class TupletValue { kStart, kContinue, kStop, kStartStop };

// Stable visual-only position for resolving rendered SVG elements. These
// fields are metadata: metric equality, counts, and edit costs ignore them.
struct SymbolLocator {
    int part_idx = -1;
    std::string staff_n;
    int measure_idx = -1;
    std::string measure_vrv_id;
    std::string measure_n;
    Fraction offset;
    int occurrence = -1;
};

struct SymNote {
    // Carrier @xml:id used in repr/op output. For chord members this remains the
    // CHORD id, matching the musicdiff-style GeneralNote carrier.
    std::string vrv_id;
    // Concrete rendered note/rest @xml:id for visual annotation. For chord
    // members this is the child note id, with vrv_id available as fallback.
    std::string visual_id;
    bool is_in_chord = false;
    int note_idx_in_chord = -1;
    SymbolLocator locator;

    std::vector<SymPitch> pitches; // exactly 1 (chords are split)
    Fraction note_head; // min(type_num, 4); Fraction: breve head is 1/2
    int dots = 0;
    std::vector<BeamValue> beamings; // post get_enhance_beamings
    std::vector<TupletValue> tuplets; // post get_tuplets_type
    std::vector<std::string> tuplet_info; // parallel to tuplets; "" on non-start
    std::vector<std::string> articulations; // sorted music21 articulation names
    std::vector<std::string> expressions; // stretch surface; normally empty
    std::string grace_type; // "", "acc", "unacc"
    bool grace_slash = false;
    Fraction gap_dur; // always 0 in flat mode (annotation.py:1224,1237)

    // Within-measure matching keys (comparison.py:1140, D14):
    Fraction note_offset; // offset in measure; chord members share the chord's
    std::string note_dur_type; // carrier m21 duration type ("quarter", "16th", "complex")
    int note_dur_dots = 0;
    bool note_is_grace = false;

    int notation_size() const; // AnnNote.notation_size at v1 tiers
    std::string str() const; // == AnnNote.__str__ (comparison content)
    std::string repr() const; // == AnnNote.__repr__ minus the m21 id (triage)
};

// kind strings are musicdiff's; enum order == lexicographic order of the
// kind strings, so sorting by (kind, offset) matches annotation.py:1255.
enum class ExtraKind {
    kBarline,
    kClef,
    kCrescendo,
    kDiminuendo,
    kDynamic,
    kKeySig,
    kRepeat,
    kSlur,
    kTimeSig
};
std::string_view ExtraKindName(ExtraKind kind);
std::string_view ExtraKindEditHeader(ExtraKind kind);
bool ExtraKindHasMetricOffset(ExtraKind kind);
bool ExtraKindHasMetricDuration(ExtraKind kind);
std::size_t ExtraKindSortRank(ExtraKind kind);

struct SymExtra {
    std::string vrv_id;
    SymbolLocator locator;
    ExtraKind kind = ExtraKind::kClef;
    std::optional<std::string> symbolic; // clef: "G2"; dynamic: "p", "ff"; else nullopt
    std::optional<std::string> content; // directions/text; unused by current metric modes
    // insertion-ordered, deterministic (mirrors AnnExtra.infodict)
    std::vector<std::pair<std::string, std::string>> infodict;
    Fraction offset; // offset in measure
    std::optional<Fraction> duration; // set on spanning controls
    bool visual_barline_boundary = false; // visual-only: left/right barline, not inline

    int notation_size() const; // AnnExtra.notation_size (content is never set at v1 tiers)
    std::string str() const; // readable form for triage
};

struct SymMeasure {
    std::string vrv_id;
    std::string measure_n; // @n, triage only
    SymbolLocator locator;
    std::vector<SymNote> notes; // flat; layer order, then event order
    std::vector<SymExtra> extras; // stable-sorted by (kind, offset)

    int n_of_elements() const
    {
        return static_cast<int>(notes.size() + extras.size());
    }
    int notation_size() const;
};

struct SymPart {
    std::string staff_n; // StaffDef @n
    int part_idx = 0;
    std::vector<SymMeasure> bar_list; // measures with n_of_elements()==0 dropped

    int notation_size() const;
};

struct SymScore {
    std::vector<SymPart> parts; // staffDef document order == music21 score.parts order

    int notation_size() const;
};

// Per-category accounting for --count-symbols; Σ categories == notation_size.
// Mirrored by harness/verosim_harness/count_oracle.py CATEGORIES — keep in sync.
struct SymbolCounts {
    long pitches = 0;
    long accidentals = 0;
    long ties = 0;
    long noteheads = 0;
    long dots = 0;
    long beams = 0;
    long tuplets = 0;
    long tuplet_info = 0;
    long grace = 0;
    long grace_slash = 0;
    long gaps = 0;
    long articulations = 0;
    long expressions = 0; // stretch surface; currently always 0
    long style = 0; // currently never set
    long barline = 0; // barlines and repeat-style barlines
    long clef = 0;
    long keysig = 0;
    long timesig = 0;
    long other_extras = 0; // slurs, dynamics, hairpins

    long total() const;
};

SymbolCounts CountSymbols(const SymScore &score);

std::string_view BeamValueName(BeamValue value);
std::string_view TupletValueName(TupletValue value);

} // namespace verosim
