#include "effects/backends/builtin/echoeffect.h"

#include <gtest/gtest.h>

#include "effects/backends/effectmanifest.h"
#include "engine/channelhandle.h"
#include "engine/effects/engineeffectparameter.h"
#include "engine/effects/groupfeaturestate.h"
#include "test/mixxxtest.h"
#include "util/samplebuffer.h"

namespace {

constexpr SINT kFramesPerBuffer = 64;

class EchoEffectTest : public MixxxTest {
  protected:
    EchoEffectTest()
            : m_pManifest(EchoEffect::getManifest()),
              m_engineParameters(mixxx::audio::SampleRate(44100), kFramesPerBuffer),
              m_channel(m_channelHandleFactory.getOrCreateHandle("[Channel1]"),
                      "[Channel1]"),
              m_input(m_engineParameters.samplesPerBuffer()),
              m_output(m_engineParameters.samplesPerBuffer()) {
        for (const auto& pManifestParameter : m_pManifest->parameters()) {
            m_parameters.insert(pManifestParameter->id(),
                    EngineEffectParameterPointer(
                            new EngineEffectParameter(pManifestParameter)));
        }
        m_parameters.value("send_amount")->setValue(0.0);
        m_effect.loadEngineEffectParameters(m_parameters);
        m_effect.initialize({m_channel}, {m_channel}, m_engineParameters);
        m_input.fill(1.0f);
    }

    void processAndAddDry(EffectEnableState enableState) {
        m_effect.process(m_channel.handle(),
                m_channel.handle(),
                m_input.data(),
                m_output.data(),
                m_engineParameters,
                enableState,
                m_groupFeatures);
        m_effect.addDry(m_channel.handle(),
                m_channel.handle(),
                m_input.data(),
                m_output.data(),
                m_engineParameters);
    }

    CSAMPLE firstSample() const {
        return m_output.span().front();
    }

    CSAMPLE lastSample() const {
        return m_output.span().back();
    }

    EffectManifestPointer m_pManifest;
    mixxx::EngineParameters m_engineParameters;
    ChannelHandleFactory m_channelHandleFactory;
    ChannelHandleAndGroup m_channel;
    QMap<QString, EngineEffectParameterPointer> m_parameters;
    EchoEffect m_effect;
    GroupFeatureState m_groupFeatures;
    mixxx::SampleBuffer m_input;
    mixxx::SampleBuffer m_output;
};

TEST_F(EchoEffectTest, DefaultDryLevelPassesDrySignal) {
    processAndAddDry(EffectEnableState::Enabling);
    processAndAddDry(EffectEnableState::Enabled);

    for (const CSAMPLE sample : m_output.span()) {
        EXPECT_FLOAT_EQ(1.0f, sample);
    }
}

TEST_F(EchoEffectTest, DryLevelScalesDrySignal) {
    m_parameters.value("dry_level")->setValue(0.25);

    processAndAddDry(EffectEnableState::Enabling);
    EXPECT_NEAR(1.0f, firstSample(), 0.05f);
    EXPECT_FLOAT_EQ(0.25f, lastSample());

    processAndAddDry(EffectEnableState::Enabled);
    for (const CSAMPLE sample : m_output.span()) {
        EXPECT_FLOAT_EQ(0.25f, sample);
    }
}

TEST_F(EchoEffectTest, DisablingRampsDryLevelBackToUnity) {
    m_parameters.value("dry_level")->setValue(0.0);
    processAndAddDry(EffectEnableState::Enabling);
    processAndAddDry(EffectEnableState::Enabled);
    EXPECT_FLOAT_EQ(0.0f, lastSample());

    processAndAddDry(EffectEnableState::Disabling);
    EXPECT_NEAR(0.0f, firstSample(), 0.05f);
    EXPECT_FLOAT_EQ(1.0f, lastSample());
}

TEST_F(EchoEffectTest, ReleaseFeatureSetsDryLevelFromProgress) {
    m_groupFeatures.release = GroupFeatureRelease{0.5, false};

    processAndAddDry(EffectEnableState::Enabling);
    processAndAddDry(EffectEnableState::Enabled);
    EXPECT_NEAR(std::cos(std::numbers::pi / 4), lastSample(), 1e-5);
}

TEST_F(EchoEffectTest, ReleaseInputCutKeepsInputOutOfTheEcho) {
    m_groupFeatures.release = GroupFeatureRelease{1.0, true};

    processAndAddDry(EffectEnableState::Enabling);
    for (int i = 0; i < 1000; ++i) {
        processAndAddDry(EffectEnableState::Enabled);
        for (const CSAMPLE sample : m_output.span()) {
            ASSERT_NEAR(0.0f, sample, 1e-6);
        }
    }
}

TEST_F(EchoEffectTest, ReleaseFeatureSendsInputIntoTheEcho) {
    m_groupFeatures.release = GroupFeatureRelease{1.0, false};

    processAndAddDry(EffectEnableState::Enabling);
    CSAMPLE peak = 0;
    for (int i = 0; i < 1000; ++i) {
        processAndAddDry(EffectEnableState::Enabled);
        for (const CSAMPLE sample : m_output.span()) {
            peak = std::max(peak, sample);
        }
    }
    EXPECT_GT(peak, 0.5f);
}

} // namespace
