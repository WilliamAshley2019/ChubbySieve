#include "PluginEditor.h"

// Colour palette constants
namespace Clr
{
    constexpr juce::uint32 BgDeep    = 0xff0e0e16;
    constexpr juce::uint32 BgPanel   = 0xff181824;
    constexpr juce::uint32 BgPanelHi = 0xff1e1e2c;
    constexpr juce::uint32 Border    = 0xff3a3250;
    constexpr juce::uint32 Amber     = 0xffd4921e;
    constexpr juce::uint32 AmberDim  = 0xff7a5010;
    constexpr juce::uint32 TextPri   = 0xfff2e8d0;
    constexpr juce::uint32 TextSec   = 0xff887a60;
    constexpr juce::uint32 TextDim   = 0xff504840;
}

//==============================================================================
// RotaryKnob
//==============================================================================
ChubbySieveEditor::RotaryKnob::RotaryKnob(
    juce::AudioProcessorValueTreeState& state,
    const juce::String& paramId,
    const juce::String& topLabel,
    const juce::String& subLabel,
    juce::Colour colour,
    SieveKnobLAF& laf)
{
    // Top label (parameter name)
    label.setText(topLabel, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
    label.setColour(juce::Label::textColourId, juce::Colour(Clr::TextPri));
    addAndMakeVisible(label);

    // Sublabel (harmonic order or function hint)
    sublabel.setText(subLabel, juce::dontSendNotification);
    sublabel.setJustificationType(juce::Justification::centred);
    sublabel.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f)));
    sublabel.setColour(juce::Label::textColourId, colour.withAlpha(0.75f));
    addAndMakeVisible(sublabel);

    // Slider
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.setColour(juce::Slider::rotarySliderFillColourId,    colour);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, colour.darker(0.4f));
    slider.setLookAndFeel(&laf);
    addAndMakeVisible(slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, paramId, slider);
}

void ChubbySieveEditor::RotaryKnob::resized()
{
    const int h       = getHeight();
    const int w       = getWidth();
    const int topH    = 16;
    const int subH    = 12;
    const int knobSize = juce::jmin(w, h - topH - subH);

    label.setBounds(0, 0, w, topH);
    slider.setBounds((w - knobSize) / 2, topH, knobSize, knobSize);
    sublabel.setBounds(0, topH + knobSize, w, subH);
}

//==============================================================================
// ToggleSwitch
//==============================================================================
ChubbySieveEditor::ToggleSwitch::ToggleSwitch(
    juce::AudioProcessorValueTreeState& state,
    const juce::String& paramId,
    const juce::String& btnText,
    SieveKnobLAF& laf)
{
    toggle.setButtonText(btnText);
    toggle.setLookAndFeel(&laf);
    addAndMakeVisible(toggle);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        state, paramId, toggle);
}

void ChubbySieveEditor::ToggleSwitch::resized()
{
    toggle.setBounds(0, 0, getWidth(), getHeight());
}

//==============================================================================
// OversampleSelector
//==============================================================================
ChubbySieveEditor::OversampleSelector::OversampleSelector(
    juce::AudioProcessorValueTreeState& state,
    const juce::String& paramId,
    SieveKnobLAF& laf)
{
    label.setText("QUALITY", juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f).withStyle("Bold")));
    label.setColour(juce::Label::textColourId, juce::Colour(Clr::TextSec));
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);

    comboBox.addItem("1x  Fast",      1);
    comboBox.addItem("2x  Balanced",  2);
    comboBox.addItem("4x  Quality",   3);
    comboBox.addItem("8x  Mastering", 4);
    comboBox.setLookAndFeel(&laf);
    addAndMakeVisible(comboBox);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, paramId, comboBox);
}

void ChubbySieveEditor::OversampleSelector::paint(juce::Graphics& g)
{
    // Subtle enclosing panel
    g.setColour(juce::Colour(Clr::BgPanel));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
    g.setColour(juce::Colour(Clr::Border));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
}

void ChubbySieveEditor::OversampleSelector::resized()
{
    label.setBounds(6, 4, 52, 14);
    comboBox.setBounds(6, 20, getWidth() - 12, 22);
}

//==============================================================================
// HarmonicDisplay
//==============================================================================
ChubbySieveEditor::HarmonicDisplay::HarmonicDisplay(ChubbySieveAudioProcessor& p)
    : proc(p)
{
}

void ChubbySieveEditor::HarmonicDisplay::paint(juce::Graphics& g)
{
    const int W = getWidth();
    const int H = getHeight();

    // Background
    g.setColour(juce::Colour(0xff0c0c18));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.0f);

    // Grid lines
    g.setColour(juce::Colour(0xff1e1e2c));
    for (int x = 0; x < W; x += 12)
        g.drawLine((float)x, 0.0f, (float)x, (float)H, 0.7f);
    for (int y = 0; y < H; y += 10)
        g.drawLine(0.0f, (float)y, (float)W, (float)y, 0.7f);

    // Clip the bars to this component
    g.reduceClipRegion(getLocalBounds());

    // Read parameter values (safe on message thread)
    levels[2] = proc.getWarmth();
    levels[3] = proc.getPunch();
    levels[4] = proc.getAir() * 0.75f;
    levels[5] = proc.getAir() * 0.45f;

    struct BarInfo { const char* name; const char* harmonic; juce::uint32 colour; };
    const BarInfo bars[4] = {
        { "WARMTH", "2nd",  0xffe87820 },
        { "PUNCH",  "3rd",  0xffd4a018 },
        { "AIR",    "4th",  0xff38a8d0 },
        { "EDGE",   "5th",  0xffb060e0 }
    };

    const int numBars  = 4;
    const int barSlot  = (W - 20) / numBars;
    const int barW     = barSlot - 8;
    const int leftOff  = 10;
    const int labelH   = 28;
    const int meterH   = H - labelH;

    for (int i = 0; i < numBars; ++i)
    {
        const float level = levels[(size_t)(i + 2)];
        const int   bx    = leftOff + i * barSlot;
        const juce::Colour col = juce::Colour(bars[i].colour);

        // Bar fill — gradient from dim at bottom to bright at top
        if (level > 0.002f)
        {
            const int barH = (int)(level * (float)meterH * 0.9f);
            const int by   = meterH - barH;

            juce::ColourGradient grad(
                col,                      (float)bx, (float)by,
                col.darker(0.9f),         (float)bx, (float)meterH, false);
            grad.addColour(0.0, col.brighter(0.3f));
            g.setGradientFill(grad);
            g.fillRoundedRectangle((float)bx, (float)by,
                                   (float)barW, (float)barH, 3.0f);

            // Top glow cap
            g.setColour(col.brighter(0.8f).withAlpha(0.7f));
            g.fillRoundedRectangle((float)bx, (float)by,
                                   (float)barW, 4.0f, 2.0f);

            // Subtle bar outline
            g.setColour(col.withAlpha(0.35f));
            g.drawRoundedRectangle((float)bx, (float)by,
                                   (float)barW, (float)barH, 3.0f, 0.8f);
        }
        else
        {
            // Empty bar ghost
            g.setColour(juce::Colour(0xff1c1c2c));
            g.fillRoundedRectangle((float)bx, 4.0f,
                                   (float)barW, (float)(meterH - 8), 3.0f);
        }

        // Label area below meter
        const int labelY = meterH + 2;
        g.setColour(col.withAlpha(0.9f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f).withStyle("Bold")));
        g.drawText(bars[i].name, bx - 4, labelY,     barW + 8, 12, juce::Justification::centred);

        g.setColour(juce::Colour(Clr::TextSec));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f)));
        g.drawText(bars[i].harmonic, bx - 4, labelY + 12, barW + 8, 10, juce::Justification::centred);
    }

    // Border
    g.setColour(juce::Colour(Clr::Border));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 5.0f, 1.0f);
}

//==============================================================================
// paintPanel — shared section-panel painter
//==============================================================================
void ChubbySieveEditor::paintPanel(juce::Graphics& g,
                                    juce::Rectangle<int> b,
                                    const juce::String& title) const
{
    // Panel body
    g.setColour(juce::Colour(Clr::BgPanel));
    g.fillRoundedRectangle(b.toFloat(), 6.0f);

    // Panel border
    g.setColour(juce::Colour(Clr::Border));
    g.drawRoundedRectangle(b.toFloat().reduced(0.5f), 6.0f, 1.0f);

    // Section title tab
    const int tabW = 110;
    const int tabH = 14;
    const int tabX = b.getX() + 10;
    const int tabY = b.getY() - 7;

    g.setColour(juce::Colour(Clr::AmberDim));
    g.fillRoundedRectangle((float)tabX, (float)tabY, (float)tabW, (float)tabH, 3.0f);
    g.setColour(juce::Colour(Clr::Amber));
    g.drawRoundedRectangle((float)tabX, (float)tabY,
                            (float)tabW, (float)tabH, 3.0f, 0.8f);

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    g.setColour(juce::Colour(Clr::Amber));
    g.drawText(title, tabX, tabY, tabW, tabH, juce::Justification::centred);
}

//==============================================================================
// Main Editor
//==============================================================================
ChubbySieveEditor::ChubbySieveEditor(ChubbySieveAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    using P = ChubbySieveAudioProcessor::Parameters;

    driveKnob  = std::make_unique<RotaryKnob>(processor.apvts, P::drive,
                     "DRIVE",  "input gain",
                     juce::Colour(0xffe84820), laf);

    warmthKnob = std::make_unique<RotaryKnob>(processor.apvts, P::warmth,
                     "WARMTH", "T2  2nd harm.",
                     juce::Colour(0xffe87820), laf);

    punchKnob  = std::make_unique<RotaryKnob>(processor.apvts, P::punch,
                     "PUNCH",  "T3  3rd harm.",
                     juce::Colour(0xffd4a018), laf);

    airKnob    = std::make_unique<RotaryKnob>(processor.apvts, P::air,
                     "AIR",    "T4/T5  high harm.",
                     juce::Colour(0xff38a8d0), laf);

    mixKnob    = std::make_unique<RotaryKnob>(processor.apvts, P::mix,
                     "MIX",    "dry / wet",
                     juce::Colour(0xffa0a8c8), laf);

    dynamicKnob = std::make_unique<RotaryKnob>(processor.apvts, P::dynamicDepth,
                     "DYNAMIC", "envelope depth",
                     juce::Colour(0xff48c848), laf);

    bassToggle = std::make_unique<ToggleSwitch>(processor.apvts, P::bassEnhance,
                     "Bass Enhance", laf);

    oversampleSelector = std::make_unique<OversampleSelector>(
                     processor.apvts, P::oversample, laf);

    harmonicDisplay = std::make_unique<HarmonicDisplay>(processor);

    addAndMakeVisible(driveKnob.get());
    addAndMakeVisible(warmthKnob.get());
    addAndMakeVisible(punchKnob.get());
    addAndMakeVisible(airKnob.get());
    addAndMakeVisible(mixKnob.get());
    addAndMakeVisible(dynamicKnob.get());
    addAndMakeVisible(bassToggle.get());
    addAndMakeVisible(oversampleSelector.get());
    addAndMakeVisible(harmonicDisplay.get());

    setSize(520, 510);
    setResizable(false, false);
}

ChubbySieveEditor::~ChubbySieveEditor()
{
    if (harmonicDisplay) harmonicDisplay->stopDisplayTimer();

    // Explicitly clear LookAndFeel on sliders before laf is destroyed.
    // C++ destroys members in reverse declaration order, and laf is declared
    // before the knob unique_ptrs, so laf would be destroyed FIRST without this.
    // Resetting to nullptr here ensures sliders revert to the default LAF safely.
    auto resetLAF = [](RotaryKnob* k) { if (k) k->slider.setLookAndFeel(nullptr); };
    resetLAF(driveKnob.get());
    resetLAF(warmthKnob.get());
    resetLAF(punchKnob.get());
    resetLAF(airKnob.get());
    resetLAF(mixKnob.get());
    resetLAF(dynamicKnob.get());
    if (bassToggle)         bassToggle->toggle.setLookAndFeel(nullptr);
    if (oversampleSelector) oversampleSelector->comboBox.setLookAndFeel(nullptr);
}

void ChubbySieveEditor::visibilityChanged()
{
    if (isShowing() && harmonicDisplay)
        harmonicDisplay->startDisplayTimer();
    else if (harmonicDisplay)
        harmonicDisplay->stopDisplayTimer();
}

//==============================================================================
void ChubbySieveEditor::paint(juce::Graphics& g)
{
    const int W = getWidth();
    const int H = getHeight();

    // === Deep background ===
    g.setColour(juce::Colour(Clr::BgDeep));
    g.fillAll();

    // Subtle radial vignette
    {
        juce::ColourGradient vignette(juce::Colour(0x00000000), W * 0.5f, H * 0.4f,
                                      juce::Colour(0x50000000), 0.0f, 0.0f, true);
        g.setGradientFill(vignette);
        g.fillAll();
    }

    // === Header bar ===
    {
        const juce::Rectangle<float> hdr(0.0f, 0.0f, (float)W, 62.0f);
        juce::ColourGradient hdrGrad(juce::Colour(0xff141428), 0.0f, 0.0f,
                                     juce::Colour(0xff0e0e1c), 0.0f, 62.0f, false);
        g.setGradientFill(hdrGrad);
        g.fillRect(hdr);

        // Amber bottom rule
        g.setColour(juce::Colour(Clr::Amber).withAlpha(0.7f));
        g.drawLine(14.0f, 62.0f, (float)(W - 14), 62.0f, 1.5f);
        g.setColour(juce::Colour(Clr::Amber).withAlpha(0.15f));
        g.drawLine(14.0f, 63.5f, (float)(W - 14), 63.5f, 0.8f);
    }

    // === Title "CHUBBYSIEVE" ===
    {
        // Shadow pass
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(28.0f).withStyle("Bold")));
        g.drawText("CHUBBYSIEVE", 2, 8, W, 30, juce::Justification::centred);

        // Main text with amber tint
        juce::ColourGradient titleGrad(juce::Colour(0xfffff0c8), (float)(W / 2 - 60), 8.0f,
                                       juce::Colour(Clr::Amber),       (float)(W / 2 + 60), 36.0f,
                                       false);
        g.setGradientFill(titleGrad);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(28.0f).withStyle("Bold")));
        g.drawText("CHUBBYSIEVE", 0, 7, W, 30, juce::Justification::centred);
    }

    // === Subtitle ===
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f).withStyle("Italic")));
    g.setColour(juce::Colour(Clr::TextSec));
    g.drawText("Chebyshev Harmonic Sculptor  //  Tn(cos th) = cos(n*th)",
               0, 38, W, 18, juce::Justification::centred);

    // === Section panels (drawn behind child components) ===
    paintPanel(g, shapingPanel, "HARMONIC SHAPING");
    paintPanel(g, blendPanel,   "BLEND  &  DYNAMICS");

    // === Bottom formula strip ===
    g.setFont(juce::Font(juce::FontOptions{}
                  .withHeight(8.0f)
                  .withName(juce::Font::getDefaultMonospacedFontName())));
    g.setColour(juce::Colour(Clr::TextDim));
    g.drawText("T2=2x^2-1     T3=4x^3-3x     T4=8x^4-8x^2+1     T5=16x^5-20x^3+5x",
               0, H - 14, W, 12, juce::Justification::centred);
}

//==============================================================================
void ChubbySieveEditor::resized()
{
    const int W  = getWidth();
    const int M  = 14;          // outer margin
    const int GM = 8;           // inner gap between knobs
    const int KH = 102;         // knob component height (includes top+sub labels)
    const int KW = (W - M * 2 - GM * 2) / 3;

    // Panel regions — used in paint() for background panels
    shapingPanel = { M - 4, 70, W - (M - 4) * 2, KH + 14 };
    blendPanel   = { M - 4, 70 + KH + 22, W - (M - 4) * 2, KH + 14 };

    // Row 1: Drive / Warmth / Punch  (harmonic shaping)
    int y = 76;
    driveKnob ->setBounds(M,              y, KW, KH);
    warmthKnob->setBounds(M + KW + GM,    y, KW, KH);
    punchKnob ->setBounds(M + KW * 2 + GM * 2, y, KW, KH);

    // Row 2: Air / Mix / Dynamic  (blend & dynamics)
    y += KH + 28;
    airKnob    ->setBounds(M,                    y, KW, KH);
    mixKnob    ->setBounds(M + KW + GM,          y, KW, KH);
    dynamicKnob->setBounds(M + KW * 2 + GM * 2, y, KW, KH);

    // Row 3: Bass toggle + oversample selector
    y += KH + 22;
    const int controlH = 48;
    bassToggle        ->setBounds(M,                  y, W / 2 - M - 6, controlH);
    oversampleSelector->setBounds(W / 2 + 6,          y, W / 2 - M - 6, controlH);

    // Harmonic meter display
    y += controlH + 10;
    const int meterH = getHeight() - y - 18;  // leave room for formula strip
    harmonicDisplay->setBounds(M, y, W - M * 2, juce::jmax(meterH, 80));
}
