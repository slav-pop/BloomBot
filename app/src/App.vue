<script setup>
import BottomNav from './components/BottomNav.vue'
</script>

<template>
  <main class="page">
    <router-view />
  </main>
  <BottomNav />
</template>

<style scoped>
.page {
  flex: 1;
  padding-bottom: 96px;
}
</style>
