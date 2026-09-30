# Changelog

## 1.0.0 — 2026-08-07

- First real release, distributed directly from the Aegis download centre.
- Full Authentication API coverage with typed responses.
- `AegisError` with API error codes and network/auth/license predicates.
- Heartbeat loop with revocation callback and automatic teardown on logout.
- Isomorphic hardware id (OS-derived on Node, persisted random in browsers).
