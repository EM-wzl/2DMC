/* 2DMC Web — 用项目内 MC 皮肤拼装 2D 正面 sprite
 * 资源：assets/skins/*.png（玩家，64x64）、assets/mobs/*.png（生物，64x32）
 * 经典 UV：头 (8,8,8,8)、身 (20,20,8,12)、右腿 (4,20,4,12)、右臂 (44,20,4,12)；
 * 64x64 皮肤另有左腿 (20,52,4,12)、左臂 (36,52,4,12)。
 */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});

  const PLAYER_SKINS = ['steve', 'alex', 'ari', 'kai', 'sunny', 'e264ca25f69863bc'];
  const MOB_FILES = {
    zombie: 'zombie', skeleton: 'skeleton', creeper: 'creeper',
    sheep: 'sheep', piglin: 'pig_zombie'
  };
  const SW = 20, SH = 32;   // 统一 sprite 画布

  // part: [srcX,srcY,srcW,srcH, dstX,dstY,dstW,dstH, flip?]
  function humanoid(is64) {
    return {
      contentH: 32,
      parts: [
        [8, 8, 8, 8, 6, 0, 8, 8, 0],                       // 头
        [20, 20, 8, 12, 6, 8, 8, 12, 0],                  // 身体
        [44, 20, 4, 12, 2, 8, 4, 12, 1],                  // 左臂（镜像）
        is64 ? [36, 52, 4, 12, 14, 8, 4, 12, 0]
             : [44, 20, 4, 12, 14, 8, 4, 12, 1],         // 右臂
        [4, 20, 4, 12, 6, 20, 4, 12, 0],                  // 左腿
        is64 ? [20, 52, 4, 12, 10, 20, 4, 12, 0]
             : [4, 20, 4, 12, 10, 20, 4, 12, 1]           // 右腿
      ]
    };
  }
  const LAYOUT = {
    creeper: {
      contentH: 26,
      parts: [
        [8, 8, 8, 8, 6, 0, 8, 8, 0],
        [20, 20, 8, 12, 6, 8, 8, 12, 0],
        [4, 20, 4, 6, 6, 20, 4, 6, 0],
        [12, 20, 4, 6, 10, 20, 4, 6, 0]
      ]
    },
    sheep: {
      // sheep.png 的内容分布（8x8 扫描）：羊毛身体在 x32~48/y8~24，羊脸在 x0~8/y8~16
      contentH: 26,
      parts: [
        [32, 8, 16, 16, 1, 2, 16, 16, 0],     // 羊毛身体
        [0, 8, 8, 8, 12, 0, 8, 8, 0],        // 头（侧视时偏右上）
        [0, 20, 4, 6, 4, 18, 4, 8, 0],       // 前腿
        [8, 20, 4, 6, 12, 18, 4, 8, 0]       // 后腿
      ]
    }
  };

  class SpritePack {
    constructor() {
      this.sprites = {};        // key -> {cv, contentH}
      this.playerSkin = 'steve';
      this.loaded = 0; this.total = 0; this.failed = 0;
      this.enabled = true;
    }
    build(key, img, layout) {
      const cv = document.createElement('canvas');
      cv.width = SW; cv.height = SH;
      const g = cv.getContext('2d');
      g.imageSmoothingEnabled = false;
      for (const p of layout.parts) {
        const [sx, sy, sw, sh, dx, dy, dw, dh, flip] = p;
        g.save();
        if (flip) { g.translate(dx + dw, dy); g.scale(-1, 1); g.drawImage(img, sx, sy, sw, sh, 0, 0, dw, dh); }
        else g.drawImage(img, sx, sy, sw, sh, dx, dy, dw, dh);
        g.restore();
      }
      this.sprites[key] = { cv, contentH: layout.contentH };
    }
    load(cb, base) {
      const b = base || '';
      const jobs = [];
      PLAYER_SKINS.forEach(n => jobs.push(['p:' + n, 'assets/skins/' + n + '.png', humanoid(true)]));
      Object.keys(MOB_FILES).forEach(t => jobs.push(['m:' + t, 'assets/mobs/' + MOB_FILES[t] + '.png',
        LAYOUT[t] || humanoid(false)]));
      this.total = jobs.length;
      let done = 0;
      jobs.forEach(([key, src, layout]) => {
        const img = new Image();
        img.onload = () => {
          this.build(key, img, layout);
          if (++done === this.total) cb && cb(this);
        };
        img.onerror = () => { this.failed++; if (++done === this.total) cb && cb(this); };
        img.src = b + src;
      });
    }
    getPlayer() { return this.enabled ? (this.sprites['p:' + this.playerSkin] || null) : null; }
    getMob(type) { return this.enabled ? (this.sprites['m:' + type] || null) : null; }
    setSkin(name) {
      if (PLAYER_SKINS.indexOf(name) < 0) return false;
      this.playerSkin = name;
      try { localStorage.setItem('2dmc-web-skin', name); } catch (e) { /* 忽略 */ }
      return true;
    }
    restoreSkin() {
      try {
        const n = localStorage.getItem('2dmc-web-skin');
        if (n && PLAYER_SKINS.indexOf(n) >= 0) this.playerSkin = n;
      } catch (e) { /* 忽略 */ }
    }
  }

  DMC.SpritePack = SpritePack;
  DMC.PLAYER_SKINS = PLAYER_SKINS;
})(typeof globalThis !== 'undefined' ? globalThis : this);
