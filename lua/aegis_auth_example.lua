--[[
  AegisAuth Lua Example
  Demonstrates initialization, user login, license validation, and requesting a signed AetherVault module token.
  Compatible with Lua 5.1, 5.2, 5.3, 5.4, and LuaJIT.
]]--

-- Configuration Placeholders
local AUTH_URL    = "https://auth.example.com"
local APP_KEY     = "YOUR_APP_KEY"
local APP_NAME    = "My Application"
local APP_VERSION = "1.0.0"

local USERNAME    = "YOUR_USERNAME"
local PASSWORD    = "YOUR_PASSWORD"
local MODULE_NAME = "minecraft"

-- Helper to extract JSON string property
local function extract_json(str, key)
    local pattern = '"' .. key .. '"%s*:%s*"([^"]+)"'
    local match = string.match(str, pattern)
    return match
end

-- Helper to execute curl for cross-platform HTTPS without binary C module compilation
local function http_post(endpoint, payload_json, session_token)
    local url = string.gsub(AUTH_URL, "/+$", "") .. "/api/public/v1/" .. endpoint
    local timestamp = os.time()

    local cmd = 'curl -s -X POST "' .. url .. '"' ..
                ' -H "Content-Type: application/json"' ..
                ' -H "x-app-key: ' .. APP_KEY .. '"' ..
                ' -H "x-timestamp: ' .. timestamp .. '"' ..
                ' -H "User-Agent: AegisAuth-Lua/' .. APP_VERSION .. '"'

    if session_token and #session_token > 0 then
        cmd = cmd .. ' -H "x-session-token: ' .. session_token .. '"'
    end

    -- Escape quotes in payload for shell
    local escaped_payload = string.gsub(payload_json, '"', '\\"')
    cmd = cmd .. ' -d "' .. escaped_payload .. '"'

    local handle = io.popen(cmd)
    local result = handle:read("*a")
    handle:close()
    return result
end

print("=== AegisAuth Lua Client [" .. APP_NAME .. " v" .. APP_VERSION .. "] ===")
local hwid = "lua-hwid-" .. tostring(os.time())
print("[*] HWID: " .. hwid)

-- 1. Handshake
print("\n[*] Initializing application...")
local init_res = http_post("init", '{"version":"' .. APP_VERSION .. '"}')
if not string.find(init_res, '"success":true') then
    print("[-] Init failed: " .. tostring(init_res))
    os.exit(1)
end
print("[+] Handshake successful.")

-- 2. Login
print("\n[*] Logging in as '" .. USERNAME .. "'...")
local login_body = '{"username":"' .. USERNAME .. '","password":"' .. PASSWORD .. '","hwid":"' .. hwid .. '"}'
local login_res = http_post("login", login_body)
if not string.find(login_res, '"success":true') then
    print("[-] Login failed: " .. tostring(login_res))
    os.exit(1)
end

local session_token = extract_json(login_res, "token") or ""
print("[+] Login successful! Session token: " .. string.sub(session_token, 1, 12) .. "...")

-- 3. Request module token for AetherVault
print("\n[*] Requesting signed download token for module '" .. MODULE_NAME .. "'...")
local token_body = '{"module":"' .. MODULE_NAME .. '"}'
local token_res = http_post("module/token", token_body, session_token)
if not string.find(token_res, '"success":true') then
    print("[-] Module token request failed: " .. tostring(token_res))
    os.exit(1)
end

local asset_key = extract_json(token_res, "key") or ""
local download_url = extract_json(token_res, "download_url") or ""

print("[+] Signed Token Received!")
print("    Target Asset: " .. asset_key)
print("    Download URL: " .. download_url)
print("\n[+] Binary payload authorized for AetherVault silent streaming!")

-- 4. Logout
print("\n[*] Logging out...")
http_post("logout", "{}", session_token)
print("[+] Logged out cleanly.")
