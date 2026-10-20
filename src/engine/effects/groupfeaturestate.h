#pragma once

#include <cmath>
#include <numbers>
#include <optional>

#include "util/types.h"

struct GroupFeatureBeatLength {
    // Beat length adjusted by the rate slider
    double seconds;
    // Rate change by temporary actions like scratching
    // and not the rate slider.
    double scratch_rate;
};

/// State of a running Release FX, only passed to effects of a release effect chain.
struct GroupFeatureRelease {
    double progress;
    bool inputCut;

    CSAMPLE_GAIN sendGain(double peakLevel) const {
        if (inputCut) {
            return 0;
        }
        return static_cast<CSAMPLE_GAIN>(
                peakLevel * std::sin(progress * std::numbers::pi / 2));
    }

    CSAMPLE_GAIN dryGain() const {
        return static_cast<CSAMPLE_GAIN>(std::cos(progress * std::numbers::pi / 2));
    }
};

/// GroupFeatureState communicates metadata about EngineChannels to EffectProcessors.
struct GroupFeatureState {
    GroupFeatureState() = default;

    std::optional<GroupFeatureBeatLength> beat_length;

    // Beat fraction (0.0 to 1.0) of the position at the buffer end.
    // Previous beat is in the current or earlier buffer. The next beat
    // in the next or later buffer.
    std::optional<double> beat_fraction_buffer_end;

    std::optional<double> gain;

    std::optional<GroupFeatureRelease> release;
};
