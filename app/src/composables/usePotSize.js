import { ref, watch } from 'vue'
import { readStorage, writeStorage } from '../lib/storage.js'

const STORAGE_KEY = 'bloombot:potSizeLiters'

export const POT_LIMITS = Object.freeze({ min: 0.5, max: 20, step: 0.5, default: 3 })

function parsePotSize(stored) {
  const liters = Number(stored)
  const isValid = Number.isFinite(liters) && liters >= POT_LIMITS.min && liters <= POT_LIMITS.max
  return isValid ? liters : POT_LIMITS.default
}

const potSizeLiters = ref(parsePotSize(readStorage(STORAGE_KEY)))
watch(potSizeLiters, (liters) => writeStorage(STORAGE_KEY, String(liters)))

/** Topfvolumen in Litern – daraus berechnet sich dose_ml (Integration.md §5). */
export function usePotSize() {
  return { potSizeLiters }
}
