---[API] The logging module of the scripting api. Use this to log to the console rather than lua's io functions.
net.log = {}

---[API] Log information to stdout with timestamp information.
---@param info string
net.log.info = function(info)end

---[API] Log a warning to stdout with timestamp information.
---@param warning string
net.log.warn = function(warning)end

---[API] Log an error to stderr with timestamp information.
---@param error string
net.log.error = function(error)end

---[API] Log a header to stdout with timestamp information.
---@param header string
net.log.header = function(header)end