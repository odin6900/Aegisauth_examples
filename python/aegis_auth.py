"""
AegisAuth — official Python SDK for the Aegis Authentication platform.

Single-file, zero-dependency client (standard library only — no `requests`
required). Drop `aegis_auth.py` into any project:

    from aegis_auth import AegisAuth

    aegis_app = AegisAuth(
        name="My Application",        # display name of your application
        ownerid="YOUR-APPLICATION-KEY",  # Application Key from the dashboard
        secret="",                    # optional API key (elevated calls only)
        version="1.0.0",              # version of the build you ship
    )

    aegis_app.init()
    aegis_app.login("ada", "correct horse battery staple")
    print(aegis_app.user_data.username)

Constructor parameter mapping:
    name     -> application display name (informational)
    ownerid  -> Application Key (public key), sent as the `x-app-key` header
    secret   -> optional API key for elevated calls, sent as `x-api-key`
    version  -> client version, checked against the minimum published version
    url      -> optional custom API base URL (self-hosted deployments)

Works on CPython 3.8+ on Windows, macOS and Linux.
"""

from __future__ import annotations

import getpass
import hashlib
import json
import platform
import subprocess
import time
import urllib.error
import urllib.request
import uuid
from datetime import datetime, timezone

__version__ = "1.0.0"
__all__ = ["AegisAuth", "DEFAULT_BASE_URL"]

# Stable preview URL serving the latest backend build. Swap to the production
# URL (drop the "-dev") once the site is published.
DEFAULT_BASE_URL = "https://project--0e9bb1c8-e87c-49ee-832e-1356cbeaddb3-dev.lovable.app"


# ---------------------------------------------------------------------------
# Data containers
# ---------------------------------------------------------------------------

class _Response:
    """Outcome of the most recent call. Check `response.success` after every method."""

    def __init__(self) -> None:
        self.success: bool = False
        self.message: str = ""


class Subscription:
    """An active license or subscription tier attached to the signed-in user."""

    def __init__(self, key: str, status: str, expires_at: str | None, name: str | None = None) -> None:
        self.subscription = key
        self.name = name or key
        self.status = status
        self.expiry = _parse_date(expires_at)

    @property
    def timeleft(self):
        if not self.expiry:
            return None
        return self.expiry - datetime.now(timezone.utc)


class _UserData:
    """Data about the signed-in user. Populated by login/register/license/check."""

    def __init__(self) -> None:
        self.username: str | None = None
        self.email: str | None = None
        self.status: str | None = None
        self.hwid: str | None = None
        self.createdate: datetime | None = None
        self.lastlogin: datetime | None = None
        self.expires_at: datetime | None = None
        self.logincount: int = 0
        self.subscriptions: list[Subscription] = []

    def clear(self) -> None:
        self.__init__()


class _AppData:
    """Application-wide data. Populated by init/fetchstats."""

    def __init__(self) -> None:
        self.numUsers: int = 0
        self.numKeys: int = 0
        self.app_ver: str = ""
        self.maintenance: bool = False
        self.maintenance_message: str | None = None
        self.hwid_required: bool = False
        self.session_timeout_minutes: int = 0
        self.server_time: str | None = None
        self.downloads: list[dict] = []


# ---------------------------------------------------------------------------
# Client
# ---------------------------------------------------------------------------

class AegisAuth:
    """AegisAuth client. Create one instance per application."""

    def __init__(
        self,
        name: str,
        ownerid: str,
        secret: str,
        version: str,
        url: str = DEFAULT_BASE_URL,
    ) -> None:
        if not name:
            raise ValueError("name is required")
        if not ownerid:
            raise ValueError("ownerid (Application Key) is required")

        self.name = name
        self.ownerid = ownerid
        self.secret = secret or ""
        self.version = version or "1.0.0"
        self._base_url = url.rstrip("/") + "/api/public/v1/"

        self.sessionid: str | None = None
        self.hwid: str = _hardware_id()

        self.user_data = _UserData()
        self.app_data = _AppData()
        self.response = _Response()

    # ------------------------------------------------------------------
    # Core
    # ------------------------------------------------------------------

    def init(self) -> None:
        """Handshake with the server. Call once before anything else; fills app_data."""
        data = self._req("init", {"version": self.version})
        if not self.response.success or not data:
            return

        version_info = data.get("version") or {}
        self.app_data.app_ver = version_info.get("current", self.version)
        self.app_data.maintenance = bool(data.get("maintenance"))
        self.app_data.maintenance_message = data.get("maintenance_message")
        self.app_data.hwid_required = bool(data.get("hwid_required"))
        self.app_data.session_timeout_minutes = int(data.get("session_timeout_minutes") or 0)
        self.app_data.server_time = data.get("server_time")

        if version_info.get("update_required"):
            self._error("An update is required before you can use this build.")
            return
        self.response.message = f"Initialized {self.name}"

    def register(self, user: str, password: str, license_key: str | None = None,
                 email: str | None = None) -> None:
        """Register a new user, optionally redeeming a license key."""
        body = {"username": user, "password": password, "hwid": self.hwid}
        if license_key:
            body["license_key"] = license_key
        if email:
            body["email"] = email
        data = self._req("register", body)
        if self.response.success:
            self._store_auth(data, "Registered successfully")

    def login(self, user: str, password: str) -> None:
        """Authenticate a user and open a session."""
        data = self._req("login", {"username": user, "password": password, "hwid": self.hwid})
        if self.response.success:
            self._store_auth(data, "Logged in successfully")

    def license(self, key: str) -> None:
        """Key-only authentication: validates a license key and binds it to this machine."""
        check = self._req("license/validate", {"license_key": key, "hwid": self.hwid})
        if not self.response.success:
            return
        activation = self._req("license/activate", {"license_key": key, "hwid": self.hwid})
        if not self.response.success:
            return

        lic = (activation or {}).get("license") or (check or {}).get("license")
        self.user_data.username = None
        self.user_data.hwid = self.hwid
        subs: list[Subscription] = []
        if lic:
            for s in lic.get("subscriptions") or []:
                if isinstance(s, dict):
                    subs.append(Subscription(s.get("name", ""), s.get("status", "active"), s.get("expires_at"), name=s.get("name")))
            subs.append(_subscription(lic))
        self.user_data.subscriptions = subs
        self.response.message = "License activated"

    def upgrade(self, user: str, key: str) -> None:
        """Attach a license key to an existing user account."""
        self._req("license/activate", {
            "license_key": key,
            "hwid": self.hwid,
            "username": user,
        })
        if self.response.success:
            self.response.message = f"License attached to {user}"

    def var(self, varid: str) -> str | None:
        """Read an application variable published from the dashboard."""
        data = self._req("variables/get", {"scope": "application"})
        if not self.response.success or not data:
            return None
        variables = data.get("variables") or {}
        if varid in variables:
            return variables[varid]
        self._error(f"Variable not found: {varid}")
        return None

    def getvar(self, varname: str) -> str | None:
        """Read a per-user variable (requires an active session)."""
        data = self._req("variables/get", {"scope": "user"})
        if not self.response.success or not data:
            return None
        return (data.get("variables") or {}).get(varname)

    def setvar(self, varname: str, value: str) -> None:
        """Write a per-user variable (requires an active session)."""
        self._req("variables/set", {"scope": "user", "key": varname, "value": value or ""})
        if self.response.success:
            self.response.message = "Variable saved"

    def log(self, message: str) -> None:
        """Write a message to the application's audit log on the dashboard."""
        self._req("log", {
            "message": message or "",
            "pcuser": getpass.getuser(),
            "hwid": self.hwid,
        })
        if self.response.success:
            self.response.message = "Logged"

    def check(self) -> bool:
        """Validate the active session against the server. Fills user_data."""
        if not self.sessionid:
            self._error("No active session")
            return False
        data = self._req("session/check", {})
        if not self.response.success or not data or not data.get("valid"):
            return False
        self._fill_user(data.get("user"), data.get("license"))
        return True

    def logout(self) -> None:
        """Terminate the active session on the server."""
        self._req("logout", {})
        self.sessionid = None
        self.user_data.clear()
        if self.response.success:
            self.response.message = "Logged out"

    def fetchstats(self) -> None:
        """Refresh app_data with live counters, versions and download links."""
        data = self._req("app/data", {})
        if not self.response.success or not data:
            return
        stats = data.get("stats") or {}
        self.app_data.numUsers = int(stats.get("users") or 0)
        self.app_data.numKeys = int(stats.get("licenses") or 0)
        application = data.get("application") or {}
        self.app_data.app_ver = application.get("current_version", self.app_data.app_ver)
        self.app_data.downloads = [
            {"name": d.get("name"), "url": d.get("file_url")}
            for d in data.get("downloads") or []
        ]
        self.response.message = "Stats fetched"

    def update_available(self, channel: str = "stable") -> bool:
        """True when a newer build than `version` has been published."""
        data = self._req("version/check", {"version": self.version, "channel": channel})
        return bool(self.response.success and data and data.get("update_available"))

    def use_session(self, token: str) -> None:
        """Restore a session token persisted earlier (skip the login screen)."""
        self.sessionid = token

    def expirydaysleft(self) -> int:
        """Days remaining on the active license, or 0 when none/expired."""
        if not self.user_data.subscriptions:
            return 0
        expiry = self.user_data.subscriptions[0].expiry
        if not expiry:
            return 0
        return max(0, (expiry - datetime.now(timezone.utc)).days)

    def has_subscription(self, name: str) -> bool:
        """Check if the user/license has an active subscription by name."""
        if not name:
            return False
        needle = name.lower()
        now = datetime.now(timezone.utc)
        for s in self.user_data.subscriptions:
            if s.name.lower() == needle or s.subscription.lower() == needle:
                if s.status and s.status.lower() != "active":
                    continue
                if s.expiry and s.expiry <= now:
                    continue
                return True
        return False

    def get_subscriptions(self) -> list[Subscription]:
        """Return the list of active subscriptions for the current user."""
        return list(self.user_data.subscriptions)

    def request_module_token(self, module: str | None = None, key: str | None = None) -> dict | None:
        """Request an ephemeral signed download token for an AetherVault asset/module."""
        body = {}
        if module:
            body["module"] = module
        if key:
            body["key"] = key
        data = self._req("module/token", body)
        return data if self.response.success else None

    # ------------------------------------------------------------------
    # Internals
    # ------------------------------------------------------------------

    def _store_auth(self, data: dict, message: str) -> None:
        session = (data or {}).get("session") or {}
        if session.get("token"):
            self.sessionid = session["token"]
        self._fill_user((data or {}).get("user"), (data or {}).get("license"))
        self.response.message = message

    def _fill_user(self, user: dict | None, license_: dict | None) -> None:
        subs: list[Subscription] = []
        if user:
            self.user_data.username = user.get("username")
            self.user_data.email = user.get("email")
            self.user_data.status = user.get("status")
            self.user_data.hwid = user.get("hwid")
            self.user_data.createdate = _parse_date(user.get("created_at"))
            self.user_data.lastlogin = _parse_date(user.get("last_login_at"))
            self.user_data.expires_at = _parse_date(user.get("expires_at"))
            self.user_data.logincount = int(user.get("login_count") or 0)
            for s in user.get("subscriptions") or []:
                if isinstance(s, dict):
                    subs.append(Subscription(s.get("name", ""), s.get("status", "active"), s.get("expires_at"), name=s.get("name")))
        if license_:
            for s in license_.get("subscriptions") or []:
                if isinstance(s, dict) and not any(x.name.lower() == str(s.get("name", "")).lower() for x in subs):
                    subs.append(Subscription(s.get("name", ""), s.get("status", "active"), s.get("expires_at"), name=s.get("name")))
            subs.append(_subscription(license_))
        self.user_data.subscriptions = subs

    def _error(self, message: str) -> None:
        self.response.success = False
        self.response.message = message

    def _req(self, endpoint: str, body: dict) -> dict | None:
        """POST to an endpoint, normalise the envelope, fill `response`."""
        headers = {
            "content-type": "application/json",
            "x-app-key": self.ownerid,
            "x-timestamp": str(int(time.time())),
            "user-agent": f"aegisauth-python/{__version__}",
        }
        if self.secret:
            headers["x-api-key"] = self.secret
        if self.sessionid:
            headers["x-session-token"] = self.sessionid

        request = urllib.request.Request(
            self._base_url + endpoint,
            data=json.dumps(body).encode("utf-8"),
            headers=headers,
            method="POST",
        )

        try:
            with urllib.request.urlopen(request, timeout=30) as handle:
                payload = json.loads(handle.read().decode("utf-8") or "{}")
        except urllib.error.HTTPError as err:
            try:
                payload = json.loads(err.read().decode("utf-8") or "{}")
            except Exception:
                self._error(f"Network error: HTTP {err.code}")
                return None
        except Exception as err:  # URLError, timeout, JSON decode, ...
            self._error(f"Network error: {err}")
            return None

        if not payload.get("success"):
            error = payload.get("error") or {}
            self._error(error.get("message") or "Request failed.")
            return None

        self.response.success = True
        self.response.message = "Success"
        return payload.get("data") or {}


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _subscription(license_: dict | None) -> Subscription:
    license_ = license_ or {}
    return Subscription(license_.get("key"), license_.get("status"), license_.get("expires_at"))


def _parse_date(iso: str | None) -> datetime | None:
    if not iso:
        return None
    try:
        parsed = datetime.fromisoformat(iso.replace("Z", "+00:00"))
        return parsed if parsed.tzinfo else parsed.replace(tzinfo=timezone.utc)
    except ValueError:
        return None


def _hardware_id() -> str:
    """Stable per-machine identifier (SHA-256 hashed; raw machine id never leaves the device)."""
    system = platform.system()
    seed = ""
    try:
        if system == "Windows":
            seed = subprocess.check_output(
                "wmic csproduct get uuid", shell=True, timeout=5
            ).decode(errors="ignore").split("\n")[1].strip()
        elif system == "Darwin":
            seed = subprocess.check_output(
                "ioreg -rd1 -c IOPlatformExpertDevice | grep IOPlatformUUID",
                shell=True, timeout=5,
            ).decode(errors="ignore").split('"')[-2]
        else:
            with open("/etc/machine-id", "r", encoding="utf-8") as handle:
                seed = handle.read().strip()
    except Exception:
        seed = ""
    if not seed:
        seed = f"{platform.node()}|{getpass.getuser()}|{uuid.getnode()}"
    return hashlib.sha256(f"aegisauth:{seed}".encode()).hexdigest()