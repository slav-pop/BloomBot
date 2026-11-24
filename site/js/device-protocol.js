// Protokoll zwischen Webseite, Brücke (scripts/device-bridge.mjs) und ESP32.
// Die Webseite schickt dieselben Ein-Zeichen-Befehle wie das Telnet-Terminal
// (src/main.cpp, Simulation: src/sim.h). Eine Quelle für Seite UND Brücke:
// nur diese Tasten kommen beim Gerät an.

// group: 'sim' = simulierte Erde/Tank, 'device' = echte Aktion, 'info' = Infos ins Log,
//        'show' = LED-Leiste/Bildschirm erzwingen, 'demo' = nur Bildschirm-Demo (demo/display_demo/main.cpp)
// only: nur bei dieser Firmware zeigen; demoLabel: andere Bedeutung in der Bildschirm-Demo
// tone: Farbpunkt auf der Taste (LED-Farbe der Szene, die sie auslöst)
export const COMMANDS = Object.freeze([
  { key: 'D', group: 'sim', label: 'Erde trocken', tone: 'need' },
  { key: '-', group: 'sim', label: 'Erde −10' },
  { key: '+', group: 'sim', label: 'Erde +10' },
  { key: 'L', group: 'sim', label: 'Tank fast leer', tone: 'low' },
  { key: '[', group: 'sim', label: 'Tank −10 %' },
  { key: ']', group: 'sim', label: 'Tank +10 %' },
  { key: 'V', group: 'sim', label: 'Tank voll', tone: 'okay' },
  { key: 'H', group: 'sim', label: 'Schlauch ab/an', only: 'main', tone: 'error' },
  { key: 'o', group: 'sim', label: 'Echte Sensoren', demoLabel: 'Alles zurück' },
  { key: 'w', group: 'device', label: 'Jetzt gießen', tone: 'watering' },
  { key: 'r', group: 'device', label: 'Fehler zurücksetzen' },
  { key: 'm', group: 'device', label: 'Erde messen' },
  { key: 'n', group: 'device', label: 'Status an Server', only: 'main' },
  { key: 's', group: 'info', label: 'Gießstatus' },
  { key: 't', group: 'info', label: 'Tank' },
  { key: 'i', group: 'info', label: 'WLAN' },
  { key: 'f', group: 'info', label: 'Bildschirm' },
  { key: '0', group: 'show', label: 'Start', tone: 'idle' },
  { key: '1', group: 'show', label: 'Durst', tone: 'need' },
  { key: '2', group: 'show', label: 'Gießen', tone: 'watering' },
  { key: '3', group: 'show', label: 'Tank leer', tone: 'low' },
  { key: '4', group: 'show', label: 'WLAN', tone: 'connecting' },
  { key: '5', group: 'show', label: 'Alles gut', tone: 'okay' },
  { key: '6', group: 'show', label: 'Fehler', tone: 'error' },
  { key: 'a', group: 'show', label: 'Automatik', demoLabel: 'Tour an' },
  { key: 'd', group: 'demo', label: 'Tour an/aus', only: 'demo' },
  { key: 'x', group: 'demo', label: 'Fehlertext wechseln', only: 'demo' },
])

// 'j' fragt nur die Statuszeile ab (macht die Brücke selbst, jede Sekunde)
export const STATE_REQUEST_KEY = 'j'
const ALLOWED = new Set([...COMMANDS.map((c) => c.key), STATE_REQUEST_KEY])
export const MAX_KEYS_PER_REQUEST = 8

// Nur erlaubte Tasten, höchstens MAX_KEYS_PER_REQUEST. Sonst null.
export function sanitizeKeys(input) {
  if (typeof input !== 'string' || input.length === 0 || input.length > MAX_KEYS_PER_REQUEST) return null
  for (const ch of input) if (!ALLOWED.has(ch)) return null
  return input
}

// Reihenfolge wie enum LedStatus in src/state.h
export const LED_STATUS = Object.freeze(['idle', 'need', 'watering', 'low', 'connecting', 'okay', 'error'])

const STATE_PREFIX = 'STATE '

function isInt(v) {
  return Number.isInteger(v)
}

// "STATE {...}" aus src/sim.cpp -> geprüftes Objekt, sonst null
export function parseStateLine(line) {
  if (typeof line !== 'string' || !line.startsWith(STATE_PREFIX)) return null
  let raw
  try {
    raw = JSON.parse(line.slice(STATE_PREFIX.length))
  } catch {
    return null
  }
  if (!raw || typeof raw !== 'object' || !isInt(raw.st) || typeof raw.ph !== 'string') return null
  if (!isInt(raw.soil) || !isInt(raw.tank) || !raw.sim || typeof raw.sim !== 'object') return null
  return {
    status: LED_STATUS[raw.st] ?? 'idle',
    phase: raw.ph,
    soil: raw.soil,
    tank: raw.tank,
    needWater: raw.need === 1,
    attempts: isInt(raw.att) ? raw.att : 0,
    error: typeof raw.err === 'string' ? raw.err : '',
    lastMl: isInt(raw.last) ? raw.last : 0,
    totalMl: isInt(raw.total) ? raw.total : 0,
    wifi: raw.wifi === 1,
    simSoil: isInt(raw.sim.soil) && raw.sim.soil >= 0,
    simTank: isInt(raw.sim.tank) && raw.sim.tank >= 0,
    hoseOn: raw.sim.hose !== 0,
    moistMin: isInt(raw.min) ? raw.min : null,
    moistTarget: isInt(raw.tgt) ? raw.tgt : null,
    moistMax: isInt(raw.max) ? raw.max : null,
    firmware: raw.fw === 'demo' ? 'demo' : 'main',
    tour: raw.tour === 1,
    glassMl: isInt(raw.glass) ? raw.glass : null,      // Wasser im Glas seit 'G' (src/sim.cpp)
    glassMaxMl: isInt(raw.glassMax) ? raw.glassMax : null,
  }
}

// TCP liefert Stücke, keine Zeilen: rest = angefangene Zeile für das nächste Stück
export function splitLines(rest, chunk) {
  const parts = (rest + chunk).split(/\r?\n/)
  const tail = parts.pop()
  return { lines: parts.filter((l) => l.length > 0), rest: tail }
}
