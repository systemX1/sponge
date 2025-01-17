#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <deque>
#include <map>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    struct segment {
        segment() = delete;
        explicit segment(const std::string &d) : data(d) {}
        size_t len() const {return data.length();}
        bool empty() const {return !data.length();}
        std::string data;
    };
    std::map<size_t, segment> _bufferMap;
    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity;    //!< The maximum number of bytes
    size_t _firstUnassembledIndex;  //!< The next unread index
    size_t _unassembledByte;   //!< The number of unassembled bytes
    bool _isEOF;       //!< eof

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    explicit StreamReassembler(size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, uint64_t index, bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const { return _unassembledByte;}

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const { return !_unassembledByte; }

    size_t firstUnassembledIndex() const { return _firstUnassembledIndex;}

    //! \returns `true` if begin2 within the range of `begin` to `begin` + `len`
    bool isOverlap(size_t begin, size_t len, size_t begin2);

    //! \brief append string `src` start at index `srcIdx` behind string `dst` start at index `dstIdx`
    //! \returns the length of overlap part of `dst` and `src`
    size_t appendSegment(size_t dstIdx, std::string &dst, size_t srcIdx, const std::string &src);

};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
