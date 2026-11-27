'use strict';
// Super Family Smash: online rooms when the game is hosted on its own web address (not inside claude.ai).
// There, claude.ai's `room` service does not exist, so this file provides the same small interface net.js uses
// (join a named room, set my presence, see everyone's) on top of the site's Convex backend (convex/rooms.ts):
// each player keeps one small row per room, updated about 10 times a second, and everyone subscribes to the room's rows.
(() => {
  if (window.claude && window.claude.use) return; // inside claude.ai: the real room service is used
  let url = null;
  try { const m = location.hash.match(/cx=([^&]+)/); if (m) { url = decodeURIComponent(m[1]); localStorage.setItem('sfs_cx', url); history.replaceState(null, '', location.pathname); } else url = localStorage.getItem('sfs_cx'); } catch (e) {}
  if (!url && window.SFS_CX && location.protocol !== 'file:') url = window.SFS_CX;
  const lib = () => window.convex;
  const me = 'p' + Math.random().toString(36).slice(2, 12);
  let client = null;
  const cx = () => client || (client = new (lib().ConvexClient)(url));
  const fn = (mod, name) => (lib().anyApi ? lib().anyApi[mod][name] : mod + ':' + name);
  window.claude = { use: async n => (n === 'room' && url && lib() && lib().ConvexClient ? ROOM : null) };
  const ROOM = { join: async name => named(name) };
  function named(name) {
    let mine = {}, dirty = true, busy = false, lastSend = 0, left = false, rows = [];
    const subs = [], cache = new Map();
    function peers() {
      const maxT = rows.reduce((a, r) => Math.max(a, r.t), 0), out = [];
      for (const r of rows) {
        if (r.peer === me || r.t < maxT - 12000) continue; // gone quiet for 12 s: they left
        let c = cache.get(r.peer);
        if (!c || c.json !== r.data) { let obj = {}; try { obj = JSON.parse(r.data); } catch (e) {} c = { json: r.data, p: { peer: r.peer, by: null, isMe: false, sameTab: false, kind: 'viewer', guest: false, presence: obj, updatedAt: Date.now() } }; cache.set(r.peer, c); }
        out.push(c.p);
      }
      out.push({ peer: me, by: null, isMe: true, sameTab: true, kind: 'viewer', guest: false, presence: mine, updatedAt: Date.now() });
      return out;
    }
    const fire = () => { if (left) return; const a = peers(); for (const f of subs) { try { f({ peers: a, joined: [], left: [], updated: [] }); } catch (e) { console.warn(e); } } };
    const unsub = cx().onUpdate(fn('rooms', 'list'), { room: name }, r => { rows = r || []; fire(); });
    const timer = setInterval(() => {
      if (left || busy) return; const now = Date.now();
      if (!dirty && now - lastSend < 2500) return; // heartbeat so others know I'm still here
      dirty = false; busy = true; lastSend = now;
      cx().mutation(fn('rooms', 'set'), { room: name, peer: me, data: JSON.stringify(mine) }).catch(() => { dirty = true; }).then(() => { busy = false; });
    }, 90);
    return { name,
      presence: async patch => { const n = Object.assign({}, mine); for (const k in patch) { if (patch[k] === null) delete n[k]; else n[k] = patch[k]; } mine = n; dirty = true; },
      peers, onPeers: f => { subs.push(f); setTimeout(fire, 0); return () => { const i = subs.indexOf(f); if (i >= 0) subs.splice(i, 1); }; },
      emit: async () => {}, on: () => () => {}, connected: () => true, onConnection: f => { setTimeout(() => f(true), 0); return () => {}; },
      leave: async () => { left = true; clearInterval(timer); try { unsub(); } catch (e) {} try { await cx().mutation(fn('rooms', 'leave'), { room: name, peer: me }); } catch (e) {} } };
  }
  addEventListener('pagehide', () => { /* rows expire by themselves when the heartbeat stops */ });
})();
