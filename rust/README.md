# Aegis SDK for Rust

Official async client for the Aegis Authentication API. Rust 1.75+, Tokio,
`reqwest` with rustls.

## Contents

```
src/lib.rs     Aegis client with every API operation
src/error.rs   AegisError with typed error codes
src/hwid.rs    Stable hardware id
src/types.rs   Request/response types
examples/      Runnable quickstart
```

## Install

No registry needed — unzip and point Cargo at the folder:

```toml
[dependencies]
aegis-sdk = { path = "./aegis-sdk-rust" }
```

## Quickstart

```rust
use aegis_sdk::{Aegis, AegisOptions, Scope};

let client = Aegis::new(AegisOptions::new("https://your-aegis-host", app_key).version("1.0.0"))?;

let info = client.init().await?;
if info.version.as_ref().is_some_and(|v| v.update_required) {
    return Err("update required".into());
}

let auth = client.login("ada", &password).await?;
println!("signed in as {}", auth.user.username);

let check = client.validate_license("AEGS-4K7P-2M9X-QT31").await?;
println!("{} {:?}", check.valid, check.status);

client.set_variable("last_level", "12", Scope::User, None).await?;
let vars = client.get_variables(Scope::User, None).await?;

let beat = client.start_heartbeat(std::time::Duration::from_secs(60), |reason| eprintln!("revoked: {reason}"));
beat.abort();
client.logout().await?;
```

## Supported operations

`init`, `status`, `app_data`, `register`, `login`, `logout`, `heartbeat`,
`check_session`, `is_authenticated`, `use_session`, `user_data`,
`validate_license`, `activate_license`, `get_variables`, `set_variable`,
`check_version`, `trigger_webhook`, plus `request()` for any endpoint added later.

## Error handling

```rust
match client.login(&username, &password).await {
    Ok(auth) => start(auth),
    Err(error) if error.code == "hwid_mismatch" => ui.show("Locked to another machine."),
    Err(error) if error.is_network_error() => ui.show("Aegis is unreachable — retrying."),
    Err(error) => return Err(error.into()),
}
```

## License

MIT — see `LICENSE`.
