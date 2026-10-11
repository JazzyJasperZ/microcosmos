/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
//==============================================================================
/**
*/
class MicrocosmosAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    MicrocosmosAudioProcessor();
    ~MicrocosmosAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    struct DivisionParams
    {
        std::atomic<float>* on     = nullptr;
        juce::AudioParameterBool* onParam = nullptr;
        std::atomic<float>* velocity = nullptr;
        std::atomic<float>* stereo = nullptr;
        
    };
    std::array<DivisionParams, 32> divisionParams;
    juce::AudioProcessorValueTreeState state;
    std::atomic<int> playingDivision { -1 };
    std::atomic<float>* mixPtr = nullptr;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> smoothMix;
    static constexpr int eventSize = 64;
    std::array<std::atomic<int>, eventSize> divisionEvents {};
    std::atomic<uint32_t> eventCount { 0 };
    int lastPushed = -2;   // audio thread only

    void pushDivisionEvent (int d)
    {
        if (d == lastPushed)
            return;

        lastPushed = d;
        const uint32_t n = eventCount.load (std::memory_order_relaxed);
        divisionEvents[n % eventSize].store (d);
        eventCount.store (n + 1, std::memory_order_release);
    }

private:
    //==============================================================================
//    juce::AudioProcessorValueTreeState state;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    int delayBufferPos = 0;
    juce::AudioBuffer<float> delayBuffer;
    juce::SmoothedValue<float> smoothGainL, smoothGainR;
    juce::dsp::StateVariableTPTFilter<float> hpf;
    std::atomic<float>* highpassPtr = nullptr;
    std::atomic<float>* stutterPtr = nullptr;
    juce::AudioBuffer<float> stutterBuffer;
    bool stutterActive = false;
    bool captureValid  = false;
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicrocosmosAudioProcessor)
};
