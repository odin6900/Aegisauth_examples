// Runnable sample:
//   clang -fobjc-arc -IAegis/include Aegis/*.m Examples/quickstart.m \
//     -framework Foundation -o quickstart && ./quickstart
#import <Foundation/Foundation.h>
#import "AegisClient.h"

static NSString *EnvOr(NSString *key, NSString *fallback) {
    NSString *value = NSProcessInfo.processInfo.environment[key];
    return value.length > 0 ? value : fallback;
}

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        AegisOptions *options = [AegisOptions optionsWithBaseUrl:EnvOr(@"AEGIS_BASE_URL", @"http://localhost:8080")
                                                          appKey:EnvOr(@"AEGIS_APP_KEY", @"")];
        options.version = @"1.0.0";
        AegisClient *aegis = [[AegisClient alloc] initWithOptions:options];

        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        __block int exitCode = 0;

        [aegis initializeApplication:^(NSDictionary *info, NSError *error) {
            if (error != nil) {
                fprintf(stderr, "Aegis error [%s] %s\n", error.aegis_code.UTF8String,
                        error.localizedDescription.UTF8String);
                exitCode = 1;
                dispatch_semaphore_signal(done);
                return;
            }
            NSLog(@"initialized: %@", info[@"status"] ?: @"ok");

            [aegis loginUsername:EnvOr(@"AEGIS_USERNAME", @"demo")
                        password:EnvOr(@"AEGIS_PASSWORD", @"demo-password")
                      completion:^(NSDictionary *auth, NSError *loginError) {
                if (loginError != nil) {
                    fprintf(stderr, "Aegis error [%s] %s\n", loginError.aegis_code.UTF8String,
                            loginError.localizedDescription.UTF8String);
                    exitCode = 1;
                    dispatch_semaphore_signal(done);
                    return;
                }
                NSLog(@"signed in as %@", auth[@"user"][@"username"]);

                [aegis startHeartbeatWithInterval:60 onRevoked:^(NSError *revoked) {
                    NSLog(@"session ended: %@", revoked.localizedDescription);
                }];

                [aegis logout:^(NSDictionary *data, NSError *logoutError) {
                    NSLog(@"signed out.");
                    dispatch_semaphore_signal(done);
                }];
            }];
        }];

        dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
        return exitCode;
    }
}