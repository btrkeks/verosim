#include "extract_internal.h"

#include <string>

namespace verosim {

using namespace extract_detail;

namespace extract_detail {

char StepFromPname(vrv::data_PITCHNAME pname)
{
    switch (pname) {
        case vrv::PITCHNAME_c: return 'c';
        case vrv::PITCHNAME_d: return 'd';
        case vrv::PITCHNAME_e: return 'e';
        case vrv::PITCHNAME_f: return 'f';
        case vrv::PITCHNAME_g: return 'g';
        case vrv::PITCHNAME_a: return 'a';
        case vrv::PITCHNAME_b: return 'b';
        default: return 0;
    }
}

int DiatonicIndex(char step)
{
    static const std::string kOrder = "cdefgab";
    return static_cast<int>(kOrder.find(step));
}

bool AccidWrittenInfo(vrv::data_ACCIDENTAL_WRITTEN accid, std::string &name, int &alter)
{
    switch (accid) {
        case vrv::ACCIDENTAL_WRITTEN_s: name = "sharp"; alter = 1; return true;
        case vrv::ACCIDENTAL_WRITTEN_f: name = "flat"; alter = -1; return true;
        case vrv::ACCIDENTAL_WRITTEN_n: name = "natural"; alter = 0; return true;
        case vrv::ACCIDENTAL_WRITTEN_ss:
        case vrv::ACCIDENTAL_WRITTEN_x: name = "double-sharp"; alter = 2; return true;
        case vrv::ACCIDENTAL_WRITTEN_ff: name = "double-flat"; alter = -2; return true;
        case vrv::ACCIDENTAL_WRITTEN_xs:
        case vrv::ACCIDENTAL_WRITTEN_sx:
        case vrv::ACCIDENTAL_WRITTEN_ts: name = "triple-sharp"; alter = 3; return true;
        case vrv::ACCIDENTAL_WRITTEN_tf: name = "triple-flat"; alter = -3; return true;
        default: return false;
    }
}

bool AccidGesturalAlter(vrv::data_ACCIDENTAL_GESTURAL accid, int &alter)
{
    switch (accid) {
        case vrv::ACCIDENTAL_GESTURAL_s: alter = 1; return true;
        case vrv::ACCIDENTAL_GESTURAL_f: alter = -1; return true;
        case vrv::ACCIDENTAL_GESTURAL_n: alter = 0; return true;
        case vrv::ACCIDENTAL_GESTURAL_ss: alter = 2; return true;
        case vrv::ACCIDENTAL_GESTURAL_ff: alter = -2; return true;
        case vrv::ACCIDENTAL_GESTURAL_ts: alter = 3; return true;
        case vrv::ACCIDENTAL_GESTURAL_tf: alter = -3; return true;
        default: return false;
    }
}

} // namespace extract_detail

SymNote Extractor::MakeSymNote(const Event &ev, const vrv::Note *note,
    const vrv::Object *carrier, int idxInChord, StaffState &state)
{
    SymNote sn;
    sn.vrv_id = carrier->GetID();
    sn.visual_id = note->GetID();
    sn.is_in_chord = idxInChord >= 0;
    sn.note_idx_in_chord = idxInChord;
    if (DetailIncludesTierB(options_.detail) && note != carrier) {
        AppendArticulations(note, sn.articulations);
    }

    SymPitch pitch;
    const char step = StepFromPname(note->GetPname());
    if (step == 0) {
        Warn("pitched note without @pname (unpitched?); using C4");
        pitch.step_octave = "C4";
    }
    else {
        pitch.step_octave = static_cast<char>(step - 'a' + 'A') + std::to_string(note->GetOct());
    }

    // visible accidental: the @accid VALUE on the accid child (iohumdrum
    // attaches empty accid children to every pitched note — D13)
    const int *writtenAlter = nullptr;
    const int *gesturalAlter = nullptr;
    int writtenValue = 0;
    int gesturalValue = 0;
    for (const vrv::Object *c : note->GetChildren()) {
        if (!c->Is(vrv::ACCID)) continue;
        const vrv::Accid *accid = vrv_cast<const vrv::Accid *>(c);
        if (accid->HasAccid()) {
            std::string name;
            if (AccidWrittenInfo(accid->GetAccid(), name, writtenValue)) {
                pitch.accid = name;
                writtenAlter = &writtenValue;
            }
            else {
                Warn("unmapped @accid value on " + note->GetID());
            }
        }
        if (accid->HasAccidGes()) {
            if (AccidGesturalAlter(accid->GetAccidGes(), gesturalValue)) {
                gesturalAlter = &gesturalValue;
            }
        }
        break;
    }

    // tie: control element (@endid/@startid, possibly registered by the
    // PREVIOUS measure's <tie>) or @tie attribute (MEI input; neither
    // importer emits it). erase() consumes the one-shot control-element refs.
    bool tied = tie_end_ids_.erase(note->GetID()) > 0;
    if (note->HasTie()
        && (note->GetTie() == vrv::TIE_m || note->GetTie() == vrv::TIE_t)) {
        tied = true;
    }
    bool tieStart = tie_start_ids_.erase(note->GetID()) > 0;
    if (note->HasTie()
        && (note->GetTie() == vrv::TIE_i || note->GetTie() == vrv::TIE_m)) {
        tieStart = true;
    }

    if (step != 0) {
        pitch.sounding_alter
            = state.accids.Resolve(step, note->GetOct(), writtenAlter, gesturalAlter, tied);
        pitch.tie = DetailIncludesTierB(options_.detail) && tieStart;
        if (tieStart) {
            state.accids.RegisterTieStart(step, note->GetOct(), pitch.sounding_alter);
        }
    }

    sn.pitches.push_back(std::move(pitch));
    return sn;
}

} // namespace verosim
