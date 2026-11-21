#pragma once
// The complete Schimmer signal path, independent of the plugin wrapper.
//
//   in -> input HP/LP -> lookahead -> [duck pre] -> echo delay
//      -> reverb in (= input + echo * amount) -> pre-delay -> pre-shimmer
//      -> Dark input LP -> input diffuser -> bloom -> freeze gate -> tank
//      -> taps + early -> grains -> Dark output LP -> tilt -> width/bass mono
//      -> + echo direct -> [duck post] -> clear-tail fade -> mix -> output
//
// Processing runs in chunks of kChunk samples (parameters are updated per
// chunk and interpolated per sample), so any host block size works.

#include "Bloom.h"
#include "Diffuser.h"
#include "Ducker.h"
#include "EchoDelay.h"
#include "Granulator.h"
#include "SpscFifo.h"
#include "Tank.h"

namespace schimmer::dsp
{
enum class DuckSource : int { Input = 0, Sidechain = 1 };
enum class DuckTarget : int { AllWet = 0, Reverb = 1, Delay = 2 };
enum class DuckPos : int { Post = 0, Pre = 1 };
enum class FreezeMode : int { Freeze = 0, FreezeAdd = 1 };
enum class Quality : int { Eco = 0, High = 1, Ultra = 2 };

struct Settings
{
    // ---- reverb core
    int mode = 1;
    float size = 0.6f;            // 0..1
    float preDelayMs = 20.0f;     // already tempo-resolved
    float decaySec = 6.0f;
    bool freeze = false;
    FreezeMode freezeMode = FreezeMode::Freeze;
    float diffusion = 0.8f;       // 0..1
    float bloomSec = 0.0f;        // 0..5
    float modRateHz = 0.4f, modDepth = 0.3f, drift = 0.6f;
    float lowCutHz = 10.0f, highCutHz = 20000.0f;
    float lowMult = 1.2f, highMult = 0.55f, lowXHz = 300.0f, highXHz = 3500.0f;
    bool dark = false;
    float warmth = 0.0f;
    Quality quality = Quality::High;

    // ---- input / output
    float inHpHz = 10.0f, inLpHz = 20000.0f;
    float tiltDb = 0.0f;
    float width = 1.0f;           // 0..2
    float bassMonoHz = 20.0f;     // <= 20 = off
    float mix = 0.35f;            // 0..1
    float outDb = 0.0f;

    // ---- shimmer
    ShimmerParams shimmer;
    bool shimmerPre = false;

    // ---- grains / delay
    GrainParams grains;
    EchoParams echo;
    float delayToReverb = 1.0f;   // 0..1
    float delayDirectDb = -60.0f; // <= -60 = off

    // ---- clear tail
    bool clearTrigger = false;    // level; the engine detects the rising edge
    bool clearDelayToo = true;

    // ---- ducking
    DuckParams duck;
    DuckSource duckSource = DuckSource::Input;
    DuckTarget duckTarget = DuckTarget::AllWet;
    DuckPos duckPos = DuckPos::Post;
    float lookaheadMs = 0.0f;
};

/** Display data, one frame per ~10 ms (audio thread -> UI). */
struct MeterFrame
{
    float inDb, wetDb, lowDb, midDb, highDb, grDb, corr;
};

class Engine
{
public:
    static constexpr int kChunk = 32;
    static constexpr float kMaxLookaheadMs = 10.0f;

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    /** Eco = 8 tank lines, High/Ultra = 16. Allocates: not on the audio thread. */
    void setQuality (Quality q);
    Quality getQuality() const noexcept { return quality; }

    /** In-place stereo processing. numInputChannels = 1 duplicates L. sc = sidechain (may be null). */
    void process (float* L, float* R, int numSamples, const Settings& s,
                  const float* scL = nullptr, const float* scR = nullptr) noexcept;

    static int lookaheadSamplesFor (float ms, double sampleRate) noexcept
    {
        return (int) std::lround (clampf (ms, 0.0f, kMaxLookaheadMs) * 0.001 * sampleRate);
    }

    double getSampleRate() const noexcept { return fs; }
    int getNumLines() const noexcept { return tank.getNumLines(); }
    const Tank& getTank() const noexcept { return tank; }
    Ducker& getDucker() noexcept { return ducker; }

    int readMeterFrames (MeterFrame* dest, int maxFrames) noexcept { return meterFifo.pop (dest, maxFrames); }
    std::atomic<int> clearCount { 0 };     // number of completed tail clears (tests/UI)
    std::atomic<float> limiterGain { 1.0f };

private:
    void processChunk (float* L, float* R, int n, const Settings& s, const float* scL, const float* scR) noexcept;
    void updateChunkParams (const Settings& s, int n) noexcept;
    void doClear (bool delayToo) noexcept;

    double fs = 48000.0;
    float fsf = 48000.0f;
    Quality quality = Quality::High;

    Diffuser diffuser;
    Bloom bloom;
    Tank tank;
    EchoDelay echo;
    Granulator grains;
    Ducker ducker;
    GrainShifter preShift[4];
    ShiftVoice preVoice[4];

    RingBuffer preDelay[2], lookahead[4];
    float preDelayCur = 0.0f, preDelayNext = 0.0f, preDelayX = 1.0f;

    Svf inHp[2], inLp[2], bassMonoHp;
    Butter8 darkIn[2], darkOut[2];
    OnePole tiltLp[2];
    OnePole meterLp[2], meterHp[2];

    // smoothed values (chunk rate) and per-sample ramps
    Smoother sMix, sOut, sTilt, sWidth, sEarly, sDelRev, sDelDir, sGate, sDark, sPreShim, sDiffMix, sInHp, sInLp;
    Ramp rDry, rWet, rTiltLo, rTiltHi, rWidth, rEarly, rDelRev, rDelDir, rGate, rDark, rPreShim, rDiffMix;
    bool firstChunk = true;
    int chunkPos = 0;   // position inside the current parameter chunk

    // clear tail
    enum class ClearState { Idle, Fading };
    ClearState clearState = ClearState::Idle;
    float clearGain = 1.0f;
    bool lastClearTrigger = false;
    bool clearDelayPending = true;
    int activeMode = -1, pendingMode = 0;
    bool modeSwitchPending = false;

    // per-chunk scratch
    float duckGain[kChunk] {};
    float detL[kChunk] {}, detR[kChunk] {};
    float grainFeedL = 0.0f, grainFeedR = 0.0f;

    // meters
    int meterLen = 480, meterCount = 0;
    double mIn = 0, mWet = 0, mLow = 0, mMid = 0, mHigh = 0, mLR = 0, mLL = 0, mRR = 0;
    SpscFifo<MeterFrame, 1024> meterFifo;

    Settings last;
};
} // namespace schimmer::dsp
