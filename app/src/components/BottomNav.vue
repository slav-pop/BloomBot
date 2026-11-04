<script setup>
import { RouterLink, useRoute } from 'vue-router'

const route = useRoute()
</script>

<template>
  <nav class="nav">
    <RouterLink to="/" class="nav-item" :class="{ active: route.name === 'home' }">
      <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
        <path d="M12 21C12 21 5 15.5 5 10.2C5 6.5 8.2 4 11.2 4.6C11.6 4.7 12 5.1 12 5.5C12 5.1 12.4 4.7 12.8 4.6C15.8 4 19 6.5 19 10.2C19 15.5 12 21 12 21Z"
          stroke="currentColor" stroke-width="1.6" stroke-linejoin="round" />
      </svg>
      <span>Meine Pflanze</span>
    </RouterLink>
    <RouterLink to="/catalog" class="nav-item" :class="{ active: route.name === 'catalog' || route.name === 'plant-detail' }">
      <svg width="22" height="22" viewBox="0 0 24 24" fill="none">
        <rect x="4" y="4" width="7" height="7" rx="2" stroke="currentColor" stroke-width="1.6" />
        <rect x="13" y="4" width="7" height="7" rx="2" stroke="currentColor" stroke-width="1.6" />
        <rect x="4" y="13" width="7" height="7" rx="2" stroke="currentColor" stroke-width="1.6" />
        <rect x="13" y="13" width="7" height="7" rx="2" stroke="currentColor" stroke-width="1.6" />
      </svg>
      <span>Pflanzen</span>
    </RouterLink>
  </nav>
</template>

<style scoped>
.nav {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  max-width: 560px;
  margin: 0 auto;
  display: flex;
  gap: 4px;
  padding: 10px 16px calc(14px + env(safe-area-inset-bottom));
  background: color-mix(in srgb, var(--surface) 92%, transparent);
  backdrop-filter: blur(14px);
  border-top: 1px solid var(--border);
}

.nav-item {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  padding: 8px 4px 6px;
  border-radius: var(--radius-md);
  color: var(--text-muted);
  font-size: 12px;
  font-weight: 500;
  transition: color 0.15s ease, background 0.15s ease;
}

.nav-item.active {
  color: var(--sage);
  background: var(--sage-dim);
}
</style>
