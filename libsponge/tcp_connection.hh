#ifndef SPONGE_LIBSPONGE_TCP_FACTORED_HH
#define SPONGE_LIBSPONGE_TCP_FACTORED_HH

#include "tcp_config.hh"
#include "tcp_receiver.hh"
#include "tcp_sender.hh"
#include "tcp_state.hh"
#include <vector>
#include <string>

//! \brief A complete endpoint of a TCP connection
class TCPConnection {
  private:
    TCPConfig _cfg;
    TCPReceiver _receiver{_cfg.recv_capacity};
    TCPSender _sender{_cfg.send_capacity, _cfg.rt_timeout, _cfg.fixed_isn};

//    enum State : unsigned {
//        LISTEN = 0ul, SYN_RCVD = 1ul, SYN_SENT = 1ul << 1, ESTABLISHED = 1ul << 2, CLOSE_WAIT = 1ul << 3, LAST_SACK = 1ul << 4,
//        FIN_WAIT_1 = 1ul << 5, FIN_WAIT_2 = 1ul << 6, CLOSING = 1ul << 7, TIME_WAIT = 1ul << 8, CLOSED = 1ul << 9, RESET = 1ul << 10
//    };
    enum State{
        LISTEN = 0ul, SYN_RCVD, SYN_SENT, ESTABLISHED, CLOSE_WAIT, LAST_SACK,
        FIN_WAIT_1, FIN_WAIT_2, CLOSING, TIME_WAIT, CLOSED
    };
    std::vector<std::string> stateStr = {"LISTEN", "SYN_RCVD", "SYN_SENT", "ESTABLISHED", "CLOSE_WAIT", "LAST_SACK",
                                    "FIN_WAIT_1", "FIN_WAIT_2", "CLOSING", "TIME_WAIT", "CLOSED"};

    State _state{CLOSED};
    //! whether the TCPConnection is active
    bool _isActive{true};
    //!  ms since last segment received
    size_t _ms_since_last_segment_received{0};
    //! outbound queue of segments that the TCPConnection wants sent
    std::queue<TCPSegment> _segments_out{};
    //! Should the TCPConnection stay active (and keep ACKing)
    //! for 10 * _cfg.rt_timeout milliseconds after both streams have ended,
    //! in case the remote TCPConnection doesn't know we've received its whole stream?
    bool _linger_after_streams_finish{true};
    //!
    bool _isRST{false};

  public:
    //! \name "Input" interface for the writer
    //!@{

    //! \brief Initiate a connection by sending a SYN segment
    void connect();

    //! \brief Write data to the outbound byte stream, and send it over TCP if possible
    //! \returns the number of bytes from `data` that were actually written.
    size_t write(const std::string &data);

    //! \returns the number of `bytes` that can be written right now.
    size_t remaining_outbound_capacity() const;

    //! \brief Shut down the outbound byte stream (still allows reading incoming data)
    void end_input_stream();
    //!@}

    //! \name "Output" interface for the reader
    //!@{

    //! \brief The inbound byte stream received from the peer
    ByteStream &inbound_stream() { return _receiver.stream_out(); }
    //!@}

    //! \name Accessors used for testing

    //!@{
    //! \brief number of bytes sent and not yet acknowledged, counting SYN/FIN each as one byte
    size_t bytes_in_flight() const;
    //! \brief number of bytes not yet reassembled
    size_t unassembled_bytes() const;
    //! \brief Number of milliseconds since the last segment was received
    size_t time_since_last_segment_received() const;
    //!< \brief summarize the state of the sender, receiver, and the connection
    TCPState state() const { return {_sender, _receiver, active(), _linger_after_streams_finish}; };
    //!@}

    //! \name Methods for the owner or operating system to call
    //!@{

    //! Called when a new segment has been received from the network
    void segment_received(const TCPSegment &seg);

    //! Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);

    //! \brief TCPSegments that the TCPConnection has enqueued for transmission.
    //! \note The owner or operating system will dequeue these and
    //! put each one into the payload of a lower-layer datagram (usually Internet datagrams (IP),
    //! but could also be user datagrams (UDP) or any other kind).
    std::queue<TCPSegment> &segments_out() { return _segments_out; }

    //! \brief Is the connection still alive in any way?
    //! \returns `true` if either stream is still running or if the TCPConnection is lingering
    //! after both streams have finished (e.g. to ACK retransmissions from the peer)
    bool active() const;
    //!@}

    //! fill the _sender'window, pop all of _sender's segments in the queue then push into _segments_out
    void sendSegment();
    //!
    void cleanShutdown();
    //!
    void uncleanShutdown(bool rst);
    //! Construct a new connection from a configuration
    explicit TCPConnection(const TCPConfig &cfg) : _cfg{cfg} {}

    //! \name construction and destruction
    //! moving is allowed; copying is disallowed; default construction not possible

    //!@{
    ~TCPConnection();  //!< destructor sends a RST if the connection is still open
    TCPConnection() = delete;
    TCPConnection(TCPConnection &&other) = default;
    TCPConnection &operator=(TCPConnection &&other) = default;
    TCPConnection(const TCPConnection &other) = delete;
    TCPConnection &operator=(const TCPConnection &other) = delete;
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_FACTORED_HH
