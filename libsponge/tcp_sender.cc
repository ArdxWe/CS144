#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <cassert>
#include <iostream>
#include <random>
#include <string>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _now_retransmission_timeout(_initial_retransmission_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    switch (_state) {
        case State::closed: {
            return 0;
        }
        case State::syn_sent: {
            return 1;
        }
        case State::syn_acked: {
            return _flight_size;
        }
        default: {
            return _flight_size;
        }
    }
}

void TCPSender::fill_window() {
    switch (_state) {
        case State::closed: {
            assert(next_seqno_absolute() == 0);

            TCPSegment segment;
            segment.header().syn = true;
            segment.header().seqno = _isn;
            cout << "close syn " << _isn.raw_value() << endl;
            _segments_out.push(segment);

            _map[_isn.raw_value()] = pair{1, segment};

            _next_seqno = 1;
            _ackno = WrappingInt32(_isn.raw_value());
            _send_seq = wrap(1, _isn);
            _flight_size++;

            if (!_timer_started) {
                assert(_passed_time == 0);
                _timer_started = true;
            }

            _state = State::syn_sent;
            break;
        }
        case State::syn_sent: {
            assert(next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight());
            break;
        }
        case State::syn_acked: {
            assert((next_seqno_absolute() > bytes_in_flight() && (!_stream.eof())) ||
                   (_stream.eof() && next_seqno_absolute() < _stream.bytes_written() + 2));

                if (_stream.eof()) {
                    if (_window_size == 0) {
                        return;
                    }
                    cout << "fuck\n";
                    TCPSegment segment;
                    segment.header().fin = true;
                    segment.header().seqno = _send_seq;
                    _next_seqno++;
                    _window_size--;
                    _send_seq = wrap(1, _send_seq);
                    cout << "send seq fuck bttt " << _send_seq.raw_value() << endl;
                    _flight_size++;
                    _segments_out.push(segment);
                    _map[segment.header().seqno.raw_value()] = pair{1, segment};
                    _state = State::fin_sent;
                    return;
                }
                while(true){
                    size_t size = std::min(TCPConfig::MAX_PAYLOAD_SIZE, _window_size);
            string read_stream = _stream.read(size);
            cout << "size " << read_stream.size() << endl;
            cout << "read " << read_stream << endl;
            if (!read_stream.empty()) {
                TCPSegment segment;
                segment.header().seqno = _send_seq;
                cout << "send seq fuck " << _send_seq.raw_value() << endl;
                _flight_size += read_stream.size();
                _next_seqno += read_stream.size();

                _window_size -= read_stream.size();
                if (_stream.eof() && _window_size > 0) {
                    segment.header().fin = true;
                    _window_size--;
                    _next_seqno++;
                }
                cout << "now window size " << _window_size << endl;

                _send_seq = wrap(segment.header().fin ? read_stream.size() + 1 : read_stream.size(), _send_seq);
                cout << "send seq " << _send_seq.raw_value() << endl;
                string copy = read_stream;
                segment.payload() = Buffer(std::move(read_stream));
                _segments_out.push(segment);
                _map[segment.header().seqno.raw_value()] = pair{segment.header().fin ? copy.size() + 1 : copy.size(), segment};
                cout << "now flight: " << _flight_size << endl;
            } else {
                break;
            }
                }
        }
        case State::fin_sent: {
            assert(_stream.eof() && next_seqno_absolute() == _stream.bytes_written() + 2 && bytes_in_flight() > 0);
            break;
        }

        default:
            return;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    cout << "ack " << ackno.raw_value() << endl;
    if (unwrap(ackno, _ackno, 0) > 0x80000000) {
        return;
    }
        bool valid = false;
    for (const auto& item : _map) {
        if (item.first + item.second.first == ackno.raw_value()) {
            valid = true;
            break;
        }
    }
    if (!valid) {
        cout << "invalid" << endl;
        return;
    }
    cout << "valid" << endl;

    size_t has_send_no_ack = unwrap(_send_seq, _ackno, 0);
    if (_ack_win.find(ackno.raw_value()) != _ack_win.end()) {
        _window_size = window_size - has_send_no_ack;
    } else {
        _ack_win[ackno.raw_value()] = window_size;
        _window_size = window_size;
        if (window_size == 0) {
            _window_size = 1;
        }
         _passed_time = 0;
    }
    if(window_size == 0) _last_zero_win_size = true;
    else {
        _last_zero_win_size = false;
    }
    cout << "ackno: " << ackno.raw_value() << endl;
    cout << "window size " << window_size << endl;
    cout << "distance " << unwrap(ackno, _ackno, _next_seqno) << endl;
    _flight_size -= unwrap(ackno, _ackno, _next_seqno);

    cout << "flight: " << _flight_size << endl;
    _ackno = ackno;

    _now_retransmission_timeout = _initial_retransmission_timeout;

    _retransmission_counts = 0;

    switch (_state) {
        case State::closed: {
            assert(false);
            break;
        }
        case State::syn_sent: {
            assert(next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight());
            assert(unwrap(ackno, _isn, _next_seqno) >= _next_seqno);

            if (unwrap(ackno, _isn, _next_seqno) == _next_seqno) {
                _state = State::syn_acked;
            }
            break;
        }
        default:
            return;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _passed_time += ms_since_last_tick;
    cout << "pass times: " << _passed_time << endl;
    cout << "now: " << _now_retransmission_timeout << endl;

    if (_passed_time >= _now_retransmission_timeout) {
        assert(_map.find(_ackno.raw_value()) != _map.end());
        cout << "bitch" << endl;

        cout << "fuck " << _ackno.raw_value() << endl;
        _segments_out.push(_map[_ackno.raw_value()].second);
        cout << "re size: " << _map[_ackno.raw_value()].second.payload().size() << endl;
        _send_seq = wrap(_map[_ackno.raw_value()].first, _ackno);
        if (_window_size != 0 || _last_zero_win_size == false) {
            cout << "asd" << endl;
            _retransmission_counts++;
            _now_retransmission_timeout *= 2;
        }

        _passed_time = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_counts; }

void TCPSender::send_empty_segment() {}
