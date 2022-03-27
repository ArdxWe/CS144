#include "tcp_receiver.hh"

#include <cassert>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    string data;
    switch (_state) {
        case State::listen:
            assert(!ackno());
            if (seg.header().syn) {
                assert(seg.length_in_sequence_space() >= 1);

                _state = State::syn_recv;
                _isn = seg.header().seqno;
                if (seg.payload().size() != 0) {
                    data = seg.payload().copy();
                }
                if (seg.header().fin) {
                    _state = State::fin_recv;
                    _reassembler.push_substring(data, 0, true);
                } else {
                    _reassembler.push_substring(data, 0, false);
                }
            }
            break;
        case State::syn_recv: {
            assert(ackno());

            assert(!seg.header().syn);

            WrappingInt32 now = seg.header().seqno;
            uint64_t start = unwrap(now, _isn, _reassembler.stream_out().bytes_written());
            data = seg.payload().copy();
            if (seg.header().fin) {
                _fin = true;
                _reassembler.push_substring(data, start - 1, true);
                if (_reassembler.unassembled_bytes() == 0) {
                    _state = State::fin_recv;
                }
            } else {
                _reassembler.push_substring(data, start - 1, false);
            }
            if (_fin && _reassembler.unassembled_bytes() == 0) {
                _state = State::fin_recv;
            }
            break;
        }
        case State::fin_recv:
            break;
        case State::error:
            break;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    auto start = static_cast<uint64_t>(_reassembler.stream_out().bytes_written());
    switch (_state) {
        case State::listen:
            return {};
        case State::syn_recv:
            return wrap(start + 1, _isn);
        case State::fin_recv:
            if (_reassembler.unassembled_bytes() != 0) {
                return wrap(start + 1, _isn);
            }
            return wrap(start + 2, _isn);
        case State::error:
            return {};
    }
    return {};
}

size_t TCPReceiver::window_size() const {
    return _capacity - (_reassembler.stream_out().bytes_written() - _reassembler.stream_out().bytes_read());
}
