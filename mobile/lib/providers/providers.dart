import 'package:dio/dio.dart' show Options;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../api/api_client.dart';
import '../api/device_transport.dart';
import '../push.dart';

/// main()에서 overrideWithValue 로 주입
final apiProvider = Provider<ApiClient>((ref) => throw UnimplementedError());

final transportProvider = Provider((ref) => DeviceTransport(ref.watch(apiProvider)));
final pushProvider = Provider((ref) => PushService(ref.watch(apiProvider)));

class AuthState {
  const AuthState({this.user, this.loading = false});
  final Map<String, dynamic>? user;
  final bool loading;
  bool get loggedIn => user != null;
}

class AuthNotifier extends StateNotifier<AuthState> {
  AuthNotifier(this.ref) : super(const AuthState(loading: true)) {
    _restore();
  }

  final Ref ref;
  ApiClient get _api => ref.read(apiProvider);

  Future<void> _restore() async {
    try {
      final me = await _api.get('/auth/me');
      state = AuthState(user: me);
      ref.read(pushProvider).register().ignore();
    } catch (_) {
      state = const AuthState();
    }
  }

  Future<void> login(String email, String password) async {
    final user = await _api.post('/auth/login', {'email': email, 'password': password});
    state = AuthState(user: Map<String, dynamic>.from(user as Map));
    ref.read(pushProvider).register().ignore();
  }

  Future<void> logout() async {
    final token = await ref.read(pushProvider).currentToken().catchError((_) => null);
    try {
      // 이 기기의 FCM 토큰만 삭제 (다른 폰 알림은 유지)
      await _api.call((d) => d.post('/auth/logout', options: _headers(token)));
    } catch (_) {}
    await _api.clearSession();
    state = const AuthState();
  }

  static Options? _headers(String? token) => token == null ? null : Options(headers: {'X-FCM-Token': token});
}

final authProvider = StateNotifierProvider<AuthNotifier, AuthState>((ref) => AuthNotifier(ref));

final devicesProvider = FutureProvider.autoDispose<List<Map<String, dynamic>>>((ref) async {
  final list = await ref.watch(apiProvider).getList('/devices');
  return list.map((e) => Map<String, dynamic>.from(e as Map)).toList();
});

final deviceProvider = FutureProvider.autoDispose.family<Map<String, dynamic>, String>((ref, id) async {
  return ref.watch(apiProvider).get('/devices/$id');
});

final historyProvider =
    FutureProvider.autoDispose.family<List<Map<String, dynamic>>, (String, int)>((ref, args) async {
  final list = await ref.watch(apiProvider).getList('/devices/${args.$1}/history', query: {'hours': args.$2});
  return list.map((e) => Map<String, dynamic>.from(e as Map)).toList();
});

final notificationsProvider = FutureProvider.autoDispose<List<Map<String, dynamic>>>((ref) async {
  final list = await ref.watch(apiProvider).getList('/notifications');
  return list.map((e) => Map<String, dynamic>.from(e as Map)).toList();
});
