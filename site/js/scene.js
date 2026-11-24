// Zeichnet die Szene der Simulation (Tank, Topf, LED-Leiste, Anzeigen).
// Gemeinsam für die Browser-Simulation (main.js) und das echte Gerät (device.js).

export const LED_COUNT = 15 // ACTIVE_LEDS in src/config.h
const TANK_TOP = 74
const TANK_HEIGHT = 182

export const STATUS_COLOR = {
  okay: 'var(--led-okay)',
  need: 'var(--led-need)',
  watering: 'var(--led-watering)',
  low: 'var(--led-low)',
  error: 'var(--led-error)',
  connecting: 'var(--led-connecting)',
  idle: 'var(--led-idle)',
  off: 'var(--led-off)',
}

function setText(node, text) {
  if (node.textContent !== text) node.textContent = text
}

// view: { status, statusText, phase, moisture (0..100 oder null), moistureText,
//         min, target, max, tankText, tankFraction (0..1), portionText, pumping, dry, unplugged }
export function createScene(root) {
  const $ = (selector) => root.querySelector(selector)
  const el = {
    leds: $('[data-sim-leds]'),
    water: $('[data-sim-water]'),
    soil: $('[data-sim-soil]'),
    status: $('[data-sim-status]'),
    statusLed: root.querySelector('.status-led'),
    phase: $('[data-sim-phase]'),
    moisture: $('[data-sim-moisture]'),
    band: $('[data-sim-band]'),
    target: $('[data-sim-target]'),
    needle: $('[data-sim-needle]'),
    tank: $('[data-sim-tank]'),
    portion: $('[data-sim-portion]'),
  }

  if (el.leds.children.length === 0) {
    for (let i = 0; i < LED_COUNT; i++) {
      const led = document.createElement('span')
      led.className = 'led'
      led.style.setProperty('--i', String(i))
      el.leds.append(led)
    }
  }

  function render(view) {
    const color = STATUS_COLOR[view.status] ?? STATUS_COLOR.off
    el.leds.dataset.status = view.status
    root.style.setProperty('--status-color', color)
    el.statusLed.style.setProperty('--status-color', color)

    setText(el.status, view.statusText)
    setText(el.phase, view.phase)
    setText(el.moisture, view.moistureText)
    setText(el.tank, view.tankText)
    setText(el.portion, view.portionText)

    if (view.min !== null && view.max !== null) {
      el.band.style.left = `${view.min}%`
      el.band.style.width = `${view.max - view.min}%`
    }
    if (view.target !== null) el.target.style.left = `${view.target}%`
    el.needle.hidden = view.moisture === null
    if (view.moisture !== null) {
      el.needle.style.left = `${view.moisture}%`
      // trocken: hellbraun -> feucht: dunkelbraun
      const t = view.moisture / 100
      el.soil.setAttribute('fill', `rgb(${Math.round(176 - t * 96)},${Math.round(138 - t * 84)},${Math.round(100 - t * 66)})`)
    }

    const waterHeight = Math.max(0, Math.min(1, view.tankFraction)) * TANK_HEIGHT
    el.water.setAttribute('y', String(TANK_TOP + TANK_HEIGHT - waterHeight))
    el.water.setAttribute('height', String(waterHeight))

    root.classList.toggle('is-pumping', view.pumping)
    root.classList.toggle('is-dry', view.dry)
    root.classList.toggle('is-unplugged', view.unplugged)
  }

  return { render }
}
