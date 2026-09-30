# Changelog

## 1.0.0 — 2026-08-07

- First real release, distributed directly from the Aegis download centre.
- Full Authentication API coverage on `java.net.http`, no third-party dependencies.
- `AegisException` with API error codes and network/auth/license predicates.
- Daemon heartbeat scheduler with revocation callback; client is `AutoCloseable`.
- Stable hardware id derived from machine facts.
