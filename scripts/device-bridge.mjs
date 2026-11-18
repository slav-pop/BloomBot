#!/usr/bin/env node
// Brücke Webseite <-> ESP32 für die Hausmesse. Ohne Abhängigkeiten, nur Node.
//
//   node scripts/device-bridge.mjs --site <ordner> [--port 8643] [--telnet 2323] [--device bloombot.local:23]
//
// - liefert die gebaute Webseite aus (nur an 127.0.0.1, nur dieser Laptop),
// - hält EINE Telnet-Verbindung zum ESP32 (src/logger.h: der ESP32 nimmt nur einen Client an),
// - teilt sie mit Terminals auf 127.0.0.1:2323 (VS Code: env:monitor_bridge oder
//   pio device monitor --port socket://127.0.0.1:2323) – Monitor und Webseite gleichzeitig,
// - POST /api/cmd {"keys":"D"}  -> Tasten an das Gerät (nur erlaubte, site/js/device-protocol.js),
// - GET  /api/events            -> Server-Sent Events: Logzeilen, Gerätestatus, Verbindung,
// - GET  /api/status            -> letzter Status als JSON,
// - fragt jede Sekunde mit 'j' den Status ab (src/sim.cpp: Zeile "STATE {...}").
import { createReadStream, statSync } from 'node:fs'
import http from 'node:http'
import net from 'node:net'
import { extname, join, resolve, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import { STATE_REQUEST_KEY, parseStateLine, sanitizeKeys, splitLines } from '../site/js/device-protocol.js'

const MAX_BODY_BYTES = 256
const HEARTBEAT_MS = 15000
// So lange ohne Daten = Verbindung tot (ESP32 aus dem WLAN). Länger als PUMP_MAX_ON_MS (10 s):
// während die Pumpe läuft, schickt der ESP32 absichtlich nichts (src/logger.cpp).
const IDLE_MS = 15000
const MAX_RECONNECT_MS = 30000
const STABLE_MS = 10000 // kürzer verbunden = jemand anderes wirft uns raus -> länger warten
const CONTENT_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.webp': 'image/webp',
  '.avif': 'image/avif',
  '.ico': 'image/x-icon',
  '.woff2': 'font/woff2',
  '.txt': 'text/plain; charset=utf-8',
}

function sendJson(res, status, body) {
  res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' })
  res.end(JSON.stringify(body))
}

// Nur Anfragen an diesen Laptop selbst (schützt gegen DNS-Rebinding fremder Webseiten)
function isLocalHost(hostHeader, port) {
  return hostHeader === `127.0.0.1:${port}` || hostHeader === `localhost:${port}`
}

// Ein Origin ist nur erlaubt, wenn er auf diese Brücke zeigt (keine fremde Webseite darf gießen)
function isOwnOrigin(origin, port) {
  return origin === undefined || origin === `http://127.0.0.1:${port}` || origin === `http://localhost:${port}`
}

// Datei aus dem Seitenordner, null wenn außerhalb oder nicht vorhanden
function resolveStatic(root, urlPath) {
  let decoded
  try {
    decoded = decodeURIComponent(urlPath.split('?')[0])
  } catch {
    return null
  }
  const target = resolve(root, '.' + (decoded.endsWith('/') ? decoded + 'index.html' : decoded))
  if (target !== root && !target.startsWith(root + sep)) return null
  try {
    const stat = statSync(target)
    if (stat.isDirectory()) return resolveStatic(root, decoded + '/')
    return stat.isFile() ? target : null
  } catch {
    return null
  }
}

function readBody(req) {
  return new Promise((resolveBody, reject) => {
    let size = 0
    let body = ''
    req.on('data', (chunk) => {
      size += chunk.length
      if (size > MAX_BODY_BYTES) {
        reject(new Error('too large'))
        req.destroy()
        return
      }
      body += chunk
    })
    req.on('end', () => resolveBody(body))
    req.on('error', reject)
  })
}

// Verbindung zum ESP32: verbindet neu, wenn sie abbricht; meldet Zeilen und Status
function createDeviceLink({ host, port, reconnectMs, pollMs, idleMs, onLine, onState, onConn, log }) {
  let socket = null
  let connected = false
  let rest = ''
  let closed = false
  let retryTimer = null
  let retryDelay = reconnectMs
  let openedAt = 0
  let lastActivityAt = 0

  const pollTimer = setInterval(() => {
    if (connected) socket.write(STATE_REQUEST_KEY)
  }, pollMs)

  // Nur EMPFANGENE Daten zählen: unser 'j' jede Sekunde hält eine tote Verbindung sonst "am Leben"
  const watchdog = setInterval(() => {
    if (socket && !socket.destroyed && Date.now() - lastActivityAt > idleMs) {
      log(connected ? 'ESP32 antwortet nicht mehr – Verbindung wird neu aufgebaut' : 'ESP32 nicht erreichbar')
      socket.destroy()
    }
  }, Math.min(1000, Math.max(10, idleMs / 4)))

  function setConnected(value) {
    if (connected === value) return
    connected = value
    onConn(value)
    log(value ? `ESP32 verbunden (${host}:${port})` : 'ESP32 getrennt, neuer Versuch …')
  }

  function scheduleRetry() {
    if (closed || retryTimer) return
    retryTimer = setTimeout(() => {
      retryTimer = null
      connect()
    }, retryDelay)
  }

  function connect() {
    if (closed) return
    rest = ''
    openedAt = Date.now()
    lastActivityAt = openedAt
    socket = net.createConnection({ host, port })
    socket.setNoDelay(true)
    socket.setKeepAlive(true, 5000)
    socket.on('connect', () => setConnected(true))
    socket.on('data', (chunk) => {
      lastActivityAt = Date.now()
      const split = splitLines(rest, chunk.toString('utf8'))
      rest = split.rest
      for (const line of split.lines) {
        const state = parseStateLine(line)
        if (state) onState(state)
        else onLine(line)
      }
    })
    socket.on('error', () => {}) // "close" folgt immer
    socket.on('close', () => {
      const wasConnected = connected
      setConnected(false)
      if (wasConnected && Date.now() - openedAt >= STABLE_MS) {
        retryDelay = reconnectMs
      } else {
        retryDelay = Math.min(retryDelay * 2, MAX_RECONNECT_MS)
        if (wasConnected) log('Verbindung wird ständig getrennt – läuft noch ein Monitor direkt auf dem ESP32?')
      }
      scheduleRetry()
    })
  }

  connect()
  return {
    isConnected: () => connected,
    send: (keys) => {
      if (!connected) return false
      socket.write(keys)
      return true
    },
    close() {
      closed = true
      clearInterval(pollTimer)
      clearInterval(watchdog)
      clearTimeout(retryTimer)
      socket?.destroy()
    },
  }
}

// Terminals (VS Code Monitor) teilen sich die eine Verbindung zum ESP32: sie sehen den Log
// ohne die STATE-Zeilen und tippen Befehle wie direkt am Gerät (nur dieser Laptop).
async function startTerminalServer({ port, host, device, terminals }) {
  const server = net.createServer((socket) => {
    terminals.add(socket)
    socket.setNoDelay(true)
    socket.write('BRIDGE: Terminal über die Brücke. Befehle gehen an den ESP32, die Webseite läuft weiter.\r\n')
    socket.on('data', (data) => {
      if (!device.send(data)) socket.write('BRIDGE: ESP32 nicht verbunden.\r\n')
    })
    socket.on('error', () => {}) // "close" folgt immer
    socket.on('close', () => terminals.delete(socket))
  })
  await new Promise((r) => server.listen(port, host, r))
  return server
}

export async function startBridge({
  siteDir, port = 8643, telnetPort = null, host = '127.0.0.1', deviceHost = 'bloombot.local', devicePort = 23,
  pollMs = 1000, reconnectMs = 3000, idleMs = IDLE_MS, log = console.log,
}) {
  const root = resolve(siteDir)
  const clients = new Set()
  const terminals = new Set()
  let lastState = null
  let actualPort = port

  function broadcast(event) {
    const data = `data: ${JSON.stringify(event)}\n\n`
    for (const res of clients) res.write(data)
  }

  const device = createDeviceLink({
    host: deviceHost, port: devicePort, reconnectMs, pollMs, idleMs, log,
    onLine: (text) => {
      broadcast({ type: 'line', text })
      for (const socket of terminals) socket.write(`${text}\r\n`)
    },
    onState: (state) => {
      lastState = state
      broadcast({ type: 'state', state, at: Date.now() })
    },
    onConn: (connected) => broadcast({ type: 'conn', connected }),
  })

  async function handleCommand(req, res) {
    if (!isOwnOrigin(req.headers.origin, actualPort)) return sendJson(res, 403, { error: 'origin' })
    if (!String(req.headers['content-type'] || '').startsWith('application/json')) {
      return sendJson(res, 415, { error: 'json only' })
    }
    let keys
    try {
      keys = sanitizeKeys(JSON.parse(await readBody(req)).keys)
    } catch {
      return sendJson(res, 400, { error: 'bad request' })
    }
    if (!keys) return sendJson(res, 400, { error: 'key not allowed' })
    if (!device.isConnected()) return sendJson(res, 503, { error: 'device offline' })
    device.send(keys)
    return sendJson(res, 200, { ok: true, keys })
  }

  function handleEvents(req, res) {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream; charset=utf-8',
      'Cache-Control': 'no-store',
      Connection: 'keep-alive',
    })
    res.write(`data: ${JSON.stringify({ type: 'conn', connected: device.isConnected() })}\n\n`)
    if (lastState) res.write(`data: ${JSON.stringify({ type: 'state', state: lastState, at: Date.now() })}\n\n`)
    clients.add(res)
    req.on('close', () => clients.delete(res))
  }

  function handleStatic(req, res) {
    const file = resolveStatic(root, req.url)
    if (!file) {
      res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' })
      return res.end('Not found')
    }
    res.writeHead(200, { 'Content-Type': CONTENT_TYPES[extname(file)] ?? 'application/octet-stream' })
    createReadStream(file).on('error', () => res.destroy()).pipe(res) // z. B. Seite wird gerade neu gebaut
  }

  const server = http.createServer((req, res) => {
    if (!isLocalHost(req.headers.host, actualPort)) return sendJson(res, 403, { error: 'host' })
    const path = req.url.split('?')[0]
    if (path === '/api/cmd' && req.method === 'POST') return void handleCommand(req, res)
    if (path === '/api/events' && req.method === 'GET') return handleEvents(req, res)
    if (path === '/api/status' && req.method === 'GET') {
      return sendJson(res, 200, { connected: device.isConnected(), device: `${deviceHost}:${devicePort}`, state: lastState })
    }
    if (req.method === 'GET') return handleStatic(req, res)
    return sendJson(res, 405, { error: 'method' })
  })

  const heartbeat = setInterval(() => {
    for (const res of clients) res.write(': ping\n\n')
  }, HEARTBEAT_MS)

  await new Promise((r) => server.listen(port, host, r))
  actualPort = server.address().port
  const terminalServer = telnetPort === null
    ? null : await startTerminalServer({ port: telnetPort, host, device, terminals })

  return {
    port: actualPort,
    telnetPort: terminalServer ? terminalServer.address().port : null,
    isConnected: () => device.isConnected(),
    lastState: () => lastState,
    close() {
      clearInterval(heartbeat)
      device.close()
      for (const res of clients) res.end()
      for (const socket of terminals) socket.destroy()
      terminalServer?.close()
      return new Promise((r) => server.close(r))
    },
  }
}

function parseArgs(argv) {
  const args = { site: null, port: 8643, telnet: 2323, device: 'bloombot.local:23' }
  for (let i = 0; i < argv.length; i += 2) {
    const value = argv[i + 1]
    if (argv[i] === '--site') args.site = value
    else if (argv[i] === '--port') args.port = Number(value)
    else if (argv[i] === '--telnet') args.telnet = Number(value)
    else if (argv[i] === '--device') args.device = value
  }
  return args
}

async function main() {
  const args = parseArgs(process.argv.slice(2))
  if (!args.site) {
    console.error('Aufruf: node scripts/device-bridge.mjs --site <gebaute Seite> [--port 8643] [--device bloombot.local:23]')
    process.exit(1)
  }
  const [deviceHost, devicePort = '23'] = args.device.split(':')
  const bridge = await startBridge({
    siteDir: args.site, port: args.port, telnetPort: args.telnet, deviceHost, devicePort: Number(devicePort),
  })
  console.log(`Webseite: http://127.0.0.1:${bridge.port}/   (Strg+C beendet)`)
  console.log(`ESP32: ${args.device}. Monitor NICHT direkt aufs Gerät, sondern über die Brücke:`)
  console.log(`  VS Code: Umgebung env:monitor_bridge -> Monitor, oder: pio device monitor --port socket://127.0.0.1:${bridge.telnetPort}`)
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main()
