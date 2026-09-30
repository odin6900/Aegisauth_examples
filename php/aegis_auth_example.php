<?php
/**
 * AegisAuth PHP Example
 * Demonstrates initialization, user login, license validation, and requesting a signed AetherVault module token.
 */

// Configuration Placeholders
$AUTH_URL    = "https://auth.example.com";
$APP_KEY     = "YOUR_APP_KEY";
$APP_NAME    = "My Application";
$APP_VERSION = "1.0.0";

$USERNAME    = "YOUR_USERNAME";
$PASSWORD    = "YOUR_PASSWORD";
$MODULE_NAME = "minecraft";

class AegisAuthClient {
    private string $baseUrl;
    private string $appKey;
    private string $appName;
    private string $appVersion;
    private string $hwid;
    private ?string $sessionToken = null;

    public function __construct(string $authUrl, string $appKey, string $appName, string $appVersion) {
        $this->baseUrl = rtrim($authUrl, '/') . '/api/public/v1/';
        $this->appKey = $appKey;
        $this->appName = $appName;
        $this->appVersion = $appVersion;
        $this->hwid = hash('sha256', php_uname('n') . '-' . PHP_OS);
    }

    private function post(string $endpoint, array $payload): array {
        $url = $this->baseUrl . $endpoint;
        $headers = [
            'Content-Type: application/json',
            'x-app-key: ' . $this->appKey,
            'x-timestamp: ' . time(),
            'User-Agent: AegisAuth-PHP/' . $this->appVersion,
        ];

        if ($this->sessionToken) {
            $headers[] = 'x-session-token: ' . $this->sessionToken;
        }

        $ch = curl_init($url);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_POST, true);
        curl_setopt($ch, CURLOPT_POSTFIELDS, json_encode($payload));
        curl_setopt($ch, CURLOPT_HTTPHEADER, $headers);
        curl_setopt($ch, CURLOPT_TIMEOUT, 15);

        $response = curl_exec($ch);
        $err = curl_error($ch);
        curl_close($ch);

        if ($err) {
            return ['success' => false, 'error' => ['message' => $err]];
        }

        $data = json_decode($response, true);
        return is_array($data) ? $data : ['success' => false, 'error' => ['message' => 'Malformed JSON response']];
    }

    public function init(): bool {
        echo "[*] Initializing {$this->appName} (v{$this->appVersion})...\n";
        echo "    HWID: {$this->hwid}\n";

        $res = $this->post('init', ['version' => $this->appVersion]);
        if (empty($res['success'])) {
            $msg = $res['error']['message'] ?? 'Unknown error';
            echo "[-] Init failed: {$msg}\n";
            return false;
        }

        echo "[+] Handshake successful.\n";
        return true;
    }

    public function login(string $username, string $password): bool {
        echo "\n[*] Logging in as '{$username}'...\n";

        $res = $this->post('login', [
            'username' => $username,
            'password' => $password,
            'hwid'     => $this->hwid,
        ]);

        if (empty($res['success'])) {
            $msg = $res['error']['message'] ?? 'Unknown error';
            echo "[-] Login failed: {$msg}\n";
            return false;
        }

        $this->sessionToken = $res['data']['token'] ?? null;
        $preview = substr($this->sessionToken ?? '', 0, 12) . '...';
        echo "[+] Logged in successfully! Session token: {$preview}\n";
        return true;
    }

    public function requestModuleToken(string $module): ?string {
        echo "\n[*] Requesting signed download token for module '{$module}'...\n";

        $res = $this->post('module/token', ['module' => $module]);
        if (empty($res['success'])) {
            $msg = $res['error']['message'] ?? 'Unknown error';
            echo "[-] Module token request failed: {$msg}\n";
            return null;
        }

        $key = $res['data']['key'] ?? '';
        $url = $res['data']['download_url'] ?? '';
        $exp = $res['data']['expires_in_seconds'] ?? 0;

        echo "[+] Signed Token Received!\n";
        echo "    Target Asset: {$key}\n";
        echo "    Download URL: {$url}\n";
        echo "    Expires In  : {$exp}s\n";
        return $url;
    }

    public function logout(): void {
        if ($this->sessionToken) {
            echo "\n[*] Logging out...\n";
            $this->post('logout', []);
            $this->sessionToken = null;
            echo "[+] Logged out cleanly.\n";
        }
    }
}

// Execution
$client = new AegisAuthClient($AUTH_URL, $APP_KEY, $APP_NAME, $APP_VERSION);
if (!$client->init()) exit(1);
if (!$client->login($USERNAME, $PASSWORD)) exit(1);

$downloadUrl = $client->requestModuleToken($MODULE_NAME);
if ($downloadUrl) {
    echo "\n[+] Binary payload authorized for AetherVault silent streaming!\n";
}

$client->logout();
