#include "PluginEditor.h"

// ---- Colour palette ----
namespace Clr
{
    constexpr juce::uint32 BgDeep = 0xff0c0c14;
    constexpr juce::uint32 BgPanel = 0xff181824;
    constexpr juce::uint32 Border = 0xff363250;
    constexpr juce::uint32 Amber = 0xffd4921e;
    constexpr juce::uint32 AmberDim = 0xff6a4810;
    constexpr juce::uint32 TextPri = 0xfff2e8d0;
    constexpr juce::uint32 TextSec = 0xff807060;
    constexpr juce::uint32 TextDim = 0xff403830;
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
    label.setText(topLabel, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f).withStyle("Bold")));
    label.setColour(juce::Label::textColourId, juce::Colour(Clr::TextPri));
    addAndMakeVisible(label);

    sublabel.setText(subLabel, juce::dontSendNotification);
    sublabel.setJustificationType(juce::Justification::centred);
    sublabel.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f)));
    sublabel.setColour(juce::Label::textColourId, colour.withAlpha(0.72f));
    addAndMakeVisible(sublabel);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, colour.darker(0.4f));
    slider.setLookAndFeel(&laf);
    addAndMakeVisible(slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, paramId, slider);
}

void ChubbySieveEditor::RotaryKnob::resized()
{
    const int w = getWidth(), h = getHeight();
    const int topH = 15, subH = 12;
    const int kSz = juce::jmin(w, h - topH - subH);
    label.setBounds(0, 0, w, topH);
    slider.setBounds((w - kSz) / 2, topH, kSz, kSz);
    sublabel.setBounds(0, topH + kSz, w, subH);
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
    label.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    label.setColour(juce::Label::textColourId, juce::Colour(Clr::TextSec));
    label.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(label);

    comboBox.addItem("1x  Fast", 1);
    comboBox.addItem("2x  Balanced", 2);
    comboBox.addItem("4x  Quality", 3);
    comboBox.addItem("8x  Mastering", 4);
    comboBox.setLookAndFeel(&laf);
    addAndMakeVisible(comboBox);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        state, paramId, comboBox);
}

void ChubbySieveEditor::OversampleSelector::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(Clr::BgPanel));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
    g.setColour(juce::Colour(Clr::Border));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
}

void ChubbySieveEditor::OversampleSelector::resized()
{
    label.setBounds(6, 3, 56, 14);
    comboBox.setBounds(6, 18, getWidth() - 12, 22);
}

//==============================================================================
// SpectrumDisplay
// Shows the actual FFT spectrum of the processed signal in 4 harmonic bands.
// The bars reflect real audio content — they react to the signal passing
// through the plugin, not just the knob positions.
//==============================================================================
ChubbySieveEditor::SpectrumDisplay::SpectrumDisplay(ChubbySieveAudioProcessor& p)
    : proc(p)
{
}

void ChubbySieveEditor::SpectrumDisplay::paint(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();
    const auto bounds = getLocalBounds().toFloat();

    // ---- Background ----
    g.setColour(juce::Colour(0xff090912));
    g.fillRoundedRectangle(bounds, 5.0f);

    // Grid
    g.setColour(juce::Colour(0xff181824));
    for (int gx = 0; gx < W; gx += 14)
        g.drawLine((float)gx, 0, (float)gx, (float)H, 0.6f);
    for (int gy = 0; gy < H; gy += 12)
        g.drawLine(0, (float)gy, (float)W, (float)gy, 0.6f);

    // ---- Band configuration ----
    struct Band {
        const char* name;
        const char* freq;
        const char* harmonic;
        juce::uint32 colour;
    };
    const Band bands[4] = {
        { "WARMTH",  "80-500 Hz",   "T2  2nd",  0xffe87820 },
        { "PUNCH",   "500-1.5 kHz", "T3  3rd",  0xffd4a018 },
        { "AIR",     "1.5-5 kHz",   "T4  4th",  0xff38a8d0 },
        { "EDGE",    "5-16 kHz",    "T5  5th",  0xffb060e0 }
    };

    const int labelH = 34;
    const int meterH = H - labelH - 6;
    const int numBars = 4;
    const int totalW = W - 20;
    const int barSlot = totalW / numBars;
    const int barW = barSlot - 10;
    const int leftOff = 10;

    for (int i = 0; i < numBars; ++i)
    {
        const float level = proc.getSpectrumBand(i);
        const int   bx = leftOff + i * barSlot;
        const juce::Colour col = juce::Colour(bands[i].colour);

        if (level > 0.005f)
        {
            const int barH = juce::jmax(2, (int)(level * meterH));
            const int by = 4 + meterH - barH;

            // Outer glow
            g.setColour(col.withAlpha(0.12f));
            g.fillRoundedRectangle((float)(bx - 3), (float)by,
                (float)(barW + 6), (float)barH, 4.0f);

            // Bar body — gradient bottom to top
            juce::ColourGradient grad(
                col.darker(0.7f), (float)bx, (float)(by + barH),
                col.brighter(0.2f), (float)bx, (float)by, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle((float)bx, (float)by,
                (float)barW, (float)barH, 3.0f);

            // Bright cap at top
            g.setColour(col.brighter(0.9f).withAlpha(0.85f));
            g.fillRoundedRectangle((float)bx, (float)by,
                (float)barW, 3.5f, 2.0f);

            // Subtle bar outline
            g.setColour(col.withAlpha(0.3f));
            g.drawRoundedRectangle((float)bx, (float)by,
                (float)barW, (float)barH, 3.0f, 0.8f);
        }
        else
        {
            // Empty/ghost state
            g.setColour(juce::Colour(0xff1a1a28));
            g.fillRoundedRectangle((float)bx, 4.0f,
                (float)barW, (float)meterH, 3.0f);
            g.setColour(juce::Colour(0xff282838));
            g.drawRoundedRectangle((float)bx, 4.0f,
                (float)barW, (float)meterH, 3.0f, 0.6f);
        }

        // ---- Labels ----
        const int labelY = 4 + meterH + 4;
        const juce::Colour nameCol = level > 0.005f ? col : juce::Colour(0xff404050);

        g.setColour(nameCol.withAlpha(0.95f));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f).withStyle("Bold")));
        g.drawText(bands[i].name, bx - 4, labelY, barW + 8, 12, juce::Justification::centred);

        g.setColour(juce::Colour(Clr::TextSec));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f)));
        g.drawText(bands[i].harmonic, bx - 4, labelY + 11, barW + 8, 10, juce::Justification::centred);
        g.drawText(bands[i].freq, bx - 4, labelY + 21, barW + 8, 10, juce::Justification::centred);
    }

    // ---- Border ----
    g.setColour(juce::Colour(Clr::Border));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    // ---- "LIVE" indicator ----
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.0f).withStyle("Bold")));
    g.setColour(juce::Colour(0xff40c840).withAlpha(0.8f));
    g.drawText("LIVE SPECTRUM", W - 90, 4, 86, 10, juce::Justification::right);
}

//==============================================================================
// paintPanel helper
//==============================================================================
void ChubbySieveEditor::paintPanel(juce::Graphics& g,
    juce::Rectangle<int> b,
    const juce::String& title) const
{
    g.setColour(juce::Colour(Clr::BgPanel));
    g.fillRoundedRectangle(b.toFloat(), 6.0f);

    g.setColour(juce::Colour(Clr::Border));
    g.drawRoundedRectangle(b.toFloat().reduced(0.5f), 6.0f, 1.0f);

    const int tabW = 120, tabH = 14;
    const int tabX = b.getX() + 10;
    const int tabY = b.getY() - 7;

    g.setColour(juce::Colour(Clr::AmberDim));
    g.fillRoundedRectangle((float)tabX, (float)tabY, (float)tabW, (float)tabH, 3.0f);
    g.setColour(juce::Colour(Clr::Amber));
    g.drawRoundedRectangle((float)tabX, (float)tabY, (float)tabW, (float)tabH, 3.0f, 0.8f);

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f).withStyle("Bold")));
    g.setColour(juce::Colour(Clr::Amber));
    g.drawText(title, tabX, tabY, tabW, tabH, juce::Justification::centred);
}

//==============================================================================
// Main Editor constructor
//==============================================================================
ChubbySieveEditor::ChubbySieveEditor(ChubbySieveAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    using P = ChubbySieveAudioProcessor::Parameters;

    // ---- Row 1: Gain / Drive ----
    inputGainKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::inputGain, "IN GAIN", "dB",
        juce::Colour(0xff8888c8), laf);

    driveKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::drive, "DRIVE", "input",
        juce::Colour(0xffe84820), laf);

    outputGainKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::outputGain, "OUT GAIN", "dB",
        juce::Colour(0xff8888c8), laf);

    // ---- Row 2: Harmonic shaping ----
    warmthKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::warmth, "WARMTH", "T2  2nd",
        juce::Colour(0xffe87820), laf);

    punchKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::punch, "PUNCH", "T3  3rd",
        juce::Colour(0xffd4a018), laf);

    airKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::air, "AIR", "T4/T5 high",
        juce::Colour(0xff38a8d0), laf);

    // ---- Row 3: Processing / output ----
    mixKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::mix, "MIX", "dry/wet",
        juce::Colour(0xffa0a8c8), laf);

    dynamicKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::dynamicDepth, "DYNAMIC", "env depth",
        juce::Colour(0xff48c848), laf);

    foldbackKnob = std::make_unique<RotaryKnob>(
        processor.apvts, P::foldback, "FOLDBACK", "peak fold",
        juce::Colour(0xffcc4888), laf);

    // ---- Controls ----
    bassToggle = std::make_unique<ToggleSwitch>(
        processor.apvts, P::bassEnhance, "Bass Enhance", laf);

    oversampleSelector = std::make_unique<OversampleSelector>(
        processor.apvts, P::oversample, laf);

    // ---- Spectrum display ----
    spectrumDisplay = std::make_unique<SpectrumDisplay>(processor);

    // Add everything
    addAndMakeVisible(inputGainKnob.get());
    addAndMakeVisible(driveKnob.get());
    addAndMakeVisible(outputGainKnob.get());
    addAndMakeVisible(warmthKnob.get());
    addAndMakeVisible(punchKnob.get());
    addAndMakeVisible(airKnob.get());
    addAndMakeVisible(mixKnob.get());
    addAndMakeVisible(dynamicKnob.get());
    addAndMakeVisible(foldbackKnob.get());
    addAndMakeVisible(bassToggle.get());
    addAndMakeVisible(oversampleSelector.get());
    addAndMakeVisible(spectrumDisplay.get());

    // FIX: start the timer unconditionally here in the constructor.
    // The previous version relied on visibilityChanged() which is not
    // reliably called when FL Studio first opens the plugin window.
    // Starting here means the display begins updating immediately.
    spectrumDisplay->startDisplayTimer();

    setSize(520, 590);
    setResizable(false, false);
}

ChubbySieveEditor::~ChubbySieveEditor()
{
    // Stop timer before anything is destroyed
    if (spectrumDisplay) spectrumDisplay->stopDisplayTimer();

    // Reset LookAndFeel pointers before laf is destroyed.
    // Without this, JUCE may try to call laf after it's freed.
    auto resetKnobLAF = [](RotaryKnob* k) { if (k) k->slider.setLookAndFeel(nullptr); };
    resetKnobLAF(inputGainKnob.get());
    resetKnobLAF(driveKnob.get());
    resetKnobLAF(outputGainKnob.get());
    resetKnobLAF(warmthKnob.get());
    resetKnobLAF(punchKnob.get());
    resetKnobLAF(airKnob.get());
    resetKnobLAF(mixKnob.get());
    resetKnobLAF(dynamicKnob.get());
    resetKnobLAF(foldbackKnob.get());
    if (bassToggle)         bassToggle->toggle.setLookAndFeel(nullptr);
    if (oversampleSelector) oversampleSelector->comboBox.setLookAndFeel(nullptr);
}

void ChubbySieveEditor::visibilityChanged()
{
    // Only stop when hidden — the timer was already started in the constructor
    // so we don't need to restart it here. Stopping saves CPU when the window
    // is minimised or the plugin panel is closed.
    if (!isShowing() && spectrumDisplay)
        spectrumDisplay->stopDisplayTimer();
}

//==============================================================================
void ChubbySieveEditor::paint(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();

    // Deep background
    g.setColour(juce::Colour(Clr::BgDeep));
    g.fillAll();

    // Radial vignette
    {
        juce::ColourGradient vig(juce::Colour(0x00000000), W * 0.5f, H * 0.38f,
            juce::Colour(0x50000000), 0.0f, 0.0f, true);
        g.setGradientFill(vig);
        g.fillAll();
    }

    // Header bar
    {
        juce::ColourGradient hdr(juce::Colour(0xff141428), 0, 0,
            juce::Colour(0xff0e0e1c), 0, 64, false);
        g.setGradientFill(hdr);
        g.fillRect(0, 0, W, 64);

        g.setColour(juce::Colour(Clr::Amber).withAlpha(0.7f));
        g.drawLine(14.0f, 64.0f, (float)(W - 14), 64.0f, 1.5f);
        g.setColour(juce::Colour(Clr::Amber).withAlpha(0.12f));
        g.drawLine(14.0f, 65.5f, (float)(W - 14), 65.5f, 0.8f);
    }

    // Title shadow
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(28.0f).withStyle("Bold")));
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawText("CHUBBYSIEVE", 2, 9, W, 30, juce::Justification::centred);

    // Title gradient
    juce::ColourGradient titleGrad(juce::Colour(0xfffff0c8), (float)(W / 2 - 60), 8.0f,
        juce::Colour(Clr::Amber), (float)(W / 2 + 60), 38.0f, false);
    g.setGradientFill(titleGrad);
    g.drawText("CHUBBYSIEVE", 0, 8, W, 30, juce::Justification::centred);

    // Subtitle
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Italic")));
    g.setColour(juce::Colour(Clr::TextSec));
    g.drawText("Chebyshev Harmonic Sculptor  //  Tn(cos th) = cos(n*th)",
        0, 40, W, 18, juce::Justification::centred);

    // Section panels
    paintPanel(g, gainPanel, "GAIN  &  DRIVE");
    paintPanel(g, shapingPanel, "HARMONIC SHAPING");
    paintPanel(g, blendPanel, "BLEND  &  DYNAMICS");

    // Formula strip
    g.setFont(juce::Font(juce::FontOptions{}
        .withHeight(7.5f)
        .withName(juce::Font::getDefaultMonospacedFontName())));
    g.setColour(juce::Colour(Clr::TextDim));
    g.drawText("T2=2x^2-1  |  T3=4x^3-3x  |  T4=8x^4-8x^2+1  |  T5=16x^5-20x^3+5x",
        0, H - 13, W, 11, juce::Justification::centred);
}

//==============================================================================
void ChubbySieveEditor::resized()
{
    const int W = getWidth();
    const int M = 14;          // outer margin
    const int GM = 8;           // gap between knobs
    const int KH = 100;         // knob component height
    const int KW = (W - M * 2 - GM * 2) / 3;
    const int CTH = 44;          // controls row height

    // Store panel rects for paint() — panels sit behind the knobs
    gainPanel = { M - 4, 70,          W - (M - 4) * 2, KH + 14 };
    shapingPanel = { M - 4, 70 + KH + 20, W - (M - 4) * 2, KH + 14 };
    blendPanel = { M - 4, 70 + (KH + 20) * 2, W - (M - 4) * 2, KH + 14 };

    // Row 1: Input Gain / Drive / Output Gain
    int y = 76;
    inputGainKnob->setBounds(M, y, KW, KH);
    driveKnob->setBounds(M + KW + GM, y, KW, KH);
    outputGainKnob->setBounds(M + KW * 2 + GM * 2, y, KW, KH);

    // Row 2: Warmth / Punch / Air
    y += KH + 26;
    warmthKnob->setBounds(M, y, KW, KH);
    punchKnob->setBounds(M + KW + GM, y, KW, KH);
    airKnob->setBounds(M + KW * 2 + GM * 2, y, KW, KH);

    // Row 3: Mix / Dynamic / Foldback
    y += KH + 26;
    mixKnob->setBounds(M, y, KW, KH);
    dynamicKnob->setBounds(M + KW + GM, y, KW, KH);
    foldbackKnob->setBounds(M + KW * 2 + GM * 2, y, KW, KH);

    // Controls: Bass toggle (left) + Oversample selector (right)
    y += KH + 18;
    bassToggle->setBounds(M, y, W / 2 - M - 6, CTH);
    oversampleSelector->setBounds(W / 2 + 6, y, W / 2 - M - 6, CTH);

    // Spectrum display — remaining height above formula strip
    y += CTH + 10;
    const int specH = getHeight() - y - 16;
    spectrumDisplay->setBounds(M, y, W - M * 2, juce::jmax(specH, 80));
}