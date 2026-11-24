import 'package:flutter_test/flutter_test.dart';
import 'package:sca_mobile/api/device_transport.dart';

void main() {
  test('CommandRoute values', () {
    expect(CommandRoute.values, [CommandRoute.local, CommandRoute.cloud]);
  });
}
