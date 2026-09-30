/**
 * AegisAuth C++ Example
 * Demonstrates initialization, user login, license validation, and requesting a signed AetherVault module token.
 * Uses native Windows WinINet with zero third-party library dependencies.
 */

#include <windows.h>
#include <wininet.h>
#include <wincrypt.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "advapi32.lib")

// Configuration Placeholders
const std::string AUTH_HOST   = "auth.example.com"; // AegisAuth host (without https://)
const INTERNET_PORT AUTH_PORT = INTERNET_DEFAULT_HTTPS_PORT;
const bool USE_HTTPS          = true;

const std::string APP_KEY     = "YOUR_APP_KEY";     // Application Key from dashboard
const std::string APP_NAME    = "My Application";
const std::string APP_VERSION = "1.0.0";

const std::string USERNAME    = "YOUR_USERNAME";
const std::string PASSWORD    = "YOUR_PASSWORD";
const std::string MODULE_NAME = "minecraft";

// Simple JSON string value extractor
std::string ExtractJsonValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.length()) return "";

    if (json[pos] == '\"') {
        pos++;
        size_t end = json.find('\"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        size_t end = json.find_first_of(",}\n\r", pos);
        if (end == std::string::npos) end = json.length();
        return json.substr(pos, end - pos);
    }
}

// Generate machine-locked HWID (hashed ComputerName + Volume Serial)
std::string GetHardwareId() {
    char compName[MAX_COMPUTER_NAME_LENGTH + 1];
    DWORD compLen = MAX_COMPUTER_NAME_LENGTH + 1;
    GetComputerNameA(compName, &compLen);

    DWORD volSerial = 0;
    GetVolumeInformationA("C:\\", NULL, 0, &volSerial, NULL, NULL, NULL, 0);

    std::stringstream ss;
    ss << compName << "-" << std::hex << volSerial;
    std::string seed = ss.str();

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string hwid = "fallback-hwid";

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptHashData(hHash, (BYTE*)seed.c_str(), (DWORD)seed.length(), 0);
            BYTE hash[32];
            DWORD hashLen = 32;
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                std::stringstream hexStream;
                for (DWORD i = 0; i < hashLen; i++) {
                    char buf[3];
                    sprintf_s(buf, "%02x", hash[i]);
                    hexStream << buf;
                }
                hwid = hexStream.str();
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return hwid;
}

class AegisAuthClient {
private:
    std::string m_host;
    INTERNET_PORT m_port;
    bool m_https;
    std::string m_appKey;
    std::string m_appName;
    std::string m_version;
    std::string m_hwid;
    std::string m_sessionToken;

    std::string Post(const std::string& endpoint, const std::string& jsonPayload) {
        HINTERNET hInternet = InternetOpenA("AegisAuth-Cpp/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return "";

        HINTERNET hConnect = InternetConnectA(hInternet, m_host.c_str(), m_port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return "";
        }

        std::string fullPath = "/api/public/v1/" + endpoint;
        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (m_https) {
            flags |= INTERNET_FLAG_SECURE;
        }

        HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", fullPath.c_str(), NULL, NULL, NULL, flags, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return "";
        }

        std::stringstream headers;
        headers << "Content-Type: application/json\r\n";
        headers << "x-app-key: " << m_appKey << "\r\n";
        headers << "x-timestamp: " << time(nullptr) << "\r\n";
        if (!m_sessionToken.empty()) {
            headers << "x-session-token: " << m_sessionToken << "\r\n";
        }

        std::string headerStr = headers.str();
        BOOL sendOk = HttpSendRequestA(hRequest, headerStr.c_str(), (DWORD)headerStr.length(),
                                       (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length());

        std::string responseBody;
        if (sendOk) {
            char buffer[4096];
            DWORD bytesRead = 0;
            while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                responseBody += buffer;
            }
        }

        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return responseBody;
    }

public:
    AegisAuthClient(const std::string& host, INTERNET_PORT port, bool https,
                    const std::string& appKey, const std::string& appName, const std::string& version)
        : m_host(host), m_port(port), m_https(https),
          m_appKey(appKey), m_appName(appName), m_version(version) {
        m_hwid = GetHardwareId();
    }

    bool Init() {
        std::cout << "[*] Initializing " << m_appName << " (v" << m_version << ")..." << std::endl;
        std::cout << "    HWID: " << m_hwid << std::endl;

        std::stringstream payload;
        payload << "{\"version\":\"" << m_version << "\"}";
        std::string res = Post("init", payload.str());

        if (res.find("\"success\":true") == std::string::npos) {
            std::cout << "[-] Init failed. Response: " << res << std::endl;
            return false;
        }

        std::cout << "[+] Handshake successful." << std::endl;
        return true;
    }

    bool Login(const std::string& username, const std::string& password) {
        std::cout << "\n[*] Logging in as '" << username << "'..." << std::endl;

        std::stringstream payload;
        payload << "{\"username\":\"" << username << "\",\"password\":\"" << password << "\",\"hwid\":\"" << m_hwid << "\"}";
        std::string res = Post("login", payload.str());

        if (res.find("\"success\":true") == std::string::npos) {
            std::string err = ExtractJsonValue(res, "message");
            std::cout << "[-] Login failed: " << (err.empty() ? res : err) << std::endl;
            return false;
        }

        m_sessionToken = ExtractJsonValue(res, "token");
        std::cout << "[+] Logged in successfully!" << std::endl;
        std::cout << "    Session Token: " << m_sessionToken.substr(0, 12) << "..." << std::endl;
        return true;
    }

    std::string RequestModuleToken(const std::string& moduleName) {
        std::cout << "\n[*] Requesting signed download token for module '" << moduleName << "'..." << std::endl;

        std::stringstream payload;
        payload << "{\"module\":\"" << moduleName << "\"}";
        std::string res = Post("module/token", payload.str());

        if (res.find("\"success\":true") == std::string::npos) {
            std::string err = ExtractJsonValue(res, "message");
            std::cout << "[-] Module token request failed: " << (err.empty() ? res : err) << std::endl;
            return "";
        }

        std::string downloadUrl = ExtractJsonValue(res, "download_url");
        std::string key = ExtractJsonValue(res, "key");

        std::cout << "[+] Module authorization verified!" << std::endl;
        std::cout << "    Target Asset: " << key << std::endl;
        std::cout << "    Signed URL  : " << downloadUrl << std::endl;
        return downloadUrl;
    }

    void Logout() {
        if (!m_sessionToken.empty()) {
            std::cout << "\n[*] Logging out..." << std::endl;
            Post("logout", "{}");
            m_sessionToken = "";
            std::cout << "[+] Logged out cleanly." << std::endl;
        }
    }
};

int main() {
    AegisAuthClient client(AUTH_HOST, AUTH_PORT, USE_HTTPS, APP_KEY, APP_NAME, APP_VERSION);

    if (!client.Init()) {
        return 1;
    }

    if (!client.Login(USERNAME, PASSWORD)) {
        return 1;
    }

    std::string downloadUrl = client.RequestModuleToken(MODULE_NAME);
    if (!downloadUrl.empty()) {
        std::cout << "\n[+] Ready to download/inject payload from AetherVault!" << std::endl;
    }

    client.Logout();
    return 0;
}
