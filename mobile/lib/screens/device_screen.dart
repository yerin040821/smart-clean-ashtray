import 'dart:async';

import 'package:material_ui/material_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../api/device_transport.dart';
import '../api/ws_client.dart';
import '../providers/providers.dart';
import 'history_screen.dart';

/// 기기 상세 — Capability 동적 렌더링 + WebSocket 실시간 + 로컬/클라우드 명령 전송
class DeviceScreen extends ConsumerStatefulWidget {
  const DeviceScreen({super.key, required this.deviceId});
  final String deviceId;

  @override
  ConsumerState<DeviceScreen> createState() => _DeviceScreenState();
}

class _DeviceScreenState extends ConsumerState<DeviceScreen> with WidgetsBindingObserver {
  DeviceSocket? _socket;
  StreamSubscription? _sub;
  final Map<String, dynamic> _live = {};
  CommandRoute? _lastRoute;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _socket = DeviceSocket(ref.read(apiProvider), widget.deviceId)..connect();
    _sub = _socket!.stream.listen((m) {
      if (m['type'] == 'telemetry') setState(() => _live.addAll(Map<String, dynamic>.from(m['data'] as Map)));
    });
    _primeLocal();
  }

  Future<void> _primeLocal() async {
    final dev = await ref.read(deviceProvider(widget.deviceId).future);
    final local = await ref.read(transportProvider).localStatus(dev);
    if (local != null && mounted) setState(() => _live.addAll(local));
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    // 백그라운드 진입 시 OS가 소켓을 끊음 → 복귀 시 즉시 재연결
    if (state == AppLifecycleState.resumed) _socket?.reconnectNow();
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _sub?.cancel();
    _socket?.dispose();
    super.dispose();
  }

  Future<void> _send(Map<String, dynamic> device, Map<String, dynamic> cap, Map<String, dynamic> params) async {
    final messenger = ScaffoldMessenger.of(context);
    try {
      final route = await ref.read(transportProvider).sendCommand(device, cap['cap_id'], cap['mqtt_action'], params);
      setState(() => _lastRoute = route);
      messenger.showSnackBar(SnackBar(content: Text('${cap['display_name']} 명령 전송 (${route == CommandRoute.local ? '로컬' : '클라우드'})')));
    } catch (e) {
      messenger.showSnackBar(SnackBar(content: Text('$e')));
    }
  }

  @override
  Widget build(BuildContext context) {
    final device = ref.watch(deviceProvider(widget.deviceId));
    return Scaffold(
      appBar: AppBar(
        title: Text(device.valueOrNull?['name'] ?? widget.deviceId),
        actions: [
          if (_lastRoute != null)
            Padding(padding: const EdgeInsets.only(right: 4), child: Chip(label: Text(_lastRoute == CommandRoute.local ? '로컬' : '클라우드'))),
          IconButton(
            icon: const Icon(Icons.show_chart),
            onPressed: () => Navigator.push(context, MaterialPageRoute(builder: (_) => HistoryScreen(deviceId: widget.deviceId))),
          ),
        ],
      ),
      body: device.when(
        loading: () => const Center(child: CircularProgressIndicator()),
        error: (e, _) => Center(child: Text('$e')),
        data: (dev) {
          final data = {...?(dev['latest'] as Map?)?.cast<String, dynamic>(), ..._live};
          final caps = (dev['capabilities'] as List).cast<Map>().map((c) => c.cast<String, dynamic>()).toList();
          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              Text('${dev['type_name']} · 상태 ${dev['state'] ?? '—'} · FW ${dev['fw_version'] ?? '—'}',
                  style: Theme.of(context).textTheme.bodySmall),
              const SizedBox(height: 12),
              for (final cap in caps) CapabilityCard(cap: cap, data: data, onCommand: (p) => _send(dev, cap, p)),
            ],
          );
        },
      ),
    );
  }
}

/// capability_definitions 기반 범용 카드: telemetry_keys 표시 + param_schema로 컨트롤 자동 생성
class CapabilityCard extends StatefulWidget {
  const CapabilityCard({super.key, required this.cap, required this.data, required this.onCommand});
  final Map<String, dynamic> cap;
  final Map<String, dynamic> data;
  final Future<void> Function(Map<String, dynamic> params) onCommand;

  @override
  State<CapabilityCard> createState() => _CapabilityCardState();
}

class _CapabilityCardState extends State<CapabilityCard> {
  late final Map<String, dynamic> _params = {
    for (final e in ((widget.cap['param_schema'] as Map?) ?? {}).entries) e.key as String: (e.value as Map)['default'],
  };
  bool _busy = false;

  static const _labels = {
    'gas_diff': ('공기질 변화량', ''), 'gas_raw': ('가스 원시값', ''), 'temperature': ('온도', '°C'), 'humidity': ('습도', '%'), 'fire': ('화재', ''),
    'ash_level': ('재 잔량', '%'), 'water_level': ('수위', '%'), 'water_ok': ('물 충분', ''), 'door_open': ('덮개 열림', ''),
    'sleeping': ('절전 중', ''), 'pump_on': ('펌프', ''), 'led_color': ('LED', ''),
  };

  Future<void> _run([Map<String, dynamic>? override]) async {
    setState(() => _busy = true);
    await widget.onCommand({..._params, ...?override});
    if (mounted) setState(() => _busy = false);
  }

  @override
  Widget build(BuildContext context) {
    final cap = widget.cap;
    final keys = ((cap['telemetry_keys'] as List?) ?? []).cast<String>();
    final schema = ((cap['param_schema'] as Map?) ?? {}).cast<String, dynamic>();
    final fire = cap['cap_id'] == 'fire_detect' && (widget.data['fire'] == 1 || widget.data['fire'] == true);

    return Card(
      color: fire ? Colors.red.shade100 : null,
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Text('${cap['icon'] ?? ''} ${cap['display_name']}', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),
          for (final k in keys)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 2),
              child: Row(children: [
                Expanded(child: Text(_labels[k]?.$1 ?? k, style: const TextStyle(color: Colors.grey))),
                Text('${_fmt(widget.data[k])}${_labels[k]?.$2 ?? ''}', style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
              ]),
            ),
          if (cap['mqtt_action'] != null) ...[
            for (final e in schema.entries) _control(e.key, (e.value as Map).cast<String, dynamic>()),
            const SizedBox(height: 8),
            if (schema.length == 1 && (schema.values.first as Map)['type'] == 'bool')
              Row(children: [
                Expanded(child: FilledButton(onPressed: _busy ? null : () => _run({schema.keys.first: true}), child: const Text('켜기 / 열기'))),
                const SizedBox(width: 8),
                Expanded(child: OutlinedButton(onPressed: _busy ? null : () => _run({schema.keys.first: false}), child: const Text('끄기 / 닫기'))),
              ])
            else
              SizedBox(
                width: double.infinity,
                child: FilledButton(onPressed: _busy ? null : _run, child: Text(_busy ? '전송 중…' : '실행')),
              ),
          ],
        ]),
      ),
    );
  }

  String _fmt(dynamic v) => v == null ? '—' : v is bool ? (v ? '예' : '아니오') : '$v';

  Widget _control(String key, Map<String, dynamic> rule) {
    if (rule['type'] == 'int') {
      final min = (rule['min'] as num?)?.toDouble() ?? 0, max = (rule['max'] as num?)?.toDouble() ?? 100;
      final v = ((_params[key] as num?)?.toDouble() ?? min).clamp(min, max);
      return Row(children: [
        SizedBox(width: 90, child: Text(key == 'duration_ms' ? '${(v / 1000).toStringAsFixed(1)}초' : '$key ${v.round()}')),
        Expanded(child: Slider(value: v, min: min, max: max, onChanged: (x) => setState(() => _params[key] = x.round()))),
      ]);
    }
    if (rule['type'] == 'color') {
      const presets = ['#00ff88', '#ff3b30', '#0a84ff', '#ffd60a', '#ffffff'];
      return Wrap(spacing: 8, children: [
        for (final c in presets)
          ChoiceChip(
            label: const SizedBox(width: 16, height: 16),
            avatar: CircleAvatar(backgroundColor: Color(int.parse('FF${c.substring(1)}', radix: 16))),
            selected: _params[key] == c,
            onSelected: (_) => setState(() => _params[key] = c),
          ),
      ]);
    }
    return const SizedBox.shrink(); // bool은 버튼 2개로 처리
  }
}
