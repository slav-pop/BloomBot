import { nextTick } from 'vue'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { plants } from '../lib/plants.js'

// Die Composables halten globalen Zustand auf Modulebene → pro Test frisch laden.
async function loadPotSize() {
  vi.resetModules()
  return import('./usePotSize.js')
}

async function loadSelectedPlant() {
  vi.resetModules()
  return import('./useSelectedPlant.js')
}

beforeEach(() => {
  localStorage.clear()
})

describe('usePotSize', () => {
  it('starts with the default pot size', async () => {
    const { usePotSize, POT_LIMITS } = await loadPotSize()
    expect(usePotSize().potSizeLiters.value).toBe(POT_LIMITS.default)
  })

  it('restores a valid stored value', async () => {
    localStorage.setItem('bloombot:potSizeLiters', '7.5')
    const { usePotSize } = await loadPotSize()
    expect(usePotSize().potSizeLiters.value).toBe(7.5)
  })

  it.each(['abc', '0', '-2', '999', 'Infinity'])('ignores the invalid stored value %s', async (stored) => {
    localStorage.setItem('bloombot:potSizeLiters', stored)
    const { usePotSize, POT_LIMITS } = await loadPotSize()
    expect(usePotSize().potSizeLiters.value).toBe(POT_LIMITS.default)
  })

  it('persists changes', async () => {
    const { usePotSize } = await loadPotSize()
    usePotSize().potSizeLiters.value = 4
    await nextTick()
    expect(localStorage.getItem('bloombot:potSizeLiters')).toBe('4')
  })

  it('keeps working when storage is unavailable', async () => {
    vi.spyOn(Storage.prototype, 'getItem').mockImplementation(() => {
      throw new Error('SecurityError')
    })
    vi.spyOn(Storage.prototype, 'setItem').mockImplementation(() => {
      throw new Error('QuotaExceededError')
    })
    const { usePotSize, POT_LIMITS } = await loadPotSize()
    const { potSizeLiters } = usePotSize()
    expect(potSizeLiters.value).toBe(POT_LIMITS.default)

    potSizeLiters.value = 5
    await nextTick()
    expect(potSizeLiters.value).toBe(5)
  })
})

describe('useSelectedPlant', () => {
  const [first] = plants

  it('has no plant at first', async () => {
    const { useSelectedPlant } = await loadSelectedPlant()
    expect(useSelectedPlant().selectedPlant.value).toBeNull()
  })

  it('restores a stored plant', async () => {
    localStorage.setItem('bloombot:selectedPlantId', String(first.id))
    const { useSelectedPlant } = await loadSelectedPlant()
    expect(useSelectedPlant().selectedPlant.value?.id).toBe(first.id)
  })

  it('drops a stored id that is no longer in the database', async () => {
    localStorage.setItem('bloombot:selectedPlantId', '123456789')
    const { useSelectedPlant } = await loadSelectedPlant()
    const { selectedId, selectedPlant } = useSelectedPlant()
    expect(selectedId.value).toBeNull()
    expect(selectedPlant.value).toBeNull()
  })

  it('selects and clears a plant and persists it', async () => {
    const { useSelectedPlant } = await loadSelectedPlant()
    const { selectPlant, clearPlant, selectedPlant } = useSelectedPlant()

    selectPlant(first.id)
    await nextTick()
    expect(selectedPlant.value?.id).toBe(first.id)
    expect(localStorage.getItem('bloombot:selectedPlantId')).toBe(String(first.id))

    clearPlant()
    await nextTick()
    expect(selectedPlant.value).toBeNull()
    expect(localStorage.getItem('bloombot:selectedPlantId')).toBeNull()
  })
})
