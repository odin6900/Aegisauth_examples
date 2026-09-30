# AegisAuth — C# / .NET Example

Full-featured .NET client for **AegisAuth** demonstrating:
1. Application initialization & version verification
2. Platform hardware ID generation (HWID)
3. User login and active subscription checks
4. Requesting ephemeral signed download tokens for **AetherVault**
5. Session termination

Uses built-in `System.Net.Http.HttpClient` and `System.Text.Json` (zero external NuGet packages required).

---

## 🚀 Running the Example

```bash
dotnet run
```

---

## ⚙️ Configuration

Open `Program.cs` and replace the placeholder constants:

```csharp
private const string AuthUrl = "https://auth.example.com";
private const string AppKey = "YOUR_APP_KEY";
private const string AppName = "My Application";
private const string AppVersion = "1.0.0";

private const string Username = "YOUR_USERNAME";
private const string Password = "YOUR_PASSWORD";
private const string ModuleName = "minecraft";
```
