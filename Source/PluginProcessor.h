#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "TapeDSP.h"

class VegaTapeProcessor : public juce::AudioProcessor
{
public:
    VegaTapeProcessor();
    ~VegaTapeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vega Productions Tape"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (ID::bypass); }

    // ---- factory presets
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- metering for the editor (VU = RMS, 0 VU = -18 dBFS)
    std::atomic<float> vuIn[2]  { 0.0f, 0.0f };
    std::atomic<float> vuOut[2] { 0.0f, 0.0f };
    std::atomic<bool>  meterShowsInput { false };
    std::atomic<float> transportActivity { 0.0f };   // 0..1, drives the reel animation
    std::atomic<double> hostBpm { 120.0 };

private:
    void updateFilters();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    double fs = 44100.0;
    int latency = 0;

    juce::dsp::Oversampling<float> oversampler { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };

    vega::TapeSaturator sat[2];
    vega::Biquad headBump[2], bumpDip[2], lowCut[2], hfLoss[2], hfLoss2[2], formulaShelf[2], dcBlock[2];
    vega::FracDelay transport[2], dryLine[2];
    vega::WowFlutter wow;
    vega::Hiss hiss[2] { vega::Hiss (11), vega::Hiss (23) };
    vega::TapeEcho echo;

    juce::SmoothedValue<float> inGain, outGain, mixAmt, bypassMix, driveSm, echoSendSm;
    float baseDelaySamples = 0;
    int lastFormula = -1, lastSpeed = -1, lastBias = -1;
    float lastHp = -1, lastLp = -1;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VegaTapeProcessor)
};
