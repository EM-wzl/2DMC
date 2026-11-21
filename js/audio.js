/* 2DMC Web — 程序化音效（Web Audio，无外部音频文件）
 * 纯合成：白噪声 + 振荡器 + 包络。浏览器首次手势时解锁 AudioContext。
 * Node 自检环境没有 AudioContext，会自动降级为 no-op。
 */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const KEY = '2dmc-web-sound-v1';

  class Sound {
    constructor() {
      this.enabled = true;
      this.ctx = null;
      this.master = null;
      this.noise = null;
      this.ambGain = null;
      this.ambKind = 'none';
      this.volume = 0.55;
      try {
        const ls = (typeof localStorage !== 'undefined') ? localStorage : null;
        if (ls && ls.getItem(KEY) === 'off') this.enabled = false;
      } catch (e) { /* file:// 或隐私模式 */ }
    }

    /* 首次用户手势调用，之后所有 play 都可以直接出声 */
    unlock() {
      if (!this.enabled) return false;
      const AC = typeof window !== 'undefined'
        ? (window.AudioContext || window.webkitAudioContext) : null;
      if (!AC) return false;
      if (!this.ctx) {
        try {
          this.ctx = new AC();
          this.master = this.ctx.createGain();
          this.master.gain.value = this.volume;
          this.master.connect(this.ctx.destination);
          // 2 秒白噪声循环，供所有噪声类音效复用
          const len = Math.floor(this.ctx.sampleRate * 2);
          const buf = this.ctx.createBuffer(1, len, this.ctx.sampleRate);
          const ch = buf.getChannelData(0);
          let seed = 12345;
          for (let i = 0; i < len; i++) {
            seed ^= seed << 13; seed >>>= 0; seed ^= seed >>> 17; seed ^= seed << 5; seed >>>= 0;
            ch[i] = (seed / 2147483648 - 1) * 0.8;
          }
          this.noise = buf;
          // 常驻环境声（雨/洞穴/下界/末地），由 ambGain 控制音量
          const src = this.ctx.createBufferSource();
          src.buffer = buf; src.loop = true;
          const lp = this.ctx.createBiquadFilter();
          lp.type = 'lowpass'; lp.frequency.value = 900;
          this.ambGain = this.ctx.createGain();
          this.ambGain.gain.value = 0;
          src.connect(lp); lp.connect(this.ambGain); this.ambGain.connect(this.master);
          src.start(0);
        } catch (e) { this.ctx = null; return false; }
      }
      if (this.ctx.state === 'suspended') this.ctx.resume().catch(() => {});
      return true;
    }

    setEnabled(v) {
      this.enabled = !!v;
      try { if (typeof localStorage !== 'undefined') localStorage.setItem(KEY, this.enabled ? 'on' : 'off'); }
      catch (e) { /* ignore */ }
      if (!this.enabled && this.master && this.ctx)
        this.master.gain.setTargetAtTime(0, this.ctx.currentTime, 0.02);
      else if (this.enabled) {
        this.unlock();
        if (this.master && this.ctx)
          this.master.gain.setTargetAtTime(this.volume, this.ctx.currentTime, 0.02);
      }
    }
    toggle() { this.setEnabled(!this.enabled); return this.enabled; }

    /* 一段带包络的噪声 */
    hiss(dur, freq, gain, q, type) {
      if (!this.ctx || !this.enabled) return;
      const c = this.ctx, t = c.currentTime;
      const s = c.createBufferSource(); s.buffer = this.noise;
      const f = c.createBiquadFilter();
      f.type = type || 'bandpass'; f.frequency.value = freq; f.Q.value = q || 0.8;
      const g = c.createGain();
      g.gain.setValueAtTime(0.0001, t);
      g.gain.exponentialRampToValueAtTime(Math.max(0.0002, gain), t + 0.008);
      g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
      s.connect(f); f.connect(g); g.connect(this.master);
      s.start(t, Math.random() * 1.5); s.stop(t + dur + 0.02);
    }
    /* 一段带滑音的振荡器 */
    tone(f0, f1, dur, gain, type, delay) {
      if (!this.ctx || !this.enabled) return;
      const c = this.ctx, t = c.currentTime + (delay || 0);
      const o = c.createOscillator(), g = c.createGain();
      o.type = type || 'sine';
      o.frequency.setValueAtTime(f0, t);
      o.frequency.exponentialRampToValueAtTime(Math.max(20, f1), t + dur);
      g.gain.setValueAtTime(0.0001, t);
      g.gain.exponentialRampToValueAtTime(Math.max(0.0002, gain), t + 0.01);
      g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
      o.connect(g); g.connect(this.master);
      o.start(t); o.stop(t + dur + 0.02);
    }
    /* 多段快速噪声（脚步 / 吃东西） */
    taps(n, gap, freq, gain) {
      for (let i = 0; i < n; i++) {
        const d = i * gap;
        if (!this.ctx || !this.enabled) return;
        const c = this.ctx, t = c.currentTime + d;
        const s = c.createBufferSource(); s.buffer = this.noise;
        const f = c.createBiquadFilter(); f.type = 'bandpass';
        f.frequency.value = freq; f.Q.value = 1.2;
        const g = c.createGain();
        g.gain.setValueAtTime(0.0001, t);
        g.gain.exponentialRampToValueAtTime(gain, t + 0.005);
        g.gain.exponentialRampToValueAtTime(0.0001, t + 0.07);
        s.connect(f); f.connect(g); g.connect(this.master);
        s.start(t, Math.random()); s.stop(t + 0.09);
      }
    }

    /* 事件名 → 声音 */
    play(name) {
      if (!this.enabled) return;
      if (!this.unlock()) return;
      switch (name) {
        case 'dig':      this.hiss(0.07, 620 + Math.random() * 300, 0.10, 1.0); break;
        case 'break':    this.hiss(0.16, 420, 0.22, 0.7, 'lowpass');
                         this.tone(180, 70, 0.12, 0.07, 'triangle'); break;
        case 'place':    this.tone(340, 190, 0.09, 0.11, 'square');
                         this.hiss(0.06, 900, 0.07, 1.0); break;
        case 'step':     this.hiss(0.055, 230 + Math.random() * 90, 0.055, 0.9, 'lowpass'); break;
        case 'jump':     this.tone(220, 380, 0.10, 0.055, 'sine'); break;
        case 'land':     this.hiss(0.10, 180, 0.09, 0.8, 'lowpass'); break;
        case 'hurt':     this.tone(320, 110, 0.24, 0.17, 'sawtooth');
                         this.hiss(0.13, 700, 0.10, 0.6); break;
        case 'death':    this.tone(300, 60, 0.65, 0.19, 'sawtooth'); break;
        case 'eat':      this.taps(3, 0.13, 480, 0.11); break;
        case 'craft':    this.tone(420, 630, 0.10, 0.10, 'triangle');
                         this.tone(630, 840, 0.11, 0.08, 'triangle', 0.08); break;
        case 'click':    this.tone(760, 620, 0.045, 0.05, 'square'); break;
        case 'pickup':   this.tone(680, 1040, 0.09, 0.07, 'sine'); break;
        case 'explode':  this.hiss(0.75, 240, 0.32, 0.5, 'lowpass');
                         this.tone(120, 32, 0.6, 0.20, 'sawtooth'); break;
        case 'dragon':   this.tone(105, 52, 0.9, 0.20, 'sawtooth');
                         this.tone(158, 74, 0.85, 0.11, 'square', 0.02);
                         this.hiss(0.8, 320, 0.12, 0.5, 'lowpass'); break;
        case 'breath':   this.hiss(0.24, 1500, 0.09, 0.6); break;
        case 'portal':   this.tone(180, 900, 0.55, 0.10, 'sine');
                         this.tone(360, 1400, 0.5, 0.06, 'triangle', 0.06); break;
        case 'thunder':  this.hiss(1.5, 160, 0.28, 0.4, 'lowpass');
                         this.tone(70, 28, 1.2, 0.13, 'sawtooth'); break;
        case 'levelup':  [523, 659, 784, 1047].forEach((f, i) =>
                           this.tone(f, f * 1.01, 0.18, 0.08, 'triangle', i * 0.09)); break;
        case 'splash':   this.hiss(0.32, 1800, 0.13, 0.5); break;
        default: break;
      }
    }

    /* 环境声：'none' | 'rain' | 'cave' | 'nether' | 'end' */
    setAmbient(kind) {
      this.ambKind = kind;
      if (!this.ctx || !this.ambGain) return;
      const levels = { none: 0, rain: 0.055, cave: 0.018, nether: 0.030, end: 0.022 };
      const v = this.enabled ? (levels[kind] || 0) : 0;
      this.ambGain.gain.setTargetAtTime(v, this.ctx.currentTime, 0.6);
    }
  }

  DMC.Sound = Sound;
})(typeof globalThis !== 'undefined' ? globalThis : this);
