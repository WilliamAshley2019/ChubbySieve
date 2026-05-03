#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// SieveKnobLAF — custom LookAndFeel for all knobs, toggles, combos.
// Declared BEFORE the editor so it is constructed first and destroyed last.
// Sliders hold a raw pointer to their LookAndFeel; it must outlive them.
//==============================================================================
class SieveKnobLAF : public juce::LookAndFeel_V4
{
public:
    SieveKnobLAF()
    {
        setColour(juce::ComboBox::backgroundColourId,            juce::Colour(0xff1c1c2a));
        setColour(juce::ComboBox::outlineColourId,               juce::Colour(0xff604820));
        setColour(juce::ComboBox::textColourId,                  juce::Colour(0xfff0e0b8));
        setColour(juce::ComboBox::arrowColourId,                 juce::Colour(0xffd4921e));
        setColour(juce::PopupMenu::backgroundColourId,           juce::Colour(0xff1c1c2a));
        setColour(juce::PopupMenu::textColourId,                 juce::Colour(0xfff0e0b8));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff302010));
        setColour(juce::PopupMenu::highlightedTextColourId,      juce::Colour(0xffd4921e));
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float startAngle, float endAngle,
                          juce::Slider& slider) override
    {
        const float radius = juce::jmin(width * 0.5f, height * 0.5f) - 5.0f;
        const float cx     = x + width  * 0.5f;
        const float cy     = y + height * 0.5f;
        const float angle  = startAngle + sliderPos * (endAngle - startAngle);
        const float trackR = radius - 2.0f;
        const auto  fill   = slider.findColour(juce::Slider::rotarySliderFillColourId);

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillEllipse(cx - radius + 1.0f, cy - radius + 3.0f, radius * 2.0f, radius * 2.0f);

        // Body gradient
        juce::ColourGradient body(juce::Colour(0xff303042), cx, cy - radius * 0.6f,
                                  juce::Colour(0xff141420), cx, cy + radius * 0.6f, false);
        g.setGradientFill(body);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Rim
        g.setColour(juce::Colour(0xff50506a));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.2f);

        // Track groove
        juce::Path groove;
        groove.addCentredArc(cx, cy, trackR, trackR, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff2c2c3c));
        g.strokePath(groove, juce::PathStrokeType(4.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc — glow + solid
        if (sliderPos > 0.002f)
        {
            juce::Path arc;
            arc.addCentredArc(cx, cy, trackR, trackR, 0.0f, startAngle, angle, true);
            g.setColour(fill.withAlpha(0.22f));
            g.strokePath(arc, juce::PathStrokeType(9.0f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(fill);
            g.strokePath(arc, juce::PathStrokeType(3.8f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Pointer — bright dot on outer ring, thin line inward
        {
            const float sinA = std::sin(angle);
            const float cosA = std::cos(angle);
            const float tipX = cx + sinA * (trackR - 1.5f);
            const float tipY = cy - cosA * (trackR - 1.5f);
            g.setColour(fill.brighter(0.7f));
            g.fillEllipse(tipX - 3.0f, tipY - 3.0f, 6.0f, 6.0f);
            const float in0 = radius * 0.22f;
            const float in1 = radius * 0.52f;
            g.setColour(fill.brighter(0.3f).withAlpha(0.85f));
            g.drawLine(cx + sinA * in0, cy - cosA * in0,
                       cx + sinA * in1, cy - cosA * in1, 2.2f);
        }

        // Centre cap
        g.setColour(juce::Colour(0xff202030));
        g.fillEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f);
        g.setColour(juce::Colour(0xff484858));
        g.drawEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f, 1.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
                          bool, bool) override
    {
        const bool  on  = btn.getToggleState();
        const float h   = (float) btn.getHeight();
        const float tw  = 28.0f;
        const float th  = 16.0f;
        const float tx  = 2.0f;
        const float ty  = (h - th) * 0.5f;

        g.setColour(on ? juce::Colour(0xff183018) : juce::Colour(0xff222230));
        g.fillRoundedRectangle(tx, ty, tw, th, th * 0.5f);
        g.setColour(on ? juce::Colour(0xff386838) : juce::Colour(0xff404050));
        g.drawRoundedRectangle(tx, ty, tw, th, th * 0.5f, 1.0f);

        const float thumbX = on ? tx + tw - th + 2.0f : tx + 2.0f;
        juce::ColourGradient tg(
            juce::Colour(on ? 0xff88e888 : 0xff909098), thumbX, ty + 2.0f,
            juce::Colour(on ? 0xff40a840 : 0xff585860), thumbX, ty + th - 2.0f, false);
        g.setGradientFill(tg);
        g.fillEllipse(thumbX, ty + 2.0f, th - 4.0f, th - 4.0f);

        g.setColour(on ? juce::Colour(0xffe0f8e0) : juce::Colour(0xff888898));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        g.drawText(btn.getButtonText(),
                   (int)(tx + tw + 7.0f), 0,
                   btn.getWidth() - (int)(tx + tw + 7.0f),
                   btn.getHeight(), juce::Justification::centredLeft);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SieveKnobLAF)
};

//==============================================================================
class ChubbySieveEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChubbySieveEditor(ChubbySieveAudioProcessor&);
    ~ChubbySieveEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    //==========================================================================
    // Sub-component structs — child widgets (slider, label, etc.) are VALUE
    // MEMBERS, not unique_ptrs.  This is the structural pattern that keeps
    // FL Studio's wrapper happy (no dangling-pointer edge cases at load time).
    //==========================================================================

    struct RotaryKnob : public juce::Component
    {
        RotaryKnob(juce::AudioProcessorValueTreeState&,
                   const juce::String& paramId,
                   const juce::String& topLabel,
                   const juce::String& subLabel,
                   juce::Colour colour,
                   SieveKnobLAF&);
        void resized() override;

        juce::Label  label;
        juce::Label  sublabel;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct ToggleSwitch : public juce::Component
    {
        ToggleSwitch(juce::AudioProcessorValueTreeState&,
                     const juce::String& paramId,
                     const juce::String& btnText,
                     SieveKnobLAF&);
        void resized() override;

        juce::ToggleButton toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    struct OversampleSelector : public juce::Component
    {
        OversampleSelector(juce::AudioProcessorValueTreeState&,
                           const juce::String& paramId,
                           SieveKnobLAF&);
        void paint(juce::Graphics&) override;
        void resized() override;

        juce::Label    label;
        juce::ComboBox comboBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    class HarmonicDisplay : public juce::Component, public juce::Timer
    {
    public:
        explicit HarmonicDisplay(ChubbySieveAudioProcessor&);
        void paint(juce::Graphics&) override;
        void timerCallback() override { repaint(); }
        void startDisplayTimer() { startTimerHz(30); }
        void stopDisplayTimer()  { stopTimer(); }
    private:
        ChubbySieveAudioProcessor& proc;
        std::array<float, 6> levels {};
    };

    //==========================================================================
    void paintPanel(juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;

    //==========================================================================
    // laf MUST be declared before any member that uses it (knobs, toggles…).
    SieveKnobLAF laf;

    ChubbySieveAudioProcessor& processor;

    std::unique_ptr<RotaryKnob>         driveKnob;
    std::unique_ptr<RotaryKnob>         warmthKnob;
    std::unique_ptr<RotaryKnob>         punchKnob;
    std::unique_ptr<RotaryKnob>         airKnob;
    std::unique_ptr<RotaryKnob>         mixKnob;
    std::unique_ptr<RotaryKnob>         dynamicKnob;
    std::unique_ptr<ToggleSwitch>       bassToggle;
    std::unique_ptr<OversampleSelector> oversampleSelector;
    std::unique_ptr<HarmonicDisplay>    harmonicDisplay;

    juce::Rectangle<int> shapingPanel;
    juce::Rectangle<int> blendPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChubbySieveEditor)
};
