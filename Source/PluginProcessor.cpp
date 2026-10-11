/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin processor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MicrocosmosAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> ("mix", "dry/wet", 0.0f, 1.0f, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterInt> ("divisions", "divisions", 1, 32, 16));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("attack", "attack", 0, 1, 0.02));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("decay", "decay", 0, 1, 0.5));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("velocityScale", "velo", 0, 1, 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("stereo", "stereo", 0.0f, 0.5f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("stutter", "stutter", 0.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> ("highpass", "highpass",
                                                             juce::NormalisableRange<float> (20.0f, 20000.0f, 0.01f, 0.3f),   // 0.3 = skew, so the slider feels logarithmic
                                                             20.0f));
    
    
    
    for (int i = 0; i < 32; ++i)
    {
        juce::Random rng;   // member variable
        juce::Random rng2;   // member variable
        
        float r = rng.nextFloat();   // 0.0 (inclusive) to 1.0 (exclusive)
        float q = rng2.nextFloat();
        layout.add (std::make_unique<juce::AudioParameterFloat> ("velocity" + juce::String (i),
                                                                 "Velocity " + juce::String (i),
                                                                 0.0f, 1.0f, r));
        if (q >= 0.5) {
            layout.add (std::make_unique<juce::AudioParameterBool> ("on" + juce::String (i),
                                                                    "On? " + juce::String (i),
                                                                    true));
        } else {
            layout.add (std::make_unique<juce::AudioParameterBool> ("on" + juce::String (i),
                                                                    "On? " + juce::String (i),
                                                                    false));
        }
        
    }
    
    
    
    return layout;
}

MicrocosmosAudioProcessor::MicrocosmosAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
: AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                  ),

#endif
state (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < 32; ++i)
    {
        //        divisionParams[(size_t) i].attack = state.getRawParameterValue ("attack" + juce::String (i));
        //        divisionParams[(size_t) i].decay  = state.getRawParameterValue ("decay"  + juce::String (i));
        divisionParams[(size_t) i].on     = state.getRawParameterValue ("division" + juce::String (i));
    }
    mixPtr = state.getRawParameterValue ("mix");

}

//, state (*this, nullptr, "STATE", {
//    //    addParameter(new juce::AudioParameterFloat ("gain", "Gain", 0.0f, 1.0f, 1.0f));
//    //    addParameter(new juce::AudioParameterInt ("bpm", "BPM", 0, 200, 140));
//    //    addParameter(new juce::AudioParameterInt ("divisions", "Bar divisions", 0, 32, 4));
////    std::make_unique<juce::AudioParameterFloat> ("gain", "Gain", 0.0f, 1.0f, 1.0f),
////    std::make_unique<juce::AudioParameterFloat> ("bpm", "BPM", 0, 200, 140),
//
//
//    std::make_unique<juce::AudioParameterInt> ("divisions", "Bar divisions", 1, 32, 16),
//    std::make_unique<juce::AudioParameterFloat> ("attack", "Attack", 0, 1, 0.02),
//    std::make_unique<juce::AudioParameterFloat> ("decay", "Decay", 0, 1, 0.5),
//    std::make_unique<juce::AudioParameterBool> ("division1", "Division 1", false),
//
//
//
//}) {
//
//}


MicrocosmosAudioProcessor::~MicrocosmosAudioProcessor()


{
    
}


//==============================================================================
const juce::String MicrocosmosAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MicrocosmosAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool MicrocosmosAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool MicrocosmosAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double MicrocosmosAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MicrocosmosAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int MicrocosmosAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MicrocosmosAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MicrocosmosAudioProcessor::getProgramName (int index)
{
    return {};
}

void MicrocosmosAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MicrocosmosAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    //    int delayMilliseconds = 200;
    //    auto delaySamples = (int) std::round (sampleRate * delayMilliseconds / 1000.0);
    //    delayBuffer.setSize (2, delaySamples);
//    delayBuffer.clear();
//    delayBufferPos = 0;
    smoothGainL.reset (sampleRate, 0.01);   // 5 ms ramp
    smoothGainR.reset (sampleRate, 0.01);
    smoothGainL.setCurrentAndTargetValue (1.0f);
    smoothGainR.setCurrentAndTargetValue (1.0f);
    dryBuffer.setSize (2, samplesPerBlock);
    smoothMix.reset (sampleRate, 0.02);   // 20 ms ramp
    smoothMix.setCurrentAndTargetValue (mixPtr->load());
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) getTotalNumOutputChannels();
    
    hpf.prepare (spec);
    hpf.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    hpf.reset();
    
}

void MicrocosmosAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MicrocosmosAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    
    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif
    
    return true;
#endif
}
#endif

void MicrocosmosAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    {
        const int n  = buffer.getNumSamples();
        const int ch = juce::jmin (buffer.getNumChannels(), 2);
        
        // some hosts exceed the block size they promised; this is a rare fallback
        if (n > dryBuffer.getNumSamples())
            dryBuffer.setSize (2, n, false, false, true);
        
        for (int c = 0; c < ch; ++c)
            dryBuffer.copyFrom (c, 0, buffer, c, 0, n);
        playingDivision.store (-1);
        pushDivisionEvent (-1);
    }

    juce::ScopedNoDenormals noDenormals;
    //    auto totalNumInputChannels  = getTotalNumInputChannels();
    //    auto totalNumOutputChannels = getTotalNumOutputChannels();
    //    double bpm = 120.0;
    
    //    if (auto* playHead = getPlayHead())
    //    {
    //        if (auto positionInfo = playHead->getPosition())
    //        {
    //            if (auto bpmFromHost = positionInfo->getBpm())
    //            {
    //                bpm = *bpmFromHost;
    //            }
    //            auto ppq  = positionInfo->getPpqPosition();
    //            auto barStart  = positionInfo->getPpqPositionOfLastBarStart();
    //            auto timeSig   = positionInfo->getTimeSignature();
    //            bool playing   = positionInfo->getIsPlaying();
    //        }
    //    }
    
    // Unwrap with fallbacks so you get plain values
    // at every early return, e.g. no playhead / no position / not playing:

    // right after EVERY updateLengths() call (before the loop, and after each division change or bar wrap):
    
    
    
    
    const int    numSamples = buffer.getNumSamples();
    const double sampleRate = getSampleRate();
    float gL = smoothGainL.getNextValue();
    float gR = smoothGainR.getNextValue();
    auto* playHead = getPlayHead();
    if (playHead == nullptr)
        return;
    
    auto pos = playHead->getPosition();
    if (! pos.hasValue())
        return;
    const int divisions = juce::jmax (1, (int) *state.getRawParameterValue ("divisions"));
    const double decay = (double) state.getRawParameterValue ("decay")->load();
    const double attack = (double) state.getRawParameterValue ("attack")->load();
    const double velocityScale = (double) state.getRawParameterValue ("velocityScale")->load();
    stutterPtr = state.getRawParameterValue ("stutter");
    //    bool stutterOn = false;
    highpassPtr = state.getRawParameterValue ("highpass");
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    
    
    const double bpm       = pos->getBpm().orFallback (120.0);
    const double ppq       = pos->getPpqPosition().orFallback (0.0);
        const bool   isPlaying = pos->getIsPlaying();
    const auto timeSig   = pos->getTimeSignature();
    const double quartersPerBar = timeSig->numerator * (4.0 / timeSig->denominator);
    const double samplesPerQuarter = sampleRate * 60.0 / bpm;
    //    const double samplesPerDivision = sampleRate * 60.0 / ((bpm / quartersPerBar) * divisions);
    double quartersIntoBar;
    if (auto barStart = pos->getPpqPositionOfLastBarStart())
        quartersIntoBar = ppq - *barStart;
    else
        quartersIntoBar = std::fmod (ppq, quartersPerBar);   // fallback if host doesn't supply it
    
    // Guard against negative values (pre-roll) and rounding past the bar end
    quartersIntoBar = std::fmod (quartersIntoBar + quartersPerBar, quartersPerBar);
    
    //    const int barProgress = (int) std::round (quartersIntoBar * samplesPerQuarter);
    //        const double gridSize          = 0.25;   // 16th notes, in quarters
    
    //    for (int channel = 0; channel < totalNumInputChannels; ++channel) {
    //        float* channelData = buffer.getWritePointer (channel);
    //        for (int i = 0; i < numSamples; ++i)
    //        {
    //            int progressAtSample = (barProgress + i) % (int) quartersIntoBar;
    //            int currentDivision;
    //            if (progressAtSample == 0)
    //                currentDivision = 0;
    ////            if (barProgress + i) % (int) quartersIntoBar)
    //
    //            channelData[i] *= 1 - ((1/(samplesPerDivision)) * ((barProgress +i)*4));
    //            // ...
    //        }
    //    }
    const double samplesPerBar      = quartersPerBar * samplesPerQuarter;
    const double samplesPerDivision = samplesPerBar / divisions;
    
    // Exact position in the bar at the start of this block (double, not the rounded int)
    double posInBar = std::fmod (quartersIntoBar * samplesPerQuarter, samplesPerBar);
    
    // Initialise from the host position
    int    currentDivision = juce::jlimit (0, divisions - 1, (int) (posInBar / samplesPerDivision));
    double divisionStart   = currentDivision * samplesPerDivision;
    
    
    const int divisionLength = (int) samplesPerDivision;
    
    int attackLength = juce::jmax (1, (int) std::round (attack * samplesPerDivision));
    int decayLength  = juce::jmax (1, juce::jmin ((int) std::round (decay * samplesPerDivision),
                                                  divisionLength - attackLength));
    
    //    const int attackLength = std::round(attack * samplesPerDivision);
//    int halfLength = 1;
    for (int i = 0; i < 32; i++) {
        divisionParams[(size_t) i].velocity = state.getRawParameterValue ("velocity" + juce::String (i));
        divisionParams[(size_t) i].on = state.getRawParameterValue ("on" + juce::String (i));
        divisionParams[(size_t) i].onParam =
        dynamic_cast<juce::AudioParameterBool*> (state.getParameter ("on" + juce::String (i)));
        
    }
    //    const bool isOn = divisionParams[(size_t) currentDivision].on->load() > 0.5f;
    float velocity = juce::jmap((divisionParams[(size_t) currentDivision].velocity->load()), 0.0f, 1.0f, (float) velocityScale, 1.0f);
    if (divisionParams[(size_t) currentDivision].on->load() < 0.5f) {
        velocity = 0;
    }
    //    float currentGainL = 1.0f, currentGainR = 1.0f;
    const auto& p = divisionParams[(size_t) currentDivision];
    juce::Random rng3;
    float currentSide = rng3.nextBool() ? 1.0f : -1.0f;// new side every division
    float v     = p.velocity->load();
    float width = (float) state.getRawParameterValue ("stereo")->load();
    float pan   = juce::jlimit (-1.0f, 1.0f, currentSide * juce::jlimit (0.0f, 1.0f, v) * width);
    
    float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    smoothGainL.reset (sampleRate, (samplesPerDivision/32)/sampleRate);
    smoothGainR.reset (sampleRate, (samplesPerDivision/32)/sampleRate);
    
    smoothGainL.setTargetValue (std::cos (angle));
    smoothGainR.setTargetValue (std::sin (angle));
    playingDivision.store (currentDivision);
    pushDivisionEvent (currentDivision);
    if (! isPlaying)
    {
        playingDivision.store (-1);
        pushDivisionEvent (-1);
        return;   // or skip the envelope loop
    }
    for (int i = 0; i < numSamples; ++i)
    {
        // 0, 1, 2, ... within the current division
        const int samplesIntoDivision = (int) (posInBar - divisionStart);
        
        // use currentDivision and samplesIntoDivision here
        
        // advance one sample
        posInBar += 1.0;
        
        // end of the current division? (the last division ends exactly at the bar end)
        const double divisionEnd = (currentDivision == divisions - 1) ? samplesPerBar
        : divisionStart + samplesPerDivision;
        
        
        if (posInBar >= divisionEnd)
        {
            if (currentDivision == divisions - 1)
            {
                currentDivision = 0;          // new bar
                divisionStart   = 0.0;
                posInBar       -= samplesPerBar;
            }
            else
            {
                ++currentDivision;
                divisionStart += samplesPerDivision;
            }
            juce::Random rng3;
            currentSide = rng3.nextBool() ? 1.0f : -1.0f;// new side every division
            velocity = juce::jmap((divisionParams[(size_t) currentDivision].velocity->load()), 0.0f, 1.0f, (float) velocityScale, 1.0f);
            v     = p.velocity->load();
            
            width = (float) state.getRawParameterValue ("stereo")->load();
            pan   = currentSide * juce::jlimit (0.0f, 1.0f, v) * width;
            angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            playingDivision.store (currentDivision);
            pushDivisionEvent (currentDivision);
            smoothGainL.setTargetValue (std::cos (angle));
            smoothGainR.setTargetValue (std::sin (angle));
            juce::Random rng4;
            stutterActive = rng4.nextFloat() < stutterPtr->load();
            
            
            //                 currentGainL = std::cos (angle);
            //                 currentGainR = std::sin (angle);
            //                velocity = juce::jmap((divisionParams[(size_t) currentDivision].velocity->load()), 0.0f, 1.0f, (float) velocityScale, 1.0f);
            if (divisionParams[(size_t) currentDivision].on->load() < 0.5f) {
                velocity = 0;
            }
            
        }
        //        for (int channel = 0; channel < totalNumInputChannels; ++channel) {
        //            float* channelData = buffer.getWritePointer (channel);
        
        // inside the per-sample loop:
        
        
        
        float env;
        if (stutterActive) {
            const int half = (int) (samplesPerDivision * 0.5);

                const int stutterAttackLength = juce::jmax (1, attackLength / 2);
                const int stutterDecayLength  = juce::jmax (1, decayLength  / 2);

                // position within the current half: 0 .. half-1 in each half
                const int pos = (samplesIntoDivision < half) ? samplesIntoDivision
                                                             : samplesIntoDivision - half;

                if (pos < stutterAttackLength)
                {
                    env = (float) pos / (float) stutterAttackLength;
                }
                else if (pos < stutterAttackLength + stutterDecayLength)
                {
                    const float t = (float) (pos - stutterAttackLength) / (float) stutterDecayLength;
                    env = 1.0f - t * (1.0f - 0);
                }
                else
                {
                    env = 0;
                }
        }
        else {
            if (samplesIntoDivision < attackLength)
            {
                // attack: 0 -> 1
                env = (float) samplesIntoDivision / (float) attackLength;
            }
            else if (samplesIntoDivision < attackLength + decayLength)
            {
                // decay: 1 -> sustainLevel
                const float t = (float) (samplesIntoDivision - attackLength) / (float) decayLength;
                env = 1.0f - t * (1.0f);
            }
            else
            {
                // after the decay, hold at the end value until the next division
                env = 0;
            }
            
        }
            gL = smoothGainL.getNextValue();
            gR = smoothGainR.getNextValue();
            
            if (right != nullptr)
            {
                left[i]  *= env * velocity * gL;
                right[i] *= env * velocity * gR;
            }
            else
            {
                left[i] *= env * velocity;
            }
            
        
    }
    const float cutoff = highpassPtr->load();
    hpf.setCutoffFrequency (cutoff);
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    hpf.process (context);
    smoothMix.setTargetValue (mixPtr->load());

    const int mixChannels = juce::jmin (buffer.getNumChannels(), 2);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float m = smoothMix.getNextValue();   // once per sample, outside the channel loop

        for (int c = 0; c < mixChannels; ++c)
        {
            auto* data = buffer.getWritePointer (c);
            data[i] = dryBuffer.getSample (c, i) * (1.0f - m) + data[i] * m;
        }
    }
}

//==============================================================================
bool MicrocosmosAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MicrocosmosAudioProcessor::createEditor()
{
        return new MicrocosmosAudioProcessorEditor (*this);
//    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void MicrocosmosAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    
    if (auto xmlState = state.copyState().createXml())
        copyXmlToBinary(*xmlState, destData);
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void MicrocosmosAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    
    if (auto xmlState = getXmlFromBinary (data, sizeInBytes))
        state.replaceState(juce::ValueTree::fromXml (*xmlState));
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MicrocosmosAudioProcessor();
}
