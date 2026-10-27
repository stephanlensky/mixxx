#pragma once

namespace TrackTime {
enum class DisplayMode {
    ELAPSED,
    REMAINING,
    ELAPSED_AND_REMAINING,
    BEAT_AND_TOTAL_BEATS,
    BEATS_REMAINING,
    BEAT_AND_BEATS_REMAINING,
};

enum class DisplayFormat {
    TRADITIONAL,
    TRADITIONAL_COARSE,
    SECONDS,
    SECONDS_LONG,
    KILO_SECONDS,
    HECTO_SECONDS,
};
} // namespace TrackTime

enum class LoadWhenDeckPlaying {
    Reject,
    Allow,
    AllowButStopDeck
};
