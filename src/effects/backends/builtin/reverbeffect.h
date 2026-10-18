// Ported from CAPS Reverb.
// This effect is GPL code.

#pragma once

#include <Reverb.h>

#include <QMap>

#include "effects/backends/effectprocessor.h"
#include "util/class.h"
#include "util/types.h"

class ReverbGroupState : public EffectState {
  public:
    ReverbGroupState(const mixxx::EngineParameters& engineParameters)
            : EffectState(engineParameters),
              sampleRate(engineParameters.sampleRate()),
              sendPrevious(0),
              dryPrevious(1),
              dryCurrent(1) {
        reverb.init(sampleRate);
    }
    ~ReverbGroupState() override = default;

    float sampleRate;
    float sendPrevious;
    float dryPrevious;
    float dryCurrent;
    MixxxPlateX2 reverb;
};

class ReverbEffect : public EffectProcessorImpl<ReverbGroupState> {
  public:
    ReverbEffect() = default;
    ~ReverbEffect() override = default;

    static QString getId();
    static EffectManifestPointer getManifest();

    void loadEngineEffectParameters(
            const QMap<QString, EngineEffectParameterPointer>& parameters) override;

    void processChannel(
            ReverbGroupState* pState,
            const CSAMPLE* pInput,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters,
            const EffectEnableState enableState,
            const GroupFeatureState& groupFeatures) override;

    void addDryChannel(
            ReverbGroupState* pState,
            const CSAMPLE* pDry,
            CSAMPLE* pOutput,
            const mixxx::EngineParameters& engineParameters) override;

  private:
    QString debugString() const {
        return getId();
    }

    EngineEffectParameterPointer m_pDecayParameter;
    EngineEffectParameterPointer m_pBandWidthParameter;
    EngineEffectParameterPointer m_pDampingParameter;
    EngineEffectParameterPointer m_pSendParameter;
    EngineEffectParameterPointer m_pDryParameter;

    DISALLOW_COPY_AND_ASSIGN(ReverbEffect);
};
