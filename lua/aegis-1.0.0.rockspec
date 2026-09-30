package = "aegis"
version = "1.0.0-1"

source = {
  url = "file://.",
}

description = {
  summary = "Official Aegis Authentication API SDK for Lua",
  detailed = "Pure Lua client covering initialization, authentication, licensing, sessions, variables, version checks and download information.",
  license = "MIT",
}

dependencies = {
  "lua >= 5.1",
}

build = {
  type = "builtin",
  modules = {
    ["aegis"] = "src/aegis/init.lua",
    ["aegis.json"] = "src/aegis/json.lua",
  },
}