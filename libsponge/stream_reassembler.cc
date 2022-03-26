#include "stream_reassembler.hh"

#include <cassert>
#include <utility>
#include <vector>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

namespace {
// overlap type
enum class Overlap : uint8_t {
    left,
    in,
    bigger,
    right,
    other,  // we don't care
};

enum Overlap relation(const std::pair<size_t, size_t> &now, const std::pair<size_t, size_t> &other) {
    assert(now.first < now.second && other.first < other.second);

    if (other.first < now.first && other.second >= now.first && other.second <= now.second) {
        return ::Overlap::left;
    } else if (other.first >= now.first && other.second <= now.second) {
        return ::Overlap::in;
    } else if (other.first < now.first && other.second > now.second) {
        return ::Overlap::bigger;
    } else if (other.first >= now.first && other.first <= now.second && other.second > now.second) {
        return ::Overlap::right;
    } else {
        return ::Overlap::other;
    }
}
}  // namespace

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t current_size = _output.bytes_written() - _output.bytes_read() + _unassembled_size;
    // full
    if (current_size == _capacity) {
        if (eof) {
            _output.end_input();
        }
        return;
    }
    // too old
    if (index + data.size() <= _start_index) {
        if (eof) {
            _output.end_input();
        }
        return;
    }

    struct Data {
        const char *data_;
        size_t size_;

        Data(const char *data, size_t size) : data_(data), size_(size) {}

        size_t size() { return size_; }
        const char *data() { return data_; }
        string to_string() { return string(data_, size_); }
    };

    // get real data we may write
    Data real_data(data.data(), data.size());
    size_t real_index = index;
    // we don't need old segment
    if (index < _start_index) {
        real_data = Data(data.data() + _start_index - index, data.size() - (_start_index - index));

        real_index = _start_index;
    } else if (index >= _start_index + _capacity - (_output.bytes_written() - _output.bytes_read())) {
        return;
    }

    // flag end index
    if (eof) {
        _eof_index = real_index + real_data.size();
    }

    // has other package
    if (_map.find(real_index) != _map.end()) {
        // longer than now, ignore now
        if (_map[real_index].first > real_index + real_data.size()) {
            if (eof) {
                _output.end_input();
            }
            return;
        }
        // shorter than now, just remove
        _unassembled_size -= (_map[real_index].first - real_index);
        _map.erase(real_index);
    }
    // process overlap

    // erase keys
    vector<size_t> erases;
    size_t remove_size = 0;

    // write interval [start_index, end_index)
    std::size_t size = real_data.size();
    if (real_index + real_data.size() > _start_index + _capacity) {
        size = _capacity - real_index + _start_index;
    }
    std::pair<size_t, size_t> interval = {real_index, real_index + size};
    real_data = Data(real_data.data(), size);
    // updated data
    string result = real_data.to_string();
    for (const auto &pair : _map) {
        assert(pair.first < pair.second.first);

        auto location = relation(interval, std::pair{pair.first, pair.second.first});

        string update;
        switch (location) {
            case Overlap::left:
                // remove two item
                erases.push_back(pair.first);

                result = string(_map[pair.first].second.data(), interval.first - pair.first) + result;
                remove_size += pair.second.second.size();
                interval = {pair.first, interval.second};
                break;
            case Overlap::in:
                // remove map item
                erases.push_back(pair.first);
                remove_size += pair.second.second.size();
                break;
            case Overlap::bigger:
                // remove now
                erases.push_back(interval.first);
                remove_size = 0;
                result = "";
                break;
            case Overlap::right:
                erases.push_back(pair.first);
                result += string(_map[pair.first].second.data() + interval.second - pair.first,
                                 pair.second.first - interval.second);
                remove_size += pair.second.second.size();
                interval = {interval.first, pair.second.first};
                break;
            case Overlap::other:
                break;
        }
        if (result.empty()) {
            break;
        }
    }

    if (result.empty()) {
        for (auto i : erases) {
            _map.erase(i);
        }
        return;
    }

    for (auto i : erases) {
        _map.erase(i);
    }

    _map[interval.first] = {interval.second, result};
    _unassembled_size += result.size();
    _unassembled_size -= remove_size;

    if (real_index != _start_index) {
        return;
    }

    std::size_t write_size = _output.write(_map[real_index].second);
    _unassembled_size -= write_size;
    _start_index += write_size;

    if (write_size != _map[real_index].second.size()) {
        _map[real_index + write_size] = {_map[real_index].first, string(_map[real_index].second.data() + write_size)};
    }
    _map.erase(real_index);
    if (eof || _eof_index == _start_index) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_size; }

bool StreamReassembler::empty() const { return _unassembled_size == 0; }

size_t StreamReassembler::remain_size() const {
    return _capacity - _unassembled_size - (_output.bytes_written() - _output.bytes_read());
}
