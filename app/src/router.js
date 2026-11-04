import { createRouter, createWebHashHistory } from 'vue-router'
import HomeView from './views/HomeView.vue'

// Hash-Routing: funktioniert auf statischem Hosting (GitHub Pages) ohne Server-Rewrites.
export const routes = [
  { path: '/', name: 'home', component: HomeView },
  { path: '/catalog', name: 'catalog', component: () => import('./views/CatalogView.vue') },
  {
    path: '/catalog/:id',
    name: 'plant-detail',
    component: () => import('./views/PlantDetailView.vue'),
    props: true,
  },
  { path: '/:pathMatch(.*)*', redirect: '/' },
]

export function createAppRouter(history = createWebHashHistory()) {
  return createRouter({
    history,
    routes,
    scrollBehavior: () => ({ top: 0 }),
  })
}
