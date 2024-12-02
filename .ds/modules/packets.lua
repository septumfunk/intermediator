---[API] The packets module of the scripting api. Used to manipulate and send/broadcast packets over the network.
ds.packets = {}

---[API] Send a packet to a client by uuid, over TCP.
---@param uuid string
---@param type string
---@param packet table
ds.packets.send_tcp = function(uuid, type, packet)end

---[API] Send a packet to a client by uuid, over UDP.
---@param uuid string
---@param type string
---@param packet table
ds.packets.send_udp = function(uuid, type, packet)end

---[API] Send a reply packet to a client. Replies only use TCP.
---@param to table
---@param reply table
ds.packets.reply = function(to, reply)end