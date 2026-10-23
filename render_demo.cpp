// render_demo.cpp - offline test + audio demo for the TwelveStringer engine.
// Synthesises a 6-string strumming part (Karplus-Strong), runs it through the
// engine, and also synthesises a "real" 12-string for reference.
//   g++ -O2 -std=c++17 -I../Source render_demo.cpp -o render_demo && ./render_demo
#include "TwelveStringEngine.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <string>

static constexpr double SR = 44100.0;

struct Note { double t; int string; double hz; float vel; };

// ---------------------------------------------------------------- KS string
static void pluck (std::vector<float>& out, double t, double hz, float amp, uint32_t seed)
{
    const double N = SR / hz;
    const int len = int (N) + 4;
    std::vector<float> line ((size_t) len, 0.f);
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> uni (-1.f, 1.f);
    // excitation: pick-position comb on lightly low-passed noise
    float lp = 0.f;
    std::vector<float> ex ((size_t) len);
    for (auto& v : ex) { lp += 0.6f * (uni (rng) - lp); v = lp; }
    const int pickPos = std::max (1, int (N * 0.13));
    for (int i = 0; i < len; ++i) line[(size_t) i] = ex[(size_t) i] - 0.8f * ex[(size_t) ((i + pickPos) % len)];

    const double decay = std::pow (0.001, 1.0 / (hz * (3.2 - std::min (2.0, hz / 300.0))));
    const size_t start = size_t (t * SR);
    double readPos = 0.0;
    float prev = 0.f;
    std::vector<float> buf (line);
    const int L = (int) buf.size();
    int w = (int) std::floor (N) % L;
    for (size_t n = start; n < out.size(); ++n)
    {
        // fractional delay N via linear interpolation behind write head
        double rp = w - N; while (rp < 0) rp += L;
        const int i0 = int (rp); const float fr = float (rp - i0);
        const float y = buf[(size_t) i0] * (1.f - fr) + buf[(size_t) ((i0 + 1) % L)] * fr;
        const float fb = float (decay) * (0.5f * (y + prev));
        prev = y;
        buf[(size_t) w] = fb;
        w = (w + 1) % L;
        out[n] += amp * y;
        if (n - start > size_t (SR * 6)) break;
        (void) readPos;
    }
}

// ---------------------------------------------------------------- WAV writer
static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4, sr = (uint32_t) SR, br = sr * 4;
    const uint16_t ch = 2, bits = 16, align = 4, fmt = 1; const uint32_t fmtLen = 16, riff = 36 + dataBytes;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtLen, 4, 1, f); std::fwrite (&fmt, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&sr, 4, 1, f); std::fwrite (&br, 4, 1, f); std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
        for (float v : { L[i], R[i] })
        {
            const int16_t s = (int16_t) std::lround (std::clamp (v, -1.f, 1.f) * 32767.f);
            std::fwrite (&s, 2, 1, f);
        }
    std::fclose (f);
}

static float peak (const std::vector<float>& v) { float p = 0; for (float x : v) p = std::max (p, std::abs (x)); return p; }
static double rms (const std::vector<float>& v) { double s = 0; for (float x : v) s += double (x) * x; return std::sqrt (s / v.size()); }

// ---------------------------------------------------------------- score
static std::vector<Note> makeScore()
{
    std::vector<Note> notes;
    struct Chord { std::vector<std::pair<int, double>> s; };
    const Chord G  { { {0, 98.00}, {1, 123.47}, {2, 146.83}, {3, 196.00}, {4, 246.94}, {5, 392.00} } };
    const Chord C  { { {1, 130.81}, {2, 164.81}, {3, 196.00}, {4, 261.63}, {5, 329.63} } };
    const Chord D  { { {2, 146.83}, {3, 220.00}, {4, 293.66}, {5, 369.99} } };
    const Chord Em { { {0, 82.41}, {1, 123.47}, {2, 164.81}, {3, 196.00}, {4, 246.94}, {5, 329.63} } };

    auto strum = [&] (const Chord& c, double t, bool down, float vel)
    {
        const int k = (int) c.s.size();
        for (int i = 0; i < k; ++i)
        {
            const auto& [s, hz] = c.s[(size_t) (down ? i : k - 1 - i)];
            notes.push_back ({ t + i * (down ? 0.014 : 0.010), s, hz, vel * (down ? 1.f : 0.7f) });
        }
    };

    // bars 1-4: strummed G C D G  (D . D U . U D U feel, 100 bpm)
    const double beat = 0.6;
    double t = 0.1;
    const Chord* prog[] = { &G, &C, &D, &G };
    for (auto* c : prog)
    {
        strum (*c, t, true, 0.9f);
        strum (*c, t + beat, true, 0.75f);
        strum (*c, t + 1.5 * beat, false, 0.7f);
        strum (*c, t + 2.5 * beat, false, 0.7f);
        strum (*c, t + 3.0 * beat, true, 0.75f);
        strum (*c, t + 3.5 * beat, false, 0.65f);
        t += 4 * beat;
    }
    // bars 5-6: Em arpeggio, then let ring
    const int order[] = { 0, 2, 3, 4, 5, 4, 3, 2 };
    for (int rep = 0; rep < 2; ++rep)
        for (int j = 0; j < 8; ++j)
        {
            const auto& [s, hz] = Em.s[(size_t) order[j]];
            notes.push_back ({ t, s, hz, 0.85f });
            t += beat * 0.5;
        }
    strum (Em, t, true, 0.9f);
    return notes;
}

int main()
{
    const auto notes = makeScore();
    const size_t len = size_t (SR * (notes.back().t + 4.0));

    // ---- 6-string (dry) and synthetic real 12-string reference ----
    std::vector<float> dry (len, 0.f), ref12 (len, 0.f);
    uint32_t seed = 1;
    for (const auto& n : notes)
    {
        pluck (dry, n.t, n.hz, 0.16f * n.vel, seed);
        pluck (ref12, n.t, n.hz, 0.16f * n.vel, seed);
        if (n.string < 4)  // octave course: thin string an octave up, picked ~3 ms earlier
            pluck (ref12, std::max (0.0, n.t - 0.003), n.hz * 2.0, 0.10f * n.vel, seed + 7777);
        else               // unison course: slightly sharp twin
            pluck (ref12, n.t + 0.002, n.hz * std::pow (2.0, 5.0 / 1200.0), 0.13f * n.vel, seed + 9999);
        ++seed;
    }
    const float norm = 0.5f / peak (dry);
    for (auto& v : dry) v *= norm;
    const float norm12 = 0.5f / peak (ref12);
    for (auto& v : ref12) v *= norm12;

    // ---- run the engine in 256-sample blocks, as a host would ----
    twelve::TwelveStringEngine eng;
    eng.prepare (SR);
    twelve::Params p;   // defaults
    eng.setParams (p);
    eng.reset();

    std::vector<float> outL (len), outR (len);
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < len; i += 256)
    {
        const int n = (int) std::min<size_t> (256, len - i);
        eng.process (dry.data() + i, dry.data() + i, outL.data() + i, outR.data() + i, n);
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();

    // checks
    bool finite = true;
    for (size_t i = 0; i < len; ++i) finite &= std::isfinite (outL[i]) && std::isfinite (outR[i]);
    double corr = 0, eL = 0, eR = 0;
    for (size_t i = 0; i < len; ++i) { corr += outL[i] * outR[i]; eL += outL[i] * outL[i]; eR += outR[i] * outR[i]; }

    std::printf ("samples        : %zu (%.1f s)\n", len, len / SR);
    std::printf ("finite output  : %s\n", finite ? "yes" : "NO");
    std::printf ("dry   peak/rms : %.3f / %.4f\n", peak (dry), rms (dry));
    std::printf ("out L peak/rms : %.3f / %.4f\n", peak (outL), rms (outL));
    std::printf ("out R peak/rms : %.3f / %.4f\n", peak (outR), rms (outR));
    std::printf ("L/R correlation: %.3f\n", corr / std::sqrt (eL * eR));
    std::printf ("latency (HQ)    : %d samples (%.1f ms)\n", eng.getLatencySamples(), 1000.0 * eng.getLatencySamples() / SR);
    std::printf ("CPU            : %.1f%% of one core (%.3f s for %.1f s audio)\n", 100.0 * secs / (len / SR), secs, len / SR);

    // live mode must also be clean
    {
        twelve::TwelveStringEngine live;
        live.prepare (SR);
        twelve::Params lp; lp.liveMode = true;
        live.setParams (lp); live.reset();
        std::vector<float> a (len), b (len);
        for (size_t i = 0; i < len; i += 256)
        {
            const int n = (int) std::min<size_t> (256, len - i);
            live.process (dry.data() + i, dry.data() + i, a.data() + i, b.data() + i, n);
        }
        bool ok = live.getLatencySamples() == 0;
        for (size_t i = 0; i < len; ++i) ok &= std::isfinite (a[i]) && std::isfinite (b[i]) && std::abs (a[i]) < 2.f;
        std::printf ("live mode      : %s, peak %.3f\n", ok ? "ok" : "FAILED", std::max (peak (a), peak (b)));
        finite &= ok;
    }

    // remove the reported latency (what FL Studio's delay compensation does)
    const int lat = eng.getLatencySamples();
    outL.erase (outL.begin(), outL.begin() + lat); outL.resize (len, 0.f);
    outR.erase (outR.begin(), outR.begin() + lat); outR.resize (len, 0.f);

    // dry component must line up with the original after compensation
    {
        double best = -1; int bestLag = 0;
        for (int lag = -400; lag <= 400; ++lag)
        {
            double c = 0;
            for (size_t i = 44100; i < 44100 * 4; ++i) c += dry[i] * (outL[(size_t) ((long) i + lag)] + outR[(size_t) ((long) i + lag)]);
            if (c > best) { best = c; bestLag = lag; }
        }
        std::printf ("dry alignment  : %d samples offset after compensation\n", bestLag);
    }

    // write demo files, levels matched to the dry track's peak
    const float g = 0.5f / std::max (peak (outL), peak (outR));
    for (size_t i = 0; i < len; ++i) { outL[i] *= g; outR[i] *= g; }
    writeWav ("1_dry_6-string.wav", dry, dry);
    writeWav ("2_TwelveStringer.wav", outL, outR);
    writeWav ("3_reference_synth_12-string.wav", ref12, ref12);
    std::printf ("wrote 3 wav files\n");
    return finite ? 0 : 1;
}
