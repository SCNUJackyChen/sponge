#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) { }

size_t ByteStream::write(const string &data) {
    size_t len = data.size();

    len = min(len, _capacity - _dq.size());
    _total_write_bytes += len;

    for(size_t i = 0; i < len; i++) {
        _dq.push_back(data[i]);
    }

    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = len;

    length = min(length, _dq.size());

    string res;
    for(size_t i = 0; i < length; i++) {
        res += _dq[i];
    }

    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = len;

    length = min(length, _dq.size());
    _total_read_bytes += length;
    
    for(size_t i = 0; i < length; i++) {
        _dq.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res;
    size_t length = len;

    length = min(length, _dq.size());
    _total_read_bytes += length;

    while(length--) {
        res += _dq.front();
        _dq.pop_front();
    }

    return res;
}

void ByteStream::end_input() { _input_end = true; }

bool ByteStream::input_ended() const { return _input_end; }

size_t ByteStream::buffer_size() const { return _dq.size(); }

bool ByteStream::buffer_empty() const { return _dq.empty(); }

bool ByteStream::eof() const { return _dq.empty() && _input_end; }

size_t ByteStream::bytes_written() const { return _total_write_bytes; }

size_t ByteStream::bytes_read() const { return _total_read_bytes; }

size_t ByteStream::remaining_capacity() const { return _capacity - _dq.size(); }
