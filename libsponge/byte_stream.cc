#include "byte_stream.hh"

#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) : cap(capacity) {}

size_t ByteStream::write(const string &data) {
    int accept;
    if (data.size() <= remaining_capacity()) {
        accept = data.size();
        this->stream += data;
    } else {
        accept = remaining_capacity();
        this->stream += data.substr(0, accept);
    }
    written += accept;
    return accept;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string str;
    if (len > this->stream.size()) {
        str = this->stream;
    } else {
        str = this->stream.substr(0, len);
    }
    return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len > this->stream.size()) {
        poped += this->stream.size();
        this->stream = "";
    } else {
        this->stream = this->stream.substr(len, this->stream.size() - len);
        poped += len;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string str;
    int pop;
    if (len > this->stream.size()) {
        pop = this->stream.size();
        str = this->stream;
        this->stream = "";
    } else {
        str = this->stream.substr(0, len);
        this->stream = this->stream.substr(len, this->stream.size() - len);
        pop = len;
    }
    poped += pop;
    return str;
}

void ByteStream::end_input() { _end = true; }

bool ByteStream::input_ended() const { return _end; }

size_t ByteStream::buffer_size() const { return this->stream.size(); }

bool ByteStream::buffer_empty() const { return !this->stream.size(); }

bool ByteStream::eof() const { return (_end && !this->stream.size()) ? true : false; }

size_t ByteStream::bytes_written() const { return written; }

size_t ByteStream::bytes_read() const { return poped; }

size_t ByteStream::remaining_capacity() const { return cap - this->stream.size(); }
