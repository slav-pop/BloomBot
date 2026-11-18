// localStorage kann fehlen oder werfen (privater Modus, gesperrte Website-Daten).
// Dann gelten die Einstellungen nur für die aktuelle Sitzung – die App läuft trotzdem.

/** @param {string} key */
export function readStorage(key) {
  try {
    return globalThis.localStorage?.getItem(key) ?? null
  } catch {
    return null
  }
}

/**
 * @param {string} key
 * @param {string | null} value  null entfernt den Eintrag
 */
export function writeStorage(key, value) {
  try {
    if (value === null) {
      globalThis.localStorage?.removeItem(key)
    } else {
      globalThis.localStorage?.setItem(key, value)
    }
  } catch {
    // Speichern nicht möglich: Wert bleibt nur im Arbeitsspeicher (siehe Kommentar oben).
  }
}
