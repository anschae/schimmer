#pragma once
// Power-of-two ring buffer with an absolute (int64) write position.
//
// Clearing is O(1): clearFromNow() records the current position, and every read
// of a sample written before that position returns 0. That makes "Clear Tail"
// instantaneous and free of CPU spikes, whatever the buffer size.

#include "Common.h"

namespace schimmer::dsp
{
class RingBuffer
{
public:
    void allocate (int minSamples)
    {
        const int size = nextPow2 (std::max (16, minSamples + 8));
        data.assign ((size_t) size, 0.0f);
        mask = size - 1;
        now = 0;
        validFrom = 0;
    }

    void reset() noexcept
    {
        std::fill (data.begin(), data.end(), 0.0f);
        now = 0;
        validFrom = 0;
    }

    /** Everything written so far reads as silence from now on (O(1)). */
    void clearFromNow() noexcept { validFrom = now; }

    int capacity() const noexcept { return (int) data.size(); }
    int64_t position() const noexcept { return now; }

    inline void write (float x) noexcept
    {
        data[(size_t) (now & mask)] = x;
        ++now;
    }

    /** Sample at an absolute index (must be < position()). */
    inline float at (int64_t idx) const noexcept
    {
        return idx < validFrom ? 0.0f : data[(size_t) (idx & mask)];
    }

    /** x[now - d], d >= 1 (d = 1 is the most recent sample). */
    inline float readInt (int d) const noexcept { return at (now - d); }

    /** Linear interpolation, d >= 1. */
    inline float readLinear (float d) const noexcept
    {
        const int di = (int) d;
        const float fr = d - (float) di;
        const int64_t i = now - di;           // x[now-di]   (newer)
        const float a = at (i), b = at (i - 1); // x[now-di-1] (older)
        return a + fr * (b - a);
    }

    /** Cubic Hermite interpolation, d >= 2. */
    inline float readHermite (float d) const noexcept
    {
        const int di = (int) d;
        const float fr = d - (float) di;
        const int64_t i = now - di;
        const float y2 = at (i + 1), y1 = at (i), y0 = at (i - 1), ym1 = at (i - 2);
        return hermite (ym1, y0, y1, y2, 1.0f - fr);
    }

    inline float read (float d, bool hermiteInterp) const noexcept
    {
        return hermiteInterp ? readHermite (d) : readLinear (d);
    }

    /** Read at an absolute fractional position (pos < position() - 2). Hermite. */
    inline float readAbsolute (double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const int64_t i = (int64_t) fl;
        const float t = (float) (pos - fl);
        return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), t);
    }

private:
    std::vector<float> data;
    int64_t mask = 0, now = 0, validFrom = 0;
};
} // namespace schimmer::dsp
