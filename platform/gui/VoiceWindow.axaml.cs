#nullable enable
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using SharpTalk;

namespace SharpTalkGui;

public partial class VoiceWindow : Window {
    readonly SharpTalkSpeaker _speaker;

    // Tracks controls so we can refresh values when a preset is applied or voice is imported
    readonly List<(string PropName, Slider Slider, TextBlock ValueLabel, Func<float> Getter)> _sliders = new();
    readonly List<(string PropName, CheckBox CheckBox, Func<bool> Getter)> _toggles = new();

    public VoiceWindow(SharpTalkSpeaker speaker) {
        InitializeComponent();
        _speaker = speaker;
        BuildSliders();
    }


    void OnBaselineClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        _speaker.Preset = VoicePreset.Baseline;
        _speaker.ApplyVoice();
        RefreshSliders();
    }

    void OnWhisperClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        _speaker.Preset = VoicePreset.Whisper;
        _speaker.ApplyVoice();
        RefreshSliders();
    }


    void BuildSliders() {
        AddGroupHeader("Klattsch Defaults");
        AddFloatSlider("klBaseF0", "Base F0 (Hz)", "Starting fundamental frequency in Hz", 70, 320, 1, () => _speaker.KlBaseF0, v => _speaker.KlBaseF0 = v);
        AddFloatSlider("klRate", "Rate (ms)", "Milliseconds per phoneme slot", 60, 500, 1, () => _speaker.KlRate, v => _speaker.KlRate = v);
        AddFloatSlider("klVibrato", "Vibrato (Hz)", "Vibrato depth: peak deviation in Hz (0 = off)", 0, 15, 0.5f, () => _speaker.KlVibrato, v => _speaker.KlVibrato = v);
        AddFloatSlider("klVibRate", "Vib Rate (Hz)", "Vibrato LFO rate in Hz (default 5)", 2, 10, 0.1f, () => _speaker.KlVibRate, v => _speaker.KlVibRate = v);
        AddFloatSlider("klAsp", "Breathiness", "Aspiration/breath mix, 0–1", 0, 1, 0.01f, () => _speaker.KlAsp, v => _speaker.KlAsp = v);
        AddFloatSlider("klTilt", "Spectral Tilt", "Spectral tilt: negative=darker, positive=brighter", -0.9f, 0.9f, 0.01f, () => _speaker.KlTilt, v => _speaker.KlTilt = v);
        AddFloatSlider("klEffort", "Effort", "Vocal effort: 0=lax, 1=tense (0.5 default)", 0, 1, 0.01f, () => _speaker.KlEffort, v => _speaker.KlEffort = v);

        AddGroupHeader("Playback");
        AddSlider("Rate", "Speaking rate (words per minute)", 40, 600, () => _speaker.Rate, v => _speaker.Rate = v);
        AddOutputVolumeSlider();

        AddGroupHeader("Voice");
        AddToggle("Female", "Shifts character towards a female voice", () => _speaker.Female, v => _speaker.Female = v);
        AddSlider("PitchHz", "Fundamental pitch in Hz", 40, 500, () => _speaker.PitchHz, v => _speaker.PitchHz = v);
        AddSlider("PitchOffsetHz", "F0 offset in Hz — shifts fundamental frequency up or down for all speech and singing. Use to transpose a voice or force singing notes lower/higher.", -200, 200, () => _speaker.PitchOffsetHz, v => _speaker.PitchOffsetHz = v);
        AddFloatSlider("TractScale", "TractScale", "Vocal tract scale — <1 larger/deeper, >1 smaller/brighter", 0.5f, 2.0f, 0.01f, () => _speaker.TractScale, v => _speaker.TractScale = v);
        AddSlider("LarynxOffset", "Larynx height (Hz offset) — shifts all formants up/down coherently. >0 raised larynx (brighter, more projected), <0 lowered larynx (darker, operatic/rounded)", -200, 200, () => _speaker.LarynxOffset, v => _speaker.LarynxOffset = v);
        AddSlider("PharyngealAmt", "Pharyngeal constriction — raises F1, lowers F2 twice as much, slight F3 lowering. The acoustic basis of the pirate \"arrr\" quality and Arabic pharyngealization.", 0, 100, () => _speaker.PharyngealAmt, v => _speaker.PharyngealAmt = v);
        AddSlider("LipRounding", "Lip rounding/spreading — positive rounds lips (F1↓, F2↓↓, F3↓), negative spreads (F1↑, F2↑↑, F3↑). Rounds front vowels toward /u/, spreads back vowels toward /i/.", -100, 100, () => _speaker.LipRounding, v => _speaker.LipRounding = v);
        AddSlider("Jitter", "Random cycle-to-cycle pitch perturbation — adds roughness and creak", 0, 100, () => _speaker.Jitter, v => _speaker.Jitter = v);
        AddSlider("Shimmer", "Random cycle-to-cycle amplitude perturbation — adds breathiness and gravel", 0, 100, () => _speaker.Shimmer, v => _speaker.Shimmer = v);
        AddSlider("Diplophonia", "Alternating strong/weak pulse pattern — adds subharmonic at F0/2, pirate growl effect", 0, 100, () => _speaker.Diplophonia, v => _speaker.Diplophonia = v);
        AddSlider("FryAmount", "Vocal fry — randomly extends glottal cycles to 1/2 or 1/4 speed, creating irregular creak. Combine with Jitter for texture.", 0, 100, () => _speaker.FryAmount, v => _speaker.FryAmount = v);
        AddSlider("SubglottalAmt", "Subglottal resonance coupling — ~350 Hz chest cavity texture mixed into the voiced source. Adds warmth and weight to chest voice.", 0, 100, () => _speaker.SubglottalAmt, v => _speaker.SubglottalAmt = v);
        AddSlider("BreathAmt", "Cycle-synchronous breathiness — noise injected only during the glottal open phase (scaled by waveform amplitude). Independent of aspiration; stays in cascade path so it has vowel formant coloring.", 0, 100, () => _speaker.BreathAmt, v => _speaker.BreathAmt = v);
        AddSlider("OpenQuotient", "Glottal open quotient — 0=pressed/bright (short open phase, harmonics-rich source), 50=neutral, 100=breathy/dark (long open phase, sine-like source)", 0, 100, () => _speaker.OpenQuotient, v => _speaker.OpenQuotient = v);
        AddSlider("OQStressLink", "Dynamic OQ: stress→pressed — each stressed syllable pushes the open quotient down (brighter, more harmonic energy). 0=static OQ only, 100=full coupling.", 0, 100, () => _speaker.OQStressLink, v => _speaker.OQStressLink = v);
        AddSlider("OQF0Link", "Dynamic OQ: pitch→breathy — F0 above baseline pushes open quotient up (darker, more breathy). Models falsetto register onset. 0=static only, 100=full coupling.", 0, 100, () => _speaker.OQF0Link, v => _speaker.OQF0Link = v);
        AddSlider("OnsetHardness", "Voiced onset ramp — 0=soft breathy onset (slow AV ramp, choir-like), 50=natural, 100=hard glottal attack (AV snaps to target instantly, aggressive vowel onset)", 0, 100, () => _speaker.OnsetHardness, v => _speaker.OnsetHardness = v);
        AddSlider("VoicingGain", "Strength of voiced (periodic) excitation", 0, 500, () => _speaker.VoicingGain, v => _speaker.VoicingGain = v);
        AddSlider("AspirationGain", "Strength of aspiration/breath noise", 0, 1000, () => _speaker.AspirationGain, v => _speaker.AspirationGain = v);
        AddSlider("AspirationCycle", "Aspiration noise cycle length", 0, 255, () => _speaker.AspirationCycle, v => _speaker.AspirationCycle = v);
        AddSlider("TremoloDepth", "Depth of amplitude tremolo — 0 = off, 100 = full; adds vocal growl/gravel", 0, 100, () => _speaker.TremoloDepth, v => _speaker.TremoloDepth = v);
        AddSlider("TremoloRate", "Rate of tremolo oscillation (×0.1 Hz) — e.g. 40 = 4 Hz; works with TremoloDepth > 0", 0, 200, () => _speaker.TremoloRate, v => _speaker.TremoloRate = v);
        AddSlider("NGain", "Nasal resonator gain", 0, 400, () => _speaker.NGain, v => _speaker.NGain = v);

        AddGroupHeader("Formants (F4–F6)");
        AddSlider("F4Freq", "4th formant frequency (Hz) — adds brightness", 2000, 5000, () => _speaker.F4Freq, v => _speaker.F4Freq = v);
        AddSlider("F4BW", "4th formant bandwidth", 50, 2048, () => _speaker.F4BW, v => _speaker.F4BW = v);
        AddSlider("F5Freq", "5th cascade formant frequency (Hz)", 2000, 8000, () => _speaker.F5Freq, v => _speaker.F5Freq = v);
        AddSlider("F5BW", "5th cascade formant bandwidth", 50, 2048, () => _speaker.F5BW, v => _speaker.F5BW = v);
        AddSlider("F4pFreq", "4th parallel formant frequency", 2000, 5000, () => _speaker.F4pFreq, v => _speaker.F4pFreq = v);
        AddSlider("F4pBW", "4th parallel formant bandwidth", 50, 2048, () => _speaker.F4pBW, v => _speaker.F4pBW = v);
        AddSlider("F5pFreq", "5th parallel formant frequency", 2000, 6000, () => _speaker.F5pFreq, v => _speaker.F5pFreq = v);
        AddSlider("F5pBW", "5th parallel formant bandwidth", 50, 2048, () => _speaker.F5pBW, v => _speaker.F5pBW = v);
        AddSlider("F6pFreq", "6th parallel formant frequency", 2000, 8000, () => _speaker.F6pFreq, v => _speaker.F6pFreq = v);
        AddSlider("F6pBW", "6th parallel formant bandwidth", 50, 2048, () => _speaker.F6pBW, v => _speaker.F6pBW = v);

        AddGroupHeader("Bandwidth");
        AddSlider("BwGain1", "F1 bandwidth gain", 0, 300, () => _speaker.BwGain1, v => _speaker.BwGain1 = v);
        AddSlider("BwGain2", "F2 bandwidth gain", 0, 300, () => _speaker.BwGain2, v => _speaker.BwGain2 = v);
        AddSlider("BwGain3", "F3 bandwidth gain", 0, 300, () => _speaker.BwGain3, v => _speaker.BwGain3 = v);

        AddGroupHeader("Nasal");
        AddSlider("NasalBase", "Nasal pole base frequency (Hz)", 200, 600, () => _speaker.NasalBase, v => _speaker.NasalBase = v);
        AddSlider("NasalTarg", "Nasal pole target frequency (Hz)", 200, 600, () => _speaker.NasalTarg, v => _speaker.NasalTarg = v);
        AddSlider("NasalBW", "Nasal pole bandwidth", 30, 2048, () => _speaker.NasalBW, v => _speaker.NasalBW = v);

        AddGroupHeader("Intonation");
        AddSlider("PitchRange", "Pitch excursion range (% of baseline)", 0, 300, () => _speaker.PitchRange, v => _speaker.PitchRange = v);
        AddSlider("StressGain", "Pitch gain on stressed syllables", 0, 200, () => _speaker.StressGain, v => _speaker.StressGain = v);
        AddSlider("Intonation", "Overall intonation depth (%)", 0, 200, () => _speaker.Intonation, v => _speaker.Intonation = v);
        AddSlider("RiseAmt", "Clause-final pitch rise amount", -100, 100, () => _speaker.RiseAmt, v => _speaker.RiseAmt = v);
        AddSlider("FallAmt", "Clause-final pitch fall amount", -100, 100, () => _speaker.FallAmt, v => _speaker.FallAmt = v);
        AddSlider("BaselineFall", "Declination — pitch fall over utterance", 0, 150, () => _speaker.BaselineFall, v => _speaker.BaselineFall = v);
        AddSlider("UptalkAmt", "Sentence-final rising tendency — 0=natural fall, 100=uptalk/rising statements", 0, 100, () => _speaker.UptalkAmt, v => _speaker.UptalkAmt = v);
        AddSlider("StressEarly", "Stress peak alignment — negative=early/assertive, 0=natural, positive=late/hesitant", -50, 50, () => _speaker.StressEarly, v => _speaker.StressEarly = v);
        AddSlider("BreakStrength", "Phrase boundary reset — 0=smooth carry-over, 50=natural, 100=hard fresh start", 0, 100, () => _speaker.BreakStrength, v => _speaker.BreakStrength = v);
        AddSlider("EmphasisBoost", "Extra pitch height for emphatic stress — amplifies contrast between emphatic and primary stress", 0, 100, () => _speaker.EmphasisBoost, v => _speaker.EmphasisBoost = v);
        AddSlider("VocalConfidence", "Pronoun emphasis — subject pronouns (I/you/he/she/it/we/they) get a rise-fall pitch accent and vowel lengthening; 0=none, 100=full", 0, 100, () => _speaker.VocalConfidence, v => _speaker.VocalConfidence = v);
    }

    void AddGroupHeader(string title) {
        var tb = new TextBlock {
            Text = title.ToUpperInvariant(),
            Foreground = new SolidColorBrush(Color.Parse("#666666")),
            FontSize = 10,
            FontWeight = FontWeight.Bold,
            Margin = new Thickness(2, 14, 2, 4),
            LetterSpacing = 1.5,
        };
        SliderPanel.Children.Add(tb);
    }

    void AddSlider(string name, string description, int min, int max, Func<int> getter, Action<int> setter)
        => AddFloatSlider(name, name, description, min, max, 1, () => (float)getter(), v => setter((int)v), "0");

    void AddOutputVolumeSlider() {
        float GetVol() => _speaker.OutputVolume;

        var grid = new Grid {
            ColumnDefinitions = new ColumnDefinitions("130,*,50"),
            Margin = new Thickness(0, 1),
            Background = Brushes.Transparent,
        };

        var label = new TextBlock {
            Text = "Volume",
            Foreground = new SolidColorBrush(Color.Parse("#CCCCCC")),
            FontSize = 12,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(2, 0, 8, 0),
        };
        Grid.SetColumn(label, 0);

        var slider = new Slider {
            Minimum = 0.1,
            Maximum = 5.0,
            Value = GetVol(),
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(0, 0, 4, 0),
            SmallChange = 0.1,
            LargeChange = 0.5,
        };
        Grid.SetColumn(slider, 1);

        var valueLabel = new TextBlock {
            Text = GetVol().ToString("0.#"),
            Foreground = new SolidColorBrush(Color.Parse("#AAAAAA")),
            FontSize = 11,
            VerticalAlignment = VerticalAlignment.Center,
            TextAlignment = TextAlignment.Right,
            Width = 46,
        };
        Grid.SetColumn(valueLabel, 2);

        slider.ValueChanged += (s, e) => {
            float v = (float)(Math.Round(e.NewValue / 0.1) * 0.1);
            valueLabel.Text = v.ToString("0.#");
            _speaker.OutputVolume = v;
        };

        grid.PointerEntered += (s, e) => {
            DescTitle.Text = "Volume";
            DescBody.Text = "Overall output volume (0.1 to 5.0)";
        };

        grid.Children.Add(label);
        grid.Children.Add(slider);
        grid.Children.Add(valueLabel);
        SliderPanel.Children.Add(grid);
    }

    void AddFloatSlider(string propName, string labelText, string description, float min, float max, float step, Func<float> getter, Action<float> setter, string format = "0.##") {
        float currentVal = getter();

        var grid = new Grid {
            ColumnDefinitions = new ColumnDefinitions("130,*,50"),
            Margin = new Thickness(0, 1),
            Background = Brushes.Transparent,
        };

        var label = new TextBlock {
            Text = labelText,
            Foreground = new SolidColorBrush(Color.Parse("#CCCCCC")),
            FontSize = 12,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(2, 0, 8, 0),
            TextTrimming = TextTrimming.CharacterEllipsis,
        };
        Grid.SetColumn(label, 0);

        var slider = new Slider {
            Minimum = min,
            Maximum = max,
            Value = currentVal,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(0, 0, 4, 0),
            SmallChange = step,
            LargeChange = Math.Max(step, (max - min) / 10),
        };
        Grid.SetColumn(slider, 1);

        var valueLabel = new TextBlock {
            Text = currentVal.ToString(format),
            Foreground = new SolidColorBrush(Color.Parse("#AAAAAA")),
            FontSize = 11,
            VerticalAlignment = VerticalAlignment.Center,
            TextAlignment = TextAlignment.Right,
            Width = 46,
        };
        Grid.SetColumn(valueLabel, 2);

        slider.ValueChanged += (s, e) => {
            float v = (float)e.NewValue;
            if (step >= 1) v = (float)Math.Round(v);
            else v = (float)(Math.Round(v / step) * step);

            valueLabel.Text = v.ToString(format);
            setter(v);
            _speaker.ApplyVoice();
        };

        grid.PointerEntered += (s, e) => {
            DescTitle.Text = labelText;
            DescBody.Text = description;
        };

        grid.Children.Add(label);
        grid.Children.Add(slider);
        grid.Children.Add(valueLabel);

        SliderPanel.Children.Add(grid);

        _sliders.Add((propName, slider, valueLabel, getter));
    }

    void AddToggle(string name, string description, Func<bool> getter, Action<bool> setter) {
        var grid = new Grid {
            ColumnDefinitions = new ColumnDefinitions("130,*,50"),
            Margin = new Thickness(0, 1),
            Background = Brushes.Transparent,
        };

        var label = new TextBlock {
            Text = name,
            Foreground = new SolidColorBrush(Color.Parse("#CCCCCC")),
            FontSize = 12,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(2, 0, 8, 0),
        };
        Grid.SetColumn(label, 0);

        var cb = new CheckBox {
            IsChecked = getter(),
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetColumn(cb, 1);

        cb.IsCheckedChanged += (s, e) => {
            setter(cb.IsChecked ?? false);
            _speaker.ApplyVoice();
        };

        grid.PointerEntered += (s, e) => {
            DescTitle.Text = name;
            DescBody.Text = description;
        };

        grid.Children.Add(label);
        grid.Children.Add(cb);
        SliderPanel.Children.Add(grid);

        _toggles.Add((name, cb, getter));
    }


    void RefreshSliders() {
        foreach (var (_, slider, valueLabel, getter) in _sliders) {
            float val = Math.Clamp(getter(), (float)slider.Minimum, (float)slider.Maximum);
            slider.Value = val;
            valueLabel.Text = val.ToString(val % 1 == 0 ? "0" : "0.##");
        }
        foreach (var (_, cb, getter) in _toggles) {
            cb.IsChecked = getter();
        }
    }


    static readonly JsonSerializerOptions _jsonOpts = new() { WriteIndented = true };

    static readonly FilePickerFileType _jsonType =
        new("Voice preset") { Patterns = ["*.json"] };

    async void OnExportClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        var file = await StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions {
            Title = "Export voice preset",
            DefaultExtension = "json",
            FileTypeChoices = [_jsonType],
            SuggestedFileName = "voice.json",
        });
        if (file is null) return;

        var dict = new Dictionary<string, object>();
        foreach (var (name, _, _, getter) in _sliders)
            dict[name] = getter();
        foreach (var (name, _, getter) in _toggles)
            dict[name] = getter();

        await using var stream = await file.OpenWriteAsync();
        await JsonSerializer.SerializeAsync(stream, dict, _jsonOpts);
    }

    async void OnImportClicked(object? sender, Avalonia.Interactivity.RoutedEventArgs e) {
        var files = await StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions {
            Title = "Import voice preset",
            AllowMultiple = false,
            FileTypeFilter = [_jsonType],
        });
        if (files.Count == 0) return;

        Dictionary<string, JsonElement>? dict;
        try {
            await using var stream = await files[0].OpenReadAsync();
            dict = await JsonSerializer.DeserializeAsync<Dictionary<string, JsonElement>>(stream);
        } catch { return; }
        if (dict is null) return;

        foreach (var (propName, slider, _, _) in _sliders) {
            if (dict.TryGetValue(propName, out var val) && val.ValueKind == JsonValueKind.Number)
                slider.Value = Math.Clamp((float)val.GetDouble(), (float)slider.Minimum, (float)slider.Maximum);
        }
        foreach (var (propName, cb, _) in _toggles) {
            if (dict.TryGetValue(propName, out var val)) {
                if (val.ValueKind == JsonValueKind.True || val.ValueKind == JsonValueKind.False)
                    cb.IsChecked = val.GetBoolean();
                else if (val.ValueKind == JsonValueKind.Number)
                    cb.IsChecked = val.GetDouble() != 0;
            }
        }
        _speaker.ApplyVoice();
    }
}
