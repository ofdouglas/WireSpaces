/*
 * AVR HDLC framing tests:
 * - CRC-16/CCITT-FALSE matches its standard check vector.
 * - A valid encoded frame round-trips through the stream decoder.
 * - A corrupted frame is rejected by the decoder.
 */

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <wirespaces/crc/crc_algorithm.h>
#include <wirespaces/links/uart_hdlc/decoder.h>
#include <wirespaces/links/uart_hdlc/encoder.h>

namespace {

constexpr std::size_t kPayloadCapacity{16U};
constexpr std::size_t kFrameCapacity{
    (kPayloadCapacity + 2U) * 2U + 2U};

bool decodeFrame(
    const std::uint8_t* frame,
    std::size_t frame_size,
    wirespaces::links::uart_hdlc::HdlcDecoder<kPayloadCapacity>& decoder) {
    bool completed{false};
    for (std::size_t index{0U}; index < frame_size; ++index) {
        completed = decoder.push(frame[index]);
    }
    return completed;
}

} // namespace

int main() {
    using wirespaces::links::uart_hdlc::HdlcDecoder;
    using wirespaces::links::uart_hdlc::HdlcEncoder;

    const std::uint8_t check_input[]{
        '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    const wirespaces::foundation::Span<const std::uint8_t> check_span{
        check_input,
        sizeof(check_input)};
    if ((wirespaces::crc::algorithm::Crc16CcittFalse::compute(check_span) !=
         0x29B1U) ||
        (wirespaces::crc::algorithm::Crc16CcittFalse::compute(check_input) !=
         0x29B1U)) {
        return 1;
    }

    const std::uint8_t payload[]{0x01U, 0x7EU, 0x7DU, 0x02U};
    std::uint8_t frame[kFrameCapacity]{};
    const std::size_t frame_size{HdlcEncoder::encode(payload, frame)};
    HdlcDecoder<kPayloadCapacity> valid_decoder{};
    if (!decodeFrame(frame, frame_size, valid_decoder)) {
        return 2;
    }
    const auto decoded_frame{valid_decoder.frame()};
    if ((decoded_frame.size() != sizeof(payload)) ||
        (std::memcmp(decoded_frame.data(), payload, sizeof(payload)) != 0)) {
        return 2;
    }

    const std::uint8_t simple_payload[]{0x01U, 0x02U};
    const std::size_t simple_frame_size{
        HdlcEncoder::encode(simple_payload, frame)};
    frame[1] ^= 0x01U;
    HdlcDecoder<kPayloadCapacity> corrupt_decoder{};
    if (decodeFrame(frame, simple_frame_size, corrupt_decoder)) {
        return 3;
    }

    return 0;
}
