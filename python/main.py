"""
AegisAuth Python Example
Uses the official aegis_auth.py SDK in this repository.
Demonstrates initialization, user login, license checks, subscription checks, and requesting a signed AetherVault module token.
"""

from aegis_auth import AegisAuth

# Configuration Placeholders
AUTH_URL = "https://auth.example.com"
APP_KEY = "YOUR_APP_KEY"
APP_NAME = "My Application"
APP_VERSION = "1.0.0"

USERNAME = "YOUR_USERNAME"
PASSWORD = "YOUR_PASSWORD"
MODULE_NAME = "minecraft"

# Initialize SDK client instance
aegis = AegisAuth(
    name=APP_NAME,
    ownerid=APP_KEY,
    secret="",         # Optional API secret (leave empty for client-side distribution)
    version=APP_VERSION,
    url=AUTH_URL,
)


def main():
    print(f"=== AegisAuth Python Client [{aegis.name} v{aegis.version}] ===")

    # 1. Handshake & Version check
    aegis.init()
    if not aegis.response.success:
        print(f"[-] Init failed: {aegis.response.message}")
        return

    print(f"[+] Initialized successfully.")
    print(f"    HWID: {aegis.hwid}")
    print(f"    Server Time: {aegis.app_data.server_time}")

    # 2. Authenticate
    print(f"\n[*] Logging in as '{USERNAME}'...")
    aegis.login(USERNAME, PASSWORD)
    if not aegis.response.success:
        print(f"[-] Login failed: {aegis.response.message}")
        return

    print(f"[+] Logged in successfully!")
    print(f"    Session Token: {aegis.sessionid[:12]}...")
    print(f"    Days left on license: {aegis.expirydaysleft()}")

    # 3. Check Subscription Tier
    if aegis.has_subscription("VIP"):
        print("    [Subscription] VIP tier active — Premium features unlocked!")
    else:
        print("    [Subscription] Standard tier active.")

    # 4. Request signed module download token for AetherVault
    print(f"\n[*] Requesting signed download token for module '{MODULE_NAME}'...")
    token_data = aegis.request_module_token(module=MODULE_NAME)
    if token_data:
        print("[+] Signed Token Received!")
        print(f"    Target Asset: {token_data.get('key')}")
        print(f"    Download URL: {token_data.get('download_url')}")
        print(f"    Expires In  : {token_data.get('expires_in_seconds')}s")
        print("\n[+] Binary payload authorized for AetherVault silent streaming!")

    # 5. Clean up session
    print("\n[*] Logging out...")
    aegis.logout()
    print("[+] Logged out cleanly.")


if __name__ == "__main__":
    main()
