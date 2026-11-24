"""일일 정리 (cron 04:00): 오래된 로그인 시도·인증코드·MQTT 이벤트 삭제."""
import os

from sqlalchemy import create_engine, text

STATEMENTS = [
    # 성공 기록은 24시간, 실패 기록은 Rate Limit 최대 관찰창(48h) 이후 삭제
    "DELETE FROM login_attempts WHERE success = TRUE AND attempted_at < NOW() - INTERVAL 1 DAY",
    "DELETE FROM login_attempts WHERE attempted_at < NOW() - INTERVAL 2 DAY",
    "DELETE FROM email_verifications WHERE expires_at < NOW() - INTERVAL 1 DAY",
    "DELETE FROM device_events WHERE created_at < NOW() - INTERVAL 90 DAY",
]

if __name__ == "__main__":
    engine = create_engine(os.environ["DATABASE_URL"].replace("+asyncmy", "+pymysql"))
    with engine.begin() as conn:
        for sql in STATEMENTS:
            n = conn.execute(text(sql)).rowcount
            print(f"{n:>6}  {sql}")
