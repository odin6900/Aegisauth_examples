# AegisAuth — C Example

Pure C11 implementation for **AegisAuth** using Windows native `WinINet`.

Ideal for compact Windows loaders, hooks, stub DLLs, and embedded C clients.

---

## 🚀 Building & Running

### Using MSVC (`cl.exe`)
```powershell
cl /O2 main.c /link wininet.lib
.\main.exe
```

### Using MinGW (`gcc`)
```powershell
gcc -O3 main.c -lwininet -o aegis_c.exe
.\aegis_c.exe
```

---

## ⚙️ Configuration

Open `main.c` and update the constants:

```c
#define AUTH_HOST    "auth.example.com"
#define APP_KEY      "YOUR_APP_KEY"
#define APP_NAME     "My Application"
#define APP_VERSION  "1.0.0"

#define USERNAME     "YOUR_USERNAME"
#define PASSWORD     "YOUR_PASSWORD"
#define MODULE_NAME  "minecraft"
```
