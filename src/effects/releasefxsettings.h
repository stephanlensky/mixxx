#pragma once

#include <QString>
#include <memory>
#include <vector>

#include "engine/controls/releasefxcontrol.h"
#include "preferences/usersettings.h"

class ControlObject;

/// ReleaseFxSettings stores what each Release FX slot does and publishes the
/// settings the engine needs as [ReleaseFx] controls.
class ReleaseFxSettings {
  public:
    struct Slot {
        ReleaseFxMode mode;
        QString presetName;
        /// Super knob value the chain sweeps to, or kNoSuperSweep.
        double superEnd;
        double beats;
        double tailSeconds;
    };

    static constexpr double kNoSuperSweep = -1.0;

    explicit ReleaseFxSettings(UserSettingsPointer pConfig);
    ~ReleaseFxSettings();

    Slot getSlot(int slotNumber) const;
    void setSlot(int slotNumber, const Slot& slot);

    void resetToDefaults();

  private:
    struct SlotControls {
        std::unique_ptr<ControlObject> pMode;
        std::unique_ptr<ControlObject> pBeats;
        std::unique_ptr<ControlObject> pTailSeconds;
    };

    static Slot getDefaultSlot(int slotNumber);
    void setSlotControls(int slotNumber, const Slot& slot);

    UserSettingsPointer m_pConfig;
    std::vector<SlotControls> m_slotControls;
};
