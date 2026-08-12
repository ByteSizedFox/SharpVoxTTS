package dev.bytesizedfox.sharpvoxapp;

import static dev.bytesizedfox.sharpvoxapp.App.shareAudioFile;
import static dev.bytesizedfox.sharpvoxapp.App.writeTextFile;
import static dev.bytesizedfox.sharpvoxapp.App.writeWavFile;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.speech.tts.TextToSpeech;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.bottomnavigation.BottomNavigationView;
import com.google.android.material.slider.Slider;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;

public class MainActivity extends AppCompatActivity implements TextToSpeech.OnInitListener {
    private TextToSpeech tts;

    public Button speakButton;
    public Button stopButton;
    public Spinner voiceSpinner;
    public Slider volumeBar;
    public Slider pitchBar;
    public Slider rateBar;
    public TextView volumeLabel;
    public TextView pitchLabel;
    public TextView rateLabel;

    public static void setVolume(int value) {
        App.current_volume = value;
    }

    private void updateLabels() {
        volumeLabel.setText("Volume: " + (int) App.current_volume + "%");
        pitchLabel.setText("Pitch: " + App.getPitchHz() + " Hz");
        rateLabel.setText("Speed: " + (100 + (App.rate * 5)) + " wpm");

        volumeBar.setStateDescription((int) App.current_volume + " percent");
        pitchBar.setStateDescription(App.pitch + " percent");
        rateBar.setStateDescription((100 + (App.rate * 5)) + " words per minute");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        tts = new TextToSpeech(this, this, "dev.bytesizedfox.sharpvoxapp");

        setContentView(R.layout.activity_main);
        setSupportActionBar(findViewById(R.id.my_toolbar));

        App.nativeInit();

        EditText inputText = findViewById(R.id.inputText);
        speakButton = findViewById(R.id.speakButton);
        speakButton.setOnClickListener(v -> {
            tts.stop();
            tts.speak(inputText.getText().toString(), TextToSpeech.QUEUE_FLUSH, null, null);
        });

        stopButton = findViewById(R.id.stopButton);
        stopButton.setOnClickListener(v -> tts.stop());

        BottomNavigationView bottomNav = findViewById(R.id.bottomNav);
        bottomNav.setOnNavigationItemSelectedListener(item -> {
            int id = item.getItemId();
            if (id == R.id.nav_speak) {
                findViewById(R.id.speakContent).setVisibility(View.VISIBLE);
                findViewById(R.id.advancedContent).setVisibility(View.GONE);
                return true;
            } else if (id == R.id.nav_advanced) {
                findViewById(R.id.speakContent).setVisibility(View.GONE);
                findViewById(R.id.advancedContent).setVisibility(View.VISIBLE);
                return true;
            }
            return false;
        });
        bottomNav.setSelectedItemId(R.id.nav_speak);

        initSettings();
    }

    @Override
    public boolean onCreateOptionsMenu( Menu menu ) {
        getMenuInflater().inflate(R.menu.menu, menu);
        return super.onCreateOptionsMenu(menu);
    }

    @Override
    public boolean onPrepareOptionsMenu(Menu menu) {
        return true;
    }

    @Override
    public boolean onOptionsItemSelected( @NonNull MenuItem item ) {
        int id = item.getItemId();

        EditText inputText = findViewById(R.id.inputText);
        String text = inputText.getText().toString();
        short[] samples = new short[0];
        if (id == R.id.share || id == R.id.export) {
            App.nativeReset();
            App.nativeInit();
            App.nativeSetRate(100 + (App.rate * 5));
            App.nativeSetPitch(App.getPitchHz());
            App.nativeSetVoice(App.current_voice);
            samples = App.nativeSpeak(text, false);
        }
        if (id == R.id.share) {
            try {
                File wavFile = writeWavFile(this, samples);
                shareAudioFile(this, wavFile);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
            return super.onOptionsItemSelected(item);
        }
        if (id == R.id.export) {
            try {
                File wavFile = writeWavFile(this, samples);
                App.saveAudioFile(this, wavFile);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
            return super.onOptionsItemSelected(item);
        }
        if (id == R.id.SaveText) {
            File textFile = null;
            try {
                textFile = writeTextFile(this, inputText.getText().toString());
                App.saveTextFile(this, textFile);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }

            return super.onOptionsItemSelected(item);
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onDestroy() {
        App.nativeReset();

        if (tts != null) {
            tts.stop();
            tts.shutdown();
        }

        super.onDestroy();
    }

    List<String> voiceList = Arrays.asList(
            "baseline",
            "beth",
            "chris",
            "deborah",
            "jack",
            "jess",
            "john",
            "matt",
            "pirate",
            "tommy",
            "whisper"
    );
    boolean initSelection = true;
    private void initSettings() {
        voiceSpinner = findViewById(R.id.VoiceSpinner);
        ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(
                this,
                R.array.voice_names,
                android.R.layout.simple_spinner_item
        );
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        voiceSpinner.setAdapter(adapter);
        voiceSpinner.setSelection(voiceList.indexOf(App.current_voice));

        App.nativeSetVoice(App.current_voice);

        SharedPreferences pref = this.getSharedPreferences("settings", MODE_PRIVATE);

        voiceSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (initSelection) {
                    initSelection = false;
                    return;
                }

                updateBars();

                String voiceID = (String) parent.getItemAtPosition(parent.getSelectedItemPosition());
                App.nativeSetVoice(voiceID);

                tts.speak(voiceID, TextToSpeech.QUEUE_FLUSH, null, null);

                App.current_voice = voiceID.toLowerCase();
                App.nativeSetPitch(App.getPitchHz());
                SharedPreferences.Editor editor = pref.edit();
                editor.putString("voice", App.current_voice);
                editor.putInt("pitch", App.pitch);
                editor.apply();
                updateLabels();
            }
            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });


        volumeBar = findViewById(R.id.volumeBar);
        volumeBar.setValue(App.current_volume);
        volumeBar.addOnChangeListener(new Slider.OnChangeListener() {
            @Override
            public void onValueChange(Slider slider, float value, boolean fromUser) {
                MainActivity.setVolume((int) value);
                updateLabels();
                SharedPreferences.Editor editor = pref.edit();
                editor.putFloat("volume", value);
                editor.apply();
            }
        });

        pitchBar = findViewById(R.id.pitchBar);
        if (App.pitch < 0 || App.pitch > 100) {
            App.pitch = 50;
        }
        pitchBar.setValue(App.pitch);
        pitchBar.addOnChangeListener(new Slider.OnChangeListener() {
            @Override
            public void onValueChange(Slider slider, float value, boolean fromUser) {
                SharedPreferences.Editor editor = pref.edit();
                App.pitch = (int) value;
                App.nativeSetPitch(App.getPitchHz());
                updateLabels();
                editor.putInt("pitch", (int) value);
                editor.apply();
            }
        });

        rateBar = findViewById(R.id.rateBar);
        rateBar.setValue(App.rate);
        rateBar.addOnChangeListener(new Slider.OnChangeListener() {
            @Override
            public void onValueChange(Slider slider, float value, boolean fromUser) {
                SharedPreferences.Editor editor = pref.edit();
                App.rate = (int) value;
                updateLabels();
                editor.putInt("rate", (int) value);
                editor.apply();
            }
        });

        volumeLabel = findViewById(R.id.volumeLabel);
        pitchLabel = findViewById(R.id.pitchLabel);
        rateLabel = findViewById(R.id.rateLabel);
        updateLabels();
    }

    void updateBars() {
        if (!App.supportsAccessibility) {
            return;
        }
        try {
            int rate = Settings.Secure.getInt(getContentResolver(), Settings.Secure.TTS_DEFAULT_RATE);

            SharedPreferences pref = this.getSharedPreferences("settings", MODE_PRIVATE);

            if (rate != App.last_system_rate) {
                App.last_system_rate = rate;
                App.rate = rate / 4;
                pref.edit().putInt("last_rate", rate).apply();
                rateBar.setValue(rate / 4);
            }
        } catch (Exception e) {
            App.supportsAccessibility = false;
        }
        updateLabels();
    }

    @Override
    public void onResume() {
        super.onResume();
        updateBars();
    }
    @Override
    public void onPause() {
        super.onPause();
        updateBars();
    }

    @Override
    public void onInit(int status) {
        if (status == TextToSpeech.SUCCESS) {
            int result = tts.setLanguage(Locale.US);
            speakButton.setEnabled(true);
            if (result == TextToSpeech.LANG_MISSING_DATA || result == TextToSpeech.LANG_NOT_SUPPORTED) {
            }
        } else {
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode == 1001 && resultCode == Activity.RESULT_OK) {
            if (data != null) {
                Uri uri = data.getData();
                try {
                    InputStream inputStream = new FileInputStream(App.currentAudioFile);
                    OutputStream outputStream = getContentResolver().openOutputStream(uri);

                    if (outputStream != null) {
                        byte[] buf = new byte[1024];
                        int len;
                        while ((len = inputStream.read(buf)) > 0) {
                            outputStream.write(buf, 0, len);
                        }
                        outputStream.close();
                        inputStream.close();
                        Toast.makeText(this, "File saved successfully", Toast.LENGTH_SHORT).show();
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                    Toast.makeText(this, "Error saving file", Toast.LENGTH_SHORT).show();
                }
            }
        }
    }
}
