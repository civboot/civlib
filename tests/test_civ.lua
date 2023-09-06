local g = {}; for k in pairs(_G) do g[k] = true end

-- assert Set first, since we are going to use it
local civ = require('civ')
local s = civ.Set{'a', 'b', 'c'}
assert(s.a)
civ.assertEq(civ.Set{'a', 'b', 'c'}, s)
local s2 = civ.Set{'b', 'c', 'd'}
civ.assertEq(s:union(s2), civ.Set{'b', 'c'})
civ.assertGlobals(g)
civ:grequire(); g = globals()

test("util", nil, function()
  local t1, t2 = {1, 2}, {3, 4}
  assert(1 == indexOf(t2, 3)); assert(2 == indexOf(t2, 4))

  t1.a = t2
  local r = deepcopy(t1)
  assert(r[1] == 1)
  assert(r.a[1] == 3)
  t2[1] = 8
  assert(r.a[1] == 3)

  assert(0, decAbs(1)); assert(0, decAbs(-1))
  assert('a', strLast('cba'))

  local t = {a=8, b=9}
  assert(8 == pop(t, 'a')) assert(9 == pop(t, 'b'))
  assert(0 == #t)

  assert(1 == bound(0, 1, 5))
  assert(1 == bound(-1, 1, 5))
  assert(3 == bound(3, 1, 5))
  assert(5 == bound(7, 1, 5))
  assert(5 == bound(5, 1, 5))
  assert(    within(1, 1, 5))
  assert(not within(0, 1, 5))
  assert(    within(3, 1, 5))
  assert(    within(5, 1, 5))
  assert(not within(6, 1, 5))

  assertError(function() error('take an error') end, 'an error')
end)

test("eval", nil, function()
  local env = {}
  local ok, err = eval('1+', env)
  assert(not ok); assert(err)
  local ok, three = eval('return 3', env)
  assert(ok); assertEq({}, env)
  assertEq(3, three)
  local ok, three = eval('seven = 7', env)
  assert(ok); assertEq({seven=7}, env)
  assert(not seven) -- did not modify globals
end)

test("str", nil, function()
  assertEq("12 34 56", strinsert("1256", 3, " 34 "))
  assertEq("78 1256", strinsert("1256", 1, "78 "))
  assertEq("1256 78", strinsert("1256", 5, " 78"))
  local a, b = strdivide('12345', 3)
  assertEq(a, '123'); assertEq(b, '45')
  assertEq({'1', ' ', 'a', 'c'}, explode'1 ac')
end)

test("safeToStr", nil, function()
  assertEq("a123",      safeToStr("a123"))
  assertEq('"123"',     safeToStr("123"))
  assertEq('"abc def"', safeToStr("abc def"))
  assertEq('423',       safeToStr(423))
  assertEq('1A',        safeToStr(26, {num='%X'}))
  assertEq('true',      safeToStr(true))
  assertMatch('Fn@%./lua/civ%.lua:%d+', safeToStr(civ.max))
  assertMatch('Tbl@0x[a-f0-9]+', safeToStr({hi=4}))
  assertMatch('?Meta@0x[a-f0-9]+',
    safeToStr(setmetatable({hi=4}, {})))
end)

test("fmt", nil, function()
  assertEq("{1,2,3}", fmt({1, 2, 3}))

  local t = {1, 2}; t[3] = t
  assertMatch('!ERROR!.*stack overflow', civ.fmt(t, {safe=true}))
  assertMatch('{1,2,RECURSE%[Tbl@0x%w+%]}', civ.fmt(t, {noRecurse=true}))
  assertEq("{1,2}", fmt({1, 2}))
  assertEq([[{baz=boo foo=bar}]], fmt({foo="bar", baz="boo"}))

  local result = fmt({a=1, b=2, c=3}, {pretty=true})
  assertEq('{\n  a=1\n  b=2\n  c=3\n}', result)

  assertEq('{1,2 :: a=12}', fmt({1, 2, a=12}))
end)

test("eq", nil, function()
  local a, b = {v=42}, {v=42}
  assert(eq(a, b)); assert(a ~= b) -- not the same instance
  -- add the metamethod __eq
  local vTable = {__eq = function(a, b) return a.v == b.v end, }
  setmetatable(a, vTable)
  assert(a == b); assert(b == a) -- uses a's metatable regardless
  assert(eq(a, b))
  a.other = 7;   assertEq(a, b) -- still uses metatable

  setmetatable(b, vTable)
  assert(a == b); assert(b == a)
end)

test('set', nil, function()
  local s = Set{'a', 'b', 'c'}
  assertEq(Set{'a', 'c', 'b'}, s)

  local l = sort(List.fromIter(s:iter()))
  assertEq(List{'a', 'b', 'c'}, l)
end)

test('map', nil, function()
  local m = Map{}
  m['a'] = 'b'; m['b'] = Map{}
  assertEq(Map{}, m:getPath{'b'})
  m:setPath({'b', 'c', 'd'}, 42)
  assertEq(Map{c=Map{d=42}}, m['b'])
  assertEq(42,        m:getPath{'b', 'c', 'd'})
end)

test('list', nil, function()
  local l = List{'a', 'b', 'c', 1, 2, 3}
  assertEq(List{1, 2, 3}, l:drain(3))
  assertEq(List{'a', 'b', 'c'}, l)
  assertEq(List{}, l:drain(0))
  assertEq(List{'a', 'b', 'c'}, l)
  assertEq(List{'c'}, l:drain(1))
  assertEq(List{'a', 'b'}, l)

  assertEq({2, 1}, reverse({1, 2}))
  assertEq({3, 2, 1}, reverse({1, 2, 3}))
end)


test("update-extend", nil, function()
  local a = {'a', 'b', 'c'}
  local t = {a=1, c=5}
  assertEq({a=1, c=5}, t)
  civ.update(t, {a=2, b=3})
  assertEq({a=2, b=3, c=5}, t)

  assertEq({[1]='a', [2]='b', [3]='c'}, a)
  civ.extend(a, {'d', 'e'})
  assertEq({'a', 'b', 'c', 'd', 'e'}, a)
end)

local function structs()
  local A = civ.struct('A', {{'a2', civ.Num}, {'a1', Str}})
  local B = civ.struct('B', {
    {'b1', civ.Num}, {'b2', civ.Num, 32},
    {'a', A, false}
  })
  return A, B
end
test("struct", nil, function()
  local A, B = structs()
  local a = A{a1='hi', a2=5}
  assert(A == getmetatable(a))
  assertEq('hi', a.a1); assertEq(5, a.a2)
  a.a2 = 4;             assertEq(4, a.a2)
  -- FIXME
  -- assertEq('A{a2=4 a1=hi}', civ.fmt(a))

  local b = B{b1=5, a=a}
  assert(B == getmetatable(b))
  assertEq(5, b.b1); assertEq(32, b.b2)
  b.b2 = 7;          assertEq(7, b.b2)
  assertEq(Num, pathTy(B, {'b1'}))
  assertEq(Str, pathTy(B, {'a', 'a1'}))
  assertEq('hi', pathVal(b, {'a', 'a1'}))

  assertEq({'a32', 'b', 'c1_32'}, dotSplit('a32.b.c1_32'))
end)

test("iter", nil, function()
  local i = List{1, 4, 6}:iterFn()
  assert(1 == select(2, i()))
  assert(4 == select(2, i()))
  assert(6 == select(2, i()))

  local l = List.fromIter(List{1, 4, 6}:iter())
  assertEq(List{1, 4, 6}, l)

  local r = Range(1, 5); assertEq('[1:5]', tostring(r))
  l = List.fromIter(Range(1, 5))
  assertEq(List{1, 2, 3, 4, 5}, l)
end)

test('LL', nil, function()
  local ll = LL(); assert(ll:isEmpty())
  ll:addFront(42); assertEq(42, ll:popBack())
  ll:addFront(46); assertEq(46, ll:popBack())
  assert(ll:isEmpty())
  ll:addFront(42);        ll:addFront(46);
  assertEq(46, ll.front.v);         assertEq(42, ll.back.v)
  assertEq(ll.front, ll.back.prev); assertEq(ll.back, ll.front.nxt)
  assertEq(nil, ll.front.prev);     assertEq(nil, ll.back.nxt)
  assertEq(42, ll:popBack()) assertEq(46, ll:popBack())
  assert(ll:isEmpty())
  assertEq(nil, ll:popBack())
  ll:addFront(42);        ll:addFront(46);   ll:addBack(41)
  assertEq(41, ll.back.v); assertEq(46, ll.front.v)
  assertEq(42, ll.front.nxt.v);  assertEq(42, ll.back.prev.v);
end)

test('time', nil, function()
  local N = Duration.NANO
  local d = Duration(3, 500)
  assertEq(Duration(2, 500),     Duration(3, 500) - Duration(1))
  assertEq(Duration(2, N - 900), Duration(3, 0)   - Duration(0, 900))
  assertEq(Duration(2, N - 800), Duration(3, 100) - Duration(0, 900))
  assertEq(Duration(2), Duration.fromMs(2000))
  assert(Duration(2) < Duration(3))
  assert(Duration(2) < Duration(2, 100))
  assert(not (Duration(2) < Duration(2)))
  assertEq(Duration(1.5), Duration(1, N * 0.5))

  assertEq(Epoch(1) - Duration(1), Epoch(0))
  assertEq(Epoch(1) - Epoch(0), Duration(1))
  local e = Epoch(1000001, 12342)
  assertEq(e - Epoch(1000000, 12342), Duration(1))
end)

assertGlobals(g)
