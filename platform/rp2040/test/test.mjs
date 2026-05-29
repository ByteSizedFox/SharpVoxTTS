// rp2040js simulation test for the SharpVox RP2040 demo.
// Usage: node test.mjs [text to speak] [path/to/sharptalk_demo.uf2]
//
// Defaults: "Hello world."  ../build/sharptalk_demo.uf2
// Output:   output.wav in the current directory

import { readFileSync, writeFileSync, existsSync } from 'fs';
import { Simulator } from '/home/user/rp2040js/dist/esm/simulator.js';
import { bootromB1 } from '/home/user/rp2040js/dist/demo/demo/bootrom.js';
import { loadUF2 } from '/home/user/rp2040js/dist/demo/demo/load-flash.js';

const testText = process.argv[2] ?? 'Hello world.';
const uf2Path  = process.argv[3] ?? new URL('../build/sharptalk_demo.uf2', import.meta.url).pathname;
const TIMEOUT_MS = 600_000;   // 10 min — simulation is slow; higher sample rates need more time

if (!existsSync(uf2Path)) {
    console.error(`UF2 not found: ${uf2Path}`);
    console.error('Build first:');
    console.error('  cd platform/rp2040 && mkdir -p build && cd build');
    console.error('  PICO_SDK_PATH=~/pico-sdk cmake .. -DPICO_BOARD=pico');
    console.error('  make -j$(nproc)');
    process.exit(1);
}

console.error(`Loading firmware: ${uf2Path}`);
console.error(`Test input:       "${testText}"`);

// --- simulator setup ---------------------------------------------------------
const sim = new Simulator();
sim.rp2040.loadBootrom(bootromB1);
loadUF2(uf2Path, sim.rp2040);
sim.rp2040.core.PC = 0x10000000;   // skip to flash (boot stage 2 → app)

// --- UART state machine ------------------------------------------------------
// Protocol produced by main.cpp:
//   "SHVX READY\r\n"
//   "SHVX BEGIN <rate>\r\n"
//   "SHVX CHUNK <n>\r\n" + n×2 bytes raw int16 PCM   (repeated)
//   "SHVX END\r\n"

let state    = 'waitReady';  // waitReady | waitBegin | waitChunkHdr | readChunk | done
let rxLine   = '';
let audioRate = 0;
let chunkBytes = 0;       // bytes remaining in current chunk
let chunkBuf   = null;    // accumulator for current chunk
let chunkIdx   = 0;
const pcmChunks = [];     // collected Buffer chunks

sim.rp2040.uart[0].onByte = (byte) => {
    if (state === 'readChunk') {
        chunkBuf[chunkIdx++] = byte;
        if (chunkIdx >= chunkBytes) {
            pcmChunks.push(Buffer.from(chunkBuf));
            state = 'waitChunkHdr';
            rxLine = '';
        }
        return;
    }

    if (byte === 0x0a /* '\n' */) {
        processLine(rxLine.replace(/\r$/, ''));
        rxLine = '';
    } else {
        rxLine += String.fromCharCode(byte);
    }
};

function processLine(line) {
    if (state === 'waitReady' && line === 'SHVX READY') {
        console.error('RP2040 ready — sending text...');
        state = 'waitBegin';
        for (const ch of testText + '\n') {
            sim.rp2040.uart[0].feedByte(ch.charCodeAt(0));
        }
        return;
    }

    if (state === 'waitBegin' && line.startsWith('SHVX BEGIN ')) {
        audioRate = parseInt(line.split(' ')[2], 10);
        console.error(`Stream started: ${audioRate} Hz`);
        state = 'waitChunkHdr';
        return;
    }

    if (state === 'waitChunkHdr' && line.startsWith('SHVX CHUNK ')) {
        const nSamples = parseInt(line.split(' ')[2], 10);
        chunkBytes = nSamples * 2;
        chunkBuf   = new Uint8Array(chunkBytes);
        chunkIdx   = 0;
        state = 'readChunk';
        return;
    }

    if (state === 'waitChunkHdr' && line === 'SHVX END') {
        finish();
        return;
    }
}

function finish() {
    clearTimeout(timeout);
    sim.stop();
    state = 'done';

    const pcmBuf   = Buffer.concat(pcmChunks);
    const dataBytes = pcmBuf.length;
    const totalSamples = dataBytes / 2;
    console.error(`Audio: ${audioRate} Hz, ${totalSamples} samples ` +
                  `(${(totalSamples / audioRate).toFixed(2)} s)`);

    // Write WAV
    const wav = Buffer.alloc(44 + dataBytes);
    wav.write('RIFF', 0);
    wav.writeUInt32LE(36 + dataBytes, 4);
    wav.write('WAVE', 8);
    wav.write('fmt ', 12);
    wav.writeUInt32LE(16, 16);
    wav.writeUInt16LE(1,  20);                          // PCM
    wav.writeUInt16LE(1,  22);                          // mono
    wav.writeUInt32LE(audioRate, 24);
    wav.writeUInt32LE(audioRate * 1 * 2, 28);           // byteRate = sampleRate × channels × bytesPerSample
    wav.writeUInt16LE(2,  32);                          // block align
    wav.writeUInt16LE(16, 34);                          // bits per sample
    wav.write('data', 36);
    wav.writeUInt32LE(dataBytes, 40);
    pcmBuf.copy(wav, 44);
    writeFileSync('output.wav', wav);

    console.error(`Written output.wav — ${(dataBytes / 1024).toFixed(1)} KB`);
    console.log('PASS');
    process.exit(0);
}

const timeout = setTimeout(() => {
    console.error(`TIMEOUT after ${TIMEOUT_MS / 1000}s — state: ${state}`);
    process.exit(1);
}, TIMEOUT_MS);

// Keep Node alive during async simulate loop
timeout.unref();

console.error('Starting simulation (this will take a while in software)...');
sim.execute();
