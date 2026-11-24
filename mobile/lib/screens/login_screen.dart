import 'package:material_ui/material_ui.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../providers/providers.dart';

class LoginScreen extends ConsumerStatefulWidget {
  const LoginScreen({super.key});

  @override
  ConsumerState<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends ConsumerState<LoginScreen> {
  final _email = TextEditingController();
  final _pw = TextEditingController();
  String? _error;
  bool _busy = false;

  Future<void> _submit() async {
    setState(() {
      _busy = true;
      _error = null;
    });
    try {
      await ref.read(authProvider.notifier).login(_email.text.trim(), _pw.text);
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(24),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text('🚭', textAlign: TextAlign.center, style: TextStyle(fontSize: 48)),
                const SizedBox(height: 8),
                Text('Smart Clean Ashtray', textAlign: TextAlign.center, style: Theme.of(context).textTheme.headlineSmall),
                const SizedBox(height: 32),
                TextField(controller: _email, keyboardType: TextInputType.emailAddress,
                    autofillHints: const [AutofillHints.email], decoration: const InputDecoration(labelText: '이메일', border: OutlineInputBorder())),
                const SizedBox(height: 12),
                TextField(controller: _pw, obscureText: true, onSubmitted: (_) => _submit(),
                    decoration: const InputDecoration(labelText: '비밀번호', border: OutlineInputBorder())),
                if (_error != null) ...[
                  const SizedBox(height: 12),
                  Text(_error!, style: TextStyle(color: Theme.of(context).colorScheme.error)),
                ],
                const SizedBox(height: 20),
                FilledButton(onPressed: _busy ? null : _submit, child: Text(_busy ? '로그인 중…' : '로그인')),
                const SizedBox(height: 12),
                const Text('회원가입·비밀번호 찾기는 www.smart-clean-ashtray.com 에서 진행해 주세요.',
                    textAlign: TextAlign.center, style: TextStyle(fontSize: 12, color: Colors.grey)),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
