<?php

declare(strict_types=1);

namespace Aegis;

/** Client for the Aegis Authentication API (PHP 8.1+, cURL). */
class Aegis
{
    private ?string $sessionToken = null;
    private ?array $currentUser = null;
    private ?array $currentLicense = null;
    public readonly string $hwid;
    private readonly string $baseUrl;

    public function __construct(
        string $baseUrl,
        private readonly string $appKey,
        private readonly ?string $apiKey = null,
        private readonly string $version = '1.0.0',
        private readonly string $channel = 'stable',
        ?string $hwid = null,
        private readonly int $timeout = 20,
        private readonly int $maxRetries = 2,
    ) {
        if ($baseUrl === '') {
            throw new AegisException('invalid_options', 'baseUrl is required.');
        }
        if ($appKey === '') {
            throw new AegisException('invalid_options', 'appKey is required.');
        }
        $this->baseUrl = rtrim($baseUrl, '/');
        $this->hwid = $hwid ?? self::hardwareId();
    }

    /** Stable, non-reversible machine identifier. */
    public static function hardwareId(): string
    {
        return hash('sha256', implode('|', [php_uname('n'), php_uname('s'), php_uname('m'), get_current_user()]));
    }

    public function sessionToken(): ?string
    {
        return $this->sessionToken;
    }

    /** Restores a session token persisted by the host application. */
    public function useSession(?string $token): void
    {
        $this->sessionToken = $token;
    }

    /** Calls any endpoint and returns its `data` payload. */
    public function request(string $endpoint, array $body = []): array
    {
        $url = $this->baseUrl . '/api/public/v1/' . trim($endpoint, '/');
        $payload = json_encode(array_filter($body, static fn ($value) => $value !== null), JSON_THROW_ON_ERROR);

        $headers = [
            'content-type: application/json',
            'x-app-key: ' . $this->appKey,
            'x-timestamp: ' . time(),
            'user-agent: aegis-php-sdk/1.0.0',
        ];
        if ($this->apiKey !== null) {
            $headers[] = 'x-api-key: ' . $this->apiKey;
        }
        if ($this->sessionToken !== null) {
            $headers[] = 'x-session-token: ' . $this->sessionToken;
        }

        $lastError = null;

        for ($attempt = 0; $attempt <= $this->maxRetries; $attempt++) {
            $curl = curl_init($url);
            curl_setopt_array($curl, [
                CURLOPT_POST => true,
                CURLOPT_POSTFIELDS => $payload,
                CURLOPT_HTTPHEADER => $headers,
                CURLOPT_RETURNTRANSFER => true,
                CURLOPT_TIMEOUT => $this->timeout,
            ]);
            $raw = curl_exec($curl);
            $status = (int) curl_getinfo($curl, CURLINFO_RESPONSE_CODE);
            $error = curl_error($curl);
            curl_close($curl);

            if ($raw === false) {
                $lastError = $error;
                if ($attempt < $this->maxRetries) {
                    usleep(250_000 * ($attempt + 1));
                    continue;
                }
                break;
            }
            if ($status >= 500 && $attempt < $this->maxRetries) {
                usleep(250_000 * ($attempt + 1));
                continue;
            }

            $envelope = json_decode((string) $raw ?: '{}', true);
            if (!is_array($envelope)) {
                throw new AegisException('invalid_response', 'Malformed API response.', $status);
            }
            if (empty($envelope['success'])) {
                throw new AegisException(
                    (string) ($envelope['error']['code'] ?? 'server_error'),
                    (string) ($envelope['error']['message'] ?? 'Request failed.'),
                    $status,
                );
            }

            return is_array($envelope['data'] ?? null) ? $envelope['data'] : [];
        }

        throw new AegisException('network_error', $lastError ?: 'Network request failed.');
    }

    // ---------------- application ----------------

    /** Handshake. Call once before any other operation. */
    public function init(): array
    {
        return $this->request('init', ['version' => $this->version]);
    }

    public function status(): array
    {
        return $this->request('status');
    }

    public function appData(): array
    {
        return $this->request('app/data');
    }

    public function checkVersion(?string $version = null): array
    {
        return $this->request('version/check', ['version' => $version ?? $this->version, 'channel' => $this->channel]);
    }

    /** Download information published for this application. */
    public function downloads(): array
    {
        return $this->request('downloads');
    }

    // ---------------- authentication ----------------

    public function register(string $username, string $password, ?string $email = null, ?string $licenseKey = null): array
    {
        $result = $this->request('register', [
            'username' => $username,
            'password' => $password,
            'email' => $email,
            'license_key' => $licenseKey,
            'hwid' => $this->hwid,
        ]);
        $this->storeSession($result);

        return $result;
    }

    public function login(string $username, string $password): array
    {
        $result = $this->request('login', [
            'username' => $username,
            'password' => $password,
            'hwid' => $this->hwid,
        ]);
        $this->storeSession($result);

        return $result;
    }

    public function logout(): void
    {
        try {
            $this->request('logout');
        } finally {
            $this->sessionToken = null;
            $this->currentUser = null;
            $this->currentLicense = null;
        }
    }

    public function heartbeat(): array
    {
        $res = $this->request('heartbeat');
        if (isset($res['user']) && is_array($res['user'])) {
            $this->currentUser = $res['user'];
        }
        return $res;
    }

    public function checkSession(): array
    {
        $res = $this->request('session/check');
        if (isset($res['user']) && is_array($res['user'])) {
            $this->currentUser = $res['user'];
        }
        if (isset($res['license']) && is_array($res['license'])) {
            $this->currentLicense = $res['license'];
        }
        return $res;
    }

    /** True when a token exists and the server still accepts it. */
    public function isAuthenticated(): bool
    {
        if ($this->sessionToken === null) {
            return false;
        }
        try {
            return (bool) ($this->checkSession()['valid'] ?? false);
        } catch (AegisException) {
            return false;
        }
    }

    public function user(): ?array
    {
        return $this->currentUser;
    }

    public function license(): ?array
    {
        return $this->currentLicense;
    }

    /** Checks whether the active user or license holds an active subscription by name. */
    public function hasSubscription(string $name): bool
    {
        if ($name === '') {
            return false;
        }
        $target = strtolower($name);
        $subs = array_merge(
            $this->currentUser['subscriptions'] ?? [],
            $this->currentLicense['subscriptions'] ?? []
        );
        $now = time();
        foreach ($subs as $sub) {
            if (strtolower((string) ($sub['name'] ?? '')) === $target) {
                $status = strtolower((string) ($sub['status'] ?? 'active'));
                if ($status !== 'active') {
                    continue;
                }
                $expiresAt = $sub['expires_at'] ?? null;
                if ($expiresAt && strtotime((string) $expiresAt) <= $now) {
                    continue;
                }
                return true;
            }
        }
        return false;
    }

    /** Returns all active subscriptions for the current user and license. */
    public function getSubscriptions(): array
    {
        return array_merge(
            $this->currentUser['subscriptions'] ?? [],
            $this->currentLicense['subscriptions'] ?? []
        );
    }

    public function userData(): array
    {
        return $this->request('user/data');
    }

    // ---------------- licensing ----------------

    public function validateLicense(string $licenseKey): array
    {
        return $this->request('license/validate', ['license_key' => $licenseKey, 'hwid' => $this->hwid]);
    }

    public function activateLicense(string $licenseKey, ?string $username = null): array
    {
        $res = $this->request('license/activate', [
            'license_key' => $licenseKey,
            'hwid' => $this->hwid,
            'username' => $username,
        ]);
        if (isset($res['license']) && is_array($res['license'])) {
            $this->currentLicense = $res['license'];
        }
        return $res;
    }

    // ---------------- variables ----------------

    public function getVariables(string $scope = 'application', ?string $licenseKey = null): array
    {
        return $this->request('variables/get', ['scope' => $scope, 'license_key' => $licenseKey]);
    }

    public function setVariable(string $key, string $value, string $scope = 'user', ?string $licenseKey = null): array
    {
        return $this->request('variables/set', [
            'scope' => $scope,
            'key' => $key,
            'value' => $value,
            'license_key' => $licenseKey,
        ]);
    }

    public function triggerWebhook(string $event, array $payload = []): array
    {
        return $this->request('webhook/trigger', ['event' => $event, 'payload' => $payload]);
    }

    private function storeSession(array $result): void
    {
        $token = $result['session']['token'] ?? null;
        if (is_string($token) && $token !== '') {
            $this->sessionToken = $token;
        }
        if (isset($result['user']) && is_array($result['user'])) {
            $this->currentUser = $result['user'];
        }
        if (isset($result['license']) && is_array($result['license'])) {
            $this->currentLicense = $result['license'];
        }
    }
}