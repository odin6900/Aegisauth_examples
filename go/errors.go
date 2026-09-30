package aegis

import (
	"fmt"
	"strings"
)

// Error is returned for every Aegis API failure.
type Error struct {
	Code    string
	Message string
	Status  int
	Cause   error
}

func (e *Error) Error() string {
	return fmt.Sprintf("aegis: [%s] %s", e.Code, e.Message)
}

func (e *Error) Unwrap() error { return e.Cause }

// IsNetworkError reports transport-level failures (no HTTP status).
func (e *Error) IsNetworkError() bool { return e.Status == 0 }

// IsAuthError reports rejected credentials or sessions.
func (e *Error) IsAuthError() bool {
	return e.Code == "unauthorized" || e.Code == "invalid_credentials"
}

// IsLicenseError reports licensing and hardware-binding failures.
func (e *Error) IsLicenseError() bool {
	return strings.HasPrefix(e.Code, "license") || e.Code == "hwid_mismatch"
}