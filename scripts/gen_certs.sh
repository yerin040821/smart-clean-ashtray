#!/usr/bin/env bash
# MQTT TLS 인증서 발급 (openssl self-signed CA)
# 사용: ./scripts/gen_certs.sh [DEVICE_ID ...]   (기본: SCA_S SCA_Y)
# ca.key 는 mosquitto/certs 가 아닌 별도 안전 위치(certs-ca/)에 보관. *.key 는 .gitignore 대상.
set -euo pipefail
cd "$(dirname "$0")/.."

CA_DIR=certs-ca
MQ_DIR=mosquitto/certs
FW_DIR=firmware/certs
MQTT_HOST=${MQTT_HOST:-mqtt.smart-clean-ashtray.com}
DAYS=3650
mkdir -p "$CA_DIR" "$MQ_DIR" "$FW_DIR"

# 1. CA
if [[ ! -f $CA_DIR/ca.key ]]; then
  openssl genrsa -out "$CA_DIR/ca.key" 4096
  openssl req -new -x509 -days $DAYS -key "$CA_DIR/ca.key" -out "$CA_DIR/ca.crt" -subj "/CN=SCA-MQTT-CA"
fi
cp "$CA_DIR/ca.crt" "$MQ_DIR/ca.crt"
cp "$CA_DIR/ca.crt" "$FW_DIR/ca.crt"

# 2. 서버(Mosquitto) — SAN 포함 (최신 TLS 클라이언트는 CN만으로 호스트 검증 안 함)
if [[ ! -f $MQ_DIR/server.crt ]]; then
  openssl genrsa -out "$MQ_DIR/server.key" 2048
  openssl req -new -key "$MQ_DIR/server.key" -out "$MQ_DIR/server.csr" -subj "/CN=$MQTT_HOST"
  printf "subjectAltName=DNS:%s\n" "$MQTT_HOST" > "$MQ_DIR/server.ext"
  openssl x509 -req -days $DAYS -in "$MQ_DIR/server.csr" -CA "$CA_DIR/ca.crt" -CAkey "$CA_DIR/ca.key" \
    -CAcreateserial -extfile "$MQ_DIR/server.ext" -out "$MQ_DIR/server.crt"
  rm -f "$MQ_DIR/server.csr" "$MQ_DIR/server.ext"
fi

# 3. 기기별 클라이언트 인증서 (CN = device_id → Mosquitto ACL username)
DEVICES=("$@"); [[ ${#DEVICES[@]} -eq 0 ]] && DEVICES=(SCA_S SCA_Y)
for dev in "${DEVICES[@]}"; do
  lower=$(echo "$dev" | tr '[:upper:]' '[:lower:]')
  [[ -f $FW_DIR/$lower.crt ]] && { echo "skip $dev (exists)"; continue; }
  openssl genrsa -out "$FW_DIR/$lower.key" 2048
  openssl req -new -key "$FW_DIR/$lower.key" -out "$FW_DIR/$lower.csr" -subj "/CN=$dev"
  openssl x509 -req -days $DAYS -in "$FW_DIR/$lower.csr" -CA "$CA_DIR/ca.crt" -CAkey "$CA_DIR/ca.key" \
    -CAcreateserial -out "$FW_DIR/$lower.crt"
  rm -f "$FW_DIR/$lower.csr"
  echo "issued $dev"
done

# 4. 펌웨어 프로젝트의 LittleFS data/ 폴더에 배치 (firmware/<DEVICE_ID>/data 가 있으면)
API_CA="$FW_DIR/api_ca.pem"
if [[ ! -f $API_CA ]]; then
  curl -fsSL https://letsencrypt.org/certs/isrgrootx1.pem -o "$API_CA" \
    || echo "⚠️ ISRG Root X1 다운로드 실패 — $API_CA 를 수동으로 준비하세요"
fi
for dev in "${DEVICES[@]}"; do
  lower=$(echo "$dev" | tr '[:upper:]' '[:lower:]')
  DATA="firmware/$dev/data"
  [[ -d $DATA ]] || continue
  cp "$FW_DIR/ca.crt" "$DATA/ca.crt"
  cp "$FW_DIR/$lower.crt" "$DATA/client.crt"
  cp "$FW_DIR/$lower.key" "$DATA/client.key"
  [[ -f $API_CA ]] && cp "$API_CA" "$DATA/api_ca.pem"
  echo "placed certs → $DATA (pio run -t uploadfs)"
done
echo "done. ⚠️ $CA_DIR/ca.key 는 오프라인 백업 후 서버에서 제거 권장"
