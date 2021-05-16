#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) 
: _output(capacity), _capacity(capacity), _map({}), _eof_tag(false), _unassemble_count(0), _head_index(0) {}


long StreamReassembler::push_substring_aux_merge(pair<uint64_t, std::string>& block1, pair<uint64_t, std::string>& block2) {
    pair<uint64_t, string> p1, p2;
    if (block1.first < block2.first) p1 = block1, p2 = block2;
    else p1 = block2, p2 = block1;

    if (p1.first + p1.second.size() < p2.first) {  // no intersaction
        return -1;
    } else if (p1.first + p1.second.size() < p2.first + p2.second.size()) {
        long overlap = p1.first + p1.second.size() - p2.first;
        block1 = {p1.first, p1.second + p2.second.substr(overlap)};
        return overlap;
    } else {   // p1 includes p2
        long overlap = p2.second.size();
        block1 = p1;
        return overlap;
    }
}


//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index > _head_index + _capacity) return;

    size_t st = index, len = data.size();
    string s = data;
    
    if (index + data.size() <= _head_index) {
        if (eof) {
            _eof_tag = true;
        }
        if (_eof_tag && empty()) {
            _output.end_input();
        }
        return;
    } else if (index < _head_index) {
        int offset = _head_index - st;
        len -= offset;
        st = _head_index;
        s = data.substr(offset);
    }
    _unassemble_count += len;

    // merge
    pair<uint64_t, string> cur_block =  {st, s};
    long merge_bytes = 0;
    do {
        // merge next blocks
        auto r = _map.lower_bound(st);
        pair<uint64_t, string> next_block;
        while(r != _map.end() && (merge_bytes = push_substring_aux_merge(cur_block, next_block = *r)) >= 0) {
            _unassemble_count -= merge_bytes;
            _map.erase(r);
            r = _map.lower_bound(cur_block.first);
        }

        // merge prev blocks
        if (r == _map.begin()) break;
        auto l = --r;
        pair<uint64_t, string> prev_block;
        while((merge_bytes = push_substring_aux_merge(cur_block, prev_block = *l)) >= 0) {
            _unassemble_count -= merge_bytes;
            _map.erase(l);
            l = _map.lower_bound(cur_block.first);
            if (l == _map.begin()) break;
            l--;
        }
    }while(false);
    _map.insert(cur_block);

    // write to byte stream
    if (_map.size() && _map.begin()->first == _head_index) {
        auto iter = _map.begin();
        size_t write_bytes = _output.write(iter->second);
        _unassemble_count -= write_bytes;
        _head_index += write_bytes;
        _map.erase(iter);
    }

    if (eof) {
        _eof_tag = true;
    }
    if (_eof_tag && empty()) {
        _output.end_input();
    }

}

size_t StreamReassembler::unassembled_bytes() const { return _unassemble_count; }

bool StreamReassembler::empty() const { return _unassemble_count == 0; }
