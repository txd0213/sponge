#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return flight_size; }

void TCPSender::fill_window() {
    _winsize = _winsize == 0 ? 1 : _winsize;
    while (_winsize > bytes_in_flight()) {
        TCPSegment seg;
        size_t winsize = _winsize - bytes_in_flight();
        if (!_ifsetSYN) {
            _ifsetSYN = true;
            seg.header().syn = true;
            winsize--;
        }

        size_t payload = std::min(_stream.buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE, winsize);
        seg.payload() = Buffer(_stream.read(payload));
        seg.header().seqno = next_seqno();
        winsize -= payload;
        if (!_ifsetFIN && _stream.eof())
            seg.header().fin = _ifsetFIN = winsize > 0;

        size_t len = seg.length_in_sequence_space();
        if (len == 0)
            break;
        if (_segments_out.empty()) {
            _RTO = _initial_retransmission_timeout;
            _con_retrans = 0;
        }
        _segments_out.push(seg);

        flight_size += len;
        _m[_next_seqno] = seg;
        _next_seqno += len;

        if (seg.header().fin)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t nseq = unwrap(ackno, _isn, _next_seqno);
    if (nseq > _next_seqno)
        return false;
    _winsize = window_size;

    for (auto p = _m.begin(); p != _m.end();) {
        TCPSegment seg = p->second;
        size_t x = seg.length_in_sequence_space();
        if (p->first + x - 1 < nseq) {
            flight_size -= x;
            p = _m.erase(p);
            _timeout = 0;
            _RTO = _initial_retransmission_timeout;
        } else
            break;
    }

    _con_retrans = 0;
    fill_window();
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timeout += ms_since_last_tick;
    auto p = _m.begin();
    if (p != _m.end() && _timeout >= _RTO) {
        if (_winsize > 0)
            _RTO <<= 1;
        _timeout = 0;
        _segments_out.push(p->second);
        _con_retrans++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _con_retrans; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
