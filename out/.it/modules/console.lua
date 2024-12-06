---[API] The console module of the scripting api. Use this to log to the console rather than lua's io functions.
net.console = {}

---[API] Log to stdout with timestamp information.
---@param info string
net.console.log = function(info)end

---[API] Log a warning to stdout with timestamp information.
---@param warning string
net.console.warn = function(warning)end

---[API] Log an error to stderr with timestamp information.
---@param error string
net.console.error = function(error)end

---[API] Log a header to stdout with timestamp information.
---@param header string
net.console.header = function(header)end