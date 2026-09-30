# Changelog — AegisAuth C# SDK

All notable changes to this SDK are documented here. The project follows
[Semantic Versioning](https://semver.org).

## [1.0.0] - 2026-01-15

### Added

- Single-file, zero-dependency `AegisAuth.cs` drop-in (namespace `AegisAuth`, class `api`).
- Full authentication flow: `init`, `register`, `login`, `license` (key-only), `upgrade`, `logout`.
- Session helpers: `check`, `use_session`, `expirydaysleft`, per-user `getvar`/`setvar`.
- Application services: `var` (application variables), `log` (client audit log), `fetchstats`, `update_available`.
- `user_data` / `app_data` / `response` data containers with subscription expiry helpers.
- Hashed hardware id (SHA-256 over the machine SID on Windows, machine profile elsewhere).
- `examples/Quickstart` console application.
