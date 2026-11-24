from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    secret_key: str = ""
    database_url: str = "mysql+asyncmy://sca:sca_password@localhost:3306/sca"
    mqtt_host: str = "localhost"
    mqtt_port: int = 1883
    redis_url: str = "redis://localhost:6379/0"

    cookie_domain: str | None = None
    cookie_secure: bool = True
    cors_origins: str = "http://localhost:5173"
    admin_allowed_ips: str = "127.0.0.1"

    access_token_minutes: int = 15
    refresh_token_days: int = 14

    smtp_host: str = "smtp.gmail.com"
    smtp_port: int = 587
    smtp_user: str = ""
    smtp_password: str = ""
    smtp_from: str = ""

    firebase_credentials: str = ""
    firmware_dir: str = "/data/firmware"
    firmware_base_url: str = ""

    @property
    def cors_origin_list(self) -> list[str]:
        return [o.strip() for o in self.cors_origins.split(",") if o.strip()]


def validate_secret_key(key: str) -> None:
    """4-2. 비밀키 길이 강제 — 32자 미만이면 서버 기동 거부."""
    if len(key) < 32:
        raise RuntimeError("SECRET_KEY는 32자 이상이어야 합니다. 서버를 시작할 수 없습니다.")


@lru_cache
def get_settings() -> Settings:
    s = Settings()
    validate_secret_key(s.secret_key)
    return s
