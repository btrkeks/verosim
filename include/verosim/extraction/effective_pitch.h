#pragma once

#include <map>
#include <string>
#include <utility>

namespace verosim {

// Effective (sounding) pitch resolution, per D13: neither Verovio importer
// guarantees a sounding alter on every note (@accid.ges appears only on some
// paths), so Layer 2 resolves it with the standard CMN state machine:
// key signature + within-measure accidental carry (per step+octave, per
// staff) + tie propagation across barlines.
//
// The VISIBLE accidental that musicdiff compares is taken from @accid
// directly (it mirrors music21 displayStatus on both importers; pinned by
// HAND-10 and the mutation corpus). The resolver's outputs are the sounding
// alter (recorded on SymPitch, not compared by the metric) and a cross-check
// signal used by --count-symbols --per-measure triage.
class AccidentalState {
public:
    // sig: key signature as signed count (+sharps / -flats), e.g. +2 = D major.
    void SetKeySig(int sig) { key_sig_ = sig; }
    int key_sig() const { return key_sig_; }

    // Called at every barline.
    void StartMeasure();

    // step: 'c'..'b' (lowercase), octave: scientific octave number.
    // written_alter: the alter of an explicit @accid on this note, or
    // nullopt when the note carries no visible accidental.
    // gestural_alter: @accid.ges when present.
    // tied_from_previous: note is the target of a tie.
    // Returns the resolved sounding alter and updates the carry state.
    int Resolve(char step, int octave, const int *written_alter, const int *gestural_alter,
        bool tied_from_previous);

    // Register a tie that starts on (step, octave) with the given sounding
    // alter, so the tied-to note across the barline resolves to it.
    void RegisterTieStart(char step, int octave, int alter)
    {
        tie_carry_[{ step, octave }] = alter;
    }

private:
    int KeyAlter(char step) const;

    int key_sig_ = 0;
    // (step, octave) -> alter carried for the rest of the measure
    std::map<std::pair<char, int>, int> measure_state_;
    // (step, octave) -> alter carried across a barline by a tie
    std::map<std::pair<char, int>, int> tie_carry_;
};

} // namespace verosim
