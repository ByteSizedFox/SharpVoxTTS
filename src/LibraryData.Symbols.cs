#nullable enable
using static SharpTalk.AudioProcessor;
namespace SharpTalk {
    internal static partial class LibraryData {
        // Short phoneme aliases - thin wrappers around AudioProcessor constants.
        private static class Ph {
            internal const byte
                IY = (byte)_IY_, IH = (byte)_IH_, EH = (byte)_EH_, AE = (byte)_AE_,
                IX = (byte)_IX_, AX = (byte)_AX_, ER = (byte)_ER_, AH = (byte)_AH_,
                AA = (byte)_AA_, AO = (byte)_AO_, UH = (byte)_UH_, UW = (byte)_UW_,
                EY = (byte)_EY_, AY = (byte)_AY_, OY = (byte)_OY_, AW = (byte)_AW_, OW = (byte)_OW_,
                AR = (byte)_AR_,
                M = (byte)_M_, N = (byte)_N_, NG = (byte)_NG_, W = (byte)_W_,
                Y = (byte)_Y_, R = (byte)_R_, L = (byte)_L_,
                HH = (byte)_HH_,
                F = (byte)_F_, V = (byte)_V_, TH = (byte)_TH_, DH = (byte)_DH_,
                S = (byte)_S_, Z = (byte)_Z_, SH = (byte)_SH_, ZH = (byte)_ZH_,
                P = (byte)_P_, B = (byte)_B_, T = (byte)_T_, D = (byte)_D_,
                K = (byte)_K_, G = (byte)_G_, CH = (byte)_CH_, JH = (byte)_JH_,
                S1 = kOpStress1, S2 = kOpStress2;
        }

        // Number and symbol pronunciation table.
        // Used by Phonemizer.AppendSymbol for number-to-speech conversion.
        internal static readonly System.Collections.Generic.IReadOnlyDictionary<string, byte[]>
            SymbolsTable = new System.Collections.Generic.Dictionary<string, byte[]> {
                // digits
                ["0"] = [Ph.Z, Ph.S1, Ph.IH, Ph.R, Ph.S2, Ph.OW],           // zero
                ["1"] = [Ph.W, Ph.S1, Ph.AH, Ph.N],                         // one
                ["2"] = [Ph.T, Ph.S1, Ph.UW],                              // two
                ["3"] = [Ph.TH, Ph.R, Ph.S1, Ph.IY],                        // three
                ["4"] = [Ph.F, Ph.S1, Ph.AO, Ph.R],                         // four
                ["5"] = [Ph.F, Ph.S1, Ph.AY, Ph.V],                         // five
                ["6"] = [Ph.S, Ph.S1, Ph.IH, Ph.K, Ph.S],                   // six
                ["7"] = [Ph.S, Ph.S1, Ph.EH, Ph.V, Ph.AX, Ph.N],             // seven
                ["8"] = [Ph.S1, Ph.EY, Ph.T],                               // eight
                ["9"] = [Ph.N, Ph.S1, Ph.AY, Ph.N],                         // nine

                // teens
                ["10"] = [Ph.T, Ph.S1, Ph.EH, Ph.N],                         // ten
                ["11"] = [Ph.IH, Ph.L, Ph.S1, Ph.EH, Ph.V, Ph.AX, Ph.N],      // eleven
                ["12"] = [Ph.T, Ph.W, Ph.S1, Ph.EH, Ph.L, Ph.V],             // twelve
                ["13"] = [Ph.TH, Ph.S2, Ph.ER, Ph.T, Ph.S1, Ph.IY, Ph.N],      // thirteen
                ["14"] = [Ph.F, Ph.S2, Ph.AO, Ph.R, Ph.T, Ph.S1, Ph.IY, Ph.N],// fourteen
                ["15"] = [Ph.F, Ph.S2, Ph.IH, Ph.F, Ph.T, Ph.S1, Ph.IY, Ph.N],// fifteen
                ["16"] = [Ph.S, Ph.S2, Ph.IH, Ph.K, Ph.S, Ph.T, Ph.S1, Ph.IY, Ph.N], // sixteen
                ["17"] = [Ph.S, Ph.S2, Ph.EH, Ph.V, Ph.IX, Ph.N, Ph.T, Ph.S1, Ph.IY, Ph.N], // seventeen
                ["18"] = [Ph.S2, Ph.EY, Ph.T, Ph.S1, Ph.IY, Ph.N],             // eighteen
                ["19"] = [Ph.N, Ph.S2, Ph.AY, Ph.N, Ph.T, Ph.S1, Ph.IY, Ph.N],// nineteen

                // tens
                ["20"] = [Ph.T, Ph.W, Ph.S1, Ph.EH, Ph.N, Ph.T, Ph.IY],     // twenty
                ["30"] = [Ph.TH, Ph.S1, Ph.ER, Ph.D, Ph.IY],                  // thirty
                ["40"] = [Ph.F, Ph.S1, Ph.AO, Ph.R, Ph.D, Ph.IY],            // forty
                ["50"] = [Ph.F, Ph.S1, Ph.IH, Ph.F, Ph.T, Ph.IY],            // fifty
                ["60"] = [Ph.S, Ph.S1, Ph.IH, Ph.K, Ph.S, Ph.T, Ph.IY],     // sixty
                ["70"] = [Ph.S, Ph.S1, Ph.EH, Ph.V, Ph.IX, Ph.N, Ph.T, Ph.IY], // seventy
                ["80"] = [Ph.S1, Ph.EY, Ph.D, Ph.IY],                        // eighty
                ["90"] = [Ph.N, Ph.S1, Ph.AY, Ph.N, Ph.T, Ph.IY],            // ninety

                // scale words
                ["100"] = [Ph.HH, Ph.S1, Ph.AH, Ph.N, Ph.D, Ph.R, Ph.IX, Ph.D],    // hundred
                ["1E1"] = [Ph.TH, Ph.S1, Ph.AW, Ph.Z, Ph.IX, Ph.N, Ph.D],          // thousand
                ["1E2"] = [Ph.M, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N],          // million
                ["1E3"] = [Ph.B, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N],          // billion
                ["1E4"] = [Ph.T, Ph.R, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N],    // trillion
                ["1E5"] = [Ph.K, Ph.W, Ph.AA, Ph.D, Ph.R, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N], // quadrillion
                ["1E6"] = [Ph.K, Ph.W, Ph.IH, Ph.N, Ph.T, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N], // quintillion
                ["1E7"] = [Ph.S, Ph.EH, Ph.K, Ph.S, Ph.T, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N], // sextillion
                ["1E8"] = [Ph.S, Ph.EH, Ph.P, Ph.T, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N],       // septillion
                ["1E9"] = [Ph.AA, Ph.K, Ph.T, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N],             // octillion
                ["1E10"] = [Ph.N, Ph.AA, Ph.N, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N],             // nonillion
                ["1E11"] = [Ph.D, Ph.EH, Ph.S, Ph.S1, Ph.IH, Ph.L, Ph.Y, Ph.IX, Ph.N],             // decillion

                // letters
                ["A"] = [Ph.S1, Ph.EY],
                ["B"] = [Ph.B, Ph.S1, Ph.IY],
                ["C"] = [Ph.S, Ph.S1, Ph.IY],
                ["D"] = [Ph.D, Ph.S1, Ph.IY],
                ["E"] = [Ph.S1, Ph.IY],
                ["F"] = [Ph.S1, Ph.EH, Ph.F],
                ["G"] = [Ph.JH, Ph.S1, Ph.IY],
                ["H"] = [Ph.S1, Ph.EY, Ph.CH],
                ["I"] = [Ph.AY],
                ["J"] = [Ph.JH, Ph.S1, Ph.EY],
                ["K"] = [Ph.K, Ph.S1, Ph.EY],
                ["L"] = [Ph.S1, Ph.EH, Ph.L],
                ["M"] = [Ph.S1, Ph.EH, Ph.M],
                ["N"] = [Ph.S1, Ph.EH, Ph.N],
                ["O"] = [Ph.S1, Ph.OW],
                ["P"] = [Ph.P, Ph.S1, Ph.IY],
                ["Q"] = [Ph.K, Ph.Y, Ph.S1, Ph.UW],
                ["R"] = [Ph.S1, Ph.AR],
                ["S"] = [Ph.S1, Ph.EH, Ph.S],
                ["T"] = [Ph.T, Ph.S1, Ph.IY],
                ["U"] = [Ph.Y, Ph.S1, Ph.UW],
                ["V"] = [Ph.V, Ph.S1, Ph.IY],
                ["W"] = [Ph.D, Ph.S1, Ph.AH, Ph.B, Ph.AX, Ph.L, 60, Ph.Y, Ph.S2, Ph.UW], // "double-you"
                ["X"] = [Ph.S1, Ph.EH, Ph.K, Ph.S],
                ["Y"] = [Ph.W, Ph.S1, Ph.AY],
                ["Z"] = [Ph.Z, Ph.S1, Ph.IY],

                // symbols
                [" "] = [Ph.S, Ph.P, Ph.EY, Ph.S],                            // space
                ["!"] = [Ph.EH, Ph.K, Ph.S, Ph.K, Ph.L, Ph.AX, Ph.M, Ph.S1, Ph.EY, Ph.SH, Ph.AX, Ph.N], // exclamation
                ["\""] = [Ph.K, Ph.W, Ph.S1, Ph.OW, Ph.T],                     // quote
                ["#"] = [Ph.N, Ph.S1, Ph.AH, Ph.M, Ph.B, Ph.ER],              // number
                ["$"] = [Ph.D, Ph.S1, Ph.AA, Ph.L, Ph.ER, Ph.Z],              // dollars
                ["%"] = [Ph.P, Ph.ER, Ph.S, Ph.S1, Ph.EH, Ph.N, Ph.T],        // percent
                ["&"] = [Ph.S2, Ph.EH, Ph.N, Ph.D],                            // and
                ["'"] = [Ph.AX, Ph.P, Ph.S1, Ph.AA, Ph.S, Ph.T, Ph.R, Ph.AX, Ph.F, Ph.IY], // apostrophe
                ["("] = [Ph.S1, Ph.OW, Ph.P, Ph.AX, Ph.N, 60, Ph.P, Ph.AX, Ph.R, Ph.S1, Ph.EH, Ph.N, Ph.TH, Ph.AX, Ph.S, Ph.IH, Ph.S], // open parenthesis
                [")"] = [Ph.K, Ph.L, Ph.S1, Ph.OW, Ph.Z, 60, Ph.P, Ph.AX, Ph.R, Ph.S1, Ph.EH, Ph.N, Ph.TH, Ph.AX, Ph.S, Ph.IH, Ph.S], // close parenthesis
                ["*"] = [Ph.S1, Ph.AE, Ph.S, Ph.T, Ph.AX, Ph.R, Ph.IH, Ph.S, Ph.K], // asterisk
                ["+"] = [Ph.P, Ph.L, Ph.S1, Ph.AH, Ph.S],                     // plus
                [","] = [Ph.K, Ph.S1, Ph.AA, Ph.M, Ph.AX],                    // comma
                ["-"] = [Ph.HH, Ph.S1, Ph.AY, Ph.F, Ph.AX, Ph.N],              // hyphen
                ["."] = [Ph.P, Ph.S1, Ph.IH, Ph.R, Ph.IY, Ph.AX, Ph.D],       // period
                ["/"] = [Ph.S, Ph.L, Ph.S1, Ph.AE, Ph.SH],                    // slash
                [":"] = [Ph.K, Ph.S1, Ph.OW, Ph.L, Ph.AX, Ph.N],              // colon
                [";"] = [Ph.S, Ph.S1, Ph.EH, Ph.M, Ph.IY, 60, Ph.K, Ph.OW, Ph.L, Ph.AX, Ph.N], // semicolon
                ["<"] = [Ph.L, Ph.S1, Ph.EH, Ph.S, 60, Ph.DH, Ph.S1, Ph.AE, Ph.N],              // less than
                ["="] = [Ph.S1, Ph.IY, Ph.K, Ph.W, Ph.AX, Ph.L, Ph.Z],        // equals
                [">"] = [Ph.G, Ph.R, Ph.S1, Ph.EY, Ph.T, Ph.ER, 60, Ph.DH, Ph.S2, Ph.AE, Ph.N], // greater than
                ["?"] = [Ph.K, Ph.W, Ph.S1, Ph.EH, Ph.S, Ph.CH, Ph.AX, Ph.N], // question
                ["@"] = [Ph.S1, Ph.AE, Ph.T],                                    // at
                ["["] = [Ph.S1, Ph.OW, Ph.P, Ph.AX, Ph.N, 60, Ph.B, Ph.R, Ph.S1, Ph.AE, Ph.K, Ph.IH, Ph.T], // open bracket
            };
    }
}
