#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool RetransmissionTimer::tick(const size_t period_in_ms) {
  if (_on) {
    _countdown -= period_in_ms;
    if (_countdown <= 0) return 1;
  }
  return 0;
}


//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

void TCPSender::create_segment(size_t size) {
  TCPSegment seg;
  if (syn()) {
    seg.header().syn = 1;
    --_window_size;
  }
  size = size > TCPConfig::MAX_PAYLOAD_SIZE ? TCPConfig::MAX_PAYLOAD_SIZE : size;
  size = size > _window_size ? _window_size : size;
  _window_size -= size;
  seg.payload() = _stream.read(size);
  if (_window_size > 0 && fin()) {
    _finished = 1;
    seg.header().fin = 1;
    --_window_size;
  }
  seg.header().seqno = next_seqno();
  _next_seqno += seg.length_in_sequence_space();
  _segments_out.push(seg);
  _retransmissionMng[_next_seqno] = seg;
  if (!_timer.statu()) _timer.start();
}

void TCPSender::fill_window() {
  if (syn()) {
    _window_size = 1;
    create_segment(0);
  }
  if (_window_size == 0 && _retransmissionMng.size() == 0) _window_size = 1;
  while (!_finished && _window_size > 0 && (_stream.buffer_size() > 0 || fin())) {
    int payload_size = _window_size > _stream.buffer_size() ? _stream.buffer_size() : _window_size;
    create_segment(payload_size);
  }
  // if (!finished && fin()) {
    // create_segment(0);
    // finished = 1;
  // }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
  size_t abs_ackno = unwrap(ackno, _isn, _stream.bytes_written());
  if (abs_ackno > _next_seqno) return;
  _window_size = abs_ackno + window_size > _next_seqno ? abs_ackno + window_size - _next_seqno : 0;
  if (_window_size == 0) _zero_window = 1;
  else _zero_window = 0;
  if (abs_ackno > _max_abs_ackno) {
    _max_abs_ackno = abs_ackno;
    _retransmissionMng.erase(_retransmissionMng.begin(), _retransmissionMng.upper_bound(abs_ackno));
    _timer.init_time(_initial_retransmission_timeout);
    if (_retransmissionMng.size() == 0) _timer.stop();
    else _timer.reset();
    _retransmissions = 0;
  }

  fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
  if (_timer.tick(ms_since_last_tick)) {
    if (_max_abs_ackno == 0 || _window_size != 0 || (!_zero_window && bytes_in_flight())) {
      ++_retransmissions;
      _timer.exp_backoff();
    }
    _timer.reset();
    _segments_out.push(_retransmissionMng.begin()->second);
  }
  else
    fill_window();
}

void TCPSender::send_empty_segment() {
  TCPSegment seg;
  seg.header().seqno = next_seqno();
  _segments_out.push(seg);
}
