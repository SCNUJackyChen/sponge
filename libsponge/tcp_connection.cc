#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _timer; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    _timer = 0;
    // bool seg_out = false;  // segment out of the window, should send ACK
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }
    
    _receiver.segment_received(seg);
        
    
    if (seg.header().syn && _has_sent_syn) {
        _isn = seg.header().seqno;
        TCPSegment SA_reply_seg;
        SA_reply_seg.header().ack = true;
        SA_reply_seg.header().ackno = _receiver.ackno().value();
        SA_reply_seg.header().seqno = _sender.next_seqno();
        _segments_out.push(SA_reply_seg);
    } else if (seg.header().fin) {
        if (!_sender.has_fin()) _linger_after_streams_finish = false; // passive closed
        _receiver.stream_out().end_input();
        TCPSegment AF_reply_seg;
        AF_reply_seg.header().ack = true;
        AF_reply_seg.header().ackno = _receiver.ackno().value();
        AF_reply_seg.header().seqno = _sender.next_seqno();
        _segments_out.push(AF_reply_seg);
    } else if (seg.header().syn && !_has_sent_syn) {
        _sender.fill_window();
    }


    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    } 
     
    if (seg.length_in_sequence_space()) {
        TCPSegment ret_seg;
        
        if (!seg.header().syn && !seg.header().fin) {
            if (_sender.segments_out().empty()) {
                TCPSegment t;
                t.header().seqno = _sender.next_seqno();
                _sender.segments_out().push(t);
            }
        }
        while(_sender.segments_out().size()) {
            ret_seg = _sender.segments_out().front();
            _sender.segments_out().pop();
            if (_receiver.ackno().has_value()) {
                ret_seg.header().ack = true;
                ret_seg.header().ackno = _receiver.ackno().value();
                ret_seg.header().win = _receiver.window_size();
                if (seg.header().syn && !_has_sent_syn) ret_seg.header().syn = true;
            }
            _segments_out.push(ret_seg);
        }
    }

    if (_receiver.stream_out().eof() &&
        _sender.stream_in().eof() &&
        bytes_in_flight() == 0 &&
        !_linger_after_streams_finish) 
        { _active = false; }

    
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t bytes_write = _sender.stream_in().write(data);
    _sender.fill_window();
    return bytes_write;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
        _sender.tick(ms_since_last_tick);
        _timer += ms_since_last_tick;
        if (_sender.segments_out().size()) {  // retransmission happen, reset timer
            _timer = 0;
        }
        
        if (_timer >= 10 * _cfg.rt_timeout && _sender.has_fin()) {  // active closed
            _active = false;
        }

        TCPSegment seg;
        
        while(_sender.segments_out().size()) {
            seg = _sender.segments_out().front();
            _sender.segments_out().pop();
            if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
                _receiver.stream_out().set_error();
                _sender.stream_in().set_error();
                _active = false;
                _linger_after_streams_finish = false;
                seg.header().rst = true;
            } else if (_receiver.ackno().has_value()) {
                seg.header().ack = true;
                seg.header().ackno = _receiver.ackno().value();
                seg.header().win = _receiver.window_size();
            }
            _segments_out.push(seg);
        }

        if (_receiver.stream_out().eof() &&
            _sender.stream_in().eof() &&
            bytes_in_flight() == 0 &&
            !_linger_after_streams_finish) 
            { _active = false; }
       
}

void TCPConnection::end_input_stream() { 
    _sender.stream_in().end_input(); 
    if (!_sender.has_fin() && _receiver.stream_out().eof()) _linger_after_streams_finish = false;   // passive closed 
    _sender.fill_window();
    while(_sender.segments_out().size()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
}

void TCPConnection::connect() {
    _sender.fill_window();
    while(_sender.segments_out().size()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        _segments_out.push(seg);
    }
    _has_sent_syn = true;
}
    

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            // cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            TCPSegment seg;
            seg.header().seqno = _sender.next_seqno();
            seg.header().rst = true;
            _segments_out.push(seg);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
