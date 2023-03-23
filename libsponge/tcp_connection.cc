#include "tcp_connection.hh"
#include "tcp_state_machine.hh"
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
  if (seg.header().rst) {
    _fsm.ToReset(*this);
    return;
  }
  // if (seg.header().seqno.raw_value() < _receiver.ackno().value().raw_value()
  //     || seg.header().seqno.raw_value() >= 
  //     _receiver.ackno().value().raw_value() + _receiver.window_size()) {
  //   if (_sender.segments_out().empty()) _sender.send_empty_segment();
  //   while (!_sender.segments_out().empty())
  //     _fsm.send_segment(*this, 1);
  //   return;
  // }
  if (_fsm != TState::FIN_WAIT_1) {
    if (seg.header().syn == 1) {
      if (_fsm == TState::CLOSED) {
        _fsm.ClosedToListen();
        _fsm.ListenToSYNrecv(*this, seg);
      }
      else if (_fsm == TState::LISTEN)
        _fsm.ListenToSYNrecv(*this, seg);
      else if ((_fsm == TState::SYN_SENT || _fsm == TState::ESTABLISHED)
                && seg.header().ackno == wrap(1, _cfg.fixed_isn.value()))
        _fsm.SYNsentToEst(*this, seg);
    }
    else if (seg.header().fin == 1) {
      if ((_fsm == TState::ESTABLISHED || _fsm == TState::CLOSE_WAIT)
          && _sender.next_seqno_absolute() == _sender.stream_in().bytes_read() + 1) {
        _fsm.EstToCloseWait(*this, seg);
      }
      else if ((_fsm == TState::FIN_WAIT_2 || _fsm == TState::TIME_WAIT)
              && seg.header().ackno == wrap(_sender.next_seqno_absolute(), _cfg.fixed_isn.value())) {
        _fsm.FINwait2ToTimeWait(*this, seg, _timewait);
      }
    }
    else if (seg.header().ack == 1) {
      if (_fsm == TState::SYN_RECEIVED && seg.header().ackno == wrap(1, _cfg.fixed_isn.value()))
        _fsm.SYNrecvToEst(*this, seg);
      else if (_fsm == TState::LAST_ACK 
              && seg.header().ackno == wrap(_sender.next_seqno_absolute(), _cfg.fixed_isn.value()))
        _fsm.LastACKToClosed(*this, seg);
      else if (_fsm == TState::CLOSING 
              && seg.header().ackno == wrap(_sender.next_seqno_absolute(), _cfg.fixed_isn.value()))
        _fsm.ClosingToTimeWait(*this, seg);
      else {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        while (!_sender.segments_out().empty())
          _fsm.send_segment(*this, 1);
      }
    }
  }
  else { 
    if (seg.header().ackno == wrap(_sender.next_seqno_absolute(), _cfg.fixed_isn.value())) {
      if (seg.header().fin) _fsm.FINwait1ToTimeWait(*this, seg);
      else _fsm.FINwait1ToFINwait2(*this, seg);
    }
    else if (seg.header().ackno.raw_value() < 
              wrap(_sender.next_seqno_absolute(), _cfg.fixed_isn.value()).raw_value()) {
      if (seg.header().fin) _fsm.FINwait1ToClosing(*this, seg);
    }
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
  if (_fsm == TState::TIME_WAIT && _receiver.unassembled_bytes() == 0) {
    _timewait += ms_since_last_tick;
    if (_timewait >= 10 * _cfg.rt_timeout)
      _fsm.TimeWaitToClosed(*this);
  }
  if (_fsm == TState::ESTABLISHED) {
    _sender.fill_window();
  }
  while (!_sender.segments_out().empty())
    _fsm.send_segment(*this, 1);
}

void TCPConnection::end_input_stream() {
  _sender.stream_in().end_input();
  _sender.fill_window();
  while (!_sender.segments_out().empty())
    _fsm.send_segment(*this, 1);
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
