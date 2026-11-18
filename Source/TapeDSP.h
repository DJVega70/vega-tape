#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

// =====================================================================
//  Vega Productions Tape — DSP building blocks
// =====================================================================
namespace vega
{
inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return g > 1.0e-6f ? 20.0f * std::log10 (g) : -120.0f; }

// ---------------------------------------------------------------------
//  RBJ biquad, transposed direct form II
// ---------------------------------------------------------------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setNormalised (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    static double clampF (double f, double fs) { return juce::jlimit (5.0, fs * 0.47, f); }

    void lowPass (double fs, double f, double q = 0.7071)
    {
        const double w = juce::MathConstants<double>::twoPi * clampF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highPass (double fs, double f, double q = 0.7071)
    {
        const double w = juce::MathConstants<double>::twoPi * clampF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void peak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = juce::MathConstants<double>::twoPi * clampF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        setNormalised (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void highShelf (double fs, double f, double gainDb, double slope = 0.8)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = juce::MathConstants<double>::twoPi * clampF (f, fs) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2), sa = 2 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                       (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
};

// ---------------------------------------------------------------------
//  Fractional delay line (4‑point Hermite interpolation)
// ---------------------------------------------------------------------
class FracDelay
{
public:
    void prepare (int maxSamples)
    {
        size = juce::nextPowerOfTwo (maxSamples + 8);
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // delay measured in samples behind the most recently pushed sample
    inline float read (float delay) const noexcept
    {
        delay = juce::jlimit (1.0f, (float) (size - 4), delay);
        const int   di = (int) delay;
        const float f  = delay - (float) di;
        const int   i0 = (w - 1 - di) & mask;
        const float y0 = buf[(size_t) ((i0 + 1) & mask)], y1 = buf[(size_t) i0],
                    y2 = buf[(size_t) ((i0 - 1) & mask)], y3 = buf[(size_t) ((i0 - 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    inline float readInt (int delay) const noexcept { return buf[(size_t) ((w - 1 - delay) & mask)]; }

private:
    std::vector<float> buf;
    int size = 0, mask = 0, w = 0;
};

// ---------------------------------------------------------------------
//  Tape formula / bias characters
// ---------------------------------------------------------------------
struct FormulaSpec { float drive, asym, bumpDb, shelfDb, hissDb, emphasisDb; };
inline constexpr FormulaSpec formulas[3]
{
    { 1.00f, 0.10f,  0.0f,  0.0f,  0.0f, 0.0f },   // Classic — balanced, round low end
    { 1.55f, 0.15f,  0.6f,  0.9f,  2.0f, 1.0f },   // Hot     — more drive, more harmonics, brighter
    { 0.72f, 0.06f, -0.3f, -1.4f, -4.0f, -1.0f },  // Smooth  — cleaner, softer top, lower noise
};
struct BiasSpec { float drive, asym, hfMul; };
inline constexpr BiasSpec biases[3]
{
    { 1.00f, 1.00f, 1.00f },   // Nominal
    { 0.82f, 0.70f, 0.74f },   // +3 over‑bias — cleaner, darker
    { 0.66f, 0.48f, 0.56f },   // +5 over‑bias — cleanest, darkest
};

// ---------------------------------------------------------------------
//  Saturating record amp + tape (runs oversampled)
//  core(u) = (tanh(u+b) - tanh b) / (1 - tanh² b)   → unity slope at 0
// ---------------------------------------------------------------------
struct TapeSaturator
{
    Biquad preEmph, deEmph;
    float drive = 1.0f, bias = 0.1f, tb = 0, norm = 1;

    void setShape (float newDrive, float newAsym)
    {
        drive = std::max (0.05f, newDrive);
        bias  = newAsym;
        tb    = std::tanh (bias);
        norm  = 1.0f / (1.0f - tb * tb);
    }
    inline float process (float x) noexcept
    {
        const float u = preEmph.process (x) * drive;
        const float s = (std::tanh (u + bias) - tb) * norm / drive;
        return deEmph.process (s);
    }
    void reset() { preEmph.reset(); deEmph.reset(); }
};

// ---------------------------------------------------------------------
//  Wow & flutter modulator (shared by both channels — the tape moves as one)
// ---------------------------------------------------------------------
class WowFlutter
{
public:
    void prepare (double sampleRate) { fs = sampleRate; phW = phF = phF2 = 0; driftState = driftTarget = 0; driftCount = 0; }

    // returns delay offset in seconds for left (first) and right (second) heads
    inline std::pair<float, float> next (float wowRate, float wowDepth01, float flutRate, float flutDepth01) noexcept
    {
        const double twoPi = juce::MathConstants<double>::twoPi;
        // random drift that wanders the wow speed a little
        if (--driftCount <= 0) { driftTarget = rng.nextFloat() * 2.0f - 1.0f; driftCount = (int) (fs * 0.35); }
        driftState += (driftTarget - driftState) * (float) (1.0 / (fs * 0.25));

        phW  += twoPi * wowRate * (1.0 + 0.25 * driftState) / fs;
        phF  += twoPi * flutRate / fs;
        phF2 += twoPi * flutRate * 1.73 / fs;
        if (phW  > twoPi) phW  -= twoPi;
        if (phF  > twoPi) phF  -= twoPi;
        if (phF2 > twoPi) phF2 -= twoPi;

        // peak speed deviation: wow up to 0.6 %, flutter up to 0.25 %
        const float aW = std::min (0.0040f, 0.006f  * wowDepth01  / (float) (twoPi * wowRate));
        const float aF = std::min (0.0006f, 0.0025f * flutDepth01 / (float) (twoPi * flutRate));

        const float w  = (float) std::sin (phW) + 0.3f * driftState;
        const float fL = (float) (std::sin (phF) + 0.35 * std::sin (phF2));
        const float fR = (float) (std::sin (phF + 0.25) + 0.35 * std::sin (phF2 + 0.4));
        lastWow = w * aW;
        return { w * aW + fL * aF, w * aW + fR * aF };
    }
    float lastWowSeconds() const { return lastWow; }

private:
    double fs = 44100, phW = 0, phF = 0, phF2 = 0;
    float driftState = 0, driftTarget = 0, lastWow = 0;
    int driftCount = 0;
    juce::Random rng { 0x5eed };
};

// ---------------------------------------------------------------------
//  Tape hiss generator (per channel)
// ---------------------------------------------------------------------
struct Hiss
{
    Biquad hp, lp;
    juce::Random rng;
    explicit Hiss (int seed = 1) : rng (seed) {}
    void prepare (double fs) { hp.highPass (fs, 450.0, 0.6); lp.lowPass (fs, 13000.0, 0.6); hp.reset(); lp.reset(); }
    inline float next() noexcept { return lp.process (hp.process (rng.nextFloat() * 2.0f - 1.0f)) * 1.7f; }
};

// ---------------------------------------------------------------------
//  Tape echo (Slap / Feedback / Ping‑Pong) with filtered, saturated repeats
// ---------------------------------------------------------------------
class TapeEcho
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& d : line) d.prepare ((int) (fs * 2.6));
        reset();
    }
    void reset()
    {
        for (auto& d : line) d.reset();
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        fbL = fbR = 0;
        curL = curR = -1.0f;
    }
    void setFilters (float hpHz, float lpHz)
    {
        for (auto& f : hp) f.highPass (fs, hpHz, 0.65);
        for (auto& f : lp) f.lowPass (fs, lpHz, 0.65);
    }

    // type: 0 slap, 1 feedback, 2 ping‑pong.  times in samples, gains linear, mod in seconds (wow)
    inline std::pair<float, float> process (float inL, float inR, int type, float tL, float tR,
                                            float fb, float gL, float gR, float modSec) noexcept
    {
        if (curL < 0) { curL = tL; curR = tR; }
        // tape‑style glide when the time changes
        const float glide = (float) (1.0 / (fs * 0.09));
        curL += (tL - curL) * glide;
        curR += (tR - curR) * glide;

        const float m = modSec * (float) fs * 1.5f;
        float eL = line[0].read (curL + m);
        float eR = line[1].read (curR + m);
        eL = sat (lp[0].process (hp[0].process (eL)));
        eR = sat (lp[1].process (hp[1].process (eR)));

        switch (type)
        {
            case 0:  line[0].push (inL); line[1].push (inR); break;
            case 1:  line[0].push (inL + fb * eL); line[1].push (inR + fb * eR); break;
            default: line[0].push (0.5f * (inL + inR) + fb * eR); line[1].push (fb * eL); break;
        }
        return { eL * gL, eR * gR };
    }

private:
    static inline float sat (float x) noexcept { return std::tanh (x * 1.15f) / 1.15f; }
    double fs = 44100;
    FracDelay line[2];
    Biquad hp[2], lp[2];
    float fbL = 0, fbR = 0, curL = -1, curR = -1;
};
} // namespace vega
