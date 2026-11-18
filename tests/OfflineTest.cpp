// Offline DSP self-test for Vega Productions Tape.
// Renders test signals through every factory preset and checks for NaN / runaway output,
// measures harmonic distortion vs Saturation, and checks that Bypass is clean and latency‑aligned.
// Optional:  VegaOfflineTest --snapshot out.png   (renders the editor to an image)
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("FAIL: %s\n", msg); ++failures; } } while (0)

static void setP (VegaTapeProcessor& p, const char* id, float v)
{
    auto* rp = p.apvts.getParameter (id);
    rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

// render `seconds` of a signal, return the output (left channel) of the last second
static std::vector<float> render (VegaTapeProcessor& p, double fs, float seconds,
                                  const std::function<float (int)>& gen, float* peakOut = nullptr)
{
    const int block = 512;
    const int total = (int) (seconds * fs);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    std::vector<float> out;
    float peak = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i) { const float x = gen (pos + i); buf.setSample (0, i, x); buf.setSample (1, i, x); }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float y = buf.getSample (c, i);
                if (! std::isfinite (y)) { std::printf ("non-finite sample\n"); ++failures; return out; }
                peak = std::max (peak, std::abs (y));
            }
        if (pos >= total - (int) fs)
            for (int i = 0; i < n; ++i) out.push_back (buf.getSample (0, i));
    }
    if (peakOut) *peakOut = peak;
    return out;
}

// level of a single harmonic via Goertzel
static double toneLevel (const std::vector<float>& x, double fs, double f)
{
    const double w = juce::MathConstants<double>::twoPi * f / fs, c = 2 * std::cos (w);
    double s1 = 0, s2 = 0;
    const size_t n = x.size();
    for (size_t i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) i / (double) (n - 1));
        const double s = x[i] * win + c * s1 - s2; s2 = s1; s1 = s;
    }
    return std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double) n * 0.25);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double fs = 48000.0;

    // ---------------------------------------------------------------- presets: stability
    {
        VegaTapeProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);
        juce::Random rng (7);
        for (int prog = 0; prog < p.getNumPrograms(); ++prog)
        {
            p.setCurrentProgram (prog);
            float peak = 0;
            render (p, fs, 3.0f, [&] (int i) {
                const float burst = (i / 12000) % 2 == 0 ? 1.0f : 0.0f;
                return 0.9f * burst * (0.6f * (float) std::sin (i * 2 * juce::MathConstants<double>::pi * 80.0 / fs)
                                       + 0.4f * (rng.nextFloat() * 2 - 1)); }, &peak);
            std::printf ("preset %-18s peak %.2f\n", p.getProgramName (prog).toRawUTF8(), peak);
            CHECK (peak < 4.0f, "output peak runaway");
        }
    }

    // ---------------------------------------------------------------- harmonics vs saturation
    {
        double prevH3 = -1;
        for (float sat : { 0.0f, 35.0f, 70.0f, 100.0f })
        {
            VegaTapeProcessor p;
            p.setPlayConfigDetails (2, 2, fs, 512);
            p.prepareToPlay (fs, 512);
            setP (p, ID::sat, sat);
            setP (p, ID::wowDepth, 0); setP (p, ID::flutterDepth, 0);
            const double f0 = 1000.0;
            auto y = render (p, fs, 2.0f, [&] (int i) { return 0.5f * (float) std::sin (i * juce::MathConstants<double>::twoPi * f0 / fs); });
            const double h1 = toneLevel (y, fs, f0), h2 = toneLevel (y, fs, 2 * f0), h3 = toneLevel (y, fs, 3 * f0);
            std::printf ("sat %3.0f%%  fund %.3f  H2 %6.1f dB  H3 %6.1f dB\n", sat, h1,
                         20 * std::log10 (h2 / h1 + 1e-12), 20 * std::log10 (h3 / h1 + 1e-12));
            CHECK (h1 > 0.3 && h1 < 0.7, "fundamental level unexpected");
            CHECK (h3 >= prevH3 * 0.9, "H3 should grow with saturation");
            prevH3 = h3;
        }
    }

    // ---------------------------------------------------------------- bypass is the dry signal, delayed by the reported latency
    {
        VegaTapeProcessor p;
        p.setPlayConfigDetails (2, 2, fs, 512);
        p.prepareToPlay (fs, 512);
        setP (p, ID::bypass, 1);
        p.prepareToPlay (fs, 512);   // start already bypassed
        const int lat = p.getLatencySamples();
        auto gen = [] (int i) { return (float) std::sin (i * 0.0123) * 0.5f; };
        auto y = render (p, fs, 2.0f, gen);
        const int start = (int) (2.0 * fs) - (int) y.size();
        double err = 0;
        for (size_t i = 0; i < y.size(); ++i) err = std::max (err, (double) std::abs (y[i] - gen (start + (int) i - lat)));
        std::printf ("latency %d samples, bypass max error %.2e\n", lat, err);
        CHECK (err < 1e-5, "bypass not transparent");
    }

    // ---------------------------------------------------------------- editor snapshot
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshot")
        {
            VegaTapeProcessor p;
            p.setPlayConfigDetails (2, 2, fs, 512);
            p.prepareToPlay (fs, 512);
            p.setCurrentProgram (std::atoi (i + 2 < argc ? argv[i + 2] : "0"));
            p.vuOut[0] = 0.12f; p.vuOut[1] = 0.10f;
            p.transportActivity = 1.0f;
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            ed->setVisible (true);
            juce::Timer::callAfterDelay (1500, [] { juce::MessageManager::getInstance()->stopDispatchLoop(); });
            juce::MessageManager::getInstance()->runDispatchLoop();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.5f);
            juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
            ed = nullptr;
        }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
