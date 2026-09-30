#import "AegisOptions.h"

@implementation AegisOptions

- (instancetype)init {
    if ((self = [super init])) {
        _baseUrl = @"";
        _appKey = @"";
        _version = @"1.0.0";
        _channel = @"stable";
        _timeout = 20;
        _maxRetries = 2;
    }
    return self;
}

+ (instancetype)optionsWithBaseUrl:(NSString *)baseUrl appKey:(NSString *)appKey {
    AegisOptions *options = [[AegisOptions alloc] init];
    options.baseUrl = baseUrl;
    options.appKey = appKey;
    return options;
}

@end