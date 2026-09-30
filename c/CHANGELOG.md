# Changelog

## 1.0.0 — 2026-08-07

- First real release, distributed directly from the Aegis download centre.
- Full Authentication API coverage on libcurl, C99, no other dependencies.
- `aegis_response` result type carrying API error codes and HTTP status.
- Automatic retry with backoff on transport failures and 5xx responses.
- Stable hardware id derived from machine facts.
