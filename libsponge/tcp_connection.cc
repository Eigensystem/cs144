#include "tcp_connection.hh"

#include <iostream>
#include <limits>
// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection::sendRst() {
  if (_sender.segments_out().empty()) _sender.send_empty_segment();
  TCPSegment seg = _sender.segments_out().front();
  seg.header().rst = 1;
  _segments_out.push(seg);
  _fsm.ToReset(*this);
}


size_t TCPConnection::remaining_outbound_capacity() const { 
  return _receiver.stream_out().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { 
  return _sender.bytes_in_flight(); 
}

size_t TCPConnection::unassembled_bytes() const { 
  return _receiver.unassembled_bytes(); 
}

size_t TCPConnection::time_since_last_segment_received() const { return _timer; }

void TCPConnection::segment_received(const TCPSegment &seg) {
  _timer = 0;
  if (seg.header().syn == 1) {
    if (_fsm == State::CLOSED) {
      _fsm.ClosedToListen();
      _fsm.ListenToSYNrecv(*this, seg);
    }
    else if (_fsm == State::LISTEN)
      _fsm.ListenToSYNrecv(*this, seg);
    else if (_fsm == State::SYN_SENT && seg.header().ackno == wrap(1, _cfg.fixed_isn.value()))
      _fsm.SYNsentToEst(*this, seg);
  }
  if (seg.header().ack == 1) {
    if (_fsm == State::SYN_RECEIVED && seg.header().ackno == wrap(1, _cfg.fixed_isn.value()))
      _fsm.SYNrecvToEst(*this, seg);
    else if (_fsm == State::ESTABLISHED && )
  }
  if (seg.header().fin == 1) {
    if (_fsm == State::ESTABLISHED 
        && _sender.next_seqno_absolute() == _sender.stream_in().bytes_read() + 1) {
      _fsm.EstToCloseWait(*this, seg);
    }
    else if (_fsm == State::ESTABLISHED && )
  }
}

bool TCPConnection::active() const { return _fsm.active(); }

size_t TCPConnection::write(const string &data) {
    return _sender.stream_in().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
  _timer += ms_since_last_tick;
  _sender.tick(ms_since_last_tick);
  if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS)
    sendRst();
  if (_fsm == State::TIME_WAIT) {
    _timewait += ms_since_last_tick;
    if (_timewait >= 10 * _cfg.rt_timeout)
      _fsm.TimeWaitToClosed();
  }
  if (_fsm == State::ESTABLISHED) {
    while (!_sender.segments_out().empty())
      _fsm.send_segment(*this, 1);
  }
}

void TCPConnection::end_input_stream() {
  _receiver.stream_out().end_input();
}

void TCPConnection::connect() {
  _fsm.ClosedToSYNsent(*this);
}

TCPConnection::~TCPConnection() {
  try {
    if (active()) {
      cerr << "Warning: Unclean shutdown of TCPConnection\n";

      sendRst();
    }
  } catch (const exception &e) {
    std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
  }
}
