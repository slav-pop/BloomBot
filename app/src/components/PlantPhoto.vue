<script setup>
import { ref, watch } from 'vue'

const props = defineProps({
  src: { type: String, default: null },
  alt: { type: String, default: '' },
})

const failed = ref(false)
watch(() => props.src, () => { failed.value = false })
</script>

<template>
  <div class="photo">
    <img v-if="src && !failed" :src="src" :alt="alt" loading="lazy" @error="failed = true" />
    <div v-else class="fallback">
      <svg width="40%" height="40%" viewBox="0 0 24 24" fill="none">
        <path d="M12 21C12 21 5 15.5 5 10.2C5 6.5 8.2 4 11.2 4.6C11.6 4.7 12 5.1 12 5.5C12 5.1 12.4 4.7 12.8 4.6C15.8 4 19 6.5 19 10.2C19 15.5 12 21 12 21Z"
          stroke="currentColor" stroke-width="1.3" stroke-linejoin="round" />
        <path d="M12 17.5V9.5" stroke="currentColor" stroke-width="1.3" stroke-linecap="round" />
      </svg>
    </div>
  </div>
</template>

<style scoped>
.photo {
  width: 100%;
  height: 100%;
  overflow: hidden;
  background: var(--surface-sunken);
}

.photo img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.fallback {
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--sage);
  opacity: 0.5;
}
</style>
