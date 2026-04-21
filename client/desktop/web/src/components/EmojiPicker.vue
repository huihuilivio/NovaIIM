<script setup lang="ts">
import { ref } from 'vue'

const emit = defineEmits<{ select: [emoji: string] }>()

const categories = [
  {
    name: '常用',
    emojis: ['😀', '😂', '😊', '😍', '🥰', '😘', '😎', '🤔', '😅', '😢', '😭', '😤', '😱', '🤗', '👍', '👎', '👌', '✌️', '🙏', '💪', '❤️', '💔', '🎉', '🔥', '⭐', '🌹', '🍻', '☕'],
  },
  {
    name: '表情',
    emojis: ['😃', '😄', '😁', '😆', '😉', '😋', '😜', '🤪', '😝', '🤑', '🤗', '🤭', '🤫', '🤐', '😐', '😑', '😶', '😏', '😒', '🙄', '😬', '🤥', '😌', '😔', '😪', '🤤', '😴', '😷'],
  },
  {
    name: '手势',
    emojis: ['👋', '🤚', '🖐️', '✋', '🖖', '👌', '🤌', '🤏', '✌️', '🤞', '🤟', '🤘', '🤙', '👈', '👉', '👆', '👇', '☝️', '👍', '👎', '✊', '👊', '🤛', '🤜', '👏', '🙌', '👐', '🤲'],
  },
  {
    name: '物品',
    emojis: ['💖', '💝', '💘', '💗', '💓', '💞', '💕', '❣️', '🎁', '🎈', '🎊', '🎂', '🍰', '🎵', '🎶', '📱', '💻', '⌚', '📷', '🔑', '💡', '📌', '📎', '✏️', '📝', '📚', '💰', '🏆'],
  },
]

const activeCategory = ref(0)
</script>

<template>
  <div class="emoji-picker" @click.stop>
    <div class="emoji-tabs">
      <button
        v-for="(cat, idx) in categories"
        :key="idx"
        :class="{ active: activeCategory === idx }"
        @click="activeCategory = idx"
      >{{ cat.name }}</button>
    </div>
    <div class="emoji-grid">
      <span
        v-for="emoji in categories[activeCategory].emojis"
        :key="emoji"
        class="emoji-item"
        @click="emit('select', emoji)"
      >{{ emoji }}</span>
    </div>
  </div>
</template>

<style scoped>
.emoji-picker {
  width: 320px;
  background: var(--bg-primary, #fff);
  border: 1px solid var(--border-light, #e4e7ed);
  border-radius: 8px;
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.12);
  overflow: hidden;
}

.emoji-tabs {
  display: flex;
  border-bottom: 1px solid var(--border-light, #e4e7ed);
}

.emoji-tabs button {
  flex: 1;
  padding: 8px 4px;
  font-size: 12px;
  background: none;
  border: none;
  border-bottom: 2px solid transparent;
  cursor: pointer;
  color: var(--text-regular, #606266);
  transition: all 0.15s;
}

.emoji-tabs button:hover {
  color: var(--primary, #409eff);
}

.emoji-tabs button.active {
  color: var(--primary, #409eff);
  border-bottom-color: var(--primary, #409eff);
}

.emoji-grid {
  display: grid;
  grid-template-columns: repeat(7, 1fr);
  gap: 2px;
  padding: 8px;
  max-height: 200px;
  overflow-y: auto;
}

.emoji-item {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 36px;
  height: 36px;
  font-size: 22px;
  cursor: pointer;
  border-radius: 4px;
  transition: background 0.1s;
}

.emoji-item:hover {
  background: var(--bg-secondary, #f5f7fa);
}
</style>
