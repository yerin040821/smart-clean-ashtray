# secrets/ — 서버 비밀 파일 (깃에 커밋 금지)

docker-compose 가 이 폴더를 api 컨테이너의 `/run/secrets` 로 읽기 전용 마운트합니다.

| 파일 | 내용 | 받는 곳 |
|---|---|---|
| `firebase.json` | Firebase Admin 서비스 계정 키 (서버 → FCM 푸시 발송) | Firebase 콘솔 → 프로젝트 ID `smart-clean-ashtray-app-64ff5` (표시 이름 "smart-clean-ashtray-app") → ⚙️ 프로젝트 설정 → 서비스 계정 → "새 비공개 키 생성" |

`.env` 의 `FIREBASE_CREDENTIALS=/run/secrets/firebase.json` 과 짝을 이룹니다.
로컬 개발(도커 없이)에서는 `FIREBASE_CREDENTIALS=./secrets/firebase.json` 처럼 실제 경로를 지정하세요.
