// AegisAuth quickstart — classic authentication flow.
// Replace the placeholders with your Application Key from the Aegis dashboard.
using System;

internal static class Program
{
    private static readonly AegisAuth.api AegisApp = new AegisAuth.api(
        name: "My Application",
        ownerid: "YOUR-APPLICATION-KEY", // Application Key (public key) from the dashboard
        secret: "",                      // leave empty for client-side use
        version: "1.0.0");

    private static void Main()
    {
        Console.Title = "AegisAuth Quickstart";

        AegisApp.init();
        if (!AegisApp.response.success)
        {
            Console.WriteLine("init failed: " + AegisApp.response.message);
            return;
        }

        Console.WriteLine("App version : " + AegisApp.app_data.app_ver);
        Console.WriteLine("HWID        : " + AegisApp.hwid);
        Console.WriteLine();
        Console.WriteLine("[1] Login");
        Console.WriteLine("[2] Register");
        Console.WriteLine("[3] License key only");
        Console.Write("Choice: ");

        switch (Console.ReadLine())
        {
            case "1":
                Console.Write("Username: ");
                var loginUser = Console.ReadLine();
                Console.Write("Password: ");
                AegisApp.login(loginUser, Console.ReadLine());
                break;
            case "2":
                Console.Write("Username: ");
                var regUser = Console.ReadLine();
                Console.Write("Password: ");
                var regPass = Console.ReadLine();
                Console.Write("License key (optional): ");
                var key = Console.ReadLine();
                AegisApp.register(regUser, regPass, string.IsNullOrWhiteSpace(key) ? null : key);
                break;
            default:
                Console.Write("License key: ");
                AegisApp.license(Console.ReadLine());
                break;
        }

        if (!AegisApp.response.success)
        {
            Console.WriteLine("Failed: " + AegisApp.response.message);
            return;
        }

        Console.WriteLine();
        Console.WriteLine("Welcome " + (AegisApp.user_data.username ?? "license holder"));
        Console.WriteLine("Days left: " + AegisApp.expirydaysleft());

        // Multi-subscription check example:
        if (AegisApp.HasSubscription("VIP"))
        {
            Console.WriteLine("[Subscription] VIP tier active — Premium unlocked!");
        }
        else
        {
            Console.WriteLine("[Subscription] Standard tier active.");
        }

        AegisApp.log("User opened the quickstart");
        Console.WriteLine("Server says: " + (AegisApp.var("welcome_message") ?? "(no welcome_message variable)"));

        // Keep the session alive and lock the app if it is revoked.
        var timer = new System.Timers.Timer(60_000);
        timer.Elapsed += (_, __) =>
        {
            if (!AegisApp.check())
            {
                Console.WriteLine("Session revoked — locking.");
                Environment.Exit(0);
            }
        };
        timer.Start();

        Console.WriteLine("Press any key to exit...");
        Console.ReadKey();
        AegisApp.logout();
    }
}