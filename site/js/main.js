// BloomBot Homepage – zeichnet die Simulation. Die Logik steckt in simulation.js.
import {
  PHASE,
  PLANT_TYPES,
  SIM,
  createSim,
  ledStatus,
  refillTank,
  resetError,
  setHoseConnected,
  setPlantType,
  step,
  tankPercent,
} from './simulation.js'
import { initDeviceMode } from './device.js'
import { createScene } from './scene.js'

const MAX_FRAME_SEC = 0.1
const MAX_LOG_ENTRIES = 30

const STATUS_TEXT = {
  okay: 'Alles gut',
  need: 'Erde trocken, Wasser sickert ein …',
  watering: 'Pumpe läuft …',
  low: 'Wenig Wasser im Tank',
  error: 'Fehler: Erde bleibt trocken',
}

// Texte wie die geplanten Bot-Nachrichten in Integration.md §4.
const EVENT_TEXT = {
  watered: (e) => `${e.ml} ml gegossen, Feuchte jetzt ${e.moisture_pct} %`,
  tank_low: (e) => `Wenig Wasser im Tank (${e.tank_pct} %). Bitte nachfüllen.`,
  tank_empty: () => 'Die Pflanze braucht Wasser, aber der Tank ist leer!',
  tank_refilled: () => 'Tank aufgefüllt, danke.',
  error: () => 'Erde ist nach allen Portionen noch trocken. Schlauch, Sensor und Pumpe prüfen.',
  error_cleared: () => 'Fehler zurückgesetzt, BloomBot prüft die Erde sofort.',
}

function formatClock(seconds) {
  const day = Math.floor(seconds / 86400) + 1
  const minutes = Math.floor((seconds % 86400) / 60)
  const hh = String(Math.floor(minutes / 60)).padStart(2, '0')
  const mm = String(minutes % 60).padStart(2, '0')
  return `Tag ${day}, ${hh}:${mm}`
}

function initSimulation(root) {
  const $ = (selector) => root.querySelector(selector)
  const scene = createScene(root)
  const el = {
    speed: $('[data-sim-speed]'),
    speedOut: $('[data-sim-speed-out]'),
    toggle: $('[data-sim-toggle]'),
    refill: $('[data-sim-refill]'),
    hose: $('[data-sim-hose]'),
    reset: $('[data-sim-reset]'),
    log: $('[data-sim-log]'),
  }

  let sim = createSim('medium')
  let running = true
  let visible = true
  let lastFrame = performance.now()

  function log(events) {
    for (const event of events) {
      const text = EVENT_TEXT[event.type]?.(event)
      if (!text) continue

      el.log.querySelector('.log-empty')?.remove()
      const item = document.createElement('li')
      item.dataset.type = event.type
      const time = document.createElement('time')
      time.textContent = formatClock(sim.clock)
      const type = document.createElement('code')
      type.textContent = event.type
      const message = document.createElement('span')
      message.textContent = text
      item.append(time, type, message)
      el.log.prepend(item)
    }
    while (el.log.children.length > MAX_LOG_ENTRIES) el.log.lastElementChild.remove()
  }

  function apply(result) {
    sim = result.sim
    log(result.events)
    render()
  }

  function render() {
    const plant = PLANT_TYPES[sim.plantKey]
    const status = ledStatus(sim)
    const inCycle = sim.phase === PHASE.PUMPING || sim.phase === PHASE.SOAKING
    scene.render({
      status,
      statusText: status === 'low' && sim.phase === PHASE.TANK_EMPTY ? 'Tank leer, bitte auffüllen' : STATUS_TEXT[status],
      phase: sim.phase,
      moisture: sim.moisture,
      moistureText: `${Math.round(sim.moisture)} %`,
      min: plant.moistMin,
      target: plant.moistTarget,
      max: plant.moistMax,
      tankText: `${tankPercent(sim)} %`,
      tankFraction: sim.tankMl / SIM.tankCapacityMl,
      portionText: inCycle ? `${sim.attempts}/${SIM.maxPortions} · ${plant.doseMl} ml` : `${plant.doseMl} ml`,
      pumping: sim.phase === PHASE.PUMPING,
      dry: sim.moisture < plant.moistMin,
      unplugged: !sim.hoseConnected,
    })
    el.reset.hidden = sim.phase !== PHASE.ERROR
  }

  const isLive = () => root.dataset.mode === 'live'

  function frame(now) {
    const dt = Math.min(MAX_FRAME_SEC, (now - lastFrame) / 1000)
    lastFrame = now
    if (running && visible && !isLive()) apply(step(sim, dt, Number(el.speed.value)))
    requestAnimationFrame(frame)
  }

  // Zurück vom echten Gerät: wieder die Simulation zeigen
  root.addEventListener('bloombot:mode', () => {
    if (!isLive()) render()
  })

  el.speed.addEventListener('input', () => {
    el.speedOut.value = `${el.speed.value}×`
  })

  el.toggle.addEventListener('click', () => {
    running = !running
    el.toggle.textContent = running ? 'Pause' : 'Weiter'
  })

  el.refill.addEventListener('click', () => apply(refillTank(sim)))
  el.reset.addEventListener('click', () => apply(resetError(sim)))

  el.hose.addEventListener('click', () => {
    const connected = !sim.hoseConnected
    apply({ sim: setHoseConnected(sim, connected), events: [] })
    el.hose.textContent = connected ? 'Schlauch abziehen' : 'Schlauch anschließen'
    el.hose.classList.toggle('is-active', !connected)
  })

  root.querySelectorAll('input[name="plant"]').forEach((input) => {
    input.addEventListener('change', () => apply({ sim: setPlantType(sim, input.value), events: [] }))
  })

  // Außerhalb des Bildschirms nicht rechnen – spart Akku.
  new IntersectionObserver(([entry]) => {
    visible = entry.isIntersecting
  }).observe(root)

  // Bei "Bewegung reduzieren" startet die Simulation pausiert.
  if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) el.toggle.click()

  render()
  requestAnimationFrame(frame)
}

const simRoot = document.querySelector('[data-sim]')
if (simRoot) {
  initSimulation(simRoot)
  // Hausmesse: dieselbe Szene zeigt das echte Gerät (nur über scripts/device-bridge.mjs)
  initDeviceMode(simRoot)
}
