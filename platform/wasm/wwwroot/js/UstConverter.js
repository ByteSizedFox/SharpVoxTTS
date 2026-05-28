// Native JS port of UstConverter.cs
(function (global) {
  'use strict';

  function sortDesc(arr) {
    return arr.slice().sort((a, b) => b[0].length - a[0].length);
  }

  function cv(vowelCode, ...cons) { return [...cons, vowelCode]; }
  const JA = "jp_aa", JI = "jp_iy", JU = "jp_uw", JE = "jp_eh", JO = "jp_ow";

  const hiragana = sortDesc([
    ["いぇ","ye"],["うぃ","wi"],["うぇ","we"],["うぉ","wo"],
    ["きゃ","kya"],["きゅ","kyu"],["きょ","kyo"],["ぎゃ","gya"],["ぎゅ","gyu"],["ぎょ","gyo"],
    ["しぇ","she"],["しゃ","sha"],["しゅ","shu"],["しょ","sho"],
    ["じぇ","je"],["じゃ","ja"],["じゅ","ju"],["じょ","jo"],
    ["ちぇ","che"],["ちゃ","cha"],["ちゅ","chu"],["ちょ","cho"],
    ["つぁ","tsa"],["つぃ","tsi"],["つぇ","tse"],["つぉ","tso"],
    ["てぃ","thi"],["でぃ","dhi"],["とゅ","thu"],["どゅ","dhu"],
    ["にゃ","nya"],["にゅ","nyu"],["にょ","nyo"],
    ["ひゃ","hya"],["ひゅ","hyu"],["ひょ","hyo"],
    ["びゃ","bya"],["びゅ","byu"],["びょ","byo"],
    ["ぴゃ","pya"],["ぴゅ","pyu"],["ぴょ","pyo"],
    ["ふぁ","fa"],["ふぃ","fi"],["ふぇ","fe"],["ふぉ","fo"],
    ["みゃ","mya"],["みゅ","myu"],["みょ","myo"],
    ["りゃ","rya"],["りゅ","ryu"],["りょ","ryo"],
    ["ゔぁ","va"],["ゔぃ","vi"],["ゔぇ","ve"],["ゔぉ","vo"],
    ["ぁ","a"],["あ","a"],["ぃ","i"],["い","i"],["ぅ","u"],["う","u"],
    ["ぇ","e"],["え","e"],["ぉ","o"],["お","o"],
    ["か","ka"],["が","ga"],["き","ki"],["ぎ","gi"],
    ["く","ku"],["ぐ","gu"],["け","ke"],["げ","ge"],["こ","ko"],["ご","go"],
    ["さ","sa"],["ざ","za"],["し","shi"],["じ","ji"],
    ["す","su"],["ず","zu"],["せ","se"],["ぜ","ze"],["そ","so"],["ぞ","zo"],
    ["た","た"],["だ","da"],["ち","chi"],["ぢ","di"],
    ["っ","q"],["つ","tsu"],["づ","du"],["て","te"],["で","de"],
    ["と","to"],["ど","do"],
    ["な","na"],["に","ni"],["ぬ","nu"],["ね","ne"],["の","no"],
    ["は","ha"],["ば","ba"],["ぱ","pa"],["ひ","hi"],["び","bi"],["ぴ","pi"],
    ["ふ","fu"],["ぶ","bu"],["ぷ","pu"],
    ["へ","he"],["べ","be"],["ぺ","pe"],["ほ","ho"],["ぼ","bo"],["ぽ","po"],
    ["ま","ma"],["み","mi"],["む","mu"],["め","me"],["も","mo"],
    ["ゃ","ya"],["や","ya"],["ゅ","yu"],["ゆ","yu"],["ょ","yo"],["よ","yo"],
    ["ら","ra"],["り","ri"],["る","ru"],["れ","re"],["ろ","ro"],
    ["わ","wa"],["ゐ","wi"],["ゑ","we"],["を","wo"],["ん","n"],["ゔ","vu"],
  ]);

  const katakana = sortDesc([
    ["イェ","ye"],["ウィ","wi"],["ウェ","we"],["ウォ","wo"],
    ["キャ","kya"],["キュ","kyu"],["キョ","kyo"],["ギャ","gya"],["ギュ","gyu"],["ギョ","gyo"],
    ["シェ","she"],["シャ","sha"],["シュ","shu"],["ショ","sho"],
    ["ジェ","je"],["じゃ","ja"],["じゅ","ju"],["じょ","jo"],
    ["チェ","che"],["チャ","cha"],["チュ","chu"],["チョ","cho"],
    ["ツぁ","tsa"],["ツィ","tsi"],["ツェ","tse"],["ツぉ","tso"],
    ["ティ","thi"],["ディ","dhi"],["トゥ","thu"],["ドゥ","dhu"],
    ["ニャ","nya"],["ニュ","nyu"],["ニョ","nyo"],
    ["ヒャ","hya"],["ヒュ","hyu"],["ヒョ","hyo"],
    ["ビャ","bya"],["びゅ","byu"],["びょ","byo"],
    ["ピャ","pya"],["ぴゅ","pyu"],["ぴょ","pyo"],
    ["ファ","fa"],["フィ","fi"],["フェ","fe"],["フォ","fo"],
    ["みゃ","mya"],["みゅ","myu"],["みょ","myo"],
    ["りゃ","rya"],["りゅ","ryu"],["りょ","ryo"],
    ["ヴァ","va"],["ヴィ","vi"],["ヴェ","ve"],["ヴォ","vo"],
    ["ァ","a"],["ア","a"],["ィ","i"],["イ","i"],["ゥ","u"],["ウ","u"],
    ["ぇ","e"],["エ","e"],["ぉ","o"],["オ","o"],
    ["カ","ka"],["ガ","ga"],["キ","ki"],["ギ","gi"],
    ["ク","ku"],["グ","gu"],["ケ","ke"],["ゲ","ge"],["コ","ko"],["ゴ","go"],
    ["サ","sa"],["ザ","za"],["シ","shi"],["ジ","ji"],
    ["ス","su"],["ズ","zu"],["セ","se"],["ぜ","ze"],["そ","so"],["ぞ","zo"],
    ["タ","ta"],["だ","da"],["チ","chi"],["ヂ","di"],
    ["っ","q"],["ツ","tsu"],["づ","du"],["て","te"],["で","de"],
    ["と","to"],["ど","do"],
    ["な","na"],["に","ni"],["ぬ","nu"],["ね","ne"],["の","no"],
    ["は","ha"],["ば","ba"],["ぱ","pa"],["ひ","hi"],["び","bi"],["ぴ","pi"],
    ["ふ","fu"],["ぶ","bu"],["ぷ","pu"],
    ["へ","he"],["べ","be"],["ぺ","pe"],["ほ","ho"],["ぼ","bo"],["ぽ","po"],
    ["ま","ma"],["み","mi"],["む","mu"],["め","me"],["も","mo"],
    ["ゃ","ya"],["ヤ","ya"],["ゅ","yu"],["ユ","yu"],["ょ","yo"],["ヨ","yo"],
    ["ら","ra"],["り","ri"],["る","ru"],["れ","re"],["ろ","ro"],
    ["わ","wa"],["ゐ","wi"],["ゑ","we"],["を","wo"],["ん","n"],["ヴ","vu"],
  ]);

  const romaji = sortDesc([
    ["a",cv(JA)],["i",cv(JI)],["u",cv(JU)],["e",cv(JE)],["o",cv(JO)],
    ["ka",cv(JA,"k")],["ki",cv(JI,"k")],["ku",cv(JU,"k")],["ke",cv(JE,"k")],["ko",cv(JO,"k")],
    ["kya",cv(JA,"k","y")],["kyu",cv(JU,"k","y")],["kyo",cv(JO,"k","y")],
    ["ga",cv(JA,"g")],["gi",cv(JI,"g")],["gu",cv(JU,"g")],["ge",cv(JE,"g")],["go",cv(JO,"g")],
    ["gya",cv(JA,"g","y")],["gyu",cv(JU,"g","y")],["gyo",cv(JO,"g","y")],
    ["sa",cv(JA,"s")],["si",cv(JI,"sh")],["su",cv(JU,"s")],["se",cv(JE,"s")],["so",cv(JO,"s")],
    ["sha",cv(JA,"sh")],["shi",cv(JI,"sh")],["shu",cv(JU,"sh")],["she",cv(JE,"sh")],["sho",cv(JO,"sh")],
    ["sya",cv(JA,"sh")],["syu",cv(JU,"sh")],["syo",cv(JO,"sh")],
    ["za",cv(JA,"z")],["zi",cv(JI,"zh")],["zu",cv(JU,"z")],["ze",cv(JE,"z")],["zo",cv(JO,"z")],
    ["ja",cv(JA,"jh")],["ji",cv(JI,"jh")],["ju",cv(JU,"jh")],["je",cv(JE,"jh")],["jo",cv(JO,"jh")],
    ["jya",cv(JA,"jh")],["jyu",cv(JU,"jh","y")],["jyo",cv(JO,"jh","y")],
    ["ta",cv(JA,"t")],["ti",cv(JI,"ch")],["tu",cv(JU,"t","s")],["te",cv(JE,"t")],["to",cv(JO,"t")],
    ["cha",cv(JA,"ch")],["chi",cv(JI,"ch")],["chu",cv(JU,"ch")],["che",cv(JE,"ch")],["cho",cv(JO,"ch")],
    ["tya",cv(JA,"ch","y")],["tyu",cv(JU,"ch","y")],["tyo",cv(JO,"ch","y")],
    ["tsa",cv(JA,"t","s")],["tsi",cv(JI,"t","s")],["tsu",cv(JA,"t","s")],["tse",cv(JE,"t","s")],["tso",cv(JO,"t","s")],
    ["thi",cv(JI,"t")],["thu",cv(JU,"t")],
    ["da",cv(JA,"d")],["di",cv(JI,"jh")],["du",cv(JU,"d","z")],["de",cv(JE,"d")],["do",cv(JO,"d")],
    ["dhi",cv(JI,"d")],["dhu",cv(JU,"d")],
    ["na",cv(JA,"n")],["ni",cv(JI,"n")],["nu",cv(JU,"n")],["ne",cv(JE,"n")],["no",cv(JO,"n")],
    ["nya",cv(JA,"n","y")],["nyu",cv(JU,"n","y")],["nyo",cv(JO,"n","y")],
    ["ha",cv(JA,"hh")],["hi",cv(JI,"hh")],["fu",cv(JU,"f")],["he",cv(JE,"hh")],["ho",cv(JO,"hh")],
    ["hya",cv(JA,"hh","y")],["hyu",cv(JU,"hh","y")],["hyo",cv(JO,"hyo")],
    ["fa",cv(JA,"f")],["fi",cv(JI,"f")],["fe",cv(JE,"f")],["fo",cv(JO,"f")],
    ["ba",cv(JA,"b")],["bi",cv(JI,"b")],["bu",cv(JU,"b")],["be",cv(JE,"b")],["bo",cv(JO,"b")],
    ["bya",cv(JA,"b","y")],["byu",cv(JU,"b","y")],["byo",cv(JO,"b","y")],
    ["pa",cv(JA,"p")],["pi",cv(JI,"p")],["pu",cv(JU,"p")],["pe",cv(JE,"p")],["po",cv(JO,"p")],
    ["pya",cv(JA,"p","y")],["pyu",cv(JU,"p","y")],["pyo",cv(JO,"p","y")],
    ["ma",cv(JA,"m")],["mi",cv(JI,"m")],["mu",cv(JU,"m")],["me",cv(JE,"m")],["mo",cv(JO,"m")],
    ["mya",cv(JA,"m","y")],["myu",cv(JU,"m","y")],["myo",cv(JO,"m","y")],
    ["ya",cv(JA,"y")],["yu",cv(JU,"y")],["yo",cv(JO,"y")],["ye",cv(JE,"y")],
    ["ra",cv(JA,"dx")],["ri",cv(JI,"dx")],["ru",cv(JU,"dx")],["re",cv(JE,"dx")],["ro",cv(JO,"dx")],
    ["rya",cv(JA,"dx","y")],["ryu",cv(JU,"dx","y")],["ryo",cv(JO,"dx","y")],
    ["wa",cv(JA,"w")],["wi",cv(JI,"w")],["we",cv(JE,"w")],["wo",cv(JO,"w")],
    ["va",cv(JA,"v")],["vi",cv(JI,"v")],["vu",cv(JU,"v")],["ve",cv(JE,"v")],["vo",cv(JO,"v")],
    ["n", ["n"]],
    ["q", []],
  ]);

  const xsampa = sortDesc([
    ["d\\`","ɖ"],["dZ","dʒ"],["dz\\","dʑ"],["g\\","ɢ"],
    ["h\\","ɦ"],["j\\","ʝ"],["l\\`","ɭ"],["l\\","ɭ"],["n\\`","ɳ"],
    ["p\\","ɸ"],["r\\`","ɻ"],["r\\","ɹ"],["s\\`","ʂ"],["s\\","ɕ"],
    ["tS","tʃ"],["t\\`","ʈ"],["ts\\","tɕ"],
    ["x\\","ɧ"],["z\\`","ʐ"],["z\\","ʑ"],
    ["B\\","ʙ"],["G\\","ɢ"],["H\\","ʜ"],["L\\","ʟ"],["O\\","ʘ"],
    ["|\\|\\","ǁ"],["!\\","ǃ"],["=\\","ǂ"],["|\\","ǀ"],
    ["&","æ"],["'","ˈ"],[",","ˌ"],["0","ɒ"],["1","ɨ"],["2","ø"],["3","ɜ"],
    ["4","ɾ"],["5","ɫ"],["6","ɐ"],["7","ɤ"],["8","ɵ"],["9","œ"],[":","ː"],
    ["=","̩"],["?","ʔ"],["@","ə"],["A","ɑ"],["B","β"],["C","ç"],["D","ð"],
    ["E","ɛ"],["F","ɱ"],["G","ɣ"],["H","ɥ"],["I","ɪ"],["J","ɲ"],["K","ɬ"],
    ["L","ʎ"],["M","ɯ"],["N","ŋ"],["O","ɔ"],["P","ʋ"],["Q","ɒ"],["R","ʁ"],
    ["S","ʃ"],["T","θ"],["U","ʊ"],["V","ʌ"],["W","ʍ"],["X","χ"],["Y","ʏ"],
    ["Z","ʒ"],["^","̯"],["_","_"],
    ["i\\","ɨ"],["u\\","ʉ"],["e\\","ɘ"],["o\\","ɵ"],["E\\","ɝ"],["a\\","ɐ"],
    ["A\\","ɑ"],["M\\","ɯ"],["U\\","ʊ"],["R\\","ʀ"],["v\\","ʋ"],
    ["k\\","χ"],["K\\","ɬ"],
  ]);

  const ipa = sortDesc([
    ["aɪ",["ay"]],["aʊ",["aw"]],["eɪ",["ey"]],["oʊ",["ow"]],["əʊ",["ow"]],
    ["ɔɪ",["oy"]],["iː",["iy"]],["uː",["uw"]],["aː",["aa"]],["ɑː",["aa"]],
    ["ɜː",["er"]],["ɔː",["ao"]],["eː",["eh"]],["oː",["ow"]],
    ["ɪə",["ih","r"]],["eə",["eh","r"]],["ʊə",["uh","r"]],["ɔə",["ao","r"]],
    ["t͡ʃ",["ch"]],["d͡ʒ",["jh"]],["t͡s",["t","s"]],["d͡z",["z"]],
    ["d͡ʑ",["jh"]],["t͡ɕ",["ch"]],
    ["tʃ",["ch"]],["dʒ",["jh"]],["ts",["t","s"]],["dz",["z"]],
    ["dʑ",["jh"]],["tɕ",["ch"]],["ɖʐ",["jh"]],["ʈʂ",["ch"]],
    ["l̩",["el"]],["m̩",["m"]],["n̩",["en"]],
    ["ŋ",["ng"]],["ɴ",["ng"]],
    ["i",["iy"]],["ɪ",["ih"]],["e",["eh"]],["ɛ",["eh"]],["æ",["ae"]],
    ["a",["aa"]],["ɑ",["aa"]],["ɒ",["aa"]],
    ["ɔ",["ao"]],["ʌ",["ah"]],["ɐ",["ah"]],
    ["ʊ",["uh"]],["u",["uw"]],
    ["ə",["ax"]],["ɘ",["ax"]],["ɵ",["ax"]],
    ["ɜ",["er"]],["ɝ",["er"]],["ɚ",["er"]],
    ["ɨ",["ix"]],["ɯ",["uw"]],
    ["ø",["er"]],["œ",["er"]],["y",["iy"]],
    ["w",["w"]],["ɥ",["w"]],["ʍ",["w"]],
    ["j",["y"]],["ʝ",["y"]],
    ["ɹ",["r"]],["r",["r"]],["ɻ",["r"]],["ʀ",["r"]],["ʁ",["r"]],
    ["ɾ",["dx"]],["ɽ",["dx"]],
    ["l",["l"]],["ɫ",["l"]],["ʎ",["l"]],["ɭ",["l"]],["ɺ",["l"]],
    ["m",["m"]],["ɱ",["m"]],
    ["n",["n"]],["ɳ",["n"]],["ɲ",["n"]],
    ["h",["hh"]],["ɦ",["hh"]],["ɸ",["f"]],["ħ",["hh"]],
    ["f",["f"]],["v",["v"]],["ʋ",["v"]],
    ["θ",["th"]],["ð",["dh"]],
    ["s",["s"]],["z",["z"]],
    ["ʂ",["sh"]],["ʃ",["sh"]],["ɕ",["sh"]],
    ["ʐ",["zh"]],["ʒ",["zh"]],["ʑ",["zh"]],
    ["ç",["sh"]],["χ",["k"]],["ɣ",["g"]],
    ["β",["b"]],["x",["k"]],
    ["p",["p"]],["b",["b"]],["ɓ",["b"]],["ʙ",["b"]],
    ["t",["t"]],["d",["d"]],["ɗ",["d"]],["ɖ",["d"]],
    ["c",["t"]],["ɟ",["d"]],
    ["k",["k"]],["g",["g"]],["ɡ",["g"]],["ɠ",["g"]],["ɢ",["g"]],
    ["q",["k"]],
    ["_",["_"]],
    ["ʔ",[]],["ˈ",[]],["ˌ",[]],["ː",[]],
    ["ˑ",[]],["̃",[]],["̩",[]],["̯",[]],["ʰ",[]],["ʲ",[]],
    ["ʷ",[]],["ˠ",[]],["ˤ",[]],
  ]);

  const arpa = new Map([
    ["AA",["aa"]],["AE",["ae"]],["AH",["ah"]],["AO",["ao"]],
    ["AW",["aw"]],["AY",["ay"]],["EH",["eh"]],["ER",["er"]],
    ["EY",["ey"]],["IH",["ih"]],["IY",["iy"]],["OW",["ow"]],
    ["OY",["oy"]],["UH",["uh"]],["UW",["uw"]],
    ["B",["b"]],["CH",["ch"]],["D",["d"]],["DH",["dh"]],
    ["F",["f"]],["G",["g"]],["HH",["hh"]],["JH",["jh"]],
    ["K",["k"]],["L",["l"]],["M",["m"]],["N",["n"]],
    ["NG",["ng"]],["P",["p"]],["R",["r"]],["S",["s"]],
    ["SH",["sh"]],["T",["t"]],["TH",["th"]],["V",["v"]],
    ["W",["w"]],["Y",["y"]],["Z",["z"]],["ZH",["zh"]],
    ["AX",["ax"]],["IX",["ix"]],["DX",["dx"]],
    ["A",["aa"]],["I",["ay"]],["U",["uw"]],["E",["eh"]],["O",["ow"]],
    ["Q",["_"]],["SIL",[]],["SP",[]],
  ]);

  const arpa_jp = new Map([
    ["AA",["jp_aa"]],["AE",["jp_aa"]],["AH",["jp_aa"]],["AO",["jp_ow"]],
    ["AW",["jp_aa","jp_uw"]],["AY",["jp_aa","jp_iy"]],["EH",["jp_eh"]],["ER",["jp_eh"]],
    ["EY",["jp_eh","jp_iy"]],["IH",["jp_iy"]],["IY",["jp_iy"]],["OW",["jp_ow"]],
    ["OY",["jp_ow","jp_iy"]],["UH",["jp_uw"]],["UW",["jp_uw"]],
    ["B",["b"]],["CH",["ch"]],["D",["d"]],["DH",["z"]],
    ["F",["f"]],["G",["g"]],["HH",["hh"]],["JH",["jh"]],
    ["K",["k"]],["L",["dx"]],["M",["m"]],["N",["n"]],
    ["NG",["n"]],["P",["p"]],["R",["dx"]],["S",["s"]],
    ["SH",["sh"]],["T",["t"]],["TH",["s"]],["V",["v"]],
    ["W",["w"]],["Y",["y"]],["Z",["z"]],["ZH",["sh"]],
    ["AX",["jp_aa"]],["IX",["jp_iy"]],["DX",["dx"]],
    ["A",["aa"]],["I",["ay"]],["U",["jp_uw"]],["E",["eh"]],["O",["jp_ow"]],
    ["Q",["_"]],["SIL",[]],["SP",[]],
  ]);

  const phonToKlattsch = new Map([
    ["jp_aa","A"],["jp_iy","I"],["jp_uw","U"],["jp_eh","E"],["jp_ow","O"],
    ["iy","IY"],["ih","IH"],["eh","EH"],["ae","AE"],
    ["aa","AA"],["ah","AH"],["ao","AO"],["uh","UH"],
    ["ax","AX"],["er","ER"],["ey","EY"],["ay","AY"],
    ["oy","OY"],["aw","AW"],["ow","OW"],["uw","UW"],["ix","IX"],
    ["w","W"],["y","Y"],["r","R"],["l","L"],
    ["m","M"],["n","N"],["ng","NG"],
    ["hh","HH"],["f","F"],["v","V"],["th","TH"],["dh","DH"],
    ["s","S"],["z","Z"],["sh","SH"],["zh","ZH"],
    ["p","P"],["b","B"],["t","T"],["d","D"],["dx","DX"],["k","K"],["g","G"],
    ["ch","CH"],["jh","JH"],["_","_"],
    ["yu","YU"],["rx","RX"],["lx","LX"],["el","EL"],["en","EN"],["tx","TX"],
  ]);

  const diphthongs = new Map([
    ["AY",{nucleus:"AA",terminal:"IY"}],
    ["AW",{nucleus:"AA",terminal:"UW"}],
    ["EY",{nucleus:"EH",terminal:"IY"}],
    ["OW",{nucleus:"OW",terminal:"UW"}],
    ["OY",{nucleus:"AO",terminal:"IY"}],
  ]);

  const phonCategories = [
    {label:"JP vowels",  members:["jp_aa","jp_iy","jp_uw","jp_eh","jp_ow"]},
    {label:"EN vowels",  members:["iy","ih","eh","ae","aa","ao","ah","uh","uw","er","ey","ay","oy","aw","ow","ax","ix"]},
    {label:"Stops",      members:["p","b","t","d","k","g","dx","tx"]},
    {label:"Fricatives", members:["f","v","th","dh","s","z","sh","zh","hh"]},
    {label:"Affricates", members:["ch","jh"]},
    {label:"Nasals",     members:["m","n","ng"]},
    {label:"Sonorants",  members:["w","y","r","l","yu","rx","lx"]},
    {label:"Syllabic",   members:["el","en"]},
  ];
  const jpVowelSet = new Set(phonCategories[0].members);
  const enVowelSet = new Set(phonCategories[1].members);

  const vccVowels = sortDesc(Object.entries({
    "ey":["ey"],"ay":["ay"],"oy":["oy"],"aw":["aw"],"ow":["ow"],
    "Er":["er"],"Ar":["aa","r"],"Or":["ao","r"],"Ir":["iy","r"],"Ur":["uw","r"],
    "aI":["ay"],"aU":["aw"],"aO":["aw"],"eI":["ey"],"oI":["oy"],"oU":["ow"],
    "A":["ey"],"E":["iy"],"I":["ay"],"O":["ow"],"U":["uw"],
    "a":["ae"],"e":["eh"],"i":["ih"],"o":["uw"],"u":["ah"],
    "@":["ax"],"1":["ih"],"2":["aw"],"3":["er"],"4":["oy"],
    "&":["ae"],"Q":["ao"],"7":["ah"],"9":["oy"],"0":["aw"],
    "8":["uh"],"6":["er"],"5":["el"],
  }));

  const vccConsonants = sortDesc(Object.entries({
    "ch":["ch"],"sh":["sh"],"th":["th"],"dh":["dh"],
    "zh":["zh"],"ng":["ng"],"ph":["f"],"kh":["k"],
    "gh":["g"],"qu":["k","w"],"ts":["t","s"],"dz":["z"],
    "w":["w"],"y":["y"],"r":["r"],"l":["l"],
    "m":["m"],"n":["n"],"f":["f"],"v":["v"],
    "s":["s"],"z":["z"],"p":["p"],"b":["b"],
    "t":["t"],"d":["d"],"k":["k"],"g":["g"],
    "h":["hh"],"j":["jh"],
  }));

  const NOTE_NAMES  = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];
  const REST_LYRICS = new Set(["R","r","_"]);
  const EXT_LYRICS  = new Set(["+","*","↑","↓","-"]);

  const RE_NOTE_SECTION = /^#[0-9A-Fa-f]+$/;
  const RE_VCV_PREFIX   = /^([aeiouAEIOU\-n1-9@&Q] ?)(.+)$/;
  const RE_TRIM_DECO    = /^[-+*↑↓\[\]{}]+|[-+*↑↓\[\]{}]+$/g;
  const RE_STRESS       = /[ˈˌ]/g;
  const RE_ARPA_STRESS  = /[012]$/;

  const IPA_INDICATORS = ["ə","ɛ","ɪ","ɔ","ʃ","ʒ","ð","θ","ŋ","ɑ","æ","ʌ","ɜ","ɯ","ɾ","ɸ","ç","ɕ","ʑ","ɹ"];
  const PITCH_CENTS_THRESHOLD = 20.0;

  const romajiKeys = new Set(romaji.map(x => x[0]));


  function ticksToMs(ticks, tempo) {
    return Math.max(1, Math.round(ticks / 480.0 * 60000.0 / tempo));
  }

  function midiToNoteName(midi) {
    midi = Math.max(0, Math.min(127, midi));
    return NOTE_NAMES[midi % 12] + String(Math.floor(midi / 12) - 1);
  }

  function midiToHz(midi) {
    return 440.0 * Math.pow(2.0, (midi - 69) / 12.0);
  }

  function pitchModifier(baseHz, cents, transient) {
    const delta = Math.round(baseHz * (Math.pow(2.0, cents / 1200.0) - 1.0));
    if (delta === 0) return "";
    const body = (delta > 0 ? "+" : "-") + Math.abs(delta);
    return transient ? "(" + body + ")" : body;
  }

  function mapPhoneme(phoneme, compatBank) {
    const t = phonToKlattsch.get(phoneme);
    if (t !== undefined) {
      if (t === "AX") return "AH";
      return t;
    }
    return phoneme.toUpperCase();
  }

  function stripVcvPrefix(lyric) {
    if (lyric.includes(" ")) {
      const m = RE_VCV_PREFIX.exec(lyric);
      if (m) return m[2];
    }
    if (lyric.startsWith("-") || lyric.startsWith("↑") || lyric.startsWith("↓"))
      return lyric.slice(1);
    return lyric;
  }

  function isRest(lyric) {
    const s = stripVcvPrefix(lyric.trim());
    return s.length === 0 || REST_LYRICS.has(s);
  }

  function isExtension(lyric) {
    return EXT_LYRICS.has(lyric.trim());
  }

  function kanaToRomaji(text) {
    let result = "", pos = 0;
    while (pos < text.length) {
      let matched = false;
      for (const [kana, r] of hiragana) {
        if (text.startsWith(kana, pos)) { result += r; pos += kana.length; matched = true; break; }
      }
      if (!matched) {
        for (const [kana, r] of katakana) {
          if (text.startsWith(kana, pos)) { result += r; pos += kana.length; matched = true; break; }
        }
      }
      if (!matched) { result += text[pos]; pos++; }
    }
    return result;
  }

  function romajiToPhonemes(r) {
    r = r.toLowerCase().trim();
    for (const [key, phons] of romaji) {
      if (r === key) return phons;
    }
    return null;
  }

  function xsampaToIpa(text) {
    let result = "", pos = 0;
    while (pos < text.length) {
      let matched = false;
      for (const [sym, ipaChar] of xsampa) {
        if (text.startsWith(sym, pos)) { result += ipaChar; pos += sym.length; matched = true; break; }
      }
      if (!matched) { result += text[pos]; pos++; }
    }
    return result;
  }

  function ipaToSharptalk(ipaStr) {
    ipaStr = ipaStr.replace(RE_STRESS, "");
    const result = [];
    let pos = 0;
    while (pos < ipaStr.length) {
      let matched = false;
      for (const [key, codes] of ipa) {
        if (ipaStr.startsWith(key, pos)) { result.push(...codes); pos += key.length; matched = true; break; }
      }
      if (!matched) pos++;
    }
    return result;
  }

  function detectNotation(lyrics) {
    const unique = new Set();
    for (const lyric of lyrics) {
      const cleaned = lyric.trim().replace(RE_TRIM_DECO, "").trim();
      if (cleaned.length > 0 && !REST_LYRICS.has(cleaned)) unique.add(cleaned);
    }
    if (unique.size === 0) return "ipa";

    let jp = 0, xs = 0, en = 0, ipaCount = 0, arpaCount = 0, vcc = 0;
    const xsIndicators = new Set("@&3690124578");

    for (const lyric of unique) {
      let hasKana = false;
      for (const c of lyric) {
        const cp = c.codePointAt(0);
        if ((cp >= 0x3040 && cp <= 0x309F) || (cp >= 0x30A0 && cp <= 0x30FF)) { hasKana = true; break; }
      }
      if (hasKana) { jp++; continue; }

      const hasVccVowel = [...lyric].some(c => "AEIOU1234790&@Q".includes(c));
      const hasCvvc = /^[a-z][A-Z1234790&@Q]$/.test(lyric);
      if (hasVccVowel || hasCvvc) { vcc++; continue; }

      if (romajiKeys.has(lyric.toLowerCase())) { jp++; continue; }

      const tokens = lyric.split(/\s+/).filter(t => t.length > 0);
      let isArpa = tokens.length > 0;
      for (const t of tokens) {
        if (!arpa.has(t.replace(RE_ARPA_STRESS, "").toUpperCase())) { isArpa = false; break; }
      }
      if (isArpa) { arpaCount++; continue; }

      const hasMixed = /[A-Z]/.test(lyric) && /[a-z]/.test(lyric);
      const hasXsInd = [...lyric].some(c => xsIndicators.has(c));
      const hasXsDig = lyric.includes("dh") || lyric.includes("th") || lyric.includes("zh") ||
                       lyric.includes("sh") || lyric.includes("ch") || lyric.includes("ng");
      if (hasXsInd || hasMixed || hasXsDig) { xs++; continue; }

      if (IPA_INDICATORS.some(ind => lyric.includes(ind))) { ipaCount++; continue; }

      if (lyric.length > 2 && /^[a-zA-Z]+$/.test(lyric)) {
        const vCount = [...lyric.toLowerCase()].filter(c => "aeiou".includes(c)).length;
        if (vCount >= lyric.length * 0.3) en++;
      }
    }

    const maxCount = Math.max(vcc, jp, xs, en, ipaCount, arpaCount);
    if (maxCount === 0) return "ipa";
    if (vcc === maxCount) return "vcc_english";
    if (jp === maxCount) return "japanese";
    if (arpaCount === maxCount) return "arpa";
    if (xs === maxCount) return "xsampa";
    if (en === maxCount) return "english";
    return "ipa";
  }

  function lyricToPhonemes(lyric, notationType, unknownLyrics) {
    const originalLyric = lyric;
    lyric = lyric.trim();
    if (lyric.length === 0 || REST_LYRICS.has(lyric)) return null;
    if (EXT_LYRICS.has(lyric)) return null;

    lyric = stripVcvPrefix(lyric);
    lyric = lyric.replace(RE_TRIM_DECO, "").trim();
    if (lyric.length === 0) return null;

    if (notationType === "vcc_english") {
      lyric = originalLyric.replace(RE_TRIM_DECO, "").trim();
      const result = [];
      let pos = 0;
      while (pos < lyric.length) {
        let matched = false;
        for (const [key, val] of vccVowels) {
          if (lyric.startsWith(key, pos)) { result.push(...val); pos += key.length; matched = true; break; }
        }
        if (matched) continue;
        for (const [key, val] of vccConsonants) {
          if (lyric.startsWith(key, pos)) { result.push(...val); pos += key.length; matched = true; break; }
        }
        if (matched) continue;
        const phons = ipaToSharptalk(lyric[pos]);
        if (phons.length > 0) result.push(...phons);
        pos++;
      }
      return result.length > 0 ? result : null;
    }

    if (notationType === "japanese") {
      let r = kanaToRomaji(lyric);
      if (!r) r = lyric.toLowerCase();
      if (r === "r" || r === "-" || r === "_") return null;
      if (r === "q") return [];
      const phons = romajiToPhonemes(r);
      if (phons !== null) return phons;
    }

    if (notationType === "arpa") {
      const parts = lyric.split(/\s+/).filter(t => t.length > 0);
      const result = [];
      for (const p of parts) {
        const phons = arpa.get(p.replace(RE_ARPA_STRESS, "").toUpperCase());
        if (phons !== undefined) result.push(...phons);
        else unknownLyrics.add(p);
      }
      return result.length > 0 ? result : null;
    }

    if (notationType === "arpa_jp") {
      const parts = lyric.split(/\s+/).filter(t => t.length > 0);
      const result = [];
      for (const p of parts) {
        const phons = arpa_jp.get(p.replace(RE_ARPA_STRESS, "").toUpperCase());
        if (phons !== undefined) result.push(...phons);
        else unknownLyrics.add(p);
      }
      return result.length > 0 ? result : null;
    }

    if (notationType === "xsampa") return ipaToSharptalk(xsampaToIpa(lyric));

    if (notationType === "english") {
      unknownLyrics.add(lyric);
      return null;
    }

    if (notationType === "ipa" || notationType === "japanese") {
      const phons = ipaToSharptalk(lyric);
      if (phons.length > 0) return phons;
    }

    unknownLyrics.add(lyric);
    return null;
  }

  function buildNote(fields) {
    let pbsValue = 0.0;
    const pbsRaw = fields["PBS"];
    if (pbsRaw && pbsRaw.includes(";")) {
      const pv = parseFloat(pbsRaw.split(";")[1]);
      if (!isNaN(pv)) pbsValue = pv;
    }

    const pby = [];
    const pbyRaw = fields["PBY"];
    if (pbyRaw) {
      for (const tok of pbyRaw.split(",")) {
        const d = parseFloat(tok.trim());
        pby.push(isNaN(d) ? 0.0 : d);
      }
    }

    const noteNum   = parseInt(fields["NoteNum"])   || 60;
    const length    = parseInt(fields["Length"])    || 480;
    const intensity = parseInt(fields["Intensity"]) || 100;
    let noteTempo = null;
    if (fields["Tempo"]) {
      const nt = parseFloat(fields["Tempo"]);
      if (!isNaN(nt) && nt > 0) noteTempo = nt;
    }

    return { lyric: fields["Lyric"] || "", noteNum, length, intensity, pbsValue, pby, tempo: noteTempo };
  }

  function parseUst(text, language) {
    const notes = [], allLyrics = [];
    let current = {}, section = null, tempo = 120.0;

    text = text.replace(/\r\n/g, "\n").replace(/\r/g, "\n");

    for (const rawLine of text.split("\n")) {
      const line = rawLine.trim();
      if (!line) continue;

      if (line.startsWith("[") && line.endsWith("]")) {
        if (section !== null &&
            (RE_NOTE_SECTION.test(section) || section.toUpperCase() === "#SETTING")) {
          if ("Lyric" in current || "Tempo" in current) {
            if (RE_NOTE_SECTION.test(section)) notes.push(buildNote(current));
            if ("Lyric" in current) allLyrics.push(current["Lyric"]);
            current = {};
          }
        }
        section = line.slice(1, -1);
      } else if (line.includes("=") && section !== null) {
        const eq = line.indexOf("=");
        const key = line.slice(0, eq).trim();
        const val = line.slice(eq + 1).trim();
        if (section.toUpperCase() === "#SETTING" && key.toLowerCase() === "tempo") {
          const t = parseFloat(val);
          if (!isNaN(t)) tempo = t;
        } else {
          current[key] = val;
        }
      }
    }

    if ("Lyric" in current) {
      notes.push(buildNote(current));
      allLyrics.push(current["Lyric"] || "");
    }

    const notationType = (!language || language === "auto")
      ? detectNotation(allLyrics) : language;
    return { notes, tempo, notationType };
  }

  function buildDiagnostics(notationType, tempo, phonemeCounts, unknownLyrics) {
    let total = 0;
    for (const v of phonemeCounts.values()) total += v;
    let out = `Language: ${notationType} | Tempo: ${Math.round(tempo)} BPM | ${total} phonemes\n`;

    for (const cat of phonCategories) {
      let catTotal = 0;
      const details = [];
      for (const p of cat.members) {
        const cnt = phonemeCounts.get(p) || 0;
        catTotal += cnt;
        if (cnt > 0) {
          const tok = phonToKlattsch.get(p) || p.toUpperCase();
          details.push(`${tok}:${cnt}`);
        }
      }
      if (catTotal === 0) { out += `${cat.label}: (none)\n`; continue; }
      out += `${cat.label}: ${catTotal}  (${details.join("  ")})\n`;
    }

    out += unknownLyrics.size > 0
      ? "Unknown lyrics: " + [...unknownLyrics].sort().join(", ") + "\n"
      : "Unknown lyrics: (none)\n";

    if (notationType === "english")
      out += "Note: espeak-ng unavailable — English lyrics show as unknown\n";

    return out.trimEnd();
  }


  function convert(ustText, language, noteOffset, compatBank) {
    language   = language   || "auto";
    noteOffset = noteOffset || 0;
    compatBank = compatBank || null;

    const compatAuto = compatBank === "auto";
    let { notes, tempo, notationType } = parseUst(ustText, language);

    if (!notationType || notationType === "auto" || notationType === "auto-detect")
      notationType = detectNotation(notes.map(n => n.lyric));

    if (compatAuto)
      compatBank = (notationType === "japanese" || notationType === "arpa_jp")
        ? "ja-mokhtari-2000" : null;

    const parts = [];
    const phonemeCounts = new Map();
    const unknownLyrics = new Set();
    let curNote = null, curRate = -1, lastPhoneme = null;
    let i = 0;

    while (i < notes.length) {
      const note = notes[i];
      if (note.tempo !== null) tempo = note.tempo;
      let durMs = ticksToMs(note.length, tempo);
      const noteName = midiToNoteName(note.noteNum + noteOffset);
      const baseHz   = midiToHz(note.noteNum + noteOffset);

      let j = i + 1;
      const mergedPby = note.pby.slice();
      while (j < notes.length && isExtension(notes[j].lyric)) {
        durMs += ticksToMs(notes[j].length, tempo);
        if (notes[j].pby.length > 0) { mergedPby.length = 0; mergedPby.push(...notes[j].pby); }
        j++;
      }

      if (isRest(note.lyric)) {
        if (durMs > 0) parts.push(`p=${durMs}`);
        lastPhoneme = null;
      } else {
        const phonemes = lyricToPhonemes(note.lyric, notationType, unknownLyrics);
        if (phonemes === null) {
          lastPhoneme = null;
        } else if (phonemes.length === 0) {
          if (durMs > 0) parts.push(`p=${durMs}`);
          lastPhoneme = null;
        } else {
          for (const p of phonemes) phonemeCounts.set(p, (phonemeCounts.get(p) || 0) + 1);

          const mapped = phonemes.map(p => mapPhoneme(p, compatBank));

          if (mapped.length > 0) {
            const d = diphthongs.get(mapped[mapped.length - 1]);
            if (d) {
              const nextLyric = j < notes.length ? notes[j].lyric : null;
              if (nextLyric && !isRest(nextLyric) && !isExtension(nextLyric)) {
                const nextPhons = lyricToPhonemes(nextLyric, notationType, new Set());
                if (nextPhons && nextPhons.length > 0) {
                  const nextFirst = mapPhoneme(nextPhons[0], compatBank);
                  if (nextFirst === mapped[mapped.length - 1] || nextFirst === d.terminal)
                    mapped[mapped.length - 1] = d.nucleus;
                }
              }
            }
          }

          const klattsch = [];
          for (let k = 0; k < mapped.length; k++) {
            let p = mapped[k];
            if (k === 0 && lastPhoneme !== null) {
              const dslur = diphthongs.get(p);
              if (dslur && dslur.nucleus === lastPhoneme) p = dslur.terminal;
            }
            klattsch.push(p);
            lastPhoneme = p;
          }

          if (noteName !== curNote) { parts.push(`b=${noteName}`); curNote = noteName; }
          if (durMs !== curRate)    { parts.push(`r=${durMs}`);    curRate = durMs; }

          if (Math.abs(note.pbsValue) >= PITCH_CENTS_THRESHOLD) {
            const mod = pitchModifier(baseHz, note.pbsValue, true);
            if (mod) klattsch[0] += mod;
          }

          const endCents = mergedPby.length > 0 ? mergedPby[mergedPby.length - 1] : 0.0;
          if (Math.abs(endCents) >= PITCH_CENTS_THRESHOLD) {
            const mod = pitchModifier(baseHz, endCents, false);
            if (mod) klattsch[klattsch.length - 1] += mod;
          }

          parts.push("( " + klattsch.join(" ") + " )");
        }
      }
      i = j;
    }

    const prefix = compatBank ? `[bank=${compatBank}] ` : "";
    return {
      klattsch:    prefix + parts.join(" "),
      diagnostics: buildDiagnostics(notationType, tempo, phonemeCounts, unknownLyrics),
    };
  }

  global.UstConverter = { convert };

})(window);
