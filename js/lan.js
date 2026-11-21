/* 2DMC Web — 局域网联机（BroadcastChannel，主机权威；对应 C++ 版的 Winsock TCP LAN） */
(function (root) {
  'use strict';
  const DMC = root.DMC || (root.DMC = {});
  const CHANNEL = '2dmc-web-lan-v1';

  class LAN {
    constructor(game) {
      this.g = game;
      this.role = 'off';                 // off | host | client
      this.bc = null;
      this.id = Math.random().toString(36).slice(2, 10);
      this.name = '玩家-' + this.id.slice(0, 3);
      this.peers = new Map();            // id -> {name,x,y,last}
      this.tick = 0;
      this.hostSeen = 0;
      this.supported = typeof root.BroadcastChannel === 'function';
    }
    get active() { return this.role !== 'off'; }

    _open() {
      if (!this.supported) return false;
      if (this.bc) return true;
      this.bc = new BroadcastChannel(CHANNEL);
      this.bc.onmessage = e => this._onMessage(e.data);
      return true;
    }
    send(type, data) {
      if (!this.bc) return;
      try { this.bc.postMessage({ type, from: this.id, name: this.name, t: Date.now(), data }); }
      catch (e) { /* 消息过大时忽略 */ }
    }
    host() {
      if (!this._open()) return '当前环境不支持 BroadcastChannel（请用 http(s) 打开，且同浏览器多标签）';
      this.role = 'host';
      this.hostSeen = Date.now();
      this.send('HELLO_HOST', {});
      return '局域网已开启：本机为主机，其它标签输入 /lan join 加入';
    }
    join() {
      if (!this._open()) return '当前环境不支持 BroadcastChannel（请用 http(s) 打开）';
      this.role = 'client';
      this.send('HELLO', {});
      return '正在寻找主机…（同一浏览器的其它标签需先 /lan host）';
    }
    stop() {
      this.send('BYE', {});
      this.role = 'off';
      this.peers.clear();
      if (this.bc) { this.bc.close(); this.bc = null; }
      return '局域网已关闭';
    }
    status() {
      if (!this.supported) return '局域网不可用（需要 http(s) 与 BroadcastChannel）';
      if (!this.active) return '局域网未开启';
      return `局域网：${this.role === 'host' ? '主机' : '客户端'} · 在线 ${this.peers.size + 1} 人 · 频道 ${CHANNEL}`;
    }

    _onMessage(msg) {
      if (!msg || msg.from === this.id || !this.active) return;
      const g = this.g, d = msg.data || {};
      switch (msg.type) {
        case 'HELLO':
          if (this.role !== 'host') return;
          this.peers.set(msg.from, { name: msg.name, x: 0, y: 0, last: Date.now() });
          this.send('WELCOME', { to: msg.from, name: this.name });
          this.sendWorld(msg.from);
          g.ui.log(`[LAN] ${msg.name} 加入了游戏`);
          break;
        case 'HELLO_HOST':
          if (this.role !== 'client') return;
          this.peers.set(msg.from, { name: msg.name, x: 0, y: 0, last: Date.now() });
          this.hostSeen = Date.now();
          this.send('HELLO', {});
          break;
        case 'WELCOME':
          if (this.role !== 'client' || (d.to && d.to !== this.id)) return;
          this.peers.set(msg.from, { name: msg.name, x: 0, y: 0, last: Date.now() });
          g.ui.toast('已加入主机 ' + msg.name);
          g.ui.log('[LAN] 已连接主机，等待世界快照…');
          break;
        case 'WORLD':
          if (this.role !== 'client') return;
          if (d.to && d.to !== this.id) return;
          this.applyWorld(d.world);
          g.ui.toast('世界同步完成');
          break;
        case 'BLOCK':
          if (d.dim === undefined || d.dim === g.world.dim) {
            g.world.set(d.x, d.y, d.id, d.dim);
            g.renderer.emit(d.x + 0.5, d.y + 0.5, '#c8b4ff', 3);
          }
          break;
        case 'PLAYER': {
          const key = d.originalFrom || msg.from;
          const nm = d.originalName || msg.name;
          const p = this.peers.get(key) || { name: nm, last: 0 };
          p.name = nm; p.x = d.x; p.y = d.y; p.last = Date.now();
          this.peers.set(key, p);
          if (this.role === 'host' && !d.relay) {
            this.send('PLAYER', { x: d.x, y: d.y, dim: d.dim, life: d.life,
                                  originalFrom: key, originalName: nm, relay: true });
          }
          break;
        }
        case 'DIM':
          if (this.role !== 'client') return;
          g.world.switchTo(d.dim);
          g.player.x = d.x; g.player.y = d.y;
          break;
        case 'STATE': {
          if (this.role !== 'client') return;
          const wasFlash = g.weather && g.weather.flash > 0.5;
          if (d.weather) g.weather = DMC.Weather.restore(d.weather, g.world.seed);
          if (typeof d.time === 'number') g.time = d.time;
          if (!wasFlash && d.weather && d.weather.flash > 0.5) g.sound.play('thunder');
          break;
        }
        case 'DRAGON':
          if (this.role !== 'client' || !g.dragon) return;
          g.dragon.hp = d.hp; g.dragon.state = d.state;
          g.dragon.alive = d.alive;
          g.dragon.head.x = d.x; g.dragon.head.y = d.y;
          for (let i = 1; i < g.dragon.segs.length; i++) {
            g.dragon.segs[i].x += (g.dragon.segs[i - 1].x - g.dragon.segs[i].x) / 6;
            g.dragon.segs[i].y += (g.dragon.segs[i - 1].y - g.dragon.segs[i].y) / 6;
          }
          break;
        case 'CHAT':
          g.ui.log('[LAN] ' + msg.name + ': ' + d.text);
          g.renderer.emit(g.player.x, g.player.y - 1, '#4de0d0', 4);
          break;
        case 'BYE':
          this.peers.delete(msg.from);
          g.ui.log('[LAN] ' + msg.name + ' 离开了游戏');
          break;
      }
    }

    sendWorld(to) {
      if (this.role !== 'host') return;
      this.send('WORLD', { to, world: this.g.world.serialize(),
                           weather: this.g.weather.serialize(), time: this.g.time });
    }
    applyWorld(o) {
      const w = DMC.World.deserialize(o);
      if (!w) { this.g.ui.toast('世界快照版本不兼容'); return; }
      this.g.world = w;
      this.g.world.switchTo(o.dim | 0);
      if (o.weather) this.g.weather = DMC.Weather.restore(o.weather, w.seed);
      if (typeof o.time === 'number') this.g.time = o.time;
      this.g.renderer.tiles.clear();
    }
    state() {
      if (!this.active || this.role !== 'host') return;
      this.send('STATE', { weather: this.g.weather.serialize(), time: this.g.time });
    }
    block(x, y, id, dim) {
      if (!this.active) return;
      this.send('BLOCK', { x, y, id, dim: dim === undefined ? this.g.world.dim : dim });
    }
    player() {
      if (!this.active) return;
      this.send('PLAYER', { x: this.g.player.x, y: this.g.player.y,
                            dim: this.g.world.dim, life: this.g.player.life });
    }
    dimension(dim) {
      if (!this.active) return;
      this.send('DIM', { dim, x: this.g.player.x, y: this.g.player.y });
      if (this.role === 'host') this.sendWorld();
    }
    dragon() {
      if (!this.active || this.role !== 'host' || !this.g.dragon) return;
      const d = this.g.dragon;
      this.send('DRAGON', { x: d.head.x, y: d.head.y, hp: d.hp,
                            state: d.state, alive: d.alive });
    }
    chat(text) {
      if (!this.active) { this.g.ui.log('[我] ' + text + '  （未开启局域网）'); return; }
      this.send('CHAT', { text });
      this.g.ui.log('[我] ' + text);
    }
    update() {
      if (!this.active) return;
      this.tick++;
      if (this.tick % 6 === 0) this.player();
      if (this.tick % 4 === 0 && this.role === 'host') this.dragon();
      if (this.tick % 30 === 0 && this.role === 'host') this.state();
      if (this.tick % 60 === 0) {
        const now = Date.now();
        for (const [k, v] of this.peers) if (now - v.last > 20000) this.peers.delete(k);
        if (this.role === 'host' && this.tick % 300 === 0) this.send('HELLO_HOST', {});
      }
      // 客户端把 peers 转成渲染用数组
      this.g.remotePlayers = Array.from(this.peers.entries()).map(([id, v]) =>
        ({ id, name: v.name, x: v.x, y: v.y }));
    }
  }

  DMC.LAN = LAN;
  DMC.LAN_CHANNEL = CHANNEL;
})(typeof globalThis !== 'undefined' ? globalThis : this);
