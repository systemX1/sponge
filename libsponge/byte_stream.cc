#include "byte_stream.hh"

#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) :
    _buffer(), _capacity(capacity),
    _writeCount(0), _readCount(0), _isInputEnded(false) {}

size_t ByteStream::write(const string &data) {
    if(data.empty())
        return 0;
    size_t len = ( data.length() > (_capacity - _buffer.size()) ) ? (_capacity - _buffer.size()) : data.length();
    _writeCount += len;
    _buffer.append(BufferList(move(string().assign(data.begin(), data.begin() + len) ) ) );
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(size_t len) const {
    size_t length = len > _buffer.size() ? _buffer.size() : len;
    string s=_buffer.concatenate();
    return string().assign(s.begin(), s.begin() + length);
    //return string().assign(_buffer.concatenate().begin(), _buffer.concatenate().begin() + static_cast<long>(length) );
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(size_t len) {
    size_t length = len > _buffer.size() ? _buffer.size() : len;
    _readCount += length;
    _buffer.remove_prefix(length);
}

std::string ByteStream::read(size_t len) {
    std::string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { _isInputEnded = true; }

bool ByteStream::input_ended() const { return _isInputEnded; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _writeCount; }

size_t ByteStream::bytes_read() const { return _readCount; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }