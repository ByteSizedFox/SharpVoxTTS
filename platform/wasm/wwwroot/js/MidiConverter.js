// Native JS port of midi.py
(function (global) {
  'use strict';

  const NOTE_NAMES    = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];
  const PAUSE_SHORT   = 100, PAUSE_MED = 200, PAUSE_LONG = 300;
  const WRAP_COL      = 100;


  function readVlq(view, pos) {
    let value = 0, n = 0;
    while (n < 4) {
      const b = view.getUint8(pos + n); n++;
      value = (value << 7) | (b & 0x7F);
      if (!(b & 0x80)) break;
    }
    return { value, n };
  }

  function midiToNoteName(note) {
    return NOTE_NAMES[note % 12] + (Math.floor(note / 12) - 1);
  }


  function parseTrack(view, start, len) {
    const events = [];
    let pos = start, absTime = 0, lastStatus = 0;
    const end = start + len;

    while (pos < end && pos < view.byteLength) {
      const { value: delta, n: dn } = readVlq(view, pos);
      pos += dn;
      absTime += delta;
      if (pos >= end) break;

      const b = view.getUint8(pos);

      if (b === 0xFF) {
        pos++;
        const metaType = view.getUint8(pos++);
        const { value: mlen, n: mln } = readVlq(view, pos);
        pos += mln;
        if (metaType === 0x51 && mlen >= 3) {
          const us = (view.getUint8(pos) << 16) | (view.getUint8(pos+1) << 8) | view.getUint8(pos+2);
          events.push({ absTime, type: 'tempo', tempo: us });
        }
        pos += mlen;

      } else if (b === 0xF0 || b === 0xF7) {
        pos++;
        const { value: slen, n: sln } = readVlq(view, pos);
        pos += sln + slen;
        lastStatus = 0;

      } else {
        let status;
        if (b & 0x80) { status = b; lastStatus = b; pos++; }
        else           { status = lastStatus; }

        const type = (status >> 4) & 0xF;
        const ch   = status & 0xF;

        switch (type) {
          case 0x8: {
            const note = view.getUint8(pos++), vel = view.getUint8(pos++);
            events.push({ absTime, type: 'note_off', ch, note, vel });
            break;
          }
          case 0x9: {
            const note = view.getUint8(pos++), vel = view.getUint8(pos++);
            events.push({ absTime, type: vel > 0 ? 'note_on' : 'note_off', ch, note, vel });
            break;
          }
          case 0xA: pos += 2; break; // aftertouch
          case 0xB: {
            const ctrl = view.getUint8(pos++), val = view.getUint8(pos++);
            events.push({ absTime, type: 'cc', ch, ctrl, val });
            break;
          }
          case 0xC: pos++;   break; // program change
          case 0xD: pos++;   break; // channel pressure
          case 0xE: pos += 2; break; // pitch bend
          default:  pos++;   break;
        }
      }
    }
    return events;
  }


  function buildTempoMap(allEvents) {
    const map = [{ tick: 0, tempo: 500000 }];
    for (const ev of allEvents) {
      if (ev.type !== 'tempo') continue;
      if (map[map.length - 1].tick === ev.absTime)
        map[map.length - 1].tempo = ev.tempo;
      else
        map.push({ tick: ev.absTime, tempo: ev.tempo });
    }
    return map;
  }

  function ticksToMs(startTick, endTick, tempoMap, tpb) {
    if (startTick >= endTick) return 0;
    let totalMs = 0, segStart = startTick, ti = 0;
    for (let i = 0; i < tempoMap.length; i++) {
      if (tempoMap[i].tick <= segStart) ti = i; else break;
    }
    while (segStart < endTick) {
      const { tempo } = tempoMap[ti];
      const nextTick  = ti + 1 < tempoMap.length ? tempoMap[ti + 1].tick : endTick;
      const segEnd    = Math.min(endTick, nextTick);
      totalMs += (segEnd - segStart) * (tempo / 1000.0 / tpb);
      segStart = segEnd;
      if (ti + 1 < tempoMap.length && tempoMap[ti + 1].tick <= segStart) ti++;
    }
    return totalMs;
  }


  function parseMidiBuffer(arrayBuffer) {
    const view = new DataView(arrayBuffer);

    const magic = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
    if (magic !== 'MThd') throw new Error('Not a valid MIDI file');

    const headerLen = view.getUint32(4);
    const ntrks     = view.getUint16(10);
    const division  = view.getUint16(12);
    if (division & 0x8000) throw new Error('SMPTE timecode MIDI not supported');
    const tpb = division;

    const allEvents = [];
    let pos = 8 + headerLen;
    for (let t = 0; t < ntrks; t++) {
      if (pos + 8 > view.byteLength) break;
      const tm = String.fromCharCode(view.getUint8(pos), view.getUint8(pos+1), view.getUint8(pos+2), view.getUint8(pos+3));
      const tlen = view.getUint32(pos + 4);
      pos += 8;
      if (tm === 'MTrk') allEvents.push(...parseTrack(view, pos, tlen));
      pos += tlen;
    }

    allEvents.sort((a, b) => a.absTime - b.absTime);

    const tempoMap = buildTempoMap(allEvents);

    const active   = new Map();
    const modWheel = new Map();
    const chEvents = new Map();

    for (const ev of allEvents) {
      if (ev.type === 'cc' && ev.ctrl === 1) {
        modWheel.set(ev.ch, ev.val);
      } else if (ev.type === 'note_on') {
        active.set(`${ev.ch},${ev.note}`, { startTick: ev.absTime, vel: ev.vel });
      } else if (ev.type === 'note_off') {
        const key = `${ev.ch},${ev.note}`;
        if (active.has(key)) {
          const { startTick, vel } = active.get(key);
          active.delete(key);
          if (!chEvents.has(ev.ch)) chEvents.set(ev.ch, []);
          chEvents.get(ev.ch).push({
            ch: ev.ch, note: ev.note, vel,
            startTick, endTick: ev.absTime,
            durationMs: ticksToMs(startTick, ev.absTime, tempoMap, tpb),
            mod: modWheel.get(ev.ch) || 0,
          });
        }
      }
    }

    for (const evs of chEvents.values()) evs.sort((a, b) => a.startTick - b.startTick);

    return { tpb, tempoMap, chEvents };
  }


  function pauseToken(ms) {
    if (ms <= 0) return "";
    if (Math.abs(ms - PAUSE_SHORT) <= 20) return ",";
    if (Math.abs(ms - PAUSE_MED)   <= 20) return ";";
    if (Math.abs(ms - PAUSE_LONG)  <= 20) return ".";
    return `p${Math.round(ms)}`;
  }

  function channelToTokens(noteEvents, phoneme, tempoMap, tpb) {
    const tokens = [];
    let runNote = null, runR = null, runV = null, prevEnd = null;

    for (const ev of noteEvents) {
      if (prevEnd !== null) {
        const gapMs = ticksToMs(prevEnd, ev.startTick, tempoMap, tpb);
        if (gapMs > 10) { const tok = pauseToken(gapMs); if (tok) tokens.push(tok); }
      }

      const r = Math.round(ev.durationMs);
      if (runR !== r) { tokens.push(`r${r}`); runR = r; }

      if (ev.mod > 0) {
        const vib = Math.round(ev.mod / 127.0 * 8.0);
        if (runV !== vib) { tokens.push(`v${vib}`); runV = vib; }
      } else if (runV) {
        tokens.push("v0"); runV = 0;
      }

      if (ev.note !== runNote) { tokens.push(`b${midiToNoteName(ev.note)}`); runNote = ev.note; }

      tokens.push(phoneme);
      prevEnd = ev.endTick;
    }
    return tokens;
  }

  function wrapTokens(tokens) {
    const lines = []; let line = [], col = 0;
    for (const tok of tokens) {
      if (col + tok.length + 1 > WRAP_COL && line.length) {
        lines.push(line.join(" ")); line = [tok]; col = tok.length;
      } else { line.push(tok); col += tok.length + 1; }
    }
    if (line.length) lines.push(line.join(" "));
    return lines.join("\n    ");
  }


  let _parsed = null;

  function parse(arrayBuffer) {
    try {
      _parsed = parseMidiBuffer(arrayBuffer);
      const channels = [];
      for (const [ch, evs] of [..._parsed.chEvents.entries()].sort(([a],[b]) => a - b)) {
        const durMs = evs.length
          ? ticksToMs(evs[0].startTick, evs[evs.length-1].endTick, _parsed.tempoMap, _parsed.tpb)
          : 0;
        channels.push({
          id: ch,
          label: `Channel ${ch}${ch === 9 ? ' [drums]' : ''} — ${evs.length} notes, ~${(durMs/1000).toFixed(2)}s`,
          noteCount: evs.length,
        });
      }
      return { channels, tpb: _parsed.tpb };
    } catch (e) {
      _parsed = null;
      return { error: e.message, channels: [] };
    }
  }

  function convert(channelId, phoneme) {
    if (!_parsed) return "";
    phoneme = (phoneme || "AW").trim().toUpperCase() || "AW";

    const targets = channelId === 'all'
      ? [..._parsed.chEvents.keys()].sort((a, b) => a - b)
      : [parseInt(channelId, 10)];

    const lines = [];
    for (const ch of targets) {
      const evs = _parsed.chEvents.get(ch);
      if (!evs || !evs.length) continue;
      const durMs = ticksToMs(evs[0].startTick, evs[evs.length-1].endTick, _parsed.tempoMap, _parsed.tpb);
      lines.push(`# channel ${ch}${ch === 9 ? ' [drums]' : ''}  (${evs.length} notes, ~${(durMs/1000).toFixed(2)}s)`);
      lines.push(wrapTokens(channelToTokens(evs, phoneme, _parsed.tempoMap, _parsed.tpb)));
      lines.push("");
    }
    return lines.join("\n").trimEnd();
  }

  global.MidiConverter = { parse, convert };

})(window);
