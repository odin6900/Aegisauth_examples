/// Every Aegis API failure surfaces as this exception.
class AegisException implements Exception {
  /// Machine-readable error code, e.g. `invalid_credentials`.
  final String code;

  /// Human-readable message safe to surface in UI.
  final String message;

  /// HTTP status, or 0 when the request never reached the API.
  final int status;

  const AegisException(this.code, this.message, [this.status = 0]);

  bool get isNetworkError => status == 0;

  bool get isAuthError => code == 'unauthorized' || code == 'invalid_credentials';

  bool get isLicenseError => code.startsWith('license') || code == 'hwid_mismatch';

  @override
  String toString() => 'Aegis error [$code] $message';
}