"""AegisAuth quickstart — classic authentication flow."""
import os
import sys

# Allow running straight from the examples/ folder.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from aegis_auth import AegisAuth  # noqa: E402

aegis_app = AegisAuth(
    name="My Application",
    ownerid="YOUR-APPLICATION-KEY",  # Application Key from the dashboard
    secret="",                       # leave empty for client-side use
    version="1.0.0",
)


def main() -> None:
    aegis_app.init()
    if not aegis_app.response.success:
        print("init failed:", aegis_app.response.message)
        return

    print("App version :", aegis_app.app_data.app_ver)
    print("HWID        :", aegis_app.hwid)
    print()
    print("[1] Login")
    print("[2] Register")
    print("[3] License key only")
    choice = input("Choice: ").strip()

    if choice == "1":
        aegis_app.login(input("Username: "), input("Password: "))
    elif choice == "2":
        user = input("Username: ")
        password = input("Password: ")
        key = input("License key (optional): ").strip()
        aegis_app.register(user, password, key or None)
    else:
        aegis_app.license(input("License key: ").strip())

    if not aegis_app.response.success:
        print("Failed:", aegis_app.response.message)
        return

    print()
    print("Welcome", aegis_app.user_data.username or "license holder")
    print("Days left:", aegis_app.expirydaysleft())

    # Multi-subscription check example:
    if aegis_app.has_subscription("VIP"):
        print("[Subscription] VIP tier active — Premium unlocked!")
    else:
        print("[Subscription] Standard tier active.")

    aegis_app.log("User opened the quickstart")
    print("Server says:", aegis_app.var("welcome_message") or "(no welcome_message variable)")

    aegis_app.logout()


if __name__ == "__main__":
    main()