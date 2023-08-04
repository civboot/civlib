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

local civ = require'civ'
local shix = require'shix'

local add, concat = table.insert, table.concat

local ShSet = civ.struct('ShSet', {
  {'check', Bool, true},  -- check the rc
  {'inp',   Str,  false}, -- value to send to process input
  {'err',   Bool, false}, -- get stderr
  {'debug', Bool, false}, -- log all inputs/outputs
  {'log',   nil,  io.stderr}, -- pipe to debug/log to

  -- return out.fork. Don't get the output or stop the fork.
  {'fork',  Bool, false},

  -- require fork=true. Will cause fork to have a pipe with
  -- the corresponding field/s
  {'w',     nil, false},  -- create/use a write pipe
  {'lr',    nil, false},  -- create/use a log-read (stderr) pipe

  -- Used for xsh
  {'host',  Str,  false},
})

local ShResult = civ.struct('ShResult', {
  {'status', Str}, -- the status of the fork

  -- These are only available when fork=false
  {'rc',  Num, false}, -- the return code
  {'out', Str, false}, -- the output
  {'err', Str, false}, -- the stderr

  -- These can only be available when fork=true
  -- Note: fork.pipes will have the requested {r, w, lr}
  {'fork', shix.Fork, false},
})

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

-- execute a command, using lua's shell
-- return the output and rc
local function luash(c)
  local f = assert(io.popen(asStr(c)), 'r')
  return f:read('*a'), {f:close()}
end

-- Just get the command, don't do anything
--
-- returns cmdSettings, cmdBuf
local function shCmd(cmd, set)
  set = ShSet(set or {})
  if nil == set.debug then set.debug = SET.debug end
  if nil == set.check and not set.fork then set.check = true end
  if set.check then assert(not set.fork) end
  if 'string' == type(cmd) then cmd = {cmd}
  else                          cmd = tcopy(cmd) end
  assert(not (set.fork and set.err), "cannot set fork and err")

  for k, v in pairs(cmd) do
    if 'string' == type(k) then
      v = quote(v); if not v then
        return nil, nil, 'flag "'..k..'" has single quote character'
      end
      add(cmd, concat({'--', k, '=', v}))
    end
  end
  cmd = concat(cmd, ' ')
  if '' == string.gsub(cmd, '%s', '') then
    error('cannot execute an empty command')
  end
  return cmd, set
end

local function _sh(cmd, set, err)
  if err then error(err) end
  local log = set.log -- output
  if set.debug then
    log = log or io.stderr
    log:write('[==[ ', cmd, ' ]==]\n')
  end

  local res = ShResult{status='not-started'}
  if not shix then
    assert(not (set.inp or set.fork or set.w or set.lr),
           'requires posix for: inp, fork, w, lr')
    local out, rc = luash(cmd)
    res = ShResult{out=out, rc=rc}
  else
    local f = shix.Fork(true, set.w or set.inp, set.lr or set.err)
    if not f.isParent then f:exec(cmd) end
    res.status = 'started'
    if set.inp then
      -- write+close input so process can function
      f.pipes.w:write(set.inp)
      f.pipes.w:close(); f.pipes.w = nil
    end
    if set.fork then res.fork = f
    else
      res.out = f.pipes.r:read('a')
      if set.err then res.err = f.pipes.lr:read('a') end
      while not f:wait() do shix.sleep(0.05) end
      res.rc = f.rc
    end
    res.status = f.status
  end
  if log and res.out then log:write(res.out, '\n') end
  if set.check and res.rc ~= 0 then error(
    'non-zero return code: ' .. tostring(res.rc)
  )end
  return res
end

-- Run a command on the bash shell
local function sh(cmd, set)
  local cmd, set, err = shCmd(cmd, set);
  return _sh(cmd, set, err)
end

SET.PWD = string.match(sh'echo $PWD'.out, '^%s*.-%s*$')

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
