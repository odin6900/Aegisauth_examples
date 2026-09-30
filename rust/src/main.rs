use sha2::{Digest, Sha256};
use std::time::{SystemTime, UNIX_EPOCH};

const AUTH_URL: &str = "https://auth.example.com";
const APP_KEY: &str = "YOUR_APP_KEY";
const APP_NAME: &str = "My Application";
const APP_VERSION: &str = "1.0.0";

const USERNAME: &str = "YOUR_USERNAME";
const PASSWORD: &str = "YOUR_PASSWORD";
const MODULE_NAME: &str = "minecraft";

fn get_hardware_id() -> String {
    let hostname = hostname::get()
        .map(|h| h.to_string_lossy().to_string())
        .unwrap_or_else(|_| "rust-client".to_string());
    let seed = format!("{}-{}", hostname, std::env::consts::OS);
    let mut hasher = Sha256::new();
    hasher.update(seed.as_bytes());
    hex::encode(hasher.finalize())
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("=== AegisAuth Rust Client [{} v{}] ===", APP_NAME, APP_VERSION);
    let hwid = get_hardware_id();
    println!("[*] Machine HWID: {}", hwid);

    let client = reqwest::Client::new();
    let base_url = format!("{}/api/public/v1/", AUTH_URL.trim_end_matches('/'));

    // 1. Handshake & Version check
    println!("\n[*] Initializing application...");
    let timestamp = SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs().to_string();

    let init_res: serde_json::Value = client
        .post(format!("{}init", base_url))
        .header("Content-Type", "application/json")
        .header("x-app-key", APP_KEY)
        .header("x-timestamp", &timestamp)
        .header("User-Agent", format!("AegisAuth-Rust/{}", APP_VERSION))
        .json(&serde_json::json!({ "version": APP_VERSION }))
        .send()
        .await?
        .json()
        .await?;

    if !init_res.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
        let msg = init_res["error"]["message"].as_str().unwrap_or("Init failed");
        eprintln!("[-] Init error: {}", msg);
        return Ok(());
    }
    println!("[+] Handshake successful.");

    // 2. Login
    println!("\n[*] Logging in as '{}'...", USERNAME);
    let login_res: serde_json::Value = client
        .post(format!("{}login", base_url))
        .header("Content-Type", "application/json")
        .header("x-app-key", APP_KEY)
        .header("x-timestamp", &timestamp)
        .header("User-Agent", format!("AegisAuth-Rust/{}", APP_VERSION))
        .json(&serde_json::json!({
            "username": USERNAME,
            "password": PASSWORD,
            "hwid": hwid
        }))
        .send()
        .await?
        .json()
        .await?;

    if !login_res.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
        let msg = login_res["error"]["message"].as_str().unwrap_or("Login failed");
        eprintln!("[-] Login error: {}", msg);
        return Ok(());
    }

    let session_token = login_res["data"]["token"].as_str().unwrap_or("");
    let license_status = login_res["data"]["license"]["status"].as_str().unwrap_or("active");
    println!("[+] Login successful! License: {}", license_status);

    // 3. Request signed module token for AetherVault
    println!("\n[*] Requesting signed download token for module '{}'...", MODULE_NAME);
    let token_res: serde_json::Value = client
        .post(format!("{}module/token", base_url))
        .header("Content-Type", "application/json")
        .header("x-app-key", APP_KEY)
        .header("x-timestamp", &timestamp)
        .header("x-session-token", session_token)
        .header("User-Agent", format!("AegisAuth-Rust/{}", APP_VERSION))
        .json(&serde_json::json!({ "module": MODULE_NAME }))
        .send()
        .await?
        .json()
        .await?;

    if !token_res.get("success").and_then(|v| v.as_bool()).unwrap_or(false) {
        let msg = token_res["error"]["message"].as_str().unwrap_or("Failed");
        eprintln!("[-] Module token request error: {}", msg);
        return Ok(());
    }

    let download_url = token_res["data"]["download_url"].as_str().unwrap_or("");
    let asset_key = token_res["data"]["key"].as_str().unwrap_or("");
    let expires_in = token_res["data"]["expires_in_seconds"].as_i64().unwrap_or(0);

    println!("[+] Signed Token Received!");
    println!("    Target Asset: {}", asset_key);
    println!("    Download URL: {}", download_url);
    println!("    Expires In  : {}s", expires_in);
    println!("\n[+] Binary payload authorized for AetherVault silent streaming!");

    // 4. Logout
    println!("\n[*] Logging out...");
    let _ = client
        .post(format!("{}logout", base_url))
        .header("Content-Type", "application/json")
        .header("x-app-key", APP_KEY)
        .header("x-session-token", session_token)
        .send()
        .await;
    println!("[+] Logged out cleanly.");

    Ok(())
}
