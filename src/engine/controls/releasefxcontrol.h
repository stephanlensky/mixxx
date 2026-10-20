#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include "control/pollingcontrolproxy.h"
#include "engine/controls/enginecontrol.h"
#include "engine/effects/groupfeaturestate.h"

class ControlObject;
class ControlPushButton;

enum class ReleaseFxMode {
    Chain = 0,
    Backspin = 1,
    Brake = 2,
};

constexpr int kNumReleaseFxSlots = 4;
inline const QString kReleaseFxGroup = QStringLiteral("[ReleaseFx]");

/// ReleaseFxControl makes a playing track leave with the effect chain, Backspin
/// or Brake configured for a Release FX slot. The release ends on a beat of the
/// beatgrid, then the deck stops.
class ReleaseFxControl : public EngineControl {
    Q_OBJECT
  public:
    ReleaseFxControl(const QString& group, UserSettingsPointer pConfig);
    ~ReleaseFxControl() override;

    void process(const double rate,
            mixxx::audio::FramePos currentPosition,
            const std::size_t bufferSize) override;
    void collectFeatureState(GroupFeatureState* pGroupFeatures) const override;
    void trackLoaded(TrackPointer pNewTrack) override;
    void trackBeatsUpdated(mixxx::BeatsPointer pBeats) override;

    /// Called before each buffer is scaled. Overrides the playback speed while a
    /// Backspin or Brake is active.
    double getSpeed(double speed,
            std::size_t outputBufferSize,
            mixxx::audio::SampleRate sampleRate,
            bool* pIsScratching);

  private:
    enum class Phase {
        Idle,
        Running,
        Tail,
    };

    void start(int slotNumber, double rate, mixxx::audio::FramePos position);
    void processChainRelease(mixxx::audio::FramePos position);
    void stop();
    bool isSpeedMode() const;

    std::vector<std::unique_ptr<ControlPushButton>> m_slotButtons;
    std::unique_ptr<ControlObject> m_pActive;
    std::unique_ptr<ControlObject> m_pProgress;
    std::vector<PollingControlProxy> m_slotModes;
    std::vector<PollingControlProxy> m_slotBeats;
    std::vector<PollingControlProxy> m_slotTailSeconds;
    PollingControlProxy m_play;

    std::atomic<int> m_requestedSlot;
    std::atomic<bool> m_resetRequested;

    // m_pTrackBeats is written from an engine worker thread
    mixxx::BeatsPointer m_pTrackBeats;

    int m_slotNumber;
    ReleaseFxMode m_mode;
    Phase m_phase;
    mixxx::audio::FramePos m_startPosition;
    mixxx::audio::FramePos m_endPosition;
    mixxx::audio::FramePos m_previousPosition;
    double m_progress;
    bool m_inputCut;
    double m_lastSpeed;
    double m_startSpeed;
    double m_bufferSeconds;
    double m_elapsedSeconds;
    double m_durationSeconds;
    double m_tailRemainingSeconds;
};
