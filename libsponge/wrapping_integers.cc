#include "wrapping_integers.hh"

#include <cassert>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

namespace {
inline uint64_t distance(uint64_t a, uint64_t b) { return a > b ? a - b : b - a; }
}  // namespace

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t max = 0x100000000;
    n = n % max;

    uint32_t start = isn.raw_value();
    if (max - start > n) {
        return WrappingInt32{static_cast<uint32_t>(start + n)};
    } else {
        return WrappingInt32{static_cast<uint32_t>(n - (max - start))};
    }
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t max = 0x100000000;
    uint32_t n_raw = n.raw_value();
    uint32_t isn_raw = isn.raw_value();

    uint64_t d =
        n_raw < isn_raw ? static_cast<uint64_t>(n_raw + max - isn_raw) : static_cast<uint64_t>(n_raw - isn_raw);

    d = d | (checkpoint & 0xFFFFFFFF00000000);

    uint64_t left = d > max ? d - max : d;
    uint64_t right = (d & 0xFFFFFFFF00000000) != 0xFFFFFFFF00000000 ? d + max : d;

    uint64_t d_a = distance(left, checkpoint);
    uint64_t d_b = distance(d, checkpoint);
    uint64_t d_c = distance(right, checkpoint);

    if (d_a > d_b) {
        if (d_b > d_c) {
            return right;
        } else {
            return d;
        }
    } else {
        if (d_a > d_c) {
            return right;
        } else {
            return left;
        }
    }
}
