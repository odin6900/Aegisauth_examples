use thiserror::Error;

/// Every Aegis API failure surfaces as this error.
#[derive(Debug, Error)]
#[error("aegis: [{code}] {message}")]
pub struct AegisError {
    pub code: String,
    pub message: String,
    pub status: u16,
}

impl AegisError {
    pub fn new(code: impl Into<String>, message: impl Into<String>, status: u16) -> Self {
        Self { code: code.into(), message: message.into(), status }
    }

    /// Transport-level failure (no HTTP status).
    pub fn is_network_error(&self) -> bool {
        self.status == 0
    }

    /// Rejected credentials or session.
    pub fn is_auth_error(&self) -> bool {
        self.code == "unauthorized" || self.code == "invalid_credentials"
    }

    /// Licensing or hardware-binding failure.
    pub fn is_license_error(&self) -> bool {
        self.code.starts_with("license") || self.code == "hwid_mismatch"
    }
}

pub type Result<T> = std::result::Result<T, AegisError>;