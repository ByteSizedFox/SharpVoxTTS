#nullable enable
namespace SharpTalk {

    [System.Serializable]
    public sealed class VoiceData {
        public short PitchHz = 208;
        public float TractScale = 1.0f;
        public short PitchRange = 120;
        public short StressGain = 60;
        public short Rate = 160;
        public short VoiceType = 0;
        public short VGain = 60;
        public short AGain = 0;
        public short ACycle = 192;

        public short TremoloDepth = 0;   // 0-100; maps to Frame.TremDepth byte (/100 -> 0.0-1.0)
        public short TremoloRate = 0;    // 0-200; maps to Frame.TremRate byte (/10 -> 0-20 Hz)

        public short Jitter = 0;        // 0-100: random cycle-to-cycle F0 perturbation (0=none, 100=+/-5% period)
        public short Shimmer = 0;       // 0-100: random cycle-to-cycle amplitude perturbation (0=none, 100=+/-20%)
        public short Diplophonia = 0;   // 0-100: alternating strong/weak pulse pattern -> subharmonic at F0/2
        public short FryAmount = 0;     // 0-100: vocal fry - random period extension creating irregular creak
        public short SubglottalAmt = 0; // 0-100: subglottal resonance coupling (~350 Hz chest cavity texture)
        public short BreathAmt = 0;     // 0-100: cycle-synchronous breathiness - open-phase noise via glottal waveform envelope
        public short OpenQuotient = 50; // 0=pressed/bright (short open phase), 50=neutral, 100=breathy/dark (long open phase)
        public short OQStressLink = 0;  // 0-100: effort->pressed (stressed syllables push OQ down - brighter/more harmonic)
        public short OQF0Link = 0;      // 0-100: F0->breathy (higher pitch pushes OQ up - models head voice / falsetto)
        public short LarynxOffset = 0;     // Hz: shifts F1-F6 up/down coherently; >0 raised larynx (brighter), <0 lowered (darker/operatic)
        public short PharyngealAmt = 0;   // 0-100: pharyngeal constriction - F1 up (+1 Hz/unit), F2 dn (-2 Hz/unit)
        public short PitchOffsetHz = 0;   // Hz: shifts F0 up/down for all speech and singing; transposes explicit notes
        public short LipRounding = 0;     // -100=spread (F1 up,F2++,F3 up), 0=neutral, +100=rounded (F1 dn,F2--,F3 dn)
        public short OnsetHardness = 50;  // 0=soft breathy onset (slow ramp), 50=natural, 100=hard glottal attack (instant)

        public short F4Freq = 3650;
        public short F4BW = 200;
        public short F5Freq = 4500;
        public short F5BW = 250;
        public short F4pFreq = 3650;
        public short F4pBW = 150;
        public short F5pFreq = 4200;
        public short F5pBW = 100;
        public short F6pFreq = 4500;
        public short F6pBW = 150;

        public short NasalBase = 330;
        public short NasalTarg = 400;
        public short NasalBW = 60;

        public short Locus = 55;
        public short BwGain1 = 135;
        public short BwGain2 = 110;
        public short BwGain3 = 100;
        public short F1_Offset = 5;
        public short F2_Offset = 15;
        public short F3_Offset = 15;
        public short Chorus = 0;
        public short NGain = 100;

        public short SPitchMidi = 0;
        public short SGain = 0;
        public short AsperW = 2;
        public short VoiceVers = 3;

        public short NasalAmt = 0;
        public short EmphVoice = 1;
        public short RvbDelay = 35;
        public short RvbDepth = 0;

        public short WaveType = 0;
        public short[] VWave = new short[]
        {
        0, 8778, 6555, 3787, 1955, 1231, 865, 724, 563, 500, 444, 420, 382, 339, 292, 286,
        271, 271, 266, 250, 240, 236, 222, 222, 214, 207, 207, 207, 199, 199, 192, 199,
        199, 207, 202, 202, 173, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 108,
        };
        public short[] VWave1 = new short[]
        {
        0, 9212, 6971, 3676, 2192, 1200, 930, 726, 552, 490, 444, 412, 366, 332, 292, 286,
        276, 270, 270, 260, 240, 236, 226, 222, 214, 207, 207, 207, 199, 199, 192, 191,
        184, 188, 184, 130, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 122, 108,
        };

        public short RiseAmt = 29;
        public short FallAmt = -29;
        public short RiseAmt1 = 29;
        public short FallAmt1 = -29;
        public int Assertiveness = 0x10000;
        public short BaselineFall = 51;
        public int Quickness = 7200;
        public int DownRampStep = 15360;
        public short StressDurTime = 50;
        public short VibratoDepth1Raw = 31;
        public short VibratoDepth2Raw = 16;
        public short VibratoFreqRaw = 47;
        public short Intonation = 100;

        public short UptalkAmt = 0;        // 0-100: sentence-final rising tendency (0=natural fall, 100=strong uptalk/rise)
        public short StressEarly = 0;      // -50 to +50: stress peak alignment (-50=early/assertive, 0=natural, +50=late/hesitant)
        public short BreakStrength = 50;   // 0-100: phrase boundary reset strength (0=smooth carry-over, 50=natural, 100=hard reset)
        public short EmphasisBoost = 0;    // 0-100: extra pitch height for emphatic vs primary stress
        public short VocalConfidence = 0;  // 0-100: pronoun emphasis - subject pronouns (I/you/he/she/it/we/they) get a rise-fall pitch accent and vowel lengthening

        public static VoiceData BaselineVoice => new VoiceData();

        public static VoiceData WhisperVoice => new VoiceData {
            PitchHz = 220,
            StressGain = 70,
            Rate = 140,
            VGain = 0,
            AGain = 400,
            ACycle = 16,
            F4Freq = 3500,
            F4BW = 50,
            F5Freq = 4500,
            F5BW = 250,
            F4pFreq = 4500,
            BwGain1 = 100,
            BwGain3 = 50,
            NGain = 200,
            VWave = new short[]
            {
            0, 15476, 6866, 3395, 1831, 1167, 1000, 861, 747, 680, 600, 540, 496, 472, 430, 401,
            367, 354, 339, 309, 307, 290, 273, 262, 211, 189, 165, 156, 144, 137, 113, 107,
            113, 107, 94, 82, 89, 77, 77, 64, 56, 0, 0, 0, 0, 0, 0, 0,
            },
            VWave1 = new short[]
            {
            0, 15476, 6866, 3395, 1831, 1167, 1000, 861, 747, 680, 600, 540, 496, 472, 430, 401,
            367, 354, 339, 309, 307, 290, 273, 262, 211, 189, 165, 156, 144, 137, 113, 107,
            113, 107, 94, 82, 89, 77, 77, 64, 56, 0, 0, 0, 0, 0, 0, 0,
            },
        };
    }

}  // namespace
