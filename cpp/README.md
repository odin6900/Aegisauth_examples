# AegisAuth — C++ Example

Zero-dependency native C++ client for **AegisAuth** using Windows `WinINet` and `wincrypt`.

Ideal for native Windows client loaders, injectors, games, and protected applications.

---

## 🚀 Building & Running

### Using MSVC (`cl.exe`)
```powershell
cl /EHsc /O2 main.cpp /link wininet.lib advapi32.lib
.\main.exe
```

### Using MinGW (`g++`)
```powershell
g++ -O3 main.cpp -lwininet -ladvapi32 -o aegis_loader.exe
.\aegis_loader.exe
```

---

## ⚙️ Configuration

Open `main.cpp` and update the constants:

```cpp
const std::string AUTH_HOST   = "auth.example.com"; // AegisAuth domain
const std::string APP_KEY     = "YOUR_APP_KEY";     // Application Key from dashboard
const std::string APP_NAME    = "My Application";
const std::string APP_VERSION = "1.0.0";

const std::string USERNAME    = "YOUR_USERNAME";
const std::string PASSWORD    = "YOUR_PASSWORD";
const std::string MODULE_NAME = "minecraft";        // Game module to request token for
```
