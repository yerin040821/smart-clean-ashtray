import 'package:fl_chart/fl_chart.dart';
import 'package:material_ui/material_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/providers.dart';

class HistoryScreen extends ConsumerStatefulWidget {
  const HistoryScreen({super.key, required this.deviceId});
  final String deviceId;

  @override
  ConsumerState<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends ConsumerState<HistoryScreen> {
  int _hours = 24;
  static const _series = {
    'gas_diff': ('공기질 (가스 변화량)', Colors.indigo),
    'ash_level': ('재 잔량 (%)', Colors.amber),
    'water_level': ('수위 (%)', Colors.lightBlue),
    'temperature': ('온도 (°C)', Colors.red),
    'humidity': ('습도 (%)', Colors.teal),
  };

  @override
  Widget build(BuildContext context) {
    final rows = ref.watch(historyProvider((widget.deviceId, _hours)));
    return Scaffold(
      appBar: AppBar(title: const Text('센서 히스토리')),
      body: Column(children: [
        Padding(
          padding: const EdgeInsets.all(12),
          child: SegmentedButton<int>(
            segments: const [
              ButtonSegment(value: 1, label: Text('1시간')),
              ButtonSegment(value: 24, label: Text('24시간')),
              ButtonSegment(value: 168, label: Text('7일')),
            ],
            selected: {_hours},
            onSelectionChanged: (s) => setState(() => _hours = s.first),
          ),
        ),
        Expanded(
          child: rows.when(
            loading: () => const Center(child: CircularProgressIndicator()),
            error: (e, _) => Center(child: Text('$e')),
            data: (list) {
              final charts = _series.entries.where((s) => list.any((r) => r[s.key] is num)).toList();
              if (charts.isEmpty) return const Center(child: Text('데이터가 없습니다.'));
              final t0 = DateTime.parse(list.first['t']).millisecondsSinceEpoch;
              return ListView(
                padding: const EdgeInsets.all(12),
                children: [
                  for (final s in charts)
                    Card(
                      child: Padding(
                        padding: const EdgeInsets.fromLTRB(8, 16, 16, 8),
                        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                          Padding(padding: const EdgeInsets.only(left: 8), child: Text(s.value.$1, style: const TextStyle(fontWeight: FontWeight.bold))),
                          const SizedBox(height: 12),
                          SizedBox(
                            height: 180,
                            child: LineChart(LineChartData(
                              titlesData: const FlTitlesData(
                                topTitles: AxisTitles(), rightTitles: AxisTitles(), bottomTitles: AxisTitles()),
                              gridData: const FlGridData(drawVerticalLine: false),
                              borderData: FlBorderData(show: false),
                              lineBarsData: [
                                LineChartBarData(
                                  spots: [
                                    for (final r in list)
                                      if (r[s.key] is num)
                                        FlSpot((DateTime.parse(r['t']).millisecondsSinceEpoch - t0) / 60000, (r[s.key] as num).toDouble()),
                                  ],
                                  color: s.value.$2,
                                  dotData: const FlDotData(show: false),
                                  barWidth: 2,
                                  isCurved: true,
                                ),
                              ],
                            )),
                          ),
                        ]),
                      ),
                    ),
                ],
              );
            },
          ),
        ),
      ]),
    );
  }
}
