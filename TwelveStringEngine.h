// TwelveStringEngine.h
// ---------------------------------------------------------------------------
// Turns a 6-string guitar signal into a 12-string sound.
//
// A real 12-string has six "courses" of paired strings:
//   * the four low courses (E A D G) pair each string with a thin string tuned
//     one OCTAVE higher, which the pick usually hits a hair earlier;
//   * the two high courses (B e) pair each string with a UNISON twin that is
//     never perfectly in tune, giving the famous shimmering chorus.
//
// This engine rebuilds those partner strings from the 6-string signal:
//   Octave voice : low-passed input -> pitch shifter (+12 st) ->
//                  thin/bright EQ.  Two decorrelated copies (L/R).
//   Unison voice : full-band input -> two slightly detuned pitch shifters
//                  (+cents / -cents), panned apart.
//   Strum offset : short pre-delay on the partner strings.
//   Drift        : slow random pitch wander so the pairs never lock.
//   HQ mode      : octave made with a phase vocoder (clean on chords); the dry
//                  signal is delayed to line up and the delay is reported to
//                  the host for automatic latency compensation.
//   Live mode    : octave made with a zero-latency grain shifter instead, for
//                  monitoring while recording (rougher on low notes).
//
// Pure C++17, no framework dependencies, so it can be unit-tested on its own.
// ---------------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <vector>

namespace twelve
{

constexpr double kTwoPi = 6.283185307179586;

//==============================================================================
struct Params
{
    float octaveMix      = 0.55f;   // 0..1   level of the octave strings
    float octaveFocusHz  = 1200.f;  // 300..8000  how high up the neck octaves reach
    float sparkleDb      = 6.f;     // -6..+12 brightness of the octave strings
    float unisonMix      = 0.40f;   // 0..1   level of the unison twins
    float detuneCents    = 6.f;     // 0..25  how far out of tune the twins are
    float strumMs        = 4.f;     // 0..20  pick offset of the partner strings
    float drift          = 0.35f;   // 0..1   random wander of the pairs
    float width          = 0.70f;   // 0..1   stereo spread
    float dryLevel       = 1.0f;    // 0..1   original guitar level
    float outputDb       = 0.f;     // -24..+12
    bool  liveMode       = false;   // true = zero latency (rougher octave) for live playing
};

//==============================================================================
class Biquad
{
public:
    void reset() noexcept { z1 = z2 = 0.f; }

    void setLowpass (double sr, double f, double q)  { calc (sr, f, q, 0, 0.0); }
    void setHighpass (double sr, double f, double q) { calc (sr, f, q, 1, 0.0); }
    void setHighShelf (double sr, double f, double gainDb) { calc (sr, f, 0.707, 2, gainDb); }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    void calc (double sr, double f, double q, int type, double gainDb)
    {
        f = std::clamp (f, 10.0, 0.45 * sr);
        const double w0 = kTwoPi * f / sr;
        const double c = std::cos (w0), s = std::sin (w0);
        double B0, B1, B2, A0, A1, A2;

        if (type == 0)          // low-pass
        {
            const double alpha = s / (2.0 * q);
            B0 = (1.0 - c) * 0.5; B1 = 1.0 - c; B2 = B0;
            A0 = 1.0 + alpha; A1 = -2.0 * c; A2 = 1.0 - alpha;
        }
        else if (type == 1)     // high-pass
        {
            const double alpha = s / (2.0 * q);
            B0 = (1.0 + c) * 0.5; B1 = -(1.0 + c); B2 = B0;
            A0 = 1.0 + alpha; A1 = -2.0 * c; A2 = 1.0 - alpha;
        }
        else                    // high shelf (RBJ, S = 1)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double sqA = std::sqrt (A);
            const double alpha = s * 0.5 * std::sqrt (2.0);
            B0 =  A * ((A + 1) + (A - 1) * c + 2 * sqA * alpha);
            B1 = -2 * A * ((A - 1) + (A + 1) * c);
            B2 =  A * ((A + 1) + (A - 1) * c - 2 * sqA * alpha);
            A0 =       (A + 1) - (A - 1) * c + 2 * sqA * alpha;
            A1 =  2 * ((A - 1) - (A + 1) * c);
            A2 =       (A + 1) - (A - 1) * c - 2 * sqA * alpha;
        }

        b0 = float (B0 / A0); b1 = float (B1 / A0); b2 = float (B2 / A0);
        a1 = float (A1 / A0); a2 = float (A2 / A0);
    }

    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

//==============================================================================
// Circular buffer with fractional (4-point Hermite) reads.
class DelayBuffer
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8) size <<= 1;
        buf.assign ((size_t) size, 0.f);
        mask = size - 1;
        w = 0;
    }

    void reset() { std::fill (buf.begin(), buf.end(), 0.f); w = 0; }

    inline void write (float x) noexcept
    {
        buf[(size_t) w] = x;
        w = (w + 1) & mask;
    }

    // delay in samples, measured from the most recently written sample (>= 2)
    inline float read (double delay) const noexcept
    {
        const double rp = double (w - 1) - delay;
        const double fl = std::floor (rp);
        const int i = (int) fl;
        const float t = float (rp - fl);
        const float xm1 = buf[(size_t) ((i - 1) & mask)];
        const float x0  = buf[(size_t) ( i      & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)];
        const float x2  = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    inline float readInt (int delay) const noexcept
    {
        return buf[(size_t) ((w - 1 - delay) & mask)];
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

//==============================================================================
// Overlapping-grain (delay-line) pitch shifter. Low latency, works on chords.
// Each grain is a read tap that sweeps across a window of W samples at a speed
// that produces the requested pitch ratio, faded in and out with a Hann window.
class GrainShifter
{
public:
    void prepare (double sampleRate, double windowMs, double maxBaseMs, int grains)
    {
        sr = sampleRate;
        W = std::max (64.0, windowMs * 0.001 * sr);
        numGrains = std::clamp (grains, 2, kMaxGrains);
        norm = 2.f / float (numGrains);   // Hann windows spaced 1/N apart sum to N/2
        buffer.prepare (int (W + maxBaseMs * 0.001 * sr) + 16);
        reset();
    }

    void reset()
    {
        buffer.reset();
        for (int i = 0; i < numGrains; ++i)
            phase[i] = double (i) / double (numGrains);
    }

    void setRatio (double r) noexcept          { ratio = r; }
    void setBaseDelay (double samples) noexcept { base = std::max (3.0, samples); }
    double meanDelaySamples() const noexcept    { return base + 0.5 * W; }
    double windowSamples() const noexcept       { return W; }

    inline float process (float x) noexcept
    {
        buffer.write (x);
        const double step = (ratio - 1.0) / W;   // may be negative (pitch down)
        float out = 0.f;

        for (int i = 0; i < numGrains; ++i)
        {
            double p = phase[i];
            const double d = base + (1.0 - p) * W;
            const float win = 0.5f - 0.5f * (float) std::cos (kTwoPi * p);
            out += win * buffer.read (d);

            p += step;
            if (p >= 1.0) p -= 1.0;
            else if (p < 0.0) p += 1.0;
            phase[i] = p;
        }
        return out * norm;
    }

private:
    static constexpr int kMaxGrains = 4;
    DelayBuffer buffer;
    double sr = 44100, W = 1024, ratio = 1, base = 3;
    double phase[kMaxGrains] {};
    int numGrains = 2;
    float norm = 1.f;
};

//==============================================================================
// Slow, smooth random wander in [-1, 1].
class Drift
{
public:
    void prepare (double sampleRate, uint32_t seed, double rateHz)
    {
        sr = sampleRate;
        state = seed ? seed : 0x9E3779B9u;
        period = int (sr / std::max (0.05, rateHz));
        coeff = float (1.0 - std::exp (-kTwoPi * rateHz * 0.8 / sr));
        counter = 0; target = nextRandom(); value = 0.f;
    }

    inline float process() noexcept
    {
        if (++counter >= period) { counter = 0; target = nextRandom(); }
        value += coeff * (target - value);
        return value;
    }

private:
    float nextRandom() noexcept
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return float (state) * (2.f / 4294967295.f) - 1.f;
    }

    double sr = 44100;
    uint32_t state = 1;
    int period = 44100, counter = 0;
    float coeff = 0.001f, target = 0.f, value = 0.f;
};

//==============================================================================
struct Smoothed
{
    void reset (float v) { cur = tgt = v; }
    void setTarget (float v) { tgt = v; }
    inline float next() noexcept { cur += coeff * (tgt - cur); return cur; }
    float cur = 0.f, tgt = 0.f, coeff = 0.002f;
};

//==============================================================================
// Small radix-2 complex FFT (in place, unnormalised).
class FFT
{
public:
    void prepare (int order)
    {
        n = 1 << order;
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
            tw[(size_t) k] = std::polar (1.0f, float (-kTwoPi * k / n));
    }

    void perform (std::complex<float>* d, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (d[i], d[rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int k = 0; k < half; ++k)
                {
                    std::complex<float> w = tw[(size_t) (k * step)];
                    if (inverse) w = std::conj (w);
                    const auto u = d[i + k];
                    const auto v = d[i + k + half] * w;
                    d[i + k] = u + v;
                    d[i + k + half] = u - v;
                }
        }
    }

    int size() const noexcept { return n; }

private:
    int n = 0;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};

//==============================================================================
// Streaming phase-vocoder pitch shifter (Bernsee-style, 8x overlap).
// Clean on full chords; latency = fftSize - hop samples.
class PhaseVocoderShifter
{
public:
    void prepare (double sampleRate)
    {
        const int order = sampleRate <= 50000.0 ? 11 : (sampleRate <= 100000.0 ? 12 : 13);
        N = 1 << order;
        osamp = 8;
        hop = N / osamp;
        latency = N - hop;
        fft.prepare (order);

        win.resize ((size_t) N);
        for (int k = 0; k < N; ++k)
            win[(size_t) k] = float (0.5 - 0.5 * std::cos (kTwoPi * k / N));

        for (auto* v : { &inFifo, &outFifo, &outAccum })
            v->assign ((size_t) N * 2, 0.f);
        frame.assign ((size_t) N, {});
        for (auto* v : { &lastPhase, &sumPhase, &anaMag, &anaFreq, &synMag, &synFreq })
            v->assign ((size_t) N / 2 + 1, 0.0);

        gain = float (1.0 / (double (N) * 0.375 * osamp));
        reset();
    }

    void reset()
    {
        for (auto* v : { &inFifo, &outFifo, &outAccum }) std::fill (v->begin(), v->end(), 0.f);
        for (auto* v : { &lastPhase, &sumPhase }) std::fill (v->begin(), v->end(), 0.0);
        rover = latency;
    }

    void setRatio (double r) noexcept
    {
        ratio = r;
        // shifting up spreads each partial over fewer bins; restore its level
        postGain = r > 1.0 ? float (std::pow (r, 0.7)) : 1.f;
    }
    int getLatency() const noexcept     { return latency; }

    inline float process (float x) noexcept
    {
        inFifo[(size_t) rover] = x;
        const float y = outFifo[(size_t) (rover - latency)];
        if (++rover >= N)
        {
            rover = latency;
            processFrame();
        }
        return y;
    }

private:
    static inline double wrap (double ph) noexcept
    {
        return ph - kTwoPi * std::floor ((ph + kTwoPi * 0.5) / kTwoPi);
    }

    void processFrame() noexcept
    {
        const int half = N / 2;
        const double expct = kTwoPi * double (hop) / double (N);

        for (int k = 0; k < N; ++k)
            frame[(size_t) k] = { inFifo[(size_t) k] * win[(size_t) k], 0.f };
        fft.perform (frame.data(), false);

        // ---- analysis: true frequency of every bin ----
        for (int k = 0; k <= half; ++k)
        {
            const auto c = frame[(size_t) k];
            const double mag = 2.0 * std::abs (c);
            const double ph = std::arg (c);
            double d = ph - lastPhase[(size_t) k];
            lastPhase[(size_t) k] = ph;
            d = wrap (d - k * expct);
            anaFreq[(size_t) k] = k + d * osamp / kTwoPi;
            anaMag[(size_t) k] = mag;
        }

        // ---- shift ----
        std::fill (synMag.begin(), synMag.end(), 0.0);
        std::fill (synFreq.begin(), synFreq.end(), 0.0);
        for (int k = 0; k <= half; ++k)
        {
            const int idx = (int) std::lround (k * ratio);
            if (idx > half) break;
            synMag[(size_t) idx] += anaMag[(size_t) k];
            synFreq[(size_t) idx] = anaFreq[(size_t) k] * ratio;
        }

        // ---- synthesis ----
        for (int k = 0; k <= half; ++k)
        {
            const double d = kTwoPi * (synFreq[(size_t) k] - k) / osamp + k * expct;
            sumPhase[(size_t) k] = wrap (sumPhase[(size_t) k] + d);
            frame[(size_t) k] = std::polar (float (synMag[(size_t) k]), float (sumPhase[(size_t) k]));
        }
        for (int k = half + 1; k < N; ++k)
            frame[(size_t) k] = {};
        fft.perform (frame.data(), true);

        for (int k = 0; k < N; ++k)
            outAccum[(size_t) k] += win[(size_t) k] * frame[(size_t) k].real() * gain * postGain;

        std::memcpy (outFifo.data(), outAccum.data(), sizeof (float) * (size_t) hop);
        std::memmove (outAccum.data(), outAccum.data() + hop, sizeof (float) * (size_t) N);
        std::memmove (inFifo.data(), inFifo.data() + hop, sizeof (float) * (size_t) latency);
    }

    FFT fft;
    int N = 2048, osamp = 8, hop = 256, latency = 1792, rover = 0;
    double ratio = 2.0;
    float gain = 1.f, postGain = 1.f;
    std::vector<float> win, inFifo, outFifo, outAccum;
    std::vector<std::complex<float>> frame;
    std::vector<double> lastPhase, sumPhase, anaMag, anaFreq, synMag, synFreq;
};

//==============================================================================
class TwelveStringEngine
{
public:
    static constexpr double kLiveOctWinMsL = 26.0, kLiveOctWinMsR = 30.0;
    static constexpr double kUnisonWinMsL  = 44.0, kUnisonWinMsR  = 50.0;
    static constexpr double kMaxStrumMs    = 25.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        pvL.prepare (sr);
        pvR.prepare (sr);
        pvLatency = pvL.getLatency();
        const double pvLatencyMs = 1000.0 * pvLatency / sr;

        liveL.prepare (sr, kLiveOctWinMsL, kMaxStrumMs + 5.0, 2);
        liveR.prepare (sr, kLiveOctWinMsR, kMaxStrumMs + 5.0, 2);
        uniL.prepare (sr, kUnisonWinMsL, pvLatencyMs + kMaxStrumMs + 5.0, 2);
        uniR.prepare (sr, kUnisonWinMsR, pvLatencyMs + kMaxStrumMs + 5.0, 2);

        strumL.prepare (int ((kMaxStrumMs + 5.0) * 0.001 * sr));
        strumR.prepare (int ((kMaxStrumMs + 5.0) * 0.001 * sr));
        dryL.prepare (pvLatency + 16);
        dryR.prepare (pvLatency + 16);

        driftOL.prepare (sr, 0x1234567u, 0.9);
        driftOR.prepare (sr, 0x7654321u, 1.1);
        driftUL.prepare (sr, 0x0badf00u, 0.6);
        driftUR.prepare (sr, 0x0c0ffeeu, 0.7);

        inHpf.setHighpass (sr, 70.0, 0.707);
        octHpfL.setHighpass (sr, 160.0, 0.707);
        octHpfR.setHighpass (sr, 160.0, 0.707);

        const float c = float (1.0 - std::exp (-1.0 / (0.02 * sr)));   // ~20 ms smoothing
        for (auto* s : { &gOct, &gUni, &gDry, &gOut, &gWidth, &gStrum })
            s->coeff = c;

        prepared = true;
        updateFilters();
        reset();
    }

    void reset()
    {
        pvL.reset(); pvR.reset(); liveL.reset(); liveR.reset(); uniL.reset(); uniR.reset();
        strumL.reset(); strumR.reset(); dryL.reset(); dryR.reset();
        for (auto* f : { &inHpf, &focus1, &focus2, &octHpfL, &octHpfR, &shelfL, &shelfR })
            f->reset();
        gOct.reset (p.octaveMix);
        gUni.reset (p.unisonMix);
        gDry.reset (p.dryLevel);
        gOut.reset (dbToGain (p.outputDb));
        gWidth.reset (p.width);
        gStrum.reset (msToSamples (p.strumMs));
        activeLive = p.liveMode;
    }

    void setParams (const Params& np)
    {
        const bool filtersChanged = std::abs (np.octaveFocusHz - p.octaveFocusHz) > 1.0e-3f
                                 || std::abs (np.sparkleDb - p.sparkleDb) > 1.0e-4f;
        p = np;
        gOct.setTarget (p.octaveMix);
        gUni.setTarget (p.unisonMix);
        gDry.setTarget (p.dryLevel);
        gOut.setTarget (dbToGain (p.outputDb));
        gWidth.setTarget (p.width);
        gStrum.setTarget (msToSamples (std::clamp (p.strumMs, 0.f, float (kMaxStrumMs))));
        if (prepared && filtersChanged) updateFilters();
    }

    // Latency the host should compensate for (0 in Live mode).
    int getLatencySamples() const noexcept { return p.liveMode ? 0 : pvLatency; }

    // In-place safe. inR may equal inL for mono input.
    void process (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        if (p.liveMode != activeLive)        // mode switch: clear the voices that change timing
        {
            activeLive = p.liveMode;
            pvL.reset(); pvR.reset(); liveL.reset(); liveR.reset();
            uniL.reset(); uniR.reset(); dryL.reset(); dryR.reset(); strumL.reset(); strumR.reset();
        }

        const bool live = activeLive;
        const int dryDelay = live ? 0 : pvLatency;
        const double octDriftCents = 4.0 * p.drift;
        const double uniDriftCents = p.drift * (1.5 + 0.35 * p.detuneCents);
        // in HQ mode the unison voices are pushed back so they line up with the delayed dry
        const double uniAlignL = live ? 0.0 : std::max (0.0, pvLatency - 0.5 * uniL.windowSamples());
        const double uniAlignR = live ? 0.0 : std::max (0.0, pvLatency - 0.5 * uniR.windowSamples());
        const double ms = 0.001 * sr;

        for (int i = 0; i < n; ++i)
        {
            const float xl = inL[i], xr = inR[i];
            const float mono = inHpf.process (0.5f * (xl + xr));
            const float strum = gStrum.next();

            // ---- octave strings: only the lower register gets an octave partner ----
            const double rOL = 2.0 * centsToRatio (octDriftCents * driftOL.process() + 1.5);
            const double rOR = 2.0 * centsToRatio (octDriftCents * driftOR.process() - 1.5);
            const float focused = focus2.process (focus1.process (mono));
            float rawL, rawR;

            if (live)
            {
                liveL.setRatio (rOL);  liveL.setBaseDelay (strum);
                liveR.setRatio (rOR);  liveR.setBaseDelay (strum + 0.7 * ms);
                rawL = liveL.process (focused);
                rawR = liveR.process (focused);
            }
            else
            {
                pvL.setRatio (rOL);
                pvR.setRatio (rOR);
                strumL.write (pvL.process (focused));
                strumR.write (pvR.process (focused));
                rawL = strumL.read (std::max (2.0, double (strum)));
                rawR = strumR.read (std::max (2.0, strum + 0.7 * ms));
            }

            const float oL = shelfL.process (octHpfL.process (rawL));
            const float oR = shelfR.process (octHpfR.process (rawR));

            // ---- unison twins ----
            uniL.setRatio (centsToRatio ( p.detuneCents + uniDriftCents * driftUL.process()));
            uniR.setRatio (centsToRatio (-p.detuneCents + uniDriftCents * driftUR.process()));
            uniL.setBaseDelay (uniAlignL + 0.5 * strum + 1.0 * ms);
            uniR.setBaseDelay (uniAlignR + 0.5 * strum + 1.8 * ms);
            const float uL = uniL.process (mono);
            const float uR = uniR.process (mono);

            // ---- dry (delayed in HQ mode to line up) ----
            dryL.write (xl);
            dryR.write (xr);
            const float dl = dryDelay ? dryL.readInt (dryDelay) : xl;
            const float dr = dryDelay ? dryR.readInt (dryDelay) : xr;

            // ---- mix ----
            const float w  = gWidth.next();
            const float go = gOct.next() * kOctaveMakeup;
            const float gu = gUni.next() * 0.7f;
            const float gd = gDry.next();
            const float g  = gOut.next();

            float plL, plR, prL, prR, ulL, ulR, urL, urR;
            pan (-0.6f * w, plL, plR);  pan (0.6f * w, prL, prR);
            pan (w, ulL, ulR);          pan (-w, urL, urR);

            outL[i] = g * (gd * dl + go * (oL * plL + oR * prL) + gu * (uL * ulL + uR * urL));
            outR[i] = g * (gd * dr + go * (oL * plR + oR * prR) + gu * (uL * ulR + uR * urR));
        }
    }

private:
    static constexpr float kOctaveMakeup = 1.6f;   // compensates for focus-filter loss

    static inline double centsToRatio (double c) noexcept { return std::pow (2.0, c / 1200.0); }
    static inline float dbToGain (float db) noexcept      { return std::pow (10.f, db / 20.f); }
    float msToSamples (float ms) const noexcept           { return float (ms * 0.001 * sr); }

    static inline void pan (float pos, float& l, float& r) noexcept
    {
        const float a = (std::clamp (pos, -1.f, 1.f) + 1.f) * 0.25f * 3.14159265f;
        l = std::cos (a);
        r = std::sin (a);
    }

    void updateFilters()
    {
        focus1.setLowpass (sr, p.octaveFocusHz, 0.54);   // 4th-order Butterworth pair
        focus2.setLowpass (sr, p.octaveFocusHz, 1.31);
        shelfL.setHighShelf (sr, 2800.0, p.sparkleDb);
        shelfR.setHighShelf (sr, 2800.0, p.sparkleDb);
    }

    double sr = 44100;
    bool prepared = false, activeLive = false;
    Params p;

    PhaseVocoderShifter pvL, pvR;
    GrainShifter liveL, liveR, uniL, uniR;
    Drift driftOL, driftOR, driftUL, driftUR;
    Biquad inHpf, focus1, focus2, octHpfL, octHpfR, shelfL, shelfR;
    DelayBuffer strumL, strumR, dryL, dryR;
    int pvLatency = 0;

    Smoothed gOct, gUni, gDry, gOut, gWidth, gStrum;
};

} // namespace twelve
