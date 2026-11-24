import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { BrowserRouter } from "react-router-dom";
import App from "./App";
import { AuthProvider } from "./hooks/useAuth";
import "./index.css";

async function bootstrap() {
  // `npm run dev:mock` — 백엔드 없이 화면 검토용 가짜 API (운영 빌드에서는 이 분기가 제거됨)
  if (import.meta.env.VITE_MOCK === "1") await import("./mock/install");

  createRoot(document.getElementById("root")!).render(
    <StrictMode>
      <BrowserRouter>
        <AuthProvider>
          <App />
        </AuthProvider>
      </BrowserRouter>
    </StrictMode>,
  );
}

bootstrap();
