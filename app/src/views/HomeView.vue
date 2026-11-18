<script setup>
import { useSelectedPlant } from '../composables/useSelectedPlant.js'
import { usePotSize } from '../composables/usePotSize.js'
import PlantPhoto from '../components/PlantPhoto.vue'
import WateringCard from '../components/WateringCard.vue'
import LightCard from '../components/LightCard.vue'
import PotSizeCard from '../components/PotSizeCard.vue'
import DoseCard from '../components/DoseCard.vue'

const { selectedPlant } = useSelectedPlant()
const { potSizeLiters } = usePotSize()
</script>

<template>
  <div v-if="selectedPlant" class="view">
    <header class="header">
      <p class="eyebrow">BloomBot</p>
      <h1>{{ selectedPlant.commonName }}</h1>
      <p class="sci">{{ selectedPlant.scientificName }}</p>
    </header>

    <div class="photo-card">
      <PlantPhoto :src="selectedPlant.image" :alt="selectedPlant.commonName" />
    </div>

    <div class="cards">
      <PotSizeCard v-model="potSizeLiters" />
      <DoseCard :plant="selectedPlant" :pot-size-liters="potSizeLiters" />
      <WateringCard :watering="selectedPlant.watering" />
      <LightCard v-if="selectedPlant.light" :light="selectedPlant.light" />
    </div>

    <RouterLink to="/catalog" class="switch">Andere Pflanze wählen</RouterLink>
  </div>

  <div v-else class="empty">
    <svg width="64" height="64" viewBox="0 0 24 24" fill="none" class="empty-icon" aria-hidden="true">
      <path d="M12 21C12 21 5 15.5 5 10.2C5 6.5 8.2 4 11.2 4.6C11.6 4.7 12 5.1 12 5.5C12 5.1 12.4 4.7 12.8 4.6C15.8 4 19 6.5 19 10.2C19 15.5 12 21 12 21Z"
        stroke="currentColor" stroke-width="1.4" stroke-linejoin="round" />
    </svg>
    <h1>Noch keine Pflanze</h1>
    <p>Wähle deine Pflanze aus dem Katalog, um ihre Gieß- und Lichtdaten zu sehen.</p>
    <RouterLink to="/catalog" class="cta">Pflanze auswählen</RouterLink>
  </div>
</template>

<style scoped>
.view {
  padding: 24px 20px 12px;
}

.header {
  margin-bottom: 20px;
}

.eyebrow {
  font-size: 13px;
  font-weight: 600;
  color: var(--sage);
  text-transform: uppercase;
  letter-spacing: 0.06em;
}

.header h1 {
  margin-top: 6px;
  font-size: 28px;
  font-weight: 700;
  text-transform: capitalize;
}

.sci {
  margin-top: 4px;
  font-size: 15px;
  font-style: italic;
  color: var(--text-muted);
}

.photo-card {
  aspect-ratio: 4 / 3;
  border-radius: var(--radius-lg);
  overflow: hidden;
  box-shadow: var(--shadow-card);
  margin-bottom: 20px;
}

.cards {
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.switch {
  display: block;
  text-align: center;
  margin-top: 22px;
  padding: 14px;
  font-size: 14px;
  font-weight: 600;
  color: var(--text-muted);
}

.empty {
  min-height: 70vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 40px;
  gap: 8px;
}

.empty-icon {
  color: var(--sage);
  opacity: 0.6;
  margin-bottom: 8px;
}

.empty h1 {
  font-size: 22px;
}

.empty p {
  color: var(--text-muted);
  font-size: 15px;
  max-width: 32ch;
}

.cta {
  margin-top: 16px;
  padding: 14px 28px;
  border-radius: var(--radius-md);
  background: var(--sage);
  color: #fff;
  font-size: 16px;
  font-weight: 600;
  box-shadow: var(--shadow-pop);
}
</style>
