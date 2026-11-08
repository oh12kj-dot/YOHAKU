import { defineConfig } from 'vitest/config';
export default defineConfig({ test: { include: ['workers/import/src/**/*.test.ts'] } });
