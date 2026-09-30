import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import 'exception.dart';
import 'hardware_id.dart';
import 'options.dart';

/// Client for the Aegis Authentication API.
///
/// Works on Flutter (mobile, desktop, web) and plain Dart. Every call returns
/// the endpoint's `data` payload, or throws an [AegisException].
class Aegis {
  static const String sdkVersion = '1.0.0';

  final AegisOptions _options;
  final String _baseUrl;
  final http.Client _http;

  /// Machine identifier sent with authentication and licensing calls.
  late final String hardwareId;

  Timer? _heartbeatTimer;
  String? _sessionToken;

  Aegis(this._options, {http.Client? httpClient})
      : _baseUrl = _options.baseUrl.replaceAll(RegExp(r'/+$'), ''),
        _http = httpClient ?? http.Client() {
    if (_options.baseUrl.isEmpty) throw const AegisException('invalid_options', 'baseUrl is required.');
    if (_options.appKey.isEmpty) throw const AegisException('invalid_options', 'appKey is required.');
    hardwareId = _options.hwid ?? HardwareId.current();
  }

  /// Current session token, or null when signed out.
  String? get sessionToken => _sessionToken;

  /// Restores a session token persisted by the host application.
  void useSession(String? token) => _sessionToken = token;

  /// Calls any endpoint and returns its `data` payload.
  Future<Map<String, dynamic>> request(String endpoint, [Map<String, dynamic>? body]) async {
    final uri = Uri.parse('$_baseUrl/api/public/v1/${endpoint.replaceAll(RegExp(r'^/+|/+$'), '')}');
    final payload = <String, dynamic>{...?body}..removeWhere((_, value) => value == null);

    final headers = <String, String>{
      'content-type': 'application/json',
      'user-agent': 'aegis-dart-sdk/$sdkVersion',
      'x-app-key': _options.appKey,
      if (_options.apiKey != null) 'x-api-key': _options.apiKey!,
      if (_sessionToken != null) 'x-session-token': _sessionToken!,
    };

    Object? lastFailure;
    for (var attempt = 0; attempt <= _options.maxRetries; attempt++) {
      try {
        final response = await _http
            .post(uri, headers: headers, body: jsonEncode(payload))
            .timeout(_options.timeout);

        if (response.statusCode >= 500 && attempt < _options.maxRetries) {
          await Future<void>.delayed(Duration(milliseconds: 250 * (attempt + 1)));
          continue;
        }

        final envelope = response.body.isEmpty
            ? const <String, dynamic>{}
            : jsonDecode(response.body) as Map<String, dynamic>;

        if (envelope['success'] == true) {
          return (envelope['data'] as Map<String, dynamic>?) ?? const <String, dynamic>{};
        }
        final error = (envelope['error'] as Map<String, dynamic>?) ?? const <String, dynamic>{};
        throw AegisException(
          error['code'] as String? ?? 'server_error',
          error['message'] as String? ?? 'Request failed.',
          response.statusCode,
        );
      } on AegisException {
        rethrow;
      } catch (error) {
        lastFailure = error;
        if (attempt < _options.maxRetries) {
          await Future<void>.delayed(Duration(milliseconds: 250 * (attempt + 1)));
        }
      }
    }
    throw AegisException('network_error', lastFailure?.toString() ?? 'Network request failed.');
  }

  // Application ---------------------------------------------------------

  /// Handshake. Call once before any other operation.
  Future<Map<String, dynamic>> init() => request('init', {'version': _options.version});

  Future<Map<String, dynamic>> status() => request('status');

  Future<Map<String, dynamic>> appData() => request('app/data');

  /// Download information published for this application.
  Future<Map<String, dynamic>> downloads() => request('downloads');

  Future<Map<String, dynamic>> checkVersion([String? version]) =>
      request('version/check', {'version': version ?? _options.version, 'channel': _options.channel});

  // Authentication ---------------------------------------------------------

  Future<Map<String, dynamic>> register({
    required String username,
    required String password,
    String? email,
    String? licenseKey,
  }) async {
    final data = await request('register', {
      'username': username,
      'password': password,
      'email': email,
      'license_key': licenseKey,
      'hwid': hardwareId,
    });
    _storeSession(data);
    return data;
  }

  Future<Map<String, dynamic>> login({required String username, required String password}) async {
    final data = await request('login', {'username': username, 'password': password, 'hwid': hardwareId});
    _storeSession(data);
    return data;
  }

  Future<void> logout() async {
    try {
      await request('logout');
    } finally {
      _sessionToken = null;
      stopHeartbeat();
    }
  }

  Future<Map<String, dynamic>> heartbeat() => request('heartbeat');

  Future<Map<String, dynamic>> checkSession() => request('session/check');

  /// True when a token exists and the server still accepts it.
  Future<bool> isAuthenticated() async {
    if (_sessionToken == null) return false;
    try {
      return (await checkSession())['valid'] == true;
    } on AegisException {
      return false;
    }
  }

  Future<Map<String, dynamic>> userData() => request('user/data');

  // Licensing ---------------------------------------------------------

  Future<Map<String, dynamic>> validateLicense(String licenseKey) =>
      request('license/validate', {'license_key': licenseKey, 'hwid': hardwareId});

  Future<Map<String, dynamic>> activateLicense(String licenseKey, {String? username}) =>
      request('license/activate', {'license_key': licenseKey, 'hwid': hardwareId, 'username': username});

  // Variables ---------------------------------------------------------

  Future<Map<String, dynamic>> getVariables({String scope = 'application', String? licenseKey}) =>
      request('variables/get', {'scope': scope, 'license_key': licenseKey});

  Future<Map<String, dynamic>> setVariable(
    String key,
    String value, {
    String scope = 'user',
    String? licenseKey,
  }) =>
      request('variables/set', {'scope': scope, 'key': key, 'value': value, 'license_key': licenseKey});

  Future<Map<String, dynamic>> triggerWebhook(String event, [Map<String, dynamic>? payload]) =>
      request('webhook/trigger', {'event': event, 'payload': payload ?? const <String, dynamic>{}});

  // Sessions ---------------------------------------------------------

  /// Starts a periodic heartbeat; [onRevoked] fires once when the session dies.
  void startHeartbeat({
    Duration interval = const Duration(seconds: 60),
    void Function(AegisException error)? onRevoked,
  }) {
    stopHeartbeat();
    _heartbeatTimer = Timer.periodic(interval, (_) async {
      try {
        await heartbeat();
      } on AegisException catch (error) {
        if (error.isNetworkError) return; // transient — retry next tick
        stopHeartbeat();
        onRevoked?.call(error);
      }
    });
  }

  void stopHeartbeat() {
    _heartbeatTimer?.cancel();
    _heartbeatTimer = null;
  }

  /// Releases the heartbeat timer and underlying HTTP client.
  void close() {
    stopHeartbeat();
    _http.close();
  }

  void _storeSession(Map<String, dynamic> data) {
    final session = data['session'];
    if (session is Map<String, dynamic> && session['token'] is String) {
      _sessionToken = session['token'] as String;
    }
  }
}