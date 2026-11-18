// Pflanzenprofil für den ESP32, genau wie in Integration.md §5 beschrieben.
// Die App zeigt damit an, womit BloomBot für die gewählte Pflanze arbeitet.

/** Zulässige Bereiche laut Integration.md §5 – außerhalb lehnt das Gerät das Profil ab. */
export const DEVICE_LIMITS = Object.freeze({
  doseMl: Object.freeze({ min: 10, max: 500 }),
  pauseHours: Object.freeze({ min: 0, max: 720 }),
  nameMaxLength: 31,
})

/** min_pause_h = watering_cycle.minimum_days × 24 × 0,5 (Koeffizient laut §9 noch TBD). */
const PAUSE_FACTOR = 0.5
const HOURS_PER_DAY = 24

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value))
}

/**
 * dose_ml = base_ml_per_liter_of_soil × pot_liters, begrenzt auf die Werte der Pflanze
 * und auf den Bereich, den das Gerät annimmt.
 * @param {{ baseMlPerLiter: number, minMl: number, maxMl: number }} profile
 * @param {number} potLiters
 * @returns {{ ml: number, clamped: boolean }}
 */
export function computeDose(profile, potLiters) {
  const { min: deviceMin, max: deviceMax } = DEVICE_LIMITS.doseMl
  const raw = profile.baseMlPerLiter * potLiters
  // Beide Grenzen bleiben im Gerätebereich, auch wenn der Pflanzenbereich daneben liegt.
  const min = clamp(profile.minMl, deviceMin, deviceMax)
  const max = clamp(profile.maxMl, min, deviceMax)
  const ml = Math.round(clamp(raw, min, max))
  return { ml, clamped: raw < min || raw > max }
}

/**
 * @param {{ id: number, commonName: string, profile: object }} plant
 * @param {number} potLiters
 */
export function toDeviceProfile(plant, potLiters) {
  const { profile } = plant
  const pauseHours = Math.round(profile.minDays * HOURS_PER_DAY * PAUSE_FACTOR)

  return {
    plant_id: plant.id,
    // Nach Zeichen kürzen, nicht nach UTF-16-Einheiten (sonst zerfallen Emojis/Umlaut-Kombinationen).
    name: [...plant.commonName].slice(0, DEVICE_LIMITS.nameMaxLength).join(''),
    mode: 'normal',
    moist_min: profile.moistMin,
    moist_target: profile.moistTarget,
    moist_max: profile.moistMax,
    dose_ml: computeDose(profile, potLiters).ml,
    min_pause_h: clamp(pauseHours, DEVICE_LIMITS.pauseHours.min, DEVICE_LIMITS.pauseHours.max),
  }
}
