local M = {}

local add = table.insert

-- ###################
-- # Formatting
-- lua cannot format raw tables. We fix that, and also build up
-- for formatting structs/etc

-- tfmt and tfmtBuf are ultra-simple implementations of table formatting
-- that do NOT use __tostring or __name.
M.tfmtBuf=function(b, t)
  if type(t) ~= 'table' then return add(b, tostring(t)) end
  add(b, '{'); local added = 0
  for i, v in ipairs(t) do
    tfmtBuf(b, v); add(b, '; ');
    added = added + 1
  end
  if added > 0 then b[#b] = ' :: ' end
  for k, v in pairs(t) do
    if type(k) == 'number' and k <= added then -- already added
    else
      tfmtBuf(b, k); add(b, '=')
      tfmtBuf(b, v); add(b, '; ')
      added = added + 1
    end
  end
  b[#b + ((added == 0 and 1) or 0)] = '}'
end

M.tfmt=function(t)
  local b = {}; tfmtBuf(b, t); return table.concat(b)
end

-- local _bufFmtTy = {
--   number = table.insert,
--   boolean = function(b, bool)
--     if(bool) then table.insert(b, 'true')
--     else          table.insert(b, 'false') end
--   end,
--   ['nil'] = function(b, obj) table.insert(b, 'nil') end,
--   string = function(b, s)
--     if not string.find(s, '%s') then table.insert(b, s)
--     else
--       table.insert(b, '"')
--       table.insert(b, s)
--       table.insert(b, '"')
--     end
--   end,
--   ['function'] = civ.fmtFnBuf,
--   table = function(b, t)
--     local mt = getmetatable(t)
--     if mt and mt.iter and b.indent then -- nothing
--     elseif mt and mt.__tostring then
--       return table.insert(b, tostring(t))
--     end
--     fmtTableRaw(b, t, orderedKeys(t))
--   end,
-- }
--
-- local function fmtTableIndent(b, t, keys)
--   local indents = b.indents or 0; b.indents = indents + 1
--   local added = false
--   table.insert(b, '\n')
--   for _, key in keys do
--     added = true
--     for i=1, indents-1 do table.insert(b, indent) end
--     table.insert(b, '+ ')
--     fmtBuf(b, key);    table.insert(b, '=')
--     fmtBuf(b, t[key]); table.insert(b, '\n')
--   end
--   b.indents = indents - 1
--   if not added then  b[#b] = 'HERE IS MEj'
--   else               b[#b] = '\n' end
-- end
-- 
-- local function fmtTableRaw(b, t, keys)
--   if b.indent then return fmtTableIndent(b, t, keys) end
--   table.insert(b, '{')
--   local endAdd = 1
--   for _, key in keys do
--     if pre then table.insert(b, pre) end
--     fmtBuf(b, key);    table.insert(b, '=')
--     fmtBuf(b, t[key]); table.insert(b, ' ')
--     endAdd = 0 -- remove space for last
--   end
--   b[#b + endAdd] = '}'
-- end

return M
