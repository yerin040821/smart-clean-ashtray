import 'package:material_ui/material_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/providers.dart';
import 'device_screen.dart';
import 'notifications_screen.dart';

class HomeScreen extends ConsumerWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final devices = ref.watch(devicesProvider);
    return Scaffold(
      appBar: AppBar(
        title: const Text('내 기기'),
        actions: [
          IconButton(
            icon: const Icon(Icons.notifications_outlined),
            onPressed: () => Navigator.push(context, MaterialPageRoute(builder: (_) => const NotificationsScreen())),
          ),
          IconButton(icon: const Icon(Icons.logout), onPressed: () => ref.read(authProvider.notifier).logout()),
        ],
      ),
      body: RefreshIndicator(
        onRefresh: () => ref.refresh(devicesProvider.future),
        child: devices.when(
          loading: () => const Center(child: CircularProgressIndicator()),
          error: (e, _) => ListView(children: [Padding(padding: const EdgeInsets.all(24), child: Text('$e'))]),
          data: (list) => list.isEmpty
              ? ListView(children: const [
                  Padding(padding: EdgeInsets.all(48), child: Text('등록된 기기가 없습니다.\n웹에서 기기 등록(AP 모드 설정)을 진행해 주세요.', textAlign: TextAlign.center)),
                ])
              : ListView.separated(
                  padding: const EdgeInsets.all(16),
                  itemCount: list.length,
                  separatorBuilder: (_, _) => const SizedBox(height: 12),
                  itemBuilder: (_, i) => _DeviceTile(device: list[i]),
                ),
        ),
      ),
    );
  }
}

class _DeviceTile extends StatelessWidget {
  const _DeviceTile({required this.device});
  final Map<String, dynamic> device;

  @override
  Widget build(BuildContext context) {
    final latest = (device['latest'] as Map?) ?? {};
    final online = device['online'] == true || device['online'] == 1;
    final fire = latest['fire'] == 1 || latest['fire'] == true;
    return Card(
      color: fire ? Colors.red.shade50 : null,
      child: ListTile(
        contentPadding: const EdgeInsets.all(16),
        title: Text(device['name'] ?? device['id'], style: const TextStyle(fontWeight: FontWeight.bold)),
        subtitle: Padding(
          padding: const EdgeInsets.only(top: 8),
          child: Text(fire
              ? '🔥 화재 감지!'
              : '공기질 Δ${latest['gas_diff'] ?? '—'} · 잔량 ${latest['ash_level'] ?? '—'}%\n${device['type_name']} · ${device['state'] ?? '—'}'),
        ),
        trailing: Icon(Icons.circle, size: 12, color: online ? Colors.green : Colors.grey),
        onTap: () => Navigator.push(context, MaterialPageRoute(builder: (_) => DeviceScreen(deviceId: device['id']))),
      ),
    );
  }
}
