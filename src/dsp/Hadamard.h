#pragma once
// Orthonormal Hadamard transform (fast Walsh-Hadamard), N = power of two.

#include "Common.h"

namespace schimmer::dsp
{
template <int N>
inline void hadamardInPlace (float* x) noexcept
{
    static_assert ((N & (N - 1)) == 0, "N must be a power of two");
    for (int h = 1; h < N; h <<= 1)
        for (int i = 0; i < N; i += h << 1)
            for (int j = i; j < i + h; ++j)
            {
                const float a = x[j], b = x[j + h];
                x[j] = a + b;
                x[j + h] = a - b;
            }
    const float norm = 1.0f / std::sqrt ((float) N);
    for (int i = 0; i < N; ++i)
        x[i] *= norm;
}

/** Runtime N (8 or 16). */
inline void hadamard (float* x, int n) noexcept
{
    if (n == 16) hadamardInPlace<16> (x);
    else if (n == 8) hadamardInPlace<8> (x);
    else if (n == 4) hadamardInPlace<4> (x);
}

/** Sign (+1/-1) of entry (row, col) of the (unnormalised) Sylvester Hadamard matrix. */
inline float hadamardSign (int row, int col) noexcept
{
    // popcount parity of row & col
    unsigned v = (unsigned) (row & col);
    int parity = 0;
    while (v) { parity ^= 1; v &= v - 1; }
    return parity ? -1.0f : 1.0f;
}
} // namespace schimmer::dsp
