#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _bufferMap(), _output(capacity), _capacity(capacity)
    , _firstUnassembledIndex(0), _unassembledByte(0), _isEOF(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
//! implementation: if
void StreamReassembler::push_substring(const string &data, size_t index, bool eof) {
    _isEOF |= eof;
    // pre process the data
    if (index >= _firstUnassembledIndex + _capacity)  // over capacity, discard it
        return;
    segment seg(data);
    if(index + seg.len() > _firstUnassembledIndex + _capacity ) { // partially over capacity, truncate it
        _isEOF = false;
        seg.data.assign(seg.data.begin(), seg.data.begin() + static_cast<long>(_firstUnassembledIndex + _capacity - index));
    } else if(data.empty() || index + data.length() <= _firstUnassembledIndex) {       // no data or reassembled redundant segment
        if(_isEOF && !_unassembledByte && !stream_out().input_ended()) {
            _output.end_input();
//            fprintf( stderr, " _LINE: %d data: %lu _unassembledByte: %lu @end_input\n", __LINE__, data.length(), _unassembledByte );
        }
        return;
    }
    if(index < _firstUnassembledIndex) {           // partially before _firstUnassembledIndex
        seg.data.assign(seg.data.begin() + static_cast<long>(_firstUnassembledIndex - index), seg.data.end());
        index = _firstUnassembledIndex;
    }

    // append the new data
    auto lastSeg = (_bufferMap.empty() || _bufferMap.size() == 1 || _bufferMap.upper_bound(index) == _bufferMap.begin())
                        ? _bufferMap.begin()
                        : prev(_bufferMap.upper_bound(index));   // find the greatest key not greater than val
    if(!_bufferMap.empty() && isOverlap(lastSeg->first, lastSeg->second.len(), index) ) {
        size_t overlap = appendSegment(lastSeg->first, lastSeg->second.data, index, seg.data);
        _unassembledByte += (seg.len() - overlap);
    } else {
        _bufferMap.insert(pair<int, segment>(index, seg));
        lastSeg = _bufferMap.find(index);
        _unassembledByte += seg.len();
    }

    // remove overlap segment
    for(auto lastSeg2 = _bufferMap.upper_bound(lastSeg->first);
        lastSeg2 != _bufferMap.end() && lastSeg2->first < lastSeg->first + lastSeg->second.len();
        lastSeg2 = _bufferMap.upper_bound(lastSeg->first)) {
        _unassembledByte -= appendSegment(lastSeg->first, lastSeg->second.data, lastSeg2->first, lastSeg2->second.data);
        _bufferMap.erase(lastSeg2->first);
    }

    // write to the ByteStream _output
    while (!_bufferMap.empty() && _bufferMap.begin()->first == _firstUnassembledIndex) {
        size_t write_byte = _output.write(_bufferMap.begin()->second.data);
        _firstUnassembledIndex += write_byte;
        _unassembledByte -= write_byte;
        _bufferMap.erase(_bufferMap.begin());
    }

    // let output steam stop
    if (empty() && _isEOF && !stream_out().input_ended()) {
        _output.end_input();
//        fprintf( stderr, " _LINE: %d data: %lu _unassembledByte: %lu @end_input\n", __LINE__, data.length(), _unassembledByte );
    }
}

bool StreamReassembler::isOverlap(size_t begin, size_t len, size_t begin2) {
    return len && (begin2 >= begin && begin2 < begin + len);
}

// 5 6 7 8 9
//     7 8 9 10
// 5 6 7 8 9 10
//   6 7
size_t StreamReassembler::appendSegment(size_t dstIdx, std::string &dst, size_t srcIdx, const std::string &src) {
    size_t offset = srcIdx + src.length() - 1;
    if(dst.empty() || src.empty() || isOverlap(dstIdx, dst.length(), offset) )
        return src.length();
    size_t overlap = dstIdx + dst.length() - srcIdx;
    offset = dstIdx + dst.length() - srcIdx;
    dst += src.substr(offset, -1);
    return overlap;
}
