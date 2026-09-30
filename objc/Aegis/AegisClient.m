#import "AegisClient.h"
#import <CommonCrypto/CommonDigest.h>

@interface AegisClient ()
@property (nonatomic, strong) AegisOptions *options;
@property (nonatomic, copy) NSString *baseUrl;
@property (nonatomic, strong) NSURLSession *session;
@property (nonatomic, strong) dispatch_source_t heartbeatTimer;
@property (nonatomic, strong) dispatch_queue_t stateQueue;
@property (nonatomic, copy, nullable) NSString *token;
@end

@implementation AegisClient

+ (NSString *)sdkVersion {
    return @"1.0.0";
}

- (instancetype)initWithOptions:(AegisOptions *)options {
    NSParameterAssert(options.baseUrl.length > 0);
    NSParameterAssert(options.appKey.length > 0);

    if ((self = [super init])) {
        _options = options;
        _baseUrl = [options.baseUrl hasSuffix:@"/"] ? [options.baseUrl substringToIndex:options.baseUrl.length - 1]
                                                    : options.baseUrl;
        _hardwareId = options.hwid.length > 0 ? [options.hwid copy] : [AegisClient hardwareIdentifier];
        _stateQueue = dispatch_queue_create("io.aegis.sdk.state", DISPATCH_QUEUE_SERIAL);

        NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration defaultSessionConfiguration];
        configuration.timeoutIntervalForRequest = options.timeout;
        _session = [NSURLSession sessionWithConfiguration:configuration];
    }
    return self;
}

+ (NSString *)hardwareIdentifier {
    NSProcessInfo *info = [NSProcessInfo processInfo];
    NSString *facts = [@[
        info.hostName ?: @"unknown",
        info.operatingSystemVersionString ?: @"",
        @(info.processorCount).stringValue,
        info.environment[@"USER"] ?: @"",
    ] componentsJoinedByString:@"|"];

    NSData *data = [facts dataUsingEncoding:NSUTF8StringEncoding];
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(data.bytes, (CC_LONG)data.length, digest);

    NSMutableString *hex = [NSMutableString stringWithCapacity:CC_SHA256_DIGEST_LENGTH * 2];
    for (int i = 0; i < CC_SHA256_DIGEST_LENGTH; i++) {
        [hex appendFormat:@"%02x", digest[i]];
    }
    return hex;
}

- (nullable NSString *)sessionToken {
    __block NSString *current = nil;
    dispatch_sync(self.stateQueue, ^{ current = self.token; });
    return current;
}

- (void)useSession:(nullable NSString *)token {
    dispatch_async(self.stateQueue, ^{ self.token = [token copy]; });
}

#pragma mark - Transport

- (NSError *)errorWithCode:(NSString *)code message:(NSString *)message status:(NSInteger)status {
    return [NSError errorWithDomain:AegisErrorDomain
                               code:status
                           userInfo:@{ AegisErrorCodeKey: code, NSLocalizedDescriptionKey: message }];
}

- (void)request:(NSString *)endpoint body:(nullable NSDictionary *)body completion:(AegisCompletion)completion {
    [self performRequest:endpoint body:body attempt:0 completion:completion];
}

- (void)performRequest:(NSString *)endpoint
                  body:(nullable NSDictionary *)body
               attempt:(NSUInteger)attempt
            completion:(AegisCompletion)completion {
    NSString *path = [endpoint stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"/"]];
    NSURL *url = [NSURL URLWithString:[NSString stringWithFormat:@"%@/api/public/v1/%@", self.baseUrl, path]];
    if (url == nil) {
        completion(nil, [self errorWithCode:@"invalid_options" message:@"Invalid baseUrl." status:0]);
        return;
    }

    NSMutableDictionary *payload = [NSMutableDictionary dictionary];
    [body enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL *stop) {
        if (value != nil && value != [NSNull null]) payload[key] = value;
    }];

    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    request.HTTPMethod = @"POST";
    request.timeoutInterval = self.options.timeout;
    [request setValue:@"application/json" forHTTPHeaderField:@"content-type"];
    [request setValue:[NSString stringWithFormat:@"aegis-objc-sdk/%@", AegisClient.sdkVersion]
   forHTTPHeaderField:@"user-agent"];
    [request setValue:self.options.appKey forHTTPHeaderField:@"x-app-key"];
    if (self.options.apiKey.length > 0) [request setValue:self.options.apiKey forHTTPHeaderField:@"x-api-key"];
    NSString *current = self.sessionToken;
    if (current.length > 0) [request setValue:current forHTTPHeaderField:@"x-session-token"];
    request.HTTPBody = [NSJSONSerialization dataWithJSONObject:payload options:0 error:nil];

    __weak typeof(self) weakSelf = self;
    NSURLSessionDataTask *task = [self.session dataTaskWithRequest:request
                                                 completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        __strong typeof(weakSelf) self = weakSelf;
        if (self == nil) return;

        NSInteger status = [response isKindOfClass:NSHTTPURLResponse.class] ? ((NSHTTPURLResponse *)response).statusCode : 0;
        BOOL retryable = (error != nil || status >= 500) && attempt < self.options.maxRetries;
        if (retryable) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.25 * (attempt + 1) * NSEC_PER_SEC)),
                           dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                [self performRequest:endpoint body:body attempt:attempt + 1 completion:completion];
            });
            return;
        }
        if (error != nil) {
            completion(nil, [self errorWithCode:@"network_error" message:error.localizedDescription status:0]);
            return;
        }

        NSDictionary *envelope = data.length > 0 ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        if (![envelope isKindOfClass:NSDictionary.class]) {
            completion(nil, [self errorWithCode:@"invalid_response" message:@"Malformed API response." status:status]);
            return;
        }
        if ([envelope[@"success"] boolValue]) {
            NSDictionary *result = envelope[@"data"];
            completion([result isKindOfClass:NSDictionary.class] ? result : @{}, nil);
            return;
        }
        NSDictionary *failure = [envelope[@"error"] isKindOfClass:NSDictionary.class] ? envelope[@"error"] : @{};
        completion(nil, [self errorWithCode:failure[@"code"] ?: @"server_error"
                                    message:failure[@"message"] ?: @"Request failed."
                                     status:status]);
    }];
    [task resume];
}

- (void)storeSession:(NSDictionary *)data {
    NSDictionary *session = data[@"session"];
    if ([session isKindOfClass:NSDictionary.class] && [session[@"token"] isKindOfClass:NSString.class]) {
        [self useSession:session[@"token"]];
    }
}

#pragma mark - Application

- (void)initializeApplication:(AegisCompletion)completion {
    [self request:@"init" body:@{ @"version": self.options.version } completion:completion];
}

- (void)status:(AegisCompletion)completion { [self request:@"status" body:nil completion:completion]; }

- (void)appData:(AegisCompletion)completion { [self request:@"app/data" body:nil completion:completion]; }

- (void)downloads:(AegisCompletion)completion { [self request:@"downloads" body:nil completion:completion]; }

- (void)checkVersion:(nullable NSString *)version completion:(AegisCompletion)completion {
    [self request:@"version/check"
             body:@{ @"version": version ?: self.options.version, @"channel": self.options.channel }
       completion:completion];
}

#pragma mark - Authentication

- (void)registerUsername:(NSString *)username
                password:(NSString *)password
                   email:(nullable NSString *)email
              licenseKey:(nullable NSString *)licenseKey
              completion:(AegisCompletion)completion {
    __weak typeof(self) weakSelf = self;
    [self request:@"register"
             body:@{ @"username": username, @"password": password,
                     @"email": email ?: [NSNull null], @"license_key": licenseKey ?: [NSNull null],
                     @"hwid": self.hardwareId }
       completion:^(NSDictionary *data, NSError *error) {
        if (data != nil) [weakSelf storeSession:data];
        completion(data, error);
    }];
}

- (void)loginUsername:(NSString *)username password:(NSString *)password completion:(AegisCompletion)completion {
    __weak typeof(self) weakSelf = self;
    [self request:@"login"
             body:@{ @"username": username, @"password": password, @"hwid": self.hardwareId }
       completion:^(NSDictionary *data, NSError *error) {
        if (data != nil) [weakSelf storeSession:data];
        completion(data, error);
    }];
}

- (void)logout:(AegisCompletion)completion {
    __weak typeof(self) weakSelf = self;
    [self request:@"logout" body:nil completion:^(NSDictionary *data, NSError *error) {
        [weakSelf useSession:nil];
        completion(data, error);
    }];
}

- (void)heartbeat:(AegisCompletion)completion { [self request:@"heartbeat" body:nil completion:completion]; }

- (void)checkSession:(AegisCompletion)completion { [self request:@"session/check" body:nil completion:completion]; }

- (void)isAuthenticated:(void (^)(BOOL authenticated))completion {
    if (self.sessionToken.length == 0) {
        completion(NO);
        return;
    }
    [self checkSession:^(NSDictionary *data, NSError *error) {
        completion(error == nil && [data[@"valid"] boolValue]);
    }];
}

- (void)userData:(AegisCompletion)completion { [self request:@"user/data" body:nil completion:completion]; }

#pragma mark - Licensing

- (void)validateLicense:(NSString *)licenseKey completion:(AegisCompletion)completion {
    [self request:@"license/validate"
             body:@{ @"license_key": licenseKey, @"hwid": self.hardwareId }
       completion:completion];
}

- (void)activateLicense:(NSString *)licenseKey
               username:(nullable NSString *)username
             completion:(AegisCompletion)completion {
    [self request:@"license/activate"
             body:@{ @"license_key": licenseKey, @"hwid": self.hardwareId, @"username": username ?: [NSNull null] }
       completion:completion];
}

#pragma mark - Variables

- (void)variablesForScope:(nullable NSString *)scope
               licenseKey:(nullable NSString *)licenseKey
               completion:(AegisCompletion)completion {
    [self request:@"variables/get"
             body:@{ @"scope": scope ?: @"application", @"license_key": licenseKey ?: [NSNull null] }
       completion:completion];
}

- (void)setVariable:(NSString *)key
              value:(NSString *)value
              scope:(nullable NSString *)scope
         licenseKey:(nullable NSString *)licenseKey
         completion:(AegisCompletion)completion {
    [self request:@"variables/set"
             body:@{ @"scope": scope ?: @"user", @"key": key, @"value": value,
                     @"license_key": licenseKey ?: [NSNull null] }
       completion:completion];
}

- (void)triggerWebhook:(NSString *)event
               payload:(nullable NSDictionary *)payload
            completion:(AegisCompletion)completion {
    [self request:@"webhook/trigger" body:@{ @"event": event, @"payload": payload ?: @{} } completion:completion];
}

#pragma mark - Sessions

- (void)startHeartbeatWithInterval:(NSTimeInterval)interval onRevoked:(nullable void (^)(NSError *))onRevoked {
    [self stopHeartbeat];

    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    dispatch_source_set_timer(timer,
                              dispatch_time(DISPATCH_TIME_NOW, (int64_t)(interval * NSEC_PER_SEC)),
                              (uint64_t)(interval * NSEC_PER_SEC),
                              (uint64_t)(NSEC_PER_SEC));

    __weak typeof(self) weakSelf = self;
    dispatch_source_set_event_handler(timer, ^{
        [weakSelf heartbeat:^(NSDictionary *data, NSError *error) {
            if (error != nil && !error.aegis_isNetworkError) {
                [weakSelf stopHeartbeat];
                if (onRevoked != nil) onRevoked(error);
            }
        }];
    });
    self.heartbeatTimer = timer;
    dispatch_resume(timer);
}

- (void)stopHeartbeat {
    if (self.heartbeatTimer != nil) {
        dispatch_source_cancel(self.heartbeatTimer);
        self.heartbeatTimer = nil;
    }
}

- (void)dealloc {
    [self stopHeartbeat];
}

@end