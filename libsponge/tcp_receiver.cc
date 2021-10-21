#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

TCPReceiver::TCPReceiver(size_t capacity):
        _reassembler(capacity), _capacity(capacity),
        _isn(0), _length(0), _netRecvIndex(0), _absSeq(0),
        _isSYN(false), _isFIN(false) {}

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // handle the syn flag
    if(_isSYN && seg.header().syn)  // already syn, reject the others
        return;
    _isSYN |= seg.header().syn;     // save syn state
    if(!_isSYN)                     // hasn't syn yet
        return;
    if(seg.header().syn) {
        _isn = seg.header().seqno;  // save isn
        _netRecvIndex++;
    }

    // push the payload
    _absSeq = unwrap(seg.header().seqno, _isn, _absSeq);
    _length = seg.length_in_sequence_space();
    _reassembler.push_substring(seg.payload().copy(), seg.header().syn ? _absSeq : _absSeq - 1, seg.header().fin);  // prevent underflow when segment with syn carry payload
    _netRecvIndex = _reassembler.firstUnassembledIndex() + _isSYN;  // _reassembler itself won't take the SYN into consideration

    // handle the fin flag
    if(_isFIN && seg.header().fin)  // already fin, reject the others
        return;
    _isFIN |= seg.header().fin;     // save fin state
    if(_reassembler.stream_out().input_ended())
        _netRecvIndex++;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(_isSYN)
        return wrap(_netRecvIndex, _isn);
    return std::nullopt;
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
