"""
AegisAuth Python Example
Demonstrates initialization, user login, license validation, and requesting a signed AetherVault module token.
Zero external dependencies (uses standard library urllib and hashlib).
"""

import hashlib
import json
import platform
import subprocess
import time
import urllib.error
import urllib.request

# Configuration Placeholders
AUTH_URL = "https://auth.example.com"
APP_KEY = "YOUR_APP_KEY"
APP_NAME = "My Application"
APP_VERSION = "1.0.0"

USERNAME = "YOUR_USERNAME"
PASSWORD = "YOUR_PASSWORD"
MODULE_NAME = "minecraft"  # Game module or asset key to fetch token for


def get_hardware_id() -> str:
    """Generate a stable per-machine identifier (SHA-256 hashed)."""
    system = platform.system()
    seed = ""
    try:
        if system == "Windows":
            out = subprocess.check_output("wmic csproduct get uuid", shell=True, timeout=5)
            lines = [line.strip() for line in out.decode(errors="ignore").splitlines() if line.strip()]
            if len(lines) > 1:
                seed = lines[1]
        elif system == "Darwin":
            out = subprocess.check_output(
                "ioreg -rd1 -c IOPlatformExpertDevice | grep IOPlatformUUID", shell=True, timeout=5
            )
            seed = out.decode(errors="ignore").split('"')[-2]
        else:
            with open("/etc/machine-id", "r", encoding="utf-8") as handle:
                seed = handle.read().strip()
    except Exception:
        seed = f"{platform.node()}-{platform.machine()}"

    if not seed:
        seed = "fallback-machine-id"

    return hashlib.sha256(seed.encode("utf-8")).hexdigest()


class AegisAuthClient:
    def __init__(self, auth_url: str, app_key: str, app_name: str, app_version: str):
        self.base_url = auth_url.rstrip("/") + "/api/public/v1/"
        self.app_key = app_key
        self.app_name = app_name
        self.app_version = app_version
        self.hwid = get_hardware_id()
        self.session_token = None

    def _post(self, endpoint: str, payload: dict) -> dict:
        url = self.base_url + endpoint
        headers = {
            "Content-Type": "application/json",
            "x-app-key": self.app_key,
            "x-timestamp": str(int(time.time())),
            "User-Agent": f"AegisAuth-Client/{self.app_version}",
        }
        if self.session_token:
            headers["x-session-token"] = self.session_token

        data = json.dumps(payload).encode("utf-8")
        req = urllib.request.Request(url, data=data, headers=headers, method="POST")

        try:
            with urllib.request.urlopen(req, timeout=15) as res:
                body = res.read().decode("utf-8")
                return json.loads(body)
        except urllib.error.HTTPError as e:
            err_body = e.read().decode("utf-8")
            try:
                return json.loads(err_body)
            except Exception:
                return {"success": False, "error": {"message": f"HTTP {e.code}"}}
        except Exception as e:
            return {"success": False, "error": {"message": str(e)}}

    def init(self) -> bool:
        print(f"[*] Initializing {self.app_name} (v{self.app_version})...")
        print(f"    HWID: {self.hwid}")
        res = self._post("init", {"version": self.app_version})
        if not res.get("success"):
            print(f"[-] Init failed: {res.get('error', {}).get('message', 'Unknown error')}")
            return False

        data = res.get("data", {})
        version_info = data.get("version", {})
        if version_info.get("update_required"):
            print(f"[!] Update required! Current latest: {version_info.get('current')}")
            return False

        print(f"[+] Initialized successfully. Server time: {data.get('server_time')}")
        return True

    def login(self, username: str, password: str) -> bool:
        print(f"\n[*] Logging in as '{username}'...")
        res = self._post("login", {
            "username": username,
            "password": password,
            "hwid": self.hwid,
        })
        if not res.get("success"):
            print(f"[-] Login failed: {res.get('error', {}).get('message', 'Unknown error')}")
            return False

        data = res.get("data", {})
        self.session_token = data.get("token")
        user = data.get("user", {})
        license_info = data.get("license", {})

        print(f"[+] Login successful! Session Token: {self.session_token[:12]}...")
        print(f"    User: {user.get('username')}")
        print(f"    License Status: {license_info.get('status', 'active')}")
        print(f"    Expires: {license_info.get('expires_at') or 'Lifetime'}")

        subs = license_info.get("subscriptions", [])
        if subs:
            print("    Active Subscriptions:")
            for sub in subs:
                print(f"      - {sub.get('name')} (Expires: {sub.get('expires_at') or 'Lifetime'})")

        return True

    def request_module_token(self, module_name: str) -> dict | None:
        """Request an ephemeral signed download token for AetherVault."""
        print(f"\n[*] Requesting signed download token for module '{module_name}'...")
        res = self._post("module/token", {"module": module_name})
        if not res.get("success"):
            print(f"[-] Module token request failed: {res.get('error', {}).get('message')}")
            return None

        data = res.get("data", {})
        print(f"[+] Signed Token Received!")
        print(f"    Target Asset: {data.get('key')}")
        print(f"    Download URL: {data.get('download_url')}")
        print(f"    Expires In  : {data.get('expires_in_seconds')} seconds")
        return data

    def logout(self) -> None:
        if self.session_token:
            print("\n[*] Logging out...")
            self._post("logout", {})
            self.session_token = None
            print("[+] Logged out cleanly.")


def main():
    client = AegisAuthClient(AUTH_URL, APP_KEY, APP_NAME, APP_VERSION)

    # 1. Handshake & Version Check
    if not client.init():
        return

    # 2. Authenticate
    if not client.login(USERNAME, PASSWORD):
        return

    # 3. Retrieve ephemeral download token for AetherVault binary
    token_data = client.request_module_token(MODULE_NAME)
    if token_data:
        print("\n[+] Success! Ready to stream binary directly from AetherVault CDN.")

    # 4. Clean up session
    client.logout()


if __name__ == "__main__":
    main()
