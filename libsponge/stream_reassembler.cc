#include "stream_reassembler.hh"

#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _bytes_wait(0), _now(0), eof_index(-1), _stored() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t not_discard = std::min(data.size(), _capacity - _output.buffer_size() - _bytes_wait);
    if (not_discard == 0) {
        if (eof)
            _output.end_input();
        return;
    }
    istring tmp(data.substr(0, not_discard), index);
    _stored.push(tmp);
    _bytes_wait += not_discard;
    if (eof)
        eof_index = index + data.size();

    while (!_stored.empty()) {
        istring x = _stored.top();
        if (_now < x.second)
            break;
        _stored.pop();
        size_t len = x.first.size();
        if (_now >= x.second + len) {
            _bytes_wait -= len;
            continue;
        }
        size_t y = len - (_now - x.second);
        _output.write(x.first.substr(_now - x.second, y));
        _now += y;
        _bytes_wait -= len;
    }

    if (eof_index <= _now)
        _output.end_input();
}

void StreamReassembler::simplify_queue() {
    std::priority_queue<istring, std::vector<istring>, cmp> t;
    istring x("", 0), y("", 0);
    if (!_stored.empty()) {
        x = _stored.top();
        _stored.pop();
        t.push(x);
    }
    while (!_stored.empty()) {
        y = _stored.top();
        _stored.pop();
        if (y.second != x.second)
            t.push(y);
        else
            _bytes_wait -= y.first.size();
        x = y;
    }
    _stored = t;
}
size_t StreamReassembler::unassembled_bytes() {
    simplify_queue();
    return _bytes_wait;
}

bool StreamReassembler::empty() const { return _bytes_wait == 0; }
