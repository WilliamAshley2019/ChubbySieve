#include "PluginProcessor.h"
#include "PluginEditor.h"

ChubbySieveAudioProcessor::ChubbySieveAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "ChubbySieveState",
        {
            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { Parameters::drive, 1 }, "Drive",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { Parameters::warmth, 1 }, "Warmth",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.3f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { Parameters::punch, 1 }, "Punch",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.2f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { Parameters::air, 1 }, "Air",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.1f),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { Parameters::mix, 1 }, "Mix",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f),

            std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID { Parameters::bassEnhance, 1 }, "Bass Enhance", false),

            std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { Parameters::oversample, 1 }, "Oversampling",
                juce::StringArray { "1x (Fast)", "2x (Balanced)", "4x (Quality)", "8x (Mastering)" }, 1),

            std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { Parameters::dynamicDepth, 1 }, "Dynamic Depth",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f),   // default 0 (off)

                // New parameters — added at version 1 to be consistent with above
                std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID { Parameters::inputGain, 1 }, "Input Gain",
                    juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f),

                std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID { Parameters::outputGain, 1 }, "Output Gain",
                    juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f),

                std::make_unique<juce::AudioParameterFloat>(
                    juce::ParameterID { Parameters::foldback, 1 }, "Foldback",
                    juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f),
        })
{
    // Cache raw parameter pointers — APVTS owns and keeps them current
    pDrive = apvts.getRawParameterValue(Parameters::drive);
    pWarmth = apvts.getRawParameterValue(Parameters::warmth);
    pPunch = apvts.getRawParameterValue(Parameters::punch);
    pAir = apvts.getRawParameterValue(Parameters::air);
    pMix = apvts.getRawParameterValue(Parameters::mix);
    pBassEnhance = apvts.getRawParameterValue(Parameters::bassEnhance);
    pOversample = apvts.getRawParameterValue(Parameters::oversample);
    pDynamicDepth = apvts.getRawParameterValue(Parameters::dynamicDepth);
    pInputGain = apvts.getRawParameterValue(Parameters::inputGain);
    pOutputGain = apvts.getRawParameterValue(Parameters::outputGain);
    pFoldback = apvts.getRawParameterValue(Parameters::foldback);

    // Listen only to the parameters that affect Chebyshev coefficients
    apvts.addParameterListener(Parameters::warmth, this);
    apvts.addParameterListener(Parameters::punch, this);
    apvts.addParameterListener(Parameters::air, this);
}

ChubbySieveAudioProcessor::~ChubbySieveAudioProcessor()
{
    apvts.removeParameterListener(Parameters::warmth, this);
    apvts.removeParameterListener(Parameters::punch, this);
    apvts.removeParameterListener(Parameters::air, this);
}

void ChubbySieveAudioProcessor::parameterChanged(const juce::String&, float)
{
    needsCoeffUpdate.store(true);
}

//==============================================================================
void ChubbySieveAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = static_cast<float>(sampleRate);

    dcBlocker[0].setSampleRate(currentSampleRate);
    dcBlocker[1].setSampleRate(currentSampleRate);
    envFollower.setSampleRate(currentSampleRate);
    envFollower.setTimes(15.0f, 150.0f);
    spectrum.prepare(currentSampleRate);

    dcBlocker[0].reset();
    dcBlocker[1].reset();
    envFollower.reset();

    updateChebyshevCoefficients();
    needsCoeffUpdate.store(false);
    juce::ignoreUnused(samplesPerBlock);
}

void ChubbySieveAudioProcessor::releaseResources()
{
    dcBlocker[0].reset();
    dcBlocker[1].reset();
    envFollower.reset();
    spectrum.reset();
}

bool ChubbySieveAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void ChubbySieveAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (needsCoeffUpdate.exchange(false))
        updateChebyshevCoefficients();

    // Save pre-processing pointer for envelope follower (reads input level)
    const float* const envSrc = buffer.getReadPointer(0);

    // Process each channel
    for (int ch = 0; ch < numIn; ++ch)
        processChannel(buffer.getWritePointer(ch), numSamples, ch);

    // Update envelope follower from the pre-processing signal (channel 0)
    for (int i = 0; i < numSamples; ++i)
        envFollower.process(envSrc[i]);

    // Push processed signal to spectrum analyser (reads post-processing)
    spectrum.pushSamples(buffer.getReadPointer(0),
        buffer.getReadPointer(1),
        numSamples);
}

//==============================================================================
void ChubbySieveAudioProcessor::processChannel(float* data, int numSamples, int ch)
{
    const float wet = getMix();
    const float dry = 1.0f - wet;
    const float dynDepth = getDynamicDepth();
    const bool  bassMode = getBassEnhance();
    const float foldAmt = getFoldback();
    const float inGainLin = juce::Decibels::decibelsToGain(getInputGain());
    const float outGain = juce::Decibels::decibelsToGain(getOutputGain()) * gainComp;

    // Exponential drive curve: 1x at 0 → 4x at 1
    const float driveGain = std::pow(4.0f, getDrive());

    // Foldback threshold: at foldback=0 → 1.0 (no fold before waveshaper clamp)
    //                     at foldback=1 → 0.3 (aggressive fold)
    const float fbThreshold = 1.0f - foldAmt * 0.7f;

    DCBlocker& dc = dcBlocker[static_cast<size_t>(juce::jlimit(0, 1, ch))];

    for (int i = 0; i < numSamples; ++i)
    {
        const float rawIn = data[i] * inGainLin;   // input gain stage

        // ---- Drive ----
        float x = rawIn * driveGain;

        // ---- Foldback clipper (peak-preserving) ----
        // Unlike hard clipping which flattens peaks, foldback reflects the
        // signal back at the threshold — energy is redistributed as additional
        // harmonics rather than lost.  Inspired by diode peak-preservation:
        // the signal "bounces" off the clipping point rather than being cut.
        if (foldAmt > 0.001f)
            x = applyFoldback(x, fbThreshold);

        // ---- Chebyshev waveshaping ----
        float shaped = waveshaper.process(x);   // waveshaper clamps to [-1,1]

        // ---- Dynamic harmonic modulation ----
        // FIX: the original code multiplied the ENTIRE wet signal by the
        // envelope, making quiet passages sound gated.  We now only scale
        // the HARMONIC DIFFERENCE — the extra content the waveshaper added
        // on top of the fundamental — leaving the fundamental always intact.
        if (dynDepth > 0.001f)
        {
            const float env = juce::jlimit(0.0f, 1.0f, envFollower.getEnvelope() * 2.0f);
            const float blend = 1.0f - dynDepth * (1.0f - env);
            // shaped = fundamental + blend * harmonics
            shaped = x + (shaped - x) * blend;
        }

        // Bass enhance: slight harmonic reduction — in a full implementation
        // this would use a crossover to process only sub-120Hz content
        if (bassMode) shaped = x + (shaped - x) * 0.8f;

        // ---- DC block (mandatory after even-order Chebyshev terms) ----
        shaped = dc.process(shaped);

        // ---- Dry/wet mix + output gain ----
        data[i] = (dry * rawIn + wet * shaped) * outGain;
    }
}

//==============================================================================
void ChubbySieveAudioProcessor::updateChebyshevCoefficients()
{
    const float w = getWarmth();
    const float p = getPunch();
    const float a = getAir();

    // c0: DC trim (counteracts even-harmonic offset)
    // c1: fundamental (slightly reduced to make headroom for harmonics)
    // c2: T2 = 2x^2-1    "Warmth" — octave up, tube-like
    // c3: T3 = 4x^3-3x   "Punch"  — 3rd harmonic, transformer character
    // c4: T4 = 8x^4-8x^2+1 "Air"  — 4th harmonic, subtle brightness
    // c5: T5 = 16x^5-20x^3+5x     — 5th harmonic, gentle edge
    std::vector<float> c(6, 0.0f);
    c[0] = -0.02f * (w + a * 0.5f);
    c[1] = 1.0f - (w * 0.10f + p * 0.15f);
    c[2] = w * 0.8f;
    c[3] = p * 0.6f;
    c[4] = a * 0.3f;
    c[5] = a * 0.15f;
    waveshaper.setCoefficients(c);

    // Gain compensation: harmonic generation increases peak level
    gainComp = 1.0f / (1.0f + (w + p + a * 0.5f) * 0.3f);
}

//==============================================================================
void ChubbySieveAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, false);
    apvts.state.writeToStream(mos);
}

void ChubbySieveAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes < 4) return;
    auto tree = juce::ValueTree::readFromData(data, size_t(sizeInBytes));
    if (tree.isValid() && tree.hasType(apvts.state.getType()))
    {
        apvts.replaceState(tree);
        needsCoeffUpdate.store(true);
    }
}

juce::AudioProcessorEditor* ChubbySieveAudioProcessor::createEditor()
{
    return new ChubbySieveEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChubbySieveAudioProcessor();
}