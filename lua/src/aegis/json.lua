-- Minimal JSON encoder/decoder for the Aegis Lua SDK.
-- Pure Lua 5.1+/LuaJIT, no dependencies.
local json = {}

local function encodeString(value)
  return '"' .. value:gsub('[%c"\\]', function(c)
    local map = { ['"'] = '\\"', ['\\'] = '\\\\', ['\n'] = '\\n', ['\r'] = '\\r', ['\t'] = '\\t' }
    return map[c] or string.format('\\u%04x', c:byte())
  end) .. '"'
end

local function isArray(value)
  local count = 0
  for _ in pairs(value) do count = count + 1 end
  return count == #value
end

--- Serialises a Lua value to JSON text.
function json.encode(value)
  local kind = type(value)
  if value == nil or value == json.null then return 'null' end
  if kind == 'boolean' then return tostring(value) end
  if kind == 'number' then return string.format('%.14g', value) end
  if kind == 'string' then return encodeString(value) end
  if kind ~= 'table' then error('Cannot encode ' .. kind .. ' as JSON.') end

  local parts = {}
  if isArray(value) then
    for _, item in ipairs(value) do parts[#parts + 1] = json.encode(item) end
    return '[' .. table.concat(parts, ',') .. ']'
  end
  for key, item in pairs(value) do
    parts[#parts + 1] = encodeString(tostring(key)) .. ':' .. json.encode(item)
  end
  return '{' .. table.concat(parts, ',') .. '}'
end

local decodeValue

local function skip(text, pos)
  local _, stop = text:find('^[ \n\r\t]*', pos)
  return (stop or pos - 1) + 1
end

local function decodeString(text, pos)
  pos = pos + 1
  local out = {}
  while pos <= #text do
    local c = text:sub(pos, pos)
    if c == '"' then return table.concat(out), pos + 1 end
    if c == '\\' then
      local escape = text:sub(pos + 1, pos + 1)
      local map = { n = '\n', t = '\t', r = '\r', b = '\b', f = '\f' }
      if escape == 'u' then
        out[#out + 1] = string.char(tonumber(text:sub(pos + 2, pos + 5), 16) % 256)
        pos = pos + 6
      else
        out[#out + 1] = map[escape] or escape
        pos = pos + 2
      end
    else
      out[#out + 1] = c
      pos = pos + 1
    end
  end
  error('Unterminated JSON string.')
end

decodeValue = function(text, pos)
  pos = skip(text, pos)
  local c = text:sub(pos, pos)

  if c == '{' then
    local object = {}
    pos = skip(text, pos + 1)
    if text:sub(pos, pos) == '}' then return object, pos + 1 end
    while true do
      local key
      pos = skip(text, pos)
      key, pos = decodeString(text, pos)
      pos = skip(text, pos)
      pos = pos + 1 -- ':'
      object[key], pos = decodeValue(text, pos)
      pos = skip(text, pos)
      local sep = text:sub(pos, pos)
      pos = pos + 1
      if sep == '}' then return object, pos end
      if sep ~= ',' then error('Malformed JSON object.') end
    end
  end

  if c == '[' then
    local array = {}
    pos = skip(text, pos + 1)
    if text:sub(pos, pos) == ']' then return array, pos + 1 end
    while true do
      local item
      item, pos = decodeValue(text, pos)
      array[#array + 1] = item
      pos = skip(text, pos)
      local sep = text:sub(pos, pos)
      pos = pos + 1
      if sep == ']' then return array, pos end
      if sep ~= ',' then error('Malformed JSON array.') end
    end
  end

  if c == '"' then return decodeString(text, pos) end
  if text:sub(pos, pos + 3) == 'true' then return true, pos + 4 end
  if text:sub(pos, pos + 4) == 'false' then return false, pos + 5 end
  if text:sub(pos, pos + 3) == 'null' then return nil, pos + 4 end

  local number = text:match('^-?%d+%.?%d*[eE]?[-+]?%d*', pos)
  if number then return tonumber(number), pos + #number end
  error('Unexpected JSON token at position ' .. pos)
end

--- Parses JSON text into Lua values.
function json.decode(text)
  if text == nil or text == '' then return {} end
  local value = decodeValue(text, 1)
  return value or {}
end

return json