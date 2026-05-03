#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// SieveKnobLAF — custom look and feel for knobs, toggles, combo boxes.
// Must be declared before the editor so its lifetime exceeds the widgets.
//==============================================================================
class SieveKnobLAF : public juce::LookAndFeel_V4
{
public:
    SieveKnobLAF()
    {
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1c1c2a));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff604820));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xfff0e0b8));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffd4921e));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1c1c2a));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff0e0b8));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff302010));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xffd4921e));
    }

    void drawRotarySlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPos,
        float startAngle, float endAngle,
        juce::Slider& slider) override
    {
        const float radius = juce::jmin(width * 0.5f, height * 0.5f) - 5.0f;
        const float cx = x + width * 0.5f;
        const float cy = y + height * 0.5f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float trackR = radius - 2.0f;
        const auto  fill = slider.findColour(juce::Slider::rotarySliderFillColourId);

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(cx - radius + 1.0f, cy - radius + 3.5f, radius * 2.0f, radius * 2.0f);

        // Body gradient
        juce::ColourGradient body(juce::Colour(0xff2e2e42), cx, cy - radius * 0.6f,
            juce::Colour(0xff131320), cx, cy + radius * 0.6f, false);
        g.setGradientFill(body);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Rim
        g.setColour(juce::Colour(0xff4e4e62));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.2f);

        // Track groove
        juce::Path groove;
        groove.addCentredArc(cx, cy, trackR, trackR, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colour(0xff28283a));
        g.strokePath(groove, juce::PathStrokeType(4.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc
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

        // Pointer
        {
            const float sa = std::sin(angle), ca = std::cos(angle);
            g.setColour(fill.brighter(0.7f));
            g.fillEllipse(cx + sa * (trackR - 1.5f) - 3.0f,
                cy - ca * (trackR - 1.5f) - 3.0f, 6.0f, 6.0f);
            g.setColour(fill.brighter(0.3f).withAlpha(0.85f));
            g.drawLine(cx + sa * radius * 0.22f, cy - ca * radius * 0.22f,
                cx + sa * radius * 0.52f, cy - ca * radius * 0.52f, 2.2f);
        }

        // Centre cap
        g.setColour(juce::Colour(0xff1e1e30));
        g.fillEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f);
        g.setColour(juce::Colour(0xff464658));
        g.drawEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f, 1.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
        bool, bool) override
    {
        const bool  on = btn.getToggleState();
        const float h = (float)btn.getHeight();
        const float tw = 28.0f, th = 16.0f, tx = 2.0f, ty = (h - th) * 0.5f;

        g.setColour(on ? juce::Colour(0xff183018) : juce::Colour(0xff222230));
        g.fillRoundedRectangle(tx, ty, tw, th, th * 0.5f);
        g.setColour(on ? juce::Colour(0xff386838) : juce::Colour(0xff404050));
        g.drawRoundedRectangle(tx, ty, tw, th, th * 0.5f, 1.0f);

        const float thumbX = on ? tx + tw - th + 2.0f : tx + 2.0f;
        juce::ColourGradient tg(juce::Colour(on ? 0xff88e888 : 0xff909098), thumbX, ty + 2.0f,
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
    // ---- Sub-components: widgets are VALUE MEMBERS, not unique_ptrs ----
    // This is the structural pattern that keeps FL Studio's wrapper happy.

    struct RotaryKnob : public juce::Component
    {
        RotaryKnob(juce::AudioProcessorValueTreeState&,
            const juce::String& paramId,
            const juce::String& topLabel,
            const juce::String& subLabel,
            juce::Colour colour,
            SieveKnobLAF&);
        void resized() override;
        juce::Label  label, sublabel;
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

    // ---- Real-time FFT spectrum display ----
    // Shows actual harmonic energy in the processed signal (not knob values).
    // Timer started in constructor so it runs immediately — fixes the bug where
    // the display only updated after closing/reopening the plugin window.
    class SpectrumDisplay : public juce::Component, public juce::Timer
    {
    public:
        explicit SpectrumDisplay(ChubbySieveAudioProcessor&);
        void paint(juce::Graphics&) override;
        void timerCallback() override { repaint(); }
        void startDisplayTimer() { startTimerHz(30); }
        void stopDisplayTimer() { stopTimer(); }
    private:
        ChubbySieveAudioProcessor& proc;
    };

    // ---- Helpers ----
    void paintPanel(juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;

    // ---- Members ----
    // laf MUST be declared before any widget that references it
    SieveKnobLAF laf;

    ChubbySieveAudioProcessor& processor;

    // Row 1 — Gain/Drive section
    std::unique_ptr<RotaryKnob>         inputGainKnob;
    std::unique_ptr<RotaryKnob>         driveKnob;
    std::unique_ptr<RotaryKnob>         outputGainKnob;
    // Row 2 — Harmonic shaping
    std::unique_ptr<RotaryKnob>         warmthKnob;
    std::unique_ptr<RotaryKnob>         punchKnob;
    std::unique_ptr<RotaryKnob>         airKnob;
    // Row 3 — Processing / output
    std::unique_ptr<RotaryKnob>         mixKnob;
    std::unique_ptr<RotaryKnob>         dynamicKnob;
    std::unique_ptr<RotaryKnob>         foldbackKnob;
    // Controls
    std::unique_ptr<ToggleSwitch>       bassToggle;
    std::unique_ptr<OversampleSelector> oversampleSelector;
    // Spectrum
    std::unique_ptr<SpectrumDisplay>    spectrumDisplay;

    // Pre-computed panel rectangles used in paint()
    juce::Rectangle<int> gainPanel, shapingPanel, blendPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChubbySieveEditor)
};