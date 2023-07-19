require('gciv'); local mod
test('load', nil, function() mod = require('sh') end)

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

test('sh', nil, function()
  assertEq('on stdout\n', sh[[ echo 'on' stdout ]])
  assertEq(''           , sh[[ echo '<stderr from test>' 1>&2 ]])
  local _, rc = sh('false', {check=false}); assertEq(1, rc)

  local cmd = {'echo', foo='bar'}
  assertEq("echo --foo='bar'", shCmd{'echo', foo='bar'})
  assertEq('--foo=bar\n'  ,  sh{'echo', foo='bar'})
  assert(select(3, shCmd{foo="that's bad"})) -- assert error
end)
