#include "engine/controls/releasefxcontrol.h"

#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "engine/engine.h"
#include "moc_releasefxcontrol.cpp"
#include "track/track.h"
#include "util/math.h"

namespace {

constexpr double kBackspinStartSpeed = -10.0;
constexpr double kFallbackBeatSeconds = 0.5;

} // namespace

ReleaseFxControl::ReleaseFxControl(const QString& group, UserSettingsPointer pConfig)
        : EngineControl(group, pConfig),
          m_pActive(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("release_fx_active")))),
          m_pProgress(std::make_unique<ControlObject>(
                  ConfigKey(group, QStringLiteral("release_fx_progress")))),
          m_play(group, QStringLiteral("play")),
          m_requestedSlot(0),
          m_resetRequested(false),
          m_slotNumber(0),
          m_mode(ReleaseFxMode::Chain),
          m_phase(Phase::Idle),
          m_progress(0.0),
          m_inputCut(false),
          m_lastSpeed(0.0),
          m_startSpeed(0.0),
          m_bufferSeconds(0.0),
          m_elapsedSeconds(0.0),
          m_durationSeconds(0.0),
          m_tailRemainingSeconds(0.0) {
    m_pActive->setReadOnly();
    m_pProgress->setReadOnly();

    for (int slotNumber = 1; slotNumber <= kNumReleaseFxSlots; ++slotNumber) {
        auto pButton = std::make_unique<ControlPushButton>(
                ConfigKey(group, QStringLiteral("release_fx_%1").arg(slotNumber)));
        connect(
                pButton.get(),
                &ControlObject::valueChanged,
                this,
                [this, slotNumber](double value) {
                    if (value > 0) {
                        m_requestedSlot.store(slotNumber);
                    }
                },
                Qt::DirectConnection);
        m_slotButtons.push_back(std::move(pButton));
        m_slotModes.emplace_back(kReleaseFxGroup,
                QStringLiteral("slot%1_mode").arg(slotNumber),
                ControlFlag::AllowMissingOrInvalid);
        m_slotBeats.emplace_back(kReleaseFxGroup,
                QStringLiteral("slot%1_beats").arg(slotNumber),
                ControlFlag::AllowMissingOrInvalid);
        m_slotTailSeconds.emplace_back(kReleaseFxGroup,
                QStringLiteral("slot%1_tail_seconds").arg(slotNumber),
                ControlFlag::AllowMissingOrInvalid);
    }
}

ReleaseFxControl::~ReleaseFxControl() = default;

void ReleaseFxControl::trackLoaded(TrackPointer pNewTrack) {
    m_pTrackBeats = pNewTrack ? pNewTrack->getBeats() : mixxx::BeatsPointer();
    m_resetRequested.store(true);
}

void ReleaseFxControl::trackBeatsUpdated(mixxx::BeatsPointer pBeats) {
    m_pTrackBeats = pBeats;
}

bool ReleaseFxControl::isSpeedMode() const {
    return m_mode == ReleaseFxMode::Backspin || m_mode == ReleaseFxMode::Brake;
}

double ReleaseFxControl::getSpeed(double speed,
        std::size_t outputBufferSize,
        mixxx::audio::SampleRate sampleRate,
        bool* pIsScratching) {
    if (sampleRate.isValid()) {
        m_bufferSeconds = static_cast<double>(outputBufferSize) /
                mixxx::kEngineChannelOutputCount / sampleRate.toDouble();
    }
    if (m_phase != Phase::Running || !isSpeedMode()) {
        m_lastSpeed = speed;
        return speed;
    }
    m_elapsedSeconds += m_bufferSeconds;
    const double remaining = 1.0 - math_min(1.0, m_elapsedSeconds / m_durationSeconds);
    *pIsScratching = true;
    if (m_mode == ReleaseFxMode::Brake) {
        return m_startSpeed * remaining;
    }
    return m_startSpeed * remaining * remaining;
}

void ReleaseFxControl::process(const double rate,
        mixxx::audio::FramePos currentPosition,
        const std::size_t bufferSize) {
    Q_UNUSED(bufferSize);
    if (m_resetRequested.exchange(false)) {
        stop();
    }

    const int requestedSlot = m_requestedSlot.exchange(0);
    if (requestedSlot != 0) {
        if (m_phase == Phase::Idle) {
            start(requestedSlot, rate, currentPosition);
        } else {
            stop();
        }
    }

    switch (m_phase) {
    case Phase::Idle:
        return;
    case Phase::Running:
        if (!isSpeedMode()) {
            processChainRelease(currentPosition);
            return;
        }
        if (!m_play.toBool()) {
            stop();
            return;
        }
        m_progress = math_min(1.0, m_elapsedSeconds / m_durationSeconds);
        m_pProgress->forceSet(m_progress);
        if (m_progress >= 1.0) {
            m_play.set(0.0);
            stop();
        }
        return;
    case Phase::Tail:
        m_tailRemainingSeconds -= m_bufferSeconds;
        if (m_play.toBool() || m_tailRemainingSeconds <= 0) {
            stop();
        }
        return;
    }
}

void ReleaseFxControl::start(int slotNumber,
        double rate,
        mixxx::audio::FramePos position) {
    const mixxx::audio::SampleRate sampleRate = frameInfo().sampleRate;
    const double beats = m_slotBeats[slotNumber - 1].get();
    const auto mode = static_cast<ReleaseFxMode>(
            static_cast<int>(m_slotModes[slotNumber - 1].get()));
    if (!m_play.toBool() || rate <= 0 || !sampleRate.isValid() || beats <= 0) {
        return;
    }

    mixxx::audio::FramePos endPosition;
    const mixxx::BeatsPointer pTrackBeats = m_pTrackBeats;
    if (pTrackBeats) {
        const mixxx::audio::FramePos closestBeat = pTrackBeats->findClosestBeat(position);
        if (closestBeat.isValid()) {
            endPosition = pTrackBeats->findNBeatsFromPosition(closestBeat, beats);
        }
    }
    if (!endPosition.isValid() || endPosition <= position) {
        endPosition = position + beats * kFallbackBeatSeconds * sampleRate.toDouble();
    }

    if (mode == ReleaseFxMode::Backspin || mode == ReleaseFxMode::Brake) {
        if (m_lastSpeed <= 0) {
            return;
        }
        m_durationSeconds = (endPosition - position) / sampleRate.toDouble() / m_lastSpeed;
        m_elapsedSeconds = 0.0;
        m_startSpeed = mode == ReleaseFxMode::Brake ? m_lastSpeed : kBackspinStartSpeed;
    }

    m_slotNumber = slotNumber;
    m_mode = mode;
    m_phase = Phase::Running;
    m_startPosition = position;
    m_endPosition = endPosition;
    m_previousPosition = position;
    m_progress = 0.0;
    m_inputCut = false;
    m_pActive->forceSet(slotNumber);
    m_pProgress->forceSet(0.0);
}

void ReleaseFxControl::processChainRelease(mixxx::audio::FramePos position) {
    m_progress = math_clamp(
            (position - m_startPosition) / (m_endPosition - m_startPosition),
            0.0,
            1.0);
    const mixxx::audio::FrameDiff_t advance = position - m_previousPosition;
    m_previousPosition = position;

    if (position + advance >= m_endPosition || !m_play.toBool()) {
        m_progress = 1.0;
        m_inputCut = true;
        m_phase = Phase::Tail;
        m_tailRemainingSeconds = m_slotTailSeconds[m_slotNumber - 1].get();
        m_play.set(0.0);
    }
    m_pProgress->forceSet(m_progress);
}

void ReleaseFxControl::stop() {
    if (m_phase == Phase::Idle) {
        return;
    }
    m_phase = Phase::Idle;
    m_progress = 0.0;
    m_inputCut = false;
    m_pActive->forceSet(0.0);
    m_pProgress->forceSet(0.0);
}

void ReleaseFxControl::collectFeatureState(GroupFeatureState* pGroupFeatures) const {
    if (m_phase != Phase::Idle && m_mode == ReleaseFxMode::Chain) {
        pGroupFeatures->release = GroupFeatureRelease{m_progress, m_inputCut};
    }
}
