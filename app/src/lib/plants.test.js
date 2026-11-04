import { describe, expect, it } from 'vitest'
import { findPlant, parseSunlight, plants, searchPlants, toPlant } from './plants.js'

const RAW = {
  id: 711,
  common_name: "Amazonian elephant's ear",
  scientific_name: "Alocasia amazonica 'Polly'",
  sunlight: 'part shade, full shade',
  irrigation_profile: {
    water_need_category: 'high',
    soil_moisture: {
      minimum_before_watering_percent: 45,
      recommended_target_percent: 65,
      maximum_safe_percent: 85,
    },
    watering_cycle: { minimum_days: 3, maximum_days: 5 },
    pump_calculation: { base_ml_per_liter_of_soil: 160, minimum_water_ml: 100, maximum_water_ml: 800 },
  },
}

describe('parseSunlight', () => {
  it('maps a single value to one level', () => {
    expect(parseSunlight('part shade')).toEqual({ lo: 2, hi: 2, label: 'Halbschatten' })
  })

  it('spans from the sunniest to the shadiest value regardless of order', () => {
    expect(parseSunlight('part shade, full sun, part sun/part shade')).toEqual({
      lo: 0,
      hi: 2,
      label: 'Sonnig – Halbschatten',
    })
  })

  it('ignores unknown values and casing', () => {
    expect(parseSunlight('Filtered Shade, moonlight')).toEqual({ lo: 3, hi: 3, label: 'Lichter Schatten' })
  })

  it('returns null when nothing is recognised', () => {
    expect(parseSunlight('')).toBeNull()
    expect(parseSunlight(undefined)).toBeNull()
  })
})

describe('toPlant', () => {
  it('maps the database record to the view model', () => {
    const plant = toPlant(RAW)

    expect(plant).toMatchObject({
      id: 711,
      commonName: "Amazonian elephant's ear",
      scientificName: "Alocasia amazonica 'Polly'",
      light: { lo: 2, hi: 4, label: 'Halbschatten – Schatten' },
      watering: { level: 3, label: 'Häufig', daysMin: 3, daysMax: 5 },
      profile: {
        moistMin: 45,
        moistTarget: 65,
        moistMax: 85,
        minDays: 3,
        maxDays: 5,
        baseMlPerLiter: 160,
        minMl: 100,
        maxMl: 800,
      },
    })
    expect(plant.image).toMatch(/plants\/711\.webp$/)
  })

  it('falls back to the medium water need for an unknown category', () => {
    const raw = { ...RAW, irrigation_profile: { ...RAW.irrigation_profile, water_need_category: 'x' } }
    expect(toPlant(raw).watering).toMatchObject({ level: 2, label: 'Normal' })
  })

  it('returns a frozen object', () => {
    expect(Object.isFrozen(toPlant(RAW))).toBe(true)
  })
})

describe('catalog', () => {
  it('loads every plant from data/plants.json', () => {
    expect(plants.length).toBeGreaterThan(0)
    expect(Object.isFrozen(plants)).toBe(true)
  })

  it('finds a plant by numeric or string id', () => {
    const [first] = plants
    expect(findPlant(first.id)).toBe(first)
    expect(findPlant(String(first.id))).toBe(first)
    expect(findPlant(-1)).toBeNull()
  })

  it('searches common and scientific names case-insensitively', () => {
    expect(searchPlants('').length).toBe(plants.length)
    expect(searchPlants('  ALOCASIA ').every((p) => /alocasia/i.test(p.scientificName))).toBe(true)
    expect(searchPlants('rubber').length).toBeGreaterThan(0)
    expect(searchPlants('zzz-no-plant')).toEqual([])
  })
})
