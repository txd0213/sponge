#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : ByteQueue(), _capsize(capacity), _readsize(0), _writesize(0), _closed(0), _error(0) {}

size_t ByteStream::write(const string &data) {
    if (_closed)
        return 0;
    size_t len = data.size();
    size_t actual_size = 0;

    if (len + ByteQueue.size() <= _capsize)
        actual_size = len;
    else
        actual_size = _capsize - ByteQueue.size();

    _writesize += actual_size;

    for (size_t i = 0; i < actual_size; i++)
        ByteQueue.push_back(data[i]);

    return actual_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t actual_size = 0;
    if (len < ByteQueue.size())
        actual_size = len;
    else
        actual_size = ByteQueue.size();
    return string(ByteQueue.begin(), ByteQueue.begin() + actual_size);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t actual_size = 0;

    if (len < ByteQueue.size())
        actual_size = len;
    else
        actual_size = ByteQueue.size();

    _readsize += actual_size;

    for (size_t i = 0; i < actual_size; i++)
        ByteQueue.pop_front();
}

void ByteStream::end_input() { _closed = true; }

bool ByteStream::input_ended() const { return _closed; }

size_t ByteStream::buffer_size() const { return ByteQueue.size(); }

bool ByteStream::buffer_empty() const { return ByteQueue.empty(); }

bool ByteStream::eof() const { return ByteQueue.empty() && _closed; }

size_t ByteStream::bytes_written() const { return _writesize; }

size_t ByteStream::bytes_read() const { return _readsize; }

size_t ByteStream::remaining_capacity() const { return _capsize - ByteQueue.size(); }
