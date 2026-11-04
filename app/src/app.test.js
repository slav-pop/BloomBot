// Komponententests über den echten Router mit derselben Hash-History wie in Produktion,
// damit auch die Zurück-Navigation (history.state.back) mitgetestet wird.
import { flushPromises, mount } from '@vue/test-utils'
import { beforeEach, describe, expect, it, vi } from 'vitest'

async function mountApp(path = '/') {
  vi.resetModules()
  const { createAppRouter } = await import('./router.js')
  const { default: App } = await import('./App.vue')
  const { plants } = await import('./lib/plants.js')

  const router = createAppRouter()
  router.push(path)
  await router.isReady()

  const wrapper = mount(App, { global: { plugins: [router] }, attachTo: document.body })
  await flushPromises()
  return { wrapper, router, plants }
}

beforeEach(() => {
  localStorage.clear()
  document.body.innerHTML = ''
  window.history.replaceState(null, '', '/')
})

describe('Home', () => {
  it('asks to pick a plant when none is selected', async () => {
    const { wrapper } = await mountApp('/')
    expect(wrapper.text()).toContain('Noch keine Pflanze')
    expect(wrapper.find('a.cta').attributes('href')).toBe('#/catalog')
  })

  it('shows the device profile for the selected plant', async () => {
    localStorage.setItem('bloombot:selectedPlantId', '711')
    localStorage.setItem('bloombot:potSizeLiters', '2')
    const { wrapper } = await mountApp('/')

    expect(wrapper.find('h1').text()).toBe("Amazonian elephant's ear")
    expect(wrapper.find('[data-test="dose"]').text()).toBe('320ml')
    expect(wrapper.text()).toContain('Gießen ab45 %')
    expect(wrapper.text()).toContain('Mindestens 36 h Pause')
    expect(wrapper.find('[data-test="clamped"]').exists()).toBe(false)
  })

  it('updates the dose when the pot size changes and flags the device limit', async () => {
    localStorage.setItem('bloombot:selectedPlantId', '711')
    localStorage.setItem('bloombot:potSizeLiters', '2')
    const { wrapper } = await mountApp('/')

    await wrapper.find('input[type="range"]').setValue('10')
    expect(wrapper.find('[data-test="dose"]').text()).toBe('500ml')
    expect(wrapper.find('[data-test="clamped"]').exists()).toBe(true)

    await wrapper.find('button[aria-label="Verkleinern"]').trigger('click')
    expect(wrapper.find('.num').text()).toBe('9.5')
  })
})

describe('Catalog', () => {
  it('lists all plants and filters by search', async () => {
    const { wrapper, plants } = await mountApp('/catalog')
    expect(wrapper.findAll('.grid > a')).toHaveLength(plants.length)

    await wrapper.find('input[type="search"]').setValue('ficus')
    const names = wrapper.findAll('.grid .sci').map((n) => n.text().toLowerCase())
    expect(names.length).toBeGreaterThan(0)
    expect(names.every((n) => n.includes('ficus'))).toBe(true)

    await wrapper.find('input[type="search"]').setValue('zzz-no-plant')
    expect(wrapper.text()).toContain('Keine Pflanze gefunden')
  })
})

describe('Plant detail', () => {
  it('selects the plant and returns home', async () => {
    const { wrapper, router } = await mountApp('/catalog/711')
    expect(wrapper.text()).toContain('Halbschatten')

    await wrapper.find('button.cta').trigger('click')
    await flushPromises()

    expect(router.currentRoute.value.name).toBe('home')
    expect(localStorage.getItem('bloombot:selectedPlantId')).toBe('711')
  })

  it('shows a not-found state for an unknown id', async () => {
    const { wrapper } = await mountApp('/catalog/999999')
    expect(wrapper.text()).toContain('Pflanze nicht gefunden')
  })

  it('goes back to the catalog when opened directly', async () => {
    const { wrapper, router } = await mountApp('/catalog/711')
    await wrapper.find('button[aria-label="Zurück"]').trigger('click')
    await flushPromises()
    expect(router.currentRoute.value.name).toBe('catalog')
  })

  it('goes back in history when navigated from the catalog', async () => {
    const { wrapper, router } = await mountApp('/catalog')
    await router.push('/catalog/711')
    await flushPromises()

    const back = vi.spyOn(router, 'back')
    await wrapper.find('button[aria-label="Zurück"]').trigger('click')
    expect(back).toHaveBeenCalled()
  })

  it('shows a placeholder when the photo cannot be loaded', async () => {
    const { wrapper } = await mountApp('/catalog/711')
    await wrapper.find('.photo img').trigger('error')
    expect(wrapper.find('.photo .fallback').exists()).toBe(true)
  })
})

describe('Routing', () => {
  it('redirects unknown paths to home', async () => {
    const { router } = await mountApp('/does-not-exist')
    expect(router.currentRoute.value.name).toBe('home')
  })
})
