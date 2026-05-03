#include "PluginProcessor.h"
#include "PluginEditor.h"   // include the custom editor

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
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f),
        })
{
    pDrive = apvts.getRawParameterValue(Parameters::drive);
    pWarmth = apvts.getRawParameterValue(Parameters::warmth);
    pPunch = apvts.getRawParameterValue(Parameters::punch);
    pAir = apvts.getRawParameterValue(Parameters::air);
    pMix = apvts.getRawParameterValue(Parameters::mix);
    pBassEnhance = apvts.getRawParameterValue(Parameters::bassEnhance);
    pOversample = apvts.getRawParameterValue(Parameters::oversample);
    pDynamicDepth = apvts.getRawParameterValue(Parameters::dynamicDepth);

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

void ChubbySieveAudioProcessor::parameterChanged(const juce::String& paramID, float)
{
    needsCoeffUpdate.store(true);
}

void ChubbySieveAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = static_cast<float>(sampleRate);
    dcBlocker[0].setSampleRate(currentSampleRate);
    dcBlocker[1].setSampleRate(currentSampleRate);
    envFollower.setSampleRate(currentSampleRate);
    envFollower.setTimes(15.0f, 150.0f);
    dcBlocker[0].reset();
    dcBlocker[1].reset();
    envFollower.reset();
    updateChebyshevCoefficients();
    needsCoeffUpdate.store(false);
}

void ChubbySieveAudioProcessor::releaseResources()
{
    dcBlocker[0].reset();
    dcBlocker[1].reset();
    envFollower.reset();
}

bool ChubbySieveAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ChubbySieveAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear(ch, 0, numSamples);

    if (needsCoeffUpdate.exchange(false))
        updateChebyshevCoefficients();

    const float* envSrc = buffer.getReadPointer(0);
    for (int ch = 0; ch < numIn; ++ch)
        processChannel(buffer.getWritePointer(ch), numSamples, ch);

    for (int i = 0; i < numSamples; ++i)
        envFollower.process(envSrc[i]);
}

void ChubbySieveAudioProcessor::processChannel(float* data, int numSamples, int ch)
{
    const float drive = getDrive();
    const float wet = getMix();
    const float dry = 1.0f - wet;
    const float dynDepth = getDynamicDepth();
    const bool bassMode = getBassEnhance();
    const float driveGain = std::pow(4.0f, drive);
    DCBlocker& dc = dcBlocker[static_cast<size_t>(juce::jlimit(0, 1, ch))];

    for (int i = 0; i < numSamples; ++i)
    {
        const float in = data[i];
        float x = in * driveGain;
        float dynMod = 1.0f;
        if (dynDepth > 0.001f)
        {
            const float env = envFollower.getEnvelope();
            dynMod = 1.0f - dynDepth + dynDepth * juce::jlimit(0.0f, 1.0f, env * 2.0f);
        }
        if (bassMode) dynMod *= 0.8f;
        float shaped = waveshaper.process(x) * dynMod;
        shaped = dc.process(shaped);
        data[i] = (dry * in + wet * shaped) * gainComp;
    }
}

void ChubbySieveAudioProcessor::updateChebyshevCoefficients()
{
    const float w = getWarmth();
    const float p = getPunch();
    const float a = getAir();
    std::vector<float> c(6, 0.0f);
    c[0] = -0.02f * (w + a * 0.5f);
    c[1] = 1.0f - (w * 0.10f + p * 0.15f);
    c[2] = w * 0.8f;
    c[3] = p * 0.6f;
    c[4] = a * 0.3f;
    c[5] = a * 0.15f;
    waveshaper.setCoefficients(c);
    gainComp = 1.0f / (1.0f + (w + p + a * 0.5f) * 0.3f);
}

void ChubbySieveAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state = apvts.copyState();
    juce::MemoryOutputStream mos(destData, false);
    state.writeToStream(mos);
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