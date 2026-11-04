<script setup>
import { SUNLIGHT_LEVELS } from '../lib/plants.js'

defineProps({
  light: { type: Object, required: true },
})
</script>

<template>
  <div class="card">
    <div class="card-head">
      <span class="icon-badge" aria-hidden="true">
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
          <circle cx="12" cy="12" r="4.2" stroke="currentColor" stroke-width="1.6" />
          <path d="M12 3V5.2M12 18.8V21M21 12H18.8M5.2 12H3M18.4 5.6L16.8 7.2M7.2 16.8L5.6 18.4M18.4 18.4L16.8 16.8M7.2 7.2L5.6 5.6"
            stroke="currentColor" stroke-width="1.6" stroke-linecap="round" />
        </svg>
      </span>
      <h3>Licht</h3>
    </div>

    <div class="scale" aria-hidden="true">
      <div class="track">
        <div
          class="range"
          :style="{
            left: (light.lo / SUNLIGHT_LEVELS) * 100 + '%',
            width: ((light.hi - light.lo + 1) / SUNLIGHT_LEVELS) * 100 + '%',
          }"
        />
      </div>
    </div>

    <p class="label">{{ light.label }}</p>
    <p class="detail">nur zur Information &middot; wird nicht gesteuert</p>
  </div>
</template>

<style scoped>
.card {
  background: var(--surface);
  border-radius: var(--radius-lg);
  padding: 20px;
  box-shadow: var(--shadow-card);
}

.card-head {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 14px;
}

.icon-badge {
  display: flex;
  color: var(--clay);
}

h3 {
  font-size: 14px;
  font-weight: 600;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.scale {
  margin-bottom: 12px;
}

.track {
  position: relative;
  height: 8px;
  border-radius: 99px;
  background: linear-gradient(
    to right,
    #F3D48A 0%,
    #E7C27C 25%,
    #C8AE8E 50%,
    #9AA48F 75%,
    #667065 100%
  );
}

.range {
  position: absolute;
  top: -3px;
  bottom: -3px;
  border-radius: 99px;
  border: 2.5px solid var(--text);
  box-shadow: 0 0 0 3px var(--surface);
}

.label {
  font-size: 20px;
  font-weight: 600;
  color: var(--text);
}

.detail {
  margin-top: 2px;
  font-size: 14px;
  color: var(--text-muted);
}
</style>
