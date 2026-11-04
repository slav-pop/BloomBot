<script setup>
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import { findPlant } from '../lib/plants.js'
import { useSelectedPlant } from '../composables/useSelectedPlant.js'
import PlantPhoto from '../components/PlantPhoto.vue'
import WateringCard from '../components/WateringCard.vue'
import LightCard from '../components/LightCard.vue'

const props = defineProps({
  id: { type: String, required: true },
})

const router = useRouter()
const { selectedId, selectPlant } = useSelectedPlant()

const plant = computed(() => findPlant(props.id))
const isMine = computed(() => selectedId.value === plant.value?.id)

function choosePlant() {
  selectPlant(plant.value.id)
  router.push('/')
}

// Direkt geöffneter Link hat keinen Verlauf in der App → zurück zum Katalog.
function goBack() {
  if (router.options.history.state?.back) {
    router.back()
  } else {
    router.push('/catalog')
  }
}
</script>

<template>
  <div v-if="!plant" class="missing" role="status">
    <h1>Pflanze nicht gefunden</h1>
    <RouterLink to="/catalog" class="cta">Zum Katalog</RouterLink>
  </div>

  <div v-else class="view">
    <button type="button" class="back" aria-label="Zurück" @click="goBack">
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none" aria-hidden="true">
        <path d="M15 5L8 12L15 19" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" />
      </svg>
    </button>

    <div class="photo">
      <PlantPhoto :src="plant.image" :alt="plant.commonName" />
    </div>

    <div class="content">
      <h1>{{ plant.commonName }}</h1>
      <p class="sci">{{ plant.scientificName }}</p>

      <div class="cards">
        <WateringCard :watering="plant.watering" />
        <LightCard v-if="plant.light" :light="plant.light" />
      </div>

      <button type="button" class="cta" :class="{ done: isMine }" @click="choosePlant">
        {{ isMine ? 'Das ist deine Pflanze ✓' : 'Als meine Pflanze festlegen' }}
      </button>
    </div>
  </div>
</template>

<style scoped>
.view {
  position: relative;
}

.missing {
  min-height: 70vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 18px;
  padding: 40px;
  text-align: center;
}

.missing h1 {
  font-size: 22px;
}

.missing .cta {
  width: auto;
  margin-top: 0;
  padding: 14px 28px;
}

.back {
  position: absolute;
  top: 20px;
  left: 16px;
  z-index: 2;
  width: 40px;
  height: 40px;
  border-radius: 99px;
  background: color-mix(in srgb, var(--surface) 80%, transparent);
  backdrop-filter: blur(8px);
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--text);
}

.photo {
  aspect-ratio: 4 / 3;
}

.content {
  padding: 20px;
}

h1 {
  font-size: 26px;
  font-weight: 700;
  text-transform: capitalize;
}

.sci {
  margin-top: 4px;
  font-size: 15px;
  font-style: italic;
  color: var(--text-muted);
}

.cards {
  margin-top: 20px;
  display: flex;
  flex-direction: column;
  gap: 14px;
}

.cta {
  width: 100%;
  margin-top: 24px;
  padding: 16px;
  border-radius: var(--radius-md);
  background: var(--sage);
  color: #fff;
  font-size: 16px;
  font-weight: 600;
  box-shadow: var(--shadow-pop);
}

.cta.done {
  background: var(--sage-dim);
  color: var(--sage);
  box-shadow: none;
}
</style>
