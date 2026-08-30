#include <platform/avr/mcp2515_can.h>
#include <platform/avr/spi0.h>
#include <platform/avr/uart0.h>

#include <cstddef>
#include <cstdint>

namespace {

using wirespaces::platform::avr::Mcp2515Can;
using wirespaces::platform::avr::Mcp2515CanFrame;
using wirespaces::platform::avr::uart0ByteAvailable;
using wirespaces::platform::avr::uart0ReadByte;
using wirespaces::platform::avr::uart0WriteByte;

constexpr std::uint32_t kBaudRate{115200UL};
constexpr std::size_t kCommandCapacity{24U};

char g_command[kCommandCapacity]{};
std::uint8_t g_command_size{0U};

void writeString(const char* text) noexcept {
    while (*text != '\0') {
        uart0WriteByte(static_cast<std::uint8_t>(*text++));
    }
}

void writeHexNibble(std::uint8_t value) noexcept {
    static constexpr char kHex[]{"0123456789ABCDEF"};
    uart0WriteByte(static_cast<std::uint8_t>(kHex[value & 0x0FU]));
}

void writeHexByte(std::uint8_t value) noexcept {
    writeHexNibble(static_cast<std::uint8_t>(value >> 4U));
    writeHexNibble(value);
}

void writeHexIdentifier(std::uint16_t identifier) noexcept {
    writeHexNibble(static_cast<std::uint8_t>(identifier >> 8U));
    writeHexByte(static_cast<std::uint8_t>(identifier));
}

bool parseHexNibble(char character, std::uint8_t& value) noexcept {
    if ((character >= '0') && (character <= '9')) {
        value = static_cast<std::uint8_t>(character - '0');
        return true;
    }
    if ((character >= 'A') && (character <= 'F')) {
        value = static_cast<std::uint8_t>(character - 'A' + 10);
        return true;
    }
    if ((character >= 'a') && (character <= 'f')) {
        value = static_cast<std::uint8_t>(character - 'a' + 10);
        return true;
    }
    return false;
}

bool parseHexByte(const char* text, std::uint8_t& value) noexcept {
    std::uint8_t high{0U};
    std::uint8_t low{0U};
    if (!parseHexNibble(text[0], high) || !parseHexNibble(text[1], low)) {
        return false;
    }
    value = static_cast<std::uint8_t>((high << 4U) | low);
    return true;
}

void processCommand() noexcept {
    if ((g_command_size < 6U) || (g_command[0] != 'T') ||
        (g_command[1] != ':') || (g_command[5] != ':') ||
        (((g_command_size - 6U) & 1U) != 0U) ||
        ((g_command_size - 6U) > 16U)) {
        writeString("T:BAD\r\n");
        return;
    }

    std::uint8_t id_high{0U};
    std::uint8_t id_middle{0U};
    std::uint8_t id_low{0U};
    if (!parseHexNibble(g_command[2], id_high) ||
        !parseHexNibble(g_command[3], id_middle) ||
        !parseHexNibble(g_command[4], id_low) || (id_high > 0x07U)) {
        writeString("T:BAD\r\n");
        return;
    }

    Mcp2515CanFrame frame{};
    frame.identifier = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(id_high) << 8U) |
        (static_cast<std::uint16_t>(id_middle) << 4U) | id_low);
    frame.size = static_cast<std::uint8_t>((g_command_size - 6U) / 2U);
    for (std::uint8_t index{0U}; index < frame.size; ++index) {
        if (!parseHexByte(&g_command[6U + (2U * index)], frame.data[index])) {
            writeString("T:BAD\r\n");
            return;
        }
    }

    writeString(Mcp2515Can::transmit(frame) ? "T:OK\r\n" : "T:BUSY\r\n");
}

void processUart() noexcept {
    while (uart0ByteAvailable()) {
        const char character{static_cast<char>(uart0ReadByte())};
        if ((character == '\r') || (character == '\n')) {
            if (g_command_size != 0U) {
                processCommand();
                g_command_size = 0U;
            }
        } else if (g_command_size < kCommandCapacity) {
            g_command[g_command_size++] = character;
        } else {
            g_command_size = 0U;
            writeString("T:BAD\r\n");
        }
    }
}

void reportReceivedFrame(const Mcp2515CanFrame& frame) noexcept {
    writeString("R:");
    writeHexIdentifier(frame.identifier);
    uart0WriteByte(':');
    for (std::uint8_t index{0U}; index < frame.size; ++index) {
        writeHexByte(frame.data[index]);
    }
    writeString("\r\n");
}

}  // namespace

int main() {
    wirespaces::platform::avr::uart0Init(kBaudRate);
    wirespaces::platform::avr::spi0Init();

    if (!Mcp2515Can::initialize500Kbps8MHz()) {
        writeString("ERROR:MCP2515\r\n");
        for (;;) {
        }
    }
    writeString("READY:500K\r\n");

    for (;;) {
        processUart();
        Mcp2515CanFrame frame{};
        while (Mcp2515Can::receive(frame)) {
            reportReceivedFrame(frame);
        }
    }
}
