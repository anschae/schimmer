#pragma once
// Ducking, adopted from Prog Delay (anschae/prog-delay, dsp/Ducker.*):
//   DuckDetector   detector filters (HP/LP), stereo-linked Peak/RMS level
//   Ducker         gain computer (threshold, depth, knee) + ballistics
//                  (attack, hold, release) -> one linear gain per sample
// The display data (detector level, gain reduction) goes to the UI through a
// lock-free FIFO, one frame per 10 ms. Schimmer adds targets (all wet, reverb,
// delay) and Pre/Post positions in the Engine.

#include "Filters.h"
#include "SpscFifo.h"

namespace schimmer::dsp
{
enum class DetMode : int { Peak = 0, Rms = 1 };

struct DuckParams
{
    bool on = false;
    float hpHz = 20.0f, lpHz = 20000.0f;
    DetMode mode = DetMode::Peak;
    float rmsMs = 30.0f;
    float threshDb = -24.0f;
    float depthDb = -12.0f;
    bool full = false;          // duck to silence
    float kneeDb = 6.0f;
    float attackMs = 5.0f, holdMs = 50.0f, releaseMs = 250.0f;  // release already tempo-resolved
};

class DuckDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        peakRel = 1.0 - std::exp (-1.0 / (sr * 0.02));
        lastHp = lastLp = lastRms = -1.0f;
        reset();
    }
    void reset()
    {
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        peakEnv = 0.0;
        ms = 0.0;
    }
    void update (const DuckParams& p) noexcept
    {
        if (p.hpHz != lastHp)
        {
            for (auto& f : hp) f.set (p.hpHz, 0.7071f, (float) sr);
            lastHp = p.hpHz;
        }
        if (p.lpHz != lastLp)
        {
            for (auto& f : lp) f.set (p.lpHz, 0.7071f, (float) sr);
            lastLp = p.lpHz;
        }
        if (p.rmsMs != lastRms)
        {
            msCoef = 1.0 - std::exp (-1.0 / (sr * p.rmsMs * 0.001));
            lastRms = p.rmsMs;
        }
        if (ms < 1.0e-20) ms = 0.0;
        if (peakEnv < 1.0e-20) peakEnv = 0.0;
    }
    /** Linear detector level of one stereo frame (stereo-linked). */
    inline float process (float l, float r, const DuckParams& p) noexcept
    {
        if (p.hpHz > 20.5f)
        {
            l = hp[0].highpass (l);
            r = hp[1].highpass (r);
        }
        if (p.lpHz < 19900.0f)
        {
            l = lp[0].lowpass (l);
            r = lp[1].lowpass (r);
        }
        if (p.mode == DetMode::Rms)
        {
            const double x2 = 0.5 * ((double) l * l + (double) r * r);
            ms += msCoef * (x2 - ms);
            return (float) std::sqrt (std::max (0.0, ms));
        }
        const double a = std::max (std::abs (l), std::abs (r));
        peakEnv = a > peakEnv ? a : peakEnv + peakRel * (a - peakEnv);
        return (float) peakEnv;
    }

private:
    double sr = 48000.0;
    Svf hp[2], lp[2];
    float lastHp = -1.0f, lastLp = -1.0f, lastRms = -1.0f;
    double peakEnv = 0.0, peakRel = 0.0, ms = 0.0, msCoef = 0.0;
};

class Ducker
{
public:
    static constexpr float kFloorDb = -80.0f;   // "full duck" target in the dB domain

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        detector.prepare (sampleRate);
        frameLen = std::max (1, (int) (sampleRate * 0.01));
        reset();
    }
    void reset()
    {
        detector.reset();
        grDb = 0.0f;
        holdCounter = 0;
        frameCount = 0;
        frameMaxLevel = -120.0f;
        frameMinGr = 0.0f;
        meterGrDb.store (0.0f);
    }

    /** Static gain-reduction curve (dB, <= 0) for a detector level in dB. */
    static float targetReductionDb (float levelDb, const DuckParams& p) noexcept
    {
        if (! p.on)
            return 0.0f;
        const float depth = p.full ? kFloorDb : clampf (p.depthDb, kFloorDb, 0.0f);
        float amount;
        if (p.kneeDb <= 0.01f)
            amount = levelDb > p.threshDb ? 1.0f : 0.0f;
        else
            amount = smoothstep ((levelDb - (p.threshDb - 0.5f * p.kneeDb)) / p.kneeDb);
        return depth * amount;
    }

    /** Fills gains[0..n) with the linear duck gain per sample. detL/detR = detector input. */
    void process (const float* detL, const float* detR, int n, const DuckParams& p, float* gains) noexcept
    {
        detector.update (p);
        const float attCoef = 1.0f - (float) std::exp (-1.0 / (sr * std::max (0.05f, p.attackMs) * 0.001));
        const float relCoef = 1.0f - (float) std::exp (-1.0 / (sr * std::max (1.0f, p.releaseMs) * 0.001));
        const int holdSamples = (int) (sr * p.holdMs * 0.001);
        for (int i = 0; i < n; ++i)
        {
            const float level = detector.process (detL[i], detR[i], p);
            const float levelDb = level > 1.0e-6f ? 20.0f * std::log10 (level) : -120.0f;
            const float target = targetReductionDb (levelDb, p);
            if (target < grDb)
            {
                grDb += (target - grDb) * attCoef;
                holdCounter = holdSamples;
            }
            else if (holdCounter > 0)
                --holdCounter;
            else
                grDb += (target - grDb) * relCoef;
            if (grDb > -1.0e-4f)
                grDb = 0.0f;
            gains[i] = grDb <= kFloorDb + 0.5f ? 0.0f : std::pow (10.0f, grDb * 0.05f);

            frameMaxLevel = std::max (frameMaxLevel, levelDb);
            frameMinGr = std::min (frameMinGr, grDb);
            if (++frameCount >= frameLen)
            {
                fifo.push ({ frameMaxLevel, frameMinGr });
                frameCount = 0;
                frameMaxLevel = -120.0f;
                frameMinGr = 0.0f;
            }
        }
        meterGrDb.store (grDb, std::memory_order_relaxed);
    }

    float getCurrentReductionDb() const noexcept { return grDb; }

    struct Frame { float levelDb, grDb; };
    int readFrames (Frame* dest, int maxFrames) noexcept { return fifo.pop (dest, maxFrames); }
    std::atomic<float> meterGrDb { 0.0f };

private:
    double sr = 48000.0;
    DuckDetector detector;
    float grDb = 0.0f;
    int holdCounter = 0;
    int frameLen = 480, frameCount = 0;
    float frameMaxLevel = -120.0f, frameMinGr = 0.0f;
    SpscFifo<Frame, 2048> fifo;
};
} // namespace schimmer::dsp
