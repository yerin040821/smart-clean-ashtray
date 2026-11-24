import 'dart:io';

import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';

import 'api/api_client.dart';

/// FCM — 앱 백그라운드/종료 시 OS가 표시, 포그라운드일 땐 로컬 알림으로 표시.
class PushService {
  PushService(this.api);

  final ApiClient api;
  final _local = FlutterLocalNotificationsPlugin();
  static const _channel = AndroidNotificationChannel('sca_alerts', '재떨이 알림',
      description: '화재 감지·잔량 경고', importance: Importance.high);

  Future<void> init() async {
    await _local.initialize(settings: const InitializationSettings(
      android: AndroidInitializationSettings('@mipmap/ic_launcher'),
      iOS: DarwinInitializationSettings(),
    ));
    await _local
        .resolvePlatformSpecificImplementation<AndroidFlutterLocalNotificationsPlugin>()
        ?.createNotificationChannel(_channel);

    FirebaseMessaging.onMessage.listen((msg) {
      final n = msg.notification;
      if (n == null) return;
      _local.show(
        id: n.hashCode,
        title: n.title,
        body: n.body,
        notificationDetails: NotificationDetails(
          android: AndroidNotificationDetails(_channel.id, _channel.name, importance: Importance.high, priority: Priority.high),
          iOS: const DarwinNotificationDetails(),
        ),
      );
    });
  }

  // firebase_messaging 17 은 getToken/onTokenRefresh 를 deprecated 처리하고 register()/onRegistered
  // (Firebase Installation ID 기반 direct-send)를 권장. 백엔드 firebase-admin 의 토큰 멀티캐스트와
  // 호환되는 기존 등록 토큰 방식을 유지 — 서버 측 direct-send 전환 시 함께 교체할 것.

  /// 로그인 직후 호출 — 토큰을 서버 fcm_tokens 테이블에 저장 (기기별 다중 토큰)
  Future<String?> register() async {
    final fm = FirebaseMessaging.instance;
    await fm.requestPermission();
    // ignore: deprecated_member_use
    final token = await fm.getToken();
    if (token != null) await _save(token);
    // ignore: deprecated_member_use
    fm.onTokenRefresh.listen(_save);
    return token;
  }

  Future<void> _save(String token) => api.post('/me/fcm-tokens', {
        'token': token,
        'device_os': Platform.isIOS ? 'ios' : 'android',
      });

  // ignore: deprecated_member_use
  Future<String?> currentToken() => FirebaseMessaging.instance.getToken();
}
