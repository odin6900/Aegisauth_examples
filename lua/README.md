# Aegis SDK for Lua

Official client for the Aegis Authentication API. Lua 5.1+ / LuaJIT, pure Lua
with a bundled JSON codec; HTTP goes through the `curl` binary.

## Contents

```
src/aegis/init.lua       Client with every API operation
src/aegis/json.lua       Bundled JSON encoder/decoder
examples/quickstart.lua  Runnable sample application
aegis-1.0.0.rockspec     Optional LuaRocks build from this folder
```

## Install

No package registry — unzip and add `src/` to your Lua path:

```bash
lua -e "package.path='src/?.lua;src/?/init.lua;'..package.path" examples/quickstart.lua
```

## Quickstart

```lua
local Aegis = require('aegis')

local aegis = Aegis.new({ baseUrl = 'https://your-aegis-host', appKey = APP_KEY, version = '1.0.0' })
aegis:init()

local auth = aegis:login('ada', password)
print('signed in as ' .. auth.user.username)

local license = aegis:validateLicense('AEGS-4K7P-2M9X-QT31')
aegis:setVariable('last_level', '12')
aegis:logout()
```

## Supported operations

`init`, `status`, `appData`, `register`, `login`, `logout`, `heartbeat`,
`checkSession`, `isAuthenticated`, `useSession`, `userData`, `validateLicense`,
`activateLicense`, `getVariables`, `setVariable`, `checkVersion`, `downloads`,
`triggerWebhook`, plus `request()` for any endpoint added later.

## Error handling

```lua
local ok, err = pcall(function() return aegis:login(username, password) end)
if not ok then
  if err.code == 'hwid_mismatch' then ui.show('Locked to another machine.')
  elseif err.isNetworkError then ui.show('Aegis is unreachable — retrying.')
  else error(err) end
end
```

## License

MIT — see `LICENSE`.
