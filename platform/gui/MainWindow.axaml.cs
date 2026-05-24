#nullable enable
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using SharpTalk;
using SharpTalk.WebUi;

namespace SharpTalkGui;

public partial class MainWindow : Window {
    static readonly string[] PHON_NAMES = new[]
    {
        // 0-22: vowels monophthongs 0-11, diphthongs 12-17, r-colored 18-22
        "IY", "IH", "EH", "AE", "IX", "AX", "ER", "AH", "AA", "AO",
        "UH", "UW", "EY", "AY", "OY", "AW", "OW", "YU",
        "IR", "XR", "AR", "OR", "UR",
        // 23: silence
        "SIL",
        // 24-26 nasals, 27-30 approximants, 31-34 syllabics
        "M", "N", "NG",
        "W", "Y", "R", "L",
        "RX", "LX", "EL", "EN",
        // 35-43 fricatives, 44-51 plosives+affricates, 52-55 allophones
        "F", "V", "TH", "DH", "S", "Z", "SH", "ZH", "HH",
        "P", "B", "T", "D", "K", "G", "CH", "JH",
        "TX", "DX", "QX", "DD",
        // 56+ prosodic and punctuation markers
        "STRESS1", "STRESS2", "EMPH", "59", "60", "61", "62",
        "SYLL", "WORD", "PREP", "VERB", "COMMA", "PERIOD", "QUEST", "EXCLAM",
    };

    static readonly IBrush VowelColor = new SolidColorBrush(Color.Parse("#5BC4F5"));
    static readonly IBrush ConsonantColor = new SolidColorBrush(Color.Parse("#F5A623"));
    static readonly IBrush NeutralColor = new SolidColorBrush(Color.Parse("#888888"));

    public class PhonemeView {
        public string Code { get; set; } = "";
        public IBrush Color { get; set; } = NeutralColor;
    }

    readonly SharpTalkSpeaker _speaker;
    readonly AudioPlayer _audio;
    CancellationTokenSource? _cts;
    readonly DispatcherTimer _pollTimer;
    VoiceWindow? _voiceWindow;
    short[]? _lastSamples;
    string? _selectedUstPath;

    public MainWindow() {
        InitializeComponent();

        _speaker = new SharpTalkSpeaker();
        _audio = new AudioPlayer();

        // initial sample rate
        if (SampleRateCombo.SelectedItem is ComboBoxItem item &&
            int.TryParse(item.Content?.ToString(), out int rate)) {
            _speaker.SampleRate = rate;
            _speaker.ApplyVoice();
        }

        _speaker.OnPhoneme += OnPhonemeEvent;

        _pollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
        _pollTimer.Tick += OnPollTimerTick;

        // ctrl-enter shortcut in text box
        InputBox.KeyDown += OnInputBoxKeyDown;
    }

    void OnTabChanged(object? sender, SelectionChangedEventArgs e) {
        if (MainTabs == null || UstPanel == null || InputBox == null) {
            return;
        }
        bool isUst = MainTabs.SelectedItem == UstTab;
        InputBox.IsVisible = !isUst;
        UstPanel.IsVisible = isUst;

        if (MainTabs.SelectedItem == KlattschTab) {
            InputBox.Watermark = "HH AH L OW\nb140 AY+15 D IH D ...";
        } else {
            InputBox.Watermark = "Enter text to speak...";
        }
    }

    // user hit stop
    void OnStopClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        _cts?.Cancel();
        _pollTimer.Stop();
        _audio.Stop();
        SetStatus("Stopped");
    }

    async void OnSelectUstFileClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions {
            Title = "Select .ust file",
            FileTypeFilter = new[] { new FilePickerFileType("UST files") { Patterns = new[] { "*.ust" } } }
        });

        if (files.Count > 0) {
            _selectedUstPath = files[0].Path.LocalPath;
            UstFileNameLabel.Text = Path.GetFileName(_selectedUstPath);
        }
    }

    async void OnConvertUstClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        if (string.IsNullOrEmpty(_selectedUstPath)) {
            SetStatus("Please select a .ust file first.");
            return;
        }

        try {
            SetStatus("Converting UST...");
            byte[] bytes = await File.ReadAllBytesAsync(_selectedUstPath);

            string langSelection = (UstLanguageCombo.SelectedItem as ComboBoxItem)?.Content?.ToString()?.ToLower() ?? "auto";
            string lang = (langSelection == "auto-detect") ? "auto" : langSelection;
            int offset = (int)(UstOffsetNumeric.Value ?? 0);
            string? bank = (UstBankCombo.SelectedItem as ComboBoxItem)?.Content?.ToString();
            if (bank == "None (Default)") bank = null;

            var result = UstConverter.ConvertFromBytes(bytes, lang, offset, bank);
            InputBox.Text = result.Klattsch;
            UstDiagnosticsBox.Text = result.Diagnostics;
            SetStatus("Conversion complete");
            MainTabs.SelectedIndex = 1; // Klattsch Tab
        } catch (Exception ex) {
            SetStatus("Conversion failed: " + ex.Message);
        }
    }


    void OnSpeakClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) => StartSpeak();

    void OnInputBoxKeyDown(object? sender, KeyEventArgs e) {
        if (e.Key == Key.Return && e.KeyModifiers.HasFlag(KeyModifiers.Control)) {
            e.Handled = true;
            StartSpeak();
        }
    }

    void OnVoiceSettingsClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        if (_voiceWindow is null || !_voiceWindow.IsVisible) {
            _voiceWindow = new VoiceWindow(_speaker);
            _voiceWindow.Show(this);
        } else {
            _voiceWindow.Activate();
        }
    }

    void OnSampleRateChanged(object? sender, SelectionChangedEventArgs e) {
        if (SampleRateCombo?.SelectedItem is ComboBoxItem item &&
            int.TryParse(item.Content?.ToString(), out int rate)) {
            _speaker.SampleRate = rate;
            _speaker.ApplyVoice();
        }
    }


    void StartSpeak() {
        // Cancel previous
        _cts?.Cancel();
        _pollTimer.Stop();
        _audio.Stop();

        var text = InputBox.Text;
        if (string.IsNullOrWhiteSpace(text)) return;

        bool isKlattsch = MainTabs.SelectedItem == KlattschTab;
        string synText = text;
        if (isKlattsch) {
            synText = $"[:klattsch on] {text} [:klattsch off]";
        }
        MainTabs.SelectedIndex = 0;

        _cts = new CancellationTokenSource();
        var ct = _cts.Token;

        SetStatus("Synthesising...");

        Task.Run(() => _speaker.SpeakWithEvents(synText), ct)
            .ContinueWith(t => {
                Dispatcher.UIThread.Post(() => {
                    if (t.IsCanceled) {
                        SetStatus("Ready");
                        return;
                    }
                    if (t.IsFaulted) {
                        SetStatus("Error");
                        return;
                    }

                    var (samples, events) = t.Result;
                    if (Math.Abs(_speaker.OutputVolume - 1.0f) > 0.001f)
                        samples = ApplyVolume(samples, _speaker.OutputVolume);
                    _lastSamples = samples;

                    _audio.Play(samples, _speaker.SampleRate);
                    SetStatus("Speaking...");
                    _pollTimer.Start();
                });
            });
    }

    void OnPollTimerTick(object? sender, EventArgs e) {
        _speaker.PollAbsolute(_audio.ElapsedSeconds);

        if (!_audio.IsPlaying) {
            _pollTimer.Stop();
            SetStatus("Ready");
            PhonemeLabel.Text = "—";
            PhonemeLabel.Foreground = NeutralColor;
        }
    }

    void OnPhonemeEvent(PhonemeEvent evt) {
        var idx = evt.Phoneme;
        var name = (idx >= 0 && idx < PHON_NAMES.Length) ? PHON_NAMES[idx] : "?";

        IBrush color = idx switch {
            >= 0 and <= 22 => VowelColor,
            >= 28 and <= 55 => ConsonantColor,
            _ => NeutralColor,
        };

        Dispatcher.UIThread.Post(() => {
            PhonemeLabel.Text = name;
            PhonemeLabel.Foreground = color;
        });
    }


    async void OnSaveWavClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        if (_lastSamples is null) {
            return;
        }
        var file = await StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions {
            Title = "Save audio",
            DefaultExtension = "wav",
            SuggestedFileName = "output.wav",
            FileTypeChoices = [new FilePickerFileType("WAV audio") { Patterns = ["*.wav"] }],
        });
        if (file is null) {
            return;
        }
        await using var stream = await file.OpenWriteAsync();
        WriteWav(stream, _lastSamples);
    }

    void WriteWav(Stream stream, short[] samples) {
        using var w = new BinaryWriter(stream);
        int dataBytes = samples.Length * 2;
        w.Write("RIFF"u8.ToArray()); w.Write(36 + dataBytes);
        w.Write("WAVE"u8.ToArray());
        w.Write("fmt "u8.ToArray()); w.Write(16); w.Write((short)1); w.Write((short)1);
        w.Write(_speaker.SampleRate); w.Write(_speaker.SampleRate * 2);
        w.Write((short)2); w.Write((short)16);
        w.Write("data"u8.ToArray()); w.Write(dataBytes);
        foreach (var s in samples) {
            w.Write(s);
        }
    }


    static short[] ApplyVolume(short[] samples, float volume) {
        var result = new short[samples.Length];
        for (int i = 0; i < samples.Length; i++) {
            result[i] = (short)Math.Clamp(MathF.Round(samples[i] * volume), short.MinValue, short.MaxValue);
        }
        return result;
    }

    void SetStatus(string s) => StatusLabel.Text = s;

    protected override void OnClosed(EventArgs e) {
        _pollTimer.Stop();
        _audio.Dispose();
        _voiceWindow?.Close();
        base.OnClosed(e);
    }
}
