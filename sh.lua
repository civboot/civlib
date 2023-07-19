-- sh.lua: use lua as your shell
--
-- local shmod = require('sh'); local sh = shmod.sh
-- sh[[ echo 'foo' bar ]]
--
-- shmod.SETTINGS.debug = false -- turn off debug for production aps
-- sh{[[ echo 'foo' bar ]], debug=true) -- force debug
-- out, rc = sh{[[ false ]], check=true) -- don't check rc

local SETTINGS = {
  debug=false, xsh=false
}

local MULLI_HEADER = 'echo $(cat << __LUA_EOF__\n'
local MULLI_FOOTER = '\n__LUA_EOF__\n) '

local add, concat = table.insert, table.concat

local function extend(t, a)
  for _, v in ipairs(a) do add(t, v) end
end

local function pop(t, ...)
  local o = {}; for _, k in ipairs({...}) do
    o[k] = t[k]; t[k] = nil;
  end; return o
end

local function tcopy(t)
  local o = {}; for k, v in pairs(t) do o[k] = v end
  return o
end

local function tfmt(t)
  if type(t) ~= 'table' then return tostring(t) end
  local out = {'{'}; for i, v in ipairs(t) do
    add(out, v); add(out, '; ');
  end
  if #out > 1 then out[#out] = ' :: ' end
  for k, v in pairs(t) do
    if type(k) == 'number' and k <= #t then -- already concat
    else
      extend(out, {tostring(k), '=', tfmt(v), '; '})
    end
    local lastI = 0
  end
  out[#out] = '}'
  return concat(out, '')
end

local function asStr(v)
  if 'table' == type(v) then        return concat(v)
  elseif 'userdata' == type(v) then return v:read('*a') end
  return v
end

-- append the string necessary for an echo to work
local function insertStdin(cmd, stdin)
  local out = {}
  extend(out, {MULTI_HEADER, asStr(stdin), MULTI_FOOTER})
  extend(out, cmd)
  return out
end

-- execute a command, return the output and rc
local function execute(c, raw)
  local f = assert(io.popen(asStr(c)), 'r')
  if raw then return f end
  return f:read('*a'), {f:close()}
end

-- Just get the command, don't do anything
--
-- returns cmdSettings, cmdBuf
local function shCmd(cmd)
  print('got cmd', tfmt(cmd), cmd.foo)
  if 'string' == type(cmd) then cmd = {cmd, debug=SETTINGS.debug}
  else                          cmd = tcopy(cmd) end
  if nil == cmd.check and not cmd.raw then cmd.check = true end
  local cmd, set = cmd, pop(cmd, 'debug', 'check', 'raw', 'stdin')
  if set.stdin then cmd = insertStdin(cmd, set.stdin) end
  for k, v in pairs(cmd) do
    if 'string' == type(k) then
      add(cmd, concat({'--', k, '=', tostring(v)}))
    end
  end
  return concat(cmd, ' '), set
end

local function shDebug(cmd, set)
  if set.debug then
    o = o or io.stderr
    o:write('[==[ ') o:write(cmd) o:write(' ]==]\n')
  end
end

-- Run a command on the bash shell
local function sh(cmd)
  local cmd, set = shCmd(cmd)
  local o = set.out -- output
  if set.raw then return execute(cmd, true) end
  local out, rc = execute(cmd); if o then o:write(out) end
  if set.check and 0 ~= rc[3] then
    error(string.format(
      '!! rc=%s for command [[ %s ]] !!\n%s', rc[3], cmd, out))
  end
  return out, rc[3]
end

-- Run on a command on an eXternal shell
-- this uses the global value SETTINGS.xsh to get the host
local function xsh(cmd, host)
  host = host or SETTINGS.xsh
  if not host then error("Must provide host or set in SETTINGS.xsh") end
end

return {
  sh=sh, shCmd=shCmd, tfmt=tfmt,
  quoted=quoted,
  SETTINGS=SETTINGS,
  extend=extend, asStr=asStr,
  addStdin=addStdin,
}
