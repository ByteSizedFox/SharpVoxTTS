# SharpVox Docs

## Basic Usage
With SharpVox, you can use it like a basic Text-To-Speech engine, you type words and it speaks them. it knows many words and can speak them using it's phonetic dictionary, which is based on CMUDict.

## Simple Singing
you can sing without using the advanced klattsch parser using the simple singing notation
```
[PHONEMES<durationinms,NOTE_NAME>PHONEMES<etc....>]
```
for example
```
[WIY<1000,C4>NER] 
```
says WEEENER with the E being one second long and the other phonemes running at their minimum duration.
The angled brackets containing the duration and note only apply to the phoneme directly to the left of it, the duration is specific to that one phoneme, but pitches are inherited by the next phoneme until changed.
## Advanced Singing
generally speaking, advanced singing follows the klattsch docs, as the klattsch mode was designed to be nearly 1:1 compatible, I'll add the klattsch token docs below:
```
tokens (whitespace-separated)
  PHONEME              ARPABET code (AY, IH, AA, S, ...)
  PHONEME!             stressed (transient: +8 Hz lift, longer duration)
  PHONEME+N            rising pitch glide of N Hz; new pitch sticks
  PHONEME-N            falling pitch glide of N Hz; new pitch sticks
  PHONEME(+N)          transient rise: ornament only, doesn't carry forward
  PHONEME(-N)          transient fall
  ( PHONEMES )         syllable group: phonemes share one rate slot
  ,  ;  .              short / medium / long pause (100/200/300 ms)
  bN     b=N           absolute base F0 in Hz (also accepts notes:
                       bC4, bC#5, bDb3, bA-1, etc.)
  b+N    b-N           relative base F0 (running pitch shifts by N)
  b                    reset base F0 to the utterance's starting value
  rN     r=N           absolute per-phoneme rate in ms
  r+N    r-N           relative rate (positive = slower)
  r                    reset rate to opts default
  pN     p=N           insert exact N-ms pause
  sN     s=N           absolute formant scale (1.0 = male baseline,
                       1.17 = typical female, 1.3 = child)
  s+N    s-N           relative scale shift (decimal: s+0.1, s-0.05)
  s                    reset scale to opts default
  vN                   vibrato depth in Hz (peak deviation; 0 = off)
  v+N    v-N           relative depth shift
  v                    reset vibrato depth
  wN                   vibrato rate in Hz (LFO frequency, default 5)
  w+N    w-N    w      relative / reset
  mN                   tremolo depth (0..1; amplitude modulation)
  m+N    m-N    m      relative / reset
  nN                   tremolo rate in Hz (default 5)
  n+N    n-N    n      relative / reset
  hN                   breathiness / aspiration mix (0..1)
  h+N    h-N    h      relative / reset
  tN                   spectral tilt (-0.9 darker .. +0.9 brighter)
  t+N    t-N    t      relative / reset
  gN                   vocal effort (0 lax .. 1 tense; 0.5 default)
  g+N    g-N    g      relative / reset
  [base=N] etc.        verbose form, equivalent to bN, rN, pN, sN
  [bank=NAME]          switch active phoneme bank for subsequent phonemes
  [bank]               reset active phoneme bank to the initial selection
  # rest of line       comment
  /* ... */            block comment (can span multiple lines)

f0 evolution
  stress (!) is transient: doesn't carry forward
  bare pitch deltas +N/-N are sticky: mutates the running pitch
  parenthesized deltas (+N)/(-N) are transient: this phoneme only
  bN (or bC4 etc.) is an absolute reset to that pitch
  bare b returns to the utterance's starting pitch
  examples:
    "AY+20 D IH+10 D"   D at base+20, IH at base+30, D at base+30
    "AY(+20) D IH(+10) D"  D at base, IH at base, D at base (ornaments)
    "AY+20 D IH(+10) D" D at base+20, IH peaks at +30, D back at +20

examples
  HH AH L OW                       hello, default voice
  b140 HH AH L OW                  higher voice
  bA3 HH AH L OW                   higher voice (note name)
  AY+15 D IH D                     "I did" with rise; D and IH stay raised
  AY-15 . b120 AY+10               fall, reset, then rise
  D IH D DH AE(+40) T              "did THAT" with accent ornament on AE
```
Note: the bank commands are unsupported and simply do nothing in SharpVox


## Mode Switching
while you have the klattsch tab in the webUI so you don't have to manually use the klattsch mode commands, you can actively swap modes in a single clause in the Text-To-Speech mode by using the `[:klattsch on/off]` command:
```
I will sing Now:
[dow<500,C4>rey<500,D4>miy<500,E4>fah<500,F4>sow<500,G4>lah<500,A4>tiy<500,B4>dow<500,C5>]
.
I am speaking Text
[:klattsch on]
# now i am speaking with klattsch
N AW AY AE M S P IY K IX NG W IH DH K L AE T CH
[:klattsch off]
now i am speaking with text again
```
Notes:
- you may notice you can use a hashtag for comment's in klattsch mode like in python, though this does not apply to text mode.
- The period at the end of the simple singing mode is used to add a pause between the singing and text.
- simple singing mode does not require a mode switch as the engine detects when you want to use it, klattsch mode is it's own mode since it isn't as compatible with the other modes inline.

## Custom Voices
You can design custom voices using both the JSON files and the WebUI sliders (which can export and import as JSON).
additionally there is inline voice changing support with the `[:voice NAME]` command
for instance
```
[:voice matt]
hi, I am matt.
[:voice beth]
And I am beth!
```
When you change voices, that voice stays until otherwise changed.

Note: the name swapping works in both klattsch mode and Text-To-Speech Mode
The available default voices are:
`beth, chris, deborah, jack, jess,
  john, matt, pirate, tommy, and whisper`
  
Additionally, you can design custom voices inline if a preset does not fit your needs, you may notice that the WebUI has a `[:custom]` button that copies a string of text to your clipboard, this string contains your current voice config in text format, it follows the pattern of:
```
[:custom PARAM_NAME PARAM_VALUE PARAM_NAME PARAM VALUE etc...]
```
here's an example of the Jack voice:
```
[:custom pitch 310 rate 155 tract 1.02 vgain 58 again 185 f4freq 3720 f4bw 275 f5freq 4280 f5bw 315 f4pfreq 3714 f4pbw 157 f5pfreq 4210 f5pbw 92 f6pfreq 4503 nasalbase 329 nasaltarg 404 nasalbw 67 ngain 101 bwgain1 126 bwgain2 107 bwgain3 118 pitchrange 80 intonation 106 riseamt 34 fallamt -17 baselinefall 47]
```
Parameters at their default value removed from the output, for effects like breathiness, vocal fry, or vibrato, look at the WebUI sliders which share the same names.

you can use this to have a more customized voice when you need to swap voices in a single clause.

## Advice and Tips
I personally recommend in simple singing mode to only apply long duration to vowels. Consonants, fricatives, and plosives aren't designed to be run extended unless you truly intend to use them for effect. I recommend placing them in the beginning or middle of phoneme clusters, splitting at the non-vowel and moving it to after the angle bracket section, for example:
```
DO:
[WAH<1000,C4>CH]
DONT:
[WAHCH<1000,C4>]
```

The phoneme sets used in both singing mode and klattsch mode are standard US arpabet with Japanese vowel extensions included. There are various WebUI converters for your convenience to assist in cover-making.

A final hint, the phoneme tracker in TTS mode is your friend, no need to write words yourself when the TTS can tell you what phonemes are in words.

# Extra Inline Commands
you have:
`[:rate VALUE] [:pitch VALUE] [:volume VALUE]`
which set the value respective to the command name inline.
They take effect from the point in which they are in the input string.
