/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MicrocosmosAudioProcessorEditor::MicrocosmosAudioProcessorEditor (MicrocosmosAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    
    customTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::BeastRegular_otf,
                                                              BinaryData::BeastRegular_otfSize);
    secondTypeface = juce::Typeface::createSystemTypefaceFor (BinaryData::JetBrainsMonoBold_ttf,
                                                              BinaryData::JetBrainsMonoBold_ttfSize);
    juce::Random rng;

   
    // editor constructor
    addAndMakeVisible (wheel);
    addAndMakeVisible (arcs);
    addAndMakeVisible (version);
    version.setVisible (false);
    title.onFinished = [this] { version.setVisible (true); };
    addAndMakeVisible (title);
    title.setTypeface (customTypeface);
    title.setText ("microcosmos");
    version.setText ("v1.0", juce::dontSendNotification);
    version.setColour (juce::Label::textColourId, juce::Colours::white);
    version.setFont (juce::Font (juce::FontOptions (secondTypeface)).withHeight (18.0f));
    version.setInterceptsMouseClicks (false, false);
//    addAndMakeVisible (version);
//    title.setText ("MICROCOSMOS");   // keep this if you want the intro scramble on load

    arcs.onHover = [this] (const juce::String& text)
    {
        const bool showingTitle = text.isEmpty();

        title.setTextInstant (showingTitle ? "microcosmos" : text);
        version.setVisible (showingTitle);
    };
    // resized()
    
    setSize (400, 650);
}

MicrocosmosAudioProcessorEditor::~MicrocosmosAudioProcessorEditor()
{
}

//==============================================================================
void MicrocosmosAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)

    
    juce::Colour myColor = juce::Colour(34, 175, 0);
    juce::Colour myColor2 = juce::Colour(16, 17, 20);
    juce::Colour myColor3 = juce::Colour(29,27,48);
    g.setColour(myColor2);
        g.fillAll();
    
//    g.setFont (juce::Font (juce::FontOptions (customTypeface)).withHeight (38.0f));
//    g.fillAll (juce::Colours::darkblue);

//    g.setColour (juce::Colours::white);
//    g.setFont (juce::FontOptions (15.0f));
//    auto area = getLocalBounds()
//                    .withTrimmedLeft (15)
//                    .withTrimmedRight (0)
//                    .withTrimmedTop (20)
//                    .withTrimmedBottom (0);
////    g.drawFittedText ("microcosmos", area, juce::Justification::topLeft, 1);
//    g.setColour(myColor3);
//    g.fillEllipse (37.5f, 78.0f, 325.0f, 325.0f);
    

    
}

void MicrocosmosAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
//    
//    const float w = (float) getWidth();
//    const float h = (float) getHeight();
//    const float shortSide = juce::jmin (w, h);
    title.setBounds (20, 15, getWidth() - 40, 50);
    version.setBounds (273, 46, 100, 16);
    const int wheelSize = 325;   // circle diameter in px, change this to resize it

        wheel.setBounds ((getWidth()  - wheelSize) / 2,          // centred horizontally
                         (getHeight() - wheelSize) / 2 - 70,     // centred vertically, then 50 px up
                         wheelSize, wheelSize);
    arcs.setBounds ((getWidth() - 400) / 2, getHeight() - 160, 400, 160);   // 400 px wide, 160 px tall, centred  // adjust the height to suit

    
//    for (auto& b : blobs)
//    {
//        const float d = b->nsize * shortSide;               // diameter, same for width and height
//                juce::Rectangle<float> r (d, d);
//                r.setCentre (b->nx * w, b->ny * h);
//                b->setBounds (r.toNearestInt());
//    }

}
