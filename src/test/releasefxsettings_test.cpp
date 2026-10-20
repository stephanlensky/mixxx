#include "effects/releasefxsettings.h"

#include <gtest/gtest.h>

#include "control/controlobject.h"
#include "test/mixxxtest.h"

namespace {

class ReleaseFxSettingsTest : public MixxxTest {
  protected:
    double getSlotControl(int slotNumber, const QString& name) const {
        return ControlObject::get(ConfigKey(
                kReleaseFxGroup, QStringLiteral("slot%1_%2").arg(slotNumber).arg(name)));
    }
};

TEST_F(ReleaseFxSettingsTest, DefaultSlots) {
    ReleaseFxSettings settings(config());

    EXPECT_EQ(ReleaseFxMode::Chain, settings.getSlot(1).mode);
    EXPECT_EQ(QStringLiteral("Echo Out"), settings.getSlot(1).presetName);
    EXPECT_EQ(ReleaseFxSettings::kNoSuperSweep, settings.getSlot(1).superEnd);
    EXPECT_EQ(QStringLiteral("Reverb Out"), settings.getSlot(2).presetName);
    EXPECT_EQ(ReleaseFxMode::Backspin, settings.getSlot(3).mode);
    EXPECT_EQ(ReleaseFxMode::Brake, settings.getSlot(4).mode);
    EXPECT_EQ(4.0, settings.getSlot(4).beats);
    EXPECT_EQ(static_cast<double>(ReleaseFxMode::Brake), getSlotControl(4, QStringLiteral("mode")));
}

TEST_F(ReleaseFxSettingsTest, SetSlotUpdatesConfigAndControls) {
    ReleaseFxSettings settings(config());

    settings.setSlot(1,
            ReleaseFxSettings::Slot{
                    ReleaseFxMode::Chain, QStringLiteral("Filter"), 1.0, 8.0, 2.5});
    settings.setSlot(4,
            ReleaseFxSettings::Slot{ReleaseFxMode::Backspin, QString(), -1.0, 2.0, 0.0});

    const ReleaseFxSettings::Slot slot = settings.getSlot(1);
    EXPECT_EQ(QStringLiteral("Filter"), slot.presetName);
    EXPECT_EQ(1.0, slot.superEnd);
    EXPECT_EQ(8.0, slot.beats);
    EXPECT_EQ(2.5, slot.tailSeconds);
    EXPECT_EQ(8.0, getSlotControl(1, QStringLiteral("beats")));
    EXPECT_EQ(2.5, getSlotControl(1, QStringLiteral("tail_seconds")));
    EXPECT_EQ(ReleaseFxMode::Backspin, settings.getSlot(4).mode);
    EXPECT_EQ(static_cast<double>(ReleaseFxMode::Backspin), getSlotControl(4, QStringLiteral("mode")));
}

} // namespace
