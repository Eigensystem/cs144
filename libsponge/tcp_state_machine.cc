#include "tcp_state_machine.hh"
#include "tcp_connection.hh"
#include <iostream>
#include <limits>

bool TCP_State_Machine::operator==(const TState state) const {
  return _state == state;
}

bool TCP_State_Machine::operator!=(const TState state) const {
  return _state != state;
}

void TCP_State_Machine::send_segment(TCPConnection &conn, bool ack) {
  TCPSegment &seg = conn._sender.segments_out().front();
  seg.header().win = conn._receiver.window_size() > std::numeric_limits<uint16_t>::max() ?
                    std::numeric_limits<uint16_t>::max() : conn._receiver.window_size();
  if (ack) {
    try {
      seg.header().ack = 1;
      seg.header().ackno = conn._receiver.ackno().value();
    } catch (const std::exception &e) {
      std::cerr << "Can not send segment with ACK before receiving SYN. Exception: "
                << e.what() << std::endl;
    }
  }
  if (seg.header().fin == 1) {
    if (conn._receiver._fin == 0) EstToFINwait1();
    else CloseWaitToLastACK();
  }
  conn._segments_out.push(seg);
  conn._sender.segments_out().pop();
}

//! 3-way handshake
void TCP_State_Machine::ClosedToListen() {
  _active = 1;
  _state = TState::LISTEN;
}

void TCP_State_Machine::ClosedToSYNsent(TCPConnection &conn) {
  conn._sender.fill_window();
  send_segment(conn, false);
  _state = TState::SYN_SENT;
}

void TCP_State_Machine::ListenToSYNrecv(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  send_segment(conn, true);
  _state = TState::SYN_RECEIVED;
}

void TCP_State_Machine::SYNsentToEst(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  if (conn._sender.segments_out().empty())
    conn._sender.send_empty_segment();
  while (!conn._sender.segments_out().empty())
    send_segment(conn, true);
  _state = TState::ESTABLISHED;
}

void TCP_State_Machine::SYNrecvToEst(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  while (!conn._sender.segments_out().empty())
    send_segment(conn, true);
  _state = TState::ESTABLISHED;
}

void TCP_State_Machine::SYNrecvToFINwait1() {
  _state = TState::FIN_WAIT_1;
}

//! Passive Close
void TCP_State_Machine::EstToCloseWait(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  while (!conn._sender.segments_out().empty())
    send_segment(conn, true);
  conn._linger_after_streams_finish = false;
  _state = TState::CLOSE_WAIT;
}

void TCP_State_Machine::CloseWaitToLastACK() {
  _state = TState::LAST_ACK;
}

void TCP_State_Machine::LastACKToClosed(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  _active = 0;
  _state = TState::CLOSED;
}

//! Active Close
void TCP_State_Machine::EstToFINwait1() {
  _state = TState::FIN_WAIT_1;
}

void TCP_State_Machine::FINwait1ToFINwait2(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  _state = TState::FIN_WAIT_2;
}

void TCP_State_Machine::FINwait1ToClosing(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  conn._sender.send_empty_segment();
  send_segment(conn, true);
  _state = TState::CLOSING;
}

void TCP_State_Machine::FINwait1ToTimeWait(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  conn._sender.send_empty_segment();
  send_segment(conn, true);
  _state = TState::TIME_WAIT;
}

void TCP_State_Machine::FINwait2ToTimeWait(TCPConnection &conn, const TCPSegment &seg) {
  conn._sender.ack_received(seg.header().ackno, seg.header().win);
  conn._receiver.segment_received(seg);
  conn._sender.send_empty_segment();
  send_segment(conn, true);
  _state = TState::TIME_WAIT;
}

void TCP_State_Machine::TimeWaitToClosed() {
  _active = 0;
  _state = TState::CLOSED;
}

//! RESET
void TCP_State_Machine::ToReset(TCPConnection &conn) {
  _active = 0;
  _state = TState::RESET;
  conn._sender.stream_in().set_error();
  conn._receiver.stream_out().set_error();
}
