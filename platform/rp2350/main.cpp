#include <cstring>
#include <string>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "pico/time.h"
#include "pico/stdio_usb.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

// main lib
#include "SharpVox.h"

#include "pico/audio_pwm.h"
#include "ws2812.pio.h"

using namespace SharpVox;

#define UART_ID          uart0
#define BAUD_RATE        9600
#define UART_TX_PIN      0
#define UART_RX_PIN      1
#define MAX_LINE_LEN     256
#define LINE_QUEUE_DEPTH 8
#define STOP_CODE        0xC0

static constexpr int32_t SAMPLE_RATE = 22050;

// sync=true marks the final chunk of a logical line.
typedef struct { char text[MAX_LINE_LEN]; bool sync; } Line;
static queue_t lineQueue;

static volatile bool stopRequested = false;
static audio_buffer_pool_t* audioPool;

static void ledSet(uint32_t grb) {
    pio_sm_put_blocking(pio1, 0, grb << 8);
}

static alarm_id_t gLedOffAlarm = 0;

static int64_t ledOffAlarm(alarm_id_t, void*) {
    ledSet(0);
    gLedOffAlarm = 0;
    return 0;
}

static void ledBlink() {
    if (gLedOffAlarm) return;
    ledSet(0x101010);
    gLedOffAlarm = add_alarm_in_ms(60, ledOffAlarm, nullptr, true);
}

static void ledInit() {
    uint offset = pio_add_program(pio1, &ws2812_program);
    ws2812_program_init(pio1, 0, offset, SHVX_LED_PIN, 800000.0f);
    ledSet(0);
}

// setup PWM
static void spkrInit() {
    static audio_format_t fmt = { (uint32_t)SAMPLE_RATE, AUDIO_BUFFER_FORMAT_PCM_S16, 1 };
    static audio_buffer_format_t pfmt = { &fmt, 2 };
    audioPool = audio_new_producer_pool(&pfmt, 4, 576);

    audio_pwm_channel_config cfg = default_mono_channel_config;
    cfg.core.base_pin = SHVX_SPK_PIN;
    if (!audio_pwm_setup(&fmt, -1, &cfg)) panic("audio_pwm_setup failed");
    audio_pwm_default_connect(audioPool, false);
    float div = clock_get_hz(clk_sys) / 48000000.0f * (22058.0f / SAMPLE_RATE);
    pio_sm_set_clkdiv(pio0, PICO_AUDIO_PWM_MONO_PIO_SM, div);
    audio_pwm_set_enabled(true);
}

struct StopSynthesis {};

static uint32_t gWritten = 0;
static const PhonemeEvent* gPendingEvents = nullptr;
static int32_t gPendingCount = 0;

static void fireDueEvents() {
    while (gPendingCount > 0 && (int32_t)(gPendingEvents->TimeSeconds * SAMPLE_RATE + 0.5f) <= (int32_t)gWritten) {
        if (gPendingEvents->IsWordStart) {
            ledBlink();
            printf("WORD START\n");
        }
        const char* name = SharpVoxSpeaker::PhonemeName(gPendingEvents->Phoneme);
        if (name) {
            printf("PHON %s\n", name);
        } else {
            printf("PHON %d\n", gPendingEvents->Phoneme);
        }
        gPendingEvents++;
        gPendingCount--;
    }
}

static void onChunk(SharpVoxSpeaker*, const int16_t* buf, int32_t len,
                    const PhonemeEvent* events, int32_t count,
                    const FormantEvent*, int32_t, void*) {
    if (stopRequested) throw StopSynthesis{};
    gPendingEvents = events;
    gPendingCount = count;

    int32_t off = 0;
    while (off < len) {
        fireDueEvents();
        audio_buffer_t* ab;
        while (!(ab = take_audio_buffer(audioPool, false))) {
            if (stopRequested) throw StopSynthesis{};
            fireDueEvents();  // sink keeps consuming while we wait
        }
        int32_t n = ab->max_sample_count;
        if (n > len - off) n = len - off;
        memcpy(ab->buffer->bytes, buf + off, (size_t)n * sizeof(int16_t));
        ab->sample_count = (uint32_t)n;
        give_audio_buffer(audioPool, ab);
        gWritten += (uint32_t)n;
        fireDueEvents();
        off += n;
    }
    fireDueEvents();  // zero-length chunks carry trailing boundary events
}

static void speakLine(SharpVoxSpeaker& speaker, const std::string& text) {
    gWritten = 0;
    try {
        speaker.SpeakWithEvents(text, onChunk, nullptr);
    } catch (const StopSynthesis&) {}
}

static void core1TtsTask() {
    ledInit();
    spkrInit();

    SharpVoxSpeaker speaker;
    speaker.AudioVolume = 2.5; // runs a little quiet so boosting it

    multicore_fifo_push_blocking(1);
    speakLine(speaker, "SharpVox is running.");

    std::string pending;
    Line line;
    while (true) {
        // 0xC0 means shut up immediately and clear queue and continue with the text after 0xc0
        if (stopRequested) {
            stopRequested = false;
            Line discard;
            while (queue_try_remove(&lineQueue, &discard)) {}
            pending.clear();
            continue;
        }
        if (!queue_try_remove(&lineQueue, &line)) {
            tight_loop_contents();
            continue;
        }

        pending += line.text;
        if (!line.sync) continue;  // overflow chunk keeps accumulating

        try {
            speakLine(speaker, pending);
        } catch (...) {}
        pending.clear();
    }
}

static void enqueueLine(char* buf, int len, bool flush) {
    buf[len] = '\0';
    Line line;
    strncpy(line.text, buf, MAX_LINE_LEN - 1);
    line.text[MAX_LINE_LEN - 1] = '\0';
    line.sync = flush;
    queue_add_blocking(&lineQueue, &line);
}

static void processByte(int ch, char* buf, int* len) {
    if ((uint8_t)ch == STOP_CODE) {
        stopRequested = true;
        *len = 0;
        while (stopRequested) tight_loop_contents();
    } else if (ch == '\n' || ch == '\r') {
        if (*len > 0) {
            enqueueLine(buf, *len, true);
            *len = 0;
        }
    } else {
        if (*len >= MAX_LINE_LEN - 2) {
            enqueueLine(buf, *len, false);
            *len = 0;
        }
        buf[(*len)++] = (char)ch;
    }
}

int main() {
    stdio_init_all();

    uart_init(UART_ID, BAUD_RATE);  // stdio_uart disabled, UART is ours
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    queue_init(&lineQueue, sizeof(Line), LINE_QUEUE_DEPTH);
    multicore_launch_core1(core1TtsTask);
    multicore_fifo_pop_blocking();

    printf("READY\n");

    char usbBuf[MAX_LINE_LEN];
    int usbLen = 0;
    char uartBuf[MAX_LINE_LEN];
    int uartLen = 0;

    while (true) {
        if (uart_is_readable(UART_ID)) {
            processByte(uart_getc(UART_ID), uartBuf, &uartLen);
        }
        int ch = getchar_timeout_us(0);
        if (ch != PICO_ERROR_TIMEOUT) {
            processByte(ch, usbBuf, &usbLen);
        } else {
            tight_loop_contents();
        }
    }
}
