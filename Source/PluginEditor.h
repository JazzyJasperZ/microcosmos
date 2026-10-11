/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "BinaryData.h"


//==============================================================================
/**
 */
class MicrocosmosAudioProcessorEditor  : public juce::AudioProcessorEditor
{
    public:
    MicrocosmosAudioProcessorEditor (MicrocosmosAudioProcessor&);
    ~MicrocosmosAudioProcessorEditor() override;
    
    //==============================================================================
    
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Typeface::Ptr customTypeface;
    juce::Typeface::Ptr secondTypeface;

    class DivisionWheel : public juce::Component, private juce::Timer
    {
        public:
        explicit DivisionWheel (juce::AudioProcessorValueTreeState& s, const std::atomic<int>& playing)
        : state (s), playingDiv (playing)
        {
            for (int i = 0; i < 32; ++i)
            {
                const auto onID  = "on" + juce::String (i);
                const auto velID = "velocity" + juce::String (i);
                
                onRaw[(size_t) i]   = state.getRawParameterValue (onID);
                velRaw[(size_t) i]  = state.getRawParameterValue (velID);
                onParam[(size_t) i] = state.getParameter (onID);
                velParam[(size_t) i] = state.getParameter (velID);
            }
            
            divisionsRaw = state.getRawParameterValue ("divisions");
            startTimerHz (120);
        }
        
        ~DivisionWheel() override { stopTimer(); }
        
        void paint (juce::Graphics& g) override
        {
            
            const auto c = centre();
            const float R = radius();
            const int n = numDivisions();
            
            g.setColour (juce::Colour (29,27,48));
            g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
            juce::Colour myColor4 = juce::Colour(51, 48, 79);
            g.setColour (myColor4);
            for (int i = 0; i < n; ++i)
            {
                
                if (rayAlpha > 0.0f)
                {
                    g.setColour (myColor4.withAlpha (rayAlpha));
                    for (int i = 0; i < n; ++i)
                    {
                        const auto d = direction (i, n);
                        g.drawLine (c.x, c.y, c.x + d.x * R, c.y + d.y * R, 1.5f);
                    }
                }
                
            }
            for (const auto& t : trails)
            {
                if (t.from >= n || t.to >= n || ! isOn (t.from) || ! isOn (t.to))
                    continue;

                g.setColour (juce::Colours::white.withAlpha (t.alpha * t.alpha));   // squared: fades quickly at first, lingers faintly
                g.drawLine (juce::Line<float> (dotPosition (t.from, n), dotPosition (t.to, n)), 2.0f);
            }
            g.setColour (juce::Colour(juce::Colours::white));
            
            const auto mouse = getMouseXYRelative().toFloat();
            
            // the preview comes back once the mouse has moved a few px after a delete
            if (suppressPreview && mouse.getDistanceFrom (suppressPos) > 4.0f)
                suppressPreview = false;
            
            // a freshly placed dot stays white until the cursor leaves it (and the button is up)
            if (whiteIndex >= 0 && ! isMouseButtonDown() && findDot (mouse) != whiteIndex)
                whiteIndex = -1;
            
            const float dr = dotRadius();
            
            int hoveredDot = -1;
            if (isMouseOver() && mouse.getDistanceFrom (c) <= R)
                hoveredDot = findDot (mouse);
            
            for (int i = 0; i < n; ++i)
            {
                const bool on = isOn (i);
                const float v = vanish[(size_t) i];
                
                if (! on && v <= 0.0f)
                    continue;
                
                const auto pos = dotPosition (i, n);
                
                if (! on)
                {
                    // dot was just deleted: grow and fade out
                    const float q = 1.0f - v;                   // 0 -> 1 over the animation
                    const float r = dr * (1.0f + 0.5f * q);     // grows to 1.5x
                    
                    g.setColour (juce::Colours::white.withAlpha (v));
                    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos));
                    continue;
                }
                
                
                if (glow[(size_t) i] > 0.001f)
                    drawGlow (g, pos, glow[(size_t) i]);
                
                const bool highlighted = (i != whiteIndex) && (i == hoveredDot || i == dragIndex);
                const float p     = pop[(size_t) i];
                const float scale = 1.0f + 0.5f * p * p;   // starts at 1.5x, settles at 1x
                const float r     = dr * scale;
                
                g.setColour (highlighted ? juce::Colour (99, 95, 138) : juce::Colours::white);
                g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos));
            }
            if (dragIndex < 0 && ! suppressPreview && isMouseOver())
            {
                const auto mouse = getMouseXYRelative().toFloat();
                
                if (mouse.getDistanceFrom (c) <= R)
                {
                    const int ray = findRay (mouse);
                    
                    if (ray >= 0 && ! isOn (ray))
                    {
                        const auto d   = direction (ray, n);
                        const auto rel = mouse - c;
                        const float along = juce::jlimit (innerR(), outerR(), rel.x * d.x + rel.y * d.y);
                        
                        g.setColour (juce::Colour (99, 95, 138));
                        g.fillEllipse (juce::Rectangle<float> (dr * 2.0f, dr * 2.0f)
                                       .withCentre (c + d * along));
                    }
                }
            }
        }
        
        void mouseDown (const juce::MouseEvent& e) override
        {
            dragIndex = -1;
            deleteOnClick = false;
            
            const auto p = e.position;
            
            if (int dot = findDot (p); dot >= 0)
            {
                beginGesture (dot);
                dragIndex = dot;
                deleteOnClick = true;          // becomes a delete if it turns out to be a click, not a drag
                return;
            }
            
            if (int ray = findRay (p); ray >= 0)
            {
                beginGesture (ray);
                dragIndex = ray;
                
                const bool wasOn = isOn (ray);
                setVelocityFromPoint (ray, p);
                
                if (! wasOn)
                    onParam[(size_t) ray]->setValueNotifyingHost (1.0f);   // create the dot
                whiteIndex = ray;
                pop[(size_t) ray] = 1.0f;   // start oversized
                vanish[(size_t) ray] = 0.0f;
            }
        }
        
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragIndex >= 0)
                setVelocityFromPoint (dragIndex, e.position);
        }
        
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragIndex < 0)
                return;
            
            if (deleteOnClick && ! e.mouseWasDraggedSinceMouseDown()) {
                onParam[(size_t) dragIndex]->setValueNotifyingHost (0.0f);   // delete the dot
                vanish[(size_t) dragIndex] = 1.0f;
                suppressPreview = true;
                suppressPos = e.position;
            }
            endGesture (dragIndex);
            dragIndex = -1;
            deleteOnClick = false;
            repaint();
        }
        
        private:
        struct Trail { int from, to; float alpha; };

        std::vector<Trail> trails;
        int    lastOn      = -1;     // last on-division that played
        int    lastPlaying = -1;     // to detect changes
        double lastTickMs  = 0.0;
        float  trailSeconds = 5.0f;  // how long a line takes to fade out
        std::array<float, 32> pop {};   // 1 = just placed (biggest), 0 = settled
        std::array<float, 32> vanish {};   // 1 = just deleted, 0 = gone
        int whiteIndex = -1;                 // dot that stays white until the cursor leaves it
        bool suppressPreview = false;        // hides the preview right after a delete
        juce::Point<float> suppressPos;
        juce::AudioProcessorValueTreeState& state;
        std::array<std::atomic<float>*, 32> onRaw {}, velRaw {};
        std::array<juce::RangedAudioParameter*, 32> onParam {}, velParam {};
        std::atomic<float>* divisionsRaw = nullptr;
        const std::atomic<int>& playingDiv;
        int  dragIndex = -1;
        bool deleteOnClick = false;
        float rayAlpha = 0.0f;
        std::array<float, 32> glow {};
        // ---- geometry ----
        juce::Point<float> centre() const { return getLocalBounds().toFloat().getCentre(); }
        float radius()  const { return juce::jmin (getWidth(), getHeight()) * 0.5f; }
        float innerR()  const { return radius() * 0.12f; }
        float outerR()  const { return radius() * 0.88f; }
        float dotRadius() const { return juce::jmax (6.0f, radius() * 0.035f); }
        
        int numDivisions() const
        {
            return juce::jlimit (1, 32, (int) std::round (divisionsRaw->load()));
        }
        
        static juce::Point<float> direction (int i, int n)
        {
            const float angle = -juce::MathConstants<float>::halfPi
            + (float) i * juce::MathConstants<float>::twoPi / (float) n;
            return { std::cos (angle), std::sin (angle) };
        }
        
        bool  isOn (int i) const { return onRaw[(size_t) i]->load() > 0.5f; }
        float velocity (int i) const { return juce::jlimit (0.0f, 1.0f, velRaw[(size_t) i]->load()); }
        
        juce::Point<float> dotPosition (int i, int n) const
        {
            const float r = innerR() + velocity (i) * (outerR() - innerR());
            return centre() + direction (i, n) * r;
        }
        
        // ---- hit testing ----
        int findDot (juce::Point<float> p) const
        {
            const int n = numDivisions();
            int best = -1;
            float bestDist = dotRadius() + 3.0f;
            
            for (int i = 0; i < n; ++i)
            {
                if (! isOn (i)) continue;
                
                const float d = p.getDistanceFrom (dotPosition (i, n));
                if (d < bestDist) { bestDist = d; best = i; }
            }
            return best;
        }
        
        int findRay (juce::Point<float> p) const
        {
            const int n = numDivisions();
            const auto rel = p - centre();
            int best = -1;
            float bestPerp = 8.0f;   // click tolerance in pixels
            
            for (int i = 0; i < n; ++i)
            {
                const auto d = direction (i, n);
                const float along = rel.x * d.x + rel.y * d.y;
                const float perp  = std::abs (rel.x * d.y - rel.y * d.x);
                
                if (along >= innerR() - dotRadius() && along <= outerR() + dotRadius() && perp < bestPerp)
                {
                    bestPerp = perp;
                    best = i;
                }
            }
            return best;
        }
        
        // ---- parameter writes (message thread only) ----
        void setVelocityFromPoint (int i, juce::Point<float> p)
        {
            const auto d = direction (i, numDivisions());
            const auto rel = p - centre();
            const float along = rel.x * d.x + rel.y * d.y;   // distance along the ray
            
            const float t = juce::jlimit (0.0f, 1.0f, (along - innerR()) / (outerR() - innerR()));
            velParam[(size_t) i]->setValueNotifyingHost (velParam[(size_t) i]->convertTo0to1 (t));
        }
        
        void beginGesture (int i)
        {
            onParam[(size_t) i]->beginChangeGesture();
            velParam[(size_t) i]->beginChangeGesture();
        }
        
        void endGesture (int i)
        {
            onParam[(size_t) i]->endChangeGesture();
            velParam[(size_t) i]->endChangeGesture();
        }
        void drawGlow (juce::Graphics& g, juce::Point<float> p, float intensity) const
        {
            const float gr = dotRadius() * 3.5f;   // glow radius
            
            juce::ColourGradient grad (juce::Colours::white.withAlpha (0.9f * intensity), p.x, p.y,
                                       juce::Colours::white.withAlpha (0.0f),             p.x + gr, p.y,
                                       true);   // radial: first point is the centre, second sets the radius
            grad.addColour (0.30, juce::Colours::white.withAlpha (0.45f * intensity));
            grad.addColour (0.60, juce::Colours::white.withAlpha (0.12f * intensity));
            
            g.setGradientFill (grad);
            g.fillEllipse (p.x - gr, p.y - gr, gr * 2.0f, gr * 2.0f);
        }
        void timerCallback() override {
            const auto mouse = getMouseXYRelative().toFloat();
            const bool insideCircle = mouse.getDistanceFrom (centre()) <= radius();
            
            const bool hovering = (isMouseOver() && insideCircle) || isMouseButtonDown();
            
            const float step   = 1.0f / 60.0f;
            const float target = hovering ? 1.0f : 0.0f;
            
            if (rayAlpha < target)
                rayAlpha = juce::jmin (target, rayAlpha + step);
            else if (rayAlpha > target)
                rayAlpha = juce::jmax (target, rayAlpha - step);
            const int playing = playingDiv.load();
            
            for (int i = 0; i < 32; ++i)
            {
                const float target = (i == playing && isOn (i)) ? 1.0f : 0.0f;
                
                if (glow[(size_t) i] < target)
                    glow[(size_t) i] = juce::jmin (target, glow[(size_t) i] + 0.5f);    // fast attack
                else
                    glow[(size_t) i] = juce::jmax (target, glow[(size_t) i] - 0.06f);   // ~250 ms release at 60 Hz
            }
            for (auto& p : pop)
                p = juce::jmax (0.0f, p - 0.1f);   // about 170 ms at 60 Hz
            for (auto& v : vanish)
                v = juce::jmax (0.0f, v - 0.1f);   // about 170 ms at 60 Hz
            const double nowMs = juce::Time::getMillisecondCounterHiRes();
            const float dt = lastTickMs > 0.0 ? (float) ((nowMs - lastTickMs) * 0.001) : 0.0f;
            lastTickMs = nowMs;

            // fade existing lines and drop finished ones
            for (auto& t : trails)
                t.alpha -= dt / trailSeconds;

            trails.erase (std::remove_if (trails.begin(), trails.end(),
                                          [] (const Trail& t) { return t.alpha <= 0.0f; }),
                          trails.end());

            // new division reached?
            if (playing != lastPlaying)
            {
                lastPlaying = playing;

                if (playing < 0)
                {
                    lastOn = -1;                       // transport stopped: next note starts a fresh line
                }
                else if (isOn (playing))
                {
                    if (lastOn >= 0 && lastOn != playing)
                        trails.push_back ({ lastOn, playing, 1.0f });

                    lastOn = playing;
                }
            }
            repaint();
        }
        
        
    };
    class ArcSliders : public juce::Component, private juce::Timer
    {
    public:
        // ids go from the innermost arc to the outermost
        ArcSliders (juce::AudioProcessorValueTreeState& state, const juce::StringArray& ids)
        : apvts (state)
    {
        for (auto& id : ids)
        {
            auto* p = state.getParameter (id);
            jassert (p != nullptr);
            params.push_back (p);
        }
        for (auto* p : params)
            shown.push_back (p->getValue());

        spinOff0.assign (params.size(), 0.0f);
            startTimerHz (120);                   // picks up automation and preset changes
        }

        ~ArcSliders() override { stopTimer(); }
        std::function<void (const juce::String& text)> onHover;
        
        void paint (juce::Graphics& g) override
        {
            drawButton (g);
            const auto c   = centre();
            const float dr = dotRadius();
            const int hot  = hoveredArc();

            for (size_t i = 0; i < params.size(); ++i)
            {
                const bool highlighted = ((int) i == hot);
                const auto colour = highlighted ? juce::Colour (165, 161, 201) : juce::Colour (91, 87, 128);

                const float r = radiusOf (i);

                juce::Path arc;
                arc.addCentredArc (c.x, c.y, r, r, 0.0f,
                                   -juce::MathConstants<float>::halfPi,
                                    juce::MathConstants<float>::halfPi, true);

                g.setColour (colour);
                g.strokePath (arc, juce::PathStrokeType (2.0f));

                if (params[i] == nullptr)
                    continue;
                const auto  pos   = dotPosition (i);
                const float above = c.y - pos.y;                                   // height above the arcs' centre line
                const float alpha = juce::jlimit (0.0f, 1.0f, above / (dr * 2.0f));

                if (alpha > 0.0f)
                {
                    g.setColour (colour.withMultipliedAlpha (alpha));
                    g.fillEllipse (juce::Rectangle<float> (dr * 2.0f, dr * 2.0f).withCentre (pos));
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.position.getDistanceFrom (centre()) <= buttonR())
            {
                randomizeWithSpin();      // was randomizeAll()
                buttonPressed = true;
                return;
            }

            active = findArc (e.position);
            if (active < 0) return;

            spinOff0[(size_t) active] = 0.0f;   // was spinTotal
            params[(size_t) active]->beginChangeGesture();
            setFromPoint (active, e.position);
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (active >= 0)
                setFromPoint (active, e.position);   // stays on the grabbed arc, however far the mouse strays
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            buttonPressed = false;

            if (active >= 0)
                params[(size_t) active]->endChangeGesture();
            active = -1;
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (int i = findArc (e.position); i >= 0)
            {
                auto* p = params[(size_t) i];
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getDefaultValue());
                p->endChangeGesture();
            }
        }

    private:
        int lastHot = -1;
        juce::String lastText;
        juce::AudioProcessorValueTreeState& apvts;
        juce::Random rng;
        float buttonScale = 1.0f;
        bool  buttonPressed = false;
        std::vector<float> shown;      // smoothed copy of each parameter's value
        std::vector<float> spinOff0;   // per arc: starting spin offset in radians (negative = laps still to go)
        float spinT = 1.0f;
        
        float buttonR() const { return innerR() * 0.62f; }   // change 0.62f to resize the button

        bool isOverButton() const
        {
            return isMouseOver() && active < 0
                   && getMouseXYRelative().toFloat().getDistanceFrom (centre()) <= buttonR();
        }

        void randomizeAll()
        {
            for (auto* p : apvts.processor.getParameters())
            {
                auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);

                if (withID == nullptr || withID->paramID == "mix")   // leave dry/wet alone
                    continue;

                p->beginChangeGesture();
                p->setValueNotifyingHost (rng.nextFloat());
                p->endChangeGesture();
            }
        }

        void drawButton (juce::Graphics& g) const
        {
            const auto c = centre();
            const float R     = buttonR() * buttonScale;
            const float glowR = R * 1.5f;

            const auto base = juce::Colours::white;

            juce::ColourGradient glow (base.withAlpha (0.9f), c.x, c.y,
                                       base.withAlpha (0.0f), c.x + glowR, c.y, true);
            glow.addColour (R / glowR, base.withAlpha (0.5f));
            glow.addColour (0.85,      base.withAlpha (0.12f));

            g.setGradientFill (glow);
            g.fillEllipse (c.x - glowR, c.y - glowR, glowR * 2.0f, glowR * 2.0f);

            g.setColour (base);
            g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
        }
        
        int hoveredArc() const
        {
            if (active >= 0)                       // while dragging, keep the grabbed arc lit
                return active;

            if (! isMouseOver())
                return -1;

            return findArc (getMouseXYRelative().toFloat());
        }
        // ---- geometry ----
        float dotRadius() const { return juce::jmin (7.0f, (float) getHeight() * 0.025f); }

        // the arcs' centre is on the bottom edge, raised so a dot at either end isn't clipped
        juce::Point<float> centre() const
        {
            return { getWidth() * 0.5f, getHeight() - dotRadius() - 2.0f };
        }

        float outerR() const
        {
            const auto c = centre();
            return juce::jmin (c.x, c.y) - dotRadius() - 2.0f;
        }

        float innerR() const { return outerR() * 0.4f; }

        float spacing() const
        {
            return params.size() > 1 ? (outerR() - innerR()) / (float) (params.size() - 1) : 1.0f;
        }

        float radiusOf (size_t i) const { return innerR() + (float) i * spacing();
        
        
        }
        // angle (radians) reserved at each end of arc i, so the dot stops short of the bottom
        float endMargin (size_t i) const
        {
            const float gapPixels = dotRadius() + 8.0f;    // change 8.0f to adjust the room
            return juce::jmin (gapPixels / radiusOf (i), juce::MathConstants<float>::halfPi * 0.5f);
        }

        float restTheta (size_t i) const
        {
            const float m = endMargin (i);
            return juce::MathConstants<float>::pi - m
                   - shown[i] * (juce::MathConstants<float>::pi - 2.0f * m);
        }

        float dotTheta (size_t i) const
        {
            if (spinT < 1.0f && spinOff0[i] != 0.0f)
            {
                const float u     = 1.0f - spinT;
                const float eased = 1.0f - u * u * u * u;
                return restTheta (i) + spinOff0[i] * (1.0f - eased);   // offset decays to exactly 0
            }
            return restTheta (i);
        }

        juce::Point<float> dotPosition (size_t i) const
        {
            const float theta = dotTheta (i);
            const auto  c = centre();
            const float r = radiusOf (i);
            return { c.x + r * std::cos (theta), c.y - r * std::sin (theta) };
        }

        // ---- interaction ----
        int findArc (juce::Point<float> p) const
        {
            const auto c = centre();
            if (p.y > c.y + dotRadius())            // only the upper half is active
                return -1;

            const float dist = p.getDistanceFrom (c);
            int best = -1;
            float bestGap = spacing() * 0.5f;       // click tolerance: halfway to the neighbouring arc

            for (size_t i = 0; i < params.size(); ++i)
            {
                const float gap = std::abs (dist - radiusOf (i));
                if (params[i] != nullptr && gap < bestGap) { bestGap = gap; best = (int) i; }
            }
            return best;
        }

        void setFromPoint (int i, juce::Point<float> p)
        {
            const auto c = centre();

            float angle = std::atan2 (c.y - p.y, p.x - c.x);
            if (angle < 0.0f)
                angle = (p.x < c.x) ? juce::MathConstants<float>::pi : 0.0f;

            const float m = endMargin ((size_t) i);
            const float v = (juce::MathConstants<float>::pi - m - angle)
                            / (juce::MathConstants<float>::pi - 2.0f * m);

            params[(size_t) i]->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
        }
        void timerCallback() override
        {
            for (size_t i = 0; i < params.size(); ++i)
            {
                const float v = params[i]->getValue();

                if ((int) i == active || std::abs (v - shown[i]) < 1.0e-4f)
                    shown[i] = v;
                else
                    shown[i] += (v - shown[i]) * 0.2f;   // glides to whatever the value really is
            }

            if (spinT < 1.0f)
            {
                spinT = juce::jmin (1.0f, spinT + 1.0f / (60.0f * 4.4f));
                if (spinT >= 1.0f)
                    std::fill (spinOff0.begin(), spinOff0.end(), 0.0f);
            }
            
            const bool overButton = isOverButton();

            const float target = buttonPressed ? 0.95f
                               : overButton    ? 1.15f
                                               : 1.0f;

            buttonScale += (target - buttonScale) * 0.25f;  // about 300 ms at 60 Hz

//            const bool overButton = isOverButton();
            const int  hot        = hoveredArc();

            juce::String text;
            if (overButton)
                text = "randomize";
            else if (hot >= 0)
                text = params[(size_t) hot]->getName (32) + "  " + params[(size_t) hot]->getCurrentValueAsText();

            if (hot != lastHot || text != lastText)
            {
                lastHot  = hot;
                lastText = text;

                if (onHover)
                    onHover (text);
            }

            repaint();
        }

        void randomizeWithSpin()
        {
            const size_t n = params.size();
            const float twoPi = juce::MathConstants<float>::twoPi;

            std::vector<float> oldValue (n), curOffset (n);
            for (size_t i = 0; i < n; ++i)
            {
                oldValue[i]  = params[i]->getValue();
                curOffset[i] = dotTheta (i) - restTheta (i);   // nonzero only if pressed mid-spin
            }

            randomizeAll();

            for (size_t i = 0; i < n; ++i)
            {
                if (std::abs (params[i]->getValue() - oldValue[i]) < 1.0e-4f)
                {
                    spinOff0[i] = 0.0f;                       // dry/wet etc. stay put
                    continue;
                }

                const float laps = n > 1 ? 1.0f + 3.0f * (float) (n - 1 - i) / (float) (n - 1) : 1.0f;
                spinOff0[i] = curOffset[i] - laps * twoPi;    // always negative, so it travels counterclockwise
            }

            spinT = 0.0f;
        }
        std::vector<juce::RangedAudioParameter*> params;
        int active = -1;
    };
    class ScrambleText : public juce::Component, private juce::Timer
    {
    public:
        ScrambleText() { setInterceptsMouseClicks (false, false); }
        ~ScrambleText() override { stopTimer(); }
        std::function<void()> onFinished;
        void setText (const juce::String& newText)
        {
            target = newText;
            restart();
        }
        void setTextInstant (const juce::String& newText)
        {
            stopTimer();
            target = newText;
            displayed = newText;
            revealFrame.clear();   // missing entries read as 0, so every character counts as revealed
            frame = 0;
            repaint();
        }
        
        
        void setTypeface (juce::Typeface::Ptr t)
        {
            typeface = t;
            repaint();
        }
      
        void restart()
        {
            revealFrame.clear();

            for (int i = 0; i < target.length(); ++i)
                revealFrame.add (10 + i * 3 + rng.nextInt (20));   // start delay + left-to-right sweep + jitter

            displayed = juce::String();
            for (int i = 0; i < target.length(); ++i)
                displayed += randomChar();

            frame = 0;
            startTimerHz (45);
        }

        void paint (juce::Graphics& g) override
        {
            juce::AttributedString s;
            s.setJustification (juce::Justification::centredLeft);
            s.setWordWrap (juce::AttributedString::none);   // keep everything on one line

            const juce::Font font = typeface != nullptr
                ? juce::Font (juce::FontOptions (typeface)).withHeight (38.0f)
                : juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 38.0f, juce::Font::plain));


            for (int i = 0; i < target.length(); ++i)
            {
//                const bool revealed = frame >= revealFrame[i] || target[i] == ' ';
                const auto colour   = juce::Colours::white;

                s.append (juce::String::charToString (displayed[i]), font, colour);
            }

            s.draw (g, getLocalBounds().toFloat());
        }

    private:
        juce::Typeface::Ptr typeface;

        void timerCallback() override
        {
            
            ++frame;
            bool allDone = true;

            for (int i = 0; i < target.length(); ++i)
            {
                if (target[i] == ' ' || frame >= revealFrame[i])
                {
                    displayed = displayed.replaceSection (i, 1, juce::String::charToString (target[i]));
                }
                else
                {
                    allDone = false;

                    if (frame % 2 == 0)   // change glyphs every other frame so it isn't a blur
                        displayed = displayed.replaceSection (i, 1, juce::String::charToString (randomChar()));
                }
            }

            repaint();

            if (allDone)
            {
                stopTimer();

                if (onFinished)
                    onFinished();
            }
        }

        juce::juce_wchar randomChar()
        {
            static const juce::String chars ("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#$%&*+=<>/\\|");
            return chars[rng.nextInt (chars.length())];
        }

        juce::String target, displayed;
        juce::Array<int> revealFrame;
        juce::Random rng;
        int frame = 0;
    };
    
    private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    MicrocosmosAudioProcessor& audioProcessor;
    ArcSliders arcs { audioProcessor.state,
                      { "mix", "highpass", "divisions", "velocityScale", "stereo", "stutter", "attack", "decay" } };
    
    DivisionWheel wheel { audioProcessor.state, audioProcessor.playingDivision };
    ScrambleText title;
    juce::Label version;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicrocosmosAudioProcessorEditor)
};
