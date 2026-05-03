#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>

//==============================================================================
// ChebyshevWaveshaper (unchanged)
//==============================================================================
class ChebyshevWaveshaper
{
public:
    ChebyshevWaveshaper() = default;
    void setCoefficients(const std::vector<float>& c)
    {
        coefficients = c;
        order = static_cast<int>(c.size()) - 1;
    }
    inline float process(float x) noexcept
    {
        x = juce::jlimit(-1.0f, 1.0f, x);
        if (coefficients.empty()) return x;
        float b1 = 0.0f, b2 = 0.0f;
        for (int k = order; k >= 1; --k)
        {
            float b0 = 2.0f * x * b1 - b2 + coefficients[static_cast<size_t>(k)];
            b2 = b1;
            b1 = b0;
        }
        return x * b1 - b2 + coefficients[0];
    }
private:
    std::vector<float> coefficients{ 0.0f, 1.0f, 0.0f, 0.0f };
    int order{ 3 };
};

//==============================================================================
// DCBlocker (unchanged)
//==============================================================================
class DCBlocker
{
public:
    DCBlocker() = default;
    void setSampleRate(float sr) noexcept
    {
        R = std::exp(-2.0f * juce::MathConstants<float>::pi * 7.5f / sr);
        R = juce::jlimit(0.0f, 0.999f, R);
    }
    inline float process(float x) noexcept
    {
        const float y = x - x1 + R * y1;
        x1 = x;  y1 = y;
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
private:
    float R{ 0.995f };
    float x1{ 0.0f }, y1{ 0.0f };
};

//==============================================================================
// EnvelopeFollower – fixed parameter name to avoid hiding member 'sr'
//==============================================================================
class EnvelopeFollower
{
public:
    EnvelopeFollower() = default;
    void setSampleRate(float newSampleRate) noexcept
    {
        sr = newSampleRate;
        updateCoeffs();
    }
    void setTimes(float atkMs, float relMs) noexcept
    {
        attackMs = atkMs; releaseMs = relMs;
        updateCoeffs();
    }
    inline float process(float in) noexcept
    {
        const float t = std::abs(in);
        const float c = (t > env) ? atkCoeff : relCoeff;
        env = c * env + (1.0f - c) * t;
        return env;
    }
    float getEnvelope() const noexcept { return env; }
    void reset() noexcept { env = 0.0f; }
private:
    void updateCoeffs() noexcept
    {
        atkCoeff = std::exp(-1.0f / (attackMs * 0.001f * sr));
        relCoeff = std::exp(-1.0f / (releaseMs * 0.001f * sr));
    }
    float sr{ 44100.0f };
    float attackMs{ 15.0f }, releaseMs{ 150.0f };
    float atkCoeff{ 0.9f }, relCoeff{ 0.99f };
    float env{ 0.0f };
};

//==============================================================================
// Processor (unchanged except includes)
//==============================================================================
class ChubbySieveAudioProcessor : public juce::AudioProcessor,
    private juce::AudioProcessorValueTreeState::Listener
{
public:
    ChubbySieveAudioProcessor();
    ~ChubbySieveAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ChubbySieve"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter IDs
    struct Parameters
    {
        static constexpr const char* drive = "drive";
        static constexpr const char* warmth = "warmth";
        static constexpr const char* punch = "punch";
        static constexpr const char* air = "air";
        static constexpr const char* mix = "mix";
        static constexpr const char* bassEnhance = "bassEnhance";
        static constexpr const char* oversample = "oversample";
        static constexpr const char* dynamicDepth = "dynamicDepth";
    };

    // Parameter getters (for editor)
    float getDrive()        const noexcept { return pDrive ? pDrive->load() : 0.5f; }
    float getWarmth()       const noexcept { return pWarmth ? pWarmth->load() : 0.3f; }
    float getPunch()        const noexcept { return pPunch ? pPunch->load() : 0.2f; }
    float getAir()          const noexcept { return pAir ? pAir->load() : 0.1f; }
    float getMix()          const noexcept { return pMix ? pMix->load() : 0.8f; }
    float getDynamicDepth() const noexcept { return pDynamicDepth ? pDynamicDepth->load() : 0.5f; }
    bool  getBassEnhance()  const noexcept { return pBassEnhance ? (*pBassEnhance > 0.5f) : false; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void parameterChanged(const juce::String& paramID, float) override;
    void updateChebyshevCoefficients();
    void processChannel(float* data, int numSamples, int channel);

    // DSP objects
    ChebyshevWaveshaper waveshaper;
    std::array<DCBlocker, 2> dcBlocker;
    EnvelopeFollower envFollower;

    // Atomic parameter pointers
    std::atomic<float>* pDrive{ nullptr };
    std::atomic<float>* pWarmth{ nullptr };
    std::atomic<float>* pPunch{ nullptr };
    std::atomic<float>* pAir{ nullptr };
    std::atomic<float>* pMix{ nullptr };
    std::atomic<float>* pBassEnhance{ nullptr };
    std::atomic<float>* pOversample{ nullptr };
    std::atomic<float>* pDynamicDepth{ nullptr };

    float currentSampleRate{ 44100.0f };
    float gainComp{ 1.0f };
    std::atomic<bool> needsCoeffUpdate{ true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChubbySieveAudioProcessor)
};