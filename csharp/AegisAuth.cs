// ============================================================================
// AegisAuth — official .NET SDK for the Aegis Authentication platform
// ----------------------------------------------------------------------------
// Single-file, zero-dependency drop-in. No NuGet packages required:
// uses only System.Net.Http and System.Runtime.Serialization.
//
// Quick start:
//
//   AegisAuth.api AegisApp = new AegisAuth.api(
//       name:    "My Application",      // your application's display name
//       ownerid: "AEGS-APP-KEY",        // Application Key from the dashboard
//       secret:  "",                    // optional API key for elevated calls
//       version: "1.0.0"                // this build's version
//   );
//
//   AegisApp.init();
//   AegisApp.login("username", "password");
//   Console.WriteLine(AegisApp.user_data.username);
//
// Works on .NET Framework 4.7.2+ and .NET 6+ (any OS).
// ============================================================================
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;

namespace AegisAuth
{
    /// <summary>
    /// AegisAuth client. Create one instance per application and keep it for
    /// the lifetime of the process.
    /// </summary>
    public sealed class api
    {
        /// <summary>Default platform endpoint. Override via the constructor for self-hosted deployments.</summary>
        // Stable preview URL serving the latest backend build. Swap to the
        // production URL (drop the "-dev") once the site is published.
        public const string DefaultBaseUrl = "https://project--0e9bb1c8-e87c-49ee-832e-1356cbeaddb3-dev.lovable.app";

        private readonly string _baseUrl;

        /// <summary>Application display name (must match the dashboard).</summary>
        public string name { get; }
        /// <summary>Application Key — the public key from your dashboard, identifies the application.</summary>
        public string ownerid { get; }
        /// <summary>Optional API key used for elevated calls (webhooks, application variable writes).</summary>
        public string secret { get; }
        /// <summary>Version of the build you are shipping, e.g. "1.0.0".</summary>
        public string version { get; }

        /// <summary>Session token for the signed-in user. Persist it and restore with <see cref="use_session"/>.</summary>
        public string sessionid { get; private set; }

        /// <summary>Hardware id of this machine, sent with auth and license calls.</summary>
        public string hwid { get; }

        /// <summary>Data about the signed-in user. Populated by login/register/license/check.</summary>
        public user_data_class user_data { get; } = new user_data_class();

        /// <summary>Application-wide data. Populated by init/fetchstats.</summary>
        public app_data_class app_data { get; } = new app_data_class();

        /// <summary>Outcome of the most recent call. Check <c>response.success</c> after every method.</summary>
        public response_class response { get; } = new response_class();

        private static readonly HttpClient _http = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };

        /// <summary>
        /// Creates the client.
        /// </summary>
        /// <param name="name">Application display name.</param>
        /// <param name="ownerid">Application Key (public key) from the Aegis dashboard.</param>
        /// <param name="secret">Optional API key; leave empty for standard client use.</param>
        /// <param name="version">Client version string, e.g. "1.0.0".</param>
        /// <param name="url">Optional custom API base URL (self-hosted deployments).</param>
        public api(string name, string ownerid, string secret, string version, string url = DefaultBaseUrl)
        {
            if (string.IsNullOrWhiteSpace(name)) throw new ArgumentException("name is required", nameof(name));
            if (string.IsNullOrWhiteSpace(ownerid)) throw new ArgumentException("ownerid (Application Key) is required", nameof(ownerid));

            this.name = name;
            this.ownerid = ownerid;
            this.secret = secret ?? string.Empty;
            this.version = version ?? "1.0.0";
            _baseUrl = (url ?? DefaultBaseUrl).TrimEnd('/') + "/api/public/v1/";
            hwid = HardwareId.Current;
        }

        /* ================================================================== */
        /* Core                                                                */
        /* ================================================================== */

        /// <summary>Handshake with the server. Call once before anything else; fills app_data.</summary>
        public void init()
        {
            var data = req<InitData>("init", new Dictionary<string, string> { ["version"] = version });
            if (!response.success || data == null) return;

            app_data.app_ver = data.Version != null ? data.Version.Current : version;
            app_data.maintenance = data.Maintenance;
            app_data.maintenance_message = data.MaintenanceMessage;
            app_data.hwid_required = data.HwidRequired;
            app_data.session_timeout_minutes = data.SessionTimeoutMinutes;
            app_data.server_time = data.ServerTime;
            if (data.Version != null && data.Version.UpdateRequired)
            {
                response.success = false;
                response.message = "An update is required before you can use this build.";
                return;
            }
            response.message = "Initialized " + name;
        }

        /// <summary>Registers a new user. Optionally redeems a license key in the same call.</summary>
        public void register(string username, string pass, string key = null, string email = null)
        {
            var body = AuthBody(username, pass);
            if (key != null) body["license_key"] = key;
            if (email != null) body["email"] = email;
            var data = req<AuthData>("register", body);
            if (response.success) StoreAuth(data, "Registered successfully");
        }

        /// <summary>Authenticates a user and opens a session.</summary>
        public void login(string username, string pass)
        {
            var data = req<AuthData>("login", AuthBody(username, pass));
            if (response.success) StoreAuth(data, "Logged in successfully");
        }

        /// <summary>
        /// Key-only authentication: validates a license key and binds it to this machine.
        /// </summary>
        public void license(string key)
        {
            var check = req<LicenseData>("license/validate", new Dictionary<string, string>
            {
                ["license_key"] = key,
                ["hwid"] = hwid,
            });
            if (!response.success) return;

            var activation = req<ActivateData>("license/activate", new Dictionary<string, string>
            {
                ["license_key"] = key,
                ["hwid"] = hwid,
            });
            if (!response.success) return;

            var lic = (activation != null ? activation.License : null) ?? (check != null ? check.License : null);
            user_data.username = null;
            user_data.hwid = hwid;
            user_data.subscriptions.Clear();
            if (lic != null)
            {
                if (lic.Subscriptions != null)
                {
                    foreach (var s in lic.Subscriptions)
                        if (s != null) user_data.subscriptions.Add(Subscription.FromWire(s));
                }
                user_data.subscriptions.Add(Subscription.From(lic));
            }
            response.message = "License activated";
        }

        /// <summary>Attaches a license key to an existing user account.</summary>
        public void upgrade(string username, string key)
        {
            req<ActivateData>("license/activate", new Dictionary<string, string>
            {
                ["license_key"] = key,
                ["hwid"] = hwid,
                ["username"] = username,
            });
            if (response.success) response.message = "License attached to " + username;
        }

        /// <summary>Reads an application variable published from the dashboard.</summary>
        public string var(string varid)
        {
            var data = req<VarsData>("variables/get", new Dictionary<string, string> { ["scope"] = "application" });
            if (!response.success || data == null) return null;
            string value;
            if (data.Variables != null && data.Variables.TryGetValue(varid, out value)) return value;
            response.success = false;
            response.message = "Variable not found: " + varid;
            return null;
        }

        /// <summary>Reads a per-user variable (requires an active session).</summary>
        public string getvar(string varname)
        {
            var data = req<VarsData>("variables/get", new Dictionary<string, string> { ["scope"] = "user" });
            if (!response.success || data == null) return null;
            string value;
            return data.Variables != null && data.Variables.TryGetValue(varname, out value) ? value : null;
        }

        /// <summary>Writes a per-user variable (requires an active session).</summary>
        public void setvar(string varname, string data)
        {
            req<VarSetData>("variables/set", new Dictionary<string, string>
            {
                ["scope"] = "user",
                ["key"] = varname,
                ["value"] = data ?? string.Empty,
            });
            if (response.success) response.message = "Variable saved";
        }

        /// <summary>Writes a message to the application's audit log on the dashboard.</summary>
        public void log(string message)
        {
            req<LogData>("log", new Dictionary<string, string>
            {
                ["message"] = message ?? string.Empty,
                ["pcuser"] = Environment.UserName,
                ["hwid"] = hwid,
            });
            if (response.success) response.message = "Logged";
        }

        /// <summary>Validates the active session against the server. Fills user_data.</summary>
        public bool check()
        {
            if (string.IsNullOrEmpty(sessionid))
            {
                response.success = false;
                response.message = "No active session";
                return false;
            }
            var data = req<CheckData>("session/check", new Dictionary<string, string>());
            if (!response.success || data == null || !data.Valid) return false;
            FillUser(data.User, data.License);
            return true;
        }

        /// <summary>Terminates the active session on the server.</summary>
        public void logout()
        {
            req<LogoutData>("logout", new Dictionary<string, string>());
            sessionid = null;
            user_data.Clear();
            if (response.success) response.message = "Logged out";
        }

        /// <summary>Request an ephemeral signed download token for an AetherVault asset/module.</summary>
        public string RequestModuleToken(string module, string key = null)
        {
            var dict = new Dictionary<string, string>();
            if (!string.IsNullOrEmpty(module)) dict["module"] = module;
            if (!string.IsNullOrEmpty(key)) dict["key"] = key;

            var data = req<ModuleTokenData>("module/token", dict);
            return (response.success && data != null) ? data.DownloadUrl : null;
        }

        /// <summary>Refreshes app_data with live counters, versions and download links.</summary>
        public void fetchstats()
        {
            var data = req<AppData>("app/data", new Dictionary<string, string>());
            if (!response.success || data == null) return;
            app_data.numUsers = data.Stats != null ? data.Stats.Users : 0;
            app_data.numKeys = data.Stats != null ? data.Stats.Licenses : 0;
            if (data.Application != null) app_data.app_ver = data.Application.CurrentVersion;
            app_data.downloads.Clear();
            if (data.Downloads != null)
                foreach (var d in data.Downloads)
                    if (d != null) app_data.downloads.Add(new app_data_class.download { name = d.Name, url = d.FileUrl });
            response.message = "Stats fetched";
        }

        /// <summary>Compares this build against the latest published version.</summary>
        public bool update_available(string channel = "stable")
        {
            var data = req<VersionCheckData>("version/check", new Dictionary<string, string>
            {
                ["version"] = version,
                ["channel"] = channel,
            });
            return response.success && data != null && data.UpdateAvailable;
        }

        /// <summary>Restores a session token persisted earlier (skip the login screen).</summary>
        public void use_session(string token) { sessionid = token; }

        /// <summary>Days remaining on the active license, or "0" when none/expired.</summary>
        public string expirydaysleft()
        {
            if (user_data.subscriptions.Count == 0) return "0";
            var expiry = user_data.subscriptions[0].expiry;
            if (!expiry.HasValue) return "0";
            var left = expiry.Value - DateTime.UtcNow;
            return Math.Max(0, (int)Math.Ceiling(left.TotalDays)).ToString();
        }

        /// <summary>Checks if the active user or license holds a specific active subscription (case-insensitive).</summary>
        public bool has_subscription(string name)
        {
            if (string.IsNullOrWhiteSpace(name)) return false;
            foreach (var sub in user_data.subscriptions)
            {
                if (string.Equals(sub.name, name, StringComparison.OrdinalIgnoreCase) ||
                    string.Equals(sub.subscription, name, StringComparison.OrdinalIgnoreCase))
                {
                    if (!string.IsNullOrEmpty(sub.status) && !sub.status.Equals("active", StringComparison.OrdinalIgnoreCase))
                        continue;
                    if (sub.expiry.HasValue && sub.expiry.Value <= DateTime.UtcNow)
                        continue;
                    return true;
                }
            }
            return false;
        }

        /// <summary>Alias for has_subscription with PascalCase naming convention.</summary>
        public bool HasSubscription(string name) => has_subscription(name);

        /// <summary>Returns all active subscriptions for the current user/license.</summary>
        public List<Subscription> get_subscriptions() => new List<Subscription>(user_data.subscriptions);

        /// <summary>Alias for get_subscriptions with PascalCase naming convention.</summary>
        public List<Subscription> GetSubscriptions() => get_subscriptions();

        /* ================================================================== */
        /* Transport                                                           */
        /* ================================================================== */

        private Dictionary<string, string> AuthBody(string username, string pass)
        {
            return new Dictionary<string, string>
            {
                ["username"] = username,
                ["password"] = pass,
                ["hwid"] = hwid,
            };
        }

        private void StoreAuth(AuthData data, string message)
        {
            if (data != null && data.Session != null) sessionid = data.Session.Token;
            if (data != null) FillUser(data.User, data.License);
            response.message = message;
        }

        private void FillUser(UserInfo user, LicenseInfo license)
        {
            if (user != null)
            {
                user_data.username = user.Username;
                user_data.email = user.Email;
                user_data.status = user.Status;
                user_data.hwid = user.Hwid;
                user_data.createdate = ParseDate(user.CreatedAt);
                user_data.lastlogin = ParseDate(user.LastLoginAt);
                user_data.expires_at = ParseDate(user.ExpiresAt);
                user_data.logincount = user.LoginCount;
            }
            user_data.subscriptions.Clear();
            if (user != null && user.Subscriptions != null)
            {
                foreach (var s in user.Subscriptions)
                    if (s != null) user_data.subscriptions.Add(Subscription.FromWire(s));
            }
            if (license != null)
            {
                if (license.Subscriptions != null)
                {
                    foreach (var s in license.Subscriptions)
                        if (s != null && !user_data.subscriptions.Exists(x => string.Equals(x.name, s.Name, StringComparison.OrdinalIgnoreCase)))
                            user_data.subscriptions.Add(Subscription.FromWire(s));
                }
                user_data.subscriptions.Add(Subscription.From(license));
            }
        }

        private static DateTime? ParseDate(string iso)
        {
            DateTime parsed;
            return DateTime.TryParse(iso, null, System.Globalization.DateTimeStyles.RoundtripKind, out parsed)
                ? (DateTime?)parsed.ToUniversalTime()
                : null;
        }

        /// <summary>Calls an endpoint, normalises the envelope, and fills <see cref="response"/>.</summary>
        private T req<T>(string endpoint, Dictionary<string, string> body) where T : class, new()
        {
            int statusCode = 0;
            string statusText = string.Empty;
            string text = null;
            try
            {
                var request = new HttpRequestMessage(HttpMethod.Post, _baseUrl + endpoint);
                request.Content = new StringContent(Json.WriteFlat(body), Encoding.UTF8, "application/json");
                request.Headers.TryAddWithoutValidation("x-app-key", ownerid);
                request.Headers.TryAddWithoutValidation("x-app-name", name);
                if (!string.IsNullOrEmpty(secret)) request.Headers.TryAddWithoutValidation("x-api-key", secret);
                if (!string.IsNullOrEmpty(sessionid)) request.Headers.TryAddWithoutValidation("x-session-token", sessionid);
                request.Headers.TryAddWithoutValidation("x-timestamp", DateTimeOffset.UtcNow.ToUnixTimeSeconds().ToString());
                request.Headers.TryAddWithoutValidation("user-agent", "aegisauth-csharp/1.0.0");

                var httpResponse = Task.Run(() => _http.SendAsync(request)).GetAwaiter().GetResult();
                statusCode = (int)httpResponse.StatusCode;
                statusText = httpResponse.ReasonPhrase ?? httpResponse.StatusCode.ToString();
                text = httpResponse.Content.ReadAsStringAsync().GetAwaiter().GetResult();
            }
            catch (Exception ex)
            {
                response.success = false;
                response.message = "Network error: " + ex.Message;
                return null;
            }

            var envelope = Json.Read<Envelope<T>>(text);
            if (envelope != null && envelope.Success)
            {
                response.success = true;
                response.message = "Success";
                return envelope.Data ?? new T();
            }

            // Error path — surface the most descriptive message available:
            //  1. the standard envelope's error.message
            //  2. fallback JSON fields: root["message"] or a plain-string root["error"]
            //  3. the exact HTTP status code plus the server's raw response text
            string message = null;
            if (envelope != null && envelope.Error != null && !string.IsNullOrWhiteSpace(envelope.Error.Message))
                message = envelope.Error.Message;
            if (string.IsNullOrWhiteSpace(message))
                message = Json.ExtractString(text, "message");
            if (string.IsNullOrWhiteSpace(message))
                message = Json.ExtractString(text, "error");
            if (string.IsNullOrWhiteSpace(message) && (statusCode < 200 || statusCode >= 300))
                message = "HTTP " + statusCode + " " + statusText +
                    (string.IsNullOrWhiteSpace(text) ? string.Empty : ": " + Json.Truncate(text.Trim(), 300));
            if (string.IsNullOrWhiteSpace(message))
                message = "Request failed (HTTP " + statusCode + " " + statusText + ").";

            response.success = false;
            response.message = message;
            return null;
        }

        /// <summary>Records an error in <see cref="response"/> and mirrors it to the console/debugger.</summary>
        public void error(string message)
        {
            response.success = false;
            response.message = message;
            Debug.WriteLine("[AegisAuth] " + message);
            Console.Error.WriteLine("[AegisAuth] " + message);
        }

        /* ================================================================== */
        /* Public data models                                                  */
        /* ================================================================== */

        public sealed class response_class
        {
            public bool success { get; set; }
            public string message { get; set; }
        }

        public sealed class user_data_class
        {
            public string username { get; set; }
            public string email { get; set; }
            public string status { get; set; }
            public string hwid { get; set; }
            public DateTime? createdate { get; set; }
            public DateTime? lastlogin { get; set; }
            public DateTime? expires_at { get; set; }
            public int logincount { get; set; }
            public List<Subscription> subscriptions { get; } = new List<Subscription>();

            internal void Clear()
            {
                username = email = status = hwid = null;
                createdate = lastlogin = expires_at = null;
                logincount = 0;
                subscriptions.Clear();
            }
        }

        public sealed class Subscription
        {
            public string subscription { get; set; }   // license key or subscription name
            public string name { get; set; }           // subscription name
            public string status { get; set; }
            public DateTime? expiry { get; set; }
            public TimeSpan timeleft { get { return expiry.HasValue ? expiry.Value - DateTime.UtcNow : TimeSpan.Zero; } }

            internal static Subscription From(LicenseInfo lic)
            {
                return new Subscription
                {
                    subscription = lic.Key,
                    name = lic.Key,
                    status = lic.Status,
                    expiry = ParseDate(lic.ExpiresAt),
                };
            }

            internal static Subscription FromWire(UserSubscriptionWire wire)
            {
                return new Subscription
                {
                    subscription = wire.Name,
                    name = wire.Name,
                    status = wire.Status,
                    expiry = ParseDate(wire.ExpiresAt),
                };
            }
        }

        public sealed class app_data_class
        {
            public int numUsers { get; set; }
            public int numKeys { get; set; }
            public string app_ver { get; set; }
            public bool maintenance { get; set; }
            public string maintenance_message { get; set; }
            public bool hwid_required { get; set; }
            public int session_timeout_minutes { get; set; }
            public string server_time { get; set; }
            public List<download> downloads { get; } = new List<download>();

            public sealed class download
            {
                public string name { get; set; }
                public string url { get; set; }
            }
        }

        /* ================================================================== */
        /* Wire contracts (DataContractJsonSerializer — no external JSON lib)  */
        /* ================================================================== */

        [DataContract] internal sealed class ErrorInfo
        {
            [DataMember(Name = "code")] public string Code { get; set; }
            [DataMember(Name = "message")] public string Message { get; set; }
        }

        [DataContract] internal sealed class Envelope<T>
        {
            [DataMember(Name = "success")] public bool Success { get; set; }
            [DataMember(Name = "data")] public T Data { get; set; }
            [DataMember(Name = "error")] public ErrorInfo Error { get; set; }
            [DataMember(Name = "timestamp")] public string Timestamp { get; set; }
        }

        [DataContract] internal sealed class AppInfo
        {
            [DataMember(Name = "id")] public string Id { get; set; }
            [DataMember(Name = "name")] public string Name { get; set; }
            [DataMember(Name = "internal_name")] public string InternalName { get; set; }
            [DataMember(Name = "environment")] public string Environment { get; set; }
            [DataMember(Name = "status")] public string Status { get; set; }
            [DataMember(Name = "current_version")] public string CurrentVersion { get; set; }
            [DataMember(Name = "minimum_version")] public string MinimumVersion { get; set; }
        }

        [DataContract] internal sealed class VersionInfo
        {
            [DataMember(Name = "current")] public string Current { get; set; }
            [DataMember(Name = "minimum")] public string Minimum { get; set; }
            [DataMember(Name = "client")] public string Client { get; set; }
            [DataMember(Name = "update_required")] public bool UpdateRequired { get; set; }
        }

        [DataContract] internal sealed class InitData
        {
            [DataMember(Name = "application")] public AppInfo Application { get; set; }
            [DataMember(Name = "version")] public VersionInfo Version { get; set; }
            [DataMember(Name = "maintenance")] public bool Maintenance { get; set; }
            [DataMember(Name = "maintenance_message")] public string MaintenanceMessage { get; set; }
            [DataMember(Name = "hwid_required")] public bool HwidRequired { get; set; }
            [DataMember(Name = "session_timeout_minutes")] public int SessionTimeoutMinutes { get; set; }
            [DataMember(Name = "server_time")] public string ServerTime { get; set; }
        }

        [DataContract] internal sealed class UserSubscriptionWire
        {
            [DataMember(Name = "name")] public string Name { get; set; }
            [DataMember(Name = "status")] public string Status { get; set; }
            [DataMember(Name = "expires_at")] public string ExpiresAt { get; set; }
            [DataMember(Name = "duration_days")] public int? DurationDays { get; set; }
            [DataMember(Name = "duration_minutes")] public int? DurationMinutes { get; set; }
        }

        [DataContract] internal sealed class UserInfo
        {
            [DataMember(Name = "id")] public string Id { get; set; }
            [DataMember(Name = "username")] public string Username { get; set; }
            [DataMember(Name = "email")] public string Email { get; set; }
            [DataMember(Name = "status")] public string Status { get; set; }
            [DataMember(Name = "hwid")] public string Hwid { get; set; }
            [DataMember(Name = "created_at")] public string CreatedAt { get; set; }
            [DataMember(Name = "last_login_at")] public string LastLoginAt { get; set; }
            [DataMember(Name = "login_count")] public int LoginCount { get; set; }
            [DataMember(Name = "expires_at")] public string ExpiresAt { get; set; }
            [DataMember(Name = "subscriptions")] public List<UserSubscriptionWire> Subscriptions { get; set; }
        }

        [DataContract] internal sealed class LicenseInfo
        {
            [DataMember(Name = "key")] public string Key { get; set; }
            [DataMember(Name = "status")] public string Status { get; set; }
            [DataMember(Name = "created_at")] public string CreatedAt { get; set; }
            [DataMember(Name = "activated_at")] public string ActivatedAt { get; set; }
            [DataMember(Name = "expires_at")] public string ExpiresAt { get; set; }
            [DataMember(Name = "duration_days")] public int DurationDays { get; set; }
            [DataMember(Name = "duration_minutes")] public int? DurationMinutes { get; set; }
            [DataMember(Name = "hwid_lock")] public bool HwidLock { get; set; }
            [DataMember(Name = "max_activations")] public int MaxActivations { get; set; }
            [DataMember(Name = "current_activations")] public int CurrentActivations { get; set; }
            [DataMember(Name = "tags")] public List<string> Tags { get; set; }
            [DataMember(Name = "subscription_id")] public string SubscriptionId { get; set; }
            [DataMember(Name = "subscriptions")] public List<UserSubscriptionWire> Subscriptions { get; set; }
        }

        [DataContract] internal sealed class SessionInfo
        {
            [DataMember(Name = "token")] public string Token { get; set; }
            [DataMember(Name = "expires_at")] public string ExpiresAt { get; set; }
        }

        [DataContract] internal sealed class AuthData
        {
            [DataMember(Name = "user")] public UserInfo User { get; set; }
            [DataMember(Name = "license")] public LicenseInfo License { get; set; }
            [DataMember(Name = "session")] public SessionInfo Session { get; set; }
        }

        [DataContract] internal sealed class LicenseData
        {
            [DataMember(Name = "valid")] public bool Valid { get; set; }
            [DataMember(Name = "license")] public LicenseInfo License { get; set; }
            [DataMember(Name = "variables")] public Dictionary<string, string> Variables { get; set; }
        }

        [DataContract] internal sealed class ActivateData
        {
            [DataMember(Name = "activated")] public bool Activated { get; set; }
            [DataMember(Name = "license")] public LicenseInfo License { get; set; }
        }

        [DataContract] internal sealed class VarsData
        {
            [DataMember(Name = "scope")] public string Scope { get; set; }
            [DataMember(Name = "variables")] public Dictionary<string, string> Variables { get; set; }
        }

        [DataContract] internal sealed class VarSetData
        {
            [DataMember(Name = "scope")] public string Scope { get; set; }
            [DataMember(Name = "key")] public string Key { get; set; }
            [DataMember(Name = "saved")] public bool Saved { get; set; }
        }

        [DataContract] internal sealed class CheckData
        {
            [DataMember(Name = "valid")] public bool Valid { get; set; }
            [DataMember(Name = "user")] public UserInfo User { get; set; }
            [DataMember(Name = "license")] public LicenseInfo License { get; set; }
            [DataMember(Name = "expires_at")] public string ExpiresAt { get; set; }
        }

        [DataContract] internal sealed class LogoutData
        {
            [DataMember(Name = "terminated")] public bool Terminated { get; set; }
        }

        [DataContract] internal sealed class LogData
        {
            [DataMember(Name = "logged")] public bool Logged { get; set; }
        }

        [DataContract] internal sealed class ModuleTokenData
        {
            [DataMember(Name = "key")] public string Key { get; set; }
            [DataMember(Name = "download_url")] public string DownloadUrl { get; set; }
            [DataMember(Name = "expires_in_seconds")] public int ExpiresInSeconds { get; set; }
        }

        [DataContract] internal sealed class StatsData
        {
            [DataMember(Name = "users")] public int Users { get; set; }
            [DataMember(Name = "licenses")] public int Licenses { get; set; }
        }

        [DataContract] internal sealed class DownloadInfo
        {
            [DataMember(Name = "id")] public string Id { get; set; }
            [DataMember(Name = "name")] public string Name { get; set; }
            [DataMember(Name = "kind")] public string Kind { get; set; }
            [DataMember(Name = "file_url")] public string FileUrl { get; set; }
            [DataMember(Name = "checksum")] public string Checksum { get; set; }
            [DataMember(Name = "size_bytes")] public long SizeBytes { get; set; }
        }

        [DataContract] internal sealed class AppData
        {
            [DataMember(Name = "application")] public AppInfo Application { get; set; }
            [DataMember(Name = "stats")] public StatsData Stats { get; set; }
            [DataMember(Name = "downloads")] public List<DownloadInfo> Downloads { get; set; }
        }

        [DataContract] internal sealed class VersionCheckData
        {
            [DataMember(Name = "client_version")] public string ClientVersion { get; set; }
            [DataMember(Name = "latest_version")] public string LatestVersion { get; set; }
            [DataMember(Name = "minimum_version")] public string MinimumVersion { get; set; }
            [DataMember(Name = "update_available")] public bool UpdateAvailable { get; set; }
            [DataMember(Name = "update_required")] public bool UpdateRequired { get; set; }
            [DataMember(Name = "release_notes")] public string ReleaseNotes { get; set; }
            [DataMember(Name = "maintenance")] public bool Maintenance { get; set; }
        }
    }

    /* ====================================================================== */
    /* Hardware id                                                             */
    /* ====================================================================== */

    /// <summary>Stable per-machine identifier (SHA-256 hashed; the raw machine id never leaves the device).</summary>
    public static class HardwareId
    {
        private static string _current;

        public static string Current { get { return _current ?? (_current = Compute()); } }

        private static string Compute()
        {
            string seed;
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                try
                {
                    var identity = System.Security.Principal.WindowsIdentity.GetCurrent();
                    seed = identity != null && identity.User != null ? identity.User.Value : Environment.MachineName;
                }
                catch
                {
                    seed = Environment.MachineName;
                }
            }
            else
            {
                seed = string.Join("|", Environment.MachineName, Environment.UserName,
                    RuntimeInformation.OSDescription);
            }

            using (var sha = SHA256.Create())
            {
                var hash = sha.ComputeHash(Encoding.UTF8.GetBytes("aegisauth:" + seed));
                var builder = new StringBuilder(hash.Length * 2);
                foreach (var b in hash) builder.Append(b.ToString("x2"));
                return builder.ToString();
            }
        }
    }

    /* ====================================================================== */
    /* Minimal JSON helpers (flat request bodies + DCJS responses)             */
    /* ====================================================================== */

    internal static class Json
    {
        /// <summary>Serializes a flat string→string body. All API request bodies are flat.</summary>
        public static string WriteFlat(Dictionary<string, string> map)
        {
            var builder = new StringBuilder("{");
            var first = true;
            foreach (var pair in map)
            {
                if (pair.Value == null) continue;
                if (!first) builder.Append(',');
                first = false;
                builder.Append('\"').Append(Escape(pair.Key)).Append("\":\"").Append(Escape(pair.Value)).Append('\"');
            }
            return builder.Append('}').ToString();
        }

        private static string Escape(string value)
        {
            var builder = new StringBuilder(value.Length + 8);
            foreach (var c in value)
            {
                switch (c)
                {
                    case '\"': builder.Append("\\\""); break;
                    case '\\': builder.Append("\\\\"); break;
                    case '\b': builder.Append("\\b"); break;
                    case '\f': builder.Append("\\f"); break;
                    case '\n': builder.Append("\\n"); break;
                    case '\r': builder.Append("\\r"); break;
                    case '\t': builder.Append("\\t"); break;
                    default:
                        if (c < ' ') builder.Append("\\u").Append(((int)c).ToString("x4"));
                        else builder.Append(c);
                        break;
                }
            }
            return builder.ToString();
        }

        public static T Read<T>(string json) where T : class
        {
            if (string.IsNullOrWhiteSpace(json)) return null;
            try
            {
                var serializer = new DataContractJsonSerializer(typeof(T));
                using (var stream = new MemoryStream(Encoding.UTF8.GetBytes(json)))
                    return serializer.ReadObject(stream) as T;
            }
            catch
            {
                return null;
            }
        }

        /// <summary>
        /// Extracts a string field from a JSON document without a full parse.
        /// Used as a fallback when the standard envelope cannot be deserialized
        /// (e.g. "error" returned as a plain string, or a non-DCJS shape).
        /// </summary>
        public static string ExtractString(string json, string key)
        {
            if (string.IsNullOrEmpty(json)) return null;
            var match = System.Text.RegularExpressions.Regex.Match(
                json,
                "\"" + System.Text.RegularExpressions.Regex.Escape(key) + "\"\\s*:\\s*\"((?:[^\"\\\\]|\\\\.)*)\"",
                System.Text.RegularExpressions.RegexOptions.IgnoreCase);
            return match.Success ? Unescape(match.Groups[1].Value) : null;
        }

        /// <summary>Truncates raw response text so error messages stay readable.</summary>
        public static string Truncate(string value, int max)
        {
            if (string.IsNullOrEmpty(value) || value.Length <= max) return value;
            return value.Substring(0, max) + "…";
        }

        private static string Unescape(string value)
        {
            var builder = new StringBuilder(value.Length);
            for (var i = 0; i < value.Length; i++)
            {
                var c = value[i];
                if (c != '\\' || i + 1 >= value.Length)
                {
                    builder.Append(c);
                    continue;
                }
                var n = value[++i];
                switch (n)
                {
                    case '"': builder.Append('"'); break;
                    case '\\': builder.Append('\\'); break;
                    case '/': builder.Append('/'); break;
                    case 'b': builder.Append('\b'); break;
                    case 'f': builder.Append('\f'); break;
                    case 'n': builder.Append('\n'); break;
                    case 'r': builder.Append('\r'); break;
                    case 't': builder.Append('\t'); break;
                    case 'u':
                        if (i + 4 < value.Length)
                        {
                            builder.Append((char)Convert.ToInt32(value.Substring(i + 1, 4), 16));
                            i += 4;
                        }
                        break;
                    default: builder.Append(n); break;
                }
            }
            return builder.ToString();
        }
    }
}