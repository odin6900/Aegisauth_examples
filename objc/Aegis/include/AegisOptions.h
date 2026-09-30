#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Client configuration.
@interface AegisOptions : NSObject

/// Base URL of your Aegis deployment, e.g. `https://your-aegis-host`.
@property (nonatomic, copy) NSString *baseUrl;

/// Public application key from the Aegis dashboard.
@property (nonatomic, copy) NSString *appKey;

/// Optional server-side API key. Never ship this in a distributed client.
@property (nonatomic, copy, nullable) NSString *apiKey;

/// Client version reported to `init` and `version/check`. Defaults to `1.0.0`.
@property (nonatomic, copy) NSString *version;

/// Release channel used by version checks. Defaults to `stable`.
@property (nonatomic, copy) NSString *channel;

/// Overrides the automatically derived hardware id.
@property (nonatomic, copy, nullable) NSString *hwid;

/// Per-request timeout in seconds. Defaults to 20.
@property (nonatomic) NSTimeInterval timeout;

/// Retries on transport/5xx failures. Defaults to 2.
@property (nonatomic) NSUInteger maxRetries;

+ (instancetype)optionsWithBaseUrl:(NSString *)baseUrl appKey:(NSString *)appKey;

@end

NS_ASSUME_NONNULL_END