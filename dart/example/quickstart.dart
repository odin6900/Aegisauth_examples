// Runnable sample: `dart pub get && dart run example/quickstart.dart`
import 'dart:io';

import 'package:aegis/aegis.dart';

String envOr(String key, String fallback) {
  final value = Platform.environment[key];
  return (value == null || value.isEmpty) ? fallback : value;
}

Future<void> main() async {
  final aegis = Aegis(AegisOptions(
    baseUrl: envOr('AEGIS_BASE_URL', 'http://localhost:8080'),
    appKey: envOr('AEGIS_APP_KEY', ''),
    version: '1.0.0',
  ));

  try {
    final info = await aegis.init();
    stdout.writeln('initialized: ${info['status'] ?? 'ok'}');

    final version = info['version'] as Map<String, dynamic>?;
    if (version?['update_required'] == true) {
      stdout.writeln('mandatory update: ${version?['latest']}');
      return;
    }

    final auth = await aegis.login(
      username: envOr('AEGIS_USERNAME', 'demo'),
      password: envOr('AEGIS_PASSWORD', 'demo-password'),
    );
    stdout.writeln('signed in as ${(auth['user'] as Map?)?['username']}');

    await aegis.setVariable('last_seen', DateTime.now().toUtc().toIso8601String());
    aegis.startHeartbeat(onRevoked: (error) => stdout.writeln('session ended: ${error.message}'));

    stdout.writeln('authenticated: ${await aegis.isAuthenticated()}');
    await aegis.logout();
    stdout.writeln('signed out.');
  } on AegisException catch (error) {
    stderr.writeln(error);
    exitCode = 1;
  } finally {
    aegis.close();
  }
}