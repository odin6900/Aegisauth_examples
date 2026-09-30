// AegisAuth C++ SDK — implementation. See include/AegisAuth/AegisAuth.hpp.
#include "AegisAuth/AegisAuth.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <thread>

#include <curl/curl.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    out->append(static_cast<const char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string trimSlashes(std::string value) {
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
}

/** FNV-1a based hex digest — stable across runs, never reveals raw machine facts. */
std::string digest(const std::string& input) {
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

/** Days between an ISO-8601 UTC timestamp ("2026-02-01T00:00:00Z") and now. */
int daysUntil(const std::string& iso) {
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

}  // namespace

namespace AegisAuth {

using aegisauth::Json;
using aegisauth::JsonObject;

api::api(std::string name, std::string ownerid, std::string secret, std::string version,
         std::string url)
    : name(std::move(name)),
      ownerid(std::move(ownerid)),
      secret(std::move(secret)),
      version(std::move(version)),
      url(trimSlashes(std::move(url))) {
    if (this->ownerid.empty()) throw std::invalid_argument("ownerid (Application Key) is required");
    hwid = hardware_id();
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

std::string api::hardware_id() {
    std::string facts;
#if defined(_WIN32)
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
    return digest(facts);
}

Json api::req(const std::string& endpoint, const JsonObject& body) {
    const std::string fullUrl = url + "/api/public/v1/" + endpoint;
    const std::string payload = Json(body).dump();
    std::string lastNetworkError;

    for (int attempt = 0; attempt <= 2; ++attempt) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            error("Unable to create an HTTP client.");
            return Json();
        }

        curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "content-type: application/json");
        headers = curl_slist_append(headers, "user-agent: aegisauth-cpp/1.0.0");
        headers = curl_slist_append(headers, ("x-app-key: " + ownerid).c_str());
        headers = curl_slist_append(
            headers, ("x-timestamp: " + std::to_string(std::time(nullptr))).c_str());
        if (!secret.empty()) headers = curl_slist_append(headers, ("x-api-key: " + secret).c_str());
        if (!sessionid.empty())
            headers = curl_slist_append(headers, ("x-session-token: " + sessionid).c_str());

        std::string text;
        curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &text);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        CURLcode code = curl_easy_perform(curl);
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (code != CURLE_OK) {
            lastNetworkError = curl_easy_strerror(code);
            std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
            continue;
        }
        if (status >= 500 && attempt < 2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250 * (attempt + 1)));
            continue;
        }

        Json envelope = Json::parse(text.empty() ? "{}" : text);
        if (!envelope["success"].asBool()) {
            error(envelope["error"]["message"].asString("Request failed."));
            return Json();
        }
        response.success = true;
        response.message = "Success";
        return envelope["data"];
    }

    error("Network error: " + (lastNetworkError.empty() ? "request failed" : lastNetworkError));
    return Json();
}

void api::error(std::string message) {
    response.success = false;
    response.message = std::move(message);
}

void api::store_auth(const Json& data, const char* message) {
    const std::string token = data["session"]["token"].asString();
    if (!token.empty()) sessionid = token;
    fill_user(data["user"], data["license"]);
    response.message = message;
}

void api::fill_user(const Json& user, const Json& license) {
    user_data.username = user["username"].asString();
    user_data.email = user["email"].asString();
    user_data.status = user["status"].asString();
    user_data.hwid = user["hwid"].asString();
    user_data.createdate = user["created_at"].asString();
    user_data.lastlogin = user["last_login_at"].asString();
    user_data.expires_at = user["expires_at"].asString();
    user_data.logincount = static_cast<int>(user["login_count"].asNumber());
    user_data.subscriptions.clear();
    for (const Json& s : user["subscriptions"].items()) {
        const std::string sname = s["name"].asString();
        if (!sname.empty()) {
            user_data.subscriptions.push_back({sname, sname, s["status"].asString(), s["expires_at"].asString()});
        }
    }
    for (const Json& s : license["subscriptions"].items()) {
        const std::string sname = s["name"].asString();
        if (!sname.empty()) {
            user_data.subscriptions.push_back({sname, sname, s["status"].asString(), s["expires_at"].asString()});
        }
    }
    const std::string key = license["key"].asString();
    if (!key.empty())
        user_data.subscriptions.push_back(
            {key, key, license["status"].asString(), license["expires_at"].asString()});
}

void api::init() {
    Json data = req("init", {{"version", Json(version)}});
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

void api::login(std::string username, std::string password) {
    Json data = req("login", {{"username", Json(std::move(username))},
                              {"password", Json(std::move(password))},
                              {"hwid", Json(hwid)}});
    if (response.success) store_auth(data, "Logged in successfully");
}

void api::regstr(std::string username, std::string password, std::string key, std::string email) {
    JsonObject body{{"username", Json(std::move(username))},
                    {"password", Json(std::move(password))},
                    {"hwid", Json(hwid)}};
    if (!key.empty()) body.emplace("license_key", Json(std::move(key)));
    if (!email.empty()) body.emplace("email", Json(std::move(email)));
    Json data = req("register", body);
    if (response.success) store_auth(data, "Registered successfully");
}

void api::license(std::string key) {
    Json check = req("license/validate", {{"license_key", Json(key)}, {"hwid", Json(hwid)}});
    if (!response.success) return;
    Json activation = req("license/activate", {{"license_key", Json(key)}, {"hwid", Json(hwid)}});
    if (!response.success) return;

    const Json lic = activation["license"]["key"].asString().empty() ? check["license"]
                                                                     : activation["license"];
    user_data.username.clear();
    user_data.hwid = hwid;
    user_data.subscriptions.clear();
    for (const Json& s : lic["subscriptions"].items()) {
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

void api::upgrade(std::string username, std::string key) {
    req("license/activate", {{"license_key", Json(std::move(key))},
                             {"hwid", Json(hwid)},
                             {"username", Json(username)}});
    if (response.success) response.message = "License attached to " + username;
}

std::string api::var(std::string varid) {
    Json data = req("variables/get", {{"scope", Json("application")}});
    if (!response.success) return "";
    const std::string value = data["variables"][varid].asString();
    if (value.empty()) {
        error("Variable not found: " + varid);
        return "";
    }
    return value;
}

std::string api::getvar(std::string varname) {
    Json data = req("variables/get", {{"scope", Json("user")}});
    if (!response.success) return "";
    return data["variables"][varname].asString();
}

void api::setvar(std::string varname, std::string value) {
    req("variables/set", {{"scope", Json("user")},
                          {"key", Json(std::move(varname))},
                          {"value", Json(std::move(value))}});
    if (response.success) response.message = "Variable saved";
}

void api::log(std::string msg) {
    const char* user = std::getenv(
#if defined(_WIN32)
        "USERNAME"
#else
        "USER"
#endif
    );
    req("log", {{"message", Json(std::move(msg))},
                {"pcuser", Json(user ? user : "")},
                {"hwid", Json(hwid)}});
    if (response.success) response.message = "Logged";
}

bool api::check() {
    if (sessionid.empty()) {
        error("No active session");
        return false;
    }
    Json data = req("session/check", {});
    if (!response.success || !data["valid"].asBool()) return false;
    fill_user(data["user"], data["license"]);
    return true;
}

void api::logout() {
    req("logout", {});
    sessionid.clear();
    user_data = user_data_class{};
    if (response.success) response.message = "Logged out";
}

std::string api::request_module_token(std::string module_name, std::string key) {
    JsonObject body;
    if (!module_name.empty()) body.emplace("module", Json(std::move(module_name)));
    if (!key.empty()) body.emplace("key", Json(std::move(key)));
    Json data = req("module/token", body);
    if (!response.success) return "";
    return data["download_url"].asString();
}

void api::fetchstats() {
    Json data = req("app/data", {});
    if (!response.success) return;
    app_data.numUsers = static_cast<int>(data["stats"]["users"].asNumber());
    app_data.numKeys = static_cast<int>(data["stats"]["licenses"].asNumber());
    app_data.app_ver = data["application"]["current_version"].asString(app_data.app_ver);
    app_data.downloads.clear();
    for (const Json& d : data["downloads"].items())
        app_data.downloads.emplace_back(d["name"].asString(), d["file_url"].asString());
    response.message = "Stats fetched";
}

bool api::update_available(std::string channel) {
    Json data = req("version/check", {{"version", Json(version)},
                                      {"channel", Json(std::move(channel))}});
    return response.success && data["update_available"].asBool();
}

void api::use_session(std::string token) { sessionid = std::move(token); }

int api::expirydaysleft() const {
    if (user_data.subscriptions.empty()) return 0;
    return daysUntil(user_data.subscriptions.front().expiry);
}

bool api::has_subscription(const std::string& name) const {
    if (name.empty()) return false;
    for (const auto& sub : user_data.subscriptions) {
        if (sub.name == name || sub.subscription == name) {
            if (!sub.status.empty() && sub.status != "active") continue;
            return true;
        }
    }
    return false;
}

std::vector<api::Subscription> api::get_subscriptions() const {
    return user_data.subscriptions;
}

}  // namespace AegisAuth