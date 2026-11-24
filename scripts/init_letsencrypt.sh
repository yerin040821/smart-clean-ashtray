#!/usr/bin/env bash
# 최초 인증서 발급 (webroot 방식).
# ⚠️ 와일드카드(*.smart-clean-ashtray.com)는 DNS-01 챌린지만 가능하고 webroot로는 발급 불가.
#    cafe24 DNS는 certbot 플러그인이 없어 자동갱신이 어려우므로, 서브도메인을 SAN으로 나열한 단일 인증서를 사용.
#    (mqtt.* 는 자체 CA로 TLS 처리하므로 포함하지 않음)
set -euo pipefail
cd "$(dirname "$0")/.."
EMAIL=${1:?usage: init_letsencrypt.sh <admin-email>}

mkdir -p certbot/www certbot/conf
# nginx가 ssl 인증서 없이 뜨지 못하므로 최초에는 임시 self-signed 인증서로 기동
LIVE=certbot/conf/live/smart-clean-ashtray.com
if [[ ! -f $LIVE/fullchain.pem ]]; then
  mkdir -p "$LIVE"
  openssl req -x509 -nodes -newkey rsa:2048 -days 1 -subj "/CN=localhost" \
    -keyout "$LIVE/privkey.pem" -out "$LIVE/fullchain.pem"
fi
docker compose up -d nginx
rm -rf "$LIVE"
docker compose run --rm certbot certonly --webroot -w /var/www/certbot \
  --cert-name smart-clean-ashtray.com \
  -d smart-clean-ashtray.com -d www.smart-clean-ashtray.com \
  -d admin.smart-clean-ashtray.com -d api.smart-clean-ashtray.com \
  --email "$EMAIL" --agree-tos --no-eff-email
docker compose exec nginx nginx -s reload
