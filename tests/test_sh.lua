require'civ':grequire(); local mod
test('load', nil, function() mod = require('sh') end)
local posix = require'posix'
local shix  = require'shix'

local sh, shCmd, assertSh = mod.sh, mod.shCmd, mod.assertSh
local tfmt = mod.tfmt

test('tfmt', nil, function()
  assertEq('{1; 2}',        tfmt{1, 2})
  assertEq('{1; 2 :: a=5}', tfmt{1, 2, a=5})
end)

local EMBEDDED = [[
 echo $(cat << __LUA_EOF__
foo
bar

__LUA_EOF__
) ]]

test('embedded', nil, function()
  assertEq(EMBEDDED, mod.embedded('foo\nbar\n'))
end)

test('fork', nil, function()
  local fork = shix.Fork(true, true)
  local p = fork.pipes
  assert(not p.lr and not p.lw)
  if not fork.isParent then
    local got, err =  p.r:read('a')
    assertEq(nil, err)
    io.stderr:write('child heard: ', tostring(got), tostring(err), '\n')
    io.stdout:write('I heard: '..got)
    p:close()
    os.exit(42)
  end
  p.w:write('to my child');  p.w:close()
  local got, err = p.r:read('a');
  assertEq(nil, err)
  assertEq('I heard: to my child', got)
  print('closing:', p.r:close())
  assert(fork:wait())
end)

test('exec', nil, function()
  local fork = shix.Fork(true)
  local p = fork.pipes
  assert(not (p.lr or p.lw))
  if not fork.isParent then
    assert(not p.r)
    print("Child executing")
    io.stderr:write'Child executing (stderr)\n'
    fork:exec([[
      SANTA='Santa Clause'; echo "Hi I am $SANTA, ho ho ho"
    ]])
  end
  assert(not p.w)
  local got, err = p.r:read('a')
  assertEq(nil, err)
  assertEq('Child executing\nHi I am Santa Clause, ho ho ho\n',
           got)
  assert(fork:wait())
end)

test('sh', nil, function()
  assertEq('on stdout\n', sh[[ echo 'on' stdout ]])
  assertEq(''           , sh[[ echo '<stderr from test>' 1>&2 ]])
  local _, rc = sh('false', {check=false}); assertEq(1, rc)

  local cmd = {'echo', foo='bar'}
  assertEq("echo --foo='bar'", shCmd{'echo', foo='bar'})
  assertEq('--foo=bar\n'  ,  sh{'echo', foo='bar'})
  assert(select(3, shCmd{foo="that's bad"})) -- assert error
end)
