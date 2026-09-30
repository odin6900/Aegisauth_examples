/**
 * AegisAuth C++ Example
 * Demonstrates how to use the official AegisAuth C++ SDK.
 */

#include <iostream>
#include "AegisAuth/AegisAuth.hpp"

// Configuration Placeholders
const std::string AUTH_URL    = "https://auth.example.com";
const std::string APP_KEY     = "YOUR_APP_KEY";     // Application Key from dashboard
const std::string APP_NAME    = "My Application";
const std::string APP_VERSION = "1.0.0";

const std::string USERNAME    = "YOUR_USERNAME";
const std::string PASSWORD    = "YOUR_PASSWORD";
const std::string MODULE_NAME = "minecraft";

int main() {
    std::cout << "=== AegisAuth C++ Client [" << APP_NAME << " v" << APP_VERSION << "] ===" << std::endl;

    // Initialize SDK instance
    AegisAuth::api aegis(APP_NAME, APP_KEY, "", APP_VERSION, AUTH_URL);

    // 1. Handshake & Version check
    std::cout << "[*] Initializing client..." << std::endl;
    aegis.init();
    if (!aegis.response.success) {
        std::cerr << "[-] Init failed: " << aegis.response.message << std::endl;
        return 1;
    }

    std::cout << "[+] Handshake successful." << std::endl;
    std::cout << "    HWID: " << aegis.hwid << std::endl;
    std::cout << "    Server Time: " << aegis.app_data.server_time << std::endl;

    // 2. Login
    std::cout << "\n[*] Logging in as '" << USERNAME << "'..." << std::endl;
    aegis.login(USERNAME, PASSWORD);
    if (!aegis.response.success) {
        std::cerr << "[-] Login failed: " << aegis.response.message << std::endl;
        return 1;
    }

    std::cout << "[+] Logged in successfully!" << std::endl;
    std::cout << "    Session Token: " << aegis.sessionid.substr(0, 12) << "..." << std::endl;

    // 3. Subscription Check
    if (aegis.has_subscription("VIP")) {
        std::cout << "    [Subscription] VIP tier active — Premium unlocked!" << std::endl;
    } else {
        std::cout << "    [Subscription] Standard tier active." << std::endl;
    }

    // 4. Request signed module download token for AetherVault
    std::cout << "\n[*] Requesting signed download token for module '" << MODULE_NAME << "'..." << std::endl;
    std::string downloadUrl = aegis.request_module_token(MODULE_NAME);
    if (!downloadUrl.empty()) {
        std::cout << "[+] Signed Token Received!" << std::endl;
        std::cout << "    Download URL: " << downloadUrl << std::endl;
        std::cout << "\n[+] Binary payload authorized for AetherVault silent streaming!" << std::endl;
    }

    // 5. Logout
    std::cout << "\n[*] Logging out..." << std::endl;
    aegis.logout();
    std::cout << "[+] Logged out cleanly." << std::endl;

    return 0;
}
