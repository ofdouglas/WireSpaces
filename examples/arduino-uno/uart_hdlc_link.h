/**
 * @file uart_hdlc_link.h
 * @brief Reusable bounded UART/HDLC Link adapter for Arduino UNO applications.
 */

#pragma once

#include <platform/avr/uart0.h>
#include <wirespaces/links/uart_hdlc/decoder.h>
#include <wirespaces/links/uart_hdlc/encoder.h>
#include <wirespaces/runtime/core.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace wirespaces::examples::arduino_uno {

/**
 * @brief Encode packets as HDLC frames and write them to USART0.
 *
 * @tparam kMaximumPayloadSize Largest payload accepted by this Link.
 */
template <std::size_t kMaximumPayloadSize>
class UartHdlcForwarder final : public PacketForwarder {
public:
    void forward(const PacketBuffer& packet, EgressSet egress_set) noexcept override;

private:
    static constexpr std::size_t kMaximumCanonicalSize{sizeof(Header) + kMaximumPayloadSize};
    static constexpr std::size_t kHdlcCrcSize{2U};
    static constexpr std::size_t kMaximumFrameCapacity{(kMaximumCanonicalSize + kHdlcCrcSize) * 2U + 2U};
};

/**
 * @brief Poll USART0 and route/dispatch every valid packet through its selected ingress.
 *
 * PacketBufferType supplies the fixed payload storage used while dispatching.
 * Forwarders and receivers must copy accepted data before process() reuses that stack storage.
 */
template <typename PacketBufferType>
class UartHdlcReceiver final {
public:
    void process(const DomainContext& domain, const Dispatcher& dispatcher, std::uint8_t ingress_index) noexcept;

private:
    static constexpr std::size_t kMaximumCanonicalSize{sizeof(Header) + PacketBufferType::kPayloadCapacity};

    links::uart_hdlc::HdlcDecoder<kMaximumCanonicalSize> decoder_{};
};

// --- UartHdlcForwarder implementations ---

template <std::size_t kMaximumPayloadSize>
void UartHdlcForwarder<kMaximumPayloadSize>::forward(const PacketBuffer& packet, EgressSet egress_set) noexcept {
    static_cast<void>(egress_set);
    if (packet.size() > kMaximumPayloadSize) {
        return;
    }

    std::uint8_t frame_storage[kMaximumFrameCapacity]{};
    const std::size_t frame_size{links::uart_hdlc::HdlcEncoder::encode(packet.headerAndPayload(), frame_storage)};
    if (frame_size == 0U) {
        return;
    }
    platform::avr::uart0WriteSpan(foundation::Span<const std::uint8_t>{frame_storage, frame_size});
}

// --- UartHdlcReceiver implementations ---

template <typename PacketBufferType>
void UartHdlcReceiver<PacketBufferType>::process(
    const DomainContext& domain, const Dispatcher& dispatcher, std::uint8_t ingress_index) noexcept {
    while (platform::avr::uart0ByteAvailable()) {
        const std::uint8_t byte{platform::avr::uart0ReadByte()};
        if (!decoder_.push(byte)) {
            continue;
        }

        const auto frame{decoder_.frame()};
        if ((frame.size() >= sizeof(Header)) && (frame.size() <= kMaximumCanonicalSize)) {
            PacketBufferType packet{};
            const auto payload_size{static_cast<std::uint16_t>(frame.size() - sizeof(Header))};
            if (packet.resize(payload_size)) {
                std::memcpy(&packet.header(), frame.data(), sizeof(Header));
                if (payload_size > 0U) {
                    std::memcpy(packet.payload().data(), frame.data() + sizeof(Header), payload_size);
                }
                domain.receive(packet, ingress_index, dispatcher);
            }
        }
        decoder_.consume();
    }
}

}  // namespace wirespaces::examples::arduino_uno
