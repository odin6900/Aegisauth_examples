//! Official Rust client for the Aegis Authentication API.

mod error;
mod hwid;
mod types;

pub use error::{AegisError, Result};
pub use hwid::hardware_id;
pub use types::*;

use serde::de::DeserializeOwned;
use serde_json::{json, Value};
use std::sync::{Arc, Mutex};
use std::time::Duration;

/// Client configuration.
#[derive(Debug, Clone)]
pub struct AegisOptions {
    pub base_url: String,
    pub app_key: String,
    pub api_key: Option<String>,
    pub version: String,
    pub channel: String,
    pub hwid: Option<String>,
    pub timeout: Duration,
    pub max_retries: u8,
}

impl AegisOptions {
    pub fn new(base_url: impl Into<String>, app_key: impl Into<String>) -> Self {
        Self {
            base_url: base_url.into(),
            app_key: app_key.into(),
            api_key: None,
            version: "1.0.0".into(),
            channel: "stable".into(),
            hwid: None,
            timeout: Duration::from_secs(20),
            max_retries: 2,
        }
    }

    pub fn version(mut self, version: impl Into<String>) -> Self {
        self.version = version.into();
        self
    }

    pub fn api_key(mut self, api_key: impl Into<String>) -> Self {
        self.api_key = Some(api_key.into());
        self
    }
}

/// Client for the Aegis Authentication API.
#[derive(Clone)]
pub struct Aegis {
    base_url: String,
    app_key: String,
    api_key: Option<String>,
    version: String,
    channel: String,
    hwid: String,
    max_retries: u8,
    http: reqwest::Client,
    session: Arc<Mutex<Option<String>>>,
}

impl Aegis {
    /// Creates a client. `base_url` and `app_key` are required.
    pub fn new(options: AegisOptions) -> Result<Self> {
        if options.base_url.is_empty() {
            return Err(AegisError::new("invalid_options", "base_url is required.", 0));
        }
        if options.app_key.is_empty() {
            return Err(AegisError::new("invalid_options", "app_key is required.", 0));
        }
        let http = reqwest::Client::builder()
            .timeout(options.timeout)
            .build()
            .map_err(|e| AegisError::new("invalid_options", e.to_string(), 0))?;

        Ok(Self {
            base_url: options.base_url.trim_end_matches('/').to_string(),
            app_key: options.app_key,
            api_key: options.api_key,
            version: options.version,
            channel: options.channel,
            hwid: options.hwid.unwrap_or_else(hardware_id),
            max_retries: options.max_retries,
            http,
            session: Arc::new(Mutex::new(None)),
        })
    }

    /// The hardware id sent with authentication calls.
    pub fn hwid(&self) -> &str {
        &self.hwid
    }

    /// Current session token, if any.
    pub fn session_token(&self) -> Option<String> {
        self.session.lock().unwrap().clone()
    }

    /// Restores a session token persisted by the host application.
    pub fn use_session(&self, token: Option<String>) {
        *self.session.lock().unwrap() = token;
    }

    /// Calls any endpoint and deserializes its `data` payload.
    pub async fn request<T: DeserializeOwned>(&self, endpoint: &str, body: Value) -> Result<T> {
        let url = format!("{}/api/public/v1/{}", self.base_url, endpoint.trim_matches('/'));
        let mut last_error: Option<String> = None;

        for attempt in 0..=self.max_retries {
            let mut request = self
                .http
                .post(&url)
                .header("content-type", "application/json")
                .header("x-app-key", &self.app_key)
                .header("user-agent", "aegis-rust-sdk/1.0.0");
            if let Some(api_key) = &self.api_key {
                request = request.header("x-api-key", api_key);
            }
            if let Some(token) = self.session_token() {
                request = request.header("x-session-token", token);
            }

            let response = match request.json(&body).send().await {
                Ok(response) => response,
                Err(error) => {
                    last_error = Some(error.to_string());
                    if attempt < self.max_retries {
                        tokio::time::sleep(Duration::from_millis(250 * (attempt as u64 + 1))).await;
                        continue;
                    }
                    break;
                }
            };

            let status = response.status().as_u16();
            let text = response.text().await.unwrap_or_default();

            if status >= 500 && attempt < self.max_retries {
                tokio::time::sleep(Duration::from_millis(250 * (attempt as u64 + 1))).await;
                continue;
            }

            let envelope: Value = serde_json::from_str(if text.is_empty() { "{}" } else { &text })
                .map_err(|e| AegisError::new("invalid_response", e.to_string(), status))?;

            if !envelope.get("success").and_then(Value::as_bool).unwrap_or(false) {
                let error = envelope.get("error");
                return Err(AegisError::new(
                    error.and_then(|e| e.get("code")).and_then(Value::as_str).unwrap_or("server_error"),
                    error.and_then(|e| e.get("message")).and_then(Value::as_str).unwrap_or("Request failed."),
                    status,
                ));
            }

            let data = envelope.get("data").cloned().unwrap_or_else(|| json!({}));
            return serde_json::from_value(data)
                .map_err(|e| AegisError::new("invalid_response", e.to_string(), status));
        }

        Err(AegisError::new(
            "network_error",
            last_error.unwrap_or_else(|| "Network request failed.".into()),
            0,
        ))
    }

    // ---------------- application ----------------

    /// Handshake. Call once before any other operation.
    pub async fn init(&self) -> Result<InitResult> {
        self.request("init", json!({ "version": self.version })).await
    }

    pub async fn status(&self) -> Result<Value> {
        self.request("status", json!({})).await
    }

    pub async fn app_data(&self) -> Result<Value> {
        self.request("app/data", json!({})).await
    }

    pub async fn check_version(&self, version: Option<&str>) -> Result<VersionInfo> {
        self.request(
            "version/check",
            json!({ "version": version.unwrap_or(&self.version), "channel": self.channel }),
        )
        .await
    }

    // ---------------- authentication ----------------

    pub async fn register(
        &self,
        username: &str,
        password: &str,
        email: Option<&str>,
        license_key: Option<&str>,
    ) -> Result<AuthResult> {
        let result: AuthResult = self
            .request(
                "register",
                json!({
                    "username": username,
                    "password": password,
                    "email": email,
                    "license_key": license_key,
                    "hwid": self.hwid,
                }),
            )
            .await?;
        self.store_session(&result);
        Ok(result)
    }

    pub async fn login(&self, username: &str, password: &str) -> Result<AuthResult> {
        let result: AuthResult = self
            .request(
                "login",
                json!({ "username": username, "password": password, "hwid": self.hwid }),
            )
            .await?;
        self.store_session(&result);
        Ok(result)
    }

    pub async fn logout(&self) -> Result<()> {
        let outcome: Result<Value> = self.request("logout", json!({})).await;
        self.use_session(None);
        outcome.map(|_| ())
    }

    pub async fn heartbeat(&self) -> Result<SessionCheck> {
        self.request("heartbeat", json!({})).await
    }

    pub async fn check_session(&self) -> Result<SessionCheck> {
        self.request("session/check", json!({})).await
    }

    /// True when a token exists and the server still accepts it.
    pub async fn is_authenticated(&self) -> bool {
        if self.session_token().is_none() {
            return false;
        }
        matches!(self.check_session().await, Ok(check) if check.valid)
    }

    pub async fn user_data(&self) -> Result<Value> {
        self.request("user/data", json!({})).await
    }

    // ---------------- licensing ----------------

    pub async fn validate_license(&self, license_key: &str) -> Result<LicenseResult> {
        self.request(
            "license/validate",
            json!({ "license_key": license_key, "hwid": self.hwid }),
        )
        .await
    }

    pub async fn activate_license(&self, license_key: &str, username: Option<&str>) -> Result<LicenseResult> {
        self.request(
            "license/activate",
            json!({ "license_key": license_key, "hwid": self.hwid, "username": username }),
        )
        .await
    }

    // ---------------- variables ----------------

    pub async fn get_variables(&self, scope: Scope, license_key: Option<&str>) -> Result<Variables> {
        self.request(
            "variables/get",
            json!({ "scope": scope.as_str(), "license_key": license_key }),
        )
        .await
    }

    pub async fn set_variable(
        &self,
        key: &str,
        value: &str,
        scope: Scope,
        license_key: Option<&str>,
    ) -> Result<()> {
        let _: Value = self
            .request(
                "variables/set",
                json!({ "scope": scope.as_str(), "key": key, "value": value, "license_key": license_key }),
            )
            .await?;
        Ok(())
    }

    pub async fn trigger_webhook(&self, event: &str, payload: Value) -> Result<()> {
        let _: Value = self
            .request("webhook/trigger", json!({ "event": event, "payload": payload }))
            .await?;
        Ok(())
    }

    // ---------------- heartbeat loop ----------------

    /// Spawns a heartbeat loop. Abort the returned handle to stop it.
    pub fn start_heartbeat<F>(&self, interval: Duration, on_revoked: F) -> tokio::task::JoinHandle<()>
    where
        F: Fn(String) + Send + 'static,
    {
        let client = self.clone();
        tokio::spawn(async move {
            loop {
                tokio::time::sleep(interval).await;
                match client.heartbeat().await {
                    Ok(_) => {}
                    Err(error) if error.is_network_error() => continue,
                    Err(error) => {
                        client.use_session(None);
                        on_revoked(error.message);
                        return;
                    }
                }
            }
        })
    }

    fn store_session(&self, result: &AuthResult) {
        if let Some(session) = &result.session {
            if !session.token.is_empty() {
                self.use_session(Some(session.token.clone()));
            }
        }
    }
}