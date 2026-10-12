#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

//==============================================================================
// One step of the 16-step bass pattern, packed into an int so the UI thread and
// the audio thread can share it lock-free.
struct BassStep
{
    bool on = false;
    bool accent = false;
    bool slide = false;
    int semis = 0; // semitones above the root (-12 .. +24)

    int pack() const noexcept
    {
        return (on ? 1 : 0) | (accent ? 2 : 0) | (slide ? 4 : 0) | ((juce::jlimit (-12, 24, semis) + 32) << 3);
    }

    static BassStep unpack (int v) noexcept
    {
        BassStep s;
        s.on = (v & 1) != 0;
        s.accent = (v & 2) != 0;
        s.slide = (v & 4) != 0;
        s.semis = ((v >> 3) & 0xff) - 32;
        return s;
    }
};

//==============================================================================
class RollingBassLabProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numSteps = 16;

    RollingBassLabProcessor();
    ~RollingBassLabProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Rolling Bass Lab"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Pattern access (message thread)
    BassStep getStep (int i) const noexcept { return BassStep::unpack (pattern[(size_t) i].load()); }
    void setStep (int i, const BassStep& s) noexcept { pattern[(size_t) i].store (s.pack()); }
    void generatePattern();
    juce::String patternToString() const;
    void patternFromString (const juce::String&);

    // Builds a 4-bar MIDI file of the current pattern and returns it (temp folder)
    juce::File writePatternMidiFile() const;

    int getRootMidiNote() const;
    int getPlayingStep() const noexcept { return playingStep.load(); }
    float getOutputLevel() const noexcept { return outputLevel.load(); }

    void applyPreset (int index);
    static juce::StringArray getPresetNames();
    static juce::StringArray getStyleNames();
    static juce::StringArray getKeyNames();
    static juce::StringArray getWaveNames();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    //==========================================================================
    // Voice
    void noteOn (int midiNote, float velocity, bool accent, bool legato, float glideMs);
    void noteOff();
    float renderSample();

    double sr = 44100.0;

    float currentFreq = 55.0f, targetFreq = 55.0f, glideCoeff = 0.0f;
    double phase1 = 0.0, phase2 = 0.0, phaseSub = 0.0;
    float ampEnv = 0.0f, filtEnv = 0.0f;
    bool gateOpen = false;
    float velocityAmt = 1.0f;
    bool accentOn = false;
    float ampAttackStage = 0.0f;
    bool attacking = false;

    // Filter state (Cytomic / TPT SVF)
    float ic1eq = 0.0f, ic2eq = 0.0f;
    float dcX = 0.0f, dcY = 0.0f;

    // MIDI-play note stack (when the sequencer isn't running)
    juce::Array<int> heldNotes;

    //==========================================================================
    // Sequencer
    std::array<std::atomic<int>, numSteps> pattern;
    std::atomic<int> playingStep { -1 };
    std::atomic<float> outputLevel { 0.0f };

    double previewPpq = 0.0;
    double gateEndPpq = -1.0;
    double lastPpqEnd = 0.0;
    bool seqNoteActive = false;
    bool wasRunning = false;

    juce::Random random;

    // Cached parameter pointers
    std::atomic<float>* pKey = nullptr;
    std::atomic<float>* pOctave = nullptr;
    std::atomic<float>* pStyle = nullptr;
    std::atomic<float>* pDensity = nullptr;
    std::atomic<float>* pSwing = nullptr;
    std::atomic<float>* pSeqOn = nullptr;
    std::atomic<float>* pPreview = nullptr;
    std::atomic<float>* pPreviewBpm = nullptr;
    std::atomic<float>* pGate = nullptr;
    std::atomic<float>* pWave = nullptr;
    std::atomic<float>* pSub = nullptr;
    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pReso = nullptr;
    std::atomic<float>* pEnvAmt = nullptr;
    std::atomic<float>* pDecay = nullptr;
    std::atomic<float>* pSustain = nullptr;
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pGlide = nullptr;
    std::atomic<float>* pDuckDepth = nullptr;
    std::atomic<float>* pDuckRelease = nullptr;
    std::atomic<float>* pQuiet = nullptr;
    std::atomic<float>* pGain = nullptr;

    juce::SmoothedValue<float> gainSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollingBassLabProcessor)
};
