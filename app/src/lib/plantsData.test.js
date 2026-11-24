// Vertrag für data/plants.json: nur benötigte Felder, Werte in den Bereichen aus Integration.md §5.
import { existsSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'
import { describe, expect, it } from 'vitest'
import rawPlants from '@data/plants.json'
import { parseSunlight } from './plants.js'

const PLANT_KEYS = ['common_name', 'id', 'irrigation_profile', 'scientific_name', 'sunlight']
const PROFILE_KEYS = ['pump_calculation', 'soil_moisture', 'water_need_category', 'watering_cycle']
const MOISTURE_KEYS = ['maximum_safe_percent', 'minimum_before_watering_percent', 'recommended_target_percent']
const CYCLE_KEYS = ['maximum_days', 'minimum_days']
const PUMP_KEYS = ['base_ml_per_liter_of_soil', 'maximum_water_ml', 'minimum_water_ml']

// node:path statt `new URL()`: happy-dom ersetzt das globale URL.
const PLANT_IMAGES = resolve(dirname(fileURLToPath(import.meta.url)), '../../public/plants')
const imagePath = (id) => resolve(PLANT_IMAGES, `${id}.webp`)
const keys = (obj) => Object.keys(obj).sort()

describe('data/plants.json', () => {
  it('has unique numeric ids', () => {
    const ids = rawPlants.map((p) => p.id)
    expect(ids.every(Number.isInteger)).toBe(true)
    expect(new Set(ids).size).toBe(ids.length)
  })

  describe.each(rawPlants.map((p) => [`${p.id} ${p.common_name}`, p]))('%s', (_, plant) => {
    const { soil_moisture: m, watering_cycle: c, pump_calculation: pump } = plant.irrigation_profile

    it('contains only the fields in use', () => {
      expect(keys(plant)).toEqual(PLANT_KEYS)
      expect(keys(plant.irrigation_profile)).toEqual(PROFILE_KEYS)
      expect(keys(m)).toEqual(MOISTURE_KEYS)
      expect(keys(c)).toEqual(CYCLE_KEYS)
      expect(keys(pump)).toEqual(PUMP_KEYS)
    })

    it('has names, a known light value and a photo', () => {
      expect(plant.common_name.trim()).not.toBe('')
      expect(plant.scientific_name.trim()).not.toBe('')
      expect(parseSunlight(plant.sunlight)).not.toBeNull()
      expect(['low', 'medium', 'high']).toContain(plant.irrigation_profile.water_need_category)
      expect(existsSync(imagePath(plant.id))).toBe(true)
    })

    it('has moisture thresholds the device accepts (min < target < max ≤ 100)', () => {
      expect(m.minimum_before_watering_percent).toBeGreaterThanOrEqual(0)
      expect(m.recommended_target_percent).toBeGreaterThan(m.minimum_before_watering_percent)
      expect(m.maximum_safe_percent).toBeGreaterThan(m.recommended_target_percent)
      expect(m.maximum_safe_percent).toBeLessThanOrEqual(100)
    })

    it('has a consistent watering cycle and pump range', () => {
      expect(c.minimum_days).toBeGreaterThan(0)
      expect(c.maximum_days).toBeGreaterThanOrEqual(c.minimum_days)
      expect(pump.base_ml_per_liter_of_soil).toBeGreaterThan(0)
      expect(pump.minimum_water_ml).toBeGreaterThan(0)
      expect(pump.maximum_water_ml).toBeGreaterThanOrEqual(pump.minimum_water_ml)
    })
  })
})
