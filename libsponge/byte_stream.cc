#include "byte_stream.hh"

#include <cassert>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : capacity_{capacity} {}

size_t ByteStream::write(const string &data) {
    assert(capacity_ >= content_.size());

    size_t remain = capacity_ - content_.size();
    size_t write_size = std::min(remain, data.size());

    for (size_t i = 0; i < write_size; i++) {
        content_.push_back(data[i]);
    }

    write_size_ += write_size;
    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t real_size = std::min(len, content_.size());

    string temp;
    for (size_t i = 0; i < real_size; i++) {
        temp += content_[i];
    }
    return temp;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t real_size = std::min(len, content_.size());

    for (size_t i = 0; i < real_size; i++) {
        content_.pop_front();
    }

    read_size_ += real_size;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { end_ = true; }

bool ByteStream::input_ended() const { return end_; }

size_t ByteStream::buffer_size() const { return content_.size(); }

bool ByteStream::buffer_empty() const { return content_.empty(); }

bool ByteStream::eof() const { return content_.empty() && end_; }

size_t ByteStream::bytes_written() const { return write_size_; }

size_t ByteStream::bytes_read() const { return read_size_; }

size_t ByteStream::remaining_capacity() const {
    assert(capacity_ >= content_.size());
    return capacity_ - content_.size();
}
