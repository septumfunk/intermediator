---[API] The logging module of the scripting api. Use this to log to the console rather than lua's io functions.
ds.log = {}

---[API] Log information to stdout with timestamp information.
---@param info string
ds.log.info = function(info)end

---[API] Log a warning to stdout with timestamp information.
---@param warning string
ds.log.warn = function(warning)end

---[API] Log an error to stderr with timestamp information.
---@param error string
ds.log.error = function(error)end

---[API] Log a header to stdout with timestamp information.
---@param header string
ds.log.header = function(header)end