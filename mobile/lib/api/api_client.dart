
import 'package:cookie_jar/cookie_jar.dart';
import 'package:dio/dio.dart';
import 'package:dio_cookie_manager/dio_cookie_manager.dart';
import 'package:path_provider/path_provider.dart';

import '../config.dart';

class ApiException implements Exception {
  ApiException(this.status, this.message);
  final int? status;
  final String message;
  @override
  String toString() => message;
}

/// 웹과 동일하게 HttpOnly JWT 쿠키 + CSRF Double Submit 사용.
/// 쿠키는 PersistCookieJar(앱 전용 저장소)에 보관.
class ApiClient {
  ApiClient._(this.dio, this.jar);

  final Dio dio;
  final PersistCookieJar jar;

  static Future<ApiClient> create() async {
    final dir = await getApplicationSupportDirectory();
    final jar = PersistCookieJar(storage: FileStorage('${dir.path}/cookies/'));
    final dio = Dio(BaseOptions(
      baseUrl: AppConfig.apiBase,
      connectTimeout: const Duration(seconds: 10),
      receiveTimeout: const Duration(seconds: 15),
      contentType: 'application/json',
    ));
    final client = ApiClient._(dio, jar);
    dio.interceptors
      ..add(CookieManager(jar))
      ..add(InterceptorsWrapper(
        onRequest: (opts, handler) async {
          if (opts.method != 'GET') {
            final csrf = await client._cookie('csrf_token');
            if (csrf != null) opts.headers['X-CSRF-Token'] = csrf;
          }
          handler.next(opts);
        },
        onError: (e, handler) async {
          final path = e.requestOptions.path;
          if (e.response?.statusCode == 401 && !path.startsWith('/auth/') && e.requestOptions.extra['retried'] != true) {
            if (await client._refresh()) {
              final req = e.requestOptions..extra['retried'] = true;
              final csrf = await client._cookie('csrf_token');
              if (csrf != null) req.headers['X-CSRF-Token'] = csrf;
              try {
                return handler.resolve(await dio.fetch(req));
              } on DioException catch (err) {
                return handler.next(err);
              }
            }
          }
          handler.next(e);
        },
      ));
    return client;
  }

  Future<String?> _cookie(String name) async {
    final cookies = await jar.loadForRequest(Uri.parse(AppConfig.apiBase));
    for (final c in cookies) {
      if (c.name == name) return c.value;
    }
    return null;
  }

  Future<bool> _refresh() async {
    try {
      await dio.post('/auth/refresh');
      return true;
    } catch (_) {
      return false;
    }
  }

  /// 쿠키 헤더 문자열 (WebSocket 핸드셰이크용)
  Future<String> cookieHeader() async {
    final cookies = await jar.loadForRequest(Uri.parse(AppConfig.apiBase));
    return cookies.map((Cookie c) => '${c.name}=${c.value}').join('; ');
  }

  Future<T> call<T>(Future<Response<dynamic>> Function(Dio d) fn) async {
    try {
      final res = await fn(dio);
      return res.data as T;
    } on DioException catch (e) {
      final detail = e.response?.data is Map ? (e.response!.data as Map)['detail'] : null;
      throw ApiException(e.response?.statusCode, detail is String ? detail : '네트워크 오류가 발생했습니다.');
    }
  }

  Future<Map<String, dynamic>> get(String path, {Map<String, dynamic>? query}) =>
      call((d) => d.get(path, queryParameters: query));
  Future<List<dynamic>> getList(String path, {Map<String, dynamic>? query}) =>
      call((d) => d.get(path, queryParameters: query));
  Future<dynamic> post(String path, [Object? body]) => call((d) => d.post(path, data: body));

  Future<void> clearSession() => jar.deleteAll();
}
