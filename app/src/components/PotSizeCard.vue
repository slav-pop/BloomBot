<script setup>
import PotSizeSlider from './PotSizeSlider.vue'

defineProps({
  modelValue: { type: Number, required: true },
})

defineEmits(['update:modelValue'])
</script>

<template>
  <div class="card">
    <div class="card-head">
      <span class="icon-badge">
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
          <path d="M6 8H18L16.6 19.2C16.5 20 15.8 20.6 15 20.6H9C8.2 20.6 7.5 20 7.4 19.2L6 8Z" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" />
          <path d="M4.5 8H19.5" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" />
          <path d="M9 8C9 8 8.7 4 12 4C15.3 4 15 8 15 8" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" />
        </svg>
      </span>
      <h3>Topfgröße</h3>
    </div>

    <PotSizeSlider :model-value="modelValue" @update:model-value="$emit('update:modelValue', $event)" />
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
  margin-bottom: 16px;
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
</style>
