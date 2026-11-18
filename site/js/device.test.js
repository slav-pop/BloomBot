// DOM-Test "Echtes Gerät" in der Simulation (Hausmesse): echte Markup aus index.html,
// fetch/EventSource von Hand, die Browser-Simulation läuft hier nicht.
import { readFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { COMMANDS } from './device-protocol.js'
import { MAX_LOG_LINES, STALE_MS, describeState, initDeviceMode } from './device.js'

const html = readFileSync(resolve(dirname(fileURLToPath(import.meta.url)), '../index.html'), 'utf8')
const body = html.slice(html.indexOf('<body>') + 6, html.indexOf('</body>')).replace(/<iframe[^>]*><\/iframe>/g, '')

const STATE = {
  status: 'need', phase: 'SOAKING', soil: 24, tank: 61, needWater: true, attempts: 1, error: '',
  lastMl: 50, totalMl: 150, wifi: true, simSoil: true, simTank: false, hoseOn: true,
  moistMin: 30, moistTarget: 50, moistMax: 70, firmware: 'main', tour: false, glassMl: null, glassMaxMl: null,
}

class FakeEventSource {
  static last = null
  constructor(url) {
    this.url = url
    FakeEventSource.last = this
  }
  emit(event) {
    this.onmessage({ data: JSON.stringify(event) })
  }
}

let root
let fetchFn
let clock
let tick

function okJson(json) {
  return Promise.resolve({ ok: true, json: () => Promise.resolve(json) })
}

async function start(statusResponse = okJson({ connected: true, state: null })) {
  fetchFn = vi.fn((url) => (url === 'api/status' ? statusResponse : okJson({ ok: true })))
  return initDeviceMode(root, {
    fetchFn,
    EventSourceCtor: FakeEventSource,
    doc: document,
    now: () => clock,
    setIntervalFn: (fn) => { tick = fn },
  })
}

const q = (selector) => root.querySelector(selector)
const key = (k) => root.querySelector(`[data-live-keys] button[data-key="${CSS.escape(k)}"]`)
const sentKeys = () => fetchFn.mock.calls.filter(([url]) => url === 'api/cmd').map(([, init]) => JSON.parse(init.body).keys)
const press = (k, extra = {}) => document.dispatchEvent(new KeyboardEvent('keydown', { key: k, bubbles: true, ...extra }))

async function startConnected(state = STATE) {
  await start()
  FakeEventSource.last.emit({ type: 'conn', connected: true })
  FakeEventSource.last.emit({ type: 'state', state })
}

beforeEach(() => {
  document.body.innerHTML = body
  root = document.querySelector('[data-sim]')
  clock = 1_000_000
  tick = null
  FakeEventSource.last = null
})

describe('describeState', () => {
  it('turns the device state into the scene view', () => {
    expect(describeState(STATE)).toEqual({
      status: 'need', statusText: 'Erde trocken – Portion 1', phase: 'SOAKING',
      moisture: 24, moistureText: '24 / 100', min: 30, target: 50, max: 70,
      tankText: '61 %', tankFraction: 0.61, portionText: '50 ml',
      pumping: false, dry: true, unplugged: false,
    })
  })

  it('shows errors, the glass, the demo and missing data', () => {
    expect(describeState({ ...STATE, status: 'error', error: 'still_dry' }).statusText).toBe('Fehler: Erde bleibt trocken')
    expect(describeState({ ...STATE, status: 'error', error: 'xyz' }).statusText).toBe('Fehler: xyz')
    expect(describeState({ ...STATE, status: 'okay', attempts: 0 }).statusText).toBe('Alles gut')
    expect(describeState({ ...STATE, glassMl: 120, glassMaxMl: 300 }).phase).toBe('SOAKING · Glas 120/300 ml')
    expect(describeState({ ...STATE, firmware: 'demo', tour: true }).phase).toBe('Demo · Tour läuft')
    const none = describeState({ ...STATE, soil: -1, tank: -1, hoseOn: false, status: 'watering' })
    expect(none).toMatchObject({ moisture: null, moistureText: '–', tankText: '–', tankFraction: 0, dry: false, unplugged: true, pumping: true })
  })
})

describe('device mode', () => {
  it('stays hidden without the bridge (GitHub Pages, dev server)', async () => {
    expect(await start(Promise.resolve({ ok: false }))).toBe(false)
    expect(await start(Promise.reject(new Error('offline')))).toBe(false)
    expect(await start(Promise.resolve({ ok: true, json: () => Promise.reject(new SyntaxError('html')) }))).toBe(false)
    expect(await start(okJson({ hello: 'world' }))).toBe(false)
    expect(q('[data-mode-switch]').hidden).toBe(true)
    expect(root.dataset.mode).toBeUndefined()
    expect(FakeEventSource.last).toBeNull()
  })

  it('starts in "Echtes Gerät" and can switch back to the simulation', async () => {
    const onMode = vi.fn()
    root.addEventListener('bloombot:mode', onMode)
    expect(await start()).toBe(true)
    expect(q('[data-mode-switch]').hidden).toBe(false)
    expect(root.dataset.mode).toBe('live')
    expect(q('[data-mode="live"]').getAttribute('aria-selected')).toBe('true')
    q('[data-mode="sim"]').click()
    expect(root.dataset.mode).toBe('sim')
    expect(q('[data-mode="sim"]').getAttribute('aria-selected')).toBe('true')
    expect(onMode).toHaveBeenCalledTimes(2)
  })

  it('draws the device state into the scene', async () => {
    await startConnected()
    expect(q('[data-sim-status]').textContent).toBe('Erde trocken – Portion 1')
    expect(q('[data-sim-moisture]').textContent).toBe('24 / 100')
    expect(q('[data-sim-tank]').textContent).toBe('61 %')
    expect(q('[data-sim-leds]').dataset.status).toBe('need')
    expect(root.classList.contains('is-dry')).toBe(true)
    expect(q('[data-device-conn]').textContent).toBe('ESP32 verbunden')
    FakeEventSource.last.emit({ type: 'conn', connected: false })
    expect(q('[data-sim-status]').textContent).toContain('Kein Gerät')
    expect(q('[data-device-conn]').textContent).toBe('ESP32 nicht verbunden')
  })

  it('waits for the first state line', async () => {
    await start()
    FakeEventSource.last.emit({ type: 'conn', connected: true })
    expect(q('[data-sim-status]').textContent).toBe('Warte auf den ESP32 …')
  })

  it('does not draw over the simulation in simulation mode', async () => {
    await start()
    q('[data-mode="sim"]').click()
    FakeEventSource.last.emit({ type: 'conn', connected: true })
    FakeEventSource.last.emit({ type: 'state', state: STATE })
    expect(q('[data-sim-moisture]').textContent).not.toBe('24 / 100')
  })

  it('renders one key per command and sends it on click', async () => {
    await start()
    expect(root.querySelectorAll('[data-live-keys] button')).toHaveLength(COMMANDS.length)
    expect(key('w').dataset.tone).toBe('watering')
    key('D').click()
    await vi.waitFor(() => expect(sentKeys()).toEqual(['D']))
  })

  it('sends keyboard shortcuts only in device mode and not while typing', async () => {
    await start()
    press('D')
    press('+')
    press('F') // not allowed
    press('w', { metaKey: true })
    press('w', { repeat: true })
    const text = document.createElement('input')
    document.body.append(text)
    text.dispatchEvent(new KeyboardEvent('keydown', { key: 'w', bubbles: true }))
    document.querySelector('input[type="range"]').dispatchEvent(new KeyboardEvent('keydown', { key: 'm', bubbles: true }))
    await vi.waitFor(() => expect(sentKeys()).toEqual(['D', '+', 'm']))
    q('[data-live-keyboard]').checked = false
    press('w')
    q('[data-live-keyboard]').checked = true
    q('[data-mode="sim"]').click()
    press('w')
    await Promise.resolve()
    expect(sentKeys()).toEqual(['D', '+', 'm'])
  })

  it('ignores Caps Lock for letters', async () => {
    await start()
    const capsOn = (k) => {
      const event = new KeyboardEvent('keydown', { key: k, bubbles: true })
      event.getModifierState = (name) => name === 'CapsLock'
      document.dispatchEvent(event)
    }
    capsOn('D')
    capsOn('d')
    capsOn('+')
    await vi.waitFor(() => expect(sentKeys()).toEqual(['d', 'D', '+']))
  })

  it('adapts the keys to the display demo firmware', async () => {
    await start()
    expect(key('d').hidden).toBe(true)
    expect(q('[data-group="demo"]').hidden).toBe(true)
    FakeEventSource.last.emit({ type: 'conn', connected: true })
    FakeEventSource.last.emit({ type: 'state', state: { ...STATE, firmware: 'demo', tour: true } })
    expect(root.dataset.firmware).toBe('demo')
    expect(key('H').hidden).toBe(true)
    expect(key('o').textContent).toContain('Alles zurück')
    expect(key('d').hidden).toBe(false)
    expect(q('[data-group="demo"]').hidden).toBe(false)
    expect(key('n').hidden).toBe(true) // the demo has one plant only
    expect(q('[data-device-conn]').textContent).toBe('ESP32 verbunden · Bildschirm-Demo')
    FakeEventSource.last.emit({ type: 'state', state: STATE })
    expect(key('H').hidden).toBe(false)
    expect(key('n').hidden).toBe(false)
    expect(key('o').textContent).toContain('Echte Sensoren')
  })

  it('writes device lines into the event log and keeps it short', async () => {
    await start()
    FakeEventSource.last.emit({ type: 'line', text: 'SIM: soil index 20' })
    const first = q('[data-sim-log] li')
    expect(first.dataset.type).toBe('sim')
    expect(first.textContent).toContain('SIM: soil index 20')
    for (let i = 0; i < MAX_LOG_LINES + 5; i++) FakeEventSource.last.emit({ type: 'line', text: `line ${i}` })
    expect(q('[data-sim-log]').children).toHaveLength(MAX_LOG_LINES)
    FakeEventSource.last.onmessage({ data: '{broken' }) // ignored
  })

  it('shows the pump running when the state line stops', async () => {
    await startConnected()
    clock += STALE_MS + 1
    tick()
    expect(q('[data-sim-status]').textContent).toBe('Pumpe läuft vermutlich …')
    expect(root.classList.contains('is-pumping')).toBe(true)
  })

  it('reports a command the bridge did not accept', async () => {
    await start()
    fetchFn.mockImplementation(() => Promise.resolve({ ok: false, status: 503 }))
    press('w')
    await vi.waitFor(() => expect(q('[data-sim-log]').textContent).toContain('nicht angekommen'))
    fetchFn.mockImplementation(() => Promise.reject(new Error('down')))
    press('m')
    await vi.waitFor(() => expect(q('[data-sim-log]').textContent).toContain('Brücke nicht erreichbar'))
  })
})
