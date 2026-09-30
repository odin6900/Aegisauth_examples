#import "AegisError.h"

NSErrorDomain const AegisErrorDomain = @"io.aegis.sdk";
NSString *const AegisErrorCodeKey = @"AegisErrorCode";

@implementation NSError (Aegis)

- (NSString *)aegis_code {
    NSString *code = self.userInfo[AegisErrorCodeKey];
    return code ?: @"unknown_error";
}

- (BOOL)aegis_isNetworkError {
    return [self.aegis_code isEqualToString:@"network_error"];
}

- (BOOL)aegis_isAuthError {
    NSString *code = self.aegis_code;
    return [code isEqualToString:@"unauthorized"] || [code isEqualToString:@"invalid_credentials"];
}

- (BOOL)aegis_isLicenseError {
    NSString *code = self.aegis_code;
    return [code hasPrefix:@"license"] || [code isEqualToString:@"hwid_mismatch"];
}

@end