package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"runtime"
	"strconv"
	"strings"
	"time"
)

// Configuration Placeholders
const (
	AuthURL    = "https://auth.example.com"
	AppKey     = "YOUR_APP_KEY"
	AppName    = "My Application"
	AppVersion = "1.0.0"

	Username   = "YOUR_USERNAME"
	Password   = "YOUR_PASSWORD"
	ModuleName = "minecraft"
)

type APIResponse struct {
	Success bool            `json:"success"`
	Data    json.RawMessage `json:"data,omitempty"`
	Error   *struct {
		Code    string `json:"code"`
		Message string `json:"message"`
	} `json:"error,omitempty"`
}

type Client struct {
	BaseURL      string
	AppKey       string
	AppName      string
	AppVersion   string
	HWID         string
	SessionToken string
	HTTPClient   *http.Client
}

func getHardwareID() string {
	hostname, _ := os.Hostname()
	seed := fmt.Sprintf("%s-%s-%s", hostname, runtime.GOOS, runtime.GOARCH)
	h := sha256.Sum256([]byte(seed))
	return hex.EncodeToString(h[:])
}

func NewClient() *Client {
	return &Client{
		BaseURL:    strings.TrimRight(AuthURL, "/") + "/api/public/v1/",
		AppKey:     AppKey,
		AppName:    AppName,
		AppVersion: AppVersion,
		HWID:       getHardwareID(),
		HTTPClient: &http.Client{Timeout: 15 * time.Second},
	}
}

func (c *Client) post(endpoint string, payload interface{}) (*APIResponse, error) {
	bodyBytes, err := json.Marshal(payload)
	if err != nil {
		return nil, err
	}

	req, err := http.NewRequest("POST", c.BaseURL+endpoint, bytes.NewReader(bodyBytes))
	if err != nil {
		return nil, err
	}

	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("x-app-key", c.AppKey)
	req.Header.Set("x-timestamp", strconv.FormatInt(time.Now().Unix(), 10))
	req.Header.Set("User-Agent", "AegisAuth-Go/"+c.AppVersion)

	if c.SessionToken != "" {
		req.Header.Set("x-session-token", c.SessionToken)
	}

	res, err := c.HTTPClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer res.Body.Close()

	resBytes, err := io.ReadAll(res.Body)
	if err != nil {
		return nil, err
	}

	var apiRes APIResponse
	if err := json.Unmarshal(resBytes, &apiRes); err != nil {
		return nil, fmt.Errorf("failed to parse JSON response: %w", err)
	}

	return &apiRes, nil
}

func (c *Client) Init() bool {
	fmt.Printf("[*] Initializing %s (v%s)...\n", c.AppName, c.AppVersion)
	fmt.Printf("    HWID: %s\n", c.HWID)

	res, err := c.post("init", map[string]string{"version": c.AppVersion})
	if err != nil || !res.Success {
		errMsg := "Unknown error"
		if res != nil && res.Error != nil {
			errMsg = res.Error.Message
		}
		fmt.Printf("[-] Init failed: %s\n", errMsg)
		return false
	}

	fmt.Println("[+] Handshake successful.")
	return true
}

func (c *Client) Login(username, password string) bool {
	fmt.Printf("\n[*] Logging in as '%s'...\n", username)

	payload := map[string]string{
		"username": username,
		"password": password,
		"hwid":     c.HWID,
	}

	res, err := c.post("login", payload)
	if err != nil || !res.Success {
		errMsg := "Unknown error"
		if res != nil && res.Error != nil {
			errMsg = res.Error.Message
		}
		fmt.Printf("[-] Login failed: %s\n", errMsg)
		return false
	}

	var data struct {
		Token   string `json:"token"`
		License struct {
			Status string `json:"status"`
		} `json:"license"`
	}
	_ = json.Unmarshal(res.Data, &data)

	c.SessionToken = data.Token
	tokenPreview := data.Token
	if len(tokenPreview) > 12 {
		tokenPreview = tokenPreview[:12] + "..."
	}
	fmt.Printf("[+] Login successful! Session: %s\n", tokenPreview)
	fmt.Printf("    License Status: %s\n", data.License.Status)
	return true
}

func (c *Client) RequestModuleToken(moduleName string) string {
	fmt.Printf("\n[*] Requesting signed download token for module '%s'...\n", moduleName)

	res, err := c.post("module/token", map[string]string{"module": moduleName})
	if err != nil || !res.Success {
		errMsg := "Unknown error"
		if res != nil && res.Error != nil {
			errMsg = res.Error.Message
		}
		fmt.Printf("[-] Module token request failed: %s\n", errMsg)
		return ""
	}

	var data struct {
		Key         string `json:"key"`
		DownloadURL string `json:"download_url"`
		ExpiresIn   int    `json:"expires_in_seconds"`
	}
	_ = json.Unmarshal(res.Data, &data)

	fmt.Println("[+] Signed Token Received!")
	fmt.Printf("    Target Asset: %s\n", data.Key)
	fmt.Printf("    Download URL: %s\n", data.DownloadURL)
	fmt.Printf("    Expires In  : %ds\n", data.ExpiresIn)
	return data.DownloadURL
}

func (c *Client) Logout() {
	if c.SessionToken != "" {
		fmt.Println("\n[*] Logging out...")
		_, _ = c.post("logout", map[string]string{})
		c.SessionToken = ""
		fmt.Println("[+] Logged out cleanly.")
	}
}

func main() {
	client := NewClient()

	if !client.Init() {
		return
	}

	if !client.Login(Username, Password) {
		return
	}

	downloadURL := client.RequestModuleToken(ModuleName)
	if downloadURL != "" {
		fmt.Println("\n[+] Binary payload authorized for AetherVault silent streaming!")
	}

	client.Logout()
}
