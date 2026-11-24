"""최초 관리자 계정 생성/승격.

docker compose exec api python scripts/create_admin.py admin@example.com 'Password!23'
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from sqlalchemy import create_engine, text  # noqa: E402

from app.security.email_norm import normalize_email  # noqa: E402
from app.security.passwords import hash_password, validate_password_policy  # noqa: E402

if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: create_admin.py <email> <password>")
    raw, pw = sys.argv[1], sys.argv[2]
    errs = validate_password_policy(pw, raw)
    if errs:
        sys.exit("\n".join(errs))
    email = normalize_email(raw)
    engine = create_engine(os.environ["DATABASE_URL"].replace("+asyncmy", "+pymysql"))
    with engine.begin() as conn:
        h = hash_password(pw)
        conn.execute(text("""INSERT INTO users (email, email_raw, password_hash, role, email_verified)
                             VALUES (:e, :r, :h, 'admin', TRUE)
                             ON DUPLICATE KEY UPDATE role = 'admin', email_verified = TRUE,
                               password_hash = VALUES(password_hash), token_version = token_version + 1"""),
                     dict(e=email, r=raw, h=h))
        uid = conn.execute(text("SELECT id FROM users WHERE email = :e"), dict(e=email)).scalar()
        conn.execute(text("INSERT INTO password_history (user_id, hash) VALUES (:u, :h)"), dict(u=uid, h=h))
    print(f"admin ready: {raw} (id={uid})")
