#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(size_t capacity) :_capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t len = data.length();
    if(len > _capacity - buffer_size() ) {
        len = _capacity - buffer_size();
    } else
        _is_input_ended = true;

    _buffer.append(BufferList(data.substr(0, len)) );
    _write_count += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(size_t len) const {
    size_t length = len;
    if (length > _buffer.size()) {
        length = _buffer.size();
    }
    return string().assign(_buffer.Buffer().begin(), _buffer.begin() + static_cast<long>(length) );
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(size_t len) {
    size_t length = len;
    if (length > _buffer.size()) {
        length = _buffer.size();
    }
    _read_count += length;
    while (length--)
        _buffer.pop_front();
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(size_t len) {
    std::string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { _is_input_ended = true; }

bool ByteStream::input_ended() const { return _is_input_ended; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return _is_input_ended && _buffer.empty(); }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
