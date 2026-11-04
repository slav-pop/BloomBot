import { describe, expect, it } from 'vitest'
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

/** Lässt die Simulation in kleinen Schritten laufen und sammelt alle Ereignisse. */
function run(sim, seconds, { dt = 0.05, speed = 1 } = {}) {
  let current = sim
  const events = []
  for (let t = 0; t < seconds; t += dt) {
    const result = step(current, dt, speed)
    current = result.sim
    events.push(...result.events)
  }
  return { sim: current, events }
}

function runUntil(sim, predicate, { maxSeconds = 600, dt = 0.05, speed = 4 } = {}) {
  let current = sim
  const events = []
  for (let t = 0; t < maxSeconds; t += dt) {
    const result = step(current, dt, speed)
    current = result.sim
    events.push(...result.events)
    if (predicate(current, events)) return { sim: current, events }
  }
  throw new Error('condition not reached')
}

describe('createSim', () => {
  it('starts idle with a moist pot and a full tank', () => {
    const sim = createSim('medium')
    expect(sim.phase).toBe(PHASE.IDLE)
    expect(sim.moisture).toBeGreaterThan(PLANT_TYPES.medium.moistTarget)
    expect(tankPercent(sim)).toBe(100)
    expect(ledStatus(sim)).toBe('okay')
  })

  it('uses the medium profile for an unknown plant type', () => {
    expect(createSim('unknown').plantKey).toBe('medium')
  })
})

describe('step', () => {
  it('does not mutate the previous state', () => {
    const sim = createSim('medium')
    const snapshot = structuredClone(sim)
    step(sim, 1, 1)
    expect(sim).toEqual(snapshot)
  })

  it('lets the soil dry out over time and advances the clock', () => {
    const sim = createSim('medium')
    const { sim: later } = run(sim, 5)
    expect(later.moisture).toBeLessThan(sim.moisture)
    expect(later.clock).toBeGreaterThan(sim.clock)
  })

  it('waters in portions until the target is reached, then rests', () => {
    const dry = { ...createSim('medium'), moisture: PLANT_TYPES.medium.moistMin - 1, sinceCheck: SIM.checkEverySec }
    const { sim, events } = runUntil(dry, (s, ev) => ev.some((e) => e.type === 'watered') && s.phase === PHASE.IDLE)

    const watered = events.filter((e) => e.type === 'watered')
    expect(watered.length).toBeGreaterThanOrEqual(1)
    expect(watered.length).toBeLessThanOrEqual(SIM.maxPortions)
    expect(watered[0].ml).toBe(PLANT_TYPES.medium.doseMl)
    expect(sim.moisture).toBeGreaterThanOrEqual(PLANT_TYPES.medium.moistTarget)
    expect(sim.tankMl).toBeCloseTo(SIM.tankCapacityMl - watered.length * PLANT_TYPES.medium.doseMl, 6)
  })

  it('shows the watering LED only while the pump runs', () => {
    const dry = { ...createSim('medium'), moisture: 10, sinceCheck: SIM.checkEverySec }
    const { sim } = step(dry, 0.01, 1)
    expect(sim.phase).toBe(PHASE.PUMPING)
    expect(ledStatus(sim)).toBe('watering')

    const soaking = runUntil(sim, (s) => s.phase === PHASE.SOAKING, { speed: 1 }).sim
    expect(ledStatus(soaking)).toBe('need')
  })

  it('never starts the pump when the soil is above the maximum', () => {
    const soaked = { ...createSim('medium'), moisture: 99, sinceCheck: SIM.checkEverySec }
    const { sim } = step(soaked, 0.01, 1)
    expect(sim.phase).toBe(PHASE.IDLE)
  })

  it('goes to ERROR (still_dry) when water does not reach the soil', () => {
    const dry = setHoseConnected({ ...createSim('medium'), moisture: 20, sinceCheck: SIM.checkEverySec }, false)
    const { sim, events } = runUntil(dry, (s) => s.phase === PHASE.ERROR)

    expect(events.filter((e) => e.type === 'watered')).toHaveLength(SIM.maxPortions)
    expect(events.at(-1)).toEqual({ type: 'error', reason: 'still_dry' })
    expect(ledStatus(sim)).toBe('error')

    const stillError = run(sim, 5).sim
    expect(stillError.phase).toBe(PHASE.ERROR)
  })

  it('clears the error on reset and checks the soil right away', () => {
    const broken = { ...createSim('medium'), phase: PHASE.ERROR, moisture: 20 }
    const { sim, events } = resetError(broken)
    expect(events).toEqual([{ type: 'error_cleared' }])
    expect(sim.phase).toBe(PHASE.IDLE)
    expect(sim.sinceCheck).toBe(SIM.checkEverySec)
  })

  it('ignores a reset outside of ERROR', () => {
    const sim = createSim('medium')
    expect(resetError(sim)).toEqual({ sim, events: [] })
  })

  it('protects the pump when the tank is empty and resumes after a refill', () => {
    const empty = {
      ...createSim('medium'),
      moisture: 20,
      tankMl: (SIM.tankEmptyPercent / 100) * SIM.tankCapacityMl,
      tankLow: true,
      sinceCheck: SIM.checkEverySec,
    }
    const { sim, events } = step(empty, 0.01, 1)
    expect(sim.phase).toBe(PHASE.TANK_EMPTY)
    expect(events).toEqual([{ type: 'tank_empty' }])
    expect(ledStatus(sim)).toBe('low')

    const refilled = refillTank(sim)
    expect(refilled.events).toEqual([{ type: 'tank_refilled', tank_pct: 100 }])
    expect(refilled.sim.phase).toBe(PHASE.IDLE)
    expect(refilled.sim.sinceCheck).toBe(SIM.checkEverySec)
  })

  it('reports tank_low once when the level falls below the threshold', () => {
    const almostLow = {
      ...createSim('medium'),
      moisture: 20,
      tankMl: SIM.tankCapacityMl * 0.21,
      sinceCheck: SIM.checkEverySec,
    }
    const { sim, events } = runUntil(almostLow, (s) => s.phase === PHASE.SOAKING)
    expect(events.filter((e) => e.type === 'tank_low')).toHaveLength(1)
    expect(sim.tankLow).toBe(true)
    expect(ledStatus({ ...sim, phase: PHASE.IDLE })).toBe('low')
  })

  it('stops pumping when the tank runs dry mid-portion', () => {
    const nearlyEmpty = {
      ...createSim('high'),
      moisture: 10,
      tankMl: SIM.tankCapacityMl * 0.07,
      tankLow: true,
      sinceCheck: SIM.checkEverySec,
    }
    const { sim, events } = runUntil(nearlyEmpty, (s) => s.phase === PHASE.TANK_EMPTY, { speed: 1 })
    expect(events.at(-1)).toEqual({ type: 'tank_empty' })
    expect(tankPercent(sim)).toBeLessThanOrEqual(SIM.tankEmptyPercent)
  })
})

describe('controls', () => {
  it('switches the plant profile and keeps the rest', () => {
    const sim = { ...createSim('medium'), tankMl: 900 }
    const next = setPlantType(sim, 'low')
    expect(next.plantKey).toBe('low')
    expect(next.tankMl).toBe(900)
    expect(setPlantType(sim, 'nope')).toBe(sim)
  })

  it('refilling a full tank in IDLE only reports the level', () => {
    const { sim, events } = refillTank(createSim('medium'))
    expect(sim.phase).toBe(PHASE.IDLE)
    expect(events).toEqual([{ type: 'tank_refilled', tank_pct: 100 }])
  })

  it('reconnecting the hose lets water reach the soil again', () => {
    const sim = setHoseConnected(createSim('medium'), false)
    expect(sim.hoseConnected).toBe(false)
    expect(setHoseConnected(sim, true).hoseConnected).toBe(true)
  })
})
