<script setup>
import { computed } from 'vue'
import { computeDose, toDeviceProfile } from '../lib/deviceProfile.js'

const props = defineProps({
  plant: { type: Object, required: true },
  potSizeLiters: { type: Number, required: true },
})

const device = computed(() => toDeviceProfile(props.plant, props.potSizeLiters))
const isClamped = computed(() => computeDose(props.plant.profile, props.potSizeLiters).clamped)

const percent = (value) => Math.min(100, Math.max(0, value))
const band = computed(() => {
  const left = percent(device.value.moist_min)
  return { left: `${left}%`, width: `${Math.max(0, percent(device.value.moist_max) - left)}%` }
})
const tick = computed(() => ({ left: `${percent(device.value.moist_target)}%` }))

const zones = computed(() => [
  { key: 'min', label: 'Gießen ab', value: device.value.moist_min },
  { key: 'target', label: 'Ziel', value: device.value.moist_target },
  { key: 'max', label: 'Nie über', value: device.value.moist_max },
])
</script>

<template>
  <section class="card" aria-labelledby="dose-title">
    <div class="card-head">
      <div class="card-head-left">
        <span class="icon-badge" aria-hidden="true">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
            <path d="M12 2.5C12 2.5 5.5 8 4.2 13C3 17.6 6.4 21.5 12 21.5C17.6 21.5 21 17.6 19.8 13C18.5 8 12 2.5 12 2.5Z"
              stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" />
          </svg>
        </span>
        <h3 id="dose-title">Profil für BloomBot</h3>
      </div>
      <span class="pill">automatisch</span>
    </div>

    <div class="dose-row">
      <p class="dose" data-test="dose">{{ device.dose_ml }}<span class="unit">ml</span></p>
      <p class="detail">pro Portion, bis das Ziel erreicht ist</p>
    </div>

    <div
      class="moisture"
      role="img"
      :aria-label="`Bodenfeuchte: gießen unter ${device.moist_min} %, Ziel ${device.moist_target} %, nie über ${device.moist_max} %`"
    >
      <div class="bar">
        <span class="band" :style="band" />
        <span class="tick" :style="tick" />
      </div>
      <dl class="zones">
        <div v-for="zone in zones" :key="zone.key" :class="['zone', `zone-${zone.key}`]">
          <dt>{{ zone.label }}</dt>
          <dd>{{ zone.value }} %</dd>
        </div>
      </dl>
    </div>

    <p class="detail pause">Mindestens {{ device.min_pause_h }} h Pause zwischen zwei Gießzyklen.</p>
    <p v-if="isClamped" class="note" data-test="clamped">
      Menge auf den sicheren Bereich für diese Pflanze und die Pumpe begrenzt.
    </p>
  </section>
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
  justify-content: space-between;
  margin-bottom: 16px;
  gap: 8px;
}

.card-head-left {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
}

.icon-badge {
  display: flex;
  flex-shrink: 0;
  color: var(--sage);
}

h3 {
  font-size: 14px;
  font-weight: 600;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.pill {
  font-size: 11px;
  font-weight: 600;
  color: var(--sage);
  background: var(--sage-dim);
  padding: 4px 10px;
  border-radius: 99px;
  white-space: nowrap;
  flex-shrink: 0;
}

.dose-row {
  display: flex;
  align-items: baseline;
  flex-wrap: wrap;
  gap: 6px 12px;
}

.dose {
  font-size: 36px;
  font-weight: 700;
  font-variant-numeric: tabular-nums;
  color: var(--text);
}

.unit {
  font-size: 16px;
  font-weight: 600;
  color: var(--text-muted);
  margin-left: 4px;
}

.detail {
  font-size: 14px;
  color: var(--text-muted);
}

.moisture {
  margin-top: 18px;
}

.bar {
  position: relative;
  height: 10px;
  border-radius: 99px;
  background: linear-gradient(to right, var(--clay-dim), var(--sky-dim));
}

.band {
  position: absolute;
  top: 0;
  bottom: 0;
  border-radius: 99px;
  background: color-mix(in srgb, var(--sage) 55%, transparent);
}

.tick {
  position: absolute;
  top: -4px;
  bottom: -4px;
  width: 3px;
  margin-left: -1.5px;
  border-radius: 2px;
  background: var(--text);
}

.zones {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 8px;
  margin: 12px 0 0;
}

.zone {
  margin: 0;
}

.zone dt {
  font-size: 12px;
  color: var(--text-muted);
}

.zone dd {
  margin: 2px 0 0;
  font-size: 17px;
  font-weight: 650;
  font-variant-numeric: tabular-nums;
}

.zone-target dd {
  color: var(--sage);
}

.zone-max {
  text-align: right;
}

.pause {
  margin-top: 14px;
}

.note {
  margin-top: 10px;
  font-size: 12px;
  color: var(--text-muted);
}
</style>
