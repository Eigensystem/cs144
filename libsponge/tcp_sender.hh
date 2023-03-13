#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <map>
#include <queue>

class RetransmissionTimer {
  private:
    long int _countdown;
    size_t _timeout, _init_time;
    bool _on = 0;

  public:
    RetransmissionTimer() = default;
    RetransmissionTimer(const size_t init_timeout)
        : _countdown(init_timeout), _timeout(init_timeout), _init_time(init_timeout) {}

    //! \note return 1 when expired
    bool tick(const size_t period_in_ms);
    bool statu() { return _on; }
    void start() {
        _on = true;
        _timeout = _init_time;
        _countdown = _timeout;
    }
    //! \note stop when all segments has been acknowledged
    void stop() { _on = false; }
    //! \note set countdown to timeout
    void reset() { _countdown = _timeout; }
    //! \note double the timeout and reset countdown
    void exp_backoff() { _timeout *= 2; }
    //! \note set timeout to initial time
    void init_time(const size_t init_timeout) {
        _timeout = init_timeout;
        _init_time = init_timeout;
    }
};

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

    std::map<size_t, TCPSegment> _retransmissionMng{};
    RetransmissionTimer _timer;

    size_t _window_size = 0;
    size_t _max_abs_ackno = 0;
    unsigned int _retransmissions = 0;

    bool _finished = 0;
    bool _zero_window = 0;

    bool syn() { return _next_seqno == 0; }
    bool fin() { return _stream.eof() && _stream.buffer_empty(); }
    void create_segment(size_t size);

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const { return _next_seqno - _max_abs_ackno; };

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const { return _retransmissions; };

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
