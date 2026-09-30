// ============================================================================
// AegisAuth — official C++ SDK for the Aegis Authentication platform
// ----------------------------------------------------------------------------
// C++17, x86 & x64, Windows / Linux / macOS. Transport: libcurl.
// JSON: bundled header-only codec (json.hpp) — no other dependencies.
//
// Quick start:
//
//   AegisAuth::api AegisApp("My Application",     // display name
//                           "YOUR-APPLICATION-KEY",// Application Key
//                           "",                    // optional API key
//                           "1.0.0");              // build version
//   AegisApp.init();
//   AegisApp.login("ada", "correct horse battery staple");
//   std::cout << AegisApp.user_data.username;
// ============================================================================
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "json.hpp"

namespace AegisAuth {

/** Default platform endpoint. Pass a custom url to the constructor for self-hosted deployments. */
constexpr const char* kDefaultBaseUrl =
    // Stable preview URL serving the latest backend build. Swap to the
    // production URL (drop the "-dev") once the site is published.
    "https://project--0e9bb1c8-e87c-49ee-832e-1356cbeaddb3-dev.lovable.app";

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

    /** Application-wide data. Populated by init/fetchstats. */
    class app_data_class {
    public:
        int numUsers = 0;
        int numKeys = 0;
        std::string app_ver;
        bool maintenance = false;
        std::string maintenance_message;
        bool hwid_required = false;
        int session_timeout_minutes = 0;
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
        std::string url = kDefaultBaseUrl);

    /** Handshake with the server. Call once before anything else; fills app_data. */
    void init();

    /** Authenticate a user and open a session. */
    void login(std::string username, std::string password);

    /** Register a new user, optionally redeeming a license key. (Named regstr: `register` is a C++ keyword.) */
    void regstr(std::string username, std::string password, std::string key = "",
                std::string email = "");

    /** Key-only authentication: validates a license key and binds it to this machine. */
    void license(std::string key);

    /** Attach a license key to an existing user account. */
    void upgrade(std::string username, std::string key);

    /** Read an application variable published from the dashboard. */
    std::string var(std::string varid);

    /** Read a per-user variable (requires an active session). */
    std::string getvar(std::string varname);

    /** Write a per-user variable (requires an active session). */
    void setvar(std::string varname, std::string value);

    /** Write a message to the application's audit log on the dashboard. */
    void log(std::string msg);

    /** Validate the active session against the server. Fills user_data. */
    bool check();

    /** Terminate the active session on the server. */
    void logout();

    /** Request an ephemeral signed download token for an AetherVault asset/module. */
    std::string request_module_token(std::string module_name, std::string key = "");

    /** Refresh app_data with live counters, versions and download links. */
    void fetchstats();

    /** True when a newer build than `version` has been published. */
    bool update_available(std::string channel = "stable");

    /** Restore a session token persisted earlier (skip the login screen). */
    void use_session(std::string token);

    /** Days remaining on the active license, or 0 when none/expired. */
    int expirydaysleft() const;

    /** Checks if the active user or license has a specific active subscription. */
    bool has_subscription(const std::string& name) const;

    /** Returns all active subscriptions for the current user and license. */
    std::vector<Subscription> get_subscriptions() const;

    /** Raw request escape hatch — returns the endpoint's `data` payload. */
    aegisauth::Json req(const std::string& endpoint, const aegisauth::JsonObject& body);

private:
    void store_auth(const aegisauth::Json& data, const char* message);
    void fill_user(const aegisauth::Json& user, const aegisauth::Json& license);
    void error(std::string message);

    static std::string hardware_id();
};

}  // namespace AegisAuth