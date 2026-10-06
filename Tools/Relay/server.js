// Relay del MODO ONLINE de Sculpturillo.
//
// El juego (PC del streamer / anfitrión) se conecta a /host y recibe un CÓDIGO de sala. Los celulares
// entran a https://<dominio>/<CODIGO>, cargan la página y abren un WebSocket a /ws?room=<CODIGO>.
// El relay solo REENVÍA mensajes entre los celulares y el juego: toda la lógica vive en el juego.
//
// Protocolo relay ↔ juego (JSON):
//   juego → relay : {t:"hello", code?, secret?}            (code+secret = retomar la sala tras un corte)
//                   {t:"_send", c, d} {t:"_bcast", d} {t:"_kick", c}
//   relay → juego : {t:"room", code, secret}                (sala lista)
//                   {t:"_open", c} {t:"_msg", c, d} {t:"_close", c}
// Los celulares reciben/mandan el texto "d" tal cual (el mismo protocolo que el modo local por WiFi).

"use strict";
const http = require("http");
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const { WebSocketServer } = require("ws");

const PORT            = +(process.env.PORT || 8080);
const MAX_ROOMS       = +(process.env.MAX_ROOMS || 200);
const MAX_PHONES      = +(process.env.MAX_PHONES_PER_ROOM || 100);
const HOST_GRACE_MS   = +(process.env.HOST_GRACE_SECONDS || 120) * 1000; // el juego puede reconectar
const MAX_PHONE_MSG   = 4 * 1024;
const PHONE_RATE      = 20;          // mensajes...
const PHONE_RATE_MS   = 5000;        // ...cada 5 s por celular
const ROOM_ALPHABET   = "BCDFGHJKLMNPQRSTVWXZ"; // sin vocales (no arma palabras) ni letras confusas

const PAGE_PATH = path.join(__dirname, "public", "index.html");
let pageCache = null;
function page() {
  if (!pageCache) {
    // La página es la misma del modo local; acá se le avisa que está detrás del relay.
    pageCache = fs.readFileSync(PAGE_PATH, "utf8").replace("<!--PT_RELAY-->", "<script>window.PT_RELAY=true</script>");
  }
  return pageCache;
}

/** @type {Map<string, Room>} */
const rooms = new Map();
let nextConnId = 1;

class Room {
  constructor(code) {
    this.code = code;
    this.secret = crypto.randomBytes(16).toString("hex");
    this.host = null;            // WebSocket del juego
    this.hostLostAt = 0;         // >0 mientras el juego está desconectado (gracia)
    this.phones = new Map();     // connId → WebSocket
  }
  toHost(obj) {
    if (this.host && this.host.readyState === 1) this.host.send(JSON.stringify(obj));
  }
  close(reason) {
    for (const ws of this.phones.values()) {
      try { ws.send(JSON.stringify({ t: "error", code: "room" })); ws.close(4000, reason); } catch (e) {}
    }
    this.phones.clear();
    if (this.host) try { this.host.close(4000, reason); } catch (e) {}
    rooms.delete(this.code);
    log(`sala ${this.code} cerrada (${reason}). salas: ${rooms.size}`);
  }
}

function log(...a) { console.log(new Date().toISOString(), ...a); }

function newCode() {
  for (let i = 0; i < 1000; i++) {
    let c = "";
    for (let j = 0; j < 4; j++) c += ROOM_ALPHABET[crypto.randomInt(ROOM_ALPHABET.length)];
    if (!rooms.has(c)) return c;
  }
  return null;
}

// ── HTTP: página + salud ─────────────────────────────────────────────────────
const server = http.createServer((req, res) => {
  const url = new URL(req.url, "http://x");
  if (url.pathname === "/health") {
    res.writeHead(200, { "Content-Type": "application/json" });
    return res.end(JSON.stringify({ ok: true, rooms: rooms.size }));
  }
  // "/" o "/KQZT" → la página del celular (el código lo lee la página desde la URL).
  if (url.pathname === "/" || /^\/[A-Za-z]{4}\/?$/.test(url.pathname) || url.pathname === "/index.html") {
    try {
      res.writeHead(200, { "Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-cache" });
      return res.end(page());
    } catch (e) {
      res.writeHead(500); return res.end("Falta public/index.html");
    }
  }
  res.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" });
  res.end("No encontrado");
});

// ── WebSockets ───────────────────────────────────────────────────────────────
const wss = new WebSocketServer({ noServer: true, maxPayload: 256 * 1024 });

server.on("upgrade", (req, socket, head) => {
  const url = new URL(req.url, "http://x");
  if (url.pathname !== "/host" && url.pathname !== "/ws") return socket.destroy();
  wss.handleUpgrade(req, socket, head, ws => {
    ws.isAlive = true;
    ws.on("pong", () => { ws.isAlive = true; });
    if (url.pathname === "/host") onHost(ws);
    else onPhone(ws, (url.searchParams.get("room") || "").toUpperCase());
  });
});

function onHost(ws) {
  let room = null;
  ws.once("message", raw => {
    let m; try { m = JSON.parse(raw); } catch (e) { return ws.close(4002, "bad hello"); }
    if (m.t !== "hello") return ws.close(4002, "bad hello");

    // ¿Retomar una sala existente? (el juego se cortó y volvió con su código + secreto)
    const prev = m.code && rooms.get(String(m.code).toUpperCase());
    if (prev && prev.secret === m.secret) {
      room = prev;
      if (room.host && room.host !== ws) try { room.host.close(4001, "replaced"); } catch (e) {}
    } else {
      if (rooms.size >= MAX_ROOMS) return ws.close(4003, "full");
      const code = newCode();
      if (!code) return ws.close(4003, "full");
      room = new Room(code);
      rooms.set(code, room);
      log(`sala ${code} creada. salas: ${rooms.size}`);
    }
    room.host = ws;
    room.hostLostAt = 0;
    ws.send(JSON.stringify({ t: "room", code: room.code, secret: room.secret }));
    // Si retomó, avisarle quiénes siguen conectados.
    for (const id of room.phones.keys()) room.toHost({ t: "_open", c: id });

    ws.on("message", raw2 => {
      let x; try { x = JSON.parse(raw2); } catch (e) { return; }
      if (x.t === "_send") {
        const p = room.phones.get(x.c);
        if (p && p.readyState === 1) p.send(String(x.d));
      } else if (x.t === "_bcast") {
        const d = String(x.d);
        for (const p of room.phones.values()) if (p.readyState === 1) p.send(d);
      } else if (x.t === "_kick") {
        const p = room.phones.get(x.c);
        if (p) try { p.close(4004, "kicked"); } catch (e) {}
      } else if (x.t === "close") {
        room.close("host");
      }
    });
  });

  ws.on("close", () => {
    if (room && room.host === ws) {
      room.host = null;
      room.hostLostAt = Date.now();
      log(`sala ${room.code}: el juego se desconectó (espera ${HOST_GRACE_MS / 1000}s)`);
    }
  });
}

function onPhone(ws, code) {
  const room = rooms.get(code);
  if (!room) {
    ws.send(JSON.stringify({ t: "error", code: "room" }));
    return ws.close(4000, "no room");
  }
  if (room.phones.size >= MAX_PHONES) {
    ws.send(JSON.stringify({ t: "error", code: "full" }));
    return ws.close(4003, "full");
  }
  const id = nextConnId++;
  room.phones.set(id, ws);
  room.toHost({ t: "_open", c: id });

  let windowStart = Date.now(), count = 0;
  ws.on("message", raw => {
    if (raw.length > MAX_PHONE_MSG) return;
    const now = Date.now();
    if (now - windowStart > PHONE_RATE_MS) { windowStart = now; count = 0; }
    if (++count > PHONE_RATE) return; // anti-spam: se descarta en silencio
    room.toHost({ t: "_msg", c: id, d: raw.toString() });
  });
  ws.on("close", () => {
    if (room.phones.get(id) === ws) {
      room.phones.delete(id);
      room.toHost({ t: "_close", c: id });
    }
  });
}

// Latido (Cloudflare corta conexiones inactivas) + limpieza de salas abandonadas.
setInterval(() => {
  for (const ws of wss.clients) {
    if (!ws.isAlive) { ws.terminate(); continue; }
    ws.isAlive = false;
    try { ws.ping(); } catch (e) {}
  }
  const now = Date.now();
  for (const room of [...rooms.values()])
    if (!room.host && room.hostLostAt && now - room.hostLostAt > HOST_GRACE_MS) room.close("host timeout");
}, 25000);

server.listen(PORT, () => log(`relay escuchando en :${PORT}`));
