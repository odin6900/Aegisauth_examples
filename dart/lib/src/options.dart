/// Client configuration.
class AegisOptions {
  /// Base URL of your Aegis deployment, e.g. `https://your-aegis-host`.
  final String baseUrl;

  /// Public application key from the Aegis dashboard.
  final String appKey;

  /// Optional server-side API key. Never ship this in a distributed client.
  final String? apiKey;

  /// Client version reported to `init` and `version/check`.
  final String version;

  /// Release channel used by version checks.
  final String channel;

  /// Overrides the automatically derived hardware id.
  final String? hwid;

  /// Per-request timeout.
  final Duration timeout;

  /// Retries on transport/5xx failures.
  final int maxRetries;

  const AegisOptions({
    required this.baseUrl,
    required this.appKey,
    this.apiKey,
    this.version = '1.0.0',
    this.channel = 'stable',
    this.hwid,
    this.timeout = const Duration(seconds: 20),
    this.maxRetries = 2,
  });
}