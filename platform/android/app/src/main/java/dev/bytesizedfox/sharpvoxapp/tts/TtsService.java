package dev.bytesizedfox.sharpvoxapp.tts;

import android.annotation.SuppressLint;
import android.media.AudioFormat;
import android.speech.tts.SynthesisCallback;
import android.speech.tts.SynthesisRequest;
import android.speech.tts.TextToSpeech;
import android.speech.tts.TextToSpeechService;
import android.speech.tts.Voice;
import android.util.Log;
import dev.bytesizedfox.sharpvoxapp.App;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

@SuppressLint("NewApi")
public class TtsService extends TextToSpeechService {
    private static final String TAG = "SharpVoxTTS";
    private static final int SAMPLE_RATE = 22050;
    private static final Locale DEFAULT_LOCALE = Locale.US;
    public static Voice mDefaultVoice;

    public native void SetCallback();

    public static SynthesisCallback mainCallback;

    public void javaCallback(short[] iwave, int length) {
        for (int i = 0; i < iwave.length; i++) {
            if (App.current_volume < 0) {
                App.current_volume = 0;
            }
            float percent = (App.current_volume / 100.0f);
            iwave[i] = (short) ( (float)iwave[i] * percent - 0.5f);
        }

        byte[] audioData = new byte[length * 2];
        ByteBuffer.wrap(audioData)
                .order(ByteOrder.LITTLE_ENDIAN)
                .asShortBuffer()
                .put(iwave);
        mainCallback.audioAvailable(audioData, 0, length * 2);
    }

    @Override
    public void onCreate() {
        super.onCreate();

        Log.d(TAG, "SharpVox TTS service created");
        Set<String> features = new HashSet<>();
        mDefaultVoice = new Voice("default", DEFAULT_LOCALE, Voice.QUALITY_VERY_HIGH, Voice.LATENCY_NORMAL, false, features);
        App.nativeInit();

        SetCallback();
    }

    @Override
    protected String[] onGetLanguage() {
        return new String[] {
                DEFAULT_LOCALE.getLanguage(),
                DEFAULT_LOCALE.getCountry(),
                DEFAULT_LOCALE.getVariant()
        };
    }

    @Override
    protected int onIsLanguageAvailable(String lang, String country, String variant) {
        return TextToSpeech.LANG_AVAILABLE;
    }

    @Override
    protected int onLoadLanguage(String lang, String country, String variant) {
        return TextToSpeech.LANG_AVAILABLE;
    }

    @Override
    public String onGetDefaultVoiceNameFor(String language, String country, String variant) {
        return mDefaultVoice.getName();
    }

    @Override
    public List<android.speech.tts.Voice> onGetVoices() {
        List<android.speech.tts.Voice> voices = new ArrayList<android.speech.tts.Voice>();
        voices.add(mDefaultVoice);
        return voices;
    }

    @Override
    public int onIsValidVoiceName(String name) {
        return TextToSpeech.SUCCESS;
    }

    @Override
    public int onLoadVoice(String name) {
        return TextToSpeech.SUCCESS;
    }

    @Override
    protected void onStop() {
        Log.d(TAG, "TTS service stopped");
        App.nativeReset();
    }

    @Override
    protected synchronized void onSynthesizeText(SynthesisRequest request, SynthesisCallback callback) {
        String text = request.getCharSequenceText().toString();
        Log.w("pitch", String.valueOf(request.getPitch()));
        Log.w("speech_rate", String.valueOf(request.getSpeechRate()));

        mainCallback = callback;

        ((App) getApplication().getApplicationContext()).loadPrefs();

        try {
            callback.start(SAMPLE_RATE, AudioFormat.ENCODING_PCM_16BIT, 1);

            App.nativeReset();
            App.nativeInit();
            App.nativeSetVolume(App.current_volume / 100.0f);
            App.nativeSetRate(100 + (App.rate * 5));
            App.nativeSetPitch(App.getPitchHz());
            App.nativeSetVoice(App.current_voice);

            App.nativeSpeak(text, true);

            callback.done();

        } catch (Exception e) {
            Log.e(TAG, "Error synthesizing text", e);
            callback.error();
        }
    }

}
