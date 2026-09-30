using System;

namespace AegisAuthExample
{
    internal static class Program
    {
        // Configuration Placeholders
        private const string AuthUrl = "https://auth.example.com";
        private const string AppKey = "YOUR_APP_KEY";
        private const string AppName = "My Application";
        private const string AppVersion = "1.0.0";

        private const string Username = "YOUR_USERNAME";
        private const string Password = "YOUR_PASSWORD";
        private const string ModuleName = "minecraft";

        private static readonly AegisAuth.api AegisApp = new AegisAuth.api(
            name: AppName,
            ownerid: AppKey,
            secret: "",
            version: AppVersion,
            url: AuthUrl
        );

        private static void Main()
        {
            Console.WriteLine($"=== AegisAuth .NET Client [{AppName} v{AppVersion}] ===");

            // 1. Handshake & Version check
            Console.WriteLine("[*] Initializing application...");
            AegisApp.init();
            if (!AegisApp.response.success)
            {
                Console.WriteLine($"[-] Init failed: {AegisApp.response.message}");
                return;
            }

            Console.WriteLine("[+] Application initialized successfully.");
            Console.WriteLine($"    HWID: {AegisApp.hwid}");
            Console.WriteLine($"    Server Time: {AegisApp.app_data.server_time}");

            // 2. Login
            Console.WriteLine($"\n[*] Logging in as '{Username}'...");
            AegisApp.login(Username, Password);
            if (!AegisApp.response.success)
            {
                Console.WriteLine($"[-] Login failed: {AegisApp.response.message}");
                return;
            }

            Console.WriteLine($"[+] Logged in! Session token: {AegisApp.sessionid[..12]}...");
            Console.WriteLine($"    User: {AegisApp.user_data.username}");
            Console.WriteLine($"    Days Left: {AegisApp.expirydaysleft()}");

            // 3. Subscription Check
            if (AegisApp.HasSubscription("VIP"))
            {
                Console.WriteLine("    [Subscription] VIP tier active — Premium unlocked!");
            }
            else
            {
                Console.WriteLine("    [Subscription] Standard tier active.");
            }

            // 4. Request signed module download token for AetherVault
            Console.WriteLine($"\n[*] Requesting signed download token for module '{ModuleName}'...");
            var downloadUrl = AegisApp.RequestModuleToken(ModuleName);
            if (!string.IsNullOrEmpty(downloadUrl))
            {
                Console.WriteLine("[+] Signed Token Received!");
                Console.WriteLine($"    Download URL: {downloadUrl}");
                Console.WriteLine("\n[+] Binary payload authorized for AetherVault silent streaming!");
            }

            // 5. Logout
            Console.WriteLine("\n[*] Logging out...");
            AegisApp.logout();
            Console.WriteLine("[+] Logged out cleanly.");
        }
    }
}
