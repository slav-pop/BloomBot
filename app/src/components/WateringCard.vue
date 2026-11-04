<script setup>
defineProps({
  watering: { type: Object, required: true },
})
</script>

<template>
  <div class="card">
    <div class="card-head">
      <span class="icon-badge">
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
          <path d="M12 3C12 3 6 10.5 6 15A6 6 0 0 0 18 15C18 10.5 12 3 12 3Z"
            stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" />
        </svg>
      </span>
      <h3>Gießen</h3>
    </div>

    <div class="drops">
      <svg v-for="n in 3" :key="n" width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path d="M12 3C12 3 6 10.5 6 15A6 6 0 0 0 18 15C18 10.5 12 3 12 3Z"
          :fill="n <= watering.level ? 'var(--sage)' : 'none'"
          stroke="var(--sage)" stroke-width="1.6" stroke-linejoin="round" />
      </svg>
    </div>

    <p class="label">{{ watering.label }}</p>
    <p class="detail">alle {{ watering.daysMin }}–{{ watering.daysMax }} Tage</p>
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
  color: var(--sage);
}

h3 {
  font-size: 14px;
  font-weight: 600;
  color: var(--text-muted);
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.drops {
  display: flex;
  gap: 6px;
  margin-bottom: 12px;
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
