#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <utility>


// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _timer(retx_timeout), _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _segments_out(), _outstandingSegment()
    , _stream(capacity), _nextSeqno(0), _lastAckno(0), _windowSize(1), _bytesinFlight(0), _consecutiveRetransmission(0)
    , _isSYN(false), _isFIN(false) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _bytesinFlight;
}

void TCPSender::fill_window() {
    TCPSegment seg;

    // syn sent yet?
    if(!_isSYN) {
        seg.header().syn = true;
        sendSegment(seg);
        _isSYN = true;
        return;
    }

    size_t windowSize = !_windowSize ? 1 : _windowSize; // 3.2 Page6 *What should I do if the window size is zero?
    size_t validWindowSize = 0;
    while ( (validWindowSize = windowSize - (_nextSeqno - _lastAckno) ) ) {
        if(_isFIN)
            return;
        // _stream.eof() but there was no space for fin in the last time
        if(_stream.eof()) {
            seg.header().fin = true;
            sendSegment(seg);
            _isFIN = true;
            return;
        }
        // read
        size_t readLen = min(TCPConfig::MAX_PAYLOAD_SIZE, validWindowSize );
        string payloadStr = _stream.read(readLen);
        seg.payload() = Buffer(move(payloadStr));

        if(_stream.eof() && validWindowSize > seg.length_in_sequence_space() ) {
            seg.header().fin = true;
            _isFIN = true;
        }
        if(!seg.length_in_sequence_space())
            return;

        sendSegment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if(_isFIN) {
        fprintf(stderr, "_LINE: %d _lastAckno: %lu ackno: %u _bytesinFlight: %zu @sender_ack_received\n",
        __LINE__, _lastAckno, ackno.raw_value(), _bytesinFlight);
    }
    uint64_t absAckno = unwrap(ackno, _isn, _lastAckno);
    uint32_t sameAcknoNum = 0;
    // not in expection
    if(absAckno < _lastAckno || absAckno > _nextSeqno)
        return;
    _windowSize = window_size;
    // no new data ack, but may have new window_size
    if(absAckno == _lastAckno) {
        sameAcknoNum++;
        fill_window();
        if(sameAcknoNum == 3 && !_outstandingSegment.empty()) {
            segments_out().push(_outstandingSegment.front() );
            sameAcknoNum = 0;
        }
        return;
    }
    // acknowledge
    _consecutiveRetransmission = 0;
    _timer._RTO = _timer._initRTO;  // Set the RTO back to its “initial value.”
    _lastAckno = absAckno;

    // remove outstanding segments
    while (!_outstandingSegment.empty() &&
           unwrap(_outstandingSegment.front().header().seqno, _isn, _nextSeqno)
           + _outstandingSegment.front().length_in_sequence_space() <= absAckno) {
        _bytesinFlight -= _outstandingSegment.front().length_in_sequence_space();
        _outstandingSegment.pop();
    }

    // The TCPSender should fill the window again if new space has opened up
    fill_window();
    // If the sender has any outstanding data, restart the retransmission timer
    if(!_outstandingSegment.empty())
        _timer.start();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.trick(ms_since_last_tick);
    if(_timer.isRetransmissionTimeout() && !_outstandingSegment.empty()) {
        segments_out().push(_outstandingSegment.front() );
        if(_windowSize) {  // If the window size is nonzero
            _consecutiveRetransmission++;   // Keep track of the number of consecutive retransmissions, and increment it because you just retransmitted something
            _timer._RTO <<= 1;  // Double the value of RTO, exponential backoff
        }
        _timer.start();     // Reset the retransmission timer and start it
    }
    if(_outstandingSegment.empty())
        _timer.stop();
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutiveRetransmission;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_nextSeqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::sendSegment(TCPSegment &seg) {
    seg.header().seqno = wrap(_nextSeqno, _isn);
    _segments_out.push(seg);
    _outstandingSegment.push(seg);

    _bytesinFlight += seg.length_in_sequence_space();
    _nextSeqno += seg.length_in_sequence_space();   // notice the order
    if(!_timer.isActive())
        _timer.start();
}
