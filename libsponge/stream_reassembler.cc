#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
    _bufferMap(), _output(capacity), _capacity(capacity),
    _firstUnassembledIndex(0), _unassembledByte(0),
    _is_eof(false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
//! implementation: if
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    _is_eof |= eof;
    // pre process the data
    if (index >= _firstUnassembledIndex + _capacity) // over capacity, discard it
        return;
    segment seg(data);
    if(index + data.length() >= _firstUnassembledIndex + _capacity ) { // partially over capacity, truncate it
        _is_eof = false;
        seg.data.assign(seg.data.begin(), seg.data.begin() + static_cast<long>(_firstUnassembledIndex + _capacity - index));
    } else if(data.empty() || index + data.length() <= _firstUnassembledIndex) {       // if it has eof and before _firstUnassembledIndex
        if(_is_eof)
            _output.end_input();
        return;
    }
    if(index < _firstUnassembledIndex) {           // partially before _firstUnassembledIndex
        seg.data.assign(seg.data.begin() + static_cast<long>(_firstUnassembledIndex - index), seg.data.end());
    }

    // append the new data
    auto lastSeg = _bufferMap.lower_bound(index);
    if(!_bufferMap.empty() && isOverlap(lastSeg->first, lastSeg->second.len(), index) ) {
        size_t overlap = appendSegment(lastSeg->first, lastSeg->second, index, seg.data);
        _unassembledByte += (seg.len() - overlap);
    } else {
        _bufferMap.insert(pair<int, segment>(index, seg));
        lastSeg = _bufferMap.find(index);
        _unassembledByte += seg.len();
    }

    // remove overlap segment
    vector<::size_t > eraseSegs;
    for(auto lastSeg2 = _bufferMap.lower_bound(index + seg.len());
         lastSeg2 != _bufferMap.end() && lastSeg2 != lastSeg;
         lastSeg2 = _bufferMap.lower_bound(lastSeg2->first)) {
        _unassembledByte -= appendSegment(lastSeg->first, lastSeg->second, lastSeg2->first, lastSeg2->second.data);
        eraseSegs.emplace_back(lastSeg2->first);
    }
    for(auto &e : eraseSegs)
        _bufferMap.erase(e);

    // write to the ByteStream _output
    while (!_bufferMap.empty() && _bufferMap.begin()->first == _firstUnassembledIndex) {
        size_t write_byte = _output.write(_bufferMap.begin()->second.data);
        _firstUnassembledIndex += write_byte;
        _unassembledByte -= write_byte;
        _bufferMap.erase(_bufferMap.begin());
    }

    if (empty() && _is_eof) {
        _output.end_input();
    }
}

bool StreamReassembler::isOverlap(size_t begin, size_t len, size_t begin2) {
    return len && (begin2 >= begin && begin2 < begin + len);
}

// 5 6 7 8 9
//     7 8 9 10
// 5 6 7 8 9 10
//   6 7
size_t StreamReassembler::appendSegment(size_t dstIdx, segment &dstSeg, size_t srcIdx, const std::string &src) {
    size_t offset = srcIdx + src.length();
    if(isOverlap(dstIdx, dstSeg.len(), offset) )
        return 0;
    dstSeg.data += src.substr(offset, -1);
    return offset - srcIdx - 1;
}
