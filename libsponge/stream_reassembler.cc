#include "stream_reassembler.hh"

#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capcity)
    : _output(capcity), _capacity(capcity), _bytes_wait(0), _now(0), _indeof(-1), _indlist(), _unassemble_strs() {}

void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t len = data.size();

    if (eof)
        _indeof = index + len;
    if (_indeof <= _now)
        _output.end_input();
    if (len == 0)
        return;
    if (len + _output.buffer_size() + _bytes_wait > _capacity)
        return;

    std::queue<std::pair<size_t, size_t>> _newlist;
    string _data = data;
    size_t left = index, right = len + index - 1;
    bool placed = false;

    for (auto &p : _indlist) {
        if (p.first > right) {
            if (!placed) {
                _newlist.push({left, right});
                _unassemble_strs[{left, right}] = _data;
                _bytes_wait += _data.size();
                placed = true;
            }
            _newlist.push(p);
        } else if (p.second < left)
            _newlist.push(p);
        else {
            string x = _unassemble_strs[p];
            _unassemble_strs.erase(p);
            _bytes_wait -= x.size();
            if (left > p.first) {
                _data = x.substr(0, left - p.first) + _data;
                left = p.first;
            }
            if (right < p.second) {
                size_t y = p.second - right;
                _data = _data + x.substr(x.size() - y, y);
                right = p.second;
            }
        }
    }
    if (!placed) {
        _newlist.push({left, right});
        _unassemble_strs[{left, right}] = _data;
        _bytes_wait += _data.size();
    }

    _indlist.clear();
    while (!_newlist.empty()) {
        std::pair<size_t, size_t> p = _newlist.front();
        _newlist.pop();
        if (_now >= p.first && _now <= p.second) {
            size_t wsize = p.second - _now + 1;
            string meg = _unassemble_strs[{p.first, p.second}];
            _unassemble_strs.erase({p.first, p.second});
            _output.write(meg.substr(meg.size() - wsize, wsize));
            _now += wsize;
            _bytes_wait -= wsize;
        } else
            _indlist.push_back(p);
    }
    if (_indeof <= _now)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() { return _bytes_wait; }

bool StreamReassembler::empty() const { return _bytes_wait == 0; }
