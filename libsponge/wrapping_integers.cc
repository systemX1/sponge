#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The 32-bit initial sequence number
//! \retval [WrappingInt32] The relative 32-bit sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
     return WrappingInt32{static_cast<uint32_t>((n & UINT32_MAX) + isn.raw_value() ) };
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The 32-bit relative sequence number
//! \param isn The 32-bit initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \retval [uint64_t] the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t offset = n.raw_value() - static_cast<uint32_t>((checkpoint & UINT32_MAX) + isn.raw_value() );
    uint64_t ret = checkpoint + offset;
    if (ret >= (1ul << 32) && offset >= (1u << 31) )
        ret -= (1ul << 32);
    return ret;
}
