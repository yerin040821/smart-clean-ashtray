import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// 개발: /api → FastAPI(localhost:8000) 프록시 (쿠키 same-origin 처리)
export default defineConfig({
  plugins: [react()],
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          react: ["react", "react-dom", "react-router-dom"],
          charts: ["recharts"],
          editor: ["@tiptap/react", "@tiptap/starter-kit"],
        },
      },
    },
  },
  server: {
    proxy: {
      "/api": {
        target: "http://localhost:8000",
        changeOrigin: true,
        ws: true,
        rewrite: (p) => p.replace(/^\/api/, ""),
      },
    },
  },
});
