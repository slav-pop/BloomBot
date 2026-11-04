<script setup>
import { computed } from 'vue'
import { POT_LIMITS } from '../composables/usePotSize.js'

const props = defineProps({
  modelValue: { type: Number, required: true },
  min: { type: Number, default: POT_LIMITS.min },
  max: { type: Number, default: POT_LIMITS.max },
  stepSize: { type: Number, default: POT_LIMITS.step },
})

const emit = defineEmits(['update:modelValue'])

const percent = computed(() => ((props.modelValue - props.min) / (props.max - props.min)) * 100)

function set(value) {
  const clamped = Math.min(props.max, Math.max(props.min, value))
  emit('update:modelValue', Math.round(clamped * 10) / 10)
}

function bump(delta) {
  set(props.modelValue + delta)
}

function onInput(e) {
  set(Number(e.target.value))
}
</script>

<template>
  <div class="pot-slider">
    <div class="row">
      <button class="stepper" type="button" aria-label="Verkleinern" :disabled="modelValue <= min" @click="bump(-stepSize)">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none">
          <path d="M5 12H19" stroke="currentColor" stroke-width="2" stroke-linecap="round" />
        </svg>
      </button>

      <div class="value">
        <span class="num">{{ modelValue.toFixed(1) }}</span>
        <span class="unit">L</span>
      </div>

      <button class="stepper" type="button" aria-label="Vergrößern" :disabled="modelValue >= max" @click="bump(stepSize)">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none">
          <path d="M12 5V19M5 12H19" stroke="currentColor" stroke-width="2" stroke-linecap="round" />
        </svg>
      </button>
    </div>

    <div class="track-wrap">
      <div class="track">
        <div class="fill" :style="{ width: percent + '%' }" />
      </div>
      <input
        class="range"
        type="range"
        :min="min"
        :max="max"
        :step="stepSize"
        :value="modelValue"
        @input="onInput"
        aria-label="Topfgröße in Litern"
      />
    </div>

    <div class="scale-labels">
      <span>{{ min }} L</span>
      <span>{{ max }} L</span>
    </div>
  </div>
</template>

<style scoped>
.pot-slider {
  width: 100%;
}

.row {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 24px;
  margin-bottom: 18px;
}

.stepper {
  width: 36px;
  height: 36px;
  flex-shrink: 0;
  border-radius: 99px;
  background: var(--surface-sunken);
  color: var(--text);
  display: flex;
  align-items: center;
  justify-content: center;
  transition: transform 0.1s ease, opacity 0.15s ease;
}

.stepper:active {
  transform: scale(0.92);
}

.stepper:disabled {
  opacity: 0.35;
}

.value {
  display: flex;
  align-items: baseline;
  gap: 4px;
  min-width: 84px;
  justify-content: center;
}

.num {
  font-size: 34px;
  font-weight: 700;
  font-variant-numeric: tabular-nums;
  color: var(--text);
}

.unit {
  font-size: 16px;
  font-weight: 600;
  color: var(--text-muted);
}

.track-wrap {
  position: relative;
  height: 28px;
  display: flex;
  align-items: center;
}

.track {
  position: relative;
  width: 100%;
  height: 6px;
  border-radius: 99px;
  background: var(--surface-sunken);
  overflow: hidden;
}

.fill {
  position: absolute;
  inset: 0 auto 0 0;
  background: var(--sage);
  border-radius: 99px;
  transition: width 0.08s linear;
}

.range {
  position: absolute;
  inset: 0;
  width: 100%;
  margin: 0;
  appearance: none;
  -webkit-appearance: none;
  background: transparent;
  cursor: pointer;
}

.range::-webkit-slider-runnable-track {
  height: 6px;
  background: transparent;
}

.range::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 26px;
  height: 26px;
  border-radius: 50%;
  background: #fff;
  border: none;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.25), 0 0 0 1px var(--border);
  margin-top: -10px;
  transition: transform 0.1s ease;
}

.range::-webkit-slider-thumb:active {
  transform: scale(1.12);
}

.range::-moz-range-track {
  height: 6px;
  background: transparent;
}

.range::-moz-range-thumb {
  width: 26px;
  height: 26px;
  border: none;
  border-radius: 50%;
  background: #fff;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.25), 0 0 0 1px var(--border);
}

.scale-labels {
  display: flex;
  justify-content: space-between;
  margin-top: 6px;
  font-size: 12px;
  color: var(--text-muted);
}
</style>
