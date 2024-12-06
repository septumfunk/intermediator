---[API] The tables module of the scripting api. Used to manipulate, copy and merge the values of tables among other things.
net.tables = {}

---[API] Merge the values of two tables, the second overriding the first. This creates a new table.
---@param table1 table
---@param table2 table
---@return table merged
---@diagnostic disable-next-line: missing-return
net.tables.merge = function(table1, table2)end

---[API] Copy the values of a table over to another, overriding duplicates.
---@param from table
---@param to table
net.tables.copy = function(from, to)end