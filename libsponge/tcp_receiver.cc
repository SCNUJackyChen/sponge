#include "tcp_receiver.hh"
#include<iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        _syn_flag = true;
        _isn_set_flag = true;
        _isn = seg.header().seqno.raw_value();
        _abs_seqno = 0; 
        _abs_ackno = 1;
    } else if (!_syn_flag) {
        return;
    }
    if (_fin_flag && _reassembler.empty()) return;

    size_t abs_seqno = unwrap(seg.header().seqno, WrappingInt32(_isn), _abs_seqno);
    // if (!seg.header().syn &&  _abs_ackno + window_size() <= abs_seqno) return;
    _abs_seqno = abs_seqno;
    if (_syn_flag && !seg.header().syn && _abs_seqno == 0) return;  // invalid seqno should be ignored

    size_t old_window_size = window_size();

    uint64_t prev_index = _reassembler.get_head_index();
    _reassembler.push_substring(seg.payload().copy(), _abs_seqno == 0 ? 0 : _abs_seqno - 1, false);
    uint64_t post_index = _reassembler.get_head_index();
    size_t lent = post_index - prev_index;

    
    if (!seg.header().fin) {
        WrappingInt32 seqt = seg.header().seqno;
        WrappingInt32 ackt = WrappingInt32(wrap(_abs_ackno, WrappingInt32(_isn)));
        if (!seg.header().syn && ackt == seqt) {
            _abs_ackno += min(lent, old_window_size);
        } else if (seg.header().syn && ackt == seqt + 1) {
            _abs_ackno += min(lent, old_window_size);
        }
    } else {
        if (!_fin_flag) _fin_flag = true;
        _abs_ackno +=  min(lent, old_window_size);
    }

    if (_fin_flag && _reassembler.empty()) {
        _reassembler.stream_out().end_input();
        _abs_ackno++;
    }
    
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_isn_set_flag) return {};
    return WrappingInt32(wrap(_abs_ackno, WrappingInt32(_isn)));
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
