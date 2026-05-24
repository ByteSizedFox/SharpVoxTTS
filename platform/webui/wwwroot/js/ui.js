window.ui = {
    _getCookie: (name) => {
        const m = document.cookie.match('(?:^|;)\\s*' + name + '=([^;]*)');
        return m ? decodeURIComponent(m[1]) : null;
    },
    _setCookie: (name, val) => {
        const d = new Date();
        d.setTime(d.getTime() + 365 * 864e5);
        document.cookie = name + '=' + encodeURIComponent(val) + ';expires=' + d.toUTCString() + ';path=/';
    },
    _collectVoiceJson: () => {
        const intMap = {
            'Rate':'Rate','PitchHz':'PitchHz','PitchOffsetHz':'PitchOffsetHz',
            'VGain':'VoicingGain','AGain':'AspirationGain','ACycle':'AspirationCycle',
            'TremoloDepth':'TremoloDepth','TremoloRate':'TremoloRate',
            'Jitter':'Jitter','Shimmer':'Shimmer','Diplophonia':'Diplophonia',
            'FryAmount':'FryAmount','SubglottalAmt':'SubglottalAmt','BreathAmt':'BreathAmt','OpenQuotient':'OpenQuotient','OQStressLink':'OQStressLink','OQF0Link':'OQF0Link','OnsetHardness':'OnsetHardness',
            'LarynxOffset':'LarynxOffset','PharyngealAmt':'PharyngealAmt','LipRounding':'LipRounding',
            'NGain':'NGain',
            'F4Freq':'F4Freq','F4BW':'F4BW','F5Freq':'F5Freq','F5BW':'F5BW',
            'F4pFreq':'F4pFreq','F4pBW':'F4pBW','F5pFreq':'F5pFreq','F5pBW':'F5pBW',
            'F6pFreq':'F6pFreq','F6pBW':'F6pBW',
            'BwGain1':'BwGain1','BwGain2':'BwGain2','BwGain3':'BwGain3',
            'NasalBase':'NasalBase','NasalTarg':'NasalTarg','NasalBW':'NasalBW',
            'PitchRange':'PitchRange','StressGain':'StressGain','Intonation':'Intonation',
            'RiseAmt':'RiseAmt','FallAmt':'FallAmt','BaselineFall':'BaselineFall',
            'UptalkAmt':'UptalkAmt','StressEarly':'StressEarly','BreakStrength':'BreakStrength',
            'EmphasisBoost':'EmphasisBoost','VocalConfidence':'VocalConfidence',
        };
        const obj = {};
        for (const [domId, key] of Object.entries(intMap)) {
            const el = document.getElementById(`v-${domId}`);
            if (el) obj[key] = parseInt(el.value);
        }
        const ts = document.getElementById('v-TractScale');
        if (ts) obj.TractScale = parseFloat(ts.value);
        const cb = document.getElementById('voice-female');
        if (cb) obj.VoiceType = cb.checked ? 1 : 0;
        return JSON.stringify(obj);
    },
    _markCustom: () => {
        const sel = document.getElementById('presetSelect');
        if (sel) sel.value = 'custom';
        window.ui._setCookie('st_custom_voice', window.ui._collectVoiceJson());
    },

    // UI Event Handlers
    onParamInput: (name, value, isFloat) => {
        const display = document.getElementById(`v-${name}-val`);
        if (display) {
            if (isFloat) {
                const f = parseFloat(value);
                display.innerText = f.toLocaleString(undefined, { minimumFractionDigits: 0, maximumFractionDigits: 2 });
            } else {
                display.innerText = value;
            }
        }
        window.sharpTalk?.UpdateParam(name, value);
        if (name !== 'sampleRate' && name !== 'OutputVolume' && !name.startsWith('kl'))
            window.ui._markCustom();
    },

    updateDesc: (title, desc) => {
        document.getElementById('activeTitle').innerText = title;
        document.getElementById('activeDesc').innerText = desc;
    },

    speak: () => {
        const text = document.getElementById('inputText').value;
        window.sharpTalk?.Speak(text);
    },

    stop: () => {
        window.sharpTalk?.StopBtn();
    },

    setMode: (mode) => {
        const klattsch = mode === 'klattsch';
        const ust = mode === 'ust';
        const tts = mode === 'tts';

        document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
        document.getElementById('tab-' + mode).classList.add('active');

        const klElems = document.querySelectorAll('.kl-only');
        klElems.forEach(el => el.style.display = klattsch ? 'block' : 'none');

        const ustElems = document.querySelectorAll('.ust-only');
        ustElems.forEach(el => el.style.display = ust ? 'block' : 'none');

        const inputTextArea = document.getElementById('inputText');
        inputTextArea.style.display = ust ? 'none' : 'block';

        if (klattsch) {
            inputTextArea.placeholder = "HH AH L OW\nb140 AY+15 D IH D ...";
        } else if (tts) {
            inputTextArea.placeholder = "Enter text to speak...";
        }

        window.sharpTalk?.SetMode(klattsch || ust);
    },

    onUstFileSelected: () => {
        const input = document.getElementById('ustFile');
        const span = document.getElementById('ustFileName');
        if (input.files.length > 0) {
            span.innerText = input.files[0].name;
        } else {
            span.innerText = "no file selected";
        }
    },

    convertUst: async () => {
        const input = document.getElementById('ustFile');
        if (input.files.length === 0) {
            window.ui.updateStatus("Please select a .ust file first.");
            return;
        }

        window.ui.updateStatus("converting ust...");
        const file = input.files[0];
        const buffer = await file.arrayBuffer();
        const bytes = new Uint8Array(buffer);

        // Detect encoding in JS, browser TextDecoder has native Shift-JIS support
        let text;
        try { text = new TextDecoder('utf-8', { fatal: true }).decode(bytes); } catch(e) {
        try { text = new TextDecoder('shift-jis', { fatal: true }).decode(bytes); } catch(e) {
            text = new TextDecoder('iso-8859-1').decode(bytes);
        }}

        const language = document.getElementById('ustLanguage').value;
        const offset = parseInt(document.getElementById('ustOffset').value) || 0;
        const compat = document.getElementById('ustCompat').checked;
        let bank = document.getElementById('ustBank').value;

        if (bank === "none") {
            bank = compat ? "auto" : null;
        }

        const resultJson = window.sharpTalk?.ConvertUst(text, language, offset, bank);
        if (resultJson) {
            const result = JSON.parse(resultJson);
            document.getElementById('inputText').value = result.klattsch;
            document.getElementById('ustDiagnostics').innerText = result.diagnostics;
            window.ui.updateStatus("conversion complete — klattsch ready");
            window.ui.setMode('klattsch');
        } else {
            window.ui.updateStatus("conversion failed");
        }
    },

    onPresetChange: async (val) => {
        if (val === 'custom') {
            const json = window.ui._getCookie('st_custom_voice');
            if (json) window.sharpTalk?.HandleImport(json);
            return;
        }
        if (val === 'baseline') {
            window.sharpTalk?.OnPresetChange(val);
        } else {
            try {
                const resp = await fetch(`voices/${val}.json`);
                if (resp.ok) {
                    const json = await resp.text();
                    window.sharpTalk?.HandleImport(json);
                } else {
                    // Fallback to C# hardcoded presets if fetch fails (like whisper)
                    window.sharpTalk?.OnPresetChange(val);
                }
            } catch (e) {
                window.sharpTalk?.OnPresetChange(val);
            }
        }
    },

    toggleFemale: (checked) => {
        window.sharpTalk?.UpdateParam('VoiceType', checked ? "1" : "0");
        window.ui._markCustom();
    },

    exportPreset: () => {
        window.sharpTalk?.ExportPreset();
    },

    importPreset: () => {
        window.ui.triggerFileInput('importFile');
    },

    handleImport: async () => {
        const json = await window.ui.readFileContent('importFile');
        if (json) {
            window.sharpTalk?.HandleImport(json);
        }
    },

    downloadWav: () => {
        const text = document.getElementById('inputText').value;
        window.sharpTalk?.DownloadWav(text);
    },

    auditionPhoneme: (code) => {
        window.sharpTalk?.AuditionPhoneme(code);
    },

    renderVideo: () => {
        const text = document.getElementById('inputText').value;
        window.sharpTalk?.ExportVideo(text);
    },

    startVideoExport: async (pcmBytes, sampleRate, eventsJson, timesJson, wordTimesJson, duration, sourceText, lipsyncTimesJson, lipsyncV1Json, lipsyncV2Json) => {
        const events = JSON.parse(eventsJson);
        const times = JSON.parse(timesJson);
        const wordTimes = JSON.parse(wordTimesJson);
        const lipsyncTimes = JSON.parse(lipsyncTimesJson);
        const lipsyncV1 = JSON.parse(lipsyncV1Json);
        const lipsyncV2 = JSON.parse(lipsyncV2Json);
        
        window.ui.updateStatus("starting video export...");
        
        const W = 1280, H = 720;
        const FPS = 30;

        const canvas = document.createElement('canvas');
        canvas.width = W; canvas.height = H;
        Object.assign(canvas.style, {
            position: 'fixed', bottom: '1rem', right: '1rem',
            width: '320px', height: 'auto',
            border: '1px solid #333', borderRadius: '3px',
            boxShadow: '0 4px 16px rgba(0,0,0,0.5)',
            background: '#050510',
            zIndex: 9999,
        });
        document.body.appendChild(canvas);
        const cctx = canvas.getContext('2d');

        const specCanvas = document.createElement('canvas');
        specCanvas.width = W; specCanvas.height = H;
        const sctx = specCanvas.getContext('2d');
        sctx.fillStyle = '#050510';
        sctx.fillRect(0, 0, W, H);

        const actx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate });
        const analyser = actx.createAnalyser();
        analyser.fftSize = 2048;
        analyser.smoothingTimeConstant = 0.2;
        analyser.minDecibels = -90;
        analyser.maxDecibels = -20;

        const dest = actx.createMediaStreamDestination();
        analyser.connect(dest);

        const pcmData = new Int16Array(pcmBytes.buffer, pcmBytes.byteOffset, pcmBytes.byteLength >> 1);
        const floatData = new Float32Array(pcmData.length);
        for (let i = 0; i < pcmData.length; i++) floatData[i] = pcmData[i] / 32768.0;

        const audioBuffer = actx.createBuffer(1, floatData.length, sampleRate);
        audioBuffer.copyToChannel(floatData, 0);

        const stream = new MediaStream([
            ...canvas.captureStream(FPS).getVideoTracks(),
            ...dest.stream.getAudioTracks(),
        ]);

        const mimeType = ['video/mp4', 'video/webm'].find(m => MediaRecorder.isTypeSupported(m)) || 'video/webm';
        const ext = mimeType.includes('mp4') ? 'mp4' : 'webm';
        const recorder = new MediaRecorder(stream, { 
            mimeType, 
            videoBitsPerSecond: 1_500_000,
            audioBitsPerSecond: 128_000
        });
        const chunks = [];
        recorder.ondataavailable = e => { if (e.data.size > 0) chunks.push(e.data); };

        const freq = new Float32Array(analyser.frequencyBinCount);
        const fMin = 40, fMax = 10000;
        const lnMin = Math.log(fMin), lnRange = Math.log(fMax) - lnMin;
        const yToBinF = new Float32Array(H);
        for (let y = 0; y < H; y++) {
            const t = 1 - y / H;
            const f = Math.exp(lnMin + lnRange * t);
            yToBinF[y] = Math.min(freq.length - 1.001, f * analyser.fftSize / sampleRate);
        }

        const blueStops = [[0, 2, 20], [0, 20, 80], [0, 80, 200], [0, 150, 255], [180, 240, 255]];
        const colorLUT = new Uint8Array(256 * 3);
        for (let i = 0; i < 256; i++) {
            const t = Math.pow(i / 255, 0.8);
            const f = t * (blueStops.length - 1);
            const ii = Math.min(blueStops.length - 2, Math.floor(f));
            const fr = f - ii;
            const a = blueStops[ii], b = blueStops[ii + 1];
            colorLUT[i * 3]     = a[0] + (b[0] - a[0]) * fr;
            colorLUT[i * 3 + 1] = a[1] + (b[1] - a[1]) * fr;
            colorLUT[i * 3 + 2] = a[2] + (b[2] - a[2]) * fr;
        }


        const lipsyncMode = document.getElementById('lipsyncMode')?.value ?? 'ziofox';
        const modelUrls = { ziofox: 'FoxHead.glb', ziowolf: 'WolfHead.glb', ziokitsune: 'KitsuneHead.glb', ziogoat: 'GoatHead.glb', zioredpanda: 'RedPandaHead.glb', ziodeer: 'ZioDeer.glb', ziocat: 'ZioCat.glb' };
        await window.lipsyncRenderer?.loadModel(modelUrls[lipsyncMode]);

        const MAX_CW = 24;
        const colImg = sctx.createImageData(MAX_CW, H);
        const colPx = colImg.data;
        for (let i = 3; i < colPx.length; i += 4) colPx[i] = 255;

        // Display logic for word-wrapped subtitle pixel positions and karaoke highlighting.
        const pad = 60, lh = 52;
        const maxW = W - pad * 2;
        cctx.font = 'bold 36px ui-monospace, monospace';
        const spaceW = cctx.measureText(' ').width;

        const displayLines = [];
        const allLineWords = [];

        sourceText.split('\n').forEach(block => {
            const rawWords = block.trim().split(/\s+/).filter(w => w);
            if (rawWords.length === 0) { displayLines.push({ wordStart: allLineWords.length, words: [] }); return; }
            let lineWordsBuf = [], lineText = "";
            rawWords.forEach(word => {
                const test = lineText ? lineText + " " + word : word;
                if (cctx.measureText(test).width > maxW && lineWordsBuf.length > 0) {
                    // flush current line
                    const wordStart = allLineWords.length;
                    let x = pad;
                    const measured = lineWordsBuf.map(w => { const entry = { text: w, x }; x += cctx.measureText(w).width + spaceW; allLineWords.push({ lineIdx: displayLines.length, text: w, x: entry.x }); return entry; });
                    displayLines.push({ wordStart, words: measured });
                    lineWordsBuf = [word];
                    lineText = word;
                } else {
                    lineWordsBuf.push(word);
                    lineText = test;
                }
            });
            if (lineWordsBuf.length > 0) {
                const wordStart = allLineWords.length;
                let x = pad;
                const measured = lineWordsBuf.map(w => { const entry = { text: w, x }; x += cctx.measureText(w).width + spaceW; allLineWords.push({ lineIdx: displayLines.length, text: w, x: entry.x }); return entry; });
                displayLines.push({ wordStart, words: measured });
            }
        });

        // Clamp wordTimes length to allLineWords so we never over-index
        const wordCount = Math.min(wordTimes.length, allLineWords.length);

        // Binary search: largest index in arr where arr[i] <= val
        const bisect = (arr, val, len) => {
            let lo = 0, hi = len - 1, result = -1;
            while (lo <= hi) {
                const mid = (lo + hi) >> 1;
                if (arr[mid] <= val) { result = mid; lo = mid + 1; } else hi = mid - 1;
            }
            return result;
        };

        const source = actx.createBufferSource();
        source.buffer = audioBuffer;
        source.connect(analyser);

        recorder.start();
        source.start();
        const t0 = performance.now();
        let xLast = 0;
        let scrollY = H - 150;
        let scrollTargetY = H - 150;

        const renderLoop = () => {
            const elapsed = (performance.now() - t0) / 1000;
            const xNow = Math.min(W, (elapsed / duration) * W);

            if (xNow > xLast) {
                analyser.getFloatFrequencyData(freq);
                const cw = Math.min(MAX_CW, Math.max(1, Math.ceil(xNow - xLast)));
                for (let y = 0; y < H; y++) {
                    const bf = yToBinF[y];
                    const i = bf | 0;
                    const frac = bf - i;
                    const dB = freq[i] * (1 - frac) + freq[i + 1] * frac;
                    const norm = Math.max(0, Math.min(1, (dB + 90) / 70));
                    const ci = (norm * 255) | 0;
                    const r = colorLUT[ci * 3], g = colorLUT[ci * 3 + 1], b = colorLUT[ci * 3 + 2];
                    const rowBase = y * MAX_CW * 4;
                    for (let x = 0; x < cw; x++) {
                        const off = rowBase + x * 4;
                        colPx[off] = r; colPx[off + 1] = g; colPx[off + 2] = b;
                    }
                }
                sctx.putImageData(colImg, Math.floor(xLast), 0, 0, 0, cw, H);
                xLast = xNow;
            }

            cctx.drawImage(specCanvas, 0, 0);

            // Current active phoneme (binary search on times array)
            const phonIdx = bisect(times, elapsed, times.length);
            const activePhoneme = phonIdx >= 0 ? events[phonIdx] : '';

            // Scroll by phoneme index ratio — works correctly for TTS, Klattsch, and mixed content
            let activeLineIdx = 0;
            if (times.length > 1) {
                const phonProgress = phonIdx >= 0 ? phonIdx / (times.length - 1) : 0;
                activeLineIdx = Math.min(displayLines.length - 1, Math.floor(phonProgress * displayLines.length));
            } else {
                const progress = Math.max(0, Math.min(1, elapsed / duration));
                activeLineIdx = Math.min(displayLines.length - 1, Math.floor(progress * displayLines.length));
            }

            // Subtitle backplate gradient
            const grad = cctx.createLinearGradient(0, H - 300, 0, H);
            grad.addColorStop(0, 'transparent');
            grad.addColorStop(0.5, 'rgba(5, 5, 20, 0.6)');
            grad.addColorStop(1, 'rgba(5, 5, 20, 0.9)');
            cctx.fillStyle = grad;
            cctx.fillRect(0, H - 300, W, 300);

            scrollTargetY = (H - 150) - (activeLineIdx * lh);
            scrollY += (scrollTargetY - scrollY) * 0.1;

            cctx.save();
            cctx.font = 'bold 36px ui-monospace, monospace';
            cctx.textBaseline = 'middle';
            cctx.textAlign = 'left';

            displayLines.forEach((line, lineIdx) => {
                const y = scrollY + lineIdx * lh;
                if (y < H - 400 || y > H + 100) return;
                const alpha = Math.max(0, Math.min(1, (y - (H - 400)) / 100));
                cctx.globalAlpha = alpha;
                cctx.shadowColor = 'rgba(0, 0, 0, 0.6)';
                cctx.shadowBlur = 8;

                cctx.fillStyle = '#fff';
                cctx.fillText(line.words.map(e => e.text).join(' '), pad, y);
            });
            cctx.restore();

            // Active phoneme overlay — top-left corner
            if (activePhoneme) {
                cctx.save();
                cctx.font = 'bold 52px ui-monospace, monospace';
                cctx.textBaseline = 'top';
                cctx.textAlign = 'left';
                cctx.shadowColor = 'rgba(0,0,0,0.8)';
                cctx.shadowBlur = 12;
                cctx.fillStyle = 'rgba(120, 200, 255, 0.9)';
                cctx.fillText(activePhoneme, 40, 60);
                cctx.restore();
            }

            // Lipsync viseme — bottom-right corner
            if (window.lipsyncRenderer?.ready) {
                let v1 = '', v2 = '', progress = 0;
                if (lipsyncTimes.length > 0) {
                    const lsIdx = bisect(lipsyncTimes, elapsed, lipsyncTimes.length);
                    if (lsIdx >= 0) {
                        v1 = lipsyncV1[lsIdx];
                        v2 = lipsyncV2[lsIdx];
                        const tStart = lipsyncTimes[lsIdx];
                        const tEnd = lsIdx + 1 < lipsyncTimes.length ? lipsyncTimes[lsIdx + 1] : duration;
                        progress = tEnd > tStart ? (elapsed - tStart) / (tEnd - tStart) : 0;
                    }
                }
                window.lipsyncRenderer.tick(v1, v2, progress, 1 / FPS);
                const sz = 300;
                cctx.save();
                cctx.globalAlpha = 0.92;
                cctx.drawImage(window.lipsyncRenderer.canvas, W - sz - 20, H - sz - 20, sz, sz);
                cctx.restore();
            }

            // Watermark
            cctx.save();
            cctx.font = '20px ui-monospace, monospace';
            cctx.fillStyle = 'rgba(255, 255, 255, 0.3)';
            cctx.textAlign = 'right';
            cctx.fillText('bytesizedfox.dev/sharptalk/', W - 40, 40);
            cctx.restore();

            if (elapsed < duration + 0.3) {
                requestAnimationFrame(renderLoop);
            } else {
                recorder.stop();
                recorder.onstop = () => {
                    const blob = new Blob(chunks, { type: mimeType });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement('a');
                    a.href = url;
                    a.download = `sharptalk_${Date.now()}.${ext}`;
                    a.click();
                    canvas.remove();
                    actx.close();
                    window.ui.updateStatus("video export complete");
                };
            }
        };

        renderLoop();
    },

    triggerFileInput: (id) => {
        document.getElementById(id).click();
    },

    readFileContent: async (id) => {
        const input = document.getElementById(id);
        if (input.files.length === 0) return null;
        const file = input.files[0];
        return await file.text();
    },

    // Methods called by .NET
    updateStatus: (text) => {
        document.getElementById('status').innerText = text;
    },

    updatePhonemes: (codesJson, activeIdx) => {
        const container = document.getElementById('phoneme-tracker');
        if (!container) return;
        const codes = typeof codesJson === 'string' ? JSON.parse(codesJson) : codesJson;
        if (!codes || codes.length === 0) {
            container.innerHTML = '';
            return;
        }
        container.innerHTML = codes.map((code, i) =>
            `<span class="phon ${i === activeIdx ? 'active' : ''}">${code}</span>`
        ).join('');
    },

    updateAllParams: (json) => {
        const params = typeof json === 'string' ? JSON.parse(json) : json;
        for (const [name, value] of Object.entries(params)) {
            if (name === 'VoiceType') {
                const cb = document.getElementById('voice-female');
                if (cb) cb.checked = (value === 1);
                continue;
            }
            if (name === 'sampleRate') {
                const sel = document.getElementById('sampleRate');
                if (sel) sel.value = value;
                continue;
            }

            const input = document.getElementById(`v-${name}`);
            const display = document.getElementById(`v-${name}-val`);
            if (input) input.value = value;
            if (display) {
                if (typeof value === 'number' && !Number.isInteger(value)) {
                    display.innerText = value.toLocaleString(undefined, { minimumFractionDigits: 0, maximumFractionDigits: 2 });
                } else {
                    display.innerText = value;
                }
            }
        }
        if (!window.ui._initDone) {
            window.ui._initDone = true;
            const saved = window.ui._getCookie('st_custom_voice');
            if (saved) {
                const sel = document.getElementById('presetSelect');
                if (sel) sel.value = 'custom';
                setTimeout(() => window.sharpTalk?.HandleImport(saved), 0);
            }
        }
    },

    showDownloadBtn: (show) => {
        document.getElementById('downloadBtn').style.display = show ? 'inline-block' : 'none';
    }
};
