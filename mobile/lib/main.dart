import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:material_ui/material_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'api/api_client.dart';
import 'firebase_options.dart';
import 'providers/providers.dart';
import 'push.dart';
import 'screens/home_screen.dart';
import 'screens/login_screen.dart';

@pragma('vm:entry-point')
Future<void> _bgHandler(RemoteMessage message) async {
  // 백그라운드/종료 상태 알림은 OS가 notification 페이로드로 직접 표시 — 별도 처리 불필요
  await Firebase.initializeApp(options: DefaultFirebaseOptions.currentPlatform);
}

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  // firebase_options.dart — `flutterfire configure` 로 생성 (프로젝트 ID smart-clean-ashtray-app-64ff5)
  await Firebase.initializeApp(options: DefaultFirebaseOptions.currentPlatform);
  FirebaseMessaging.onBackgroundMessage(_bgHandler);
  final api = await ApiClient.create();
  await PushService(api).init();
  runApp(ProviderScope(overrides: [apiProvider.overrideWithValue(api)], child: const ScaApp()));
}

class ScaApp extends ConsumerWidget {
  const ScaApp({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final auth = ref.watch(authProvider);
    return MaterialApp(
      title: 'Smart Clean Ashtray',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(colorSchemeSeed: const Color(0xFF059669), useMaterial3: true),
      darkTheme: ThemeData(colorSchemeSeed: const Color(0xFF059669), brightness: Brightness.dark, useMaterial3: true),
      home: auth.loading
          ? const Scaffold(body: Center(child: CircularProgressIndicator()))
          : auth.loggedIn
              ? const HomeScreen()
              : const LoginScreen(),
    );
  }
}
