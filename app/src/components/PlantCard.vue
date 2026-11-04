<script setup>
import PlantPhoto from './PlantPhoto.vue'

defineProps({
  plant: { type: Object, required: true },
})
</script>

<template>
  <RouterLink :to="`/catalog/${plant.id}`" class="card">
    <div class="thumb">
      <PlantPhoto :src="plant.image" :alt="plant.commonName" />
    </div>
    <p class="name">{{ plant.commonName }}</p>
    <p class="sci">{{ plant.scientificName }}</p>
  </RouterLink>
</template>

<style scoped>
.card {
  display: block;
  background: var(--surface);
  border-radius: var(--radius-md);
  overflow: hidden;
  box-shadow: var(--shadow-card);
}

.thumb {
  aspect-ratio: 1 / 1;
}

.name {
  padding: 10px 12px 0;
  font-size: 14px;
  font-weight: 600;
  color: var(--text);
  text-transform: capitalize;
  line-height: 1.25;
}

.sci {
  padding: 2px 12px 12px;
  font-size: 12px;
  font-style: italic;
  color: var(--text-muted);
}
</style>
