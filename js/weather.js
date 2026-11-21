/* 2DMC Web — 天气状态机（纯逻辑，可在 Node 里自检）
 * 主世界才有天气：晴 / 雨 / 雪 / 雷暴。雪原降雪、沙漠不下雨。
 * 渲染层读 type / intensity / flash 画雨丝、雪花、阴天与闪电。
 */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const Core = DMC.Core;
  const clamp = Core ? Core.clamp : (v, a, b) => v < a ? a : (v > b ? b : v);

  const TYPES = ['clear', 'rain', 'snow', 'thunder'];
  const NAMES = { clear: '晴', rain: '雨', snow: '雪', thunder: '雷暴' };

  class Weather {
    constructor(seed) {
      this.rng = Core.makeRNG((seed >>> 0) ^ 0x5eed1234);
      this.type = 'clear';
      this.ticks = 2400;
      this.intensity = 0;
      this.flash = 0;
      this.thunderCd = 160;
      this.biome = 0;
    }
    get raining() { return this.type === 'rain' || this.type === 'thunder'; }
    get snowing() { return this.type === 'snow'; }
    get name() { return NAMES[this.type] || '晴'; }
    get skyDim() {          // 阴天对光照的削弱：0~0.3
      return this.intensity * (this.type === 'thunder' ? 0.30 :
                               this.type === 'rain' ? 0.20 :
                               this.type === 'snow' ? 0.14 : 0);
    }
    set(type, ticks) {
      if (TYPES.indexOf(type) < 0) return false;
      this.type = type;
      this.ticks = Math.max(1, ticks === undefined ? 1800 : ticks);
      this.thunderCd = 120 + Math.floor(this.rng() * 180);
      return true;
    }
    pick(biome) {
      const r = this.rng();
      let t;
      if (r < 0.56) t = 'clear';
      else if (r < 0.85) t = 'rain';
      else if (r < 0.94) t = 'thunder';
      else t = 'snow';
      // 沙漠干旱；雪原把雨变成雪；其它群系不会下雪
      if (biome === 3) t = 'clear';
      else if (biome === 1) { if (t === 'rain' || t === 'thunder') t = 'snow'; }
      else if (t === 'snow') t = 'rain';
      this.set(t, t === 'clear' ? 1800 + Math.floor(this.rng() * 2400)
                                : 900 + Math.floor(this.rng() * 1500));
    }
    /* 每帧调用；返回 'thunder' / 'start' / 'stop' / null */
    step(dim, biome) {
      this.biome = biome | 0;
      if (this.flash > 0) this.flash = Math.max(0, this.flash - 0.085);
      if (dim !== 0) {
        const was = this.type;
        this.type = 'clear';
        this.intensity = Math.max(0, this.intensity - 0.02);
        return was !== 'clear' ? 'stop' : null;
      }
      const target = this.type === 'clear' ? 0 : 1;
      this.intensity += clamp(target - this.intensity, -0.008, 0.008);
      this.intensity = clamp(this.intensity, 0, 1);
      if (--this.ticks <= 0) { this.pick(this.biome); return 'change'; }
      if (this.type === 'thunder' && --this.thunderCd <= 0) {
        this.thunderCd = 180 + Math.floor(this.rng() * 320);
        this.flash = 1;
        return 'thunder';
      }
      return null;
    }
    serialize() {
      return { type: this.type, ticks: this.ticks, intensity: this.intensity,
               flash: this.flash, thunderCd: this.thunderCd, biome: this.biome };
    }
    static restore(d, seed) {
      const w = new Weather(seed || 1);
      if (!d) return w;
      if (TYPES.indexOf(d.type) >= 0) w.type = d.type;
      if (Number.isFinite(d.ticks)) w.ticks = d.ticks;
      if (Number.isFinite(d.intensity)) w.intensity = clamp(d.intensity, 0, 1);
      if (Number.isFinite(d.flash)) w.flash = clamp(d.flash, 0, 1);
      if (Number.isFinite(d.thunderCd)) w.thunderCd = d.thunderCd;
      if (Number.isFinite(d.biome)) w.biome = d.biome;
      return w;
    }
  }

  DMC.Weather = Weather;
  DMC.WEATHER_TYPES = TYPES;
  DMC.WEATHER_NAMES = NAMES;
})(typeof globalThis !== 'undefined' ? globalThis : this);
