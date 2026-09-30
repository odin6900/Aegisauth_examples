#import <Foundation/Foundation.h>
#import "AegisError.h"
#import "AegisOptions.h"

NS_ASSUME_NONNULL_BEGIN

/// Completion handler for every asynchronous Aegis call.
/// Exactly one of `data` / `error` is non-nil.
typedef void (^AegisCompletion)(NSDictionary *_Nullable data, NSError *_Nullable error);

/// Client for the Aegis Authentication API.
/// Pure Foundation (NSURLSession + NSJSONSerialization), thread-safe.
@interface AegisClient : NSObject

/// SDK version string, reported in the User-Agent header.
@property (class, nonatomic, readonly, copy) NSString *sdkVersion;

/// Machine identifier sent with authentication and licensing calls.
@property (nonatomic, readonly, copy) NSString *hardwareId;

/// Current session token, or nil when signed out.
@property (nonatomic, readonly, copy, nullable) NSString *sessionToken;

- (instancetype)initWithOptions:(AegisOptions *)options NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Stable, non-reversible machine identifier.
+ (NSString *)hardwareIdentifier;

/// Restores a session token persisted by the host application.
- (void)useSession:(nullable NSString *)token;

/// Calls any endpoint and returns its `data` payload.
- (void)request:(NSString *)endpoint body:(nullable NSDictionary *)body completion:(AegisCompletion)completion;

#pragma mark - Application

/// Handshake. Call once before any other operation.
- (void)initializeApplication:(AegisCompletion)completion;
- (void)status:(AegisCompletion)completion;
- (void)appData:(AegisCompletion)completion;
/// Download information published for this application.
- (void)downloads:(AegisCompletion)completion;
- (void)checkVersion:(nullable NSString *)version completion:(AegisCompletion)completion;

#pragma mark - Authentication

- (void)registerUsername:(NSString *)username
                password:(NSString *)password
                   email:(nullable NSString *)email
              licenseKey:(nullable NSString *)licenseKey
              completion:(AegisCompletion)completion;

- (void)loginUsername:(NSString *)username password:(NSString *)password completion:(AegisCompletion)completion;
- (void)logout:(AegisCompletion)completion;
- (void)heartbeat:(AegisCompletion)completion;
- (void)checkSession:(AegisCompletion)completion;
/// Calls back with YES when a token exists and the server still accepts it.
- (void)isAuthenticated:(void (^)(BOOL authenticated))completion;
- (void)userData:(AegisCompletion)completion;

#pragma mark - Licensing

- (void)validateLicense:(NSString *)licenseKey completion:(AegisCompletion)completion;
- (void)activateLicense:(NSString *)licenseKey
               username:(nullable NSString *)username
             completion:(AegisCompletion)completion;

#pragma mark - Variables

- (void)variablesForScope:(nullable NSString *)scope
               licenseKey:(nullable NSString *)licenseKey
               completion:(AegisCompletion)completion;

- (void)setVariable:(NSString *)key
              value:(NSString *)value
              scope:(nullable NSString *)scope
         licenseKey:(nullable NSString *)licenseKey
         completion:(AegisCompletion)completion;

- (void)triggerWebhook:(NSString *)event
               payload:(nullable NSDictionary *)payload
            completion:(AegisCompletion)completion;

#pragma mark - Sessions

/// Starts a background heartbeat; `onRevoked` fires once when the session dies.
- (void)startHeartbeatWithInterval:(NSTimeInterval)interval onRevoked:(nullable void (^)(NSError *error))onRevoked;
- (void)stopHeartbeat;

@end

NS_ASSUME_NONNULL_END