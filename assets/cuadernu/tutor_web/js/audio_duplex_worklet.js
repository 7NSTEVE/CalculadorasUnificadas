/**
 * Full-Duplex Audio Engine for CuadernU Tutor Whiteboard:
 * - Mic capture: getUserMedia with Chromium WebRTC AEC3 (echoCancellation: true)
 *   + AudioWorklet 16 kHz PCM16 20 ms chunks framed with 8-byte "CU" binary header.
 *   + Silent GainNode(0) -> ctx.destination sink so Chromium ALWAYS pulls AudioWorklet frames.
 *   + Exact fractional phase resampling (zero clicks on 44.1/48 kHz Windows devices) + live RMS VU meter.
 *   + ScriptProcessorNode fallback if AudioWorklet is unavailable.
 * - Live PCM24 playback (Gemini 3.8 Live) pre-unlocked on user gesture with <30 ms hard-stop on barge-in.
 * - Synced WAV/MP3 playback (Gemini Native TTS) driving exact <cue> timestamps + real playbackRate.
 */

const WORKLET_CODE = `
class CuadernuMicProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this._acc = [];
    this._ratio = sampleRate / 16000;
    this._phase = 0;
    this._rmsSum = 0;
    this._rmsCnt = 0;
  }
  process(inputs) {
    const input = inputs[0];
    if (!input || !input[0]) return true;
    const ch = input[0];
    for (let i = 0; i < ch.length; i++) {
      this._rmsSum += ch[i] * ch[i];
      this._rmsCnt += 1;
    }
    while (this._phase < ch.length) {
      const idx = Math.floor(this._phase);
      const frac = this._phase - idx;
      const s0 = ch[idx] || 0;
      const s1 = ch[Math.min(idx + 1, ch.length - 1)] || s0;
      const s = Math.max(-1, Math.min(1, s0 + (s1 - s0) * frac));
      this._acc.push(s < 0 ? Math.round(s * 32768) : Math.round(s * 32767));
      this._phase += this._ratio;
    }
    this._phase -= ch.length;

    while (this._acc.length >= 320) {
      const frame = new Int16Array(this._acc.splice(0, 320));
      const rms = this._rmsCnt > 0 ? Math.sqrt(this._rmsSum / this._rmsCnt) : 0;
      this._rmsSum = 0;
      this._rmsCnt = 0;
      this.port.postMessage({ pcm: frame.buffer, rms }, [frame.buffer]);
    }
    return true;
  }
}
registerProcessor('cuadernu-mic-proc', CuadernuMicProcessor);
`;

export class DuplexAudioEngine {
  constructor({
    onMicFramePcm16,
    onMicLevel,
    onPlaybackClockMs,
    onPlaybackEnded,
    onLiveAudioStart,
    onLiveAudioDrain,
  } = {}) {
    this.onMicFramePcm16 = onMicFramePcm16;
    this.onMicLevel = onMicLevel;
    this.onPlaybackClockMs = onPlaybackClockMs;
    this.onPlaybackEnded = onPlaybackEnded;
    this.onLiveAudioStart = onLiveAudioStart;
    this.onLiveAudioDrain = onLiveAudioDrain;

    this._micCtx = null;
    this._micStream = null;
    this._micSrc = null;
    this._workletNode = null;
    this._zeroGain = null;
    this._micSeq = 0;

    this._playCtx = null;
    this._nextStartTime = 0;
    this._liveSources = new Set();
    this._liveChunks = [];
    this._liveGeneration = 0;
    this._liveTurnAudioStarted = false;

    this._mp3Audio = null;
    this._bufferSource = null;
    this._bufferStartTime = 0;
    this._bufferDurationSec = 0;
    this._activeRate = 1.0;
    this._playToken = 0;
    this._rafClockId = null;
    this._silentStartPerf = 0;
    this._silentDurationMs = 0;
  }

  /** Must be called inside a user click gesture to bypass browser AudioContext autoplay suspension. */
  async unlockPlayback() {
    const ctx = this._ensurePlayCtx();
    if (ctx.state === 'suspended') {
      try { await ctx.resume(); } catch {}
    }
  }

  async listAudioInputs() {
    if (!navigator.mediaDevices || !navigator.mediaDevices.enumerateDevices) return [];
    try {
      const devs = await navigator.mediaDevices.enumerateDevices();
      return devs.filter((d) => d.kind === 'audioinput');
    } catch {
      return [];
    }
  }

  _emitPcmFrame(pcmArrayBuffer, rms) {
    if (this.onMicLevel) this.onMicLevel(rms);
    if (!pcmArrayBuffer || !this.onMicFramePcm16) return;
    this._micSeq += 1;
    const packet = new Uint8Array(8 + pcmArrayBuffer.byteLength);
    const dv = new DataView(packet.buffer);
    packet[0] = 0x43; // 'C'
    packet[1] = 0x55; // 'U'
    packet[2] = 1;    // CH_MIC_PCM16
    packet[3] = 0;
    dv.setUint32(4, this._micSeq, true);
    packet.set(new Uint8Array(pcmArrayBuffer), 8);
    this.onMicFramePcm16(packet.buffer, rms);
  }

  async startMicCapture(deviceId = '') {
    this.stopMicCapture();
    await this.unlockPlayback();

    const audioConstraints = {
      echoCancellation: true,
      noiseSuppression: true,
      autoGainControl: true,
      channelCount: 1,
    };
    if (deviceId) {
      audioConstraints.deviceId = { exact: deviceId };
    }

    const stream = await navigator.mediaDevices.getUserMedia({ audio: audioConstraints });
    this._micStream = stream;

    const ctx = new AudioContext();
    this._micCtx = ctx;
    if (ctx.state === 'suspended') {
      await ctx.resume();
    }

    const src = ctx.createMediaStreamSource(stream);
    this._micSrc = src;

    // Silent sink keeps Chromium/Edge/WebView2 AudioContext actively pulling mic frames
    const zeroGain = ctx.createGain();
    zeroGain.gain.value = 0;
    zeroGain.connect(ctx.destination);
    this._zeroGain = zeroGain;

    let workletOk = false;
    if (ctx.audioWorklet) {
      const blob = new Blob([WORKLET_CODE], { type: 'application/javascript' });
      const url = URL.createObjectURL(blob);
      try {
        await ctx.audioWorklet.addModule(url);
        const node = new AudioWorkletNode(ctx, 'cuadernu-mic-proc');
        node.port.onmessage = (ev) => {
          const data = ev.data;
          if (!data) return;
          if (data instanceof ArrayBuffer) {
            this._emitPcmFrame(data, 0);
          } else if (data.pcm) {
            this._emitPcmFrame(data.pcm, Number(data.rms) || 0);
          }
        };
        src.connect(node);
        node.connect(zeroGain);
        this._workletNode = node;
        workletOk = true;
      } catch {
        workletOk = false;
      } finally {
        URL.revokeObjectURL(url);
      }
    }

    if (!workletOk) {
      // Resilient ScriptProcessorNode fallback if AudioWorklet fails to load
      const proc = ctx.createScriptProcessor(2048, 1, 1);
      const ratio = ctx.sampleRate / 16000;
      let phase = 0;
      const acc = [];
      proc.onaudioprocess = (ev) => {
        const ch = ev.inputBuffer.getChannelData(0);
        let sumSq = 0;
        for (let i = 0; i < ch.length; i++) sumSq += ch[i] * ch[i];
        const rms = Math.sqrt(sumSq / Math.max(1, ch.length));
        while (phase < ch.length) {
          const idx = Math.floor(phase);
          const s = Math.max(-1, Math.min(1, ch[idx] || 0));
          acc.push(s < 0 ? Math.round(s * 32768) : Math.round(s * 32767));
          phase += ratio;
        }
        phase -= ch.length;
        while (acc.length >= 320) {
          const frame = new Int16Array(acc.splice(0, 320));
          this._emitPcmFrame(frame.buffer, rms);
        }
      };
      src.connect(proc);
      proc.connect(zeroGain);
      this._workletNode = proc;
    }

    const track = stream.getAudioTracks()[0];
    return {
      label: track ? track.label : 'Micrófono activo',
      sampleRate: ctx.sampleRate,
      engine: workletOk ? 'audioworklet' : 'scriptprocessor',
    };
  }

  stopMicCapture() {
    if (this._workletNode) {
      try { this._workletNode.disconnect(); } catch {}
      this._workletNode = null;
    }
    if (this._micSrc) {
      try { this._micSrc.disconnect(); } catch {}
      this._micSrc = null;
    }
    if (this._zeroGain) {
      try { this._zeroGain.disconnect(); } catch {}
      this._zeroGain = null;
    }
    if (this._micStream) {
      this._micStream.getTracks().forEach((t) => t.stop());
      this._micStream = null;
    }
    if (this._micCtx) {
      try { this._micCtx.close(); } catch {}
      this._micCtx = null;
    }
    if (this.onMicLevel) this.onMicLevel(0);
  }

  _ensurePlayCtx() {
    if (!this._playCtx || this._playCtx.state === 'closed') {
      this._playCtx = new AudioContext({ sampleRate: 24000 });
      this._nextStartTime = this._playCtx.currentTime;
    }
    if (this._playCtx.state === 'suspended') {
      this._playCtx.resume().catch(() => {});
    }
    return this._playCtx;
  }

  enqueueLivePcm24(pcmInt16Bytes) {
    const ctx = this._ensurePlayCtx();
    const samples = Math.floor(pcmInt16Bytes.byteLength / 2);
    if (samples <= 0) return;
    // Copy to aligned buffer safely even when subarray byteOffset is odd
    const aligned = new Uint8Array(samples * 2);
    aligned.set(pcmInt16Bytes.subarray(0, samples * 2));
    const i16 = new Int16Array(aligned.buffer);
    const f32 = new Float32Array(samples);
    let sumSq = 0;
    for (let i = 0; i < samples; i++) {
      const v = i16[i] / 32768.0;
      f32[i] = v;
      sumSq += v * v;
    }
    const rms = Math.sqrt(sumSq / samples);

    const abuf = ctx.createBuffer(1, samples, 24000);
    abuf.copyToChannel(f32, 0);
    const src = ctx.createBufferSource();
    src.buffer = abuf;
    src.connect(ctx.destination);

    const wasEmpty = this._liveSources.size === 0;
    const startAt = Math.max(ctx.currentTime + 0.015, this._nextStartTime);
    const endAt = startAt + abuf.duration;
    src.start(startAt);
    this._nextStartTime = endAt;
    this._liveSources.add(src);
    this._liveChunks.push({ startAt, endAt, rms });
    if (this._liveChunks.length > 120) {
      const nowT = ctx.currentTime;
      this._liveChunks = this._liveChunks.filter((c) => c.endAt >= nowT - 0.15);
    }

    if (wasEmpty || !this._liveTurnAudioStarted) {
      this._liveTurnAudioStarted = true;
      if (this.onLiveAudioStart) {
        const delayMs = Math.max(0, Math.round((startAt - ctx.currentTime) * 1000));
        this.onLiveAudioStart(delayMs);
      }
    }

    const gen = this._liveGeneration;
    src.onended = () => {
      this._liveSources.delete(src);
      if (gen === this._liveGeneration && this._liveSources.size === 0) {
        this._liveTurnAudioStarted = false;
        this._liveChunks = [];
        if (this.onLiveAudioDrain) this.onLiveAudioDrain();
      }
    };
  }

  resetLiveTurnAudioFlag() {
    this._liveTurnAudioStarted = false;
  }

  getLiveGeneration() {
    return this._liveGeneration;
  }

  isLiveAudioPlaying() {
    if (!this._playCtx || this._liveSources.size === 0) return false;
    return this._nextStartTime > this._playCtx.currentTime + 0.015;
  }

  /**
   * Real-time speech-activity speed multiplier for whiteboard handwriting:
   * 1.0 while the professor is actively speaking, 0.22 during natural breath pauses,
   * so the marker moves in sync with her live voice cadence instead of racing ahead.
   */
  getLiveSpeechSpeedFactor() {
    if (!this._playCtx || this._liveSources.size === 0) return 1.0;
    const nowT = this._playCtx.currentTime;
    for (let i = this._liveChunks.length - 1; i >= 0; i--) {
      const c = this._liveChunks[i];
      if (nowT >= c.startAt - 0.02 && nowT <= c.endAt + 0.04) {
        return c.rms >= 0.011 ? 1.0 : 0.22;
      }
    }
    return this._nextStartTime > nowT + 0.02 ? 0.85 : 1.0;
  }

  /**
   * Returns current live audio jitter-buffer lead in ms so LIVE_CANVAS_ACTION
   * gestures land precisely when the corresponding spoken PCM24 audio reaches the speakers.
   */
  getLivePlaybackLeadMs() {
    if (!this._playCtx || this._playCtx.state !== 'running') return 0;
    const leadSec = this._nextStartTime - this._playCtx.currentTime;
    if (!Number.isFinite(leadSec) || leadSec <= 0) return 0;
    return Math.round(leadSec * 1000);
  }

  /** Drive visual cues WITHOUT interrupting ongoing Gemini 3.8 Live PCM24 voice playback! */
  runVisualCueClockOnly(durationMs = 1500) {
    this._stopClockLoop();
    this._startSilentClock(durationMs);
  }

  _decodeBase64ToArrayBuffer(b64) {
    const bin = atob(b64);
    const bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    return bytes.buffer;
  }

  playAlignedAudioOrClock(audioBase64, audioMime = 'audio/wav', fallbackDurationMs = 3000, playbackRate = 1.0) {
    if (!audioBase64) {
      // Never kill live Gemini PCM24 sources when dispatching silent visual-only tool beats!
      this.runVisualCueClockOnly(fallbackDurationMs);
      return;
    }
    this.hardStopPlayback();
    const myToken = ++this._playToken;
    const rate = Math.max(0.75, Math.min(1.35, Number(playbackRate) || 1.0));
    this._activeRate = rate;

    const ctx = this._ensurePlayCtx();
    let arrBuf;
    try {
      arrBuf = this._decodeBase64ToArrayBuffer(audioBase64);
    } catch {
      this._startSilentClock(fallbackDurationMs);
      return;
    }

    ctx.decodeAudioData(arrBuf.slice(0))
      .then((audioBuf) => {
        if (myToken !== this._playToken) return;
        const src = ctx.createBufferSource();
        src.buffer = audioBuf;
        src.playbackRate.value = rate;
        src.connect(ctx.destination);

        const startAt = ctx.currentTime + 0.012;
        this._bufferSource = src;
        this._bufferStartTime = startAt;
        this._bufferDurationSec = audioBuf.duration;

        src.onended = () => {
          if (myToken !== this._playToken) return;
          if (this._bufferSource === src) this._bufferSource = null;
          this._stopClockLoop();
          if (this.onPlaybackEnded) this.onPlaybackEnded();
        };

        src.start(startAt);
        this._startBufferClockLoop(myToken);
      })
      .catch(() => {
        if (myToken !== this._playToken) return;
        // Fallback to HTMLAudioElement if decodeAudioData fails on an unsupported codec
        const mime = audioMime || 'audio/wav';
        const audio = new Audio(`data:${mime};base64,${audioBase64}`);
        audio.playbackRate = rate;
        this._mp3Audio = audio;
        audio.onended = () => {
          if (myToken !== this._playToken) return;
          this._stopClockLoop();
          if (this.onPlaybackEnded) this.onPlaybackEnded();
        };
        audio.play().catch(() => {
          if (myToken !== this._playToken) return;
          this._mp3Audio = null;
          this._startSilentClock(fallbackDurationMs);
        });
        this._startAudioClockLoop();
      });
  }

  playAlignedMp3OrClock(mp3Base64, fallbackDurationMs = 3000) {
    this.playAlignedAudioOrClock(mp3Base64, 'audio/mpeg', fallbackDurationMs, 1.0);
  }

  _startBufferClockLoop(token) {
    this._stopClockLoop();
    const tick = () => {
      if (token !== this._playToken || !this._bufferSource || !this._playCtx) return;
      const elapsedSec = Math.max(0, (this._playCtx.currentTime - this._bufferStartTime) * this._activeRate);
      const ms = Math.round(elapsedSec * 1000);
      if (this.onPlaybackClockMs) this.onPlaybackClockMs(ms);
      this._rafClockId = requestAnimationFrame(tick);
    };
    this._rafClockId = requestAnimationFrame(tick);
  }

  _startAudioClockLoop() {
    this._stopClockLoop();
    const tick = () => {
      if (!this._mp3Audio) return;
      const ms = Math.round(this._mp3Audio.currentTime * 1000);
      if (this.onPlaybackClockMs) this.onPlaybackClockMs(ms);
      this._rafClockId = requestAnimationFrame(tick);
    };
    this._rafClockId = requestAnimationFrame(tick);
  }

  _startSilentClock(durationMs) {
    this._stopClockLoop();
    this._silentStartPerf = performance.now();
    this._silentDurationMs = Math.max(600, durationMs);
    const tick = () => {
      const elapsed = Math.round(performance.now() - this._silentStartPerf);
      if (this.onPlaybackClockMs) this.onPlaybackClockMs(elapsed);
      if (elapsed >= this._silentDurationMs) {
        this._stopClockLoop();
        if (this.onPlaybackEnded) this.onPlaybackEnded();
        return;
      }
      this._rafClockId = requestAnimationFrame(tick);
    };
    this._rafClockId = requestAnimationFrame(tick);
  }

  _stopClockLoop() {
    if (this._rafClockId !== null) {
      cancelAnimationFrame(this._rafClockId);
      this._rafClockId = null;
    }
  }

  hardStopPlayback() {
    this._playToken += 1;
    this._stopClockLoop();
    if (this._bufferSource) {
      try {
        this._bufferSource.onended = null;
        this._bufferSource.stop(0);
        this._bufferSource.disconnect();
      } catch {}
      this._bufferSource = null;
    }
    if (this._mp3Audio) {
      try {
        this._mp3Audio.pause();
        this._mp3Audio.currentTime = 0;
      } catch {}
      this._mp3Audio = null;
    }
    for (const src of this._liveSources) {
      try { src.stop(0); } catch {}
    }
    this._liveSources.clear();
    this._liveChunks = [];
    this._liveGeneration += 1;
    this._liveTurnAudioStarted = false;
    if (this._playCtx) {
      this._nextStartTime = this._playCtx.currentTime;
    }
  }
}
