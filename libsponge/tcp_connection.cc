#include "tcp_connection.hh"
#include <limits>
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

void TCPConnection::send_segments(bool rst) {
  while (!_sender.segments_out().empty() && !rst) {
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    if (_receiver.window_size() > numeric_limits<uint16_t>::max()) 
      seg.header().win = numeric_limits<uint16_t>::max();
    else
      seg.header().win = _receiver.window_size();
    //! ack and ackno should be sent when ackno has value(syn already received) even if 
    //!   connection has not received a segment before sending this segment(after sending last segment)
    if (_receiver.ackno().has_value()) {
      seg.header().ack = 1;
      seg.header().ackno = _receiver.ackno().value();
    }
    _segments_out.push(seg);
  }
  if (rst) {
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    seg.header().rst = 1;
    _segments_out.push(seg);
  }
}

void TCPConnection::segment_received(const TCPSegment &seg) {
  _time_since_last_segment_received = 0;
  //! if receive some segments before sending SYN, just drop them.
  if (_sender.next_seqno_absolute() == 0)
    return;
  //! RST received, abort connnection immediately.
  if (seg.header().rst) {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    return;
  }
  //! if received ack, sender should be notified to restart count down, update seqno
  //!   should be sent next, update window size, and deal with retransmission
  //! We will use segments_out in sender later. After received ack, sender will satisfy
  //!   remote peer's window size and fill segments_out.
  if (seg.header().ack) _sender.ack_received(seg.header().ackno, seg.header().win);
  _sender.fill_window();
  //! if acknowledge number of segment fit in receiver window, receive it.
  if (seg.header().syn 
      || (_receiver.ackno().has_value() 
          && static_cast<size_t>(seg.header().seqno - _receiver.ackno().value()) < _receiver.window_size()))
    _receiver.segment_received(seg);
  //* FIN processing
  _linger_time = 0;
  if (seg.header().fin) {
    //! If no bytes in flight and stream_in has reached EOF, the FIN has been acknowledged by remote
    //! which means this peer has to linger after sending ACK of remote's FIN to avoid lost of ACK
    if (_sender.stream_in().bytes_read() + 2 == _sender.next_seqno_absolute()) {
      _linger_after_streams_finish = true;
      _linger = 1;
      //! if we received FIN again after start to linger, reset linger time to 0 because remote have 
      //! not received ACK of FIN
    }
    //! This peer have not FIN-ed yet while remote peer FIN-ed. Can deliver message after ACK remote's FIN.
    //! It is not necessary to linger when this peer finish.
    else _linger_after_streams_finish = false;
    _receiver.stream_out().end_input();
  } 
  //* ACK the segment
  //! ackno has value indicate syn has received(bytestream in this direction has established)
  //! if a segment occupy seqno or do not occupy seqno but has a invaild seqno(keep alive), 
  //! ack should be sent
  if (_receiver.ackno().has_value()
      && (seg.length_in_sequence_space() != 0
          || seg.header().seqno.raw_value() < _receiver.ackno().value().raw_value())) {
    if (_sender.segments_out().empty()) _sender.send_empty_segment();
    send_segments(0);
  }
}

bool TCPConnection::active() const {
  bool flag = 1;
  if (_sender.stream_in().error() || _receiver.stream_out().error()) flag = 0;
  if (!_linger_after_streams_finish 
      && _sender.stream_in().eof() && !_sender.bytes_in_flight()
      && _receiver.stream_out().eof()) flag = 0;
  return flag;
}

size_t TCPConnection::write(const string &data) {
  return _sender.stream_in().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
  _time_since_last_segment_received += ms_since_last_tick;
  _sender.tick(ms_since_last_tick);
  //! if we start to linger, count down
  if (_linger) {
    _linger_time += ms_since_last_tick;
    if (_linger_time >= 10 * static_cast<size_t>(_cfg.rt_timeout)) _linger_after_streams_finish = 0;
  }
  if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
    _sender.send_empty_segment();
    send_segments(1);
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    return;
  }
  _sender.fill_window();
  send_segments(0);
}

void TCPConnection::end_input_stream() {
  _sender.stream_in().end_input();
  _sender.fill_window();
  send_segments(0);
}

void TCPConnection::connect() { 
  if (_sender.next_seqno_absolute() == 0) {
    _sender.fill_window();
    send_segments(0);
  }
}

TCPConnection::~TCPConnection() {
  try {
    if (active()) {
      cerr << "Warning: Unclean shutdown of TCPConnection\n";
      if (_sender.segments_out().empty()) _sender.send_empty_segment();
      send_segments(1);
      _sender.stream_in().set_error();
      _receiver.stream_out().set_error();
    }
  } catch (const exception &e) {
    std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
  }
}
