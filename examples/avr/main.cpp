#include <avr/io.h>

#include <stdio.h>
#include <stdint.h>

namespace {

constexpr uint32_t kBaudRate{9600UL};
constexpr uint16_t kBaudDivider{
    static_cast<uint16_t>((F_CPU / (16UL * kBaudRate)) - 1UL)};

void uartInit() {
    UBRR0H = static_cast<uint8_t>(kBaudDivider >> 8U);
    UBRR0L = static_cast<uint8_t>(kBaudDivider);
    UCSR0A = 0U;
    UCSR0B = _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}

int uartPutChar(char character, FILE* stream) {
    (void)stream;

    if (character == '\n') {
        uartPutChar('\r', stream);
    }

    while ((UCSR0A & _BV(UDRE0)) == 0U) {
    }

    UDR0 = static_cast<uint8_t>(character);
    return 0;
}

FILE g_uart_stdout{};

} // namespace

int main() {
    uartInit();
    fdev_setup_stream(&g_uart_stdout, uartPutChar, nullptr, _FDEV_SETUP_WRITE);
    stdout = &g_uart_stdout;

    puts("Hello, world!");

    for (;;) {
    }
}
