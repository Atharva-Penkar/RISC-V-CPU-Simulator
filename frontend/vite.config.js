import react from "@vitejs/plugin-react";
import { defineConfig } from "vite";

// https://vite.dev/config/
export default defineConfig(({ command }) => ({
  plugins: [react()],
  // GitHub Pages serves the site under /RISC-V-CPU-Simulator/; the dev server serves it at /.
  base: command === "build" ? "/RISC-V-CPU-Simulator/" : "/",
  server: {
    // In development the API runs locally (backend: npm start); requests to /api go there.
    proxy: {
      "/api": "http://localhost:3001",
    },
  },
}));
