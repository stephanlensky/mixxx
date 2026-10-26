#include "network/deckstateserver.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTemporaryDir>

#include "control/controlobject.h"
#include "mixer/playerinfo.h"
#include "test/mixxxtest.h"
#include "track/beats.h"
#include "track/track.h"

using namespace mixxx;

namespace {

constexpr auto kBpm = Bpm(120.0);
constexpr auto kSampleRate = audio::SampleRate(48000);
constexpr auto kFirstBeat = audio::FramePos(400);
constexpr double kBeatLength = 24000;
const QString kGroup = QStringLiteral("[Channel1]");

TEST(DeckStateServerBeatTest, ConstTempo) {
    const auto beats = Beats(kFirstBeat, kBpm, kSampleRate, QString());
    EXPECT_DOUBLE_EQ(0.0, *DeckStateServer::getBeatPosition(beats, kFirstBeat));
    EXPECT_DOUBLE_EQ(0.5,
            *DeckStateServer::getBeatPosition(beats, kFirstBeat + kBeatLength / 2));
    EXPECT_DOUBLE_EQ(3.0,
            *DeckStateServer::getBeatPosition(beats, kFirstBeat + 3 * kBeatLength));
    EXPECT_DOUBLE_EQ(1.0 - 400 / kBeatLength,
            *DeckStateServer::getBeatPosition(beats, kFirstBeat + kBeatLength - 400));
}

TEST(DeckStateServerBeatTest, CountsFromFirstBeatAfterTrackStart) {
    const auto beats = Beats(kFirstBeat + 10 * kBeatLength, kBpm, kSampleRate, QString());
    EXPECT_DOUBLE_EQ(0.0, *DeckStateServer::getBeatPosition(beats, kFirstBeat));
    EXPECT_DOUBLE_EQ(10.25,
            *DeckStateServer::getBeatPosition(beats, kFirstBeat + 10.25 * kBeatLength));
}

TEST(DeckStateServerBeatTest, NonConstTempo) {
    const auto beats = Beats(
            std::vector<BeatMarker>{BeatMarker{kFirstBeat, 8}},
            kFirstBeat + 8 * kBeatLength,
            Bpm(60.0),
            kSampleRate,
            QString());
    EXPECT_DOUBLE_EQ(7.5,
            *DeckStateServer::getBeatPosition(beats, kFirstBeat + 7.5 * kBeatLength));
    EXPECT_DOUBLE_EQ(8.5,
            *DeckStateServer::getBeatPosition(
                    beats, kFirstBeat + 8 * kBeatLength + kBeatLength));
}

TEST(DeckStateServerBeatTest, InvalidPosition) {
    const auto beats = Beats(kFirstBeat, kBpm, kSampleRate, QString());
    EXPECT_FALSE(DeckStateServer::getBeatPosition(beats, audio::kInvalidFramePos));
}

class DeckStateServerTest : public MixxxTest {
  protected:
    void SetUp() override {
        addControl(QStringLiteral("[App]"), QStringLiteral("num_decks"), 1);
        addControl(QStringLiteral("[App]"), QStringLiteral("output_latency_ms"), 12);
        addControl(QStringLiteral("[Master]"), QStringLiteral("crossfader"), -0.5);
        addControl(kGroup, QStringLiteral("play"), 1);
        addControl(kGroup, QStringLiteral("bpm"), kBpm.value());
        addControl(kGroup, QStringLiteral("playposition"), 0);
        addControl(kGroup, QStringLiteral("track_samples"), 0);
        addControl(kGroup, QStringLiteral("track_samplerate"), kSampleRate.value());
        addControl(kGroup, QStringLiteral("volume"), 1);
        addControl(kGroup, QStringLiteral("pregain"), 1);
        addControl(kGroup, QStringLiteral("orientation"), 0);
        PlayerInfo::create();

        ASSERT_TRUE(m_socketDir.isValid());
        m_pServer = std::make_unique<DeckStateServer>(getSocketPath());
        EXPECT_FALSE(m_pServer->isListening());
        setEnabled(true);
        ASSERT_TRUE(m_pServer->isListening());
    }

    void TearDown() override {
        m_pServer.reset();
        PlayerInfo::destroy();
        m_controls.clear();
    }

    void addControl(const QString& group, const QString& item, double value) {
        m_controls.push_back(std::make_unique<ControlObject>(ConfigKey(group, item)));
        m_controls.back()->set(value);
    }

    void setEnabled(bool enabled) {
        ControlObject::set(ConfigKey(QStringLiteral("[DeckStateServer]"), QStringLiteral("enabled")),
                enabled ? 1.0 : 0.0);
    }

    QString getSocketPath() const {
        return m_socketDir.filePath(QStringLiteral("deck-state.sock"));
    }

    TrackPointer loadTrack() {
        TrackPointer pTrack = Track::newTemporary();
        pTrack->setArtist(QStringLiteral("Artist"));
        pTrack->setTitle(QStringLiteral("Title"));
        pTrack->setAudioProperties(audio::ChannelCount(2),
                kSampleRate,
                audio::Bitrate(),
                Duration::fromSeconds(180));
        pTrack->trySetBeats(Beats::fromConstTempo(kSampleRate, kFirstBeat, kBpm));
        ControlObject::set(ConfigKey(kGroup, QStringLiteral("track_samples")),
                180 * kSampleRate.value() * 2);
        PlayerInfo::instance().setTrackInfo(kGroup, pTrack);
        return pTrack;
    }

    QJsonObject readMessage(QLocalSocket* pSocket) {
        QElapsedTimer timer;
        timer.start();
        while (!pSocket->canReadLine() && timer.elapsed() < 2000) {
            QCoreApplication::processEvents();
            pSocket->waitForReadyRead(10);
        }
        EXPECT_TRUE(pSocket->canReadLine());
        return QJsonDocument::fromJson(pSocket->readLine()).object();
    }

    QJsonObject readMessage(QLocalSocket* pSocket, const QString& type) {
        for (int i = 0; i < 10; ++i) {
            const QJsonObject message = readMessage(pSocket);
            if (message.value(QStringLiteral("type")).toString() == type) {
                return message;
            }
        }
        ADD_FAILURE() << "No message of type" << type.toStdString();
        return QJsonObject();
    }

    std::vector<std::unique_ptr<ControlObject>> m_controls;
    QTemporaryDir m_socketDir;
    std::unique_ptr<DeckStateServer> m_pServer;
};

TEST_F(DeckStateServerTest, SendsSnapshotOnConnect) {
    loadTrack();
    ControlObject::set(ConfigKey(kGroup, QStringLiteral("playposition")),
            (kFirstBeat.value() + 2.5 * kBeatLength) / (180 * kSampleRate.value()));

    QLocalSocket socket;
    socket.connectToServer(getSocketPath());
    ASSERT_TRUE(socket.waitForConnected(1000));

    const QJsonObject track = readMessage(&socket);
    EXPECT_EQ(QStringLiteral("track"), track.value(QStringLiteral("type")).toString());
    EXPECT_EQ(kGroup, track.value(QStringLiteral("group")).toString());
    EXPECT_EQ(QStringLiteral("Title"),
            track.value(QStringLiteral("track"))[QStringLiteral("title")].toString());

    const QJsonObject decks = readMessage(&socket);
    EXPECT_EQ(QStringLiteral("decks"), decks.value(QStringLiteral("type")).toString());
    EXPECT_DOUBLE_EQ(12, decks.value(QStringLiteral("outputLatency")).toDouble());
    EXPECT_DOUBLE_EQ(-0.5, decks.value(QStringLiteral("crossfader")).toDouble());
    const QJsonObject deck = decks.value(QStringLiteral("decks")).toArray().at(0).toObject();
    EXPECT_EQ(kGroup, deck.value(QStringLiteral("group")).toString());
    EXPECT_TRUE(deck.value(QStringLiteral("play")).toBool());
    EXPECT_DOUBLE_EQ(kBpm.value(), deck.value(QStringLiteral("bpm")).toDouble());
    EXPECT_NEAR(2.5, deck.value(QStringLiteral("beat")).toDouble(), 1e-6);
    EXPECT_NEAR((kFirstBeat.value() + 2.5 * kBeatLength) / kSampleRate.value(),
            deck.value(QStringLiteral("position")).toDouble(),
            1e-6);
    EXPECT_EQ(QStringLiteral("left"), deck.value(QStringLiteral("orientation")).toString());
}

TEST_F(DeckStateServerTest, SendsTrackChanges) {
    QLocalSocket socket;
    socket.connectToServer(getSocketPath());
    ASSERT_TRUE(socket.waitForConnected(1000));

    const QJsonObject emptyTrack = readMessage(&socket, QStringLiteral("track"));
    EXPECT_TRUE(emptyTrack.value(QStringLiteral("track")).isNull());
    const QJsonObject emptyDecks = readMessage(&socket, QStringLiteral("decks"));
    EXPECT_TRUE(emptyDecks.value(QStringLiteral("decks"))
                    .toArray()
                    .at(0)[QStringLiteral("beat")]
                    .isNull());

    loadTrack();
    const QJsonObject track = readMessage(&socket, QStringLiteral("track"));
    EXPECT_EQ(QStringLiteral("Artist"),
            track.value(QStringLiteral("track"))[QStringLiteral("artist")].toString());

    PlayerInfo::instance().setTrackInfo(kGroup, TrackPointer());
    const QJsonObject ejected = readMessage(&socket, QStringLiteral("track"));
    EXPECT_TRUE(ejected.value(QStringLiteral("track")).isNull());
}

TEST_F(DeckStateServerTest, DisablingDisconnectsClients) {
    QLocalSocket socket;
    socket.connectToServer(getSocketPath());
    ASSERT_TRUE(socket.waitForConnected(1000));
    readMessage(&socket, QStringLiteral("decks"));

    setEnabled(false);
    EXPECT_FALSE(m_pServer->isListening());
    EXPECT_TRUE(socket.state() == QLocalSocket::UnconnectedState ||
            socket.waitForDisconnected(1000));

    QLocalSocket rejected;
    rejected.connectToServer(getSocketPath());
    EXPECT_FALSE(rejected.waitForConnected(100));

    setEnabled(true);
    QLocalSocket reconnected;
    reconnected.connectToServer(getSocketPath());
    ASSERT_TRUE(reconnected.waitForConnected(1000));
    readMessage(&reconnected, QStringLiteral("decks"));
}

} // namespace
