"""텔레메트리 파티션 관리 (매월 1일 cron).

1) 다음 달 파티션 사전 생성  2) 13개월 이전 파티션 DROP

설계서 초안의 `ADD PARTITION IF NOT EXISTS` / `DROP PARTITION IF EXISTS`는 MySQL에 없는 문법 →
information_schema로 존재 여부를 확인하고, MAXVALUE 파티션이 있으므로 ADD 대신 REORGANIZE 로 분할.
"""
import os
import sys
from datetime import date

from dateutil.relativedelta import relativedelta
from sqlalchemy import create_engine, text

RETENTION_MONTHS = 13


def existing_partitions(conn) -> set[str]:
    rows = conn.execute(text("""SELECT PARTITION_NAME FROM information_schema.PARTITIONS
                                WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'telemetry'""")).all()
    return {r[0] for r in rows if r[0]}


def plan(today: date, existing: set[str]) -> tuple[list[tuple[str, str]], list[str]]:
    """(생성할 [(이름, LESS THAN 값)], 삭제할 [이름]) — 순수 함수 (테스트용)."""
    first = today.replace(day=1)
    to_add = []
    for i in (0, 1):   # 이번 달(누락 대비) + 다음 달
        m = first + relativedelta(months=i)
        name = f"p{m:%Y%m}"
        if name not in existing:
            to_add.append((name, f"{m + relativedelta(months=1):%Y%m}"))
    cutoff = f"p{first - relativedelta(months=RETENTION_MONTHS):%Y%m}"
    to_drop = sorted(p for p in existing if p != "pmax" and p.startswith("p") and p <= cutoff)
    return to_add, to_drop


def main() -> None:
    url = os.environ["DATABASE_URL"].replace("+asyncmy", "+pymysql")
    engine = create_engine(url)
    with engine.begin() as conn:
        to_add, to_drop = plan(date.today(), existing_partitions(conn))
        for name, limit in to_add:
            conn.execute(text(f"""ALTER TABLE telemetry REORGANIZE PARTITION pmax INTO (
                PARTITION {name} VALUES LESS THAN ({limit}),
                PARTITION pmax VALUES LESS THAN MAXVALUE)"""))
            print(f"added {name}")
        for name in to_drop:
            conn.execute(text(f"ALTER TABLE telemetry DROP PARTITION {name}"))
            print(f"dropped {name}")
    return None


if __name__ == "__main__":
    sys.exit(main())
