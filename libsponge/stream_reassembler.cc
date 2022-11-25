#include "stream_reassembler.hh"
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;


void WaitManager::add_string(size_t index, string data, size_t idx) {
  // fix _manager start on idx offset
  if (_start < idx) {
    _manager.assign(_manager.begin() + idx - _start, _manager.end());
    _start = idx;
  }
  size_t begin = index - _start;
  size_t end = index + data.size() - _start;
  size_t count = 0;
  for (size_t i = begin; i < end ; ++i) {
    if (_manager[i]) ++count;
  }
  if (count == 0) {
    fill(_manager.begin() + begin, _manager.begin() + end, 1);
    _unassembled_bytes += data.size();
    //check bounder, combine substring with the front and behind one
    if (!_manager[end] && !_manager[begin-1]) {
      _substrings[begin + _start] = data;
    }
    if (_manager[end]) {
      _substrings[begin + _start] = data + _substrings[end + _start];
      _substrings.erase(end + _start);
    }
    if (_manager[begin - 1]) {
      for (int i = begin - 1; i > 0; --i) {
        if (!_manager[i - 1]) {
          _substrings[i + _start] += _substrings[begin + _start];
          _substrings.erase(begin + _start);
          break;
        }
        // the first bool value of _manager always be false
        //  because if a substring index equal to idx, it will be 
        //  added to the output stream immediately
        // if (i - 1 == 0 && _manager[i - 1]) {}
      }
    }
  }
  
  else if (count != data.size()) {
    _unassembled_bytes -= count;
    _unassembled_bytes += data.size();
    //check bounder, combine substring with the front and behind one
    if (!_manager[end] && !_manager[begin - 1]) {
      _substrings[begin + _start] = data;
    }
    else {
      if (_manager[end]) {
        for (size_t i = end; i > begin; --i) {
          if (!_manager[i - 1]) {
            try
            {
              _substrings[begin + _start] = data + _substrings[i + _start].substr(end - i);
            }
            catch(const std::exception& e)
            {
              cout << e.what() << '\n';
              cout << "_start:" << _start << " _idx:" << idx <<"\n";
              cout << data<< "\n"<< "size:"<< data.size()<< " index:"<< index<< " end:"<< end+_start<< "\n";
              cout << "merge string start:" << _start + i << " size:"<< _substrings[i+_start].size()<<"\n";
              cout << "flags:" << "\n";
              for (size_t j = begin; j <= end; ++j) {
                cout << _manager[j] << " ";
              }
              cout << "\n";
              for (auto iter = _substrings.lower_bound(index); iter != _substrings.upper_bound(end+_start); ++i) {
                cout << " "<< iter->first << " " << iter->second.size() << " " << "***";
              }
              cout << "\n******************************\n";
            }
            
            break;
          }
        }
      }
      if(_manager[begin - 1]) {
        for (int i = begin - 1; i > 0; --i) {
          if (!_manager[i - 1]) {
            _substrings[i + _start] = _substrings[i + _start].substr(0, begin - i) + _substrings[begin + _start];
            _substrings.erase(begin + _start);
            break;
          }
        }
      }
    }
    for (size_t i = begin; i < end; ++i) {
      if(_manager[i] && !_manager[i - 1]) {
        _substrings.erase(i + _start);
      }
    }
    fflush(stdout);
    fill(_manager.begin() + begin, _manager.begin() + end, 1);
  }
}

string WaitManager::pop_string(size_t last_idx, size_t idx) {
  auto lower = _substrings.lower_bound(last_idx);
  auto upper = _substrings.upper_bound(idx);
  if (last_idx == idx || lower == upper) {
    return "";
  }
  auto iter = upper;
  iter--;
  size_t index = iter->first;
  string pop_str = iter->second;
  auto i = lower;
  while (i != upper) {
    _unassembled_bytes -= i->second.size();
    _substrings.erase(i++);
  }
  if (index + pop_str.size() <= idx) {
    return "";
  }
  // cout << last_idx << " " << index << " " << pop_str;
  pop_str = pop_str.substr(idx - index);
  return pop_str;
}

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
  if (eof) {
    _eof = index + data.size();
  }
    if (data == "") {
      if (_idx >= _eof) _output.end_input();
      return;
    }
    //add sub string to output stream directly
    if (index == _idx) {
      if (_output.buffer_size() + data.size() > _capacity) {
        //TODO: check cap of manager?
        _last_idx = _idx;
        _idx += _capacity - _output.buffer_size();
        _output.write(data.substr(0, _capacity - _output.buffer_size()));
      }
      else {
        _output.write(data);
        _last_idx = _idx;
        _idx += data.size();
      }
    }
    else if (index < _idx && index + data.size() > _idx) {
      if (_output.buffer_size() + index + data.size() - _idx > _capacity) {
        _last_idx = _idx;
        _idx += _capacity - _output.buffer_size();
        _output.write(data.substr(_last_idx - index, _capacity - _output.buffer_size()));
      }
      else {
        _output.write(data.substr(_idx - index));
        _last_idx = _idx;
        _idx = index + data.size();
      }
    }
    //cannot match the sequence, add substring to waiting queue
    else if (_idx < index && _output.buffer_size() + _manager.unassembled_bytes() 
                            + data.size() <= _capacity) {
      _manager.add_string(index, data, _idx);
    }

  // dealing with the sub string in wait queue
  string str = _manager.pop_string(_last_idx, _idx);
  // cout << "3";
  if (str != "") {
    if (_output.buffer_size() + str.size() <= _capacity) {
      _last_idx = _idx;
      _idx += str.size();
      _output.write(str);
    }
    else {
      _last_idx = _idx;
      _idx += _capacity - _output.buffer_size();
      _output.write(str.substr(0, _capacity - _output.buffer_size()));
    }
  }
  
  // cout << "4";
  if (_idx >= _eof) {
    _output.end_input();
  }
}

size_t StreamReassembler::unassembled_bytes() const { return _manager.unassembled_bytes(); }

bool StreamReassembler::empty() const { return _manager.empty(); }
