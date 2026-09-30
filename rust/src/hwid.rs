use sha2::{Digest, Sha256};
use std::sync::OnceLock;

static HWID: OnceLock<String> = OnceLock::new();

/// Stable, non-reversible machine identifier.
pub fn hardware_id() -> String {
    HWID.get_or_init(|| {
        let host = std::env::var("HOSTNAME")
            .or_else(|_| std::env::var("COMPUTERNAME"))
            .unwrap_or_default();
        let user = std::env::var("USER")
            .or_else(|_| std::env::var("USERNAME"))
            .unwrap_or_default();
        let seed = format!(
            "{host}|{}|{}|{user}",
            std::env::consts::OS,
            std::env::consts::ARCH
        );
        hex::encode(Sha256::digest(seed.as_bytes()))
    })
    .clone()
}