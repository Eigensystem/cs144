#include "wrapping_integers.hh"
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t num = static_cast<uint32_t>(n << 32 >> 32);
    return isn + num;
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
    //! \variable: absolute sequence number % (2 ^ 32)
    uint32_t wrapped_abs = n - isn;
    //! \variable: checkpoint % (2 ^ 32)
    uint32_t wrapped_chk = checkpoint << 32 >> 32;
    //! \variable: transform uint32 to int32: find the closest abs offset to checkpoint
    int32_t off = wrapped_abs - wrapped_chk;
    //! \note: if off is minus, checkpoint + minus must be bigger than 0
    //! \note: overflow check
    if ((checkpoint <= INT32_MAX) && (checkpoint + off > UINT32_MAX)) {
        return checkpoint + static_cast<uint32_t>(off);
    } else {
        return checkpoint + off;
    }
}
