import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';

import 'package:web_socket_channel/io.dart';

import '../config.dart';
import 'api_client.dart';

/// /ws/devices/{id} 실시간 텔레메트리 — 끊기면 지수 백오프 재연결.
/// 앱이 백그라운드로 가면 OS가 소켓을 끊으므로 resume 시 [reconnectNow] 호출 필수.
class DeviceSocket {
  DeviceSocket(this.api, this.deviceId);

  final ApiClient api;
  final String deviceId;
  final _controller = StreamController<Map<String, dynamic>>.broadcast();
  IOWebSocketChannel? _ch;
  Timer? _retry;
  Timer? _ping;
  int _attempt = 0;
  bool _closed = false;

  Stream<Map<String, dynamic>> get stream => _controller.stream;

  Future<void> connect() async {
    if (_closed) return;
    _retry?.cancel();
    try {
      final ws = await WebSocket.connect('${AppConfig.wsBase}/ws/devices/$deviceId',
          headers: {'Cookie': await api.cookieHeader()});
      _ch = IOWebSocketChannel(ws);
      _attempt = 0;
      _ping = Timer.periodic(const Duration(seconds: 25), (_) => _ch?.sink.add('ping'));
      _ch!.stream.listen(
        (data) {
          if (data == 'pong') return;
          try {
            _controller.add(jsonDecode(data as String) as Map<String, dynamic>);
          } catch (_) {}
        },
        onDone: () => _scheduleRetry(_ch?.closeCode),
        onError: (_) => _scheduleRetry(null),
        cancelOnError: true,
      );
    } catch (_) {
      _scheduleRetry(null);
    }
  }

  void _scheduleRetry(int? code) {
    _ping?.cancel();
    if (_closed || code == 4401 || code == 4403) return;
    final delay = Duration(milliseconds: min(30000, 1000 * pow(2, _attempt++).toInt()));
    _retry = Timer(delay, connect);
  }

  void reconnectNow() {
    _ch?.sink.close();
    _attempt = 0;
    connect();
  }

  void dispose() {
    _closed = true;
    _retry?.cancel();
    _ping?.cancel();
    _ch?.sink.close();
    _controller.close();
  }
}
