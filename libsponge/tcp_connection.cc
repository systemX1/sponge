#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _ms_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if(_state == CLOSED) {
        if (_isActive)
            _state = LISTEN;
        else
            return;
    }
    _ms_since_last_segment_received = 0;
    if(_state != CLOSING) {
        fprintf( stderr, " _state: %s syn: %d ack: %d fin: %d seqno: %u ackno: %u payload: %zu win_size: %hu @seg_recvd\n",
                stateStr.at(_state).c_str(), seg.header().syn, seg.header().ack, seg.header().fin,
                seg.header().seqno.raw_value(), seg.header().ackno.raw_value(),
                seg.payload().size(), seg.header().win);
    }

    // gives the segment to the TCPReceiver and TCPSender
    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno, seg.header().win);

    switch (_state) {
        case LISTEN:
            if (seg.header().syn && _sender.next_seqno_absolute() == 0) {
                connect();
                _state = ESTABLISHED;
                return;
            }
            if(seg.header().rst)
                uncleanShutdown(false);
            break;
        case SYN_SENT:
            if(seg.header().ack && (seg.payload().size() || !_sender.lastAbsAckno()) )  // bad ACKs in SYN_SENT should be ignored
                return;
            if(seg.header().rst) {
                uncleanShutdown(false);
                _state = LISTEN;
                return;
            }
            _sender.send_empty_segment();
            sendSegment();
            _state = ESTABLISHED;
            break;
        case ESTABLISHED: case CLOSE_WAIT:
            // if the rst (reset) flag is set, sets both the inbound and outbound streams to the error
            // state and kills the connection permanently
            if(seg.header().rst) {
                uncleanShutdown(false);
                _state = LISTEN;
                return;
            }
            if(_receiver._isFIN) {
                _sender.send_empty_segment();
                _state = CLOSE_WAIT;
            } else if(seg.length_in_sequence_space())
                _sender.send_empty_segment();
            else if(_receiver.ackno().has_value() && !seg.length_in_sequence_space()
                     && seg.header().seqno == _receiver.ackno().value() - 1)
                _sender.send_empty_segment();
            sendSegment();
            break;
        case LAST_SACK:
            // discard bad ack
            if(!_sender.bytes_in_flight() )
                _state = CLOSED;
            break;
        case FIN_WAIT_1: case FIN_WAIT_2:
            if(seg.header().ack && !seg.header().fin)
                _state = FIN_WAIT_2;
            else if(seg.header().ack && _receiver._isFIN && _receiver.ackno() == seg.header().ackno)
                _state = TIME_WAIT;
            if(seg.length_in_sequence_space()) {
                _sender.send_empty_segment();
                sendSegment();
            }
            break;
        case TIME_WAIT:
            if(seg.header().ack && seg.header().fin) {
                _sender.send_empty_segment();
                sendSegment();
            }
            break;
        default:    break;
    }
    cleanShutdown();
}

bool TCPConnection::active() const { return _isActive; }

size_t TCPConnection::write(const string &data) {
    if(data.empty())
        return 0;
    size_t writeSize = _sender.stream_in().write(data);
    sendSegment();
    return writeSize;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if(!_isActive)
        return;
    _ms_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS)
        uncleanShutdown(true);
    if( ((_ms_since_last_segment_received >=  9 * _cfg.rt_timeout && _ms_since_last_segment_received <= _cfg.rt_timeout * 11) ||  _ms_since_last_segment_received >= _cfg.rt_timeout * 505 )
            && _ms_since_last_segment_received % 10 == 0) {
        cerr << " _state: " << _state
             << " tick: " << _ms_since_last_segment_received << " "
             << "rt_timeout: " << 10 * _cfg.rt_timeout << " "
             << "@tick\n";
    }
    sendSegment();
    cleanShutdown();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    sendSegment();
}

void TCPConnection::connect() {
    if(_sender._isSYN)
        return;
    if(_state == CLOSED && _isActive)
        _state = SYN_SENT;
    else if(_state == LISTEN)
        _state = SYN_RCVD;
    sendSegment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send an RST segment to the peer
            uncleanShutdown(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::sendSegment() {
    if(_state == LISTEN || _state == CLOSED)
        return;
    _sender.fill_window();
    TCPSegment seg;
    // pop all of _sender's segments and push into _segments_out
    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        if(_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }

        if(_state != CLOSING) {
            fprintf( stderr, " _state: %s syn: %d ack: %d fin: %d seqno: %u ackno: %u payload: %zu win_size: %hu @seg_sent\n",
                    stateStr.at(_state).c_str(), seg.header().syn, seg.header().ack, seg.header().fin,
                    seg.header().seqno.raw_value(), seg.header().ackno.raw_value(),
                    seg.payload().size(), seg.header().win);
        }

        if(_isRST) {
            seg.header().rst = true;
            _isRST = false;
        }
        switch (_state) {
            case ESTABLISHED:
                if(seg.header().fin)
                    _state = FIN_WAIT_1;
                break;
            case CLOSE_WAIT:
                if(seg.header().fin)
                    _state = LAST_SACK;
                break;
            default:    break;
        }

        _sender.segments_out().pop();
        _segments_out.push(seg);
    }
    cleanShutdown();
}

void TCPConnection::cleanShutdown() {
    if( _ms_since_last_segment_received % 300 == 0 && _ms_since_last_segment_received) {
        cerr << " _state: " << _state
             << " tick: " << _ms_since_last_segment_received << " "
             << "rt_timeout: " << 10 * _cfg.rt_timeout << " "
             << "@cleanShutdown_called\n";
    }
    // Option B: passive close
    if (_receiver.stream_out().input_ended() && !(_sender.stream_in().eof() ) )
        _linger_after_streams_finish = false;
    // 4 Prerequisites
    if (_sender.stream_in().eof()
        && _receiver.stream_out().input_ended()
        && _sender.bytes_in_flight() == 0
        && (!_linger_after_streams_finish || _ms_since_last_segment_received >= 10 * _cfg.rt_timeout) ) {
            _isActive = false;
            fprintf( stderr, " _state: %u set _isActive=false @cleanShutdown\n", _state);
    }
}

void TCPConnection::uncleanShutdown(bool rst) {
    if(_state == LAST_SACK || _state == TIME_WAIT || _state == FIN_WAIT_2) {
        cerr << " _state: " << _state << " seg: "
             << "tick: " << _ms_since_last_segment_received << " "
             << "rt_timeout: " << 10 * _cfg.rt_timeout << " "
             << "@uncleanShutdown_called\n";
    }
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    if(rst) {
        _isRST = true;
        if(_sender.segments_out().empty())  // if _sender's queue is empty, add an empty segment to carry RST flag
            _sender.send_empty_segment();
        sendSegment();
    }
    _isActive = false;
}
