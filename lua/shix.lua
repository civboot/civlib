local civ = require'civ'
local posix = civ.want'posix'
if not posix then
  print'posix not available'
  return nil
end

local std_r, std_w, std_lw = 0, 1, 2
if posix then
  assert(std_r  == posix.fileno(io.stdin))
  assert(std_w  == posix.fileno(io.stdout))
  assert(std_lw == posix.fileno(io.stderr))
end

local function sleep(duration)
  posix.nanosleep(duration.s, duration.ns)
end

-- Return the epoch time
local function epoch()
  local s, ns, errnum = posix.clock_gettime(posix.CLOCK_REALTIME)
  assert(s, ns, errnum)
  return civ.Epoch{s=s, ns=ns}
end

local Pipe = civ.struct('Pipe', {{'fd', Num}, {'closed', Bool, false}})
civ.method(Pipe, 'close', function(self)
  if not self.closed then
    posix.close(self.fd); self.closed = true
  end
end)
civ.method(Pipe, 'read', function(self, m)
  local i = 1
  if('a' == m or 'a*' == m) then
    local t = {};
    while true do
      io.stderr:write('reading ', self.fd, '\n')
      i = i + 1
      local s, err = posix.read(self.fd, 1024)
      if nil == s then return nil, err end
      table.insert(t, s)
      if '' == s then return table.concat(t) end
    end
  end
  return posix.read(self.fd, 1024)
end)

civ.method(Pipe, 'write', function(self, w)
  return posix.write(self.fd, w)
end)

local function pipe()
  local r, w = posix.pipe()
  assert(r, 'no pipe')
  print('got pipe', r, w)
  return r, w
end

-- r = read from pipe  (stdout)
-- w = write to pipe   (stdin)
-- lw = log write pipe (stderr)
-- lr = log read pipe  (parent reading child stderr)
local Pipes = civ.struct('Pipes', {'r', 'w', 'lr', 'lw'})
civ.method(Pipes, 'from', function(fds)
  local p = Pipes{}; for k, fd in pairs(fds) do
    p[k] = Pipe{fd=fd}
  end;
  print('Pipes', p)
  return p;
end)
-- close all the pipes (in parent or child)
civ.method(Pipes, 'close', function(p)
  if p.r  then p.r:close()  ; p.r  = nil end
  if p.w  then p.w:close()  ; p.w  = nil end
  if p.lr then p.lr:close() ; p.lr = nil end
  if p.lw then p.lw:close() ; p.lw = nil end
end)
civ.method(Pipes, 'dupStd', function(p)
  -- dup pipes to std file descriptors
  if p.r  then posix.dup2(p.r.fd,  std_r) end
  if p.w  then posix.dup2(p.w.fd,  std_w) end
  if p.lw then posix.dup2(p.lw.fd, std_lw) end
end)

local Fork = civ.struct('Fork', {
  {'isParent', Bool, false}, {'cpid', Num},
  {'pipes', Pipes, false},
  {'status', Str, 'running'},
  {'rc', Num, false},
})

-- Fork object with pipes for each
-- r=true creates parent.r and child.w (parent reads child's stdout)
civ.constructor(Fork, function(ty_, r, w, l)
  local parent, child = {}, {}
  if r then parent.r , child.w  = pipe() end
  if w then child.r,   parent.w = pipe() end
  if l then parent.lr, child.lw = pipe() end
  parent, child = Pipes.from(parent), Pipes.from(child)
  print('pipes', parent, child)

  local self = {cpid = posix.fork()}
  if(not self.cpid) then error('fork failed') end

  if 0 == self.cpid then -- is child
    parent:close()
    self.pipes = child
    child:dupStd()
  else
    child:close()
    self.pipes = parent
    self.isParent = true
  end
  return setmetatable(self, ty_)
end)
-- call posix.wait and return isDone, err, errNum
civ.method(Fork, 'wait', function(self)
  assert(self.isParent, 'wait() can only be called on parent')
  assert(self.status == 'running', "wait called when child completed")
  self.pipes:close()
  local a, b, c = posix.wait(self.cpid)
  if nil == a then self.status = 'error'; return nil, b, c end
  if 'running' == b then return false end
  print('done ', a, b, c)
  self.rc = c; self.status = b; return true
end)

-- Execute a shell command on the child. This terminates
-- the process (since exec doesn't typically return).
civ.method(Fork, 'exec', function(self, cmd)
  assert(not self.isParent, 'exec can only be called on child')
  local a, err = posix.exec(
    '/bin/sh', {'-c', cmd, nil})
  if nil == a then error(err) end
  os.exit(42)
end)

-- read stdin and close it, returning the return code
local function rclose(r)
  print('closing ', r, type(r))
  if 'number' == type(r) then
    print('closing number', r)
    assert(false)
  end
  return r:read('*a'), select(3, r:close())
end

return {
  Fork=Fork,
  Pipes=Pipes,
  Pipe=Pipe,
  sleep=sleep,
  std_r, std_w, std_lw,
}
