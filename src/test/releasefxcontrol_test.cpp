#include "engine/controls/releasefxcontrol.h"

#include <gtest/gtest.h>

#include "control/controlobject.h"
#include "test/signalpathtest.h"
#include "track/beats.h"

namespace {

constexpr double kBpm = 120.0;
constexpr double kBeats = 4.0;

class ReleaseFxControlTest : public SignalPathTest {
  protected:
    void SetUp() override {
        SignalPathTest::SetUp();
        m_pTrack = m_pMixerDeck1->getLoadedTrack();
        m_pBeats = mixxx::Beats::fromConstTempo(m_pTrack->getSampleRate(),
                mixxx::audio::kStartFramePos,
                mixxx::Bpm(kBpm));
        m_pTrack->trySetBeats(m_pBeats);
        ProcessBuffer();
    }

    void set(const QString& item, double value) {
        ControlObject::set(ConfigKey(m_sGroup1, item), value);
    }

    double get(const QString& item) const {
        return ControlObject::get(ConfigKey(m_sGroup1, item));
    }

    void press(const QString& item) {
        set(item, 1.0);
        set(item, 0.0);
    }

    mixxx::audio::FramePos playPos() const {
        return m_pChannel1->getEngineBuffer()->getPlayPos();
    }

    void playBuffers(int count) {
        set(QStringLiteral("play"), 1.0);
        for (int i = 0; i < count; ++i) {
            ProcessBuffer();
        }
    }

    int processUntilStopped() {
        int buffers = 0;
        while (get(QStringLiteral("play")) > 0 && buffers < 2000) {
            ProcessBuffer();
            ++buffers;
        }
        return buffers;
    }

    mixxx::audio::FramePos expectedEndPosition(mixxx::audio::FramePos triggerPosition) const {
        return m_pBeats->findNBeatsFromPosition(
                m_pBeats->findClosestBeat(triggerPosition), kBeats);
    }

    double framesPerBuffer() const {
        return static_cast<double>(kProcessBufferSize) / mixxx::kEngineChannelOutputCount;
    }

    TrackPointer m_pTrack;
    mixxx::BeatsPointer m_pBeats;
};

TEST_F(ReleaseFxControlTest, ChainReleaseStopsTheDeckJustBeforeTheTargetBeat) {
    playBuffers(30);
    const mixxx::audio::FramePos endPosition = expectedEndPosition(playPos());

    press(QStringLiteral("release_fx_1"));
    processUntilStopped();

    EXPECT_EQ(1.0, get(QStringLiteral("release_fx_active")));
    EXPECT_DOUBLE_EQ(1.0, get(QStringLiteral("release_fx_progress")));
    EXPECT_LE(playPos(), endPosition);
    EXPECT_GT(playPos(), endPosition - 2 * framesPerBuffer());
}

TEST_F(ReleaseFxControlTest, ChainReleaseTailEndsAfterTailSeconds) {
    ControlObject::set(ConfigKey(kReleaseFxGroup, QStringLiteral("slot1_tail_seconds")), 0.1);
    playBuffers(30);
    press(QStringLiteral("release_fx_1"));
    processUntilStopped();
    ASSERT_EQ(1.0, get(QStringLiteral("release_fx_active")));

    for (int i = 0; i < 20; ++i) {
        ProcessBuffer();
    }
    EXPECT_EQ(0.0, get(QStringLiteral("release_fx_active")));
}

TEST_F(ReleaseFxControlTest, PlayDuringTailCancelsTheRelease) {
    playBuffers(30);
    press(QStringLiteral("release_fx_2"));
    processUntilStopped();
    ASSERT_EQ(2.0, get(QStringLiteral("release_fx_active")));

    playBuffers(1);
    EXPECT_EQ(0.0, get(QStringLiteral("release_fx_active")));
}

TEST_F(ReleaseFxControlTest, SecondPressCancelsTheRelease) {
    playBuffers(30);
    press(QStringLiteral("release_fx_1"));
    ProcessBuffer();
    ASSERT_EQ(1.0, get(QStringLiteral("release_fx_active")));

    press(QStringLiteral("release_fx_4"));
    ProcessBuffer();
    EXPECT_EQ(0.0, get(QStringLiteral("release_fx_active")));
    EXPECT_EQ(1.0, get(QStringLiteral("play")));
}

TEST_F(ReleaseFxControlTest, PausedDeckDoesNotStartARelease) {
    press(QStringLiteral("release_fx_1"));
    ProcessBuffer();
    EXPECT_EQ(0.0, get(QStringLiteral("release_fx_active")));
}

TEST_F(ReleaseFxControlTest, BrakeStopsTheDeckWhenTheTargetBeatWouldHavePlayed) {
    playBuffers(30);
    const mixxx::audio::FramePos endPosition = expectedEndPosition(playPos());

    press(QStringLiteral("release_fx_4"));
    ProcessBuffer();
    const mixxx::audio::FramePos startPosition = playPos();
    ASSERT_EQ(4.0, get(QStringLiteral("release_fx_active")));
    const int buffers = processUntilStopped();

    const double expectedBuffers = (endPosition - startPosition) / framesPerBuffer();
    EXPECT_NEAR(expectedBuffers, buffers, 2);
    EXPECT_EQ(0.0, get(QStringLiteral("release_fx_active")));
    EXPECT_LT(playPos(), endPosition);
}

TEST_F(ReleaseFxControlTest, BackspinPlaysBackwardsThenStops) {
    playBuffers(300);
    const mixxx::audio::FramePos triggerPosition = playPos();

    press(QStringLiteral("release_fx_3"));
    processUntilStopped();

    EXPECT_LT(playPos(), triggerPosition);
    EXPECT_EQ(0.0, get(QStringLiteral("release_fx_active")));
}

} // namespace
