
require'civ':grequire()
local posix = require'posix'


local std_r = posix.fileno(io.stdin)
local std_w = posix.fileno(io.stdout)
local std_l = posix.fileno(io.stderr)

assert(0 == std_r)
assert(1 == std_w)
assert(2 == std_l)

local parent_r, child_w  = posix.pipe()
local child_r , parent_w = posix.pipe()
assert(parent_r and parent_w)
assert(child_r and child_w)

local cpid = posix.fork() -- child pid
assert(cpid)

local function read(fd)
   local outs, errmsg = posix.read(fd, 1024)
   assert(outs ~= nil, errmsg)
   return outs
end

if 0 == cpid then -- child
  print('child', cpid)
  posix.close(parent_r)
  posix.close(parent_w)
  -- setup r (stdin) and w (stdout) of the child
  posix.dup2(child_r, std_r)
  posix.dup2(child_w, std_w)

  local got = 'I heard: ' .. io.stdin:read('a')
  io.stdout:write(got)
  io.stderr:write('child wrote: ', got, '\n')
  os.exit(0)
else -- parent
  print('parent', cpid)
  posix.close(child_r)
  posix.close(child_w)

  local msg = 'to my child'
  posix.write(parent_w, msg)
  posix.close(parent_w)
  local got = read(parent_r);
  posix.close(parent_r)

  io.stderr:write('parent read: ', got, '\n')
  assert(got == 'I heard: to my child')
  print('parent can still print stdout')
end
