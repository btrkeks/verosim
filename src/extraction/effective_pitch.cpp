#include "verosim/extraction/effective_pitch.h"

namespace verosim {

namespace {

// circle-of-fifths order: sharps F C G D A E B, flats B E A D G C F
constexpr char kSharpOrder[] = { 'f', 'c', 'g', 'd', 'a', 'e', 'b' };
constexpr char kFlatOrder[] = { 'b', 'e', 'a', 'd', 'g', 'c', 'f' };

} // namespace

void AccidentalState::StartMeasure()
{
    measure_state_.clear();
    // tie_carry_ deliberately survives the barline: an entry lives until its
    // tie target consumes it (Resolve below). Entries from ties whose target
    // never resolves linger, harmlessly — they are only consulted by notes
    // that ARE tie targets, and a genuine re-registration overwrites them.
}

int AccidentalState::KeyAlter(char step) const
{
    if (key_sig_ > 0) {
        for (int i = 0; i < key_sig_ && i < 7; ++i) {
            if (kSharpOrder[i] == step) return 1;
        }
    }
    else if (key_sig_ < 0) {
        for (int i = 0; i < -key_sig_ && i < 7; ++i) {
            if (kFlatOrder[i] == step) return -1;
        }
    }
    return 0;
}

int AccidentalState::Resolve(char step, int octave, const int *written_alter,
    const int *gestural_alter, bool tied_from_previous)
{
    const std::pair<char, int> key{ step, octave };

    int alter;
    if (written_alter) {
        alter = *written_alter;
    }
    else if (gestural_alter) {
        alter = *gestural_alter;
    }
    else if (tied_from_previous && tie_carry_.count(key)) {
        // only the tie target consumes the entry: an untied same-pitch note
        // in another layer must not clear a carry that is still pending
        alter = tie_carry_.at(key);
        tie_carry_.erase(key);
    }
    else if (measure_state_.count(key)) {
        alter = measure_state_.at(key); // within-measure carry
    }
    else {
        alter = KeyAlter(step);
    }

    measure_state_[key] = alter;
    return alter;
}

} // namespace verosim
