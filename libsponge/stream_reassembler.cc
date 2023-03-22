#include "stream_reassembler.hh"
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`
#include <iostream>
using namespace std;


StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

void StreamReassembler::cap_limit() {
  if (_output.buffer_size() + _unassembled_bytes > _capacity) {
    size_t release_size = _output.buffer_size() + _unassembled_bytes - _capacity;
    auto mgr_last = _manager.rbegin();
    auto str_last = _substrings.rbegin();
    while (release_size && !_manager.empty() && !_substrings.empty()) {
      size_t size = mgr_last->second - mgr_last->first;
      if (size > release_size) {
        mgr_last->second -= release_size;
        str_last->second = str_last->second.substr(0, size - release_size);
        _unassembled_bytes -= release_size;
        release_size = 0;
      }
      else {
        _manager.erase(mgr_last++ -> first);
        _substrings.erase(str_last++ -> first);
        _unassembled_bytes -= size;
        release_size -= size;
      }
    }
    return;
  }
}

string StreamReassembler::merge_substr(string data, size_t start, size_t end) {
  if (end <= _current) {
    return "";
  }
  else if (start < _current) {
    data = data.substr(_current - start);
    start = _current;
  }
  
  auto left = _manager.lower_bound(start);  //the first substr begin after(or at) start
  auto right = _manager.upper_bound(end);   //the first substr begin after end
  const size_t lower = start, upper = end;
  bool fwd = 0, bck = 0, valid = 0;
  if (left != _manager.begin()) {
    fwd = 1;
    left--;
  }
  if (right != _manager.begin()) {
    bck = 1;
    right--;
  }
  //expand
  if ((!fwd && !bck) || 
      (!fwd && bck && right->second <= end) ||
      ( fwd && bck && left->second < start && right->second <= end)) {
    valid = 1;
    _unassembled_bytes += data.size();
  }
  //backward merge
  if (bck && right->first >= start && right->second > end) {
    valid = 1;
    end = _manager[right->first];
    data += _substrings[right->first].substr(upper - right->first);
    _unassembled_bytes += data.size();
  }
  //forward merge
  if (fwd && start <= left->second && left->second < end) {
    valid = 1;
    start = left->first;
    data = _substrings[left->first] + data.substr(left->second - lower);
    if (data.size() > upper - lower) {
      _unassembled_bytes -= left->second - lower;
    }
    else {
      _unassembled_bytes += upper - left->second;
    }
  }

  //erase manager element
  auto i = _manager.lower_bound(lower);
  while (i != _manager.upper_bound(upper)) {
    _manager.erase(i++);
  }
  //erase substrings element
  auto j = _substrings.lower_bound(lower);
  while (j != _substrings.upper_bound(upper)) {
    _unassembled_bytes -= j->second.size();
    _substrings.erase(j++);
  }

  if (valid) {
    _manager[start] = end;
    _substrings[start] = data;
  }
  // if (!_manager.empty() && !_substrings.empty())
    cap_limit();
  if (_substrings.begin()->first == _current) {
    string str = _substrings.begin()->second;
    _substrings.erase(_substrings.begin());
    _manager.erase(_manager.begin());
    _unassembled_bytes -= str.size();
    return str;
  }
  else {
    return "";
  }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
  if (eof) {
    _eof = index + data.size();
  }

  if (data != "") {
    string str = merge_substr(data, index, index + data.size());
    if (str != "") {
      _output.write(str);
      _current += str.size();
    }
  }

  if (_current >= _eof) {
    _output.end_input();
  }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes; }
