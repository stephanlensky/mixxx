#include "effects/releasefxsettings.h"

#include "control/controlobject.h"

namespace {

constexpr double kDefaultBeats = 4.0;
constexpr double kDefaultTailSeconds = 6.0;

ConfigKey slotConfigKey(int slotNumber, const QString& name) {
    return ConfigKey(kReleaseFxGroup, QStringLiteral("Slot%1%2").arg(slotNumber).arg(name));
}

ConfigKey slotControlKey(int slotNumber, const QString& name) {
    return ConfigKey(kReleaseFxGroup, QStringLiteral("slot%1_%2").arg(slotNumber).arg(name));
}

} // namespace

ReleaseFxSettings::ReleaseFxSettings(UserSettingsPointer pConfig)
        : m_pConfig(pConfig) {
    for (int slotNumber = 1; slotNumber <= kNumReleaseFxSlots; ++slotNumber) {
        SlotControls controls{
                std::make_unique<ControlObject>(slotControlKey(slotNumber, "mode")),
                std::make_unique<ControlObject>(slotControlKey(slotNumber, "beats")),
                std::make_unique<ControlObject>(slotControlKey(slotNumber, "tail_seconds"))};
        controls.pMode->setReadOnly();
        m_slotControls.push_back(std::move(controls));
        setSlotControls(slotNumber, getSlot(slotNumber));
    }
}

ReleaseFxSettings::~ReleaseFxSettings() = default;

ReleaseFxSettings::Slot ReleaseFxSettings::getDefaultSlot(int slotNumber) {
    switch (slotNumber) {
    case 1:
        return Slot{ReleaseFxMode::Chain,
                QStringLiteral("Echo Out"),
                kNoSuperSweep,
                kDefaultBeats,
                kDefaultTailSeconds};
    case 2:
        return Slot{ReleaseFxMode::Chain,
                QStringLiteral("Reverb Out"),
                kNoSuperSweep,
                kDefaultBeats,
                kDefaultTailSeconds};
    case 3:
        return Slot{ReleaseFxMode::Backspin,
                QString(),
                kNoSuperSweep,
                kDefaultBeats,
                kDefaultTailSeconds};
    default:
        return Slot{ReleaseFxMode::Brake,
                QString(),
                kNoSuperSweep,
                kDefaultBeats,
                kDefaultTailSeconds};
    }
}

ReleaseFxSettings::Slot ReleaseFxSettings::getSlot(int slotNumber) const {
    const Slot defaultSlot = getDefaultSlot(slotNumber);
    return Slot{
            static_cast<ReleaseFxMode>(m_pConfig->getValue(slotConfigKey(slotNumber, "Mode"),
                    static_cast<int>(defaultSlot.mode))),
            m_pConfig->getValue(slotConfigKey(slotNumber, "Preset"), defaultSlot.presetName),
            m_pConfig->getValue(slotConfigKey(slotNumber, "SuperEnd"), defaultSlot.superEnd),
            m_pConfig->getValue(slotConfigKey(slotNumber, "Beats"), defaultSlot.beats),
            m_pConfig->getValue(slotConfigKey(slotNumber, "TailSeconds"),
                    defaultSlot.tailSeconds)};
}

void ReleaseFxSettings::setSlot(int slotNumber, const Slot& slot) {
    m_pConfig->setValue(slotConfigKey(slotNumber, "Mode"), static_cast<int>(slot.mode));
    m_pConfig->setValue(slotConfigKey(slotNumber, "Preset"), slot.presetName);
    m_pConfig->setValue(slotConfigKey(slotNumber, "SuperEnd"), slot.superEnd);
    m_pConfig->setValue(slotConfigKey(slotNumber, "Beats"), slot.beats);
    m_pConfig->setValue(slotConfigKey(slotNumber, "TailSeconds"), slot.tailSeconds);
    setSlotControls(slotNumber, slot);
}

void ReleaseFxSettings::setSlotControls(int slotNumber, const Slot& slot) {
    const SlotControls& controls = m_slotControls[slotNumber - 1];
    controls.pMode->forceSet(static_cast<double>(slot.mode));
    controls.pBeats->set(slot.beats);
    controls.pTailSeconds->set(slot.tailSeconds);
}

void ReleaseFxSettings::resetToDefaults() {
    for (int slotNumber = 1; slotNumber <= kNumReleaseFxSlots; ++slotNumber) {
        setSlot(slotNumber, getDefaultSlot(slotNumber));
    }
}
