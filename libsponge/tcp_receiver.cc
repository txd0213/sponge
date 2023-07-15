#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader h = seg.header();
    bool has_syn = _ifsyn;
    if (h.syn && !_ifsyn) {
        _ifsyn = true;
        _acknum = h.seqno;
    } else if (!_ifsyn)
        return false;
    if(_reassembler.stream_out().input_ended() && h.fin)
        return false;
    
    uint32_t abs = _reassembler.stream_out().bytes_written() + 1;
    uint64_t unwrap_abs = unwrap(h.seqno, _acknum, abs) + h.syn - 1;
    string s = seg.payload().copy();
    optional<WrappingInt32> ackno_opt = ackno();
    WrappingInt32 left(0);
    if(ackno_opt)
        left = *ackno_opt;
    WrappingInt32 right = left + window_size() - 1;

    if (has_syn && (left.raw_value() > h.seqno.raw_value() + s.size() + h.fin - 1 || right.raw_value() < h.seqno.raw_value()))
        return false;
    _reassembler.push_substring(s, unwrap_abs, h.fin);
    return true;
}

std::optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_ifsyn) {
        if (_reassembler.stream_out().input_ended())
            return _acknum + _reassembler.stream_out().bytes_written() + 1 + 1;
        return _acknum + _reassembler.stream_out().bytes_written() + 1;
    }
    return std::nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
