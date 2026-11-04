import { describe, expect, it } from 'vitest'
import { DEVICE_LIMITS, computeDose, toDeviceProfile } from './deviceProfile.js'

const PROFILE = {
  moistMin: 30,
  moistTarget: 50,
  moistMax: 70,
  minDays: 6,
  maxDays: 9,
  baseMlPerLiter: 120,
  minMl: 80,
  maxMl: 600,
}

const PLANT = { id: 626, commonName: 'Chinese evergreen', profile: PROFILE }

describe('computeDose (Integration.md §5)', () => {
  it('multiplies ml per liter of soil by the pot size', () => {
    expect(computeDose(PROFILE, 2)).toEqual({ ml: 240, clamped: false })
  })

  it('rounds to whole millilitres', () => {
    expect(computeDose(PROFILE, 1.25).ml).toBe(150)
  })

  it('raises tiny pots to the plant minimum', () => {
    expect(computeDose(PROFILE, 0.5)).toEqual({ ml: 80, clamped: true })
  })

  it('caps large pots at the plant maximum', () => {
    expect(computeDose({ ...PROFILE, maxMl: 400 }, 4)).toEqual({ ml: 400, clamped: true })
  })

  it('never exceeds the range the device accepts', () => {
    const { ml, clamped } = computeDose(PROFILE, 20)
    expect(ml).toBe(DEVICE_LIMITS.doseMl.max)
    expect(clamped).toBe(true)
  })

  it('never goes below the range the device accepts', () => {
    expect(computeDose({ ...PROFILE, minMl: 1 }, 0.01).ml).toBe(DEVICE_LIMITS.doseMl.min)
  })

  it('stays in the device range even if the plant range lies outside it', () => {
    expect(computeDose({ ...PROFILE, minMl: 900, maxMl: 1200 }, 1).ml).toBe(DEVICE_LIMITS.doseMl.max)
    expect(computeDose({ ...PROFILE, minMl: 1, maxMl: 5 }, 1).ml).toBe(DEVICE_LIMITS.doseMl.min)
  })
})

describe('toDeviceProfile', () => {
  it('builds the profile the ESP32 receives', () => {
    expect(toDeviceProfile(PLANT, 2)).toEqual({
      plant_id: 626,
      name: 'Chinese evergreen',
      mode: 'normal',
      moist_min: 30,
      moist_target: 50,
      moist_max: 70,
      dose_ml: 240,
      min_pause_h: 72,
    })
  })

  it('truncates the name to what the device stores', () => {
    const long = { ...PLANT, commonName: 'x'.repeat(40) }
    expect(toDeviceProfile(long, 2).name).toHaveLength(DEVICE_LIMITS.nameMaxLength)
  })

  it('does not split characters when truncating', () => {
    const emoji = { ...PLANT, commonName: '🌿'.repeat(40) }
    expect([...toDeviceProfile(emoji, 2).name]).toHaveLength(DEVICE_LIMITS.nameMaxLength)
  })

  it('caps the pause at the device maximum', () => {
    const slow = { ...PLANT, profile: { ...PROFILE, minDays: 100 } }
    expect(toDeviceProfile(slow, 2).min_pause_h).toBe(DEVICE_LIMITS.pauseHours.max)
  })
})
