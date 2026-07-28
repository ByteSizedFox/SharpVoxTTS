#ifndef SHARPVOX_FRAME_H
#define SHARPVOX_FRAME_H

// Windows builds lack M_PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <cstdint>
#include <cmath>

namespace SharpVox {

    struct Frame {
        int16_t Av;
        int16_t Af;
        int16_t F0;
        int16_t F1;
        int16_t F2;
        int16_t F3;
        int16_t A2;
        int16_t A3;
        int16_t A4;
        int16_t A5;
        int16_t A6;
        int16_t FNZ;
        int16_t AB;
        int16_t Bw1;
        int16_t Bw2;
        int16_t Bw3;
        int16_t PhonEdge;
        int64_t Marker;

        // Klattsch parameters
        uint8_t Aspiration;
        uint8_t Tilt;
        uint8_t Effort;
        uint8_t VibDepth;
        uint8_t VibRate;
        uint8_t TremDepth;
        uint8_t TremRate;
    };

    // Converts Hz to log-domain pitch code to allow integer formant arithmetic.
    inline int16_t HzToPitch(int16_t hz) {
        const int32_t ratioK = 2621;
        int32_t fk, freq;
        if (hz <= 0) {
            return 0;
        }
        if (hz < 50) {
            freq = hz << 4;
            fk = 0x0;
        } else if (hz < 100) {
            freq = hz << 3;
            fk = 0x100;
        } else if (hz < 200) {
            freq = hz << 2;
            fk = 0x200;
        } else if (hz < 400) {
            freq = hz << 1;
            fk = 0x300;
        } else if (hz < 800) {
            freq = hz;
            fk = 0x400;
        } else if (hz < 1600) {
            freq = hz >> 1;
            fk = 0x500;
        } else if (hz < 3200) {
            freq = hz >> 2;
            fk = 0x600;
        } else {
            freq = hz >> 3;
            fk = 0x700;
        }

        int32_t ratio = ((freq - 400) * ratioK) >> 11;
        if (ratio < 0) {
            ratio = 0;
        }
        if (ratio > 511) {
            ratio = 511;
        }
        // Runtime LogarithmBase2Table replacement: floor(256*log2(1 + ratio/512))
        int32_t log = (int32_t)(256.0 * std::log(1.0 + (ratio / 512.0)) / std::log(2.0));
        return (int16_t)(log + fk);
    }

    inline int16_t PitchToHz(int16_t pitch) {
        // Runtime OctaveFrequencyTable + ExponentialOf2Table replacement:
        // OctaveFrequencyTable[oct] = 25<<oct, ExponentialOf2Table[i] = round(32768*2^(i/256))
        int32_t oct = (pitch & 0xF00) >> 8;
        int32_t frac = pitch & 0xFF;
        int32_t baseFreq = 25 << oct;
        int32_t exp = (int32_t)std::round(32768.0 * std::pow(2.0, frac / 256.0));
        return (int16_t)((baseFreq * exp) >> 15);
    }

} // namespace SharpVox

#endif // SHARPVOX_FRAME_H
