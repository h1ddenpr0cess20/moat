import { defineConfig } from 'vite';

// The engine awaits the GPU at the top level, so the build targets browsers that have it.
export default defineConfig({
  base: './',
  build: { target: 'esnext', chunkSizeWarningLimit: 1500, rollupOptions: { input: { main: 'index.html', nes: 'nes.html' } } },
  esbuild: { target: 'esnext' },
  optimizeDeps: { esbuildOptions: { target: 'esnext' } },
});
