// DOM-Test der Homepage-Simulation: echte Markup aus index.html, Animationsframes von Hand.
import { readFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'

const html = readFileSync(resolve(dirname(fileURLToPath(import.meta.url)), '../index.html'), 'utf8')
// Ohne iframe (App-Vorschau): happy-dom lädt keine Seiten nach.
const body = html.slice(html.indexOf('<body>') + 6, html.indexOf('</body>')).replace(/<iframe[^>]*><\/iframe>/g, '')

const FRAME_MS = 100
let frameCallback
let now
let observerCallback

function frames(count) {
  for (let i = 0; i < count; i++) {
    now += FRAME_MS
    frameCallback(now)
  }
}

function framesUntil(predicate, max = 5000) {
  for (let i = 0; i < max; i++) {
    if (predicate()) return
    frames(1)
  }
  throw new Error('condition not reached')
}

const $ = (selector) => document.querySelector(selector)
const logTypes = () => [...document.querySelectorAll('[data-sim-log] li[data-type]')].map((li) => li.dataset.type)
const moisture = () => parseInt($('[data-sim-moisture]').textContent, 10)

async function load({ reducedMotion = false } = {}) {
  vi.resetModules()
  document.body.innerHTML = body
  now = 0
  vi.spyOn(performance, 'now').mockReturnValue(0)
  vi.stubGlobal('requestAnimationFrame', (cb) => {
    frameCallback = cb
    return 1
  })
  vi.stubGlobal(
    'IntersectionObserver',
    class {
      constructor(cb) {
        observerCallback = cb
      }
      observe() {}
    },
  )
  vi.stubGlobal('matchMedia', (query) => ({ matches: reducedMotion && query.includes('reduce') }))
  await import('./main.js')
}

beforeEach(async () => {
  await load()
  $('[data-sim-speed]').value = '8'
  $('[data-sim-speed]').dispatchEvent(new Event('input'))
})

afterEach(() => {
  vi.unstubAllGlobals()
})

describe('homepage simulation', () => {
  it('renders the initial state', () => {
    expect(document.querySelectorAll('[data-sim-leds] .led')).toHaveLength(15)
    expect($('[data-sim-leds]').dataset.status).toBe('okay')
    expect($('[data-sim-status]').textContent).toBe('Alles gut')
    expect($('[data-sim-phase]').textContent).toBe('IDLE')
    expect($('[data-sim-reset]').hidden).toBe(true)
    expect($('[data-sim-speed-out]').value).toBe('8×')
  })

  it('dries out, waters in portions and logs it', () => {
    const start = moisture()
    framesUntil(() => logTypes().includes('watered'))
    expect(moisture()).toBeGreaterThan(0)
    expect(start).toBeGreaterThan(0)
    expect($('[data-sim-log] li time').textContent).toMatch(/^Tag \d+, \d{2}:\d{2}$/)
    expect($('.log-empty')).toBeNull()
  })

  it('shows pumping state while the pump runs', () => {
    framesUntil(() => $('[data-sim-phase]').textContent === 'PUMPING')
    expect($('[data-sim]').classList.contains('is-pumping')).toBe(true)
    expect($('[data-sim-leds]').dataset.status).toBe('watering')
    expect($('[data-sim-portion]').textContent).toMatch(/^1\/2 · 120 ml$/)
  })

  it('goes to ERROR with the hose unplugged and can be reset', () => {
    $('[data-sim-hose]').click()
    expect($('[data-sim-hose]').textContent).toBe('Schlauch anschließen')
    expect($('[data-sim]').classList.contains('is-unplugged')).toBe(true)

    framesUntil(() => $('[data-sim-phase]').textContent === 'ERROR')
    expect($('[data-sim-reset]').hidden).toBe(false)
    expect(logTypes()[0]).toBe('error')

    $('[data-sim-hose]').click()
    $('[data-sim-reset]').click()
    expect(logTypes()[0]).toBe('error_cleared')
    expect($('[data-sim-reset]').hidden).toBe(true)
  })

  it('refills the tank', () => {
    $('[data-sim-refill]').click()
    expect(logTypes()[0]).toBe('tank_refilled')
    expect($('[data-sim-tank]').textContent).toBe('100 %')
  })

  it('switches the plant profile', () => {
    const cactus = $('input[name="plant"][value="low"]')
    cactus.checked = true
    cactus.dispatchEvent(new Event('change'))
    expect($('[data-sim-portion]').textContent).toBe('80 ml')
  })

  it('pauses and resumes', () => {
    $('[data-sim-toggle]').click()
    expect($('[data-sim-toggle]').textContent).toBe('Weiter')
    const before = $('[data-sim-moisture]').textContent
    frames(200)
    expect($('[data-sim-moisture]').textContent).toBe(before)

    $('[data-sim-toggle]').click()
    expect($('[data-sim-toggle]').textContent).toBe('Pause')
  })

  it('does not compute while the section is off screen', () => {
    observerCallback([{ isIntersecting: false }])
    const before = $('[data-sim-moisture]').textContent
    frames(200)
    expect($('[data-sim-moisture]').textContent).toBe(before)
  })

  it('caps the log length', () => {
    for (let i = 0; i < 40; i++) $('[data-sim-refill]').click()
    expect(document.querySelectorAll('[data-sim-log] li')).toHaveLength(30)
  })
})

describe('reduced motion', () => {
  it('starts paused', async () => {
    await load({ reducedMotion: true })
    expect($('[data-sim-toggle]').textContent).toBe('Weiter')
  })
})
