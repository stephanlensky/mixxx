#pragma once


#include "track/track_decl.h"
#include "wnumber.h"
#include "preferences/interface.h"

class ControlProxy;

class WNumberPos : public WNumber {
    Q_OBJECT

  public:
    explicit WNumberPos(const QString& group, QWidget* parent = nullptr);

    static QString getDisplayModeName(TrackTime::DisplayMode displayMode);

  protected:
    void mousePressEvent(QMouseEvent* pEvent) override;
    void contextMenuEvent(QContextMenuEvent* pEvent) override;

  private slots:
    void setValue(double dValue) override;
    void slotSetTimeElapsed(double);
    void slotTimeRemainingUpdated(double);
    void slotSetDisplayMode(double);
    void slotSetTimeFormat(double);
    void slotTrackChanged(const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack);
    void slotBeatsUpdated();

  private:
    void setDisplayMode(TrackTime::DisplayMode displayMode);
    QString getBeatsText(double dTimeElapsed, double dTimeRemaining) const;

    const QString m_group;
    TrackPointer m_pTrack;

    TrackTime::DisplayMode m_displayMode;
    TrackTime::DisplayFormat m_displayFormat;

    double m_dOldTimeElapsed;
    ControlProxy* m_pTimeElapsed;
    ControlProxy* m_pTimeRemaining;
    ControlProxy* m_pTrackSamples;
    ControlProxy* m_pShowTrackTimeRemaining;
    ControlProxy* m_pTimeFormat;
};
