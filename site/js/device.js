// "Echtes Gerät" in der Simulation (Hausmesse): dieselbe Szene zeigt den ESP32,
// Tasten schicken dieselben Befehle wie das Telnet-Terminal. Nur sichtbar, wenn die
// Seite über scripts/device-bridge.mjs läuft – auf GitHub Pages gibt es /api/status nicht.
import { COMMANDS, sanitizeKeys } from './device-protocol.js'
import { createScene } from './scene.js'

export const MAX_LOG_LINES = 30
export const STALE_MS = 2500 // keine Statuszeile so lange: Gerät pumpt (Logger hält Text zurück)

const GROUPS = [
  { id: 'sim', title: 'Erde & Tank simulieren' },
  { id: 'device', title: 'Gerät' },
  { id: 'show', title: 'Szene auf Bildschirm & LED-Leiste' },
  { id: 'demo', title: 'Nur Bildschirm-Demo' },
  { id: 'info', title: 'Infos ins Log', compact: true },
]

const STATUS_TEXT = {
  idle: 'Startet – noch keine Messwerte',
  need: 'Erde trocken',
  watering: 'Pumpe läuft',
  low: 'Wenig Wasser im Tank',
  connecting: 'WLAN verbindet',
  okay: 'Alles gut',
  error: 'Fehler',
}

const ERROR_TEXT = {
  still_dry: 'Erde bleibt trocken',
  pump_timeout: 'Pumpe lief zu lange',
  sensor_tank: 'Tanksensor',
  sensor_moisture: 'Bodensensor',
}

const NON_TEXT_INPUTS = ['checkbox', 'radio', 'range', 'button', 'submit', 'reset', 'color']

// Status vom ESP32 -> Ansicht der Szene (scene.js)
export function describeState(s) {
  let statusText = STATUS_TEXT[s.status] ?? s.status
  if (s.status === 'error') statusText = `Fehler: ${ERROR_TEXT[s.error] ?? s.error}`
  else if (s.attempts > 0) statusText += ` – Portion ${s.attempts}`
  const phase = s.firmware === 'demo' ? `Demo · Tour ${s.tour ? 'läuft' : 'aus'}` : s.phase
  const glass = s.glassMaxMl ? ` · Glas ${s.glassMl}/${s.glassMaxMl} ml` : ''
  const hasSoil = s.soil >= 0
  return {
    status: s.status,
    statusText,
    phase: phase + glass,
    moisture: hasSoil ? s.soil : null,
    moistureText: hasSoil ? `${s.soil} / 100` : '–',
    min: s.moistMin,
    target: s.moistTarget,
    max: s.moistMax,
    tankText: s.tank >= 0 ? `${s.tank} %` : '–',
    tankFraction: s.tank >= 0 ? s.tank / 100 : 0,
    portionText: `${s.lastMl} ml`,
    pumping: s.status === 'watering',
    dry: hasSoil && s.moistMin !== null && s.soil < s.moistMin,
    unplugged: !s.hoseOn,
  }
}

const OFFLINE_VIEW = {
  status: 'off', statusText: 'Kein Gerät – Hotspot und ESP32 prüfen', phase: 'offline',
  moisture: null, moistureText: '–', min: null, target: null, max: null,
  tankText: '–', tankFraction: 0, portionText: '–', pumping: false, dry: false, unplugged: false,
}

// Nur echte Texteingaben blockieren die Tasten (Checkbox und Schieberegler nicht)
function isTyping(target) {
  if (!target || !target.tagName) return false
  if (target.tagName === 'INPUT') return !NON_TEXT_INPUTS.includes(target.type)
  return ['TEXTAREA', 'SELECT'].includes(target.tagName) || target.isContentEditable
}

// Feststelltaste: 'd' bleibt 'd' (sonst würde 'D' = Erde trocken geschickt)
function keyFromEvent(event) {
  const key = event.key
  if (!/^[a-zA-Z]$/.test(key) || !event.getModifierState?.('CapsLock')) return key
  return key === key.toUpperCase() ? key.toLowerCase() : key.toUpperCase()
}

function renderKeys(container, doc) {
  for (const group of GROUPS) {
    const box = doc.createElement('div')
    box.className = group.compact ? 'key-group key-group-compact' : 'key-group'
    box.dataset.group = group.id
    const title = doc.createElement('h4')
    title.textContent = group.title
    const row = doc.createElement('div')
    row.className = 'key-row'
    for (const { key, label, only, tone } of COMMANDS.filter((c) => c.group === group.id)) {
      const button = doc.createElement('button')
      button.type = 'button'
      button.className = 'key'
      button.dataset.key = key
      if (tone) button.dataset.tone = tone
      button.hidden = only === 'demo' // bis das Gerät meldet, welche Firmware läuft
      const cap = doc.createElement('kbd')
      cap.textContent = key
      const text = doc.createElement('span')
      text.className = 'key-label'
      text.textContent = label
      button.append(cap, text)
      row.append(button)
    }
    box.hidden = [...row.children].every((b) => b.hidden)
    box.append(title, row)
    container.append(box)
  }
}

export async function initDeviceMode(root, {
  fetchFn = globalThis.fetch,
  EventSourceCtor = globalThis.EventSource,
  doc = document,
  now = Date.now,
  setIntervalFn = setInterval,
} = {}) {
  try {
    const res = await fetchFn('api/status', { cache: 'no-store' })
    if (!res.ok) return false
    // Nur die echte Brücke antwortet mit diesem JSON (ein Dev-Server liefert sonst index.html)
    const status = await res.json()
    if (typeof status?.connected !== 'boolean') return false
  } catch {
    return false
  }

  const scene = createScene(root)
  const $ = (selector) => root.querySelector(selector)
  const switcher = $('[data-mode-switch]')
  const conn = $('[data-device-conn]')
  const keysBox = $('[data-live-keys]')
  const keyboard = $('[data-live-keyboard]')
  const log = $('[data-sim-log]')
  let connected = false
  let lastState = null
  let lastStateAt = 0

  const isLive = () => root.dataset.mode === 'live'

  function addLog(text, kind = 'esp32') {
    log.querySelector('.log-empty')?.remove()
    const item = doc.createElement('li')
    item.dataset.type = kind
    const time = doc.createElement('time')
    time.textContent = new Date(now()).toLocaleTimeString('de-DE')
    const type = doc.createElement('code')
    type.textContent = kind
    const message = doc.createElement('span')
    message.textContent = text
    item.append(time, type, message)
    log.prepend(item)
    while (log.children.length > MAX_LOG_LINES) log.lastElementChild.remove()
  }

  function draw() {
    if (!isLive()) return
    if (!connected) return scene.render(OFFLINE_VIEW)
    if (!lastState) return scene.render({ ...OFFLINE_VIEW, statusText: 'Warte auf den ESP32 …', phase: '…' })
    scene.render(describeState(lastState))
  }

  // Bildschirm-Demo oder echte Firmware: passende Tasten und Beschriftungen
  function showFirmware(firmware) {
    if (root.dataset.firmware === firmware) return
    root.dataset.firmware = firmware
    for (const { key, label, demoLabel, only } of COMMANDS) {
      const button = keysBox.querySelector(`button[data-key="${CSS.escape(key)}"]`)
      button.hidden = Boolean(only) && only !== firmware
      button.querySelector('.key-label').textContent = firmware === 'demo' && demoLabel ? demoLabel : label
    }
    for (const group of keysBox.querySelectorAll('.key-group')) {
      group.hidden = [...group.querySelectorAll('.key')].every((k) => k.hidden)
    }
  }

  function showConnection() {
    conn.textContent = connected
      ? `ESP32 verbunden${root.dataset.firmware === 'demo' ? ' · Bildschirm-Demo' : ''}`
      : 'ESP32 nicht verbunden'
    conn.classList.toggle('is-on', connected)
  }

  function setMode(mode) {
    root.dataset.mode = mode
    for (const tab of switcher.querySelectorAll('[data-mode]')) {
      tab.setAttribute('aria-selected', String(tab.dataset.mode === mode))
    }
    root.dispatchEvent(new CustomEvent('bloombot:mode', { detail: { mode } }))
    draw()
  }

  async function send(keys) {
    try {
      const res = await fetchFn('api/cmd', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ keys }),
      })
      if (!res.ok) addLog(`Befehl „${keys}“ nicht angekommen (${res.status})`, 'fehler')
    } catch {
      addLog('Brücke nicht erreichbar – Fenster mit der Brücke offen?', 'fehler')
    }
  }

  function press(key) {
    const button = keysBox.querySelector(`button[data-key="${CSS.escape(key)}"]`)
    button?.classList.add('is-pressed')
    setTimeout(() => button?.classList.remove('is-pressed'), 160)
    send(key)
  }

  renderKeys(keysBox, doc)
  keysBox.addEventListener('click', (event) => {
    const button = event.target.closest('button[data-key]')
    if (button) press(button.dataset.key)
  })
  switcher.addEventListener('click', (event) => {
    const tab = event.target.closest('[data-mode]')
    if (tab) setMode(tab.dataset.mode)
  })

  doc.addEventListener('keydown', (event) => {
    if (!isLive() || !keyboard.checked || event.repeat || event.ctrlKey || event.metaKey || event.altKey) return
    if (isTyping(event.target)) return
    const keys = sanitizeKeys(keyFromEvent(event))
    if (!keys || !COMMANDS.some((c) => c.key === keys)) return
    event.preventDefault()
    press(keys)
  })

  const events = new EventSourceCtor('api/events')
  events.onmessage = (message) => {
    let event
    try {
      event = JSON.parse(message.data)
    } catch {
      return
    }
    if (event.type === 'conn') {
      connected = event.connected
      showConnection()
      draw()
    } else if (event.type === 'state') {
      lastState = event.state
      lastStateAt = now()
      showFirmware(event.state.firmware)
      showConnection()
      draw()
    } else if (event.type === 'line') {
      addLog(event.text, event.text.startsWith('SIM:') ? 'sim' : 'esp32')
    }
  }

  // Während die Pumpe läuft, schickt der ESP32 keine Statuszeile (src/sim.cpp)
  setIntervalFn(() => {
    if (isLive() && connected && lastState && now() - lastStateAt > STALE_MS) {
      scene.render({
        ...describeState(lastState), status: 'watering', pumping: true,
        statusText: 'Pumpe läuft vermutlich …',
      })
    }
  }, 500)

  const empty = log.querySelector('.log-empty')
  if (empty) empty.textContent = 'Noch keine Meldung vom ESP32. Drück eine Taste – D, w oder 1 bis 6.'
  switcher.hidden = false
  showConnection()
  setMode('live')
  return true
}
