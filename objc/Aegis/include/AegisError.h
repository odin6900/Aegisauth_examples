#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// Domain used by every NSError the SDK produces.
extern NSErrorDomain const AegisErrorDomain;

/// `userInfo` key holding the machine-readable Aegis error code.
extern NSString *const AegisErrorCodeKey;

/// Helpers for classifying an NSError returned by the SDK.
@interface NSError (Aegis)

/// Machine-readable Aegis error code, e.g. `invalid_credentials`.
@property (nonatomic, readonly, copy) NSString *aegis_code;

/// YES when the request never reached the Aegis API.
@property (nonatomic, readonly) BOOL aegis_isNetworkError;

/// YES for credential/session failures.
@property (nonatomic, readonly) BOOL aegis_isAuthError;

/// YES for licensing and hardware-binding failures.
@property (nonatomic, readonly) BOOL aegis_isLicenseError;

@end

NS_ASSUME_NONNULL_END