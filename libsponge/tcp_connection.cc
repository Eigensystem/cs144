#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { 
  return _sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { 
  return _time_since_last_segment_received; 
}

void TCPConnection::segment_received(const TCPSegment &seg) {
  _time_since_last_segment_received = 0;
  //! if received ack, sender should be notified to restart count down, update seqno
  //! should be sent next, update window size, and deal with retransmission
  if (seg.header().ack) _sender.ack_received(seg.header().ackno, seg.header().win);
  if (seg.header().rst) {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
  }
  _receiver.segment_received(seg);
  //! ackno has value indicate syn has received(bytestream in this direction has established)
  //! if a segment occupy seqno or do not occupy seqno but has a invaild seqno(keep alive), 
  //! ack should be sent
  if (_receiver.ackno().has_value()
      && (seg.length_in_sequence_space() != 0
          || seg.header().seqno.raw_value() < _receiver.ackno().value().raw_value())) {
    if (_sender.segments_out().empty()) _sender.send_empty_segment();
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    seg.header().ack = 1;
    seg.header().ackno = _receiver.ackno().value();
    seg.header().win = _receiver.window_size();
    _segments_out.push(seg);
  }
}

bool TCPConnection::active() const { return {}; }

size_t TCPConnection::write(const string &data) {
  return _sender.stream_in().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
  _time_since_last_segment_received += ms_since_last_tick;
  _sender.tick(ms_since_last_tick);
}

void TCPConnection::end_input_stream() {}

void TCPConnection::connect() { _sender.fill_window(); }

TCPConnection::~TCPConnection() {
  try {
    if (active()) {
      cerr << "Warning: Unclean shutdown of TCPConnection\n";

      // Your code here: need to send a RST segment to the peer
    }
  } catch (const exception &e) {
    std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
  }
}
