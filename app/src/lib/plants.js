import rawPlants from '@data/plants.json'

// Lichtskala von sonnig (0) bis schattig (4), Werte wie in data/plants.json → sunlight.
const SUNLIGHT_SCALE = ['full sun', 'part sun/part shade', 'part shade', 'filtered shade', 'full shade']
const SUNLIGHT_LABELS = ['Sonnig', 'Halbsonnig', 'Halbschatten', 'Lichter Schatten', 'Schatten']

export const SUNLIGHT_LEVELS = SUNLIGHT_SCALE.length

const WATER_NEED = {
  low: { level: 1, label: 'Selten' },
  medium: { level: 2, label: 'Normal' },
  high: { level: 3, label: 'Häufig' },
}

/**
 * "part shade, full sun" → { lo: 0, hi: 2, label: 'Sonnig – Halbschatten' }
 * @param {string | undefined} raw
 * @returns {{ lo: number, hi: number, label: string } | null}
 */
export function parseSunlight(raw) {
  const levels = String(raw ?? '')
    .split(',')
    .map((part) => SUNLIGHT_SCALE.indexOf(part.trim().toLowerCase()))
    .filter((level) => level >= 0)

  if (levels.length === 0) return null

  const lo = Math.min(...levels)
  const hi = Math.max(...levels)
  const label = lo === hi ? SUNLIGHT_LABELS[lo] : `${SUNLIGHT_LABELS[lo]} – ${SUNLIGHT_LABELS[hi]}`
  return { lo, hi, label }
}

/**
 * Datensatz aus data/plants.json → Modell für die Oberfläche.
 * @param {object} raw
 */
export function toPlant(raw) {
  const { water_need_category, soil_moisture, watering_cycle, pump_calculation } = raw.irrigation_profile
  const need = WATER_NEED[water_need_category] ?? WATER_NEED.medium

  return Object.freeze({
    id: raw.id,
    commonName: raw.common_name,
    scientificName: raw.scientific_name,
    image: `${import.meta.env.BASE_URL}plants/${raw.id}.webp`,
    light: parseSunlight(raw.sunlight),
    watering: Object.freeze({
      ...need,
      daysMin: watering_cycle.minimum_days,
      daysMax: watering_cycle.maximum_days,
    }),
    profile: Object.freeze({
      moistMin: soil_moisture.minimum_before_watering_percent,
      moistTarget: soil_moisture.recommended_target_percent,
      moistMax: soil_moisture.maximum_safe_percent,
      minDays: watering_cycle.minimum_days,
      maxDays: watering_cycle.maximum_days,
      baseMlPerLiter: pump_calculation.base_ml_per_liter_of_soil,
      minMl: pump_calculation.minimum_water_ml,
      maxMl: pump_calculation.maximum_water_ml,
    }),
  })
}

export const plants = Object.freeze(rawPlants.map(toPlant))

/** @param {number | string} id */
export function findPlant(id) {
  const wanted = Number(id)
  return plants.find((plant) => plant.id === wanted) ?? null
}

/** @param {string} query */
export function searchPlants(query) {
  const q = query.trim().toLowerCase()
  if (!q) return plants
  return plants.filter(
    (plant) => plant.commonName.toLowerCase().includes(q) || plant.scientificName.toLowerCase().includes(q),
  )
}
