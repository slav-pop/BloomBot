import { computed, ref, watch } from 'vue'
import { findPlant } from '../lib/plants.js'
import { readStorage, writeStorage } from '../lib/storage.js'

const STORAGE_KEY = 'bloombot:selectedPlantId'

// Eine gespeicherte ID, die es in der Datenbank nicht mehr gibt, wird verworfen.
const selectedId = ref(findPlant(readStorage(STORAGE_KEY) ?? NaN)?.id ?? null)

watch(selectedId, (id) => writeStorage(STORAGE_KEY, id === null ? null : String(id)))

const selectedPlant = computed(() => (selectedId.value === null ? null : findPlant(selectedId.value)))

function selectPlant(id) {
  selectedId.value = findPlant(id)?.id ?? null
}

function clearPlant() {
  selectedId.value = null
}

export function useSelectedPlant() {
  return { selectedId, selectedPlant, selectPlant, clearPlant }
}
