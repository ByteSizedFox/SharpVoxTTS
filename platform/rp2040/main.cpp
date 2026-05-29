#include <cstdio>
#include <cstring>
#include <string>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

#include "TtsEngine.h"
#include "LibraryData.h"

using namespace SharpVox;

#define UART_PORT   uart0
#define UART_BAUD   230400
#define UART_TX_PIN 0
#define UART_RX_PIN 1

// 22050 Hz: half CD quality. ~44 KB/s output exceeds 230400 baud on real
// hardware (~20 KB/s usable) — accept that constraint for now.
static constexpr int32_t SAMPLE_RATE = 22050;

static constexpr int MAX_LINE = 255;

static void uart_write_all(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) uart_putc_raw(UART_PORT, (char)data[i]);
}

// Streams each synthesis chunk directly to UART — no audio buffer in RAM.
// Protocol:
//   "SHVX BEGIN <rate>\r\n"
//   "SHVX CHUNK <n>\r\n" + n bytes raw PCM  (repeated per chunk)
//   "SHVX END\r\n"
static void on_chunk(const int16_t* buf, int32_t len, void*) {
#ifndef BENCH
    char hdr[32];
    int hlen = snprintf(hdr, sizeof(hdr), "SHVX CHUNK %d\r\n", (int)len);
    uart_write_all(reinterpret_cast<const uint8_t*>(hdr), (size_t)hlen);
    uart_write_all(reinterpret_cast<const uint8_t*>(buf), (size_t)len * sizeof(int16_t));
#else
    (void)buf; (void)len;
#endif
}

int main() {
    uart_init(UART_PORT, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_PORT, false, false);
    uart_set_format(UART_PORT, 8, 1, UART_PARITY_NONE);

    // All rodata (dictionary blob, tables) lives in flash via XIP.
    TtsEngine engine(
        LibraryData::dictionary,
        static_cast<size_t>(LibraryData::dictionarySize),
        [](const std::string& key, size_t& sz) -> const uint8_t* {
            return LibraryData::FindSymbol(key.c_str(), sz);
        },
        SAMPLE_RATE);

    uart_puts(UART_PORT, "SHVX READY\r\n");

    char line[MAX_LINE + 1];
    int linePos = 0;

    while (true) {
        if (!uart_is_readable(UART_PORT)) {
            tight_loop_contents();
            continue;
        }

        char c = uart_getc(UART_PORT);
        if (c == '\r') continue;

        if (c == '\n') {
            if (linePos == 0) continue;
            line[linePos] = '\0';
            linePos = 0;

            char hdr[32];
            int hlen = snprintf(hdr, sizeof(hdr), "SHVX BEGIN %d\r\n", (int)SAMPLE_RATE);
            uart_write_all(reinterpret_cast<const uint8_t*>(hdr), (size_t)hlen);

            engine.Speak(std::string(line), on_chunk, nullptr);

            uart_puts(UART_PORT, "SHVX END\r\n");

        } else if (linePos < MAX_LINE) {
            line[linePos++] = c;
        }
    }
}
