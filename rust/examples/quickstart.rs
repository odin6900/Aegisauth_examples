//! Runnable quickstart for the Aegis Rust SDK.
//!
//! AEGIS_BASE_URL=https://your-aegis-host AEGIS_APP_KEY=pk_live_... cargo run --example quickstart
use aegis_sdk::{Aegis, AegisOptions, Scope};
use std::time::Duration;

#[tokio::main]
async fn main() {
    let base_url = std::env::var("AEGIS_BASE_URL").unwrap_or_else(|_| "http://localhost:8080".into());
    let app_key = std::env::var("AEGIS_APP_KEY").unwrap_or_default();

    let client = match Aegis::new(AegisOptions::new(base_url, app_key).version("1.0.0")) {
        Ok(client) => client,
        Err(error) => return eprintln!("{error}"),
    };

    let info = match client.init().await {
        Ok(info) => info,
        Err(error) => return eprintln!("aegis error [{}] {}", error.code, error.message),
    };
    println!("initialized: {:?}", info.status);

    if info.version.as_ref().map(|v| v.update_required).unwrap_or(false) {
        println!("mandatory update available");
        return;
    }

    let username = std::env::var("AEGIS_USERNAME").unwrap_or_else(|_| "demo".into());
    let password = std::env::var("AEGIS_PASSWORD").unwrap_or_else(|_| "demo-password".into());

    match client.login(&username, &password).await {
        Ok(auth) => println!("signed in as {}", auth.user.username),
        Err(error) => return eprintln!("aegis error [{}] {}", error.code, error.message),
    }

    if let Ok(vars) = client.get_variables(Scope::User, None).await {
        println!("user variables: {:?}", vars.variables);
    }
    let _ = client.set_variable("last_seen", "now", Scope::User, None).await;

    let beat = client.start_heartbeat(Duration::from_secs(60), |reason| {
        eprintln!("session revoked: {reason}");
    });
    tokio::time::sleep(Duration::from_secs(2)).await;
    beat.abort();

    let _ = client.logout().await;
    println!("signed out.");
}