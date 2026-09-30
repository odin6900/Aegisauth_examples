import 'dart:convert';
import 'dart:io' show Platform;

import 'package:crypto/crypto.dart';

/// Stable, non-reversible machine identifier.
///
/// Derived from OS, architecture, hostname and user facts, then hashed with
/// SHA-256 so no raw machine detail ever leaves the device.
class HardwareId {
  const HardwareId._();

  static String? _cached;

  static String current() {
    final cached = _cached;
    if (cached != null) return cached;

    final facts = <String>[
      Platform.operatingSystem,
      Platform.operatingSystemVersion,
      Platform.localHostname,
      Platform.numberOfProcessors.toString(),
      Platform.environment['USER'] ?? Platform.environment['USERNAME'] ?? '',
    ].join('|');

    return _cached = sha256.convert(utf8.encode(facts)).toString();
  }
}