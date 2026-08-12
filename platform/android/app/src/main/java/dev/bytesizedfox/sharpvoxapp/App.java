package dev.bytesizedfox.sharpvoxapp;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.Log;
import android.widget.Toast;

import androidx.core.content.FileProvider;

import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;

public class App extends Application {
    public static String current_voice = "john";
    public static float current_volume = 0.0f;
    public static int rate = 50;
    public static int pitch = 50;
    public static int last_system_pitch = 0;
    public static int last_system_rate = 0;
    public static boolean supportsAccessibility = true;
    public static boolean openSettings = false;

    public static native void nativeInit();
    public static native void nativeReset();
    public static native void nativeSetRate(int rate);
    public static native void nativeSetPitch(int pitch);
    public static native void nativeSetVolume(float volume);
    public static native void nativeSetVoice(String preset);
    public static native int nativeGetVoicePitchHz();
    public static native short[] nativeSpeak(String text, boolean enableCallback);
    public static native void nativeSync(int what);

    public static int getPitchHz() {
        return (int) (nativeGetVoicePitchHz() * (0.5f + pitch / 100.0f));
    }

    public static boolean isReady = false;

    public void loadPrefs() {
        if (isReady) {
            return;
        }

        SharedPreferences pref;

        try {
            pref = this.getSharedPreferences("settings", MODE_PRIVATE);
        } catch (Exception e) {
            App.current_voice = "john";
            App.current_volume = 90;
            App.pitch = 50;
            App.rate = 50;
            return;
        }

        isReady = true;

        try {
            App.current_voice = pref.getString("voice", "john");
            App.current_volume = pref.getFloat("volume", 50.0f);
            App.pitch = pref.getInt("pitch", 50);
            App.rate = pref.getInt("rate", 50);
            if (App.pitch < 0 || App.pitch > 100) {
                App.pitch = 50;
            }
            App.last_system_pitch = pref.getInt("last_pitch", 0);
            App.last_system_rate = pref.getInt("last_rate", 0);
        } catch (Exception e) {
            pref.edit().clear().commit();
            App.current_voice = "john";
            App.current_volume = 90;
            App.pitch = 50;
            App.rate = 50;
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.w("TtsApp","starting SharpVox TTS API!");
        System.loadLibrary("sharpvox");
        loadPrefs();
    }

    public static File writeWavFile(Context context, short[] audioData) throws IOException {
        File outputFile = File.createTempFile("audio_", ".wav", context.getCacheDir());
        int dataSize = audioData.length * 2;

        try (DataOutputStream out = new DataOutputStream(
                new BufferedOutputStream(new FileOutputStream(outputFile)))) {

            out.writeBytes("RIFF");
            out.writeInt(Integer.reverseBytes(36 + dataSize));
            out.writeBytes("WAVE");

            out.writeBytes("fmt ");
            out.writeInt(Integer.reverseBytes(16));
            out.writeShort(Short.reverseBytes((short) 1));
            out.writeShort(Short.reverseBytes((short) 1));
            out.writeInt(Integer.reverseBytes(22050));
            out.writeInt(Integer.reverseBytes(22050 * 2));
            out.writeShort(Short.reverseBytes((short) 2));
            out.writeShort(Short.reverseBytes((short) 16));

            out.writeBytes("data");
            out.writeInt(Integer.reverseBytes(dataSize));

            for (short sample : audioData) {
                out.writeShort(Short.reverseBytes(sample));
            }

            return outputFile;
        }
    }

    public static File writeTextFile(Context context, String textData) throws IOException {
        File outputFile = File.createTempFile("text_", ".txt", context.getCacheDir());

        try (DataOutputStream out = new DataOutputStream(
                new BufferedOutputStream(new FileOutputStream(outputFile)))) {
            out.writeBytes(textData);
            return outputFile;
        }
    }

    public static void shareAudioFile(Context context, File audioFile) {
        try {
            Uri fileUri = FileProvider.getUriForFile(
                    context,
                    context.getApplicationContext().getPackageName() + ".fileprovider",
                    audioFile
            );

            Intent shareIntent = new Intent(Intent.ACTION_SEND);
            shareIntent.setType("audio/wav");
            shareIntent.putExtra(Intent.EXTRA_STREAM, fileUri);
            shareIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

            context.startActivity(Intent.createChooser(shareIntent, "Share Audio File"));

        } catch (IllegalArgumentException e) {
            e.printStackTrace();
            Toast.makeText(context, "Error sharing file", Toast.LENGTH_SHORT).show();
        }
    }

    public static File currentAudioFile;
    public static void saveAudioFile(Activity activity, File audioFile) {
        currentAudioFile = audioFile;
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("audio/wav");
        intent.putExtra(Intent.EXTRA_TITLE, "recording.wav");
        activity.startActivityForResult(intent, 1001);
    }

    public static void saveTextFile(Activity activity, File textFile) {
        currentAudioFile = textFile;
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("audio/text");
        intent.putExtra(Intent.EXTRA_TITLE, "output.txt");
        activity.startActivityForResult(intent, 1001);
    }
}
