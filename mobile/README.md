# SCA Mobile (Flutter + Riverpod 2.x)

Flutter 3.50.1 (Dart 3.14) 기준으로 android/ios 폴더 생성 완료. 최소 **Android API 24 (7.0) / iOS 15**.

```bash
cd mobile
flutter pub get
flutterfire configure            # Firebase 프로젝트 연결 → google-services.json / GoogleService-Info.plist 생성
flutter run --dart-define=API_BASE=https://api.smart-clean-ashtray.com --dart-define=WS_BASE=wss://api.smart-clean-ashtray.com
```

## 적용된 플랫폼 설정

| 파일 | 내용 |
|---|---|
| `android/app/build.gradle.kts` | `minSdk = 24`, core library desugaring (flutter_local_notifications 요구) |
| `android/app/src/main/AndroidManifest.xml` | INTERNET, POST_NOTIFICATIONS, 멀티캐스트 권한, FCM 기본 채널 `sca_alerts` |
| `android/app/src/main/res/xml/network_security_config.xml` | 로컬 모드 평문 HTTP 허용 (서버 API는 https 고정) |
| `ios/Runner/Info.plist` | 로컬 네트워크 권한 문구, Bonjour, `NSAllowsLocalNetworking`, 원격 알림 백그라운드 모드 |
| `ios/Runner.xcodeproj` | 배포 타깃 iOS 15.0 |

### 아직 직접 해야 하는 것
- `flutterfire configure` → `google-services.json`, `GoogleService-Info.plist` (Firebase 프로젝트 필요)
- Xcode(맥)에서 **Push Notifications** capability 추가 + APNs 키를 Firebase 콘솔에 등록
- 릴리스 서명 키 설정 (`build.gradle.kts` 의 release signingConfig 가 현재 debug)

### 최소 버전을 바꾼 이유
- Android: Flutter 3.50 은 minSdk 23 미만이면 **빌드 에러**, 24 미만이면 경고 → 설계서의 API 21 불가
- iOS: Flutter 3.50 은 배포 타깃 15 미만을 15.0 으로 자동 상향 → 설계서의 iOS 14 불가

## 구조
| 파일 | 역할 |
|---|---|
| `api/api_client.dart` | Dio + 쿠키 저장소, CSRF 헤더 자동 첨부, 401 시 `/auth/refresh` 후 재시도 |
| `api/device_transport.dart` | 로컬(500ms) → 클라우드 자동 폴백 |
| `api/ws_client.dart` | WebSocket 재연결 (지수 백오프, 앱 resume 시 즉시) |
| `push.dart` | FCM 토큰 등록(`/me/fcm-tokens`), 포그라운드 로컬 알림 |
| `screens/device_screen.dart` | capability `param_schema` 기반 컨트롤 자동 생성 |
