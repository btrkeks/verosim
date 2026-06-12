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
    // tie_carry_ survives exactly one barline: anything not consumed by the
    // first note of the new measure is dropped when that note resolves (we
    // keep it simple and let entries persist until overwritten — a tie
    // target consumes its entry below).
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
        alter = tie_carry_.at(key);
    }
    else if (measure_state_.count(key)) {
        alter = measure_state_.at(key); // within-measure carry
    }
    else {
        alter = KeyAlter(step);
    }

    measure_state_[key] = alter;
    tie_carry_.erase(key);
    return alter;
}

} // namespace verosim
