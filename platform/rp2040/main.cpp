#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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

// 11025 Hz: lowest quality tier, halves synthesis work vs 22050.
// At 11025×2 bytes/s = 22 KB/s, well within 230400 baud (~20 KB/s usable).
static constexpr int32_t SAMPLE_RATE = 11025;

// Cap at 6 seconds to stay within RP2040's 264 KB SRAM.
// Settled engine heap ~146 KB + 6s audio ~132 KB = ~278 KB — tight but fits
// because SymbolsTable peak (during init) and audio buffer never overlap.
static constexpr int32_t MAX_SAMPLES = SAMPLE_RATE * 6;

static constexpr int MAX_LINE = 255;

struct SynthCtx {
    std::vector<int16_t>* out;
    bool capped;
};

static void on_chunk(const int16_t* buf, int32_t len, void* ud) {
    auto* ctx = static_cast<SynthCtx*>(ud);
    if (ctx->capped) return;
    int32_t space = MAX_SAMPLES - (int32_t)ctx->out->size();
    if (space <= 0) { ctx->capped = true; return; }
    int32_t take = (len < space) ? len : space;
    ctx->out->insert(ctx->out->end(), buf, buf + take);
    if (take < len) ctx->capped = true;
}

static void uart_write_all(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) uart_putc_raw(UART_PORT, (char)data[i]);
}

int main() {
    uart_init(UART_PORT, UART_BAUD);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_PORT, false, false);
    uart_set_format(UART_PORT, 8, 1, UART_PARITY_NONE);

    // Build engine — heavy: dict index (60 KB), LTS rules, SymbolsTable etc.
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

            std::vector<int16_t> samples;
            samples.reserve(SAMPLE_RATE);
            SynthCtx ctx{ &samples, false };

            engine.Speak(std::string(line), on_chunk, &ctx);

            // Header: "SHVX AUDIO <rate> <count>\r\n"
            char hdr[48];
            int hlen = snprintf(hdr, sizeof(hdr), "SHVX AUDIO %d %d\r\n",
                                (int)SAMPLE_RATE, (int)samples.size());
            uart_write_all(reinterpret_cast<const uint8_t*>(hdr), (size_t)hlen);

            // Raw PCM (little-endian int16, native RP2040 byte order)
            uart_write_all(reinterpret_cast<const uint8_t*>(samples.data()),
                           samples.size() * sizeof(int16_t));

            uart_puts(UART_PORT, "SHVX END\r\n");

        } else if (linePos < MAX_LINE) {
            line[linePos++] = c;
        }
    }
}
