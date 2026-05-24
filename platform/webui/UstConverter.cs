using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace SharpTalk.WebUi
{
    public record UstConvertResult(string Klattsch, string Diagnostics);

    internal sealed class UstNote
    {
        public string Lyric { get; init; } = "";
        public int NoteNum { get; init; }
        public int Length { get; init; }
        public int Intensity { get; init; } = 100;
        public double PbsValue { get; init; }
        public List<double> Pby { get; init; } = new();
        public double? Tempo { get; init; }
    }

    public static class UstConverter
    {
        private static readonly (string Kana, string Romaji)[] s_hiragana;
        private static readonly (string Kana, string Romaji)[] s_katakana;
        private static readonly (string Romaji, string[])[] s_romaji;
        private static readonly (string Ipa, string[] Codes)[] s_ipa;
        private static readonly (string Sym, string Ipa)[] s_xsampa;
        private static readonly Dictionary<string, string[]> s_arpa;
        private static readonly Dictionary<string, string[]> s_arpa_jp;
        private static readonly Dictionary<string, string> s_phonToKlattsch;

        private static readonly string[] s_noteNames = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        private static readonly HashSet<string> s_restLyrics = new(StringComparer.Ordinal) { "R", "r", "_" };
        private static readonly HashSet<string> s_extLyrics  = new(StringComparer.Ordinal) { "+", "*", "↑", "↓", "-" };
        private static readonly Regex s_noteSection    = new(@"^#[0-9A-Fa-f]+$", RegexOptions.Compiled);
        private static readonly Regex s_vcvPrefix      = new(@"^([aeiouAEIOU\-n1-9@&Q] ?)(.+)$", RegexOptions.Compiled);
        private static readonly Regex s_trimDecorators = new(@"^[-+*↑↓\[\]{}]+|[-+*↑↓\[\]{}]+$", RegexOptions.Compiled);
        private static readonly Regex s_stressMarkers  = new(@"[ˈˌ]", RegexOptions.Compiled);

        private static readonly Dictionary<string, string[]> s_vccVowels = new(StringComparer.Ordinal) {
            {"ey", new[]{"ey"}}, {"ay", new[]{"ay"}}, {"oy", new[]{"oy"}}, {"aw", new[]{"aw"}}, {"ow", new[]{"ow"}},
            {"Er", new[]{"er"}}, {"Ar", new[]{"aa","r"}}, {"Or", new[]{"ao","r"}}, {"Ir", new[]{"iy","r"}}, {"Ur", new[]{"uw","r"}},
            {"aI", new[]{"ay"}}, {"aU", new[]{"aw"}}, {"aO", new[]{"aw"}}, {"eI", new[]{"ey"}}, {"oI", new[]{"oy"}}, {"oU", new[]{"ow"}},
            {"A", new[]{"ey"}}, {"E", new[]{"iy"}}, {"I", new[]{"ay"}}, {"O", new[]{"ow"}}, {"U", new[]{"uw"}},
            {"a", new[]{"ae"}}, {"e", new[]{"eh"}}, {"i", new[]{"ih"}}, {"o", new[]{"uw"}}, {"u", new[]{"ah"}},
            {"@", new[]{"ax"}}, {"1", new[]{"ih"}}, {"2", new[]{"aw"}}, {"3", new[]{"er"}}, {"4", new[]{"oy"}},
            {"&", new[]{"ae"}}, {"Q", new[]{"ao"}}, {"7", new[]{"ah"}}, {"9", new[]{"oy"}}, {"0", new[]{"aw"}},
            {"8", new[]{"uh"}}, {"6", new[]{"er"}}, {"5", new[]{"el"}},
        };

        private static readonly Dictionary<string, string[]> s_vccConsonants = new(StringComparer.Ordinal) {
            {"ch", new[]{"ch"}}, {"sh", new[]{"sh"}}, {"th", new[]{"th"}}, {"dh", new[]{"dh"}}, 
            {"zh", new[]{"zh"}}, {"ng", new[]{"ng"}}, {"ph", new[]{"f"}},  {"kh", new[]{"k"}},  
            {"gh", new[]{"g"}},  {"qu", new[]{"k","w"}}, {"ts", new[]{"t","s"}}, {"dz", new[]{"z"}},
            {"w", new[]{"w"}}, {"y", new[]{"y"}}, {"r", new[]{"r"}}, {"l", new[]{"l"}},
            {"m", new[]{"m"}}, {"n", new[]{"n"}}, {"f", new[]{"f"}}, {"v", new[]{"v"}},
            {"s", new[]{"s"}}, {"z", new[]{"z"}}, {"p", new[]{"p"}}, {"b", new[]{"b"}},
            {"t", new[]{"t"}}, {"d", new[]{"d"}}, {"k", new[]{"k"}}, {"g", new[]{"g"}},
            {"h", new[]{"hh"}}, {"j", new[]{"jh"}},
        };

        private static readonly (string Key, string[] Value)[] s_vccVowelsSorted;
        private static readonly (string Key, string[] Value)[] s_vccConsonantsSorted;

        private static readonly string[] s_ipaIndicators = {
            "ə","ɛ","ɪ","ɔ","ʃ","ʒ","ð","θ","ŋ","ɑ","æ","ʌ","ɜ","ɯ","ɾ","ɸ","ç","ɕ","ʑ","ɹ"
        };

        private const double PitchCentsThreshold = 20.0;

        private static readonly (string Label, string[] Members)[] s_phonCategories = {
            ("JP vowels",  new[] { "jp_aa","jp_iy","jp_uw","jp_eh","jp_ow" }),
            ("EN vowels",  new[] { "iy","ih","eh","ae","aa","ao","ah","uh","uw","er","ey","ay","oy","aw","ow","ax","ix" }),
            ("Stops",      new[] { "p","b","t","d","k","g","dx","tx" }),
            ("Fricatives", new[] { "f","v","th","dh","s","z","sh","zh","hh" }),
            ("Affricates", new[] { "ch","jh" }),
            ("Nasals",     new[] { "m","n","ng" }),
            ("Sonorants",  new[] { "w","y","r","l","yu","rx","lx" }),
            ("Syllabic",   new[] { "el","en" }),
        };

        static UstConverter()
        {
            s_vccVowelsSorted = s_vccVowels.Select(x => (x.Key, x.Value)).OrderByDescending(x => x.Key.Length).ToArray();
            s_vccConsonantsSorted = s_vccConsonants.Select(x => (x.Key, x.Value)).OrderByDescending(x => x.Key.Length).ToArray();

            var hiraganaRaw = new (string, string)[] {
                ("いぇ","ye"),("うぃ","wi"),("うぇ","we"),("うぉ","wo"),
                ("きゃ","kya"),("きゅ","kyu"),("きょ","kyo"),("ぎゃ","gya"),("ぎゅ","gyu"),("ぎょ","gyo"),
                ("しぇ","she"),("しゃ","sha"),("しゅ","shu"),("しょ","sho"),
                ("じぇ","je"),("じゃ","ja"),("じゅ","ju"),("じょ","jo"),
                ("ちぇ","che"),("ちゃ","cha"),("ちゅ","chu"),("ちょ","cho"),
                ("つぁ","tsa"),("つぃ","tsi"),("つぇ","tse"),("つぉ","tso"),
                ("てぃ","thi"),("でぃ","dhi"),("とゅ","thu"),("どゅ","dhu"),
                ("にゃ","nya"),("にゅ","nyu"),("にょ","nyo"),
                ("ひゃ","hya"),("ひゅ","hyu"),("ひょ","hyo"),
                ("びゃ","bya"),("びゅ","byu"),("びょ","byo"),
                ("ぴゃ","pya"),("ぴゅ","pyu"),("ぴょ","pyo"),
                ("ふぁ","fa"),("ふぃ","fi"),("ふぇ","fe"),("ふぉ","fo"),
                ("みゃ","mya"),("みゅ","myu"),("みょ","myo"),
                ("りゃ","rya"),("りゅ","ryu"),("りょ","ryo"),
                ("ゔぁ","va"),("ゔぃ","vi"),("ゔぇ","ve"),("ゔぉ","vo"),
                ("ぁ","a"),("あ","a"),("ぃ","i"),("い","i"),("ぅ","u"),("う","u"),
                ("ぇ","e"),("え","e"),("ぉ","o"),("お","o"),
                ("か","ka"),("が","ga"),("き","ki"),("ぎ","gi"),
                ("く","ku"),("ぐ","gu"),("け","ke"),("げ","ge"),("こ","ko"),("ご","go"),
                ("さ","sa"),("ざ","za"),("し","shi"),("じ","ji"),
                ("す","su"),("ず","zu"),("せ","se"),("ぜ","ze"),("そ","so"),("ぞ","zo"),
                ("た","た"),("だ","da"),("ち","chi"),("ぢ","di"),
                ("っ","q"),("つ","tsu"),("づ","du"),("て","te"),("で","de"),
                ("と","to"),("ど","do"),
                ("な","na"),("に","ni"),("ぬ","nu"),("ね","ne"),("の","no"),
                ("は","ha"),("ば","ba"),("ぱ","pa"),("ひ","hi"),("び","bi"),("ぴ","pi"),
                ("ふ","fu"),("ぶ","bu"),("ぷ","pu"),
                ("へ","he"),("べ","be"),("ぺ","pe"),("ほ","ho"),("ぼ","bo"),("ぽ","po"),
                ("ま","ma"),("み","mi"),("む","mu"),("め","me"),("も","mo"),
                ("ゃ","ya"),("や","ya"),("ゅ","yu"),("ゆ","yu"),("ょ","yo"),("よ","yo"),
                ("ら","ra"),("り","ri"),("る","ru"),("れ","re"),("ろ","ro"),
                ("わ","wa"),("ゐ","wi"),("ゑ","we"),("を","wo"),("ん","n"),("ゔ","vu"),
            };
            s_hiragana = hiraganaRaw.OrderByDescending(x => x.Item1.Length).ToArray();

            var katakanaRaw = new (string, string)[] {
                ("イェ","ye"),("ウィ","wi"),("ウェ","we"),("ウォ","wo"),
                ("キャ","kya"),("キュ","kyu"),("キョ","kyo"),("ギャ","gya"),("ギュ","gyu"),("ギョ","gyo"),
                ("シェ","she"),("シャ","sha"),("シュ","shu"),("ショ","sho"),
                ("ジェ","je"),("じゃ","ja"),("じゅ","ju"),("じょ","jo"),
                ("チェ","che"),("チャ","cha"),("チュ","chu"),("チョ","cho"),
                ("ツぁ","tsa"),("ツィ","tsi"),("ツェ","tse"),("ツぉ","tso"),
                ("ティ","thi"),("ディ","dhi"),("トゥ","thu"),("ドゥ","dhu"),
                ("ニャ","nya"),("ニュ","nyu"),("ニョ","nyo"),
                ("ヒャ","hya"),("ヒュ","hyu"),("ヒョ","hyo"),
                ("ビャ","bya"),("びゅ","byu"),("びょ","byo"),
                ("ピャ","pya"),("ぴゅ","pyu"),("ぴょ","pyo"),
                ("ファ","fa"),("フィ","fi"),("フェ","fe"),("フォ","fo"),
                ("みゃ","mya"),("みゅ","myu"),("みょ","myo"),
                ("りゃ","rya"),("りゅ","ryu"),("りょ","ryo"),
                ("ヴァ","va"),("ヴィ","vi"),("ヴェ","ve"),("ヴォ","vo"),
                ("ァ","a"),("ア","a"),("ィ","i"),("イ","i"),("ゥ","u"),("ウ","u"),
                ("ぇ","e"),("エ","e"),("ぉ","o"),("オ","o"),
                ("カ","ka"),("ガ","ga"),("キ","ki"),("ギ","gi"),
                ("ク","ku"),("グ","gu"),("ケ","ke"),("ゲ","ge"),("コ","ko"),("ゴ","go"),
                ("サ","sa"),("ザ","za"),("シ","shi"),("ジ","ji"),
                ("ス","su"),("ズ","zu"),("セ","se"),("ぜ","ze"),("そ","so"),("ぞ","zo"),
                ("タ","ta"),("だ","da"),("チ","chi"),("ヂ","di"),
                ("っ","q"),("ツ","tsu"),("づ","du"),("て","te"),("で","de"),
                ("と","to"),("ど","do"),
                ("な","na"),("に","ni"),("ぬ","nu"),("ね","ne"),("の","no"),
                ("は","ha"),("ば","ba"),("ぱ","pa"),("ひ","hi"),("び","bi"),("ぴ","pi"),
                ("ふ","fu"),("ぶ","bu"),("ぷ","pu"),
                ("へ","he"),("べ","be"),("ぺ","pe"),("ほ","ho"),("ぼ","bo"),("ぽ","po"),
                ("ま","ma"),("み","mi"),("む","mu"),("め","me"),("も","mo"),
                ("ゃ","ya"),("ヤ","ya"),("ゅ","yu"),("ユ","yu"),("ょ","yo"),("ヨ","yo"),
                ("ら","ra"),("り","ri"),("る","ru"),("れ","re"),("ろ","ro"),
                ("わ","wa"),("ゐ","wi"),("ゑ","we"),("を","wo"),("ん","n"),("ヴ","vu"),
            };
            s_katakana = katakanaRaw.OrderByDescending(x => x.Item1.Length).ToArray();

            // Helper for building JP phoneme arrays: consonants followed by vowel code
            static string[] Cv(string vowelCode, params string[] cons) {
                var r = new string[cons.Length + 1];
                for (int k = 0; k < cons.Length; k++) r[k] = cons[k];
                r[cons.Length] = vowelCode;
                return r;
            }
            const string JA = "jp_aa", JI = "jp_iy", JU = "jp_uw", JE = "jp_eh", JO = "jp_ow";

            var romajiRaw = new (string, string[])[] {
                ("a",Cv(JA)),("i",Cv(JI)),("u",Cv(JU)),("e",Cv(JE)),("o",Cv(JO)),
                ("ka",Cv(JA,"k")),("ki",Cv(JI,"k")),("ku",Cv(JU,"k")),("ke",Cv(JE,"k")),("ko",Cv(JO,"k")),
                ("kya",Cv(JA,"k","y")),("kyu",Cv(JU,"k","y")),("kyo",Cv(JO,"k","y")),
                ("ga",Cv(JA,"g")),("gi",Cv(JI,"g")),("gu",Cv(JU,"g")),("ge",Cv(JE,"g")),("go",Cv(JO,"g")),
                ("gya",Cv(JA,"g","y")),("gyu",Cv(JU,"g","y")),("gyo",Cv(JO,"g","y")),
                ("sa",Cv(JA,"s")),("si",Cv(JI,"sh")),("su",Cv(JU,"s")),("se",Cv(JE,"s")),("so",Cv(JO,"s")),
                ("sha",Cv(JA,"sh")),("shi",Cv(JI,"sh")),("shu",Cv(JU,"sh")),("she",Cv(JE,"sh")),("sho",Cv(JO,"sh")),
                ("sya",Cv(JA,"sh")),("syu",Cv(JU,"sh")),("syo",Cv(JO,"sh")),
                ("za",Cv(JA,"z")),("zi",Cv(JI,"zh")),("zu",Cv(JU,"z")),("ze",Cv(JE,"z")),("zo",Cv(JO,"z")),
                ("ja",Cv(JA,"jh")),("ji",Cv(JI,"jh")),("ju",Cv(JU,"jh")),("je",Cv(JE,"jh")),("jo",Cv(JO,"jh")),
                ("jya",Cv(JA,"jh")),("jyu",Cv(JU,"jh","y")),("jyo",Cv(JO,"jh","y")),
                ("ta",Cv(JA,"t")),("ti",Cv(JI,"ch")),("tu",Cv(JU,"t","s")),("te",Cv(JE,"t")),("to",Cv(JO,"t")),
                ("cha",Cv(JA,"ch")),("chi",Cv(JI,"ch")),("chu",Cv(JU,"ch")),("che",Cv(JE,"ch")),("cho",Cv(JO,"ch")),
                ("tya",Cv(JA,"ch","y")),("tyu",Cv(JU,"ch","y")),("tyo",Cv(JO,"ch","y")),
                ("tsa",Cv(JA,"t","s")),("tsi",Cv(JI,"t","s")),("tsu",Cv(JA,"t","s")),("tse",Cv(JE,"t","s")),("tso",Cv(JO,"t","s")),
                ("thi",Cv(JI,"t")),("thu",Cv(JU,"t")),
                ("da",Cv(JA,"d")),("di",Cv(JI,"jh")),("du",Cv(JU,"d","z")),("de",Cv(JE,"d")),("do",Cv(JO,"d")),
                ("dhi",Cv(JI,"d")),("dhu",Cv(JU,"d")),
                ("na",Cv(JA,"n")),("ni",Cv(JI,"n")),("nu",Cv(JU,"n")),("ne",Cv(JE,"n")),("no",Cv(JO,"n")),
                ("nya",Cv(JA,"n","y")),("nyu",Cv(JU,"n","y")),("nyo",Cv(JO,"n","y")),
                ("ha",Cv(JA,"hh")),("hi",Cv(JI,"hh")),("fu",Cv(JU,"f")),("he",Cv(JE,"hh")),("ho",Cv(JO,"hh")),
                ("hya",Cv(JA,"hh","y")),("hyu",Cv(JU,"hh","y")),("hyo",Cv(JO,"hyo")),
                ("fa",Cv(JA,"f")),("fi",Cv(JI,"f")),("fe",Cv(JE,"f")),("fo",Cv(JO,"f")),
                ("ba",Cv(JA,"b")),("bi",Cv(JI,"b")),("bu",Cv(JU,"b")),("be",Cv(JE,"b")),("bo",Cv(JO,"b")),
                ("bya",Cv(JA,"b","y")),("byu",Cv(JU,"b","y")),("byo",Cv(JO,"b","y")),
                ("pa",Cv(JA,"p")),("pi",Cv(JI,"p")),("pu",Cv(JU,"p")),("pe",Cv(JE,"p")),("po",Cv(JO,"p")),
                ("pya",Cv(JA,"p","y")),("pyu",Cv(JU,"p","y")),("pyo",Cv(JO,"p","y")),
                ("ma",Cv(JA,"m")),("mi",Cv(JI,"m")),("mu",Cv(JU,"m")),("me",Cv(JE,"m")),("mo",Cv(JO,"m")),
                ("mya",Cv(JA,"m","y")),("myu",Cv(JU,"m","y")),("myo",Cv(JO,"m","y")),
                ("ya",Cv(JA,"y")),("yu",Cv(JU,"y")),("yo",Cv(JO,"y")),("ye",Cv(JE,"y")),
                ("ra",Cv(JA,"dx")),("ri",Cv(JI,"dx")),("ru",Cv(JU,"dx")),("re",Cv(JE,"dx")),("ro",Cv(JO,"dx")),
                ("rya",Cv(JA,"dx","y")),("ryu",Cv(JU,"dx","y")),("ryo",Cv(JO,"dx","y")),
                ("wa",Cv(JA,"w")),("wi",Cv(JI,"w")),("we",Cv(JE,"w")),("wo",Cv(JO,"w")),
                ("va",Cv(JA,"v")),("vi",Cv(JI,"v")),("vu",Cv(JU,"v")),("ve",Cv(JE,"v")),("vo",Cv(JO,"v")),
                ("n",  new[] { "n" }),
                ("q",  Array.Empty<string>()),
            };
            s_romaji = romajiRaw.OrderByDescending(x => x.Item1.Length).ToArray();

            var xsampaRaw = new (string, string)[] {
                ("d\\`","ɖ"), ("dZ","dʒ"), ("dz\\","dʑ"), ("g\\","ɢ"),
                ("h\\","ɦ"), ("j\\","ʝ"), ("l\\`","ɭ"), ("l\\","ɭ"), ("n\\`","ɳ"),
                ("p\\","ɸ"), ("r\\`","ɻ"), ("r\\","ɹ"), ("s\\`","ʂ"), ("s\\","ɕ"),
                ("tS","tʃ"), ("t\\`","ʈ"), ("ts\\","tɕ"),
                ("x\\","ɧ"), ("z\\`","ʐ"), ("z\\","ʑ"),
                ("B\\","ʙ"), ("G\\","ɢ"), ("H\\","ʜ"), ("L\\","ʟ"), ("O\\","ʘ"),
                ("|\\|\\","ǁ"), ("!\\","ǃ"), ("=\\","ǂ"), ("|\\","ǀ"),
                ("&","æ"), ("'","ˈ"), (",","ˌ"), ("0","ɒ"), ("1","ɨ"), ("2","ø"), ("3","ɜ"),
                ("4","ɾ"), ("5","ɫ"), ("6","ɐ"), ("7","ɤ"), ("8","ɵ"), ("9","œ"), (":","ː"),
                ("=","̩"), ("?","ʔ"), ("@","ə"), ("A","ɑ"), ("B","β"), ("C","ç"), ("D","ð"),
                ("E","ɛ"), ("F","ɱ"), ("G","ɣ"), ("H","ɥ"), ("I","ɪ"), ("J","ɲ"), ("K","ɬ"),
                ("L","ʎ"), ("M","ɯ"), ("N","ŋ"), ("O","ɔ"), ("P","ʋ"), ("Q","ɒ"), ("R","ʁ"),
                ("S","ʃ"), ("T","θ"), ("U","ʊ"), ("V","ʌ"), ("W","ʍ"), ("X","χ"), ("Y","ʏ"),
                ("Z","ʒ"), ("^","̯"), ("_","_"),
                ("i\\","ɨ"), ("u\\","ʉ"), ("e\\","ɘ"), ("o\\","ɵ"), ("E\\","ɝ"), ("a\\","ɐ"),
                ("A\\","ɑ"), ("M\\","ɯ"), ("U\\","ʊ"), ("r\\`","ɻ"), ("R\\","ʀ"), ("v\\","ʋ"),
                ("p\\","ɸ"), ("k\\","χ"), ("K\\","ɬ"),
            };
            s_xsampa = xsampaRaw.OrderByDescending(x => x.Item1.Length).ToArray();

            var ipaRaw = new (string, string[])[] {
                ("aɪ",new[]{"ay"}),("aʊ",new[]{"aw"}),("eɪ",new[]{"ey"}),("oʊ",new[]{"ow"}),("əʊ",new[]{"ow"}),
                ("ɔɪ",new[]{"oy"}),("iː",new[]{"iy"}),("uː",new[]{"uw"}),("aː",new[]{"aa"}),("ɑː",new[]{"aa"}),
                ("ɜː",new[]{"er"}),("ɔː",new[]{"ao"}),("eː",new[]{"eh"}),("oː",new[]{"ow"}),
                ("ɪə",new[]{"ih","r"}),("eə",new[]{"eh","r"}),("ʊə",new[]{"uh","r"}),("ɔə",new[]{"ao","r"}),
                ("t͡ʃ",new[]{"ch"}),("d͡ʒ",new[]{"jh"}),("t͡s",new[]{"t","s"}),("d͡z",new[]{"z"}),
                ("d͡ʑ",new[]{"jh"}),("t͡ɕ",new[]{"ch"}),
                ("tʃ",new[]{"ch"}),("dʒ",new[]{"jh"}),("ts",new[]{"t","s"}),("dz",new[]{"z"}),
                ("dʑ",new[]{"jh"}),("tɕ",new[]{"ch"}),("ɖʐ",new[]{"jh"}),("ʈʂ",new[]{"ch"}),
                ("l̩",new[]{"el"}),("m̩",new[]{"m"}),("n̩",new[]{"en"}),
                ("ŋ",new[]{"ng"}),("ɴ",new[]{"ng"}),
                ("i",new[]{"iy"}),("ɪ",new[]{"ih"}),("e",new[]{"eh"}),("ɛ",new[]{"eh"}),("æ",new[]{"ae"}),
                ("a",new[]{"aa"}),("ɑ",new[]{"aa"}),("ɒ",new[]{"aa"}),
                ("ɔ",new[]{"ao"}),("ʌ",new[]{"ah"}),("ɐ",new[]{"ah"}),
                ("ʊ",new[]{"uh"}),("u",new[]{"uw"}),
                ("ə",new[]{"ax"}),("ɘ",new[]{"ax"}),("ɵ",new[]{"ax"}),
                ("ɜ",new[]{"er"}),("ɝ",new[]{"er"}),("ɚ",new[]{"er"}),
                ("ɨ",new[]{"ix"}),("ɯ",new[]{"uw"}),
                ("ø",new[]{"er"}),("œ",new[]{"er"}),("y",new[]{"iy"}),
                ("w",new[]{"w"}),("ɥ",new[]{"w"}),("ʍ",new[]{"w"}),
                ("j",new[]{"y"}),("ʝ",new[]{"y"}),
                ("ɹ",new[]{"r"}),("r",new[]{"r"}),("ɻ",new[]{"r"}),("ʀ",new[]{"r"}),("ʁ",new[]{"r"}),
                ("ɾ",new[]{"dx"}),("ɽ",new[]{"dx"}),
                ("l",new[]{"l"}),("ɫ",new[]{"l"}),("ʎ",new[]{"l"}),("ɭ",new[]{"l"}),("ɺ",new[]{"l"}),
                ("m",new[]{"m"}),("ɱ",new[]{"m"}),
                ("n",new[]{"n"}),("ɳ",new[]{"n"}),("ɲ",new[]{"n"}),
                ("h",new[]{"hh"}),("ɦ",new[]{"hh"}),("ɸ",new[]{"f"}),("ħ",new[]{"hh"}),
                ("f",new[]{"f"}),("v",new[]{"v"}),("ʋ",new[]{"v"}),
                ("θ",new[]{"th"}),("ð",new[]{"dh"}),
                ("s",new[]{"s"}),("z",new[]{"z"}),
                ("ʂ",new[]{"sh"}),("ʃ",new[]{"sh"}),("ɕ",new[]{"sh"}),
                ("ʐ",new[]{"zh"}),("ʒ",new[]{"zh"}),("ʑ",new[]{"zh"}),
                ("ç",new[]{"sh"}),("χ",new[]{"k"}),("ɣ",new[]{"g"}),
                ("β",new[]{"b"}),("x",new[]{"k"}),
                ("p",new[]{"p"}),("b",new[]{"b"}),("ɓ",new[]{"b"}),("ʙ",new[]{"b"}),
                ("t",new[]{"t"}),("d",new[]{"d"}),("ɗ",new[]{"d"}),("ɖ",new[]{"d"}),
                ("c",new[]{"t"}),("ɟ",new[]{"d"}),
                ("k",new[]{"k"}),("g",new[]{"g"}),("ɡ",new[]{"g"}),("ɠ",new[]{"g"}),("ɢ",new[]{"g"}),
                ("q",new[]{"k"}),
                ("_",new[]{"_"}),
                ("ʔ",Array.Empty<string>()),
                ("ˈ",Array.Empty<string>()),("ˌ",Array.Empty<string>()),("ː",Array.Empty<string>()),
                ("ˑ",Array.Empty<string>()),("̃",Array.Empty<string>()),("̩",Array.Empty<string>()),
                ("̯",Array.Empty<string>()),("ʰ",Array.Empty<string>()),("ʲ",Array.Empty<string>()),
                ("ʷ",Array.Empty<string>()),("ˠ",Array.Empty<string>()),("ˤ",Array.Empty<string>()),
            };
            s_ipa = ipaRaw.OrderByDescending(x => x.Item1.Length).ToArray();

            var arpaRaw = new (string, string[])[] {
                ("AA", new[]{"aa"}), ("AE", new[]{"ae"}), ("AH", new[]{"ah"}), ("AO", new[]{"ao"}),
                ("AW", new[]{"aw"}), ("AY", new[]{"ay"}), ("EH", new[]{"eh"}), ("ER", new[]{"er"}),
                ("EY", new[]{"ey"}), ("IH", new[]{"ih"}), ("IY", new[]{"iy"}), ("OW", new[]{"ow"}),
                ("OY", new[]{"oy"}), ("UH", new[]{"uh"}), ("UW", new[]{"uw"}),
                ("B",  new[]{"b"}),  ("CH", new[]{"ch"}), ("D",  new[]{"d"}),  ("DH", new[]{"dh"}),
                ("F",  new[]{"f"}),  ("G",  new[]{"g"}),  ("HH", new[]{"hh"}), ("JH", new[]{"jh"}),
                ("K",  new[]{"k"}),  ("L",  new[]{"l"}),  ("M",  new[]{"m"}),  ("N",  new[]{"n"}),
                ("NG", new[]{"ng"}), ("P",  new[]{"p"}),  ("R",  new[]{"r"}),  ("S",  new[]{"s"}),
                ("SH", new[]{"sh"}), ("T",  new[]{"t"}),  ("TH", new[]{"th"}), ("V",  new[]{"v"}),
                ("W",  new[]{"w"}),  ("Y",  new[]{"y"}),  ("Z",  new[]{"z"}),  ("ZH", new[]{"zh"}),
                ("AX", new[]{"ax"}), ("IX", new[]{"ix"}), ("DX", new[]{"dx"}),
                ("A",  new[]{"aa"}), ("I",  new[]{"ay"}), ("U",  new[]{"uw"}), ("E",  new[]{"eh"}), ("O",  new[]{"ow"}),

                ("Q",  new[]{"_"}),  ("SIL", Array.Empty<string>()), ("SP", Array.Empty<string>()),
            };
            s_arpa = arpaRaw.ToDictionary(x => x.Item1, x => x.Item2, StringComparer.OrdinalIgnoreCase);

            var arpaJpRaw = new (string, string[])[] {
                ("AA", new[]{"jp_aa"}), ("AE", new[]{"jp_aa"}), ("AH", new[]{"jp_aa"}), ("AO", new[]{"jp_ow"}),
                ("AW", new[]{"jp_aa","jp_uw"}), ("AY", new[]{"jp_aa","jp_iy"}), ("EH", new[]{"jp_eh"}), ("ER", new[]{"jp_eh"}),
                ("EY", new[]{"jp_eh","jp_iy"}), ("IH", new[]{"jp_iy"}), ("IY", new[]{"jp_iy"}), ("OW", new[]{"jp_ow"}),
                ("OY", new[]{"jp_ow","jp_iy"}), ("UH", new[]{"jp_uw"}), ("UW", new[]{"jp_uw"}),
                ("B",  new[]{"b"}),  ("CH", new[]{"ch"}), ("D",  new[]{"d"}),  ("DH", new[]{"z"}),
                ("F",  new[]{"f"}),  ("G",  new[]{"g"}),  ("HH", new[]{"hh"}), ("JH", new[]{"jh"}),
                ("K",  new[]{"k"}),  ("L",  new[]{"dx"}), ("M",  new[]{"m"}),  ("N",  new[]{"n"}),
                ("NG", new[]{"n"}),  ("P",  new[]{"p"}),  ("R",  new[]{"dx"}), ("S",  new[]{"s"}),
                ("SH", new[]{"sh"}), ("T",  new[]{"t"}),  ("TH", new[]{"s"}),  ("V",  new[]{"v"}),
                ("W",  new[]{"w"}),  ("Y",  new[]{"y"}),  ("Z",  new[]{"z"}),  ("ZH", new[]{"sh"}),
                ("AX", new[]{"jp_aa"}), ("IX", new[]{"jp_iy"}), ("DX", new[]{"dx"}),
                ("A",  new[]{"aa"}), ("I",  new[]{"ay"}), ("U",  new[]{"jp_uw"}), ("E",  new[]{"eh"}), ("O",  new[]{"jp_ow"}),
                ("Q",  new[]{"_"}),  ("SIL", Array.Empty<string>()), ("SP", Array.Empty<string>()),
            };
            s_arpa_jp = arpaJpRaw.ToDictionary(x => x.Item1, x => x.Item2, StringComparer.OrdinalIgnoreCase);

            s_phonToKlattsch = new Dictionary<string, string>(StringComparer.Ordinal) {
                {"jp_aa","A"},{"jp_iy","I"},{"jp_uw","U"},{"jp_eh","E"},{"jp_ow","O"},
                {"iy","IY"},{"ih","IH"},{"eh","EH"},{"ae","AE"},
                {"aa","AA"},{"ah","AH"},{"ao","AO"},{"uh","UH"},
                {"ax","AX"},{"er","ER"},{"ey","EY"},{"ay","AY"},
                {"oy","OY"},{"aw","AW"},{"ow","OW"},{"uw","UW"},{"ix","IX"},
                {"w","W"},{"y","Y"},{"r","R"},{"l","L"},
                {"m","M"},{"n","N"},{"ng","NG"},
                {"hh","HH"},{"f","F"},{"v","V"},{"th","TH"},{"dh","DH"},
                {"s","S"},{"z","Z"},{"sh","SH"},{"zh","ZH"},
                {"p","P"},{"b","B"},{"t","T"},{"d","D"},{"dx","DX"},{"k","K"},{"g","G"},
                {"ch","CH"},{"jh","JH"},{"_","_"},
                {"yu","YU"},{"rx","RX"},{"lx","LX"},{"el","EL"},{"en","EN"},{"tx","TX"},
            };
        }

        private static readonly Dictionary<string, (string Nucleus, string Terminal)> s_diphthongs = new(StringComparer.OrdinalIgnoreCase) {
            { "AY", ("AA", "IY") },
            { "AW", ("AA", "UW") },
            { "EY", ("EH", "IY") },
            { "OW", ("OW", "UW") },
            { "OY", ("AO", "IY") }
        };

        public static UstConvertResult Convert(
            string ustText,
            string language = "auto",
            int noteOffset = 0,
            string? compatBank = null,
            Func<string, string[]?>? englishPhonemizer = null)
        {
            bool compatAuto = string.Equals(compatBank, "auto", StringComparison.Ordinal);

            double tempo = 120.0;
            var unknownLyrics = new HashSet<string>(StringComparer.Ordinal);
            var notes = ParseUst(ustText, ref tempo, out string notationType, language);

            if (string.IsNullOrEmpty(notationType) || notationType == "auto" || notationType == "auto-detect")
                notationType = DetectNotation(notes.Select(n => n.Lyric).ToList());

            if (compatAuto)
                compatBank = (notationType == "japanese" || notationType == "arpa_jp") ? "ja-mokhtari-2000" : null;

            var parts = new List<string>();
            var phonemeCounts = new Dictionary<string, int>(StringComparer.Ordinal);
            string? curNote = null;
            int curRate = -1;
            string? lastPhoneme = null;
            int i = 0;

            while (i < notes.Count)
            {
                var note = notes[i];
                if (note.Tempo.HasValue)
                    tempo = note.Tempo.Value;
                int durMs = TicksToMs(note.Length, tempo);
                string noteName = MidiToNoteName(note.NoteNum + noteOffset);
                double baseHz = MidiToHz(note.NoteNum + noteOffset);

                int j = i + 1;
                var mergedPby = new List<double>(note.Pby);
                while (j < notes.Count && IsExtension(notes[j].Lyric))
                {
                    durMs += TicksToMs(notes[j].Length, tempo);
                    if (notes[j].Pby.Count > 0)
                        mergedPby = new List<double>(notes[j].Pby);
                    j++;
                }

                if (IsRest(note.Lyric))
                {
                    if (durMs > 0)
                        parts.Add($"p={durMs}");
                    lastPhoneme = null;
                }
                else
                {
                    var phonemes = LyricToPhonemes(note.Lyric, notationType, unknownLyrics, englishPhonemizer);
                    if (phonemes is null)
                    {
                        lastPhoneme = null;
                    }
                    else if (phonemes.Length == 0)
                    {
                        if (durMs > 0) parts.Add($"p={durMs}");
                        lastPhoneme = null;
                    }
                    else
                    {
                        foreach (var p in phonemes)
                            phonemeCounts[p] = phonemeCounts.GetValueOrDefault(p) + 1;

                        var mapped = phonemes.Select(p => MapPhoneme(p, compatBank)).ToList();

                        // Nucleus-Sustain slurring: If this note is followed by the same diphthong,
                        // sustain the nucleus here and glide on the last note of the chain.
                        if (mapped.Count > 0)
                        {
                            string lastP = mapped[^1];
                            if (s_diphthongs.TryGetValue(lastP, out var d))
                            {
                                // Look ahead
                                string? nextLyric = (j < notes.Count) ? notes[j].Lyric : null;
                                if (nextLyric != null && !IsRest(nextLyric) && !IsExtension(nextLyric))
                                {
                                    var nextPhonemes = LyricToPhonemes(nextLyric, notationType, new HashSet<string>(), englishPhonemizer);
                                    if (nextPhonemes != null && nextPhonemes.Length > 0)
                                    {
                                        string nextFirst = MapPhoneme(nextPhonemes[0], compatBank);
                                        if (nextFirst == lastP || nextFirst == d.Terminal)
                                            mapped[^1] = d.Nucleus;
                                    }
                                }
                            }
                        }

                        var klattsch = new List<string>();
                        for (int k = 0; k < mapped.Count; k++)
                        {
                            string p = mapped[k];
                            string pLower = p.ToLowerInvariant();

                            bool pIsVowel = s_phonCategories[1].Members.Contains(pLower) || 
                                            s_phonCategories[0].Members.Contains(pLower);

                            bool lastIsVowel = false;
                            if (lastPhoneme != null)
                            {
                                string lLower = lastPhoneme.ToLowerInvariant();
                                lastIsVowel = s_phonCategories[1].Members.Contains(lLower) ||
                                              s_phonCategories[0].Members.Contains(lLower);
                            }

                            if (k == 0 && lastPhoneme != null &&
                                s_diphthongs.TryGetValue(p, out var dslur) && dslur.Nucleus == lastPhoneme)
                            {
                                // Diphthong continuation: previous note held the nucleus, this note gets the glide
                                p = dslur.Terminal;
                            }

                            string finalP = p;
                            klattsch.Add(finalP);
                            lastPhoneme = finalP;
                        }

                        if (noteName != curNote) { parts.Add($"b={noteName}"); curNote = noteName; }
                        if (durMs != curRate)    { parts.Add($"r={durMs}");    curRate = durMs; }

                        if (Math.Abs(note.PbsValue) >= PitchCentsThreshold)
                        {
                            string mod = PitchModifier(baseHz, note.PbsValue, transient: true);
                            if (mod.Length > 0) klattsch[0] += mod;
                        }

                        double endCents = mergedPby.Count > 0 ? mergedPby[^1] : 0.0;
                        if (Math.Abs(endCents) >= PitchCentsThreshold)
                        {
                            string mod = PitchModifier(baseHz, endCents, transient: false);
                            if (mod.Length > 0) klattsch[^1] += mod;
                        }

                        parts.Add("( " + string.Join(" ", klattsch) + " )");
                    }
                }
                i = j;
            }

            string prefix = compatBank is not null ? $"[bank={compatBank}] " : "";
            string klattschOutput = prefix + string.Join(" ", parts);
            string diagnostics = BuildDiagnostics(notationType, tempo, phonemeCounts, unknownLyrics);
            return new UstConvertResult(klattschOutput, diagnostics);
        }

        public static UstConvertResult ConvertFromBytes(
            byte[] data,
            string language = "auto",
            int noteOffset = 0,
            string? compatBank = null,
            Func<string, string[]?>? englishPhonemizer = null)
        {
            string text = DecodeUst(data);
            return Convert(text, language, noteOffset, compatBank, englishPhonemizer);
        }

        private static string DecodeUst(byte[] data)
        {
            // shift-jis and windows-31j need CodePagesEncodingProvider
            // Prioritize shift-jis as it is the standard for UST files
            var encodings = new[] { "shift-jis", "windows-31j", "utf-8" };
            foreach (var name in encodings)
            {
                try
                {
                    var enc = Encoding.GetEncoding(name, EncoderFallback.ExceptionFallback, DecoderFallback.ExceptionFallback);
                    return enc.GetString(data);
                }
                catch (DecoderFallbackException) { }
                catch (ArgumentException) { }
                catch (NotSupportedException) { }
            }
            return Encoding.Latin1.GetString(data);
        }

        private static List<UstNote> ParseUst(string text, ref double tempo, out string notationType, string language)
        {
            var notes = new List<UstNote>();
            var current = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            string? section = null;
            var allLyrics = new List<string>();

            // Normalize line endings
            text = text.Replace("\r\n", "\n").Replace("\r", "\n");

            foreach (var rawLine in text.Split('\n'))
            {
                var line = rawLine.Trim();
                if (line.Length == 0) continue;

                if (line.StartsWith('[') && line.EndsWith(']'))
                {
                    if (section is not null && s_noteSection.IsMatch(section) || (section == "#SETTING"))
                    {
                        if (current.ContainsKey("Lyric") || current.ContainsKey("Tempo"))
                        {
                            if (s_noteSection.IsMatch(section))
                                notes.Add(BuildNote(current));
                            
                            if (current.TryGetValue("Lyric", out var lv)) allLyrics.Add(lv);
                            current.Clear();
                        }
                    }
                    section = line[1..^1];
                }
                else if (line.Contains('=') && section is not null)
                {
                    int eq = line.IndexOf('=');
                    string key = line[..eq].Trim();
                    string val = line[(eq + 1)..].Trim();
                    if (string.Equals(section, "#SETTING", StringComparison.OrdinalIgnoreCase) &&
                        string.Equals(key, "Tempo", StringComparison.OrdinalIgnoreCase))
                    {
                        if (double.TryParse(val, System.Globalization.NumberStyles.Float,
                                System.Globalization.CultureInfo.InvariantCulture, out double t))
                            tempo = t;
                    }
                    else
                        current[key] = val;
                }
            }

            if (current.Count > 0 && current.ContainsKey("Lyric"))
            {
                notes.Add(BuildNote(current));
                allLyrics.Add(current.TryGetValue("Lyric", out var lv2) ? lv2 : "");
            }

            notationType = (string.IsNullOrEmpty(language) || language == "auto")
                ? DetectNotation(allLyrics) : language;
            return notes;
        }

        private static UstNote BuildNote(Dictionary<string, string> fields)
        {
            double pbsValue = 0.0;
            if (fields.TryGetValue("PBS", out var pbsRaw) && pbsRaw.Contains(';'))
            {
                var pbsParts = pbsRaw.Split(';');
                if (pbsParts.Length > 1 &&
                    double.TryParse(pbsParts[1], System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out double pv))
                    pbsValue = pv;
            }

            var pby = new List<double>();
            if (fields.TryGetValue("PBY", out var pbyRaw) && pbyRaw.Length > 0)
            {
                foreach (var tok in pbyRaw.Split(','))
                {
                    var t = tok.Trim();
                    pby.Add(double.TryParse(t, System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out double d) ? d : 0.0);
                }
            }

            fields.TryGetValue("Lyric", out var lyric);
            int noteNum  = fields.TryGetValue("NoteNum",   out var s) && int.TryParse(s, out int n) ? n : 60;
            int length   = fields.TryGetValue("Length",    out var s2) && int.TryParse(s2, out int l) ? l : 480;
            int intensity= fields.TryGetValue("Intensity", out var s3) && int.TryParse(s3, out int it) ? it : 100;

            double? noteTempo = null;
            if (fields.TryGetValue("Tempo", out var ts) &&
                double.TryParse(ts, System.Globalization.NumberStyles.Float,
                    System.Globalization.CultureInfo.InvariantCulture, out double nt) && nt > 0)
                noteTempo = nt;

            return new UstNote { Lyric=lyric??"", NoteNum=noteNum, Length=length,
                                 Intensity=intensity, PbsValue=pbsValue, Pby=pby,
                                 Tempo=noteTempo };
        }

        private static string StripVcvPrefix(string lyric)
        {
            if (lyric.Contains(' '))
            {
                var m = s_vcvPrefix.Match(lyric);
                if (m.Success) return m.Groups[2].Value;
            }
            if (lyric.StartsWith("-") || lyric.StartsWith("↑") || lyric.StartsWith("↓"))
                return lyric.Substring(1);
            return lyric;
        }

        private static bool IsRest(string lyric)
        {
            var s = StripVcvPrefix(lyric.Trim());
            return s.Length == 0 || s_restLyrics.Contains(s);
        }

        private static bool IsExtension(string lyric) => s_extLyrics.Contains(lyric.Trim());

        private static string DetectNotation(List<string> lyrics)
        {
            var unique = new HashSet<string>(StringComparer.Ordinal);
            foreach (var lyric in lyrics)
            {
                var trimmed = lyric.Trim();
                // Strip decorators like  up dn but NOT the VCV prefix yet
                var cleaned = s_trimDecorators.Replace(trimmed, "").Trim();
                if (cleaned.Length > 0 && !s_restLyrics.Contains(cleaned))
                    unique.Add(cleaned);
            }
            if (unique.Count == 0) return "ipa";

            int jp = 0, xs = 0, en = 0, ipa = 0, arpa = 0, vcc = 0;
            var xsIndicators = new HashSet<char>("@&3690124578");
            var romajiKeys = new HashSet<string>(s_romaji.Select(x => x.Romaji), StringComparer.OrdinalIgnoreCase);

            foreach (var lyric in unique)
            {
                bool hasKana = false;
                foreach (char c in lyric)
                    if ((c >= '぀' && c <= 'ゟ') || (c >= '゠' && c <= 'ヿ'))
                    { hasKana = true; break; }
                if (hasKana) { jp++; continue; }

                // Check for VCC indicators in the original lyric (with prefixes)
                // This ensures "O s", "i k" etc are detected as VCC
                bool hasVccVowel = lyric.Any(c => "AEIOU1234790&@Q".Contains(c));
                bool hasCvvc = Regex.IsMatch(lyric, @"^[a-z][A-Z1234790&@Q]$");
                if (hasVccVowel || hasCvvc) { vcc++; continue; }

                string lower = lyric.ToLowerInvariant();
                if (romajiKeys.Contains(lower)) { jp++; continue; }

                bool isArpa = true;
                var tokens = lyric.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                if (tokens.Length == 0) isArpa = false;
                foreach (var t in tokens)
                {
                    string key = Regex.Replace(t, "[012]$","").ToUpperInvariant();
                    if (!s_arpa.ContainsKey(key)) { isArpa = false; break; }
                }
                if (isArpa) { arpa++; continue; }

                bool hasMixed = lyric.Any(char.IsUpper) && lyric.Any(char.IsLower);
                bool hasXsInd = lyric.Any(c => xsIndicators.Contains(c));
                bool hasXsDig = lyric.Contains("dh") || lyric.Contains("th") || lyric.Contains("zh") ||
                                lyric.Contains("sh") || lyric.Contains("ch") || lyric.Contains("ng");
                if (hasXsInd || hasMixed || hasXsDig) { xs++; continue; }

                bool hasIpa = s_ipaIndicators.Any(lyric.Contains);
                if (hasIpa) { ipa++; continue; }

                if (lyric.Length > 2 && lyric.All(c => c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z'))
                {
                    int v = lyric.ToLowerInvariant().Count(c => "aeiou".Contains(c));
                    if (v >= lyric.Length * 0.3) en++;
                }
            }

            int maxCount = Math.Max(vcc, Math.Max(jp, Math.Max(xs, Math.Max(en, Math.Max(ipa, arpa)))));
            if (maxCount == 0) return "ipa";
            if (vcc == maxCount) return "vcc_english";
            if (jp == maxCount) return "japanese";
            if (arpa == maxCount) return "arpa";
            if (xs == maxCount) return "xsampa";
            if (en == maxCount) return "english";
            return "ipa";
        }

        private static int TicksToMs(int ticks, double tempo)
            => Math.Max(1, (int)(ticks / 480.0 * 60_000.0 / tempo));

        private static string MidiToNoteName(int midi)
        {
            midi = Math.Max(0, Math.Min(127, midi));
            return s_noteNames[midi % 12] + (midi / 12 - 1).ToString();
        }

        private static double MidiToHz(int midi)
            => 440.0 * Math.Pow(2.0, (midi - 69) / 12.0);

        private static string PitchModifier(double baseHz, double cents, bool transient)
        {
            int delta = (int)Math.Round(baseHz * (Math.Pow(2.0, cents / 1200.0) - 1.0));
            if (delta == 0) return "";
            string body = (delta > 0 ? "+" : "-") + Math.Abs(delta).ToString();
            return transient ? "(" + body + ")" : body;
        }

        private static string MapPhoneme(string phoneme, string? compatBank)
        {
            if (s_phonToKlattsch.TryGetValue(phoneme, out var t)) {
                // AX doesn't exist in original klattsch banks, map to AH
                if (t == "AX") return "AH";
                return t;
            }
            return phoneme.ToUpperInvariant();
        }

        private static string KanaToRomaji(string text)
        {
            var sb = new StringBuilder(text.Length * 2);
            int pos = 0;
            while (pos < text.Length)
            {
                bool matched = false;
                foreach (var (kana, romaji) in s_hiragana)
                {
                    int kl = kana.Length;
                    if (pos + kl <= text.Length && string.CompareOrdinal(text, pos, kana, 0, kl) == 0)
                    { sb.Append(romaji); pos += kl; matched = true; break; }
                }
                if (!matched)
                    foreach (var (kana, romaji) in s_katakana)
                    {
                        int kl = kana.Length;
                        if (pos + kl <= text.Length && string.CompareOrdinal(text, pos, kana, 0, kl) == 0)
                        { sb.Append(romaji); pos += kl; matched = true; break; }
                    }
                if (!matched) { sb.Append(text[pos]); pos++; }
            }
            return sb.ToString();
        }

        private static string[]? RomajiToPhonemes(string romaji)
        {
            romaji = romaji.ToLowerInvariant().Trim();
            foreach (var (key, phonemes) in s_romaji)
                if (string.Equals(romaji, key, StringComparison.Ordinal))
                    return phonemes;
            return null;
        }

        private static string XsampaToIpa(string text)
        {
            var sb = new StringBuilder(text.Length);
            int pos = 0;
            while (pos < text.Length)
            {
                bool matched = false;
                foreach (var (sym, ipa) in s_xsampa)
                {
                    int sl = sym.Length;
                    if (pos + sl <= text.Length && string.CompareOrdinal(text, pos, sym, 0, sl) == 0)
                    { sb.Append(ipa); pos += sl; matched = true; break; }
                }
                if (!matched) { sb.Append(text[pos]); pos++; }
            }
            return sb.ToString();
        }

        private static string[] IpaToSharptalk(string ipa)
        {
            ipa = s_stressMarkers.Replace(ipa, "");
            var result = new List<string>();
            int pos = 0;
            while (pos < ipa.Length)
            {
                bool matched = false;
                foreach (var (key, codes) in s_ipa)
                {
                    int kl = key.Length;
                    if (pos + kl <= ipa.Length && string.CompareOrdinal(ipa, pos, key, 0, kl) == 0)
                    { result.AddRange(codes); pos += kl; matched = true; break; }
                }
                if (!matched) pos++;
            }
            return result.ToArray();
        }

        private static string[]? LyricToPhonemes(string lyric, string notationType, HashSet<string> unknownLyrics, Func<string, string[]?>? englishPhonemizer)
        {
            string originalLyric = lyric;
            lyric = lyric.Trim();
            if (lyric.Length == 0 || s_restLyrics.Contains(lyric)) return null;
            if (s_extLyrics.Contains(lyric)) return null;

            lyric = StripVcvPrefix(lyric);
            lyric = s_trimDecorators.Replace(lyric, "").Trim();
            if (lyric.Length == 0) return null;

            if (notationType == "vcc_english")
            {
                // Re-strip decorators on the original lyric to avoid StripVcvPrefix's 
                // destructive behavior on space-containing VCC transitions like "O s"
                lyric = s_trimDecorators.Replace(originalLyric, "").Trim();
                
                var result = new List<string>();
                int pos = 0;
                while (pos < lyric.Length)
                {
                    bool matched = false;
                    foreach (var kv in s_vccVowelsSorted)
                    {
                        if (pos + kv.Key.Length <= lyric.Length && 
                            string.CompareOrdinal(lyric, pos, kv.Key, 0, kv.Key.Length) == 0)
                        {
                            result.AddRange(kv.Value);
                            pos += kv.Key.Length;
                            matched = true;
                            break;
                        }
                    }
                    if (matched) continue;

                    foreach (var kv in s_vccConsonantsSorted)
                    {
                        if (pos + kv.Key.Length <= lyric.Length && 
                            string.CompareOrdinal(lyric, pos, kv.Key, 0, kv.Key.Length) == 0)
                        {
                            result.AddRange(kv.Value);
                            pos += kv.Key.Length;
                            matched = true;
                            break;
                        }
                    }
                    if (matched) continue;

                    string sub = lyric.Substring(pos, 1);
                    var phons = IpaToSharptalk(sub);
                    if (phons.Length > 0) result.AddRange(phons);
                    pos++;
                }
                return result.Count > 0 ? result.ToArray() : null;
            }

            if (notationType == "japanese")
            {
                string romaji = KanaToRomaji(lyric);
                if (string.IsNullOrEmpty(romaji)) romaji = lyric.ToLowerInvariant();
                
                if (romaji == "r" || romaji == "-" || romaji == "_") return null;
                if (romaji == "q") return Array.Empty<string>();
                
                var phonemes = RomajiToPhonemes(romaji);
                if (phonemes is not null) return phonemes;
            }

            if (notationType == "arpa")
            {
                var parts = lyric.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                var result = new List<string>();
                foreach (var p in parts)
                {
                    string key = Regex.Replace(p, "[012]$","").ToUpperInvariant();
                    if (s_arpa.TryGetValue(key, out var phons))
                        result.AddRange(phons);
                    else
                        unknownLyrics.Add(p);
                }
                return result.Count > 0 ? result.ToArray() : null;
            }

            if (notationType == "arpa_jp")
            {
                var parts = lyric.Split(' ', StringSplitOptions.RemoveEmptyEntries);
                var result = new List<string>();
                foreach (var p in parts)
                {
                    string key = Regex.Replace(p, "[012]$","").ToUpperInvariant();
                    if (s_arpa_jp.TryGetValue(key, out var phons))
                        result.AddRange(phons);
                    else
                        unknownLyrics.Add(p);
                }
                return result.Count > 0 ? result.ToArray() : null;
            }

            if (notationType == "xsampa")
                return IpaToSharptalk(XsampaToIpa(lyric));

            if (notationType == "english")
            {
                if (englishPhonemizer != null)
                {
                    var phons = englishPhonemizer(lyric);
                    if (phons != null && phons.Length > 0) return phons;
                }
                unknownLyrics.Add(lyric);
                return null;
            }

            if (notationType == "ipa" || notationType == "japanese")
            {
                var phonemes = IpaToSharptalk(lyric);
                if (phonemes.Length > 0) return phonemes;
            }

            unknownLyrics.Add(lyric);
            return null;
        }

        private static string BuildDiagnostics(
            string notationType,
            double tempo,
            Dictionary<string, int> counts,
            HashSet<string> unknownLyrics)
        {
            int total = counts.Values.Sum();
            var sb = new StringBuilder();
            sb.AppendLine($"Language: {notationType} | Tempo: {tempo:F0} BPM | {total} phonemes");

            foreach (var (label, members) in s_phonCategories)
            {
                int catTotal = members.Sum(p => counts.GetValueOrDefault(p));
                if (catTotal == 0) { sb.AppendLine($"{label}: (none)"); continue; }
                var detail = string.Join("  ", members
                    .Where(p => counts.GetValueOrDefault(p) > 0)
                    .Select(p => {
                        var tok = s_phonToKlattsch.TryGetValue(p, out var t) ? t : p.ToUpperInvariant();
                        return $"{tok}:{counts[p]}";
                    }));
                sb.AppendLine($"{label}: {catTotal}  ({detail})");
            }

            if (unknownLyrics.Count > 0)
                sb.AppendLine("Unknown lyrics: " + string.Join(", ", unknownLyrics.OrderBy(x => x)));
            else
                sb.AppendLine("Unknown lyrics: (none)");

            if (notationType == "english")
                sb.AppendLine("Note: espeak-ng unavailable in WebAssembly — English lyrics show as unknown");

            return sb.ToString().TrimEnd();
        }
    }
}
