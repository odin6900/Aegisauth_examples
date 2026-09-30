# AegisAuth — Objective-C Example

Objective-C client using Apple Foundation and `NSURLSession`.

---

## 🚀 Building & Running

```bash
clang -fobjc-arc -framework Foundation main.m -o aegis_objc
./aegis_objc
```

---

## ⚙️ Configuration

Open `main.m` and update the constants:

```objc
static NSString *const AuthUrl = @"https://auth.example.com";
static NSString *const AppKey = @"YOUR_APP_KEY";
static NSString *const AppName = @"My Application";
static NSString *const AppVersion = @"1.0.0";

static NSString *const Username = @"YOUR_USERNAME";
static NSString *const Password = @"YOUR_PASSWORD";
static NSString *const ModuleName = @"minecraft";
```
