// Package aegis is the official Go client for the Aegis Authentication API.
package aegis

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"
)

// Options configures a Client.
type Options struct {
	BaseURL    string
	AppKey     string
	APIKey     string
	Version    string
	Channel    string
	HWID       string
	Timeout    time.Duration
	MaxRetries int
	HTTPClient *http.Client
}

// Client talks to the Aegis Authentication API.
type Client struct {
	baseURL    string
	appKey     string
	apiKey     string
	version    string
	channel    string
	hwid       string
	maxRetries int
	http       *http.Client

	mu           sync.RWMutex
	sessionToken string
	stopBeat     chan struct{}
}

// New creates a client. BaseURL and AppKey are required.
func New(opts Options) (*Client, error) {
	if opts.BaseURL == "" {
		return nil, &Error{Code: "invalid_options", Message: "BaseURL is required."}
	}
	if opts.AppKey == "" {
		return nil, &Error{Code: "invalid_options", Message: "AppKey is required."}
	}
	timeout := opts.Timeout
	if timeout == 0 {
		timeout = 20 * time.Second
	}
	httpClient := opts.HTTPClient
	if httpClient == nil {
		httpClient = &http.Client{Timeout: timeout}
	}
	version := opts.Version
	if version == "" {
		version = "1.0.0"
	}
	channel := opts.Channel
	if channel == "" {
		channel = "stable"
	}
	hwid := opts.HWID
	if hwid == "" {
		hwid = HardwareID()
	}
	return &Client{
		baseURL:    strings.TrimRight(opts.BaseURL, "/"),
		appKey:     opts.AppKey,
		apiKey:     opts.APIKey,
		version:    version,
		channel:    channel,
		hwid:       hwid,
		maxRetries: opts.MaxRetries,
		http:       httpClient,
	}, nil
}

// HWID returns the hardware id sent with authentication calls.
func (c *Client) HWID() string { return c.hwid }

// SessionToken returns the current session token, if any.
func (c *Client) SessionToken() string {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.sessionToken
}

// UseSession restores a session token persisted by the host application.
func (c *Client) UseSession(token string) {
	c.mu.Lock()
	c.sessionToken = token
	c.mu.Unlock()
}

// Request calls any endpoint and decodes its data payload into out.
func (c *Client) Request(ctx context.Context, endpoint string, body map[string]interface{}, out interface{}) error {
	url := fmt.Sprintf("%s/api/public/v1/%s", c.baseURL, strings.Trim(endpoint, "/"))
	payload, err := json.Marshal(compact(body))
	if err != nil {
		return &Error{Code: "invalid_request", Message: err.Error(), Cause: err}
	}

	var lastErr error
	for attempt := 0; attempt <= c.maxRetries; attempt++ {
		req, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(payload))
		if err != nil {
			return &Error{Code: "invalid_request", Message: err.Error(), Cause: err}
		}
		req.Header.Set("content-type", "application/json")
		req.Header.Set("x-app-key", c.appKey)
		req.Header.Set("x-timestamp", strconv.FormatInt(time.Now().Unix(), 10))
		req.Header.Set("user-agent", "aegis-go-sdk/1.0.0")
		if c.apiKey != "" {
			req.Header.Set("x-api-key", c.apiKey)
		}
		if token := c.SessionToken(); token != "" {
			req.Header.Set("x-session-token", token)
		}

		resp, err := c.http.Do(req)
		if err != nil {
			lastErr = err
			if attempt < c.maxRetries {
				time.Sleep(time.Duration(attempt+1) * 250 * time.Millisecond)
				continue
			}
			break
		}

		raw, readErr := io.ReadAll(resp.Body)
		resp.Body.Close()
		if readErr != nil {
			lastErr = readErr
			if attempt < c.maxRetries {
				continue
			}
			break
		}
		if resp.StatusCode >= 500 && attempt < c.maxRetries {
			time.Sleep(time.Duration(attempt+1) * 250 * time.Millisecond)
			continue
		}

		var env struct {
			Success bool            `json:"success"`
			Data    json.RawMessage `json:"data"`
			Error   *struct {
				Code    string `json:"code"`
				Message string `json:"message"`
			} `json:"error"`
		}
		if err := json.Unmarshal(raw, &env); err != nil {
			return &Error{Code: "invalid_response", Message: "Malformed API response.", Status: resp.StatusCode, Cause: err}
		}
		if !env.Success {
			code, message := "server_error", "Request failed."
			if env.Error != nil {
				code, message = env.Error.Code, env.Error.Message
			}
			return &Error{Code: code, Message: message, Status: resp.StatusCode}
		}
		if out == nil || len(env.Data) == 0 {
			return nil
		}
		if err := json.Unmarshal(env.Data, out); err != nil {
			return &Error{Code: "invalid_response", Message: err.Error(), Status: resp.StatusCode, Cause: err}
		}
		return nil
	}

	message := "Network request failed."
	if lastErr != nil {
		message = lastErr.Error()
	}
	return &Error{Code: "network_error", Message: message, Cause: lastErr}
}

// ---------------- application ----------------

// Init performs the handshake. Call once before any other operation.
func (c *Client) Init(ctx context.Context) (*InitResult, error) {
	out := &InitResult{}
	return out, c.Request(ctx, "init", map[string]interface{}{"version": c.version}, out)
}

// Status returns application availability information.
func (c *Client) Status(ctx context.Context) (map[string]interface{}, error) {
	out := map[string]interface{}{}
	return out, c.Request(ctx, "status", nil, &out)
}

// AppData returns public application metadata.
func (c *Client) AppData(ctx context.Context) (map[string]interface{}, error) {
	out := map[string]interface{}{}
	return out, c.Request(ctx, "app/data", nil, &out)
}

// CheckVersion compares a build against the configured release channel.
func (c *Client) CheckVersion(ctx context.Context, version string) (*VersionInfo, error) {
	if version == "" {
		version = c.version
	}
	out := &VersionInfo{}
	return out, c.Request(ctx, "version/check", map[string]interface{}{"version": version, "channel": c.channel}, out)
}

// ---------------- authentication ----------------

// Register creates an end user and starts a session.
func (c *Client) Register(ctx context.Context, username, password, email, licenseKey string) (*AuthResult, error) {
	out := &AuthResult{}
	err := c.Request(ctx, "register", map[string]interface{}{
		"username":    username,
		"password":    password,
		"email":       email,
		"license_key": licenseKey,
		"hwid":        c.hwid,
	}, out)
	c.storeSession(out, err)
	return out, err
}

// Login authenticates an end user and starts a session.
func (c *Client) Login(ctx context.Context, username, password string) (*AuthResult, error) {
	out := &AuthResult{}
	err := c.Request(ctx, "login", map[string]interface{}{
		"username": username,
		"password": password,
		"hwid":     c.hwid,
	}, out)
	c.storeSession(out, err)
	return out, err
}

// Logout ends the session and stops any heartbeat loop.
func (c *Client) Logout(ctx context.Context) error {
	c.StopHeartbeat()
	err := c.Request(ctx, "logout", nil, nil)
	c.UseSession("")
	return err
}

// Heartbeat keeps the session alive.
func (c *Client) Heartbeat(ctx context.Context) (*SessionCheck, error) {
	out := &SessionCheck{}
	return out, c.Request(ctx, "heartbeat", nil, out)
}

// CheckSession validates the stored session token.
func (c *Client) CheckSession(ctx context.Context) (*SessionCheck, error) {
	out := &SessionCheck{}
	return out, c.Request(ctx, "session/check", nil, out)
}

// IsAuthenticated reports whether the server still accepts the session.
func (c *Client) IsAuthenticated(ctx context.Context) bool {
	if c.SessionToken() == "" {
		return false
	}
	check, err := c.CheckSession(ctx)
	return err == nil && check.Valid
}

// UserData returns the signed-in user's stored data.
func (c *Client) UserData(ctx context.Context) (map[string]interface{}, error) {
	out := map[string]interface{}{}
	return out, c.Request(ctx, "user/data", nil, &out)
}

// ---------------- licensing ----------------

// ValidateLicense checks a license key against this machine.
func (c *Client) ValidateLicense(ctx context.Context, licenseKey string) (*LicenseResult, error) {
	out := &LicenseResult{}
	return out, c.Request(ctx, "license/validate", map[string]interface{}{"license_key": licenseKey, "hwid": c.hwid}, out)
}

// ActivateLicense binds a license key to this machine.
func (c *Client) ActivateLicense(ctx context.Context, licenseKey, username string) (*LicenseResult, error) {
	out := &LicenseResult{}
	return out, c.Request(ctx, "license/activate", map[string]interface{}{
		"license_key": licenseKey,
		"hwid":        c.hwid,
		"username":    username,
	}, out)
}

// ---------------- variables ----------------

// GetVariables reads a scoped variable bag ("application", "user" or "license").
func (c *Client) GetVariables(ctx context.Context, scope, licenseKey string) (*Variables, error) {
	if scope == "" {
		scope = "application"
	}
	out := &Variables{}
	return out, c.Request(ctx, "variables/get", map[string]interface{}{"scope": scope, "license_key": licenseKey}, out)
}

// SetVariable writes a scoped variable.
func (c *Client) SetVariable(ctx context.Context, key, value, scope, licenseKey string) error {
	if scope == "" {
		scope = "user"
	}
	return c.Request(ctx, "variables/set", map[string]interface{}{
		"scope":       scope,
		"key":         key,
		"value":       value,
		"license_key": licenseKey,
	}, nil)
}

// TriggerWebhook dispatches an application webhook event.
func (c *Client) TriggerWebhook(ctx context.Context, event string, payload map[string]interface{}) error {
	if payload == nil {
		payload = map[string]interface{}{}
	}
	return c.Request(ctx, "webhook/trigger", map[string]interface{}{"event": event, "payload": payload}, nil)
}

// ---------------- heartbeat loop ----------------

// StartHeartbeat runs a background heartbeat until the session is revoked or
// StopHeartbeat is called. onRevoked may be nil.
func (c *Client) StartHeartbeat(ctx context.Context, interval time.Duration, onRevoked func(reason string)) {
	c.StopHeartbeat()
	if interval <= 0 {
		interval = time.Minute
	}
	stop := make(chan struct{})
	c.mu.Lock()
	c.stopBeat = stop
	c.mu.Unlock()

	go func() {
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-stop:
				return
			case <-ticker.C:
				if _, err := c.Heartbeat(ctx); err != nil {
					if aerr, ok := err.(*Error); ok && aerr.IsNetworkError() {
						continue
					}
					c.UseSession("")
					if onRevoked != nil {
						onRevoked(err.Error())
					}
					return
				}
			}
		}
	}()
}

// StopHeartbeat ends a running heartbeat loop.
func (c *Client) StopHeartbeat() {
	c.mu.Lock()
	if c.stopBeat != nil {
		close(c.stopBeat)
		c.stopBeat = nil
	}
	c.mu.Unlock()
}

func (c *Client) storeSession(result *AuthResult, err error) {
	if err == nil && result != nil && result.Session != nil && result.Session.Token != "" {
		c.UseSession(result.Session.Token)
	}
}

func compact(body map[string]interface{}) map[string]interface{} {
	out := map[string]interface{}{}
	for key, value := range body {
		switch v := value.(type) {
		case nil:
			continue
		case string:
			if v == "" {
				continue
			}
		}
		out[key] = value
	}
	return out
}