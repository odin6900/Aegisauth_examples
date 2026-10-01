// ============================================================================
// AegisAuth — official C++ SDK for the Aegis Authentication platform
// ----------------------------------------------------------------------------
// C++17, x86 & x64, Windows / Linux / macOS.
// Header-only: native WinHTTP on Windows (no libcurl required), libcurl on POSIX.
// JSON: bundled header-only codec (json.hpp) — no external package dependencies.
//
// Quick start:
//
//   aegis::Options options;
//   options.name = "My Application";
//   options.ownerId = "YOUR-APPLICATION-KEY";
//   options.secret = "OPTIONAL-SECRET";
//   options.version = "1.0.0";
//
//   aegis::Client app(options);
//   app.init();
//   app.login("username", "password");
//   if (app.response.success) {
//       std::cout << "Logged in as: " << app.user_data.username << std::endl;
//   }
// ============================================================================
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "json.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#else
#include <unistd.h>
#include <curl/curl.h>
#endif

namespace AegisAuth {

/** Default platform endpoint. Pass a custom url to options or constructor for self-hosted deployments. */
constexpr const char* kDefaultBaseUrl = "https://aegisauth.realm.sryze.cc";

namespace internal {

inline std::string trimSlashes(std::string value) {
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
}

inline std::string digest(const std::string& input) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : input) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    std::ostringstream out;
    for (int round = 0; round < 4; ++round) {
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        out << std::hex << h;
    }
    return out.str().substr(0, 64);
}

inline int daysUntil(const std::string& iso) {
    if (iso.size() < 19) return 0;
    std::tm tm{};
    if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6)
        return 0;
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#if defined(_WIN32)
    std::time_t expiry = _mkgmtime(&tm);
#else
    std::time_t expiry = timegm(&tm);
#endif
    if (expiry < 0) return 0;
    const double days = std::difftime(expiry, std::time(nullptr)) / 86400.0;
    return days > 0 ? static_cast<int>(days + 0.999) : 0;
}

#if defined(_WIN32)
inline bool httpPostWin(const std::string& fullUrl, const std::string& body,
                        const std::vector<std::string>& headers,
                        long& statusCode, std::string& responseText, std::string& errorMsg) {
    statusCode = 0;
    responseText.clear();
    errorMsg.clear();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, fullUrl.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        errorMsg = "Invalid URL encoding";
        return false;
    }
    std::wstring wUrl(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, fullUrl.c_str(), -1, &wUrl[0], wlen);

    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength    = (DWORD)-1;
    urlComp.dwHostNameLength  = (DWORD)-1;
    urlComp.dwUrlPathLength   = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wcslen(wUrl.c_str()), 0, &urlComp)) {
        errorMsg = "WinHttpCrackUrl failed: " + std::to_string(GetLastError());
        return false;
    }

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(L"aegisauth-cpp/1.0.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        errorMsg = "WinHttpOpen failed: " + std::to_string(GetLastError());
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        errorMsg = "WinHttpConnect failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD dwFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
    if (!hRequest) {
        errorMsg = "WinHttpOpenRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring allHeaders;
    for (const auto& h : headers) {
        int hlen = MultiByteToWideChar(CP_UTF8, 0, h.c_str(), -1, nullptr, 0);
        std::wstring wh(hlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, h.c_str(), -1, &wh[0], hlen);
        if (!wh.empty() && wh.back() == L'\0') wh.pop_back();
        allHeaders += wh + L"\r\n";
    }

    BOOL bResults = WinHttpSendRequest(hRequest,
                                       allHeaders.c_str(), (DWORD)allHeaders.length(),
                                       (LPVOID)body.data(), (DWORD)body.length(),
                                       (DWORD)body.length(), 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, nullptr);
    }

    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
        statusCode = dwStatusCode;

        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize + 1, 0);
            if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                responseText.append(buffer.data(), dwDownloaded);
            } else {
                break;
            }
        } while (dwSize > 0);
    } else {
        errorMsg = "WinHTTP transport error: " + std::to_string(GetLastError());
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return bResults != FALSE;
}
#else
inline size_t writeCallbackCurl(void* contents, size_t size, size_t nmemb, std::string* out) {
    out->append(static_cast<const char*>(contents), size * nmemb);
    return size * nmemb;
}

inline bool httpPostCurl(const std::string& fullUrl, const std::string& body,
                         const std::vector<std::string>& headers,
                         long& statusCode, std::string& responseText, std::string& errorMsg) {
    statusCode = 0;
    responseText.clear();
    errorMsg.clear();

    CURL* curl = curl_easy_init();
    if (!curl) {
        errorMsg = "curl_easy_init failed";
        return false;
    }

    curl_slist* list = nullptr;
    for (const auto& h : headers) {
        list = curl_slist_append(list, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallbackCurl);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseText);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        errorMsg = curl_easy_strerror(code);
        return false;
    }
    return true;
}
#endif

}  // namespace internal

/** Application configuration options. */
struct Options {
    std::string name;
    std::string ownerId;
    std::string secret;
    std::string version = "1.0.0";
    std::string baseUrl = kDefaultBaseUrl;
};

/**
 * AegisAuth client. Create one instance per application and keep it alive
 * for the lifetime of the process.
 */
class api {
public:
    std::string name;     ///< Application display name (informational).
    std::string ownerid;  ///< Application Key (public key) — identifies the application.
    std::string secret;   ///< Optional API key for elevated calls.
    std::string version;  ///< Version of the build you are shipping.
    std::string url;      ///< API base URL.

    std::string sessionid;  ///< Active session token. Persist + restore with use_session().
    std::string hwid;       ///< Hardware id of this machine (hashed).

    /** Outcome of the most recent call. Check response.success after every method. */
    class response_class {
    public:
        bool success = false;
        std::string message;
    };

    /** An active license or subscription tier attached to the signed-in user (or key-only session). */
    struct Subscription {
        std::string subscription;  ///< license key or subscription name
        std::string name;          ///< subscription name
        std::string status;
        std::string expiry;        ///< ISO-8601 timestamp
    };

    /** Data about the signed-in user. Populated by login/regstr/license/check. */
    class user_data_class {
    public:
        std::string username;
        std::string email;
        std::string status;
        std::string hwid;
        std::string createdate;
        std::string lastlogin;
        std::string expires_at;
        int logincount = 0;
        std::vector<Subscription> subscriptions;
    };

    /** Metadata and remote configuration fetched from the dashboard via init(). */
    class app_data_class {
    public:
        int numUsers = 0;
        int numKeys = 0;
        std::string app_ver;
        std::string customer_panel;
        bool maintenance = false;
        std::string maintenance_message;
        bool hwid_required = false;
        int session_timeout_minutes = 60;
        std::string server_time;
        std::vector<std::pair<std::string, std::string>> downloads;  ///< (name, url)
    };

    response_class response;
    user_data_class user_data;
    app_data_class app_data;

    /**
     * Creates the client.
     * @param name     Application display name.
     * @param ownerid  Application Key (public key) from the Aegis dashboard.
     * @param secret   Optional API key; pass "" for standard client use.
     * @param version  Client version string, e.g. "1.0.0".
     * @param url      Optional custom API base URL.
     */
    api(std::string name, std::string ownerid, std::string secret, std::string version,
        std::string url = kDefaultBaseUrl)
        : name(std::move(name)),
          ownerid(std::move(ownerid)),
          secret(std::move(secret)),
          version(std::move(version)),
          url(internal::trimSlashes(std::move(url))) {
        if (this->ownerid.empty()) throw std::invalid_argument("ownerid (Application Key) is required");
        hwid = hardware_id();
#if !defined(_WIN32)
        curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    }

    /** Creates the client from an Options object. */
    api(const Options& opt)
        : api(opt.name, opt.ownerId, opt.secret, opt.version,
              opt.baseUrl.empty() ? kDefaultBaseUrl : opt.baseUrl) {}

    /** Handshake with the server. Call once before anything else; fills app_data. */
    void init() {
        aegisauth::Json data = req("init", {{"version", aegisauth::Json(version)}});
        if (!response.success) return;

        app_data.app_ver = data["version"]["current"].asString(version);
        app_data.maintenance = data["maintenance"].asBool();
        app_data.maintenance_message = data["maintenance_message"].asString();
        app_data.hwid_required = data["hwid_required"].asBool();
        app_data.session_timeout_minutes =
            static_cast<int>(data["session_timeout_minutes"].asNumber());
        app_data.server_time = data["server_time"].asString();

        if (data["version"]["update_required"].asBool()) {
            error("An update is required before you can use this build.");
            return;
        }
        response.message = "Initialized " + name;
    }

    /** Authenticate a user and open a session. */
    void login(std::string username, std::string password) {
        aegisauth::Json data = req("login", {{"username", aegisauth::Json(std::move(username))},
                                             {"password", aegisauth::Json(std::move(password))},
                                             {"hwid", aegisauth::Json(hwid)}});
        if (response.success) store_auth(data, "Logged in successfully");
    }

    /** Register a new user, optionally redeeming a license key. */
    void regstr(std::string username, std::string password, std::string key = "",
                std::string email = "") {
        aegisauth::JsonObject body{{"username", aegisauth::Json(std::move(username))},
                                   {"password", aegisauth::Json(std::move(password))},
                                   {"hwid", aegisauth::Json(hwid)}};
        if (!key.empty()) body.emplace("license_key", aegisauth::Json(std::move(key)));
        if (!email.empty()) body.emplace("email", aegisauth::Json(std::move(email)));
        aegisauth::Json data = req("register", body);
        if (response.success) store_auth(data, "Registered successfully");
    }

    /** Key-only authentication: validates a license key and binds it to this machine. */
    void license(std::string key) {
        aegisauth::Json check = req("license/validate", {{"license_key", aegisauth::Json(key)}, {"hwid", aegisauth::Json(hwid)}});
        if (!response.success) return;
        aegisauth::Json activation = req("license/activate", {{"license_key", aegisauth::Json(key)}, {"hwid", aegisauth::Json(hwid)}});
        if (!response.success) return;

        const aegisauth::Json lic = activation["license"]["key"].asString().empty() ? check["license"]
                                                                         : activation["license"];
        user_data.username.clear();
        user_data.hwid = hwid;
        user_data.subscriptions.clear();
        for (const aegisauth::Json& s : lic["subscriptions"].items()) {
            const std::string sname = s["name"].asString();
            if (!sname.empty()) {
                user_data.subscriptions.push_back({sname, sname, s["status"].asString(), s["expires_at"].asString()});
            }
        }
        if (!lic["key"].asString().empty())
            user_data.subscriptions.push_back(
                {lic["key"].asString(), lic["key"].asString(), lic["status"].asString(), lic["expires_at"].asString()});
        response.message = "License activated";
    }

    /** Attach a license key to an existing user account. */
    void upgrade(std::string username, std::string key) {
        req("license/activate", {{"license_key", aegisauth::Json(std::move(key))},
                                 {"hwid", aegisauth::Json(hwid)},
                                 {"username", aegisauth::Json(username)}});
        if (response.success) response.message = "License attached to " + username;
    }

    /** Read an application variable published from the dashboard. */
    std::string var(std::string varid) {
        aegisauth::Json data = req("variables/get", {{"scope", aegisauth::Json("application")}});
        if (!response.success) return "";
        const std::string value = data["variables"][varid].asString();
        if (value.empty()) {
            error("Variable not found: " + varid);
            return "";
        }
        return value;
    }

    /** Read a per-user variable (requires an active session). */
    std::string getvar(std::string varname) {
        aegisauth::Json data = req("variables/get", {{"scope", aegisauth::Json("user")}});
        if (!response.success) return "";
        return data["variables"][varname].asString();
    }

    /** Write a per-user variable (requires an active session). */
    void setvar(std::string varname, std::string value) {
        req("variables/set", {{"scope", aegisauth::Json("user")},
                              {"key", aegisauth::Json(std::move(varname))},
                              {"value", aegisauth::Json(std::move(value))}});
        if (response.success) response.message = "Variable saved";
    }

    /** Write a message to the application's audit log on the dashboard. */
    void log(std::string msg) {
        const char* user = std::getenv(
#if defined(_WIN32)
            "USERNAME"
#else
            "USER"
#endif
        );
        req("log", {{"message", aegisauth::Json(std::move(msg))},
                    {"pcuser", aegisauth::Json(user ? user : "")},
                    {"hwid", aegisauth::Json(hwid)}});
        if (response.success) response.message = "Logged";
    }

    /** Validate the active session against the server. Fills user_data. */
    bool check() {
        if (sessionid.empty()) {
            error("No active session");
            return false;
        }
        aegisauth::Json data = req("session/check", {});
        if (!response.success || !data["valid"].asBool()) return false;
        fill_user(data["user"], data["license"]);
        return true;
    }

    /** Terminate the active session on the server. */
    void logout() {
        req("logout", {});
        sessionid.clear();
        user_data = user_data_class{};
        if (response.success) response.message = "Logged out";
    }

    /** Request an ephemeral signed download token for an AetherVault asset/module. */
    std::string request_module_token(std::string module_name, std::string key = "") {
        aegisauth::JsonObject body;
        if (!module_name.empty()) body.emplace("module", aegisauth::Json(std::move(module_name)));
        if (!key.empty()) body.emplace("key", aegisauth::Json(std::move(key)));
        aegisauth::Json data = req("module/token", body);
        if (!response.success) return "";
        return data["download_url"].asString();
    }

    /** Refresh app_data with live counters, versions and download links. */
    void fetchstats() {
        aegisauth::Json data = req("app/data", {});
        if (!response.success) return;
        app_data.numUsers = static_cast<int>(data["stats"]["users"].asNumber());
        app_data.numKeys = static_cast<int>(data["stats"]["licenses"].asNumber());
        app_data.app_ver = data["application"]["current_version"].asString(app_data.app_ver);
        app_data.downloads.clear();
        for (const aegisauth::Json& d : data["downloads"].items())
            app_data.downloads.emplace_back(d["name"].asString(), d["file_url"].asString());
        response.message = "Stats fetched";
    }

    /** True when a newer build than `version` has been published. */
    bool update_available(std::string channel = "stable") {
        aegisauth::Json data = req("version/check", {{"version", aegisauth::Json(version)},
                                          {"channel", aegisauth::Json(std::move(channel))}});
        return response.success && data["update_available"].asBool();
    }

    /** Restore a session token persisted earlier (skip the login screen). */
    void use_session(std::string token) { sessionid = std::move(token); }

    /** Days remaining on the active license, or 0 when none/expired. */
    int expirydaysleft() const {
        if (user_data.subscriptions.empty()) return 0;
        return internal::daysUntil(user_data.subscriptions.front().expiry);
    }

    /** Checks if the active user or license has a specific active subscription. */
    bool has_subscription(const std::string& subName) const {
        if (subName.empty()) return false;
        for (const auto& sub : user_data.subscriptions) {
            if (sub.name == subName || sub.subscription == subName) {
                if (!sub.status.empty() && sub.status != "active") continue;
                return true;
            }
        }
        return false;
    }

    /** Returns all active subscriptions for the current user and license. */
    std::vector<Subscription> get_subscriptions() const {
        return user_data.subscriptions;
    }

    /** Raw request escape hatch — returns the endpoint's `data` payload. */
    aegisauth::Json req(const std::string& endpoint, const aegisauth::JsonObject& body) {
        const std::string fullUrl = url + "/api/public/v1/" + endpoint;
        const std::string payload = aegisauth::Json(body).dump();
        std::string lastNetworkError;

        std::vector<std::string> headers = {
            "Content-Type: application/json",
            "User-Agent: aegisauth-cpp/1.0.0",
            "x-app-key: " + ownerid,
            "x-timestamp: " + std::to_string(std::time(nullptr))
        };
        if (!secret.empty()) headers.push_back("x-api-key: " + secret);
        if (!sessionid.empty()) headers.push_back("x-session-token: " + sessionid);

        for (int attempt = 0; attempt <= 2; ++attempt) {
            long status = 0;
            std::string text;
            std::string err;
            bool ok = false;

#if defined(_WIN32)
            ok = internal::httpPostWin(fullUrl, payload, headers, status, text, err);
#else
            ok = internal::httpPostCurl(fullUrl, payload, headers, status, text, err);
#endif

            if (!ok) {
                lastNetworkError = err;
                std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
                continue;
            }
            if (status >= 500 && attempt < 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
                continue;
            }

            aegisauth::Json envelope = aegisauth::Json::parse(text.empty() ? "{}" : text);
            if (!envelope["success"].asBool()) {
                error(envelope["error"]["message"].asString("Request failed."));
                return aegisauth::Json();
            }
            response.success = true;
            response.message = "Success";
            return envelope["data"];
        }

        error("Network error: " + (lastNetworkError.empty() ? "request failed" : lastNetworkError));
        return aegisauth::Json();
    }

private:
    void error(std::string message) {
        response.success = false;
        response.message = std::move(message);
    }

    void store_auth(const aegisauth::Json& data, const char* msg) {
        const std::string token = data["session"]["token"].asString();
        if (!token.empty()) sessionid = token;
        fill_user(data["user"], data["license"]);
        response.message = msg;
    }

    void fill_user(const aegisauth::Json& user, const aegisauth::Json& lic) {
        user_data.username = user["username"].asString();
        user_data.email = user["email"].asString();
        user_data.status = user["status"].asString();
        user_data.hwid = user["hwid"].asString();
        user_data.createdate = user["created_at"].asString();
        user_data.lastlogin = user["last_login_at"].asString();
        user_data.expires_at = user["expires_at"].asString();
        user_data.logincount = static_cast<int>(user["login_count"].asNumber());
        user_data.subscriptions.clear();
        for (const aegisauth::Json& s : user["subscriptions"].items()) {
            const std::string sname = s["name"].asString();
            if (!sname.empty()) {
                user_data.subscriptions.push_back({sname, sname, s["status"].asString(), s["expires_at"].asString()});
            }
        }
        for (const aegisauth::Json& s : lic["subscriptions"].items()) {
            const std::string sname = s["name"].asString();
            if (!sname.empty()) {
                user_data.subscriptions.push_back({sname, sname, s["status"].asString(), s["expires_at"].asString()});
            }
        }
        const std::string key = lic["key"].asString();
        if (!key.empty()) {
            user_data.subscriptions.push_back(
                {key, key, lic["status"].asString(), lic["expires_at"].asString()});
        }
    }

    static std::string hardware_id() {
        std::string facts;
#if defined(_WIN32)
        HKEY hKey = nullptr;
        char guid[256] = {0};
        DWORD sz = sizeof(guid);
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr, reinterpret_cast<LPBYTE>(guid), &sz);
            RegCloseKey(hKey);
        }
        if (guid[0] != '\0') {
            return std::string(guid);
        }
        char name[256] = {0};
        DWORD size = sizeof(name);
        GetComputerNameA(name, &size);
        facts += name;
        if (const char* user = std::getenv("USERNAME")) facts += user;
        facts += "windows";
#else
        std::array<char, 256> host{};
        if (gethostname(host.data(), host.size()) == 0) facts += host.data();
        if (const char* user = std::getenv("USER")) facts += user;
        facts += "posix";
#endif
        return internal::digest(facts);
    }
};

/** Client class inheriting api for modern Options-based configuration. */
class Client : public api {
public:
    explicit Client(const Options& opt) : api(opt) {}
    using api::api;
};

}  // namespace AegisAuth

namespace aegis {
    using Options = AegisAuth::Options;
    using Client = AegisAuth::Client;
    using api = AegisAuth::api;
}