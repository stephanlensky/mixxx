#include "widget/wnumberpos.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <array>

#include "control/controlproxy.h"
#include "mixer/playerinfo.h"
#include "moc_wnumberpos.cpp"
#include "track/track.h"
#include "util/duration.h"

namespace {
constexpr std::array kTimeDisplayModes = {
        TrackTime::DisplayMode::ELAPSED,
        TrackTime::DisplayMode::REMAINING,
        TrackTime::DisplayMode::ELAPSED_AND_REMAINING,
};
constexpr std::array kBeatDisplayModes = {
        TrackTime::DisplayMode::BEAT_AND_TOTAL_BEATS,
        TrackTime::DisplayMode::BEATS_REMAINING,
        TrackTime::DisplayMode::BEAT_AND_BEATS_REMAINING,
};
constexpr int kDisplayModeCount =
        static_cast<int>(TrackTime::DisplayMode::BEAT_AND_BEATS_REMAINING) + 1;
} // namespace

WNumberPos::WNumberPos(const QString& group, QWidget* parent)
        : WNumber(parent),
          m_group(group),
          m_pTrack(PlayerInfo::instance().getTrackInfo(group)),
          m_displayFormat(TrackTime::DisplayFormat::TRADITIONAL),
          m_dOldTimeElapsed(0.0) {
    m_pTimeElapsed = new ControlProxy(group, "time_elapsed", this, ControlFlag::NoAssertIfMissing);
    m_pTimeElapsed->connectValueChanged(this, &WNumberPos::slotSetTimeElapsed);
    m_pTimeRemaining = new ControlProxy(
            group, "time_remaining", this, ControlFlag::NoAssertIfMissing);
    m_pTimeRemaining->connectValueChanged(
            this, &WNumberPos::slotTimeRemainingUpdated);
    m_pTrackSamples = new ControlProxy(
            group, "track_samples", this, ControlFlag::NoAssertIfMissing);

    if (m_pTrack) {
        connect(m_pTrack.get(), &Track::beatsUpdated, this, &WNumberPos::slotBeatsUpdated);
    }
    connect(&PlayerInfo::instance(),
            &PlayerInfo::trackChanged,
            this,
            &WNumberPos::slotTrackChanged);

    m_pShowTrackTimeRemaining = new ControlProxy(
            "[Controls]", "ShowDurationRemaining", this);
    m_pShowTrackTimeRemaining->connectValueChanged(
            this, &WNumberPos::slotSetDisplayMode);
    slotSetDisplayMode(m_pShowTrackTimeRemaining->get());

    m_pTimeFormat = new ControlProxy(
            "[Controls]", "TimeFormat", this);
    m_pTimeFormat->connectValueChanged(
            this, &WNumberPos::slotSetTimeFormat);
    slotSetTimeFormat(m_pTimeFormat->get());
}

QString WNumberPos::getDisplayModeName(TrackTime::DisplayMode displayMode) {
    switch (displayMode) {
    case TrackTime::DisplayMode::ELAPSED:
        return tr("Elapsed");
    case TrackTime::DisplayMode::REMAINING:
        return tr("Remaining");
    case TrackTime::DisplayMode::ELAPSED_AND_REMAINING:
        return tr("Elapsed and Remaining");
    case TrackTime::DisplayMode::BEAT_AND_TOTAL_BEATS:
        return tr("Beat and Total Beats");
    case TrackTime::DisplayMode::BEATS_REMAINING:
        return tr("Beats Remaining");
    case TrackTime::DisplayMode::BEAT_AND_BEATS_REMAINING:
        return tr("Beat and Beats Remaining");
    }
    return QString();
}

void WNumberPos::mousePressEvent(QMouseEvent* pEvent) {
    if (pEvent->button() == Qt::LeftButton) {
        setDisplayMode(static_cast<TrackTime::DisplayMode>(
                (static_cast<int>(m_displayMode) + 1) % kDisplayModeCount));
    }
}

void WNumberPos::contextMenuEvent(QContextMenuEvent* pEvent) {
    QMenu menu(this);
    QActionGroup actionGroup(&menu);
    const auto addDisplayModeActions = [&](const auto& displayModes) {
        for (const auto displayMode : displayModes) {
            QAction* pAction = menu.addAction(getDisplayModeName(displayMode));
            pAction->setCheckable(true);
            pAction->setChecked(displayMode == m_displayMode);
            actionGroup.addAction(pAction);
            connect(pAction, &QAction::triggered, this, [this, displayMode] {
                setDisplayMode(displayMode);
            });
        }
    };
    addDisplayModeActions(kTimeDisplayModes);
    menu.addSeparator();
    addDisplayModeActions(kBeatDisplayModes);
    menu.exec(pEvent->globalPos());
}

void WNumberPos::setDisplayMode(TrackTime::DisplayMode displayMode) {
    m_displayMode = displayMode;
    m_pShowTrackTimeRemaining->set(static_cast<double>(m_displayMode));
    slotSetTimeElapsed(m_dOldTimeElapsed);
}

// Reimplementing WNumber::setValue
void WNumberPos::setValue(double dValue) {
    // Ignore midi-scaled signals from the skin connection.
    Q_UNUSED(dValue);
    // Update our value with the old value.
    slotSetTimeElapsed(m_dOldTimeElapsed);
}

void WNumberPos::slotSetTimeElapsed(double dTimeElapsed) {
    double dTimeRemaining = m_pTimeRemaining->get();
    QString (*timeFormat)(double dSeconds, mixxx::Duration::Precision precision);

    if (m_displayFormat == TrackTime::DisplayFormat::KILO_SECONDS) {
        timeFormat = &mixxx::Duration::formatKiloSeconds;
    } else if (m_displayFormat == TrackTime::DisplayFormat::SECONDS_LONG) {
        timeFormat = &mixxx::Duration::formatSecondsLong;
    } else if (m_displayFormat == TrackTime::DisplayFormat::SECONDS) {
       timeFormat = &mixxx::Duration::formatSeconds;
    } else {
        timeFormat = &mixxx::Duration::formatTime;
    }

    mixxx::Duration::Precision precision;
    if (m_displayFormat != TrackTime::DisplayFormat::TRADITIONAL_COARSE) {
        precision = mixxx::Duration::Precision::CENTISECONDS;
    } else {
        precision = mixxx::Duration::Precision::SECONDS;
    }

    if (m_displayMode == TrackTime::DisplayMode::ELAPSED) {
        if (dTimeElapsed >= 0.0) {
            setText(timeFormat(dTimeElapsed, precision));
        } else {
            setText(QLatin1String("-") % timeFormat(-dTimeElapsed, precision));
        }
    } else if (m_displayMode == TrackTime::DisplayMode::REMAINING) {
        setText(QLatin1String("-") % timeFormat(dTimeRemaining, precision));
    } else if (m_displayMode == TrackTime::DisplayMode::ELAPSED_AND_REMAINING) {
        if (dTimeElapsed >= 0.0) {
            setText(timeFormat(dTimeElapsed, precision)
                    % QLatin1String("  -") % timeFormat(dTimeRemaining, precision));
        } else {
            setText(QLatin1String("-") % timeFormat(-dTimeElapsed, precision)
                    % QLatin1String("  -") % timeFormat(dTimeRemaining, precision));
        }
    } else {
        setText(getBeatsText(dTimeElapsed, dTimeRemaining));
    }
    m_dOldTimeElapsed = dTimeElapsed;
}

// m_pTimeElapsed is not updated when paused at the beginning of a track,
// but m_pTimeRemaining is updated in that case. So, call slotSetTimeElapsed to
// update this widget's text.
void WNumberPos::slotTimeRemainingUpdated(double dTimeRemaining) {
    Q_UNUSED(dTimeRemaining);
    double dTimeElapsed = m_pTimeElapsed->get();
    if (dTimeElapsed == 0.0) {
        slotSetTimeElapsed(dTimeElapsed);
    }
}

QString WNumberPos::getBeatsText(double dTimeElapsed, double dTimeRemaining) const {
    const double trackSeconds = dTimeElapsed + dTimeRemaining;
    const mixxx::BeatsPointer pBeats = m_pTrack ? m_pTrack->getBeats() : nullptr;
    if (!pBeats || trackSeconds <= 0.0) {
        return QStringLiteral("--");
    }
    const double trackSamples = m_pTrackSamples->get();
    const auto position = mixxx::audio::FramePos::fromEngineSamplePos(
            dTimeElapsed / trackSeconds * trackSamples);
    const auto trackEnd = mixxx::audio::FramePos::fromEngineSamplePos(trackSamples);
    const auto firstBeat = pBeats->iteratorFrom(mixxx::audio::kStartFramePos);
    const auto nextBeat = pBeats->iteratorFrom(position);
    const int totalBeats = pBeats->iteratorFrom(trackEnd) - firstBeat;
    const int beat = (nextBeat - firstBeat) + (*nextBeat == position ? 1 : 0);
    const QString beatsRemaining = QLatin1String("-") % QString::number(totalBeats - beat);

    if (m_displayMode == TrackTime::DisplayMode::BEAT_AND_TOTAL_BEATS) {
        return QString::number(beat) % QLatin1String(" / ") % QString::number(totalBeats);
    } else if (m_displayMode == TrackTime::DisplayMode::BEATS_REMAINING) {
        return beatsRemaining;
    }
    return QString::number(beat) % QLatin1String("  ") % beatsRemaining;
}

void WNumberPos::slotTrackChanged(
        const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack) {
    Q_UNUSED(pOldTrack);
    if (group != m_group) {
        return;
    }
    if (m_pTrack) {
        disconnect(m_pTrack.get(), nullptr, this, nullptr);
    }
    m_pTrack = pNewTrack;
    if (m_pTrack) {
        connect(m_pTrack.get(), &Track::beatsUpdated, this, &WNumberPos::slotBeatsUpdated);
    }
    slotSetTimeElapsed(m_dOldTimeElapsed);
}

void WNumberPos::slotBeatsUpdated() {
    slotSetTimeElapsed(m_dOldTimeElapsed);
}

void WNumberPos::slotSetDisplayMode(double value) {
    const int displayMode = static_cast<int>(value);
    if (displayMode >= 0 && displayMode < kDisplayModeCount) {
        m_displayMode = static_cast<TrackTime::DisplayMode>(displayMode);
    } else {
        m_displayMode = TrackTime::DisplayMode::ELAPSED;
    }

    slotSetTimeElapsed(m_dOldTimeElapsed);
}
void WNumberPos::slotSetTimeFormat(double v) {
    m_displayFormat = static_cast<TrackTime::DisplayFormat>(static_cast<int>(v));

    slotSetTimeElapsed(m_dOldTimeElapsed);
}
