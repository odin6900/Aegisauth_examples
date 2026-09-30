-- Runnable quickstart for the Aegis Lua SDK.
--
--   AEGIS_APP_KEY=... lua -e "package.path='src/?.lua;src/?/init.lua;'..package.path" examples/quickstart.lua
package.path = 'src/?.lua;src/?/init.lua;' .. package.path

local Aegis = require('aegis')

local function envOr(key, fallback)
  local value = os.getenv(key)
  if value == nil or value == '' then return fallback end
  return value
end

local aegis = Aegis.new({
  baseUrl = envOr('AEGIS_BASE_URL', 'http://localhost:8080'),
  appKey = envOr('AEGIS_APP_KEY', ''),
  version = '1.0.0',
})

local ok, err = pcall(function()
  local info = aegis:init()
  print('initialized: ' .. tostring(info.status))

  if info.version and info.version.update_required then
    print('mandatory update: ' .. tostring(info.version.latest))
    return
  end

  local auth = aegis:login(envOr('AEGIS_USERNAME', 'demo'), envOr('AEGIS_PASSWORD', 'demo-password'))
  print('signed in as ' .. tostring(auth.user and auth.user.username))

  aegis:setVariable('last_seen', os.date('!%Y-%m-%dT%H:%M:%SZ'))
  print('authenticated: ' .. tostring(aegis:isAuthenticated()))

  aegis:heartbeat()
  aegis:logout()
  print('signed out.')
end)

if not ok then
  io.stderr:write(tostring(err) .. '\n')
  os.exit(1)
end