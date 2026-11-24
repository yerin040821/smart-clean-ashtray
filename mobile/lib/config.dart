class AppConfig {
  static const apiBase = String.fromEnvironment('API_BASE', defaultValue: 'https://api.smart-clean-ashtray.com');
  static const wsBase = String.fromEnvironment('WS_BASE', defaultValue: 'wss://api.smart-clean-ashtray.com');

  /// 로컬 모드 시도 타임아웃 — 실패 시 클라우드 폴백 (설계서 8장)
  static const localTimeout = Duration(milliseconds: 500);
}
