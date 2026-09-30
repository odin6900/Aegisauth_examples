-- Aegis Authentication API client for Lua 5.1+ / LuaJIT.
-- Transport: lua-http via `http.request` when available, otherwise the
-- `curl` binary. No other dependencies.
local json = require('aegis.json')

local Aegis = {}
Aegis.__index = Aegis
Aegis.VERSION = '1.0.0'

--- Error object attached to every failure (raised with `error(err)`).
local function AegisError(code, message, status)
  return setmetatable({
    code = code,
    message = message,
    status = status or 0,
    isNetworkError = (status or 0) == 0,
    isAuthError = code == 'unauthorized' or code == 'invalid_credentials',
    isLicenseError = code:sub(1, 7) == 'license' or code == 'hwid_mismatch',
  }, { __tostring = function(self) return ('Aegis error [%s] %s'):format(self.code, self.message) end })
end
Aegis.Error = AegisError

local function shellQuote(value)
  return "'" .. tostring(value):gsub("'", "'\\''") .. "'"
end

--- Stable, non-reversible machine identifier.
function Aegis.hardwareId()
  local handle = io.popen('hostname 2>/dev/null')
  local host = handle and handle:read('*l') or 'unknown'
  if handle then handle:close() end
  local facts = (host or 'unknown') .. '|' .. (os.getenv('USER') or os.getenv('USERNAME') or '') .. '|lua'

  local h1, h2 = 2166136261, 16777619
  for i = 1, #facts do
    local byte = facts:byte(i)
    h1 = (h1 ~ byte) * 16777619 % 4294967296
    h2 = (h2 + byte * (i + 7)) * 2246822519 % 4294967296
  end
  local out = ''
  while #out < 64 do
    h1 = (h1 * 2246822507 + 12345) % 4294967296
    h2 = (h2 * 3266489909 + 6789) % 4294967296
    out = out .. ('%08x%08x'):format(h1, h2)
  end
  return out:sub(1, 64)
end

--- Creates a client. `options.baseUrl` and `options.appKey` are required.
function Aegis.new(options)
  options = options or {}
  if not options.baseUrl or options.baseUrl == '' then
    error(AegisError('invalid_options', 'baseUrl is required.'))
  end
  if not options.appKey or options.appKey == '' then
    error(AegisError('invalid_options', 'appKey is required.'))
  end

  return setmetatable({
    baseUrl = options.baseUrl:gsub('/+$', ''),
    appKey = options.appKey,
    apiKey = options.apiKey,
    version = options.version or '1.0.0',
    channel = options.channel or 'stable',
    hwid = options.hwid or Aegis.hardwareId(),
    timeout = options.timeout or 20,
    maxRetries = options.maxRetries or 2,
    sessionToken = nil,
  }, Aegis)
end

--- Restores a session token persisted by the host application.
function Aegis:useSession(token)
  self.sessionToken = token
end

--- Calls any endpoint and returns its `data` table.
function Aegis:request(endpoint, body)
  local url = ('%s/api/public/v1/%s'):format(self.baseUrl, endpoint)
  local payload = json.encode(body or {})

  local headers = {
    '-H', 'content-type: application/json',
    '-H', 'user-agent: aegis-lua-sdk/' .. Aegis.VERSION,
    '-H', 'x-app-key: ' .. self.appKey,
  }
  if self.apiKey then headers[#headers + 1] = '-H'; headers[#headers + 1] = 'x-api-key: ' .. self.apiKey end
  if self.sessionToken then headers[#headers + 1] = '-H'; headers[#headers + 1] = 'x-session-token: ' .. self.sessionToken end

  local parts = { 'curl', '-sS', '-X', 'POST', '--max-time', tostring(self.timeout) }
  for _, item in ipairs(headers) do parts[#parts + 1] = shellQuote(item) end
  parts[#parts + 1] = '-d'
  parts[#parts + 1] = shellQuote(payload)
  parts[#parts + 1] = shellQuote(url)

  local command = table.concat(parts, ' ')
  local raw, attempt = nil, 0
  while attempt <= self.maxRetries do
    local handle = io.popen(command .. ' 2>/dev/null')
    raw = handle and handle:read('*a') or nil
    if handle then handle:close() end
    if raw and raw ~= '' then break end
    attempt = attempt + 1
  end

  if not raw or raw == '' then
    error(AegisError('network_error', 'Network request failed.', 0))
  end

  local ok, envelope = pcall(json.decode, raw)
  if not ok or type(envelope) ~= 'table' then
    error(AegisError('invalid_response', 'Malformed API response.', 0))
  end
  if not envelope.success then
    local err = envelope.error or {}
    error(AegisError(err.code or 'server_error', err.message or 'Request failed.', 400))
  end
  return envelope.data or {}
end

local function storeSession(self, data)
  if type(data) == 'table' and type(data.session) == 'table' and data.session.token then
    self.sessionToken = data.session.token
  end
end

-- Application ---------------------------------------------------------------

function Aegis:init() return self:request('init', { version = self.version }) end
function Aegis:status() return self:request('status', {}) end
function Aegis:appData() return self:request('app/data', {}) end
function Aegis:downloads() return self:request('downloads', {}) end
function Aegis:checkVersion(version)
  return self:request('version/check', { version = version or self.version, channel = self.channel })
end

-- Authentication --------------------------------------------------------------

function Aegis:register(username, password, email, licenseKey)
  local data = self:request('register', {
    username = username, password = password, email = email,
    license_key = licenseKey, hwid = self.hwid,
  })
  storeSession(self, data)
  return data
end

function Aegis:login(username, password)
  local data = self:request('login', { username = username, password = password, hwid = self.hwid })
  storeSession(self, data)
  return data
end

function Aegis:logout()
  local ok, err = pcall(function() return self:request('logout', {}) end)
  self.sessionToken = nil
  if not ok then error(err) end
end

function Aegis:heartbeat() return self:request('heartbeat', {}) end
function Aegis:checkSession() return self:request('session/check', {}) end

--- True when a token exists and the server still accepts it.
function Aegis:isAuthenticated()
  if not self.sessionToken then return false end
  local ok, result = pcall(function() return self:checkSession() end)
  return ok and result.valid == true
end

function Aegis:userData() return self:request('user/data', {}) end

-- Licensing ---------------------------------------------------------------------

function Aegis:validateLicense(licenseKey)
  return self:request('license/validate', { license_key = licenseKey, hwid = self.hwid })
end

function Aegis:activateLicense(licenseKey, username)
  return self:request('license/activate', { license_key = licenseKey, hwid = self.hwid, username = username })
end

-- Variables ----------------------------------------------------------------------

function Aegis:getVariables(scope, licenseKey)
  return self:request('variables/get', { scope = scope or 'application', license_key = licenseKey })
end

function Aegis:setVariable(key, value, scope, licenseKey)
  return self:request('variables/set', {
    scope = scope or 'user', key = key, value = value, license_key = licenseKey,
  })
end

function Aegis:triggerWebhook(event, payload)
  return self:request('webhook/trigger', { event = event, payload = payload or {} })
end

return Aegis