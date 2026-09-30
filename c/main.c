/**
 * AegisAuth C Example
 * Demonstrates initialization, user login, license validation, and requesting a signed AetherVault module token.
 * Uses native Windows WinINet with zero third-party library dependencies.
 */

#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <time.h>

#pragma comment(lib, "wininet.lib")

// Configuration Placeholders
#define AUTH_HOST    "auth.example.com"
#define AUTH_PORT    INTERNET_DEFAULT_HTTPS_PORT
#define USE_HTTPS    TRUE

#define APP_KEY      "YOUR_APP_KEY"
#define APP_NAME     "My Application"
#define APP_VERSION  "1.0.0"

#define USERNAME     "YOUR_USERNAME"
#define PASSWORD     "YOUR_PASSWORD"
#define MODULE_NAME  "minecraft"

// Helper to extract JSON string property
int ExtractJsonString(const char* json, const char* key, char* out, size_t maxLen) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* pos = strstr(json, search);
    if (!pos) return 0;

    pos += strlen(search);
    while (*pos == ' ' || *pos == '\t') pos++;

    if (*pos == '\"') {
        pos++;
        const char* end = strchr(pos, '\"');
        if (!end) return 0;
        size_t len = (size_t)(end - pos);
        if (len >= maxLen) len = maxLen - 1;
        strncpy_s(out, maxLen, pos, len);
        out[len] = '\0';
        return 1;
    }
    return 0;
}

// Perform HTTP POST request via WinINet
int HttpPost(const char* endpoint, const char* jsonBody, const char* sessionToken, char* responseBuffer, DWORD bufferSize) {
    HINTERNET hInternet = InternetOpenA("AegisAuth-C/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return 0;

    HINTERNET hConnect = InternetConnectA(hInternet, AUTH_HOST, AUTH_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        return 0;
    }

    char path[256];
    snprintf(path, sizeof(path), "/api/public/v1/%s", endpoint);

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (USE_HTTPS) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path, NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return 0;
    }

    char headers[512];
    time_t now = time(NULL);
    if (sessionToken && strlen(sessionToken) > 0) {
        snprintf(headers, sizeof(headers),
            "Content-Type: application/json\r\nx-app-key: %s\r\nx-timestamp: %lld\r\nx-session-token: %s\r\n",
            APP_KEY, (long long)now, sessionToken);
    } else {
        snprintf(headers, sizeof(headers),
            "Content-Type: application/json\r\nx-app-key: %s\r\nx-timestamp: %lld\r\n",
            APP_KEY, (long long)now);
    }

    DWORD bodyLen = (DWORD)strlen(jsonBody);
    BOOL sendOk = HttpSendRequestA(hRequest, headers, (DWORD)strlen(headers), (LPVOID)jsonBody, bodyLen);

    DWORD totalRead = 0;
    if (sendOk) {
        char chunk[1024];
        DWORD bytesRead = 0;
        while (InternetReadFile(hRequest, chunk, sizeof(chunk) - 1, &bytesRead) && bytesRead > 0) {
            if (totalRead + bytesRead < bufferSize - 1) {
                memcpy(responseBuffer + totalRead, chunk, bytesRead);
                totalRead += bytesRead;
            }
        }
    }
    responseBuffer[totalRead] = '\0';

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    return (totalRead > 0);
}

int main() {
    printf("=== AegisAuth C Client [%s v%s] ===\n", APP_NAME, APP_VERSION);

    char resp[8192];
    char sessionToken[256] = {0};

    // 1. Handshake
    printf("[*] Initializing application...\n");
    if (!HttpPost("init", "{\"version\":\"" APP_VERSION "\"}", NULL, resp, sizeof(resp)) || !strstr(resp, "\"success\":true")) {
        printf("[-] Init failed: %s\n", resp);
        return 1;
    }
    printf("[+] Application initialized successfully.\n");

    // 2. Login
    printf("\n[*] Logging in as '%s'...\n", USERNAME);
    char loginBody[512];
    snprintf(loginBody, sizeof(loginBody), "{\"username\":\"%s\",\"password\":\"%s\",\"hwid\":\"c-client-hwid\"}", USERNAME, PASSWORD);
    if (!HttpPost("login", loginBody, NULL, resp, sizeof(resp)) || !strstr(resp, "\"success\":true")) {
        printf("[-] Login failed: %s\n", resp);
        return 1;
    }

    ExtractJsonString(resp, "token", sessionToken, sizeof(sessionToken));
    printf("[+] Logged in! Session token: %.12s...\n", sessionToken);

    // 3. Request module token for AetherVault
    printf("\n[*] Requesting signed download token for module '%s'...\n", MODULE_NAME);
    char moduleBody[256];
    snprintf(moduleBody, sizeof(moduleBody), "{\"module\":\"%s\"}", MODULE_NAME);
    if (!HttpPost("module/token", moduleBody, sessionToken, resp, sizeof(resp)) || !strstr(resp, "\"success\":true")) {
        printf("[-] Module token request failed: %s\n", resp);
        return 1;
    }

    char downloadUrl[1024] = {0};
    char assetKey[256] = {0};
    ExtractJsonString(resp, "download_url", downloadUrl, sizeof(downloadUrl));
    ExtractJsonString(resp, "key", assetKey, sizeof(assetKey));

    printf("[+] Signed Token Received!\n");
    printf("    Target Asset: %s\n", assetKey);
    printf("    Download URL: %s\n", downloadUrl);
    printf("\n[+] Binary payload authorized for AetherVault silent streaming!\n");

    // 4. Logout
    printf("\n[*] Logging out...\n");
    HttpPost("logout", "{}", sessionToken, resp, sizeof(resp));
    printf("[+] Logged out cleanly.\n");

    return 0;
}
