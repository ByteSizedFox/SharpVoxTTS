let audioContext = null;
let nextStartTime = 0;

window.initAudio = (sampleRate) => {
    if (!audioContext) {
        audioContext = new (window.AudioContext || window.webkitAudioContext)({
            sampleRate: sampleRate || 22050
        });
        nextStartTime = 0;
    }
    if (audioContext.state === 'suspended') {
        audioContext.resume();
    }
};

window.playAudioStream = (pcmBytes, sampleRate) => {
    if (!audioContext) window.initAudio(sampleRate);

    // pcmBytes is a Uint8Array (byte[] from .NET), reinterpret as Int16Array
    const pcmData = new Int16Array(pcmBytes.buffer, pcmBytes.byteOffset, pcmBytes.byteLength >> 1);
    const floatData = new Float32Array(pcmData.length);
    for (let i = 0; i < pcmData.length; i++) {
        floatData[i] = pcmData[i] / 32768.0;
    }

    const buffer = audioContext.createBuffer(1, floatData.length, sampleRate || 22050);
    buffer.copyToChannel(floatData, 0);

    const source = audioContext.createBufferSource();
    source.buffer = buffer;
    source.connect(audioContext.destination);

    const currentTime = audioContext.currentTime;
    if (nextStartTime < currentTime) {
        nextStartTime = currentTime + 0.05;
    }

    source.start(nextStartTime);
    nextStartTime += buffer.duration;
};

window.stopAudio = () => {
    if (audioContext) {
        audioContext.close();
        audioContext = null;
        nextStartTime = 0;
    }
};

window.downloadBytes = (data, filename, mimeType) => {
    const blob = new Blob([data], { type: mimeType });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
};

window.downloadFile = (filename, content) => {
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
};

window.yieldToEventLoop = () => new Promise(resolve => setTimeout(resolve, 0));

// Reserve a start time slot and return it so the caller can anchor phoneme
// tracking before the first audio chunk is queued.
window.reserveStartTime = (sampleRate) => {
    if (!audioContext) window.initAudio(sampleRate);
    const cur = audioContext.currentTime;
    if (nextStartTime < cur) nextStartTime = cur + 0.05;
    return nextStartTime;
};

let _trackingRaf = null;
let _trackingCodesJson = '[]';
let _trackingStartSecs = [];
let _trackingStart = 0;
let _lastTrackIdx = -1;

window.startPhonemeTracking = (codesJson, startSecsJson, playAt) => {
    if (_trackingRaf) { cancelAnimationFrame(_trackingRaf); _trackingRaf = null; }
    _trackingCodesJson = codesJson || '[]';
    _trackingStartSecs = JSON.parse(startSecsJson || '[]');
    _trackingStart = playAt;
    _lastTrackIdx = -1;

    if (_trackingStartSecs.length === 0) return;

    function tick() {
        if (!audioContext) return;
        const elapsed = audioContext.currentTime - _trackingStart;
        let idx = -1;
        for (let i = 0; i < _trackingStartSecs.length; i++) {
            if (_trackingStartSecs[i] <= elapsed) idx = i;
            else break;
        }
        if (idx !== _lastTrackIdx) {
            _lastTrackIdx = idx;
            window.ui?.updatePhonemes(_trackingCodesJson, idx);
        }
        const lastStart = _trackingStartSecs[_trackingStartSecs.length - 1] ?? 0;
        if (elapsed < lastStart + 1.5) {
            _trackingRaf = requestAnimationFrame(tick);
        } else {
            window.ui?.updatePhonemes(_trackingCodesJson, -1);
            _trackingRaf = null;
        }
    }
    _trackingRaf = requestAnimationFrame(tick);
};

window.stopPhonemeTracking = () => {
    if (_trackingRaf) { cancelAnimationFrame(_trackingRaf); _trackingRaf = null; }
    window.ui?.updatePhonemes(_trackingCodesJson, -1);
    _lastTrackIdx = -1;
};
