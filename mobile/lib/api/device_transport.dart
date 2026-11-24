import 'package:dio/dio.dart';

import '../config.dart';
import 'api_client.dart';

enum CommandRoute { local, cloud }

/// 설계서 8장 — 로컬 우선(500ms) → 실패 시 클라우드 자동 폴백.
///
/// 로컬: http://{local_ip}/api/... (또는 ashtray-{id}.local) — 헤더 X-Local-Key 로 인증
/// 클라우드: https://api.../devices/{id}/command → MQTT
class DeviceTransport {
  DeviceTransport(this.api);

  final ApiClient api;
  final _local = Dio(BaseOptions(
    connectTimeout: AppConfig.localTimeout,
    receiveTimeout: const Duration(seconds: 2),
    contentType: 'application/json',
  ));

  /// 최근 로컬 실패 기기는 30초간 로컬 시도 생략 (매번 500ms 지연 방지)
  final _localFailedAt = <String, DateTime>{};

  List<String> _hosts(Map<String, dynamic> device) => [
        if (device['local_ip'] != null) device['local_ip'] as String,
        'ashtray-${(device['id'] as String).toLowerCase()}.local',
      ];

  bool _skipLocal(String id) {
    final t = _localFailedAt[id];
    return t != null && DateTime.now().difference(t) < const Duration(seconds: 30);
  }

  Future<CommandRoute> sendCommand(Map<String, dynamic> device, String capability, String action,
      Map<String, dynamic> params) async {
    final id = device['id'] as String;
    final key = device['local_key'] as String?;
    if (key != null && !_skipLocal(id)) {
      for (final host in _hosts(device)) {
        try {
          await _local.post('http://$host/api/command',
              data: {'action': action, 'params': params}, options: Options(headers: {'X-Local-Key': key}));
          _localFailedAt.remove(id);
          return CommandRoute.local;
        } catch (_) {/* 다음 후보 */}
      }
      _localFailedAt[id] = DateTime.now();
    }
    await api.post('/devices/$id/command', {'capability': capability, 'params': params});
    return CommandRoute.cloud;
  }

  /// 로컬에서 최신 센서값 직접 조회 (같은 WiFi일 때 빠른 초기 표시용). 실패 시 null.
  Future<Map<String, dynamic>?> localStatus(Map<String, dynamic> device) async {
    final key = device['local_key'] as String?;
    if (key == null || _skipLocal(device['id'] as String)) return null;
    for (final host in _hosts(device)) {
      try {
        final r = await _local.get('http://$host/api/status', options: Options(headers: {'X-Local-Key': key}));
        return Map<String, dynamic>.from(r.data as Map);
      } catch (_) {}
    }
    _localFailedAt[device['id'] as String] = DateTime.now();
    return null;
  }
}
