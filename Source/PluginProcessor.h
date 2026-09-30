#pragma once
#include <JuceHeader.h>
#include <NAM/dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include "DriveDSP.h"
#include "DelayDSP.h"
#include "SpaceDSP.h"
#include "DSP/StableTuner.h"

class SDNAProcessor final : public juce::AudioProcessor
{
public:
    enum ModuleId { Drive = 0, Amp, EQ, IR, Comp, Delay, Space, ModuleCount };
    using ModuleOrder = std::array<int, ModuleCount>;

    SDNAProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    bool loadModel(bool preampOrSingle, const juce::File&, juce::String& error);
    juce::String modelName(bool preampOrSingle) const;
    bool loadIR(const juce::File&, juce::String& error);
    juce::String irName() const;
    juce::String status() const { return warning; }

    float inputPeak() const { return inputLevel.load(); }
    float outputPeak() const { return outputLevel.load(); }
    float powerInputPeak() const { return powerFeedLevel.load(); }
    float ampOutputPeak() const { return ampOutputLevel.load(); }
    float cabinetOutputPeak() const { return cabinetOutputLevel.load(); }
    float cpuPercent() const { return cpuLevel.load(); }
    float tunerFrequency() const { return tunerHz.load(); }
    float gainReduction() const { return compReduction.load(); }

    void setDirectMonitor(bool enabled) { directMonitor.store(enabled); }
    void setAudioMuted(bool enabled) { audioMuted.store(enabled); }
    bool isAudioMuted() const { return audioMuted.load(); }
    bool isDirectMonitor() const { return directMonitor.load(); }

    ModuleOrder getModuleOrder() const noexcept;
    void setModuleOrder(const ModuleOrder&) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();

private:
    struct Slot { std::unique_ptr<nam::DSP> model; juce::File file; };
    Slot preOrSingle, power;
    mutable std::mutex modelMutex;
    mutable std::mutex cabinetMutex;
    double rate = 48000.0;
    int maxBlock = 512;
    juce::String warning;

    juce::dsp::IIR::Filter<float> lowShelf, midPeak, highShelf, presence, depth;
    std::array<std::array<juce::dsp::IIR::Filter<float>, 10>, 2> graphicEQ;
    std::array<juce::dsp::IIR::Filter<float>, 2> irLowCut, irHighCut;
    std::array<float, 5> lastAmpEQ { 1000, 1000, 1000, 1000, 1000 };
    std::array<float, 10> lastGraphicEQ {};
    std::array<float, 2> lastIRFilters { -1.0f, -1.0f };
    bool ampFiltersReady = false, graphicFiltersReady = false;

    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };
    std::atomic<float> powerFeedLevel { 0.0f }, ampOutputLevel { 0.0f }, cabinetOutputLevel { 0.0f };
    std::atomic<float> cpuLevel { 0.0f }, tunerHz { 0.0f }, compReduction { 0.0f };
    std::atomic<bool> directMonitor { false }, audioMuted { true };

    juce::dsp::Convolution cabinetIRL, cabinetIRR;
    sdna::StereoDrive pedalDrive;
    sdna::StereoDelay delay;
    sdna::Space reverb;
    StableTuner tuner;
    juce::File irFile;
    float compEnvelope = 0.0f, compGain = 1.0f;
    std::atomic<uint32_t> packedOrder { 0x06543210u };

    void updateAmpFilters();
    void updateGraphicFilters();
    void updateIRFilters();
    void processAmp(juce::AudioBuffer<float>&);
    void processDrive(juce::AudioBuffer<float>&);
    void processGraphicEQ(juce::AudioBuffer<float>&);
    void processIR(juce::AudioBuffer<float>&);
    void processComp(juce::AudioBuffer<float>&);
    void processDelay(juce::AudioBuffer<float>&);
    void processSpace(juce::AudioBuffer<float>&);
    void restoreModels(const juce::ValueTree&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SDNAProcessor)
};
