#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float midiToHz (float note) noexcept { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }

    // PolyBLEP residual for band-limited saw/square
    inline float polyBlep (double t, double dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return (float) (t + t - t * t - 1.0);
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return (float) (t * t + t + t + 1.0);
        }
        return 0.0f;
    }

    inline float sawBlep (double phase, double dt) noexcept
    {
        return (float) (2.0 * phase - 1.0) - polyBlep (phase, dt);
    }

    inline float squareBlep (double phase, double dt) noexcept
    {
        float v = phase < 0.5 ? 1.0f : -1.0f;
        v += polyBlep (phase, dt);
        v -= polyBlep (std::fmod (phase + 0.5, 1.0), dt);
        return v;
    }

    inline void advance (double& phase, double inc) noexcept
    {
        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;
    }

    inline float getVal (std::atomic<float>* p) noexcept { return p->load(); }
}

//==============================================================================
juce::StringArray RollingBassLabProcessor::getStyleNames()
{
    return { "Rolling", "Offbeat", "Bouncy", "Minimal", "Acid", "Disco Octave" };
}

juce::StringArray RollingBassLabProcessor::getKeyNames()
{
    return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

juce::StringArray RollingBassLabProcessor::getWaveNames()
{
    return { "Saw", "Square", "Sine+", "Reese" };
}

juce::StringArray RollingBassLabProcessor::getPresetNames()
{
    return { "Rolling Sub", "Offbeat Punch", "Pluck Roller", "Reese Rumble", "Acid Roll", "Disco Bounce", "Deep Minimal" };
}

juce::AudioProcessorValueTreeState::ParameterLayout RollingBassLabProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto msRange = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    // Pattern
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", getKeyNames(), 5));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "octave", 1 }, "Octave", 0, 3, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", getStyleNames(), 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "density", 1 }, "Density", NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (0.0f, 1.0f), 0.15f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "seqOn", 1 }, "Pattern On", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "preview", 1 }, "Preview", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "previewBpm", 1 }, "Preview BPM", NormalisableRange<float> (110.0f, 140.0f, 1.0f), 126.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gate", 1 }, "Gate", NormalisableRange<float> (0.1f, 1.0f), 0.55f));

    // Synth
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "wave", 1 }, "Wave", getWaveNames(), 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sub", 1 }, "Sub", NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "cutoff", 1 }, "Cutoff", msRange (40.0f, 12000.0f, 600.0f), 320.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "reso", 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.25f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "envAmt", 1 }, "Env Amount", NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "decay", 1 }, "Decay", msRange (20.0f, 1500.0f, 200.0f), 160.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sustain", 1 }, "Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.55f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 1.0f), 0.3f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "glide", 1 }, "Glide", msRange (0.0f, 300.0f, 60.0f), 0.0f));

    // Kick duck
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "duckDepth", 1 }, "Duck Depth", NormalisableRange<float> (0.0f, 1.0f), 0.7f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "duckRelease", 1 }, "Duck Release", msRange (40.0f, 450.0f, 150.0f), 170.0f));

    // Output
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "quiet", 1 }, "Quiet Check", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f), -4.0f));

    return { p.begin(), p.end() };
}

//==============================================================================
RollingBassLabProcessor::RollingBassLabProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pKey         = apvts.getRawParameterValue ("key");
    pOctave      = apvts.getRawParameterValue ("octave");
    pStyle       = apvts.getRawParameterValue ("style");
    pDensity     = apvts.getRawParameterValue ("density");
    pSwing       = apvts.getRawParameterValue ("swing");
    pSeqOn       = apvts.getRawParameterValue ("seqOn");
    pPreview     = apvts.getRawParameterValue ("preview");
    pPreviewBpm  = apvts.getRawParameterValue ("previewBpm");
    pGate        = apvts.getRawParameterValue ("gate");
    pWave        = apvts.getRawParameterValue ("wave");
    pSub         = apvts.getRawParameterValue ("sub");
    pCutoff      = apvts.getRawParameterValue ("cutoff");
    pReso        = apvts.getRawParameterValue ("reso");
    pEnvAmt      = apvts.getRawParameterValue ("envAmt");
    pDecay       = apvts.getRawParameterValue ("decay");
    pSustain     = apvts.getRawParameterValue ("sustain");
    pDrive       = apvts.getRawParameterValue ("drive");
    pGlide       = apvts.getRawParameterValue ("glide");
    pDuckDepth   = apvts.getRawParameterValue ("duckDepth");
    pDuckRelease = apvts.getRawParameterValue ("duckRelease");
    pQuiet       = apvts.getRawParameterValue ("quiet");
    pGain        = apvts.getRawParameterValue ("gain");

    for (auto& s : pattern)
        s.store (BassStep().pack());

    generatePattern();
}

bool RollingBassLabProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RollingBassLabProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate > 0 ? sampleRate : 44100.0;
    ic1eq = ic2eq = 0.0f;
    dcX = dcY = 0.0f;
    ampEnv = filtEnv = 0.0f;
    gateOpen = attacking = false;
    seqNoteActive = false;
    heldNotes.clearQuick();
    gainSmoothed.reset (sr, 0.03);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (getVal (pGain)));
}

int RollingBassLabProcessor::getRootMidiNote() const
{
    // Octave 1 + key F  ->  MIDI 41 (F1 in Ableton naming, ~87 Hz; the sub sits an octave below)
    return 24 + 12 * (int) getVal (pOctave) + (int) getVal (pKey);
}

//==============================================================================
// Voice
void RollingBassLabProcessor::noteOn (int midiNote, float velocity, bool accent, bool legato, float glideMs)
{
    targetFreq = midiToHz ((float) midiNote);

    if (glideMs < 0.5f || ampEnv < 0.0005f)
    {
        currentFreq = targetFreq;
        glideCoeff = 0.0f;
    }
    else
    {
        glideCoeff = std::exp (-1.0f / (glideMs * 0.001f * (float) sr * 0.35f));
    }

    if (! legato)
    {
        if (ampEnv < 0.001f)
        {
            phase1 = phase2 = 0.0;
            phaseSub = 0.0;
        }
        attacking = true;
        filtEnv = 1.0f;
        velocityAmt = velocity;
        accentOn = accent;
    }

    gateOpen = true;
}

void RollingBassLabProcessor::noteOff()
{
    gateOpen = false;
    attacking = false;
}

//==============================================================================
void RollingBassLabProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    // ---- Transport -----------------------------------------------------------
    bool hostPlaying = false;
    double hostPpq = 0.0, bpm = 120.0;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto b = pos->getBpm())
                bpm = *b;
            if (auto q = pos->getPpqPosition())
                hostPpq = *q;
        }
    }

    const bool preview = getVal (pPreview) > 0.5f;
    bool clockRunning = hostPlaying;
    double ppqStart = hostPpq;

    if (! hostPlaying && preview)
    {
        clockRunning = true;
        bpm = getVal (pPreviewBpm);
        ppqStart = previewPpq;
    }
    else if (! hostPlaying)
    {
        previewPpq = 0.0;
    }

    bpm = juce::jlimit (20.0, 400.0, bpm);
    const double ppqPerSample = bpm / 60.0 / sr;
    const double ppqEnd = ppqStart + numSamples * ppqPerSample;

    const bool seqRunning = clockRunning && getVal (pSeqOn) > 0.5f;

    if (! seqRunning && wasRunning)
    {
        if (seqNoteActive)
            noteOff();
        seqNoteActive = false;
        playingStep.store (-1);
    }
    wasRunning = seqRunning;

    // Transport jumped (loop / relocate) -> release the sequencer note
    if (seqRunning && seqNoteActive && std::abs (ppqStart - lastPpqEnd) > 0.05)
    {
        noteOff();
        seqNoteActive = false;
    }

    // ---- Collect step triggers inside this block ------------------------------
    struct Trigger { int sample; int globalStep; double ppq; };
    std::array<Trigger, 64> triggers {};
    int numTriggers = 0;

    const double swingDelay = getVal (pSwing) * 0.5 * 0.25;
    auto stepStartPpq = [swingDelay] (long long n) { return (double) n * 0.25 + ((n & 1) != 0 ? swingDelay : 0.0); };

    if (seqRunning)
    {
        const long long firstN = (long long) std::floor (ppqStart * 4.0) - 1;
        const long long lastN  = (long long) std::floor (ppqEnd * 4.0) + 1;

        for (long long n = firstN; n <= lastN && numTriggers < (int) triggers.size(); ++n)
        {
            const double t = stepStartPpq (n);
            if (t >= ppqStart && t < ppqEnd)
            {
                int s = (int) std::floor ((t - ppqStart) / ppqPerSample);
                triggers[(size_t) numTriggers++] = { juce::jlimit (0, numSamples - 1, s), (int) n, t };
            }
        }
    }

    // ---- Per-block parameter values -------------------------------------------
    const int root = getRootMidiNote();
    const float gate = getVal (pGate);
    const int wave = (int) getVal (pWave);
    const float sub = getVal (pSub);
    const float cutoff = getVal (pCutoff);
    const float reso = getVal (pReso);
    const float envAmt = getVal (pEnvAmt);
    const float decaySec = getVal (pDecay) * 0.001f;
    const float sustain = getVal (pSustain);
    const float drive = getVal (pDrive);
    const float glideMs = getVal (pGlide);
    const float duckDepth = getVal (pDuckDepth);
    const float duckRel = getVal (pDuckRelease) * 0.001f;
    const bool quiet = getVal (pQuiet) > 0.5f;

    float targetGain = juce::Decibels::decibelsToGain (getVal (pGain));
    if (quiet)
        targetGain *= juce::Decibels::decibelsToGain (-18.0f);
    gainSmoothed.setTargetValue (targetGain);

    const float fsr = (float) sr;
    const float ampDecayCoeff  = std::exp (-1.0f / (decaySec * fsr));
    const float filtDecayCoeff = std::exp (-1.0f / (decaySec * 0.8f * fsr));
    const float releaseCoeff   = std::exp (-1.0f / (0.03f * fsr));
    const float attackInc      = 1.0f / (0.0015f * fsr);
    const float k              = 2.0f - 1.85f * reso;           // SVF damping
    const float driveAmt       = 1.0f + drive * 9.0f;
    const float driveNorm      = 1.0f / std::tanh (driveAmt);
    const float dcR            = 1.0f - (twoPi * 15.0f / fsr);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto midiIt = midi.cbegin();
    int trig = 0;
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const double ppq = ppqStart + i * ppqPerSample;

        // ---- MIDI (plays the synth when the pattern isn't running) ----
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
        {
            const auto msg = (*midiIt).getMessage();
            if (! seqRunning)
            {
                if (msg.isNoteOn())
                {
                    const bool legato = ! heldNotes.isEmpty();
                    heldNotes.removeFirstMatchingValue (msg.getNoteNumber());
                    heldNotes.add (msg.getNoteNumber());
                    noteOn (msg.getNoteNumber(), 0.6f + 0.4f * msg.getFloatVelocity(),
                            msg.getVelocity() > 110, legato, legato ? juce::jmax (glideMs, 40.0f) : glideMs);
                }
                else if (msg.isNoteOff())
                {
                    heldNotes.removeFirstMatchingValue (msg.getNoteNumber());
                    if (heldNotes.isEmpty())
                        noteOff();
                    else
                        noteOn (heldNotes.getLast(), velocityAmt, accentOn, true, juce::jmax (glideMs, 40.0f));
                }
                else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                {
                    heldNotes.clearQuick();
                    noteOff();
                }
            }
            ++midiIt;
        }

        // ---- Sequencer ----
        if (seqRunning)
        {
            if (seqNoteActive && ppq >= gateEndPpq)
            {
                noteOff();
                seqNoteActive = false;
            }

            while (trig < numTriggers && triggers[(size_t) trig].sample <= i)
            {
                const auto& t = triggers[(size_t) trig];
                const int idx = (int) (((t.globalStep % numSteps) + numSteps) % numSteps);
                playingStep.store (idx);

                const auto step = getStep (idx);
                if (step.on)
                {
                    const bool legato = seqNoteActive && gateOpen;
                    noteOn (root + step.semis, step.accent ? 1.0f : 0.78f, step.accent, legato,
                            legato ? juce::jmax (glideMs, 55.0f) : glideMs);

                    const auto next = getStep ((idx + 1) % numSteps);
                    if (step.slide && next.on)
                        gateEndPpq = stepStartPpq ((long long) t.globalStep + 1) + 0.02;
                    else
                        gateEndPpq = t.ppq + 0.25 * gate;

                    seqNoteActive = true;
                }
                ++trig;
            }
        }

        // ---- Voice ----
        float out = 0.0f;

        if (ampEnv > 1.0e-5f || gateOpen || attacking)
        {
            currentFreq = targetFreq + (currentFreq - targetFreq) * glideCoeff;

            if (attacking)
            {
                ampEnv += attackInc;
                if (ampEnv >= 1.0f)
                {
                    ampEnv = 1.0f;
                    attacking = false;
                }
            }
            else if (gateOpen)
            {
                ampEnv = sustain + (ampEnv - sustain) * ampDecayCoeff;
            }
            else
            {
                ampEnv *= releaseCoeff;
            }

            filtEnv *= filtDecayCoeff;

            const double inc = (double) currentFreq / sr;
            float osc = 0.0f;

            switch (wave)
            {
                case 0: osc = sawBlep (phase1, inc); break;
                case 1: osc = squareBlep (phase1, inc) * 0.8f; break;
                case 2:
                {
                    const float ph = (float) phase1 * twoPi;
                    osc = std::sin (ph) + 0.35f * std::sin (2.0f * ph) + 0.15f * std::sin (3.0f * ph);
                    break;
                }
                default:
                {
                    const double inc2 = inc * 1.007;
                    osc = 0.6f * (sawBlep (phase1, inc) + sawBlep (phase2, inc2));
                    advance (phase2, inc2);
                    break;
                }
            }
            advance (phase1, inc);

            const float subSig = std::sin ((float) phaseSub * twoPi);
            advance (phaseSub, inc * 0.5);

            float x = osc * (1.0f - 0.45f * sub) + subSig * sub * 1.1f;

            // Filter with envelope
            const float envOct = envAmt * filtEnv * (accentOn ? 6.5f : 5.0f);
            const float fc = juce::jlimit (30.0f, fsr * 0.45f, cutoff * std::exp2 (envOct));
            const float g = std::tan (juce::MathConstants<float>::pi * fc / fsr);
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;
            const float v3 = x - ic2eq;
            const float v1 = a1 * ic1eq + a2 * v3;
            const float v2 = ic2eq + a2 * ic1eq + a3 * v3;
            ic1eq = 2.0f * v1 - ic1eq;
            ic2eq = 2.0f * v2 - ic2eq;
            x = v2;

            // Drive
            x = std::tanh (x * driveAmt) * driveNorm * 0.85f;

            out = x * ampEnv * velocityAmt;
        }

        // DC blocker
        const float dcOut = out - dcX + dcR * dcY;
        dcX = out;
        dcY = dcOut;
        out = dcOut;

        // ---- Kick duck (4-on-the-floor, follows the beat) ----
        if (clockRunning && duckDepth > 0.0f)
        {
            const double beatFrac = ppq - std::floor (ppq);
            const float tSec = (float) (beatFrac * 60.0 / bpm);
            float duckGain = 1.0f;

            if (tSec < 0.002f)
                duckGain = 1.0f - duckDepth * (tSec / 0.002f);
            else if (tSec < duckRel)
            {
                const float r = 1.0f - (tSec - 0.002f) / juce::jmax (0.001f, duckRel - 0.002f);
                duckGain = 1.0f - duckDepth * r * r;
            }
            out *= duckGain;
        }

        out *= gainSmoothed.getNextValue();

        left[i] = out;
        if (right != nullptr)
            right[i] = out;

        peak = juce::jmax (peak, std::abs (out));
    }

    if (clockRunning && ! hostPlaying)
        previewPpq = ppqEnd;
    lastPpqEnd = ppqEnd;

    outputLevel.store (juce::jmax (peak, outputLevel.load() * 0.85f));

    if (! std::isfinite (ic1eq) || ! std::isfinite (ic2eq))
        ic1eq = ic2eq = 0.0f;
}

//==============================================================================
// Pattern generator
void RollingBassLabProcessor::generatePattern()
{
    const int style = (int) getVal (pStyle);
    const float density = getVal (pDensity);
    auto& r = random;
    r.setSeedRandomly();

    std::array<BassStep, numSteps> s {};
    auto chance = [&r] (float p) { return r.nextFloat() < p; };

    // Tech-house friendly note pools (minor key colour)
    auto pickNote = [&] (float rootWeight) -> int
    {
        if (chance (rootWeight)) return 0;
        static const int pool[] = { 12, 7, 10, 3, -2, 5, 12, 0 };
        return pool[r.nextInt (8)];
    };

    switch (style)
    {
        case 0: // Rolling: 16ths that dodge the kick
        {
            for (int i = 0; i < numSteps; ++i)
            {
                const bool kickStep = (i % 4) == 0;
                if (kickStep)
                    continue;
                const bool strong = (i % 4) == 2;
                s[(size_t) i].on = strong ? chance (0.9f) : chance (0.35f + density * 0.6f);
                s[(size_t) i].semis = pickNote (0.75f);
                s[(size_t) i].accent = strong && chance (0.5f);
            }
            if (chance (0.5f)) { s[15].on = true; s[15].semis = chance (0.5f) ? 12 : -2; }
            break;
        }
        case 1: // Offbeat: hits on the "and"
        {
            for (int i = 2; i < numSteps; i += 4)
            {
                s[(size_t) i].on = true;
                s[(size_t) i].accent = true;
                s[(size_t) i].semis = (i == 14 && chance (0.4f)) ? pickNote (0.3f) : 0;
            }
            for (int i = 3; i < numSteps; i += 4)
            {
                if (chance (density * 0.6f))
                {
                    s[(size_t) i].on = true;
                    s[(size_t) i].semis = chance (0.5f) ? 0 : 12;
                }
            }
            break;
        }
        case 2: // Bouncy: pairs off the kick with octave lifts
        {
            for (int beat = 0; beat < 4; ++beat)
            {
                const int a = beat * 4 + 2, b = beat * 4 + 3;
                s[(size_t) a].on = true;
                s[(size_t) a].accent = chance (0.6f);
                s[(size_t) a].semis = (beat == 3 && chance (0.5f)) ? pickNote (0.2f) : 0;
                if (chance (0.4f + density * 0.5f))
                {
                    s[(size_t) b].on = true;
                    s[(size_t) b].semis = chance (0.6f) ? 12 : s[(size_t) a].semis;
                }
                if (chance (density * 0.35f))
                {
                    s[(size_t) (beat * 4 + 1)].on = true;
                    s[(size_t) (beat * 4 + 1)].semis = 0;
                }
            }
            break;
        }
        case 3: // Minimal: a few long, deep notes
        {
            const int starts[] = { 2, 3, 6 };
            const int first = starts[r.nextInt (3)];
            s[(size_t) first].on = true;
            s[(size_t) first].accent = true;
            s[(size_t) first].slide = chance (0.3f);
            const int second = 10 + r.nextInt (2);
            s[(size_t) second].on = true;
            s[(size_t) second].semis = chance (0.7f) ? 0 : pickNote (0.0f);
            if (chance (density))
            {
                s[14].on = true;
                s[14].semis = chance (0.5f) ? 0 : -2;
            }
            break;
        }
        case 4: // Acid: busy 16ths, slides and accents
        {
            for (int i = 0; i < numSteps; ++i)
            {
                s[(size_t) i].on = chance (0.45f + density * 0.45f) && ! ((i % 4) == 0 && chance (0.6f));
                s[(size_t) i].semis = pickNote (0.45f);
                s[(size_t) i].accent = chance (0.3f);
                s[(size_t) i].slide = chance (0.25f);
            }
            s[2].on = true;
            break;
        }
        default: // Disco Octave: fast offbeat octave bounce
        {
            const int shift = chance (0.5f) ? 5 : (chance (0.5f) ? -2 : 3);
            for (int i = 0; i < numSteps; ++i)
            {
                const int pos = i % 4;
                const int base = (i >= 8 && chance (0.7f)) ? shift : 0;
                if (pos == 2)        { s[(size_t) i].on = true; s[(size_t) i].semis = base; s[(size_t) i].accent = true; }
                else if (pos == 3)   { s[(size_t) i].on = chance (0.5f + density * 0.5f); s[(size_t) i].semis = base + 12; }
                else if (pos == 1)   { s[(size_t) i].on = chance (density * 0.4f); s[(size_t) i].semis = base + 12; }
            }
            break;
        }
    }

    // Slides only make sense if the next step plays
    for (int i = 0; i < numSteps; ++i)
        if (s[(size_t) i].slide && ! s[(size_t) ((i + 1) % numSteps)].on)
            s[(size_t) i].slide = false;

    for (int i = 0; i < numSteps; ++i)
        setStep (i, s[(size_t) i]);
}

juce::String RollingBassLabProcessor::patternToString() const
{
    juce::StringArray parts;
    for (int i = 0; i < numSteps; ++i)
        parts.add (juce::String (pattern[(size_t) i].load()));
    return parts.joinIntoString (",");
}

void RollingBassLabProcessor::patternFromString (const juce::String& str)
{
    juce::StringArray parts;
    parts.addTokens (str, ",", "");
    if (parts.size() != numSteps)
        return;
    for (int i = 0; i < numSteps; ++i)
        pattern[(size_t) i].store (parts[i].getIntValue());
}

//==============================================================================
juce::File RollingBassLabProcessor::writePatternMidiFile() const
{
    constexpr int ppqTicks = 960;
    constexpr int stepTicks = ppqTicks / 4;
    const double swingTicks = getVal (pSwing) * 0.5 * stepTicks;
    const float gate = getVal (pGate);
    const int root = getRootMidiNote();

    juce::MidiMessageSequence seq;

    for (int bar = 0; bar < 4; ++bar)
    {
        for (int i = 0; i < numSteps; ++i)
        {
            const auto s = getStep (i);
            if (! s.on)
                continue;

            const int n = bar * numSteps + i;
            const double start = n * stepTicks + ((i & 1) != 0 ? swingTicks : 0.0);
            double end;
            if (s.slide && getStep ((i + 1) % numSteps).on)
            {
                const int nn = n + 1;
                end = nn * stepTicks + ((nn & 1) != 0 ? swingTicks : 0.0) + 20.0;
            }
            else
            {
                end = start + stepTicks * gate;
            }

            const int note = juce::jlimit (0, 127, root + s.semis);
            const auto vel = (juce::uint8) (s.accent ? 127 : 100);
            seq.addEvent (juce::MidiMessage::noteOn (1, note, vel), start);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), end);
        }
    }

    seq.updateMatchedPairs();
    seq.addEvent (juce::MidiMessage::endOfTrack(), 4.0 * numSteps * stepTicks);

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppqTicks);
    mf.addTrack (seq);

    const auto keyName = getKeyNames()[(int) getVal (pKey)].replace ("#", "s");
    const auto styleName = getStyleNames()[(int) getVal (pStyle)].removeCharacters (" ");
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("RollingBassLab_" + styleName + "_" + keyName + ".mid");

    file.deleteFile();
    if (auto stream = file.createOutputStream())
        mf.writeTo (*stream);

    return file;
}

//==============================================================================
void RollingBassLabProcessor::applyPreset (int index)
{
    struct P { int wave; float sub, cutoff, reso, envAmt, decay, sustain, drive, glide, gate, duck, duckRel; int style; };

    static const P presets[] = {
        // wave sub   cutoff reso  env   decay sus   drive glide gate  duck rel  style
        { 0, 0.65f, 260.f, 0.20f, 0.45f, 180.f, 0.55f, 0.25f, 0.f,  0.55f, 0.7f, 170.f, 0 }, // Rolling Sub
        { 1, 0.55f, 380.f, 0.30f, 0.55f, 120.f, 0.35f, 0.45f, 0.f,  0.45f, 0.8f, 150.f, 1 }, // Offbeat Punch
        { 0, 0.40f, 220.f, 0.45f, 0.75f,  90.f, 0.10f, 0.35f, 0.f,  0.40f, 0.6f, 140.f, 2 }, // Pluck Roller
        { 3, 0.50f, 420.f, 0.15f, 0.30f, 400.f, 0.70f, 0.55f, 20.f, 0.75f, 0.7f, 200.f, 3 }, // Reese Rumble
        { 0, 0.30f, 300.f, 0.75f, 0.80f, 200.f, 0.30f, 0.60f, 0.f,  0.50f, 0.6f, 150.f, 4 }, // Acid Roll
        { 1, 0.45f, 700.f, 0.35f, 0.50f, 110.f, 0.30f, 0.30f, 0.f,  0.40f, 0.6f, 130.f, 5 }, // Disco Bounce
        { 2, 0.75f, 180.f, 0.10f, 0.25f, 600.f, 0.80f, 0.15f, 30.f, 0.85f, 0.5f, 220.f, 3 }, // Deep Minimal
    };

    if (index < 0 || index >= (int) std::size (presets))
        return;

    const auto& p = presets[index];
    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set ("wave", (float) p.wave);
    set ("sub", p.sub);
    set ("cutoff", p.cutoff);
    set ("reso", p.reso);
    set ("envAmt", p.envAmt);
    set ("decay", p.decay);
    set ("sustain", p.sustain);
    set ("drive", p.drive);
    set ("glide", p.glide);
    set ("gate", p.gate);
    set ("duckDepth", p.duck);
    set ("duckRelease", p.duckRel);
    set ("style", (float) p.style);

    generatePattern();
}

//==============================================================================
void RollingBassLabProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", patternToString(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RollingBassLabProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const auto pat = tree.getProperty ("pattern").toString();
            apvts.replaceState (tree);
            if (pat.isNotEmpty())
                patternFromString (pat);
        }
    }
}

juce::AudioProcessorEditor* RollingBassLabProcessor::createEditor()
{
    return new RollingBassLabEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RollingBassLabProcessor();
}
