using System;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace AegisAuthExample
{
    internal class Program
    {
        // Configuration Placeholders
        private const string AuthUrl = "https://auth.example.com";
        private const string AppKey = "YOUR_APP_KEY";
        private const string AppName = "My Application";
        private const string AppVersion = "1.0.0";

        private const string Username = "YOUR_USERNAME";
        private const string Password = "YOUR_PASSWORD";
        private const string ModuleName = "minecraft";

        private static readonly HttpClient Http = new HttpClient();
        private static string _sessionToken = string.Empty;
        private static string _hwid = string.Empty;

        private static async Task Main(string[] args)
        {
            Console.WriteLine($"=== AegisAuth .NET Client [{AppName} v{AppVersion}] ===");
            _hwid = GetHardwareId();
            Console.WriteLine($"[*] Machine HWID: {_hwid}");

            // 1. Handshake & Version check
            if (!await InitAsync()) return;

            // 2. Authenticate
            if (!await LoginAsync(Username, Password)) return;

            // 3. Request signed module download token for AetherVault
            var downloadUrl = await RequestModuleTokenAsync(ModuleName);
            if (!string.IsNullOrEmpty(downloadUrl))
            {
                Console.WriteLine("\n[+] Success! Ready to stream binary payload from AetherVault.");
            }

            // 4. Logout
            await LogoutAsync();
        }

        private static string GetHardwareId()
        {
            var seed = $"{Environment.MachineName}-{Environment.ProcessorCount}-{Environment.OSVersion}";
            using var sha = SHA256.Create();
            var hash = sha.ComputeHash(Encoding.UTF8.GetBytes(seed));
            return Convert.ToHexString(hash).ToLowerInvariant();
        }

        private static async Task<JsonDocument?> PostAsync(string endpoint, object payload)
        {
            var url = $"{AuthUrl.TrimEnd('/')}/api/public/v1/{endpoint}";
            var json = JsonSerializer.Serialize(payload);
            using var request = new HttpRequestMessage(HttpMethod.Post, url);
            request.Content = new StringContent(json, Encoding.UTF8, "application/json");

            request.Headers.Add("x-app-key", AppKey);
            request.Headers.Add("x-timestamp", DateTimeOffset.UtcNow.ToUnixTimeSeconds().ToString());
            request.Headers.Add("User-Agent", $"AegisAuth-DotNet/{AppVersion}");

            if (!string.IsNullOrEmpty(_sessionToken))
            {
                request.Headers.Add("x-session-token", _sessionToken);
            }

            try
            {
                var response = await Http.SendAsync(request);
                var body = await response.Content.ReadAsStringAsync();
                return JsonDocument.Parse(body);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[-] Network error: {ex.Message}");
                return null;
            }
        }

        private static async Task<bool> InitAsync()
        {
            Console.WriteLine("\n[*] Initializing application...");
            var doc = await PostAsync("init", new { version = AppVersion });
            if (doc == null) return false;

            var root = doc.RootElement;
            if (!root.GetProperty("success").GetBoolean())
            {
                var msg = root.GetProperty("error").GetProperty("message").GetString();
                Console.WriteLine($"[-] Init failed: {msg}");
                return false;
            }

            Console.WriteLine("[+] Application initialized successfully.");
            return true;
        }

        private static async Task<bool> LoginAsync(string user, string pass)
        {
            Console.WriteLine($"\n[*] Logging in as '{user}'...");
            var doc = await PostAsync("login", new
            {
                username = user,
                password = pass,
                hwid = _hwid
            });
            if (doc == null) return false;

            var root = doc.RootElement;
            if (!root.GetProperty("success").GetBoolean())
            {
                var msg = root.GetProperty("error").GetProperty("message").GetString();
                Console.WriteLine($"[-] Login failed: {msg}");
                return false;
            }

            var data = root.GetProperty("data");
            _sessionToken = data.GetProperty("token").GetString() ?? "";
            var license = data.GetProperty("license");

            Console.WriteLine($"[+] Login successful! Session token: {_sessionToken[..12]}...");
            Console.WriteLine($"    License Status: {license.GetProperty("status").GetString()}");
            return true;
        }

        private static async Task<string?> RequestModuleTokenAsync(string module)
        {
            Console.WriteLine($"\n[*] Requesting signed download token for module '{module}'...");
            var doc = await PostAsync("module/token", new { module });
            if (doc == null) return null;

            var root = doc.RootElement;
            if (!root.GetProperty("success").GetBoolean())
            {
                var msg = root.GetProperty("error").GetProperty("message").GetString();
                Console.WriteLine($"[-] Module token request failed: {msg}");
                return null;
            }

            var data = root.GetProperty("data");
            var key = data.GetProperty("key").GetString();
            var downloadUrl = data.GetProperty("download_url").GetString();
            var expiresIn = data.GetProperty("expires_in_seconds").GetInt32();

            Console.WriteLine("[+] Signed Token Received!");
            Console.WriteLine($"    Target Asset: {key}");
            Console.WriteLine($"    Download URL: {downloadUrl}");
            Console.WriteLine($"    Expires In  : {expiresIn}s");
            return downloadUrl;
        }

        private static async Task LogoutAsync()
        {
            if (!string.IsNullOrEmpty(_sessionToken))
            {
                Console.WriteLine("\n[*] Logging out...");
                await PostAsync("logout", new { });
                _sessionToken = string.Empty;
                Console.WriteLine("[+] Logged out cleanly.");
            }
        }
    }
}
