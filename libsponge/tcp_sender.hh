#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! retransmission timer for the connection
    struct Timer {
        //! initiative retransmission timeout
        uint32_t _initRTO;
        //! retransmission timeout
        uint32_t _RTO;
        //! timeout
        uint32_t _TO;
        //! whether timer is running or not
        bool _isActive;

        explicit Timer(uint32_t initRTO) : _initRTO(initRTO), _RTO(initRTO), _TO(0), _isActive(false) {}
        bool isActive() const { return _isActive;}
        void start() {
            _isActive = true;
            _TO = 0;
        }
        void stop() {
            _isActive = false;
            _TO = 0;
        }
        void trick(size_t ms_since_last_tick) {
            if(!_isActive)
                return;
            _TO += ms_since_last_tick;
        }
        bool isRetransmissionTimeout() {
            if(!_isActive)
                return false;
            if(_TO >= _RTO)
                return true;
            return false;
        }
    };

    //! retransmission timer for the connection
    Timer _timer;
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;
    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out;
    //! outstanding queue of segments that the have been sent but no ack
    std::queue<TCPSegment> _outstandingSegment;
    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;
    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _nextSeqno;
    //! last received ackno
    uint64_t _lastAckno;
    //! the window's size
    size_t _windowSize;
    //! the number of bytes in flight, including syn and fin flag
    size_t _bytesinFlight;
    //! the number of consecutive retransmissions
    size_t _consecutiveRetransmission;

  public:
    //! hasn't syn sent yet
    bool _isSYN;
    //! hasn't syn sent yet
    bool _isFIN;

  public:
    //! Initialize a TCPSender
    TCPSender(size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(WrappingInt32 ackno, uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _nextSeqno; }

    //! \brief absolute seqno for the last acked no
    uint64_t lastAbsAckno() const { return _lastAckno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_nextSeqno, _isn); }
    //!@}

    //!
    void sendSegment(TCPSegment &seg);
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
