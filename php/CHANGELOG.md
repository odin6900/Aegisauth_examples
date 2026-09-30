# Changelog

## 1.0.0 — 2026-08-07

- First real release, distributed directly from the Aegis download centre.
- Full Authentication API coverage over cURL, no third-party packages.
- `AegisException` with API error codes and network/auth/license predicates.
- Automatic retry with backoff on network failures and 5xx responses.
- Stable hardware id derived from machine facts.
