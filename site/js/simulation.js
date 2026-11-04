// BloomBot – Simulation der Gießlogik im Browser.
// Bildet die Zustandsmaschine aus Integration.md §8 nach und erzeugt dieselben
// Ereignisse wie das Gerät (§4). Reine Funktionen: jeder Schritt liefert einen neuen Zustand.

export const PHASE = Object.freeze({
  IDLE: 'IDLE',
  PUMPING: 'PUMPING',
  SOAKING: 'SOAKING',
  TANK_EMPTY: 'TANK_EMPTY',
  ERROR: 'ERROR',
})

/**
 * Werte aus src/config.h und Integration.md; Topf mit 1 Liter Erde.
 * maxPortions folgt der Firmware (MAX_WATER_ATTEMPTS = 2). Integration.md §8 nennt 3 – offen im Team.
 * min_pause_h (§5/§8) wird nicht simuliert: der Koeffizient ist laut §9 noch TBD.
 */
export const SIM = Object.freeze({
  tankCapacityMl: 2000,
  tankEmptyPercent: 5,
  tankLowOnPercent: 20,
  tankLowOffPercent: 25,
  pumpFlowMlPerSec: 23,
  maxPortions: 2,
  checkEverySec: 3600,
  soakSec: 600,
  dryingPercentPerHour: 1.2,
  moisturePercentPerMl: 0.12,
})

/** Die drei Profilgruppen aus data/plants.json (water_need_category), Dosis für 1 l Erde. */
export const PLANT_TYPES = Object.freeze({
  low: Object.freeze({ label: 'Kaktus & Sukkulente', moistMin: 15, moistTarget: 35, moistMax: 55, doseMl: 80 }),
  medium: Object.freeze({ label: 'Grünpflanze', moistMin: 30, moistTarget: 50, moistMax: 70, doseMl: 120 }),
  high: Object.freeze({ label: 'Farn & Calathea', moistMin: 45, moistTarget: 65, moistMax: 85, doseMl: 160 }),
})

// Simulierte Sekunden pro echter Sekunde: Warten läuft im Zeitraffer, die Pumpe fast in Echtzeit.
const TIME_SCALE = Object.freeze({
  [PHASE.IDLE]: 1800,
  [PHASE.SOAKING]: 300,
  [PHASE.PUMPING]: 2,
  [PHASE.TANK_EMPTY]: 1800,
  [PHASE.ERROR]: 1800,
})

const SECONDS_PER_HOUR = 3600

const clamp = (value, min, max) => Math.min(max, Math.max(min, value))

export function tankPercent(sim) {
  return Math.round((sim.tankMl / SIM.tankCapacityMl) * 100)
}

export function createSim(plantKey = 'medium') {
  const key = PLANT_TYPES[plantKey] ? plantKey : 'medium'
  const plant = PLANT_TYPES[key]
  return {
    plantKey: key,
    clock: 0,
    phase: PHASE.IDLE,
    moisture: Math.min(plant.moistMax - 5, plant.moistTarget + 15),
    tankMl: SIM.tankCapacityMl,
    tankLow: false,
    needWater: false,
    hoseConnected: true,
    attempts: 0,
    sinceCheck: 0,
    phaseElapsed: 0,
    doseLeftMl: 0,
    portionMl: 0,
  }
}

/** LED-Status mit derselben Priorität wie updateStatus() in src/main.cpp. */
export function ledStatus(sim) {
  if (sim.phase === PHASE.ERROR) return 'error'
  if (sim.phase === PHASE.PUMPING) return 'watering'
  if (sim.tankLow || sim.phase === PHASE.TANK_EMPTY) return 'low'
  if (sim.needWater) return 'need'
  return 'okay'
}

function enter(sim, phase) {
  return { ...sim, phase, phaseElapsed: 0 }
}

function updateTankLow(sim) {
  const pct = tankPercent(sim)
  if (!sim.tankLow && pct < SIM.tankLowOnPercent) {
    return { sim: { ...sim, tankLow: true }, events: [{ type: 'tank_low', tank_pct: pct }] }
  }
  if (sim.tankLow && pct > SIM.tankLowOffPercent) {
    return { sim: { ...sim, tankLow: false }, events: [] }
  }
  return { sim, events: [] }
}

function startPortion(sim) {
  const plant = PLANT_TYPES[sim.plantKey]
  if (sim.moisture >= plant.moistMax) {
    return { sim: enter({ ...sim, needWater: false }, PHASE.IDLE), events: [] }
  }
  if (tankPercent(sim) <= SIM.tankEmptyPercent) {
    return { sim: enter({ ...sim, needWater: true }, PHASE.TANK_EMPTY), events: [{ type: 'tank_empty' }] }
  }
  const pumping = { ...sim, needWater: true, attempts: sim.attempts + 1, doseLeftMl: plant.doseMl, portionMl: 0 }
  return { sim: enter(pumping, PHASE.PUMPING), events: [] }
}

function checkSoil(sim, afterWatering) {
  const plant = PLANT_TYPES[sim.plantKey]
  const checked = { ...sim, sinceCheck: 0 }
  const rest = () => ({ sim: enter({ ...checked, needWater: false, attempts: 0 }, PHASE.IDLE), events: [] })

  if (!afterWatering) {
    return checked.moisture >= plant.moistMin ? rest() : startPortion({ ...checked, attempts: 0 })
  }
  if (checked.moisture >= plant.moistTarget) return rest()
  if (checked.attempts < SIM.maxPortions) return startPortion(checked)
  if (checked.moisture >= plant.moistMin) return rest()

  return {
    sim: enter(checked, PHASE.ERROR),
    events: [{ type: 'error', reason: 'still_dry' }],
  }
}

function pump(sim, dt) {
  const ml = Math.min(SIM.pumpFlowMlPerSec * dt, sim.doseLeftMl, sim.tankMl)
  const moistureGain = sim.hoseConnected ? ml * SIM.moisturePercentPerMl : 0
  const pumped = {
    ...sim,
    tankMl: sim.tankMl - ml,
    doseLeftMl: sim.doseLeftMl - ml,
    portionMl: sim.portionMl + ml,
    moisture: clamp(sim.moisture + moistureGain, 0, 100),
  }
  const { sim: tracked, events } = updateTankLow(pumped)

  if (tankPercent(tracked) <= SIM.tankEmptyPercent && tracked.doseLeftMl > 0) {
    return { sim: enter(tracked, PHASE.TANK_EMPTY), events: [...events, { type: 'tank_empty' }] }
  }
  if (tracked.doseLeftMl <= 0) {
    const watered = { type: 'watered', ml: Math.round(tracked.portionMl), moisture_pct: Math.round(tracked.moisture) }
    return { sim: enter(tracked, PHASE.SOAKING), events: [...events, watered] }
  }
  return { sim: tracked, events }
}

/**
 * Ein Zeitschritt.
 * @param {object} sim       aktueller Zustand (wird nicht verändert)
 * @param {number} dtReal    vergangene echte Sekunden
 * @param {number} speed     Zeitraffer-Faktor für Warten und Austrocknen
 * @returns {{ sim: object, events: object[] }}
 */
export function step(sim, dtReal, speed = 1) {
  const pumping = sim.phase === PHASE.PUMPING
  const dt = dtReal * TIME_SCALE[sim.phase] * (pumping ? 1 : speed)
  const drying = pumping ? 0 : (SIM.dryingPercentPerHour * dt) / SECONDS_PER_HOUR
  const next = { ...sim, clock: sim.clock + dt, moisture: clamp(sim.moisture - drying, 0, 100) }

  switch (next.phase) {
    case PHASE.IDLE: {
      const waited = { ...next, sinceCheck: next.sinceCheck + dt }
      return waited.sinceCheck >= SIM.checkEverySec ? checkSoil(waited, false) : { sim: waited, events: [] }
    }
    case PHASE.PUMPING:
      return pump(next, dt)
    case PHASE.SOAKING: {
      const soaked = { ...next, phaseElapsed: next.phaseElapsed + dt }
      return soaked.phaseElapsed >= SIM.soakSec ? checkSoil(soaked, true) : { sim: soaked, events: [] }
    }
    default:
      // TANK_EMPTY und ERROR warten auf den Benutzer (Tank auffüllen / Fehler zurücksetzen).
      return { sim: next, events: [] }
  }
}

export function refillTank(sim) {
  const full = { ...sim, tankMl: SIM.tankCapacityMl, tankLow: false }
  const events = [{ type: 'tank_refilled', tank_pct: 100 }]
  if (sim.phase !== PHASE.TANK_EMPTY) return { sim: full, events }
  return { sim: enter({ ...full, sinceCheck: SIM.checkEverySec, attempts: 0 }, PHASE.IDLE), events }
}

export function resetError(sim) {
  if (sim.phase !== PHASE.ERROR) return { sim, events: [] }
  const reset = { ...sim, attempts: 0, sinceCheck: SIM.checkEverySec, needWater: false }
  return { sim: enter(reset, PHASE.IDLE), events: [{ type: 'error_cleared' }] }
}

export function setPlantType(sim, plantKey) {
  return PLANT_TYPES[plantKey] ? { ...sim, plantKey } : sim
}

export function setHoseConnected(sim, hoseConnected) {
  return { ...sim, hoseConnected }
}
