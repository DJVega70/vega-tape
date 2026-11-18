#include "PluginProcessor.h"
#include "PluginEditor.h"

// =====================================================================
//  Factory presets  (anything not listed uses the parameter's default)
// =====================================================================
namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

    const std::vector<Preset>& factoryPresets()
    {
        static const std::vector<Preset> list
        {
            { "Init", {} },
            { "Warm Vocal",      { { ID::formula, 0 }, { ID::speed, 1 }, { ID::bias, 1 }, { ID::sat, 40 },
                                   { ID::wowDepth, 8 }, { ID::flutterDepth, 8 } } },
            { "Drum Bus Glue",   { { ID::formula, 1 }, { ID::speed, 1 }, { ID::bias, 0 }, { ID::input, 4 },
                                   { ID::levelLink, 1 }, { ID::sat, 55 }, { ID::wowDepth, 4 }, { ID::flutterDepth, 6 } } },
            { "Soulful Keys",    { { ID::formula, 0 }, { ID::speed, 0 }, { ID::bias, 1 }, { ID::sat, 45 },
                                   { ID::wowRate, 0.6f }, { ID::wowDepth, 22 }, { ID::flutterDepth, 14 } } },
            { "Master Tape",     { { ID::formula, 2 }, { ID::speed, 1 }, { ID::bias, 1 }, { ID::sat, 25 },
                                   { ID::wowDepth, 3 }, { ID::flutterDepth, 4 } } },
            { "Deep House Bass", { { ID::formula, 0 }, { ID::speed, 0 }, { ID::bias, 0 }, { ID::sat, 50 },
                                   { ID::wowDepth, 2 }, { ID::flutterDepth, 3 } } },
            { "Lo-Fi Wobble",    { { ID::formula, 1 }, { ID::speed, 0 }, { ID::bias, 2 }, { ID::sat, 70 },
                                   { ID::wowRate, 0.8f }, { ID::wowDepth, 65 }, { ID::flutterRate, 11 },
                                   { ID::flutterDepth, 40 }, { ID::hiss, 35 } } },
            { "Slapback Vocal",  { { ID::sat, 35 }, { ID::dlyOn, 1 }, { ID::dlyType, 0 }, { ID::dlyTimeL, 110 },
                                   { ID::dlyTimeR, 110 }, { ID::dlyLevelL, -9 }, { ID::dlyLevelR, -9 },
                                   { ID::dlyHp, 200 }, { ID::dlyLp, 5000 } } },
            { "Dub Echo (Send)", { { ID::dlyOn, 1 }, { ID::dlyType, 1 }, { ID::dlyMode, 1 }, { ID::dlySync, 1 },
                                   { ID::dlyDivL, 9 }, { ID::dlyDivR, 9 }, { ID::dlyFb, 62 }, { ID::dlyLevelL, 0 },
                                   { ID::dlyLevelR, 0 }, { ID::dlyHp, 250 }, { ID::dlyLp, 3500 }, { ID::wowDepth, 18 } } },
            { "Ping-Pong Groove", { { ID::dlyOn, 1 }, { ID::dlyType, 2 }, { ID::dlySync, 1 }, { ID::dlyDivL, 5 },
                                    { ID::dlyDivR, 5 }, { ID::dlyFb, 45 }, { ID::dlyLevelL, -6 }, { ID::dlyLevelR, -6 },
                                    { ID::dlyHp, 300 }, { ID::dlyLp, 6500 } } },
        };
        return list;
    }

    float blockRms (const float* d, int n)
    {
        double s = 0;
        for (int i = 0; i < n; ++i) s += (double) d[i] * d[i];
        return n > 0 ? (float) std::sqrt (s / n) : 0.0f;
    }
}

// =====================================================================
VegaTapeProcessor::VegaTapeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VegaTape", createParameterLayout())
{
}

bool VegaTapeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void VegaTapeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    oversampler.reset();
    oversampler.initProcessing ((size_t) samplesPerBlock);

    baseDelaySamples = (float) (0.005 * fs);
    latency = juce::roundToInt (baseDelaySamples + oversampler.getLatencyInSamples());
    setLatencySamples (latency);

    for (int c = 0; c < 2; ++c)
    {
        transport[c].prepare ((int) (fs * 0.02));
        dryLine[c].prepare (latency + 16);
        hiss[c].prepare (fs);
        sat[c].reset();
        for (auto* f : { &headBump[c], &bumpDip[c], &lowCut[c], &hfLoss[c], &hfLoss2[c], &formulaShelf[c], &dcBlock[c] })
            f->reset();
    }
    wow.prepare (fs);
    echo.prepare (fs);

    for (auto* s : { &inGain, &outGain, &mixAmt, &bypassMix, &driveSm, &echoSendSm })
        s->reset (fs, 0.03);
    inGain.setCurrentAndTargetValue (vega::dbToGain (param (ID::input)));
    outGain.setCurrentAndTargetValue (1.0f);
    mixAmt.setCurrentAndTargetValue (param (ID::mix) * 0.01f);
    bypassMix.setCurrentAndTargetValue (param (ID::bypass) > 0.5f ? 1.0f : 0.0f);
    echoSendSm.setCurrentAndTargetValue (param (ID::dlyOn) > 0.5f ? 1.0f : 0.0f);
    driveSm.setCurrentAndTargetValue (1.0f);

    lastFormula = lastSpeed = lastBias = -1;
    lastHp = lastLp = -1;
    updateFilters();
}

void VegaTapeProcessor::updateFilters()
{
    const int formula = (int) param (ID::formula), speed = (int) param (ID::speed), bias = (int) param (ID::bias);
    const float hpHz = param (ID::dlyHp), lpHz = param (ID::dlyLp);

    if (hpHz != lastHp || lpHz != lastLp)
    {
        echo.setFilters (hpHz, lpHz);
        lastHp = hpHz; lastLp = lpHz;
    }
    if (formula == lastFormula && speed == lastSpeed && bias == lastBias) return;
    lastFormula = formula; lastSpeed = speed; lastBias = bias;

    const auto& F = vega::formulas[juce::jlimit (0, 2, formula)];
    const auto& B = vega::biases[juce::jlimit (0, 2, bias)];
    const bool fast = speed == 1;
    const double fsOs = fs * 2.0;

    const double emph     = (fast ? 4.0 : 6.5) + F.emphasisDb;
    const double bumpF    = fast ? 58.0 : 36.0;
    const double bumpDb   = (fast ? 2.0 : 2.8) + F.bumpDb;
    const double dipDb    = fast ? -1.0 : -1.5;
    const double hfCorner = (fast ? 21000.0 : 13500.0) * B.hfMul;

    for (int c = 0; c < 2; ++c)
    {
        sat[c].preEmph.highShelf (fsOs, 2800.0, emph);
        sat[c].deEmph.highShelf  (fsOs, 2800.0, -emph);
        lowCut[c].highPass (fs, fast ? 20.0 : 28.0, 0.75);
        headBump[c].peak (fs, bumpF, 1.1, bumpDb);
        bumpDip[c].peak (fs, bumpF * 2.1, 1.4, dipDb);
        formulaShelf[c].highShelf (fs, 7000.0, F.shelfDb);
        hfLoss[c].lowPass (fs, hfCorner, 0.62);
        hfLoss2[c].lowPass (fs, std::min (hfCorner * 1.3, fs * 0.46), 0.52);
        dcBlock[c].highPass (fs, 8.0, 0.7071);
    }
}

// =====================================================================
void VegaTapeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = std::min (2, buffer.getNumChannels());
    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || numCh == 0) return;

    updateFilters();

    // ---- host tempo / transport
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) hostBpm.store (*bpm);
            playing = pos->getIsPlaying();
        }

    // ---- parameters
    const int formula = (int) param (ID::formula), bias = (int) param (ID::bias), speed = (int) param (ID::speed);
    const auto& F = vega::formulas[juce::jlimit (0, 2, formula)];
    const auto& B = vega::biases[juce::jlimit (0, 2, bias)];

    const float inDb  = param (ID::input);
    const float outDb = param (ID::output) - (param (ID::levelLink) > 0.5f ? inDb : 0.0f);
    inGain.setTargetValue (vega::dbToGain (inDb));
    outGain.setTargetValue (vega::dbToGain (outDb));
    mixAmt.setTargetValue (param (ID::mix) * 0.01f);
    bypassMix.setTargetValue (param (ID::bypass) > 0.5f ? 1.0f : 0.0f);
    echoSendSm.setTargetValue (param (ID::dlyOn) > 0.5f ? 1.0f : 0.0f);

    const float sat01 = param (ID::sat) * 0.01f;
    driveSm.setTargetValue ((0.3f + std::pow (sat01, 1.4f) * 3.4f) * F.drive * B.drive);
    // make-up gain so turning up Saturation adds colour without a big level drop
    const float makeup = vega::dbToGain (3.0f * sat01 * sat01 * F.drive * B.drive);
    const float asym = F.asym * B.asym * (0.7f + 0.6f * sat01);

    const float wowRate = param (ID::wowRate), wowDepth = param (ID::wowDepth) * 0.01f;
    const float flRate = param (ID::flutterRate), flDepth = param (ID::flutterDepth) * 0.01f;
    const float hissP = param (ID::hiss);
    const float hissGain = hissP <= 0.05f ? 0.0f
                         : vega::dbToGain (-84.0f + 0.42f * hissP + F.hissDb + (speed == 0 ? 3.0f : 0.0f));

    // delay
    const bool  link   = param (ID::dlyLink) > 0.5f;
    const bool  sync   = param (ID::dlySync) > 0.5f;
    const int   dType  = (int) param (ID::dlyType);
    const bool  sendMode = (int) param (ID::dlyMode) == 1;
    const double bpm   = juce::jlimit (20.0, 400.0, hostBpm.load());
    auto timeSamples = [&] (const char* msId, const char* divId)
    {
        double sec = sync ? Divisions::beats[juce::jlimit (0, 11, (int) param (divId))] * 60.0 / bpm
                          : param (msId) * 0.001;
        return (float) (juce::jlimit (0.005, 2.5, sec) * fs);
    };
    const float tL = timeSamples (ID::dlyTimeL, ID::dlyDivL);
    const float tR = link ? tL : timeSamples (ID::dlyTimeR, ID::dlyDivR);
    const float gL = vega::dbToGain (param (ID::dlyLevelL));
    const float gR = link ? gL : vega::dbToGain (param (ID::dlyLevelR));
    const float fb = dType == 0 ? 0.0f : param (ID::dlyFb) * 0.01f;

    // ---- keep the dry signal (for Mix and Bypass, latency aligned)
    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer, true);

    // ---- input gain
    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        for (int c = 0; c < numCh; ++c) buffer.getWritePointer (c)[i] *= g;
    }
    float inRms[2] { 0, 0 };
    for (int c = 0; c < numCh; ++c) inRms[c] = blockRms (buffer.getReadPointer (c), n);
    if (numCh == 1) inRms[1] = inRms[0];

    // ---- record amp + tape saturation (2x oversampled)
    {
        const float drive = driveSm.skip (n);
        for (int c = 0; c < 2; ++c) sat[c].setShape (drive, asym);

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        auto up = oversampler.processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = sat[c].process (d[i]);
        }
        oversampler.processSamplesDown (block);
    }

    // ---- playback head EQ, hiss, wow & flutter, mix, echo, bypass
    float* ch[2] { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };
    const float* dr[2] { dry.getReadPointer (0), dry.getReadPointer (numCh > 1 ? 1 : 0) };

    for (int i = 0; i < n; ++i)
    {
        const auto mod = wow.next (wowRate, wowDepth, flRate, flDepth);
        const float modS[2] { mod.first * (float) fs, mod.second * (float) fs };
        const float og = outGain.getNextValue(), mx = mixAmt.getNextValue();
        const float byp = bypassMix.getNextValue(), eOn = echoSendSm.getNextValue();

        float out[2], dryD[2];
        for (int c = 0; c < numCh; ++c)
        {
            float y = ch[c][i];
            y = lowCut[c].process (y);
            y = headBump[c].process (y);
            y = bumpDip[c].process (y);
            y = formulaShelf[c].process (y);
            y = hfLoss[c].process (y);
            y = hfLoss2[c].process (y);
            y = dcBlock[c].process (y);
            if (hissGain > 0.0f) y += hiss[c].next() * hissGain;

            transport[c].push (y);
            y = transport[c].read (baseDelaySamples + modS[c]);

            dryLine[c].push (dr[c][i]);
            dryD[c] = dryLine[c].readInt (latency);
            out[c] = dryD[c] * (1.0f - mx) + y * og * makeup * mx;
        }
        if (numCh == 1) { out[1] = out[0]; dryD[1] = dryD[0]; }

        // tape echo
        const auto e = echo.process (out[0], out[1], dType, tL, tR, fb, gL, gR, wow.lastWowSeconds());
        if (sendMode)
        {
            out[0] = out[0] * (1.0f - eOn) + e.first  * eOn;
            out[1] = out[1] * (1.0f - eOn) + e.second * eOn;
        }
        else
        {
            out[0] += e.first  * eOn;
            out[1] += e.second * eOn;
        }

        for (int c = 0; c < numCh; ++c)
        {
            float v = out[numCh == 1 ? 0 : c];
            if (numCh == 1) v = 0.5f * (out[0] + out[1]);
            v = v * (1.0f - byp) + dryD[c] * byp;
            ch[c][i] = std::isfinite (v) ? v : 0.0f;
        }
    }

    // ---- meters (exponential VU‑style averaging is done in the editor)
    float outRms[2] { 0, 0 };
    for (int c = 0; c < numCh; ++c) outRms[c] = blockRms (buffer.getReadPointer (c), n);
    if (numCh == 1) outRms[1] = outRms[0];
    for (int c = 0; c < 2; ++c) { vuIn[c].store (inRms[c]); vuOut[c].store (outRms[c]); }

    const bool signal = std::max (inRms[0], inRms[1]) > 0.0005f;
    transportActivity.store ((playing || signal) && param (ID::bypass) < 0.5f ? 1.0f : 0.0f);
}

// =====================================================================
int VegaTapeProcessor::getNumPrograms() { return (int) factoryPresets().size(); }
const juce::String VegaTapeProcessor::getProgramName (int index)
{
    const auto& p = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) p.size()) ? juce::String (p[(size_t) index].name) : juce::String();
}
void VegaTapeProcessor::setCurrentProgram (int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    currentProgram = index;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID() != ID::bypass)
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [id, v] : list[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

void VegaTapeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}
void VegaTapeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessorEditor* VegaTapeProcessor::createEditor() { return new VegaTapeEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VegaTapeProcessor(); }
