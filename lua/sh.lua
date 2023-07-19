-- sh.lua: use lua as your shell
--
-- local shmod = require('sh')
-- local sh = shmod.sh
-- sh[[ echo 'foo' bar ]]
--
-- shmod.SET.debug = true            -- turn on shell debug
-- sh{[[ echo 'foo' bar ]], debug=true)   -- force debug
-- out, rc = sh{[[ false ]], check=false) -- don't check rc

local SET = {
  debug=false, host=false,
}

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

local function quote(v)
  v = asStr(v)
  if string.match(v, "'") then return nil end -- cannot quote values with '
  return "'" .. asStr(v) .. "'"
end

local EMBED0 = ' echo $(cat << __LUA_EOF__\n'
local EMBED1 = '\n__LUA_EOF__\n) '
local function embedded(s) return EMBED0 .. asStr(s) .. EMBED1 end

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
local function shCmd(cmd, set)
  set = set or {}
  if nil == set.debug then set.debug = SET.debug end
  if nil == set.check and not set.raw then set.check = true end
  if 'string' == type(cmd) then cmd = {cmd}
  else                          cmd = tcopy(cmd) end
  if set.stdin then cmd = insertStdin(cmd, set.stdin) end

  for k, v in pairs(cmd) do
    if 'string' == type(k) then
      v = quote(v); if not v then
        return nil, nil, 'flag "'..k..'" has single quote character'
      end
      add(cmd, concat({'--', k, '=', v}))
    end
  end
  return concat(cmd, ' '), set
end

local function _sh(cmd, set, err)
  if err then error(err) end
  if set.debug then
    o = o or io.stderr
    o:write('[==[ ') o:write(cmd) o:write(' ]==]\n')
  end
  local o = set.out -- output
  if set.raw then return execute(cmd, true) end
  local out, rc = execute(cmd); if o then o:write(out) end
  if set.check and 0 ~= rc[3] then
    error(string.format(
      '!! rc=%s for command [[ %s ]] !!\n%s', rc[3], cmd, out))
  end
  return out, rc[3]
end
-- Run a command on the bash shell
local function sh(cmd, set)
  local cmd, set, err = shCmd(cmd, set);
  return _sh(cmd, set, err)
end

SET.PWD = string.match(sh'echo $PWD', '^%s*.-%s*$')

-- Run on a command (over ssh) on an eXternal shell
-- This uses the global value SET.host to get the host
-- It also changes the directory to the current directory
-- (set in SET.PWD)
local function xsh(cmd, set)  set = set or {}
  set.host = set.host or SET.host
  if not set.host then error("Must set a host") end
  local cmd, set, err = shCmd(cmd, set); if err then error(err) end
  local x = {}
  extend(x, {'ssh -o LogLevel=QUIET -t ', set.host, ' '})
  add(x, embedded{'cd ', SET.PWD, ' && ', cmd})
  return _sh(asStr(x), set)
end

-- "user" variants that write to stdout
local function shu(cmd, set) io.stdout:write(sh(cmd, set)) end
local function xshu(cmd, set) io.stdout:write(xsh(cmd, set)) end

return {
  shu=shu, xshu=xshu,
  sh=sh, xsh=xsh, shCmd=shCmd, tfmt=tfmt,
  quoted=quoted, embedded=embedded,
  SET=SET,
  extend=extend, asStr=asStr,
  addStdin=addStdin,
}
