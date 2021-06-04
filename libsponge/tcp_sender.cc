#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include<iostream>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _RTO{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return _byte_in_flight; }

void TCPSender::fill_window() {
    size_t win_size = _win_size == 0 ? 1 : _win_size;
    size_t sent_bytes = _next_seqno - _ackno;
    size_t remain_win_size = win_size - sent_bytes;
    bool flag = false;
    while(remain_win_size > 0 || flag == true) {
        size_t read_len = min(remain_win_size, TCPConfig::MAX_PAYLOAD_SIZE);
        if (_ackno == 0) read_len = 0;  // have not connected yet, should not carry payload
        string str = _stream.read(read_len);

        TCPSegment seg;
        
        seg.header().seqno = next_seqno();
        seg.header().syn = _ackno == 0 ? true : false;
        seg.header().fin = stream_in().eof() && 
                           stream_in().bytes_read() + 1 == _next_seqno + str.size();
        seg.payload() = Buffer(move(str));
        
        // with FIN flag, but exceed window size
        if (seg.header().fin) {
            if(_next_seqno + seg.length_in_sequence_space() == _ackno + win_size + 1)
                seg.header().fin = false; // temporarily remove the FIN flag and try to send it next time
            flag = true;
            _fin_flag = true;
        }

        if (seg.length_in_sequence_space() == 0) return;
        

        _next_seqno += seg.length_in_sequence_space();
        _byte_in_flight += seg.length_in_sequence_space();
      
        _segments_outstanding.push(seg);
        _segments_out.push(seg);

        if (!_set_time) {
            _set_time = true;
            _timer = 0;
        }
        sent_bytes = _next_seqno - _ackno;
        remain_win_size = win_size - sent_bytes;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    size_t new_ackno = unwrap(ackno, _isn, _ackno);
    if (new_ackno < _ackno) return;  // outdated ACK
    while(_segments_outstanding.size()) {
        TCPSegment seg = _segments_outstanding.front();
        uint64_t abs_seqno = unwrap(seg.header().seqno, _isn, _next_seqno);
        uint64_t abs_ackno = unwrap(ackno, _isn, _ackno);
        uint64_t len = seg.length_in_sequence_space();
        if (abs_seqno + len <= abs_ackno) {
            _segments_outstanding.pop();
        } else {
            break;
        }
    }
    size_t received_bytes = new_ackno - _ackno;
    // cerr << "recv_bytes: " << received_bytes << endl;
    if (_ackno == 0 && received_bytes != 1) return; // wrong ACK when connecting
    _ackno = new_ackno;
    _byte_in_flight -= received_bytes;
    _win_size = window_size;

    // reset
    _RTO = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;
    if (_segments_outstanding.size() && received_bytes != 0) {
        _set_time = true;
        _timer = 0;
    }

    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    _timer += ms_since_last_tick;
    
    if (_timer >= _RTO) {
        if (_segments_outstanding.size()) {
            TCPSegment seg = _segments_outstanding.front();
            string s = seg.payload().copy();
            _segments_out.push(seg);
            if (_win_size) {
                _consecutive_retransmissions++;
                _RTO <<= 1;
            }
        }
        _timer = 0;
    }
    
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);    
}
