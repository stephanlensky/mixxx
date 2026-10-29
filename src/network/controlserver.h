#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QTimer>
#include <memory>
#include <optional>
#include <vector>

#include "audio/frame.h"
#include "control/pollingcontrolproxy.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"

class ControlPushButton;
class QLocalServer;
class QLocalSocket;

namespace mixxx {

class Beats;

/// Streams deck state and accepts commands as newline-delimited JSON over a local socket.
class ControlServer : public QObject {
    Q_OBJECT
  public:
    explicit ControlServer(const QString& socketPath, QObject* pParent = nullptr);
    ~ControlServer() override;

    static QString getSocketPath(const UserSettingsPointer& pConfig);

    /// Fractional beat number at `position`, where beat 0 is the first beat at or after the track start.
    static std::optional<double> getBeatPosition(
            const Beats& beats, audio::FramePos position);

    bool isListening() const;

  private slots:
    void slotEnabledChanged(double value);
    void slotNewConnection();
    void slotTrackChanged(const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack);
    void slotUpdate();

  private:
    struct DeckControls {
        explicit DeckControls(const QString& group);

        const QString group;
        PollingControlProxy play;
        PollingControlProxy bpm;
        PollingControlProxy playPosition;
        PollingControlProxy trackSamples;
        PollingControlProxy trackSampleRate;
        PollingControlProxy volume;
        PollingControlProxy orientation;
    };

    void listen();
    void close();
    void updateDeckControls();
    QJsonObject getDeckJson(const DeckControls& controls) const;
    QJsonObject getDecksMessage() const;
    void send(const QJsonObject& message);
    void sendTo(QLocalSocket* pSocket, const QJsonObject& message);
    void removeSocket(QLocalSocket* pSocket);
    void readCommands(QLocalSocket* pSocket);
    void handleCommand(const QJsonObject& command);

    const QString m_socketPath;
    const std::unique_ptr<ControlPushButton> m_pEnabled;
    QLocalServer* m_pServer;
    QList<QLocalSocket*> m_sockets;
    QTimer m_timer;
    PollingControlProxy m_numDecks;
    PollingControlProxy m_crossfader;
    PollingControlProxy m_outputLatency;
    PollingControlProxy m_mainGain;
    std::vector<std::unique_ptr<DeckControls>> m_decks;
};

} // namespace mixxx
