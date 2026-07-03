import { defineConfig } from 'vite'
import tailwindcss from '@tailwindcss/vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig(() => {
  const base = process.env.VITE_BASE_PATH ?? '/'
  const wasmVersion = process.env.VITE_WASM_VERSION?.trim()

  if (base !== '/' && !wasmVersion) {
    throw new Error('VITE_WASM_VERSION is required for non-root deployments to prevent stale WASM assets')
  }

  return {
    base,
    plugins: [vue(), tailwindcss()],
    build: {
      rollupOptions: {
        output: {
          manualChunks(id: string) {
            if (id.includes('node_modules/phaser')) {
              return 'phaser'
            }

            if (id.includes('node_modules/vue')) {
              return 'vue'
            }

            return undefined
          },
        },
      },
    },
  }
})
