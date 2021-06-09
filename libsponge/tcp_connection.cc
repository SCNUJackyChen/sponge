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
    if (!active()) return;
    _timer = 0;
    
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }
    
    _receiver.segment_received(seg);

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
    } 
    if (seg.header().syn) {  // listen mode and receive syn (the first segment)
        _sender.fill_window();
    }
     
    if (seg.length_in_sequence_space()) {  // segment with SYN/FIN/payload should be ack
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        
        while(_sender.segments_out().size()) {
            TCPSegment ret_seg = _sender.segments_out().front();
            _sender.segments_out().pop();
            if (_receiver.ackno().has_value()) {
                ret_seg.header().ack = true;
                ret_seg.header().ackno = _receiver.ackno().value();
                ret_seg.header().win = _receiver.window_size();
                if (seg.header().syn && !_has_sent_syn) ret_seg.header().syn = true;  // listen mode and receive SYN
            }
            _segments_out.push(ret_seg);
        }
    }
    
    clean_shutdown();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t bytes_write = _sender.stream_in().write(data);
    _sender.fill_window();
    return bytes_write;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
        if (!active()) return;

        _sender.tick(ms_since_last_tick);
        _timer += ms_since_last_tick;
        
        while(_sender.segments_out().size()) {    // retransmission happen
            TCPSegment seg = _sender.segments_out().front();
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

        clean_shutdown();       
}

void TCPConnection::end_input_stream() { 
    _sender.stream_in().end_input(); 
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
    clean_shutdown();
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
    

void TCPConnection::clean_shutdown() {
    if (_receiver.stream_out().input_ended() && !(_sender.stream_in().eof())) {
        _linger_after_streams_finish = false;
    }
    if (_sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()) {
        if (!_linger_after_streams_finish || time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            _active = false;

            if (_sender.segments_out().empty()) {
                _sender.send_empty_segment();
            }
          
            TCPSegment ret_seg = _sender.segments_out().front();
            _sender.segments_out().pop();
            if (_receiver.ackno().has_value()) {
                ret_seg.header().ack = true;
                ret_seg.header().ackno = _receiver.ackno().value();
                ret_seg.header().win = _receiver.window_size();
            }
            ret_seg.header().rst = true;
            _segments_out.push(ret_seg);
            
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
