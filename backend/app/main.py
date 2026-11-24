import asyncio
import logging
import os
from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from .config import get_settings
from .routers import admin, auth, devices, inquiry, notifications, ws
from .security.csrf import CSRFMiddleware
from .services import mqtt_bridge
from .services.redis_client import close_redis
from .services.ws_manager import manager

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")

settings = get_settings()   # SECRET_KEY 32자 미만이면 여기서 기동 실패


@asynccontextmanager
async def lifespan(_: FastAPI):
    tasks = [asyncio.create_task(mqtt_bridge.run_forever()), asyncio.create_task(manager.run_listener())]
    yield
    for t in tasks:
        t.cancel()
    await close_redis()


app = FastAPI(title="Smart Clean Ashtray API", lifespan=lifespan,
              docs_url="/docs" if not settings.cookie_secure else None, redoc_url=None)

# 마지막에 추가한 미들웨어가 가장 바깥 → CORS가 CSRF 403 응답에도 헤더를 붙이도록 CORS를 나중에
app.add_middleware(CSRFMiddleware)
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origin_list,
    allow_credentials=True,
    allow_methods=["GET", "POST", "PUT", "PATCH", "DELETE"],
    allow_headers=["Content-Type", "X-CSRF-Token", "X-FCM-Token"],
)

for r in (auth.router, devices.router, notifications.router, inquiry.router, admin.router, ws.router):
    app.include_router(r)

os.makedirs(settings.firmware_dir, exist_ok=True)
# OTA 다운로드 (ESP32가 sha256으로 무결성 검증)
app.mount("/firmware", StaticFiles(directory=settings.firmware_dir), name="firmware")


@app.get("/health")
async def health():
    return {"status": "ok"}
