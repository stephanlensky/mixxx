#include "network/deckstateserver.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "moc_deckstateserver.cpp"
#include "track/beats.h"
#include "track/track.h"
#include "util/logger.h"

namespace mixxx {

namespace {

const Logger kLogger("DeckStateServer");

const QString kConfigGroup = QStringLiteral("[DeckStateServer]");
const ConfigKey kEnabledKey = ConfigKey(kConfigGroup, QStringLiteral("Enabled"));
const ConfigKey kSocketPathKey = ConfigKey(kConfigGroup, QStringLiteral("SocketPath"));
const QString kSocketFileName = QStringLiteral("mixxx-deck-state.sock");

const QString kAppGroup = QStringLiteral("[App]");
const QString kMainGroup = QStringLiteral("[Master]");

constexpr int kUpdateIntervalMillis = 50;
constexpr qint64 kMaxPendingBytes = 1 << 20;

QJsonValue getTrackJson(const TrackPointer& pTrack) {
    if (!pTrack) {
        return QJsonValue::Null;
    }
    return QJsonObject{
            {QStringLiteral("id"), QJsonValue::fromVariant(pTrack->getId().toVariant())},
            {QStringLiteral("artist"), pTrack->getArtist()},
            {QStringLiteral("title"), pTrack->getTitle()},
            {QStringLiteral("album"), pTrack->getAlbum()},
            {QStringLiteral("location"), pTrack->getLocation()},
            {QStringLiteral("duration"), pTrack->getDuration()},
            {QStringLiteral("bpm"), pTrack->getBpm()},
    };
}

QJsonObject getTrackMessage(const QString& group, const TrackPointer& pTrack) {
    return QJsonObject{
            {QStringLiteral("type"), QStringLiteral("track")},
            {QStringLiteral("group"), group},
            {QStringLiteral("track"), getTrackJson(pTrack)},
    };
}

QString getOrientationName(double orientation) {
    switch (static_cast<int>(orientation)) {
    case 0:
        return QStringLiteral("left");
    case 2:
        return QStringLiteral("right");
    default:
        return QStringLiteral("center");
    }
}

} // namespace

DeckStateServer::DeckControls::DeckControls(const QString& group)
        : group(group),
          play(group, QStringLiteral("play")),
          bpm(group, QStringLiteral("bpm")),
          playPosition(group, QStringLiteral("playposition")),
          trackSamples(group, QStringLiteral("track_samples")),
          trackSampleRate(group, QStringLiteral("track_samplerate")),
          volume(group, QStringLiteral("volume")),
          orientation(group, QStringLiteral("orientation")) {
}

DeckStateServer::DeckStateServer(const QString& socketPath, QObject* pParent)
        : QObject(pParent),
          m_pServer(new QLocalServer(this)),
          m_numDecks(kAppGroup, QStringLiteral("num_decks")),
          m_crossfader(kMainGroup, QStringLiteral("crossfader")),
          m_outputLatency(kAppGroup, QStringLiteral("output_latency_ms")) {
    m_timer.setInterval(kUpdateIntervalMillis);
    connect(&m_timer, &QTimer::timeout, this, &DeckStateServer::slotUpdate);
    connect(m_pServer, &QLocalServer::newConnection, this, &DeckStateServer::slotNewConnection);
    connect(&PlayerInfo::instance(),
            &PlayerInfo::trackChanged,
            this,
            &DeckStateServer::slotTrackChanged);

    QLocalServer::removeServer(socketPath);
    if (m_pServer->listen(socketPath)) {
        kLogger.info() << "Listening on" << socketPath;
    } else {
        kLogger.warning() << "Failed to listen on" << socketPath << m_pServer->errorString();
    }
}

DeckStateServer::~DeckStateServer() {
    m_pServer->close();
}

bool DeckStateServer::isEnabled(const UserSettingsPointer& pConfig) {
    return pConfig->getValue(kEnabledKey, false);
}

QString DeckStateServer::getSocketPath(const UserSettingsPointer& pConfig) {
    const QString socketPath = pConfig->getValueString(kSocketPathKey);
    if (!socketPath.isEmpty()) {
        return socketPath;
    }
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
            QChar('/') + kSocketFileName;
}

std::optional<double> DeckStateServer::getBeatPosition(
        const Beats& beats, audio::FramePos position) {
    if (!position.isValid()) {
        return std::nullopt;
    }
    const auto firstBeat = beats.iteratorFrom(audio::kStartFramePos);
    const auto nextBeat = beats.iteratorFrom(position);
    const auto nextBeatNumber = static_cast<double>(nextBeat - firstBeat);
    if (*nextBeat == position) {
        return nextBeatNumber;
    }
    const auto prevBeat = std::prev(nextBeat);
    return nextBeatNumber - 1 + (position - *prevBeat) / (*nextBeat - *prevBeat);
}

bool DeckStateServer::isListening() const {
    return m_pServer->isListening();
}

void DeckStateServer::slotNewConnection() {
    updateDeckControls();
    while (QLocalSocket* pSocket = m_pServer->nextPendingConnection()) {
        connect(pSocket, &QLocalSocket::disconnected, this, [this, pSocket]() {
            removeSocket(pSocket);
        });
        connect(pSocket, &QLocalSocket::readyRead, pSocket, [pSocket]() {
            pSocket->readAll();
        });
        m_sockets.append(pSocket);
        for (const auto& pDeck : m_decks) {
            sendTo(pSocket,
                    getTrackMessage(pDeck->group,
                            PlayerInfo::instance().getTrackInfo(pDeck->group)));
        }
        sendTo(pSocket, getDecksMessage());
    }
    if (!m_sockets.isEmpty()) {
        m_timer.start();
    }
}

void DeckStateServer::slotTrackChanged(
        const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack) {
    Q_UNUSED(pOldTrack);
    if (m_sockets.isEmpty() || !PlayerManager::isDeckGroup(group)) {
        return;
    }
    send(getTrackMessage(group, pNewTrack));
}

void DeckStateServer::slotUpdate() {
    updateDeckControls();
    send(getDecksMessage());
}

void DeckStateServer::updateDeckControls() {
    const auto numDecks = static_cast<std::size_t>(std::max(0.0, m_numDecks.get()));
    while (m_decks.size() < numDecks) {
        m_decks.push_back(std::make_unique<DeckControls>(
                PlayerManager::groupForDeck(static_cast<int>(m_decks.size()))));
    }
    m_decks.resize(numDecks);
}

QJsonObject DeckStateServer::getDeckJson(const DeckControls& controls) const {
    const auto position = audio::FramePos::fromEngineSamplePos(
            controls.playPosition.get() * controls.trackSamples.get());
    const double sampleRate = controls.trackSampleRate.get();

    QJsonValue beat = QJsonValue::Null;
    const TrackPointer pTrack = PlayerInfo::instance().getTrackInfo(controls.group);
    if (pTrack) {
        const BeatsPointer pBeats = pTrack->getBeats();
        if (pBeats) {
            const auto beatPosition = getBeatPosition(*pBeats, position);
            if (beatPosition) {
                beat = *beatPosition;
            }
        }
    }

    return QJsonObject{
            {QStringLiteral("group"), controls.group},
            {QStringLiteral("play"), controls.play.toBool()},
            {QStringLiteral("bpm"), controls.bpm.get()},
            {QStringLiteral("beat"), beat},
            {QStringLiteral("position"), sampleRate > 0 ? position.value() / sampleRate : 0.0},
            {QStringLiteral("volume"), controls.volume.get()},
            {QStringLiteral("orientation"), getOrientationName(controls.orientation.get())},
    };
}

QJsonObject DeckStateServer::getDecksMessage() const {
    QJsonArray decks;
    for (const auto& pDeck : m_decks) {
        decks.append(getDeckJson(*pDeck));
    }
    return QJsonObject{
            {QStringLiteral("type"), QStringLiteral("decks")},
            {QStringLiteral("time"), QDateTime::currentMSecsSinceEpoch()},
            {QStringLiteral("outputLatency"), m_outputLatency.get()},
            {QStringLiteral("crossfader"), m_crossfader.get()},
            {QStringLiteral("decks"), decks},
    };
}

void DeckStateServer::send(const QJsonObject& message) {
    const auto sockets = m_sockets;
    for (QLocalSocket* pSocket : sockets) {
        sendTo(pSocket, message);
    }
}

void DeckStateServer::sendTo(QLocalSocket* pSocket, const QJsonObject& message) {
    if (pSocket->bytesToWrite() > kMaxPendingBytes) {
        kLogger.warning() << "Disconnecting client that stopped reading";
        pSocket->abort();
        return;
    }
    pSocket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void DeckStateServer::removeSocket(QLocalSocket* pSocket) {
    m_sockets.removeOne(pSocket);
    pSocket->deleteLater();
    if (m_sockets.isEmpty()) {
        m_timer.stop();
    }
}

} // namespace mixxx
