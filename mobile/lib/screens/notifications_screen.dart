import 'package:material_ui/material_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/providers.dart';

class NotificationsScreen extends ConsumerWidget {
  const NotificationsScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final list = ref.watch(notificationsProvider);
    return Scaffold(
      appBar: AppBar(
        title: const Text('알림'),
        actions: [
          TextButton(
            onPressed: () async {
              await ref.read(apiProvider).post('/notifications/read-all');
              ref.invalidate(notificationsProvider);
            },
            child: const Text('모두 읽음'),
          ),
        ],
      ),
      body: list.when(
        loading: () => const Center(child: CircularProgressIndicator()),
        error: (e, _) => Center(child: Text('$e')),
        data: (items) => items.isEmpty
            ? const Center(child: Text('알림이 없습니다.'))
            : ListView.separated(
                itemCount: items.length,
                separatorBuilder: (_, _) => const Divider(height: 1),
                itemBuilder: (_, i) {
                  final n = items[i];
                  final unread = n['is_read'] != true && n['is_read'] != 1;
                  return ListTile(
                    leading: Icon(Icons.notifications, color: unread ? Theme.of(context).colorScheme.primary : Colors.grey),
                    title: Text(n['body'] ?? '', style: TextStyle(fontWeight: unread ? FontWeight.bold : FontWeight.normal)),
                    subtitle: Text('${n['device_id'] ?? n['title']} · ${n['created_at']}'),
                  );
                },
              ),
      ),
    );
  }
}
