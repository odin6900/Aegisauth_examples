#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>

// Configuration Placeholders
static NSString *const AuthUrl = @"https://auth.example.com";
static NSString *const AppKey = @"YOUR_APP_KEY";
static NSString *const AppName = @"My Application";
static NSString *const AppVersion = @"1.0.0";

static NSString *const Username = @"YOUR_USERNAME";
static NSString *const Password = @"YOUR_PASSWORD";
static NSString *const ModuleName = @"minecraft";

static NSString *GetHardwareId(void) {
    NSString *host = [[NSHost currentHost] localizedName] ?: @"mac-host";
    const char *str = [host UTF8String];
    unsigned char result[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(str, (CC_LONG)strlen(str), result);
    NSMutableString *ret = [NSMutableString stringWithCapacity:CC_SHA256_DIGEST_LENGTH*2];
    for (int i = 0; i<CC_SHA256_DIGEST_LENGTH; i++) {
        [ret appendFormat:@"%02x", result[i]];
    }
    return ret;
}

static NSDictionary *PostSync(NSString *endpoint, NSDictionary *payload, NSString *sessionToken) {
    NSString *urlString = [NSString stringWithFormat:@"%@/api/public/v1/%@", [AuthUrl stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"/"]], endpoint];
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:urlString]];
    [req setHTTPMethod:@"POST"];
    [req setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [req setValue:AppKey forHTTPHeaderField:@"x-app-key"];
    [req setValue:[NSString stringWithFormat:@"%ld", (long)[[NSDate date] timeIntervalSince1970]] forHTTPHeaderField:@"x-timestamp"];
    [req setValue:[NSString stringWithFormat:@"AegisAuth-ObjC/%@", AppVersion] forHTTPHeaderField:@"User-Agent"];

    if (sessionToken && sessionToken.length > 0) {
        [req setValue:sessionToken forHTTPHeaderField:@"x-session-token"];
    }

    [req setHTTPBody:[NSJSONSerialization dataWithJSONObject:payload options:0 error:nil]];

    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSDictionary *jsonDict = nil;

    [[[NSURLSession sharedSession] dataTaskWithRequest:req completionHandler:^(NSData *data, NSURLResponse *res, NSError *err) {
        if (data) {
            jsonDict = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        }
        dispatch_semaphore_signal(sema);
    }] resume];

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    return jsonDict;
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"=== AegisAuth Objective-C Client [%@ v%@] ===", AppName, AppVersion);
        NSString *hwid = GetHardwareId();
        NSLog(@"[*] HWID: %@", hwid);

        // 1. Handshake
        NSLog(@"[*] Initializing application...");
        NSDictionary *initRes = PostSync(@"init", @{@"version": AppVersion}, nil);
        if (![initRes[@"success"] boolValue]) {
            NSLog(@"[-] Init failed: %@", initRes[@"error"][@"message"]);
            return 1;
        }
        NSLog(@"[+] Handshake successful.");

        // 2. Login
        NSLog(@"\n[*] Logging in as '%@'...", Username);
        NSDictionary *loginRes = PostSync(@"login", @{
            @"username": Username,
            @"password": Password,
            @"hwid": hwid
        }, nil);

        if (![loginRes[@"success"] boolValue]) {
            NSLog(@"[-] Login failed: %@", loginRes[@"error"][@"message"]);
            return 1;
        }

        NSString *sessionToken = loginRes[@"data"][@"token"];
        NSLog(@"[+] Logged in! Session token: %@...", [sessionToken substringToIndex:MIN(12, sessionToken.length)]);

        // 3. Request module token for AetherVault
        NSLog(@"\n[*] Requesting signed download token for module '%@'...", ModuleName);
        NSDictionary *tokenRes = PostSync(@"module/token", @{@"module": ModuleName}, sessionToken);
        if (![tokenRes[@"success"] boolValue]) {
            NSLog(@"[-] Module token request failed: %@", tokenRes[@"error"][@"message"]);
            return 1;
        }

        NSDictionary *data = tokenRes[@"data"];
        NSLog(@"[+] Signed Token Received!");
        NSLog(@"    Target Asset: %@", data[@"key"]);
        NSLog(@"    Download URL: %@", data[@"download_url"]);
        NSLog(@"    Expires In  : %@s", data[@"expires_in_seconds"]);
        NSLog(@"\n[+] Binary payload authorized for AetherVault silent streaming!");

        // 4. Logout
        NSLog(@"\n[*] Logging out...");
        PostSync(@"logout", @{}, sessionToken);
        NSLog(@"[+] Logged out cleanly.");
    }
    return 0;
}
