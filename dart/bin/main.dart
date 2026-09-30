import 'dart:convert';
import 'dart:io';
import 'package:crypto/crypto.dart';

// Configuration Placeholders
const String authUrl = "https://auth.example.com";
const String appKey = "YOUR_APP_KEY";
const String appName = "My Application";
const String appVersion = "1.0.0";

const String username = "YOUR_USERNAME";
const String password = "YOUR_PASSWORD";
const String moduleName = "minecraft";

String getHardwareId() {
  final seed = "${Platform.localHostname}-${Platform.operatingSystem}-${Platform.numberOfProcessors}";
  return sha256.convert(utf8.encode(seed)).toString();
}

class AegisAuthClient {
  final String baseUrl;
  final String hwid;
  final HttpClient httpClient = HttpClient();
  String? sessionToken;

  AegisAuthClient()
      : baseUrl = "${authUrl.replaceAll(RegExp(r'/+$'), '')}/api/public/v1/",
        hwid = getHardwareId();

  Future<Map<String, dynamic>> post(String endpoint, Map<String, dynamic> body) async {
    try {
      final uri = Uri.parse("$baseUrl$endpoint");
      final request = await httpClient.postUrl(uri);
      request.headers.set('content-type', 'application/json');
      request.headers.set('x-app-key', appKey);
      request.headers.set('x-timestamp', (DateTime.now().millisecondsSinceEpoch ~/ 1000).toString());
      request.headers.set('user-agent', 'AegisAuth-Dart/$appVersion');

      if (sessionToken != null) {
        request.headers.set('x-session-token', sessionToken!);
      }

      request.add(utf8.encode(jsonEncode(body)));
      final response = await request.close();
      final responseBody = await response.transform(utf8.decoder).join();
      return jsonDecode(responseBody) as Map<String, dynamic>;
    } catch (e) {
      return {'success': false, 'error': {'message': e.toString()}};
    }
  }

  Future<bool> init() async {
    print("[*] Initializing $appName (v$appVersion)...");
    print("    HWID: $hwid");

    final res = await post("init", {"version": appVersion});
    if (res['success'] != true) {
      print("[-] Init failed: ${res['error']?['message']}");
      return false;
    }

    print("[+] Handshake successful.");
    return true;
  }

  Future<bool> login(String user, String pass) async {
    print("\n[*] Logging in as '$user'...");

    final res = await post("login", {
      "username": user,
      "password": pass,
      "hwid": hwid,
    });

    if (res['success'] != true) {
      print("[-] Login failed: ${res['error']?['message']}");
      return false;
    }

    sessionToken = res['data']?['token'];
    final preview = (sessionToken != null && sessionToken!.length > 12)
        ? "${sessionToken!.substring(0, 12)}..."
        : sessionToken;
    print("[+] Login successful! Session token: $preview");
    return true;
  }

  Future<String?> requestModuleToken(String module) async {
    print("\n[*] Requesting signed download token for module '$module'...");

    final res = await post("module/token", {"module": module});
    if (res['success'] != true) {
      print("[-] Module token request failed: ${res['error']?['message']}");
      return null;
    }

    final data = res['data'] as Map<String, dynamic>;
    print("[+] Signed Token Received!");
    print("    Target Asset: ${data['key']}");
    print("    Download URL: ${data['download_url']}");
    print("    Expires In  : ${data['expires_in_seconds']}s");
    return data['download_url'] as String?;
  }

  Future<void> logout() async {
    if (sessionToken != null) {
      print("\n[*] Logging out...");
      await post("logout", {});
      sessionToken = null;
      print("[+] Logged out cleanly.");
    }
    httpClient.close();
  }
}

Future<void> main() async {
  final client = AegisAuthClient();

  if (!await client.init()) return;
  if (!await client.login(username, password)) return;

  final downloadUrl = await client.requestModuleToken(moduleName);
  if (downloadUrl != null) {
    print("\n[+] Binary payload authorized for AetherVault silent streaming!");
  }

  await client.logout();
}
