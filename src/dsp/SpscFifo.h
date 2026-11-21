#pragma once
// Lock-free single-producer / single-consumer FIFO (audio thread -> UI thread).
// The producer drops items when the FIFO is full (e.g. no editor is open).

#include <atomic>
#include <cstddef>

namespace schimmer::dsp
{
template <typename T, size_t Capacity>
class SpscFifo
{
    static_assert ((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    bool push (const T& item) noexcept
    {
        const size_t w = writePos.load (std::memory_order_relaxed);
        const size_t r = readPos.load (std::memory_order_acquire);
        if (w - r >= Capacity)
            return false;
        items[w & (Capacity - 1)] = item;
        writePos.store (w + 1, std::memory_order_release);
        return true;
    }

    bool pop (T& item) noexcept
    {
        const size_t r = readPos.load (std::memory_order_relaxed);
        const size_t w = writePos.load (std::memory_order_acquire);
        if (r == w)
            return false;
        item = items[r & (Capacity - 1)];
        readPos.store (r + 1, std::memory_order_release);
        return true;
    }

    int pop (T* dest, int maxItems) noexcept
    {
        int k = 0;
        while (k < maxItems && pop (dest[k]))
            ++k;
        return k;
    }

    size_t size() const noexcept
    {
        return writePos.load (std::memory_order_acquire) - readPos.load (std::memory_order_acquire);
    }

private:
    T items[Capacity] {};
    std::atomic<size_t> writePos { 0 }, readPos { 0 };
};
} // namespace schimmer::dsp
