#pragma once
// Stereo echo in front of the reverb: tempo sync (resolved by the caller),
// feedback up to just below self-oscillation, ping-pong with stereo offset,
// HP/LP in the feedback path, subtle modulation. Time changes crossfade between
// two read positions (no pitch glide, no clicks).

#include "Filters.h"
#include "RingBuffer.h"

namespace schimmer::dsp
{
struct EchoParams
{
    bool on = false;
    float timeMs = 375.0f;
    float offsetMs = 0.0f;    // R time = time + offset (ping-pong stereo offset)
    float feedback = 0.35f;   // 0..0.98
    bool pingPong = false;
    float hpHz = 80.0f, lpHz = 9000.0f;
    float mod = 0.0f;         // 0..1
};

class EchoDelay
{
public:
    static constexpr float kMaxSec = 6.0f;
    static constexpr float kXfadeSec = 0.04f;

    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& b : buf) b.allocate ((int) std::ceil ((kMaxSec + 0.05f) * fs) + 8);
        reset();
    }
    void reset()
    {
        for (auto& b : buf) b.reset();
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        for (auto& h : head) h = {};
        first = true;
    }
    void clear() noexcept
    {
        for (auto& b : buf) b.clearFromNow();
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
    }

    void setParams (const EchoParams& p, int n) noexcept
    {
        params = p;
        if (first)
        {
            sOn.setTime (0.02f, fs);
            sFb.setTime (0.03f, fs);
            sOn.snap (p.on ? 1.0f : 0.0f);
            sFb.snap (p.feedback);
        }
        onRamp.setTarget (sOn.next (p.on ? 1.0f : 0.0f, n), n);
        fbRamp.setTarget (sFb.next (clampf (p.feedback, 0.0f, 0.98f), n), n);
        for (int ch = 0; ch < 2; ++ch)
        {
            hp[ch].set (p.hpHz, 0.6f, fs);
            lp[ch].set (p.lpHz, 0.6f, fs);
            const float ms = p.timeMs + (ch == 1 ? p.offsetMs : 0.0f);
            const float target = clampf (ms * 0.001f * fs, 4.0f, kMaxSec * fs);
            auto& h = head[ch];
            if (first) { h.cur = h.next = target; h.x = 1.0f; }
            if (h.x >= 1.0f && std::abs (target - h.cur) > 0.5f)
            {
                h.next = target;
                h.x = 0.0f;
            }
            if (h.x >= 1.0f) h.cur = h.next;
        }
        modPhase += kTwoPi * 0.43f * (float) n / fs;
        if (modPhase > kTwoPi) modPhase -= kTwoPi;
        const float depth = p.mod * 0.0018f * fs;
        modL.setTarget (depth * (1.0f + std::sin (modPhase)), n);
        modR.setTarget (depth * (1.0f + std::cos (modPhase * 1.07f)), n);
        if (first) { modL.snap (modL.value + modL.step * (float) n); modR.snap (modR.value + modR.step * (float) n); }
        first = false;
    }

    /** Stereo in -> delayed (wet only) stereo out. */
    inline void processSample (float inL, float inR, float& outL, float& outR) noexcept
    {
        const float on = onRamp.next();
        const float fb = fbRamp.next();
        const float m[2] = { modL.next(), modR.next() };
        float y[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& h = head[ch];
            float v = buf[ch].readHermite (h.cur + m[ch]);
            if (h.x < 1.0f)
            {
                const float vn = buf[ch].readHermite (h.next + m[ch]);
                const float w = smoothstep (h.x);
                v += w * (vn - v);
                h.x += 1.0f / (kXfadeSec * fs);
                if (h.x >= 1.0f) { h.x = 1.0f; h.cur = h.next; }
            }
            y[ch] = v;
        }
        // feedback path: filters + soft limiting
        float f[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = y[ch] * fb;
            if (params.hpHz > 20.5f) x = hp[ch].highpass (x);
            if (params.lpHz < 19900.0f) x = lp[ch].lowpass (x);
            f[ch] = x;
        }
        // the recirculating sum is soft-limited: untouched below ~0.6, never above ~1.4
        const float gin = on;
        if (params.pingPong)
        {
            const float mono = 0.5f * (inL + inR) * gin;
            buf[0].write (limit (mono + f[1]));
            buf[1].write (limit (f[0]));
        }
        else
        {
            buf[0].write (limit (inL * gin + f[0]));
            buf[1].write (limit (inR * gin + f[1]));
        }
        outL = y[0] * on;
        outR = y[1] * on;
    }

private:
    static inline float limit (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.6f) return x;
        const float over = (a - 0.6f) / 0.8f;
        return std::copysign (0.6f + 0.8f * softClip (over), x);
    }

    struct Head { float cur = 0.0f, next = 0.0f, x = 1.0f; };
    float fs = 48000.0f;
    EchoParams params;
    RingBuffer buf[2];
    Svf hp[2], lp[2];
    Head head[2];
    Smoother sOn, sFb;
    Ramp onRamp, fbRamp, modL, modR;
    float modPhase = 0.0f;
    bool first = true;
};
} // namespace schimmer::dsp
