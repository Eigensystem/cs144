#include "tcp_receiver.hh"
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
  auto header = seg.header();
  if (header.syn) {
    _syn = 1;
    _isn = header.seqno;
  }

  if (_syn) {
    if (header.fin) {
      _fin = 1;
    }
    uint64_t abs_seqno = unwrap(header.seqno, _isn, stream_out().bytes_written());
    size_t index;
    if (header.syn) 
      index = abs_seqno;
    else
      index = abs_seqno - 1;
    
    string payload = seg.payload().copy();
    _reassembler.push_substring(payload, index, _fin);
  }
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
  // cout << "\n" << stream_out().bytes_written() + 1 << " "<< _isn<<endl;
  // cout << wrap(stream_out().bytes_written() + 1, _isn);
  if (_syn) {
    uint64_t ackno = (_fin && !_reassembler.unassembled_bytes()) ? 
                      stream_out().bytes_written() + 2 : 
                      stream_out().bytes_written() + 1;
    return wrap(ackno, _isn);
  }
  else
    return nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }