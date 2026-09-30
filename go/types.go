package aegis

// Subscription describes an active tier or assignment.
type Subscription struct {
	Name            string `json:"name"`
	Status          string `json:"status"`
	ExpiresAt       string `json:"expires_at,omitempty"`
	DurationDays    *int   `json:"duration_days,omitempty"`
	DurationMinutes *int   `json:"duration_minutes,omitempty"`
}

// User is an application end user.
type User struct {
	ID            string         `json:"id"`
	Username      string         `json:"username"`
	Email         string         `json:"email,omitempty"`
	Status        string         `json:"status,omitempty"`
	HWID          string         `json:"hwid,omitempty"`
	CreatedAt     string         `json:"created_at,omitempty"`
	LastLoginAt   string         `json:"last_login_at,omitempty"`
	ExpiresAt     string         `json:"expires_at,omitempty"`
	Subscriptions []Subscription `json:"subscriptions,omitempty"`
}

// License describes a license and its activation state.
type License struct {
	Key             string         `json:"key,omitempty"`
	Status          string         `json:"status,omitempty"`
	ExpiresAt       string         `json:"expires_at,omitempty"`
	DurationDays    *int           `json:"duration_days,omitempty"`
	DurationMinutes *int           `json:"duration_minutes,omitempty"`
	Activations     int            `json:"activations,omitempty"`
	MaxActivations  int            `json:"max_activations,omitempty"`
	Level           int            `json:"level,omitempty"`
	Subscriptions   []Subscription `json:"subscriptions,omitempty"`
}

// Session is an issued session token.
type Session struct {
	Token     string `json:"token"`
	ExpiresAt string `json:"expires_at,omitempty"`
}

// VersionInfo is the result of a version check.
type VersionInfo struct {
	Latest          string `json:"latest,omitempty"`
	Current         string `json:"current,omitempty"`
	UpdateAvailable bool   `json:"update_available,omitempty"`
	UpdateRequired  bool   `json:"update_required,omitempty"`
	DownloadURL     string `json:"download_url,omitempty"`
	Changelog       string `json:"changelog,omitempty"`
}

// InitResult is returned by Init.
type InitResult struct {
	Status      string                 `json:"status,omitempty"`
	Application map[string]interface{} `json:"application,omitempty"`
	Version     *VersionInfo           `json:"version,omitempty"`
}

// AuthResult is returned by Login and Register.
type AuthResult struct {
	User    User     `json:"user"`
	License *License `json:"license,omitempty"`
	Session *Session `json:"session,omitempty"`
}

// LicenseResult is returned by license validation and activation.
type LicenseResult struct {
	Valid     bool     `json:"valid,omitempty"`
	Activated bool     `json:"activated,omitempty"`
	Status    string   `json:"status,omitempty"`
	License   *License `json:"license,omitempty"`
}

// SessionCheck is returned by Heartbeat and CheckSession.
type SessionCheck struct {
	Valid     bool     `json:"valid,omitempty"`
	Alive     bool     `json:"alive,omitempty"`
	User      *User    `json:"user,omitempty"`
	License   *License `json:"license,omitempty"`
	ExpiresAt string   `json:"expires_at,omitempty"`
}

// Variables is a scoped key/value bag.
type Variables struct {
	Scope     string            `json:"scope"`
	Variables map[string]string `json:"variables"`
}

// HasSubscription reports whether the user holds an active subscription by name.
func (u *User) HasSubscription(name string) bool {
	if u == nil || name == "" {
		return false
	}
	target := name
	for _, s := range u.Subscriptions {
		if s.Name == target || (s.Name != "" && len(s.Name) == len(target) && (s.Name == target)) {
			if s.Status != "" && s.Status != "active" {
				continue
			}
			return true
		}
	}
	return false
}

// HasSubscription reports whether the license holds an active subscription by name.
func (l *License) HasSubscription(name string) bool {
	if l == nil || name == "" {
		return false
	}
	target := name
	for _, s := range l.Subscriptions {
		if s.Name == target {
			if s.Status != "" && s.Status != "active" {
				continue
			}
			return true
		}
	}
	return false
}