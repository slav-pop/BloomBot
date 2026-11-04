<script setup>
import { computed, ref } from 'vue'
import { plants, searchPlants } from '../lib/plants.js'
import PlantCard from '../components/PlantCard.vue'

const query = ref('')
const filtered = computed(() => searchPlants(query.value))
</script>

<template>
  <div class="view">
    <header class="header">
      <h1>Pflanzenkatalog</h1>
      <p class="subtitle">{{ plants.length }} Pflanzen &middot; wähle deine aus</p>
    </header>

    <div class="search">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" aria-hidden="true">
        <circle cx="11" cy="11" r="6.5" stroke="currentColor" stroke-width="1.6" />
        <path d="M20 20L16 16" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" />
      </svg>
      <input v-model="query" type="search" placeholder="Pflanze suchen…" aria-label="Pflanze suchen" />
    </div>

    <p v-if="!filtered.length" class="empty" role="status">Keine Pflanze gefunden.</p>
    <div v-else class="grid">
      <PlantCard v-for="plant in filtered" :key="plant.id" :plant="plant" />
    </div>
  </div>
</template>

<style scoped>
.view {
  padding: 24px 20px 12px;
}

.header {
  margin-bottom: 20px;
}

h1 {
  font-size: 26px;
  font-weight: 700;
}

.subtitle {
  margin-top: 4px;
  font-size: 14px;
  color: var(--text-muted);
}

.search {
  display: flex;
  align-items: center;
  gap: 10px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-md);
  padding: 12px 16px;
  color: var(--text-muted);
  margin-bottom: 20px;
}

.search:focus-within {
  border-color: var(--sage);
  outline: 2px solid var(--sage);
  outline-offset: 2px;
}

.search input {
  flex: 1;
  border: none;
  outline: none;
  background: none;
  font-size: 15px;
  color: var(--text);
}

.grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 14px;
}

.empty {
  text-align: center;
  color: var(--text-muted);
  padding: 40px 0;
}
</style>
