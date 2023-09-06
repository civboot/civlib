local CIV = {checkField = false}

local civ = require'civ.fmt'

-- ###################
-- # Utility Functions
local add, sfmt = table.insert, string.format

local function identity(v) return v end
civ.sfmt = sfmt
civ.noop = function() end
civ.noopFn = function() return noop end
civ.max = function(a, b) if a > b then return a end return b end
civ.min = function(a, b) if a < b then return a end return b end
civ.isEven = function(a) return a % 2 == 0 end
civ.isOdd  = function(a) return a % 2 == 1 end
civ.bound = function(v, min, max)
  if v > max then return max end
  if v < min then return min end
  return v
end
civ.within = function(v, min, max)
  return (min <= v) and (v <= max)
end
civ.sort2 = function(a, b)
  if a <= b then return a, b end
  return b, a
end
-- reverse a list-like table in-place
civ.reverse = function(t)
  local l = #t; for i=1, l/2 do
    t[i], t[l-i+1] = t[l-i+1], t[i]
  end
  return t
end
civ.decAbs = function(v)
  if v == 0 then return 0 end
  return ((v > 0) and v - 1) or v + 1
end
civ.strLast = function(s) return s:sub(#s, #s) end
civ.callerSource = function()
  local info = debug.getinfo(3)
  return string.format('%s:%s', info.source, info.currentline)
end

-- evaluate some lua code and call it with pcall.
--
-- The return value is in the same structure as pcall: (isOk, <results or error>)
civ.eval = function(s, env, name)
  assert(type(s) == 'string'); assert(type(env) == 'table')
  name = name or civ.callerSource()
  local e, err = load(s, name, 't', env)
  if err then return false, err end
  return pcall(e)
end

-- return keys array
local function keysarr(t)
  local a = {}; for k in pairs(t) do table.insert(a, k) end
  return a
end
local function iterpairs(t)
  local p = {}; for k, v in pairs(t) do
    table.insert(p, {k, v})
  end
  local i = 0; return function()
    i = i + 1; if i <= #p then
      return table.unpack(p[i])
    end
  end
end

local function explode(s)
  local t = {}; for ch in s:gmatch('.') do add(t, ch) end
  return t
end; civ.explode = explode

-- Mutating map function.
-- Modifies the table in-place using the function
local function map(t, fn)
  for i, v in pairs(t) do t[i] = fn(v) end
  return t
end

-- convert array to an iterator
local function iterarr(l)
  local i = 0
  return function()
    i = i + 1; if i <= #l then return i, l[i] end
  end
end

local function iterV(l)
  local i = 0
  return function()
    i = i + 1; if i <= #l then return l[i], i end
  end
end

local function concat(t, sep)
  local s = {}; for i, v in ipairs(t) do s[i] = tostring(v) end
  return table.concat(s, sep)
end
local function keyorder(a, b)
  if type(a) == type(b) then return a < b
  else return type(a) < type(b) end
end

civ.KEYS_MAX = 64
local function sort(t, fn) table.sort(t, fn); return t end
local function orderedKeys(t, max)
  local keys, len, max = {}, 0, max or civ.KEYS_MAX
  for k in pairs(t) do
    if len >= max then break end
    len = len + 1
    add(keys, k)
  end
  pcall(function() table.sort(keys) end)
  return keys
end

-- return as the index (first value) from an iterator when there is no index
local NoIndex = setmetatable({}, {__name='NoIndex'})
local function keysIter(t)
  local i, keys = 0, keysarr(t)
  return function()
    i = i + 1; if i < #keys then return NoIndex, keys[i] end
  end
end

-- return the first i characters and the remainder
civ.strdivide = function(s, i)
  return string.sub(s, 1, i), string.sub(s, i+1)
end
civ.strinsert = function (s, i, v)
  return string.sub(s, 1, i-1) .. v .. string.sub(s, i)
end
local function deepcopy(t)
  local out = {}; for k, v in pairs(t) do
    if 'table' == type(v) then v = deepcopy(v) end
    out[k] = v
  end
  return setmetatable(out, getmetatable(t))
end; civ.deepcopy = deepcopy

-- shallow copy and update with add
local function copy(t, add)
  local out = {}
  for k, v in pairs(t) do out[k] = v end
  if add then
    for k, v in pairs(add) do out[k] = v end
  end
  return setmetatable(out, getmetatable(t))
end

-- update is for dict/set, extend for list
local function update(t, add) for k, v in pairs(add) do t[k] = v end end
civ.updateKeys = function(t, add, keys)
  for _, k in ipairs(keys) do t[k] = add[k] end
end
local function extend(a, vals)
  for _, v in ipairs(vals) do table.insert(a, v) end
end
local function pop(t, k) local v = t[k]; t[k] = nil; return v end
civ.indexOf = function(t, v)
  for i, tv in ipairs(t) do
    if tv == v then return i end
  end
end

local _tyName = {
  boolean = identity, number = identity, string = identity,
  ['nil'] = identity, ['function'] = identity,
}
-- Get the name of obj's type
local function tyName(obj)
  local n = _tyName[type(obj)]; if n then return n end
  local mt = getmetatable(obj)
  if not mt or not mt.__name then return type(obj) end
  return mt.__name
end; civ.tyName = tyName

-- __index function for only getting methods
local function methIndex(self, k)
  return getmetatable(self)[k]
end

-- __index function used for most types
local function _tyIndex(self, k)
  local ty = getmetatable(self)
  local v = ty["#defaults"][k]; if v == nil then v = ty[k] end
  if v ~= nil then  return v  end
  k = ('table' == type(k) and k) or tostring(k)
  -- TODO: sometimes used by ipairs, figure out how to disable
  if k == 1 then return nil end
  error("Unknown member: " .. tyName(self) .. "." .. k)
end

-- tyIndex but allows new integer values
civ.listIndex = function(self, k)
  if 'number' == type(k) then return nil end
  return _tyIndex(self, k)
end

local Set = nil

local ALL_TYS = {}

-- Types are generated from other types.
-- You can retreive or add them here.
--
-- Example:
--   generated = genTyRepo({A, B})
--   if not generated then
--     generated = genTyRepo({A, B})
--   end
local GEN_TYS = {}
local function genTyRepo(tys, first)
  local t = GEN_TYS
  -- if first then t = genTyRepo(first) end
  for _, ty_ in ipairs(tys) do
    if not ty_ then return nil end -- error
    local e = t[ty_]
    if not e then e = {}; t[ty_] = e end
    t = e
  end
  return t
end

-- A Ty has methods (which holds documentation)
-- and field metadata. It also has a metatable
-- so we can override __call (constructor)
local function newTy(name)
  local ty = {
    __name=name,
    __index=_tyIndex,
    ["#civTy"]=true,
    ["#defaults"]={},
    __tostring=civ.fmt,
  }
  ALL_TYS[ty] = ty
  return setmetatable(ty, {
    __name=concat{'Ty[', name, ']'},
  })
end

local Nil  = newTy('Nil');  local Fn   = newTy('Fn')
local Bool = newTy('Bool'); local Num  = newTy('Num')
local Str  = newTy('Str');  local Tbl  = newTy('Tbl')

assert('Ty[Str]' == tyName(Str)); assert('table' == tyName({}))

local _tyMap = {
  ['nil']      = function(_) return Nil  end,
  ['function'] = function(_) return Fn   end,
  boolean      = function(_) return Bool end,
  number       = function(_) return Num  end,
  string       = function(_) return Str  end,
  table        = function(t)
    local ty = getmetatable(t)
    if not ty or not ty['#civTy'] then  return Tbl
    else                                return ty  end
  end,
}
local function ty(obj) return _tyMap[type(obj)](obj) end
assert(Str == ty('hi')); assert(Num == ty(3))
assert(Tbl == ty({}))


-- Get the type of a container based on the value
local function containerTy(cont)
  for _, v in pairs(cont) do
    return ty(v)
  end
  error("contains no values!")
end

-- In some cases we cannot trust an object's __index
local function callMethod(obj, meth, ...)
  return ty(obj)[meth](obj, ...)
end


-- check if given is a subtype (descendant) of req
local function tyCheck(req, given)
  if (true == req) or (req == given) then return true end
  local a = given["#ancestors"]
  return a and a[req]
end

-- Define a types method
local function constructor(ty, fn)
  getmetatable(ty).__call = fn
end
local function method(ty, name, fn) ty[name] = fn  end
civ.methods = function(ty, m)
  for name, fn in pairs(m) do method(ty, name, fn) end
end

-------------------------------
-- Fmt: formatter
--
-- Fields:
--   set: table of settings
--     num: number format string, default='%i'
--     noRecurse: if true will prevent all recursion
--
--   done: if noRecurse is set will contain all formatted
--     (or currently formatting) tables
local Fmt = {}; civ.Fmt = Fmt
Fmt.__name = 'Fmt'
Fmt.__tostring = function() return 'Fmt{...}' end
setmetatable(Fmt, { __name='FmtTy',
  __call=function(ty_, t)
    local set = t.set or {}
    set = t.set or {}
    set.indent   = set.indent or '  '
    set.keysMax  = set.keysMax or civ.KEYS_MAX
    set.listSep  = set.listSep or ','
    set.itemSep  = set.itemSep or ((set.pretty and '\n') or ' ')
    set.levelSep = set.levelSep or ((set.pretty and '\n') or nil)
    t.set = set
    t.done = t.done or {}
    t.level = t.level or 0
    return setmetatable(t, Fmt)
  end,
})
Fmt.__index=function(f, key) return Fmt[key] end

civ.fnToStr = function(fn)
  local s = debug.getinfo(fn, 'nS')
  local n = ''; if s.name then n = sfmt('%q', s.name) end
  return sfmt('Fn%s@%s:%s', n, s.short_src, s.linedefined)
end

local STR_AMBIGUOUS = {['true']=true, ['false']=true, ['nil']=true}
civ.strAmbiguous = function(s)
  return (
    STR_AMBIGUOUS[s]
    or tonumber(s)
    or s:find('[%s\'"={}%[%]]')
  )
end
-- Invariant: metatable is nil or is missing __tostring
civ.tblIdUnsafe = function(t)
  return tostring(t):gsub('table: ', '')
end

civ.metaName = function(mt)
  if not mt then return 'Tbl' end
  return mt.__name or '?Meta'
end

civ.tblToStrSafe = function(t)
  local mt = getmetatable(t);
  if not mt then return sfmt('Tbl@%s', civ.tblIdUnsafe(t)) end
  if mt.__tostring then return sfmt('%s{...}', civ.metaName(mt)) end
  return sfmt('%s@%s', civ.metaName(mt), civ.tblIdUnsafe(t))
end

civ.SAFE_TOSTRING = {
  ['nil']=function(n) return 'nil' end,
  ['function']=civ.fnToStr,
  boolean=function(v, set) return tostring(v) end,
  number=function(n, set)
    local f = '%i'
    if set then f = set.num or '%i' end
    return sfmt(f, n)
  end,
  string=function(s)
    if civ.strAmbiguous(s) then return sfmt('%q', s) end
    return s
  end,
  table=civ.tblToStrSafe,
}

civ.safeToStr = function(v, set)
  return civ.SAFE_TOSTRING[type(v)](v, set)
end

civ.tblFmt = function(t, f)
  assert(type(t) == 'table', type(t))
  local mt = getmetatable(t)
  local name = civ.metaName(mt)
  if name ~= 'Tbl' then add(f, name) end
  local listI, isEmpty = 0, 1
  f:levelEnter('{')
  for i, v in ipairs(t) do
    f:fmt(v); f:sep(f.set.listSep)
    listI = listI + 1
  end
  if listI > 0 then isEmpty = 0; f[#f] = ' :: ' end
  local keys = orderedKeys(t, f.set.keysMax)
  for _, k in ipairs(keys) do
    if type(k) == 'number' and k <= listI then -- already added
    else
      f:fmt(k);    add(f, '=')
      f:fmt(t[k]); f:sep(f.set.itemSep)
      isEmpty = 0
    end
  end
  if #keys >= f.set.keysMax then add(f, '...'); isEmpty = 1 end
  f:levelLeave('}', isEmpty)
end

-- Handle indent for separation between values and going into
Fmt.sep = function(f, sep)
  add(f, sep); if sep == '\n' then
    add(f, string.rep(f.set.indent, f.level))
  end
end
Fmt.levelEnter = function(f, startCh)
  add(f, startCh)
  f.level = f.level + 1
  if f.set.levelSep then f:sep(f.set.levelSep) end
end
Fmt.levelLeave = function(f, endCh, appendLast)
  f.level = f.level - 1
  if f.set.levelSep then
    f[#f + appendLast] = string.rep(f.set.levelSep, f.level)
    add(f, endCh)
  else f[#f + appendLast] = endCh end
end

-- Format the value and store the result in `f`
Fmt.fmt = function(f, v)
  local tystr = type(v)
  if tystr ~= 'table' then
    add(f, civ.SAFE_TOSTRING[tystr](v, f.set))
    return f
  end
  if f.set.noRecurse then
    if f.done[v] then
      add(f, 'RECURSE['); add(f, civ.tblToStrSafe(v)); add(f, ']')
      return f
    else f.done[v] = true end
  end
  local mt = getmetatable(v)
  local len = #f
  local ok, err = pcall(function()
    if not mt then civ.tblFmt(v, f)
    elseif rawget(mt, '__fmt')      then mt.__fmt(v, f)
    elseif rawget(mt, '__tostring') ~= civ.fmt then
      add(f, tostring(v))
    else civ.tblFmt(v, f) end
  end)
  if ok then return f end
  while #f > len do table.remove(f) end
  add(f, civ.safeToStr(v) )
  add(f, '-->!ERROR!['); add(f, civ.safeToStr(err));
  add(f, ']');
  return f
end

Fmt.toStr = function(f) return table.concat(f, '') end
Fmt.write = function(f, fd)
  for _, s in ipairs(f) do fd:write(s) end
end
Fmt.pnt = function(f)
  f:write(io.stdout)
  io.stdout:write('\n')
  io.stdout:flush()
end

civ.fmt = function(v, set)
  return Fmt{set=set or {}}:fmt(v):toStr()
end
civ.pnt = function(...)
  local args = {...}
  for i, arg in ipairs(args) do
    if type(arg) ~= 'table' then
      io.stdout:write(tostring(arg))
    else io.stdout:write(civ.fmt(arg)) end
    if i < #args then io.stdout:write('\t') end
  end
  io.stdout:write('\n')
  io.stdout:flush()
end
civ.pntf   = function(...) pnt(string.format(...))   end
civ.errorf = function(...) error(string.format(...)) end

-------------------------------
-- Core Data Types

local Iter = newTy('Iter')
constructor(Iter, function(ty, l)
  return setmetatable({data=l, i=0}, ty)
end)
method(Iter, '__call', function(self, l)
  if self.i < #self.data then
    self.i = self.i + 1
    return self.i, self.data[self.i]
  end
end)

-- These can be useful as filter functions
local function isEq(vEq)
  return function(v) return vEq == v end
end
local function isNotEq(nEq)
  return function(v) return nEq ~= v end
end
local function isIn(in_)
  return function(v) return in_[v] end
end

local function rangeFmt(r)
end
local Range = newTy('Range')
constructor(Range, function(ty_, start, end_, step)
  if not start or not end_ then error(
    "must provide start and end for range"
  )end
  return setmetatable({
    i=start, start=start, end_=end_,
    step=step or 1}, ty_)
end)
method(Range, '__tostring', function(self)
  local step = ''
  if self.step ~= 1 then step = ' '..self.step end
  return string.format(
    '[%s:%s%s]', self.start, self.end_, step)
end)
method(Range, '__call', function(self)
  local i = self.i; if i <= self.end_ then
    self.i = i + self.step; return i, i
  end
end)

local result = Iter{3, 4}
assert('Iter' == tyName(result))
assert(3 == select(2, result()))
assert(4 == select(2, result()))
assert(nil == result());

-- A Map: literally a lua table with some methods.
-- Use like:
--
--   t = Mbl{'a'=2, 'b'=7, 1='d', 2='f'}
--   s[3] = 'e'
local Map = newTy('Map')
constructor(Map,  function(ty_, t)
  return setmetatable(t, ty_)
end)
Map.__fmt = civ.tblFmt
method(Map, '__index', methIndex)
-- get a value. If vFn is given it will be called to
-- set the value (and return it)
method(Map, 'empty', function() return Map{} end)
method(Map, 'from', function(t, keyFn)
  local out = Map{}
  for k, v in pairs(t) do
    k, v = keyFn(k, v); out[k] = v
  end
  return out
end)
method(Map, 'isEmpty', function(self)
  for k, v in pairs(self) do return false end
  return true
end)
method(Map, 'get', function(self, k, vFn)
  local v = self[k]; if v then return v end
  if vFn then v = vFn(self); self[k] = v end
  return v
end)
method(Map, 'pop', function(self, k)
  local v = self[k]; self[k] = nil; return v
end)
method(Map, 'getPath', function(self, path, vFn)
  local d = self
  for i, k in ipairs(path) do
    local nxt = d[k]
    if nxt then
    elseif vFn then
      nxt = vFn(d, i); d[k] = nxt
    else return nil end
    d = nxt
  end
  return d
end)
method(Map, 'setPath', function(self, path, value)
  local d, len = self, #path
  assert(len > 0, 'empty path')
  for i, k in ipairs(path) do
    if i >= len then break end
    d = d:get(k, Map.empty)
  end
  d[path[len]] = value
end)
method(Map, 'diff', function(self, r)
  local left = Map{}
  for k in pairs(self) do if r[k] == nil then left[k] = k end end
  return left
end)
method(Map, 'iter',   pairs)
method(Map, 'iterFn', iterpairs)

local result = Map{a=2, b=3}
assert(2 == result.a); assert(3 == result.b)
assert(nil == result.c) -- TODO raises error "unknown member"
assert(5 == result:get('c', function() return 5 end))
assert(5 == result.c)
assert(Map == ty(result))

-- A set: unique keys in arbitrary order. Values are all `true`
-- Use like:
--
--   s = Set{'a', 'b', 1, 2}
--   s:add(3)
Set = newTy('Set')
constructor(Set, function(ty, t)
  local s = {}
  for _, key in pairs(t) do s[key] = key end
  return setmetatable(s, ty)
end)
method(Set, '__index',    methIndex)
method(Set, 'asSorted', function(self)
  local l = List{}
  for k in pairs(self) do l:add(k) end
  return sort(l)
end)
method(Set, 'add', function(self, k)  self[k] = true  end)
method(Set, 'update', function(self, t)
  for _, key in pairs(t) do self[key] = key end
end)
method(Set, 'union', function(self, r)
  local both = Set{}
  for k in pairs(self) do if r[k] then both[k] = k end end
  return both
end)
-- difference: get items in self that are NOT in r
method(Set, 'diff', function(self, r)
  local left = Set{}
  for k in pairs(self) do if not r[k] then left[k] = k end end
  return left
end)
method(Set, 'iter',   pairs)
method(Set, 'iterFn', iterpairs)

-- Note: Bool is intentionally not here.
-- Technically it can be used, but it causes awkwardness
-- in APIs and there is really no value in using it.
local KEY_TYS = Set{Num, Str}

-- List: ordered set of possibly duplicated values
-- A list is just the normal Lua list with type information and a few methods.
-- Use iparis to iterate over it, etc
--
--   l = List{'a', 'b', 1, 2}
--   l:add(3)
--   l:extend{1, 2, 3}
--   l[1] get first (aka very first) index.
local List = newTy("List")
constructor(List, function(ty, t)
  return setmetatable(t, ty)
end)
method(List, 'empty', function() return List{} end)
method(List, '__index', methIndex)
method(List, 'add', table.insert)
method(List, 'pop', table.remove)
method(List, 'extend', extend)

-- pop len elements from list
method(List, 'drain', function(self, len)
  local l = List{}
  for i=#self - len + 1, #self do
    l:add(self[i]) self[i] = nil
  end; return l
end)
method(List, 'asSorted', sort)
-- from a normal (index,value) iterator
method(List, 'fromIter', function(...)
  local l = List{}
  for i, v in ... do l:add(v) end
  return l
end)
-- from an iterator that outputs values
method(List, 'fromIterV', function(...)
  local l = List{}
  for v in ... do l:add(v) end
  return l
end)
method(List, '__pairs',  ipairs)
method(List, 'iter',   ipairs)
method(List, 'iterFn', iterarr)
method(List, 'iterV', iterV)

result = List{5, 6}; assert(5 == result[1])
result:extend{3, 4}; assert(4 == result[4])

civ.LL = newTy('LL')
civ.LL.__index = methIndex
constructor(civ.LL, function(ty_)
  return setmetatable({}, LL)
end)
method(civ.LL, 'isEmpty', function(self)
  return nil == self.front
end)
method(civ.LL, 'addFront', function(self, v)
  if nil == v then return end
  local a = {v=v, nxt=self.front, prev=nil}
  if self.front then self.front.prev = a end
  self.front = a
  if not self.back then self.back = self.front end
end)
method(civ.LL, 'addBack', function(self, v)
  if nil == v then return end
  local a = {v=v, nxt=nil, prev=self.back}
  if self.back then self.back.nxt = a end
  self.back = a
  if not self.front then self.front = self.back end
end)
method(civ.LL, 'popBack', function(self)
  local o = self.back; if o == nil then return end
  self.back = o.prev
  if self.back then self.back.nxt = nil
  else self.front = nil end
  return o.v
end)


method(Set, '__fmt', function(self, f)
  add(f, 'Set{'); local isEmpty = 1
  local keys = orderedKeys(self, f.set.keysMax)
  for _, k in ipairs(keys) do
    f:fmt(k); add(f, ' '); isEmpty = 0
  end
  if #keys >= civ.KEYS_MAX then add(f, '...'); isEmpty = 1 end
  f[#f + isEmpty] = '}'
end)
result = Set{'a', 'b', 'c'}; assert("Set{a b c}"   == tostring(result))
result:add('d');             assert("Set{a b c d}" == tostring(result))

List.__fmt=function(self, f)
  add(f, '['); local isEmpty = 1
  for _, v in ipairs(self) do
    f:fmt(v); add(f, ' '); isEmpty = 0
  end
  f[#f + isEmpty] = ']'
end

assert("[5 6 3 4]"  == tostring(List{5, 6, 3, 4}))
result = tostring(Map{a=5, b=77}); assert("Map{a=5 b=77}" == result)

local function tyError(req, given)
  error(string.format("%s is not an ancestor of %s", given, req))
end

-- ###################
-- # Generic Equality

local function nativeEq(a, b) return a == b end

-- Note: requires that the types are identical
-- modified from: https://www.lua.org/manual/5.1/manual.html
local function getcomphandler (op1, op2, event)
  return (getmetatable(op1) or {})[event] or (getmetatable(op2) or {})[event]
end

local eq = nil
local function eqDeep(a, b)
  if rawequal(a, b)     then return true   end
  if ty(a) ~= ty(b)     then return false  end
  local aLen = 0
  for aKey, aValue in pairs(a) do
    local bValue = b[aKey]
    if not eq(aValue, bValue) then return false end
    aLen = aLen + 1
  end
  local bLen = 0
  -- Note: #b only returns length of integer indexes
  for bKey in pairs(b) do bLen = bLen + 1 end
  return aLen == bLen
end

local eqTy = {
  number = nativeEq, boolean = nativeEq, string = nativeEq,
  ['nil'] = nativeEq, ['function'] = nativeEq,
  ['table'] = function(a, b)
    if getcomphandler(a, b, '__eq') then return a == b end
    if a == b                       then return true   end
    return eqDeep(a, b)
  end,
}
eq = function(a, b) return eqTy[type(a)](a, b) end

assert(eq(2, 2))
assert(eq({4, 5}, {4, 5}))
assert(eq({4, {5, 6}}, {4, {5, 6}}))
assert(not eq({4, {5, 6, 7}}, {4, {5, 6}}))
assert(not eq({4, {5, 6}}, {4, {5, 6, 7}}))

-- Now update the eq for our types
method(Map, '__eq', eqDeep)
method(Set, '__eq', function(self, s)
  for k in pairs(self) do
    if(not s[k])    then return false end
  end
  for k in pairs(s) do
    if(not self[k]) then return false end
  end
  return true
end)
assert(Map{4, 5} == Map{4, 5})

local function eqArr(a, b)
  if(#a ~= #b) then return false end
  for i, av in ipairs(a) do
    if not eq(av, b[i]) then return false end
  end
  return true
end
method(List, '__eq', eqArr)

-- ###################
-- # Struct

local function structInvalidField (st, f)
  error(string.format("%s does not have field %s", st, f))
end
local function structTy (t) return getmetatable(t).__name end
local function structIndex (t, k)
  local mt = getmetatable(t)
  local v = mt["#defaults"][k]
  if v ~= nil then return v end
  v = mt[k]; if v ~= nil then return v end
  -- TODO: sometimes used by ipairs, figure out how to disable
  if k == 1 then return nil end
  structInvalidField(getmetatable(t), k)
end

local function structNewIndex (t, k, v)
  if CIV.checkField and not getmetatable(t)["#tys"][k] then
    structInvalidField(getmetatable(t), k)
  end
  rawset(t, k, v)
end

local function specifyFields(fields)
  local tys, defaults, ordered = {}, {}, {}
  for _, f in ipairs(fields) do
    local fname, fty, fdef = nil, nil, false
    if 'string' == type(f) then fname = f
    else fname, fty, fdef = table.unpack(f) end
    assert(type(fname) == 'string');
    tys[fname] = fty or true; defaults[fname] = fdef;
    List.add(ordered, fname)
  end
  return tys, defaults, ordered
end

-- Examples: to get value of
--   a.a1       pathVal(a, {'a1'})
--   a.a1.b2    pathVal(a, {'a1', 'b2'})
local function pathVal(st, path)
  for _, p in ipairs(path) do st = st[p] end
  return st
end

-- Examples: to get type of
--   A.a1       pathTy(A, {'a1'})
--   A.a1.b2    pathTy(A, {'a1', 'b2'})
local function pathTy(st, path)
  for _, p in ipairs(path) do st = st["#tys"][p] end
  return st
end

local function split(text) return text:gmatch'%S+'    end
local function lines(text) return text:gmatch'[^\n]*' end

local function findLine(lines, i, line)
  while i < #lines do
    if lines[i] == line then return i end
    i = i + 1
  end
end

local function strDiffPlace(sL, sR)
  local i, sL, sR = 1, explode(sL), explode(sR)
  while i <= #sL and i <= #sR do
    if sL[i] ~= sR[i] then return i end
    i = i + 1
  end
  if #sL < #sR then return #sL + 1 end
  if #sR < #sL then return #sR + 1 end
  return nil
end; civ.strDiffPlace = strDiffPlace

-- return the l, c where there is a difference
local function linesDiffPlace(linesL, linesR)
  local i = 1
  while i <= #linesL and i <= #linesR do
    local lL, lR = linesL[i], linesR[i]
    if lL ~= lR then
      return i, assert(strDiffPlace(lL, lR))
    end
    i = i + 1
  end
  if #linesL < #linesR then return #linesL + 1, 1 end
  if #linesR < #linesL then return #linesR + 1, 1 end
  return nil
end; civ.linesDiffPlace = linesDiffPlace


local function matches(text, m)
  local out = {}; for v in string.gmatch(text, m) do
    table.insert(out, v) end
  return out
end
local function trim(text)
  return string.match(text, '^%s*(.-)%s*$')
end

local function dotSplit(pathStr)
  return matches(pathStr, "[^%.]*")
end
local function makePath(path)
  if 'string' == type(path) then return dotSplit(path) end
  return path
end

local function tyCheckPath(st, path, given)
  local req = pathTy(st, path)
  if not tyCheck(req, given) then error(string.format(
    "%s not is not ancestor of %s (%s.%s)",
    given, req, st, civ.fmt(path)
  ))end
  return req
end

civ.structConstructor = function(st, t)
  for f, v in pairs(t) do
    local fTy = st["#tys"][f]
    if not fTy then
      if type(f) == 'number' and (st['#attr'] or {}).list then
      else structInvalidField(st, f) end
    elseif not tyCheck(fTy, ty(v)) then tyError(fTy, ty(v)) end
  end
  for f in pairs(st["#tys"]) do
    if nil == t[f] and nil == st["#defaults"][f] then
      error("missing field: " .. f)
    end
  end
  return setmetatable(t, st)
end


local struct = newTy('Struct')
constructor(struct, function(ty_, name, fields)
  local st = newTy(name)
  local tys, defaults, ordered = specifyFields(fields)
  update(st, {
    ["#tys"]=tys,  ["#ordered"]=ordered,
    ["#defaults"]=defaults,
    __index=structIndex, __newindex=structNewIndex,
    __tostring=civ.fmt,
  })
  constructor(st, civ.structConstructor)
  return st
end)

local BufFillerWs = {
  [6] = '      ', [3] = '   ', [1] = ' ',
}
local BufFillerHeader = {
  [6] = '======', [3] = '===', [1] = '=',
}
local BufFillerRow = {
  [6] = ' -  - ', [3] = ' - ',  [1] = ' ',
}

local function fillBuf(b, num, filler)
  filler = filler or BufFillerWs
  while num >= 6 do table.insert(b, filler[6]);  num = num - 6 end
  while num >= 3 do table.insert(b, filler[3]);  num = num - 3 end
  while num >= 1 do table.insert(b, filler[1]);  num = num - 1 end
  return b
end

local function stringDiffBuf(b, sL, sR)
  local linesL = List.fromIterV(lines(sL))
  local linesR = List.fromIterV(lines(sR))
  local l, c = linesDiffPlace(linesL, linesR)
  assert(l); assert(c)
  add(b, string.format("## Difference line=%q (", l))
  add(b, string.format('lines[%q|%q]', #linesL, #linesR))
  add(b, string.format(' strlen[%q|%q])\n', #sL, #sR))
  add(b, ' left: '); add(b, linesL[l]); add(b, '\n')
  add(b, 'right: '); add(b, linesR[l]); add(b, '\n')
  fillBuf(b, c - 1 + 7);
  add(b, string.format('^ (column %q)\n', c))
  add(b, '####(end diff)\n')
end
local function stringDiff(sL, sR)
  local b = {}
  stringDiffBuf(b, sL, sR)
  return table.concat(b, '')
end

local function assertEq(left, right, pretty)
  if eq(left, right) then return end
  local f = Fmt{}
  add(f, "Values not equal:")
  add(f, "\n LEFT: ");  f:fmt(left)
  add(f, "\nRIGHT: "); f:fmt(right)
  add(f, '\n')
  if type(left) == 'string' and type(right) == 'string' then
    stringDiffBuf(f, left, right)
  end
  error(f:toStr())
end

civ.assertMatch = function(expectPat, result)
  if not result:match(expectPat) then
    errorf('Does not match pattern:\nPattern: %q\n Result:  %s',
           expectPat, result)
  end
end

civ.assertError = function(fn, errPat, plain)
  local ok, err = pcall(fn)
  if ok then error('! No error received, expected error: '..errPat) end
  if not err:find(errPat, 1, plain) then errorf(
    '! Expected error %q but got %q', errPat, err
  )end
end

local result = List{1, 'a', 2}
assertEq(List{1, 'a', 2},   result)
assert  (List{1, 'a', 2} == result)
assert  (List{2, 'a', 2} ~= result)
assert([[
## Difference line=2 (lines[2|2] strlen[7|7])
 left: 123
right: 12d
         ^ (column 3)
####(end diff)
]] == stringDiff('abc\n123', 'abc\n12d'))

-- ###################
-- # File Helpers
local function readAll(path)
  local f = io.open(path, 'r')
  local out = f:read('a'); f:close()
  return out
end

-- ###################
-- # Picker / Query
-- Picker is an ergonomic way to query over a list of structs
-- while using struct indexes
--
-- Example use:
--   l = List{
--     A{a1='one',   a2=1},
--     A{a1='two',   a2=2},
--     A{a1='three', a2=3},
--   }
--   p = Picker(A, l) -- construct the picker with the type.
--
--   -- `.q` creates a Query object. Here we filter/query by equality:
--   oneOnly  = p.q.a1:eq('one') 
--   twoThree = p.q.a2:in_{2, 3}.a1:eq('two')

local DisplayCell = struct('DisplayCell',
  {{'lines', List}, {'width', Int}})
local DisplayCol = struct('DisplayCol',
  {{'data', List}, {'width', Int}})

local Display = newTy('Display')
constructor(Display, function(ty_, struct, iter)
  local totalW, cols = 0, Map{}
  for _, name in ipairs(struct['#ordered']) do
    cols[name] = DisplayCol{data=List{}, width=0}
  end
  local len, dWidth = 0, 0
  for _, row in iter do
    len = len + 1
    for _, cname in ipairs(struct['#ordered']) do
      local v = row[cname]
      local w, txt = 0, tostring(Fmt.pretty(v))
      local cLines = List{}
      local first = true
      for line in lines(txt) do
        if first and trim(line) == '' then -- remove empty lines at front
        else
          first, w = false, max(w, string.len(line))
          cLines:add(line)
        end
      end
      -- remove lines at the end
      while '' == trim(cLines[#cLines]) do cLines[#cLines] = null end
      local dcol = cols[cname]
      dWidth = max(dWidth, w)
      dcol.width = max(dcol.width, w)
      dcol.data:add(DisplayCell{lines=cLines, width=w})
    end
  end
  return setmetatable({
    struct=struct, cols=cols, len=len, width=dWidth,
  }, ty_)
end)
local COL_SEP = ' | '
method(Display, '__tostring', function(self)
  for ri=1, self.len do
    for c, dCol in pairs(self.cols) do
    end
  end
  local availW = 100
  local b = setmetatable({indent='  '}, Fmt)
  local widths, heights = Map{}, List{}
  local colNames = self.struct['#ordered']

  for _, c in ipairs(colNames) do heights[0] = 1 end
  for ri=1, self.len do
    local r = 0
    for _, c in ipairs(colNames) do
      r = max(r, #self.cols[c].data[ri].lines)
    end
    heights[ri] = r
  end

  if self.width <= availW - (#COL_SEP * #colNames) then
    for _, c in ipairs(colNames) do
      widths[c] = max(#c, self.cols[c].width)
    end
  else error('auto-width not impl') end

  local addCell = function(ci, ri, li, c, lines, sep, filler)
    if li > #lines then if filler or ci < #colNames then
        fillBuf(b, widths[c], filler)
      end
    else
      local l = lines[li]
      table.insert(b, l)
      if filler or ci < #colNames then
        fillBuf(b, widths[c] - string.len(l), filler)
      end
    end
    if(ci < #colNames)  then table.insert(b, sep or COL_SEP)
    else table.insert(b, '\n') end
  end

  -------+-----------+------ = header separator
  local breaker = function(filler, sep)
    for ci, c in ipairs(colNames) do
      local b = List{}; fillBuf(b, widths[c], filler)
      addCell(ci, 0, 1, c, {}, sep, filler)
    end
  end

  breaker(BufFillerHeader, '=+=')
  -- bob | george    | ringo = header
  for ci, c in ipairs(colNames) do addCell(ci, 0, 1, c, {c}) end
  breaker(BufFillerHeader, '=+=')

  for ri = 1, self.len do
    for li = 1, heights[ri] do
      for ci, c in ipairs(colNames) do
        local lines = self.cols[c].data[ri].lines
          addCell(ci, ri, li, c, lines)
      end
    end
    breaker(BufFillerRow, ' + ')
  end
  return concat(b)
end)


local Picker = newTy('Picker')
local Query = struct('Query', {
    -- data comes from one of
    -- (picker,i[ndexIter]) or (iter, struct)
    {'#picker', Picker, false}, {'#i', nil, false},
    {'#iter', nil, false}, {'#struct', nil, false},
    {'#iNew', nil, false},

    -- path and ops are built-up by user
    {'#path', List}, {'#ops', List},
  })
method(Query, 'debug', function(self)
  return string.format(
      'Query[%s %s %s]', rawget(self, '#ops'),
      rawget(self, '#iNew'), rawget(self, '#iter'))
end)
local PathBuilder = struct('PathBuilder', {'#query'})
local QueryOp = struct('Op',
  {{'name', Str}, {'path'}, {'value'}})

local function fmtStructFull(stTy)
  local endAdd, b = 1, List{stTy.__name, '{'}
  for _, field in ipairs(stTy['#ordered']) do
    local fieldTy = stTy['#tys'][field]
    if true == fieldTy then fieldTy = 'Any' end
    b:extend{field, ':', tostring(fieldTy), ' '}
    endAdd = 0
  end
  b[#b + endAdd] = '}'
  return concat(b)
end
local function genStruct(name, namedTys)
  local repo = genTyRepo(namedTys, {name})
  if not repo then error(string.format(
    "All types must be defined in path: %s", sel))
  end
  if repo.ty then return repo.ty end
  local fields = List{}
  local i = 1; while i+1 <= #namedTys do
    fields:add{namedTys[i], namedTys[i+1]}
    i = i + 2
  end
  local st = struct(name, fields); repo.ty = st
  getmetatable(st).__tostring = fmtStructFull
  return repo.ty
end

-- A set of query operations to perform on a Picker
-- path is built up by multiple field accesses.
method(Query, '__tostring', function(self) return 'Query' end)
method(Query, 'new',  function(picker)
  return Query{['#picker']=picker, ['#path']=List{}, ['#ops']=List{},
               ['#i']=Range(1, picker.len)}
end)
method(Query, 'iter', function(self)
  assert(Query == ty(self))
  local r = rawget(self, '#iNew')
  if r then self['#i'] = r()
  else r = rawget(self, '#picker');
    if r then self['#i'] = Range(1, r.len) end
  end
  return self
end)

-- A picker itself, which holds the struct type
-- and the data (or the way to access the data)
constructor(Picker, function(ty, struct, data)
  local p = {struct=struct, data=data, len=#data,
             indexes=Map{}}
  return setmetatable(p, ty)
end)
method(Picker, '__index', function(self, k)
  if 'q' == k then return Query.new(self) end
  local mv = getmetatable(self)[k]; if mv then return mv end
  error(k .. ' not on Picker. Use Picker.q to start query')
end)
method(Picker, '__tostring', function(self)
  return string.format("Picker[%s len=%s]",
    rawget(self, 'struct').__name, rawget(self, 'len'))
end)

local function queryStruct(q)
  return rawget(q, '#struct') or q['#picker'].struct
end
local function queryCheckTy(query, ty_, path)
  path = path or query['#path']
  return tyCheckPath(queryStruct(query), path, ty_)
end

local function _queryOpImpl(query, op, value, ty_)
  queryCheckTy(query, ty_)
  query['#ops']:add(QueryOp{
    name=op, path=query['#path'], value=value})
  query['#path'] = List{}
  return query

end
local function _queryOp(op)
  return function(self, value)
    assert(Query == ty(self))
    queryCheckTy(self, ty(value))
    self['#ops']:add(QueryOp{
      name=op, path=self['#path'], value=value})
    self['#path'] = List{}
    return self
  end
end
for _, op in pairs({'filter', 'lt', 'lte', 'gt', 'gte'}) do
  method(Query, op, _queryOp(op))
end

local function queryCreateIndexes(query, filter)
  return idx
end

local function queryIndexPath(query, op, vTy, path)
  path = path or query['#path']
  local pty = queryCheckTy(query, vTy, path)
  local indexes = (query['#picker']or{}).indexes
  if not indexes or not KEY_TYS[pty] then return nil end
  return List{op, concat(path)}, query['#picker'], indexes
end
-- Get or create query indexes using filter.
local function queryIndexes(query, op, v, vTy, filter, path)
  local path, picker, indexes = queryIndexPath(query, op, vTy, path)
  if 'table' == type(v) then path:extend(v:asSorted())
  else                       path:add(v) end
  indexes = indexes:getPath(path, function(d, i)
    if i < #path then return Map{}
    else              return List{} end
  end)
  if #indexes ~= 0 then return indexes end
  -- fill indexes
  local stTy, path = queryStruct(query), query['#path']
  for i, v in ipairs(picker.data) do
    if filter(pathVal(v, path)) then indexes:add(i) end
  end
  return indexes
end

local function queryUseIndexes(query, idx)
  if not idx then return nil end
  local iNew = function() return idx:iterFn() end
  query['#iNew'], query['#i'] = iNew, idx:iterFn()
  query['#path'] = List{}
  return query
end

local normalEq = _queryOp('eq')

method(Query, 'eq', function(self, value)
  assert(Query == ty(self))
  local idx = queryIndexes(self, 'eq', value, ty(value), isEq(value))
  if idx then return queryUseIndexes(self, idx) end
  return normalEq(self, value)
end)
method(Query, 'in_', function(self, value)
  assert(Query == ty(self))
  if 'table' == type(value) then value = Set(value) end
  if Set ~= ty(value) then error(
    "in_ must be on Set, got " .. tyName(value)
  )end
  local idx = Set{}; local cTy = containerTy(value)
  for v in value:iter() do
    local add = queryIndexes(self, 'in_', v, cTy, isEq(v))
    if not add then idx = nil; break end
    idx:update(add)
  end
  if idx then return queryUseIndexes(self, idx) end
end)

local function querySelect(iter, stTy, paths)
  return function()
    local i, st = iter(); if nil == i then return end
    local keys = {}; for key, path in pairs(paths) do
      keys[key] = pathVal(st, path)
    end
    return i, stTy(keys)
  end
end
-- select{'a.b', 'c.d'}}       -- accessible through .c, .d
-- select{{x='a.b', y='c.d'}}, -- now .x, .y
method(Query, 'select', function(self, sel)
  assert(Query == ty(self))
  local st = queryStruct(self)
  local paths, tys, namedTys = {}, {}, List{}
  for key, p in pairs(sel) do
    p = makePath(p)
    -- by index uses the last field name
    if 'number' == type(key) then key = p[#p] end
    if 'string' ~= type(key) then error(
      'must provide name for non-key path: ' .. tfmt(key)
    )end
    if paths[key] then error(
      'key used multiple times: ' .. key
    )end
    paths[key] = p; tys[key] = pathTy(st, p)
    namedTys:extend{key, tys[key]}
  end
  self['#path'] = List{}
  local st = genStruct('Q', namedTys)
  return Query{
    ['#iter']=querySelect(self:iter(), st, paths),
    ['#struct']=st,
    ['#path']=List{}, ['#ops']=List{},
  }
end)

-- The __index function for Query.
-- Mostly you do queries doing `q.structField.rightField.lt(3)`
--
-- If a struct field is (i.e) 'lt' you can use `path` like so:
--   q.path.lt.eq(3) -- lt less equal to 3
local function buildQuery(query, field)
  local st = queryStruct(query)
  st = pathTy(st, rawget(query, '#path'))
  local tys = assert(rawget(st, '#tys'), civ.tfmt(st))
  if not tys[field] then error(
    string.format("%s does not have field %s", st, field)
  )end
  query['#path']:add(field);
  return query
end
method(Query, '__index', function(self, k)
  local mv = getmetatable(self)[k]; if mv then return mv end
  if 'path' == k then return PathBuilder{['#query']=self} end
  return buildQuery(self, k)
end)
method(PathBuilder, '__index', function(self, field)
  return buildQuery(self['#query'])
end)

local opIml = {
  in_=function(op, v)    return op.value[v]     end,
  filter=function(op, f) return f(op.value)     end,
  eq =function(op, v) return op.value == v      end,
  lt =function(op, v) return op.value <  v      end,
  lte=function(op, v) return op.value <= v      end,
  gt =function(op, v) return op.value >  v      end,
  gte=function(op, v) return op.value >= v      end,
}
-- apply the query operations to the value
local function queryKeep(self, v)
  for _, op in ipairs(self['#ops']) do
    if not opIml[op.name](op, pathVal(v, op.path)) then
      return false
    end
  end
  return true
end
method(Query, '__call', function(self)
  local iter = rawget(self, '#iter')
  if iter then return iter() end
  while true do
    iter = self['#i']; local i = iter()
    local data = self['#picker'].data
    if not i or i > #data then return      end
    local v = data[i]
    if queryKeep(self, v) then return i, v end
  end
end)
method(Query, 'toList', function(self)
  assert(Query == ty(self))
  return List.fromIter(callMethod(self, 'iter'))
end)

local function asQuery(t)
  if(ty(t) == Query)  then return t end
  if(ty(t) == Picker) then return Query.new(t) end
  error("Invalid query type: " .. tostring(ty(t)))
end

method(Query, 'joinEq', function(
    left, leftField, right, rightField, idxQuery)
  assert(Query == ty(left))
  leftField = makePath(leftField); rightField = makePath(rightField)
  local idxField, noIdxField, noIdxQuery
  if idxQuery == right then
    right = asQuery(right); idxQuery = right
    idxField = rightField; noIdxField = leftField
    right = idxQuery
  else
    right = asQuery(right); noIdxQuery = right
    idxField = leftField;   noIdxField = rightField
    idxQuery = left
  end
  local st = queryStruct(idxQuery)
  local pty = pathTy(st, idxField)
  local path, idxPicker, baseIdx = queryIndexPath(
    idxQuery, 'eq', pty, idxField)

  if not idxPicker then
    idxPicker = Picker(
      assert(idxQuery['#struct']), List.fromIter(idxQuery))
  end
  local idxData = idxPicker.data
  local indexes = baseIdx:getPath(path, Map.empty)
  if indexes:isEmpty() then
    for i, st in ipairs(idxData) do
      local v = pathVal(st, idxField)
      indexes:get(v, List.empty):add(i)
    end
  end

  local joined = List{}
  local genSt = genStruct(
    'joinEq',
    {'j1', queryStruct(left), 'j2', queryStruct(right)})

  for i, st in noIdxQuery do
    local v = pathVal(st, noIdxField)
    for _, di in ipairs(indexes[v] or {}) do
      if left == idxQuery then joined:add(genSt{
        j1=idxData[di], j2=st})
      else                     joined:add(genSt{
        j1=st,          j2=idxData[di]})
      end
    end
  end
  return Picker(genSt, joined)
end)

-- ###################
-- # Test Harness

-- assert that globals haven't changed
local function globals()
  local out = Set{}
  for k in pairs(_G) do out[k] = true end
  return out
end
local function assertGlobals(prev, expect)
  expect = expect or Set{}
  local new = globals():diff(prev)
  if not eq(expect, new) then
    error(string.format("New globals: %s", new))
  end
end
assertGlobals(globals())

local function test(name, options, fn)
  local g, options = globals(), options or {}
  print("## Test", name)
  fn()
  assertGlobals(g, options.expectGlobals)
end

-- #####################
-- # Import Utilities

-- Optional require. Example:
--     myMod, err = want'myMod'
--     if not myMod then print(err) end
local function want(name)
  local out; if xpcall(
      function()  out = require(name) end,
      function(e) out = e end)
  then return out          -- success
  else return nil, out end -- error
end

local GREQUIRED = {}
-- global require: import and assign all values to be global
-- mod can be a name or a table
local function grequire(mod)
  mod = 'string' == type(mod) and require(mod) or mod
  if GREQUIRED[mod] then return GREQUIRED[mod] end
  for k, v in pairs(mod) do
    if nil ~= _G[k] then error(
      'grequire: "'..k..'" is already a global!'
    ) end
    _G[k] = v
  end
  GREQUIRED[mod] = mod
  return mod
end

local NANO  = 1000000000
local MILLI = 1000000000
local function durationSub(s, ns, s2, ns2)
  s, ns = s - s2, ns - ns2
  if ns < 0 then
    ns = NANO + ns
    s = s - 1
  end
  return s, ns
end

local function assertTime(t)
  assert(math.floor(t.s) == t.s, 'non-int seconds')
  assert(math.floor(t.ns) == t.ns, 'non-int nano-seconds')
  assert(t.ns < NANO, 'ns too large')
  return t
end

civ.Duration = struct('Duration', {{'s', Num}, {'ns', Num}})
constructor(civ.Duration, function(ty_, s, ns)
  if ns == nil then return civ.Duration.fromSeconds(s) end
  local out = {s=s, ns=ns}
  return setmetatable(assertTime(out), civ.Duration)
end)
civ.Duration.NANO = NANO
method(civ.Duration, 'fromSeconds', function(s)
  local sec = math.floor(s)
  return civ.Duration(sec, NANO * (s - sec))
end)
method(civ.Duration, 'fromMs', function(s)
  return civ.Duration(s / 1000)
end)
method(civ.Duration, 'asSeconds', function(self)
  return self.s + (self.ns / NANO)
end)
method(civ.Duration, '__sub', function(self, r)
  assert(ty(r) == Duration)
  local s, ns = durationSub(self.s, self.ns, r.s, r.ns)
  return Duration(s, ns)
end)
method(civ.Duration, '__lt', function(self, o)
  if self.s < o.s then return true end
  return self.ns < o.ns
end)
method(civ.Duration, '__tostring', function(self)
  return self:asSeconds() .. 's'
end)

-- time since the epoch
civ.Epoch = struct('Epoch', {{'s', Num}, {'ns', Num}})
constructor(civ.Epoch, function(ty_, s, ns)
  if ns == nil then return civ.Epoch.fromSeconds(s) end
  local out = {s=s, ns=ns}
  return setmetatable(assertTime(out), ty_)
end)
method(civ.Epoch, 'fromSeconds', function(s)
  local sec = math.floor(s)
  return civ.Epoch(sec, NANO * (sec - s))
end)
method(civ.Epoch, 'asSeconds', civ.Duration.asSeconds)
method(civ.Epoch, '__sub', function(self, r)
  assert(self)
  assert(r)
  assertTime(self)
  assertTime(r)
  local s, ns = durationSub(self.s, self.ns, r.s, r.ns)
  if ty(r) == Duration then return Epoch(s, ns) end
  assert(ty(r) == Epoch)
  return Duration(s, ns)
end)
method(civ.Epoch, '__tostring', function(self)
  return string.format('Epoch(%ss)', self:asSeconds())
end)

update(civ, {
  CIV = CIV,
  -- types
  Nil = Nil, Bool = Bool, Str  = Str,  Num = Num,
  Fn  = Fn,  Tbl  = Tbl,
  Map = Map, List = List, Set = Set,
  Iter = Iter, NoIndex = NoIndex,
  Range = Range,

  -- Generic operations
  copy = copy, eq = eq,
  update = update, extend = extend, pop = pop, -- table (map)
  sort = sort, -- table (list)
  split = split, lines = lines, trim = trim,        -- string
  matches = matches,

  -- Formatters
  concat = concat,
  fillBuf = fillBuf,

  -- struct
  ty = ty,
  struct = struct, constructor=constructor, method=method,
  pathVal = pathVal, pathTy = pathTy,
  dotSplit = dotSplit,
  genStruct = genStruct,
  Display = Display,

  -- newTy
  newTy = newTy,
  methIndex = methIndex,

  -- Picker
  Picker = Picker,

  -- file
  readAll = readAll,

  -- tests
  test = test,
  assertEq = assertEq,
  globals = globals, assertGlobals = assertGlobals,

  -- import utilities
  want=want, grequire=grequire,
})
return civ
