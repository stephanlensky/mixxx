#include "effects/chains/releaseeffectchain.h"

#include "control/controlproxy.h"
#include "effects/effectslot.h"
#include "effects/presets/effectchainpreset.h"
#include "effects/presets/effectchainpresetmanager.h"
#include "effects/releasefxsettings.h"
#include "moc_releaseeffectchain.cpp"

ReleaseEffectChain::ReleaseEffectChain(
        const ChannelHandleAndGroup& handleAndGroup,
        EffectsManager* pEffectsManager,
        EffectsMessengerPointer pEffectsMessenger,
        const ReleaseFxSettings* pReleaseFxSettings)
        : PerGroupEffectChain(
                  handleAndGroup,
                  formatEffectChainGroup(handleAndGroup.name()),
                  SignalProcessingStage::Postfader,
                  pEffectsManager,
                  pEffectsMessenger,
                  true),
          m_handleAndGroup(handleAndGroup),
          m_pReleaseFxSettings(pReleaseFxSettings),
          m_pReleaseFxActive(new ControlProxy(handleAndGroup.name(),
                  QStringLiteral("release_fx_active"),
                  this,
                  ControlFlag::AllowMissingOrInvalid)),
          m_pReleaseFxProgress(new ControlProxy(handleAndGroup.name(),
                  QStringLiteral("release_fx_progress"),
                  this,
                  ControlFlag::AllowMissingOrInvalid)),
          m_sweepingSuperKnob(false),
          m_superStart(0.0),
          m_superEnd(0.0) {
    for (int i = 0; i < kNumEffectsPerUnit; ++i) {
        addEffectSlot(formatEffectSlotGroup(handleAndGroup.name(), i));
        m_effectSlots.at(i)->setEnabled(true);
    }
    m_pReleaseFxActive->connectValueChanged(this,
            &ReleaseEffectChain::slotReleaseFxActiveChanged);
    m_pReleaseFxProgress->connectValueChanged(this,
            &ReleaseEffectChain::slotReleaseFxProgressChanged);
}

QString ReleaseEffectChain::formatEffectChainGroup(const QString& group) {
    return QStringLiteral("[ReleaseEffectRack1_%1]").arg(group);
}

QString ReleaseEffectChain::formatEffectSlotGroup(
        const QString& group, const int iEffectSlotNumber) {
    return QStringLiteral("[ReleaseEffectRack1_%1_Effect%2]")
            .arg(group, QString::number(iEffectSlotNumber + 1));
}

void ReleaseEffectChain::slotReleaseFxActiveChanged(double value) {
    const int slotNumber = static_cast<int>(value);
    if (slotNumber < 1 || slotNumber > kNumReleaseFxSlots) {
        stopRelease();
        return;
    }
    const ReleaseFxSettings::Slot slot = m_pReleaseFxSettings->getSlot(slotNumber);
    if (slot.mode != ReleaseFxMode::Chain) {
        stopRelease();
        return;
    }

    EffectChainPresetPointer pPreset = m_pChainPresetManager->getPreset(slot.presetName);
    if (!pPreset) {
        qWarning() << "Release FX chain preset not found:" << slot.presetName;
        return;
    }
    if (pPreset->name() != m_presetName) {
        loadChainPreset(pPreset);
    }
    m_superStart = pPreset->superKnob();
    m_superEnd = slot.superEnd;
    m_sweepingSuperKnob = slot.superEnd != ReleaseFxSettings::kNoSuperSweep;
    setSuperParameter(m_superStart, true);
    enableForInputChannel(m_handleAndGroup);
}

void ReleaseEffectChain::slotReleaseFxProgressChanged(double value) {
    if (m_sweepingSuperKnob) {
        setSuperParameter(m_superStart + (m_superEnd - m_superStart) * value, true);
    }
}

void ReleaseEffectChain::stopRelease() {
    disableForInputChannel(m_handleAndGroup);
    if (m_sweepingSuperKnob) {
        m_sweepingSuperKnob = false;
        setSuperParameter(m_superStart, true);
    }
}
