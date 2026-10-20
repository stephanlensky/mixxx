#pragma once

#include "effects/chains/pergroupeffectchain.h"

class ControlProxy;
class ReleaseFxSettings;

/// ReleaseEffectChain plays the effect chain of a deck's Release FX. It loads
/// the chain preset of the active slot and is only enabled while a release is
/// active. Its effects receive the release progress through GroupFeatureState.
class ReleaseEffectChain : public PerGroupEffectChain {
    Q_OBJECT
  public:
    ReleaseEffectChain(
            const ChannelHandleAndGroup& handleAndGroup,
            EffectsManager* pEffectsManager,
            EffectsMessengerPointer pEffectsMessenger,
            const ReleaseFxSettings* pReleaseFxSettings);

    static QString formatEffectChainGroup(const QString& group);
    static QString formatEffectSlotGroup(const QString& group,
            const int iEffectSlotNumber = 0);

  private slots:
    void slotReleaseFxActiveChanged(double value);
    void slotReleaseFxProgressChanged(double value);

  private:
    void stopRelease();

    const ChannelHandleAndGroup m_handleAndGroup;
    const ReleaseFxSettings* m_pReleaseFxSettings;
    ControlProxy* m_pReleaseFxActive;
    ControlProxy* m_pReleaseFxProgress;
    bool m_sweepingSuperKnob;
    double m_superStart;
    double m_superEnd;
};
