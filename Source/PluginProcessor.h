#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
// ChebyshevWaveshaper — Clenshaw backward recurrence
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
            b2 = b1;  b1 = b0;
        }
        return x * b1 - b2 + coefficients[0];
    }
private:
    std::vector<float> coefficients{ 0.0f, 1.0f, 0.0f, 0.0f };
    int order{ 3 };
};

//==============================================================================
// DCBlocker — H(z) = (1-z^-1)/(1-R*z^-1),  fc ~7.5 Hz
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
        x1 = x;  y1 = y;  return y;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
private:
    float R{ 0.995f }, x1{ 0.0f }, y1{ 0.0f };
};

//==============================================================================
// EnvelopeFollower
//==============================================================================
class EnvelopeFollower
{
public:
    EnvelopeFollower() = default;
    void setSampleRate(float newSR) noexcept { sr = newSR; updateCoeffs(); }
    void setTimes(float atkMs, float relMs) noexcept
    {
        attackMs = atkMs; releaseMs = relMs; updateCoeffs();
    }
    inline float process(float in) noexcept
    {
        const float t = std::abs(in);
        const float c = (t > env) ? atkCoeff : relCoeff;
        env = c * env + (1.0f - c) * t;
        return env;
    }
    float getEnvelope() const noexcept { return env; }
    void  reset() noexcept { env = 0.0f; }
private:
    void updateCoeffs() noexcept
    {
        atkCoeff = std::exp(-1.0f / (attackMs * 0.001f * sr));
        relCoeff = std::exp(-1.0f / (releaseMs * 0.001f * sr));
    }
    float sr{ 44100.0f }, attackMs{ 15.0f }, releaseMs{ 150.0f };
    float atkCoeff{ 0.9f }, relCoeff{ 0.99f }, env{ 0.0f };
};

//==============================================================================
// SpectrumAnalyser
//
// Runs a 1024-point FFT on the processed signal (audio thread) and exposes
// 4 smoothed band levels as atomic floats for the editor to read.
//
// Bands loosely map to where the Chebyshev harmonics land on typical audio:
//   Band 0  80 – 500 Hz    (Warmth / T2 region)
//   Band 1  500 – 1500 Hz  (Punch  / T3 region)
//   Band 2  1500 – 5000 Hz (Air    / T4 region)
//   Band 3  5000 – 16000 Hz(Edge   / T5 region)
//==============================================================================
class SpectrumAnalyser
{
public:
    static constexpr int fftOrder = 10;             // 2^10 = 1024
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBands = 4;

    SpectrumAnalyser() : fft(fftOrder)
    {
        // Hann window
        for (int i = 0; i < fftSize; ++i)
            window[i] = 0.5f * (1.0f - std::cos(
                2.0f * juce::MathConstants<float>::pi * i / float(fftSize - 1)));
        for (auto& b : bandLevels) b.store(0.0f);
    }

    void prepare(float sampleRate) noexcept
    {
        sr = sampleRate;
        fifoIdx = 0;
        std::fill(fifo, fifo + fftSize, 0.0f);
        std::fill(fftBuf, fftBuf + fftSize * 2, 0.0f);
        for (auto& b : bandLevels) b.store(0.0f);
    }

    // Called from the audio thread — push a mono mix of L and R
    void pushSamples(const float* L, const float* R, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            fifo[fifoIdx++] = (L[i] + R[i]) * 0.5f;
            if (fifoIdx >= fftSize)
            {
                fifoIdx = 0;
                runFFT();
            }
        }
    }

    // Called from the message thread (safe — reads only atomic values)
    float getBandLevel(int band) const noexcept
    {
        return (band >= 0 && band < numBands) ? bandLevels[band].load() : 0.0f;
    }

    void reset() noexcept
    {
        fifoIdx = 0;
        std::fill(fifo, fifo + fftSize, 0.0f);
        for (auto& b : bandLevels) b.store(0.0f);
    }

private:
    void runFFT() noexcept
    {
        // Apply Hann window and copy into FFT buffer
        for (int i = 0; i < fftSize; ++i)
            fftBuf[i] = fifo[i] * window[i];
        std::fill(fftBuf + fftSize, fftBuf + fftSize * 2, 0.0f);

        // performFrequencyOnlyForwardTransform: takes 2*fftSize buffer,
        // writes magnitudes into positions 0 … fftSize/2-1
        fft.performFrequencyOnlyForwardTransform(fftBuf);

        // Band edge frequencies
        const float binHz = sr / float(fftSize);
        const float edges[numBands + 1] = { 80.0f, 500.0f, 1500.0f, 5000.0f, 16000.0f };

        for (int b = 0; b < numBands; ++b)
        {
            const int lo = juce::jmax(1, (int)(edges[b] / binHz));
            const int hi = juce::jmin(fftSize / 2, (int)(edges[b + 1] / binHz));

            // Peak in this band
            float peak = 0.0f;
            for (int i = lo; i < hi; ++i)
                peak = juce::jmax(peak, fftBuf[i]);

            // Normalise → dB → 0–1 display range
            const float normalised = peak / float(fftSize);
            const float dB = juce::Decibels::gainToDecibels(normalised + 1e-7f);
            const float lvl = juce::jlimit(0.0f, 1.0f,
                juce::jmap(dB, -72.0f, -12.0f, 0.0f, 1.0f));

            // Smooth: fast attack, slow decay
            const float prev = bandLevels[b].load();
            bandLevels[b].store(lvl > prev ? lvl
                : prev * 0.90f + lvl * 0.10f);
        }
    }

    juce::dsp::FFT fft;
    float fifo[fftSize]{};
    float fftBuf[fftSize * 2]{};
    float window[fftSize]{};
    int   fifoIdx{ 0 };
    float sr{ 44100.0f };
    std::atomic<float> bandLevels[numBands]{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyser)
};

//==============================================================================
// ChubbySieveAudioProcessor
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
    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ---- Parameter ID strings ----
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
        static constexpr const char* inputGain = "inputGain";    // dB, -24..+24
        static constexpr const char* outputGain = "outputGain";   // dB, -24..+24
        static constexpr const char* foldback = "foldback";     // 0..1
    };

    // ---- Thread-safe parameter reads ----
    float getDrive()        const noexcept { return pDrive ? pDrive->load() : 0.5f; }
    float getWarmth()       const noexcept { return pWarmth ? pWarmth->load() : 0.3f; }
    float getPunch()        const noexcept { return pPunch ? pPunch->load() : 0.2f; }
    float getAir()          const noexcept { return pAir ? pAir->load() : 0.1f; }
    float getMix()          const noexcept { return pMix ? pMix->load() : 0.8f; }
    float getDynamicDepth() const noexcept { return pDynamicDepth ? pDynamicDepth->load() : 0.5f; }
    float getInputGain()    const noexcept { return pInputGain ? pInputGain->load() : 0.0f; }
    float getOutputGain()   const noexcept { return pOutputGain ? pOutputGain->load() : 0.0f; }
    float getFoldback()     const noexcept { return pFoldback ? pFoldback->load() : 0.0f; }
    bool  getBassEnhance()  const noexcept { return pBassEnhance ? (*pBassEnhance > 0.5f) : false; }

    // ---- Spectrum data for the editor ----
    float getSpectrumBand(int band) const noexcept { return spectrum.getBandLevel(band); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void parameterChanged(const juce::String& paramID, float) override;
    void updateChebyshevCoefficients();
    void processChannel(float* data, int numSamples, int ch);

    // Foldback clipper — reflects signal at threshold, preserving energy
    // rather than hard-clipping it flat.
    static float applyFoldback(float x, float threshold) noexcept
    {
        // One reflection pass is sufficient for the drive levels we use.
        // A second pass handles the case where driveGain pushes x beyond 2*threshold.
        if (x > threshold) x = 2.0f * threshold - x;
        if (x < -threshold) x = -2.0f * threshold - x;
        // Clamp in case x was beyond 2*threshold (deep foldback)
        return juce::jlimit(-threshold, threshold, x);
    }

    // DSP objects
    ChebyshevWaveshaper      waveshaper;
    std::array<DCBlocker, 2> dcBlocker;
    EnvelopeFollower         envFollower;
    SpectrumAnalyser         spectrum;

    // Raw APVTS parameter pointers (valid for plugin lifetime)
    std::atomic<float>* pDrive{ nullptr };
    std::atomic<float>* pWarmth{ nullptr };
    std::atomic<float>* pPunch{ nullptr };
    std::atomic<float>* pAir{ nullptr };
    std::atomic<float>* pMix{ nullptr };
    std::atomic<float>* pBassEnhance{ nullptr };
    std::atomic<float>* pOversample{ nullptr };
    std::atomic<float>* pDynamicDepth{ nullptr };
    std::atomic<float>* pInputGain{ nullptr };
    std::atomic<float>* pOutputGain{ nullptr };
    std::atomic<float>* pFoldback{ nullptr };

    float currentSampleRate{ 44100.0f };
    float gainComp{ 1.0f };
    std::atomic<bool> needsCoeffUpdate{ true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChubbySieveAudioProcessor)
};