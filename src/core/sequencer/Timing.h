#pragma once
#include "model/Note.h"
#include "model/Project.h"
#include <cmath>

namespace mc::seq {

using model::Tick;
using model::kPPQ;

// Constant-tempo timing (tempo maps can be added later behind these functions).
inline double ticksToSeconds(Tick t, double bpm) { return (double)t / kPPQ * 60.0 / bpm; }
inline double secondsToTicks(double s, double bpm) { return s * bpm / 60.0 * kPPQ; }
inline double ticksPerSample(double bpm, double sampleRate) { return bpm / 60.0 * kPPQ / sampleRate; }

inline Tick ticksPerBeat(const model::TimeSignature& ts) { return kPPQ * 4 / ts.denominator; }
inline Tick ticksPerBar(const model::TimeSignature& ts) { return ticksPerBeat(ts) * ts.numerator; }

struct BarBeat { int bar; int beat; Tick tick; }; // 1-based bar/beat, remaining ticks
inline BarBeat toBarBeat(Tick t, const model::TimeSignature& ts)
{
    const Tick bar = ticksPerBar(ts), beat = ticksPerBeat(ts);
    return { (int)(t / bar) + 1, (int)((t % bar) / beat) + 1, t % beat };
}

// Grid divisions expressed as note value denominators (4 = quarter, 8 = eighth...).
// Triplet/dotted variants can be added by extending GridValue.
struct GridValue {
    int denominator = 16;
    bool triplet = false;
    bool dotted = false;
    Tick ticks() const
    {
        Tick t = kPPQ * 4 / denominator;
        if (triplet) t = t * 2 / 3;
        if (dotted) t = t * 3 / 2;
        return t;
    }
};

inline Tick snapFloor(Tick t, Tick grid) { return grid <= 0 ? t : (t / grid) * grid; }
inline Tick snapNearest(Tick t, Tick grid) { return grid <= 0 ? t : ((t + grid / 2) / grid) * grid; }

} // namespace mc::seq
