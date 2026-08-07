/*
 * FujisakiPitchModel.h - Fujisaki-Bartman phrase + accent command filters.
 *
 * Ported from TGSpeechBox (https://github.com/tgeczy/TGSpeechBox/),
 * src/pitchModel.h - "Fujisaki-Bartman pitch contour model for the DSP".
 * Copyright 2025-2026 Tamas Geczy. Licensed under the MIT License.
 * See LICENSE for details.
 *
 * SharpVox adaptation: the filters are stepped once per synthesis frame
 * (5 ms) instead of once per audio sample, so all lengths are expressed
 * in frames. The default time constants preserve the 22050 Hz reference
 * timing (phrase rise to peak ~193 ms, accent attack ~46 ms, accent
 * pulse ~340 ms). The reference returns exp(y1 + y2); this port returns
 * the raw log-domain sum so the caller can convert into its own pitch
 * scale without an exp/log round trip.
 */

#ifndef SHARPVOX_FUJISAKI_PITCH_MODEL_H
#define SHARPVOX_FUJISAKI_PITCH_MODEL_H

#include <cmath>
#include <cstdint>

namespace SharpVox {

    // Fujisaki-Bartman pitch contour model (DSP side).
    //
    //   phrase command: one-frame impulse    -> 2nd-order IIR (rise ~193 ms)
    //   accent command: rectangular pulse    -> 2nd-order IIR (attack ~46 ms)
    //   F0(t) = base(t) * exp(phraseResp(t) + accentResp(t))
    //
    // Step() returns the log-domain sum (y1 + y2) in nepers.  In a
    // logarithmic pitch scale the exp() multiplier becomes a linear
    // addition, so the caller adds (y1 + y2) * pitchUnitsPerNeper to its
    // baseline pitch.
    class FujisakiPitchModel {
    public:
        // Default time constants in 5 ms frames, derived from the
        // reference values at 22050 Hz (4250 / 1024 / 7500 samples).
        static constexpr int32_t kDefaultPhraseLenFrames = 39;  // ~193 ms rise to peak
        static constexpr int32_t kDefaultAccentLenFrames = 9;   // ~46 ms attack
        static constexpr int32_t kDefaultAccentDurFrames = 68;  // ~340 ms pulse

        FujisakiPitchModel() {
            Reset();
        }

        void Reset() {
            _pa = _pb = _pc = 0.0;
            _aa = _ab = _ac = 0.0;
            _px1 = _px2 = 0.0;
            _ax1 = _ax2 = 0.0;
            _phr = 0.0;
            _acc = 0.0;
            _countdown = 0;
        }

        // Trigger a phrase command: one-frame impulse of amplitude a
        // (nepers).  phraseLenFrames is the rise time to the peak;
        // 0 keeps the current design.
        void Phrase(double a, int32_t phraseLenFrames) {
            if (!(a > 0.0)) {
                return;
            }
            _phr = a;
            if (phraseLenFrames > 0) {
                DesignPhrase(phraseLenFrames);
            }
        }

        // Trigger an accent command: rectangular pulse of height a
        // (nepers) held for durationFrames frames, shaped by the accent
        // filter.  accentLenFrames is the attack time; 0 keeps the
        // current design.
        void Accent(double a, int32_t durationFrames, int32_t accentLenFrames) {
            if (!(a > 0.0)) {
                return;
            }
            _acc = a;
            _countdown = (durationFrames > 0) ? durationFrames : kDefaultAccentDurFrames;
            if (accentLenFrames > 0) {
                DesignAccent(accentLenFrames);
            }
        }

        // Advance both filters one frame and return y1 + y2 (nepers).
        double Step() {
            // Phrase command (impulse)
            const double y1 = _pa * _phr + _pb * _px1 + _pc * _px2;
            _px2 = _px1;
            _px1 = y1;
            _phr = 0.0;

            // Accent command (rectangular pulse while countdown > 0)
            double aimp = 0.0;
            if (_countdown > 0) {
                aimp = _acc;
                _countdown -= 1;
            }
            const double y2 = _aa * aimp + _ab * _ax1 + _ac * _ax2;
            _ax2 = _ax1;
            _ax1 = y2;

            return y1 + y2;
        }

        // Current filter responses (nepers) for diagnostics.
        double PhraseResp() const { return _px1; }
        double AccentResp() const { return _ax1; }

    private:
        static inline int32_t ClampInt(int32_t v, int32_t lo, int32_t hi) {
            if (v < lo) return lo;
            if (v > hi) return hi;
            return v;
        }

        void DesignPhrase(int32_t l) {
            // l is the length to reach the peak (in frames).
            if (l < 1) l = 1;
            const double nf = -1.0 / (double)l;
            const double r = std::exp(nf);
            const double c = -(r * r);
            const double b = 2.0 * r;
            const double p = std::exp(std::exp(1.0) * nf);  // gain compensation
            const double a = 1.0 - b * p - c * p;
            _pa = a; _pb = b; _pc = c;
        }

        void DesignAccent(int32_t l) {
            // l is the length to reach the peak (in frames).
            if (l < 1) l = 1;
            const double nf = -1.0 / (double)l;
            const double r = std::exp(nf);
            const double c = -(r * r);
            const double b = 2.0 * r;
            const double a = 1.0 - b - c;
            _aa = a; _ab = b; _ac = c;
        }

        // Phrase filter coefficients
        double _pa, _pb, _pc;
        // Accent filter coefficients
        double _aa, _ab, _ac;
        // Past outputs
        double _px1, _px2;
        double _ax1, _ax2;
        // Impulse / pulse state
        double _phr;
        double _acc;
        int32_t _countdown;
    };

}  // namespace SharpVox

#endif  // SHARPVOX_FUJISAKI_PITCH_MODEL_H
