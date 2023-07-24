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

size_t TCPConnection::time_since_last_segment_received() const { return _tslsr; }

bool TCPConnection::active() const { return _isalive; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
    _tslsr = 0;
    TCPHeader header = seg.header();
    Buffer payload = seg.payload();

    if (state()==TCPState::State::SYN_SENT) {
        if (!header.rst && header.ack && header.ackno!=_sender.next_seqno()) {
            _sender.send_empty_segment();
            TCPSegment s = _sender.segments_out().front();
            _sender.segments_out().pop();
            s.header().rst=true;
            s.header().seqno = header.ackno;
            _segments_out.push(s);
            setRST();
            return;
        }
        if ((header.rst && header.ack && header.ackno!=_sender.next_seqno())) {
            return;
        }

        if(header.rst && header.ack){
            setRST();
            return;
        }
        if (!header.syn) {
            return;
        }
    }

    if (state()==TCPState::State::LISTEN) {
        std::cout << "fsadf" << std::endl;
        if (header.ack) {
            return;
        }
        if (!header.syn) {
            return;
        }
    }

    if (header.rst && header.seqno!=_receiver.ackno()) {
        return;
    }

    if (header.rst) {
        setRST();
        return;
    }

    bool ifemptyACK = seg.length_in_sequence_space();

    _receiver.segment_received(seg);

    // if(seg.header().rst && _receiver.ifreceivedSYN()){
    //     setRST();
    //     return;
    // }

    if(seg.header().ack){
        if(!_sender.setSYN()){
            TCPSegment rst_seg;
            rst_seg.header().rst = true;
            _segments_out.push(rst_seg);
            setRST();
            return;
        }
        if(!_sender.ack_received(seg.header().ackno, seg.header().win))
            ifemptyACK = true;
        if(!_segments_out.empty())
            ifemptyACK = false;
    }

    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }

    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        _linger_after_streams_finish = false;

    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _isalive = false;
        return;
    }

    if (ifemptyACK)
        _sender.send_empty_segment();
    add_ackno_winsize();
}


size_t TCPConnection::write(const string &data) {
    size_t wsize = _sender.stream_in().write(data);

    _sender.fill_window();
    add_ackno_winsize();

    return wsize;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        _sender.segments_out().pop();
        TCPSegment rst_seg;
        rst_seg.header().rst = true;
        _segments_out.push(rst_seg);
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _linger_after_streams_finish = false;
        _isalive = false;
        return;
    }


    add_ackno_winsize();

    _tslsr += ms_since_last_tick;

    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish &&
        _tslsr >= 10 * _cfg.rt_timeout) {
        _isalive = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    add_ackno_winsize();
}

void TCPConnection::connect() {
    _isalive = true;
    _sender.fill_window();
    add_ackno_winsize();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _receiver.stream_out().set_error();
            _sender.stream_in().set_error();
            _linger_after_streams_finish = false;
            _isalive = false;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::add_ackno_winsize(){
    std::queue<TCPSegment> &out = _sender.segments_out();
    while(!out.empty()){
        TCPSegment segment = out.front();
        out.pop();
        if(_receiver.ackno().has_value()){
            segment.header().ack = true;
            segment.header().ackno = *(_receiver.ackno());
            segment.header().win = _receiver.window_size();
        }
        _segments_out.push(segment);
        std::cout<< "fin :" << segment.header().fin << " " <<segment.header().seqno << std::endl;
    }
}

void  TCPConnection::setRST(){
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _linger_after_streams_finish = false;
    _isalive = false;
}