---[API] The main dynamicserver table containing all api components and modules.
ds = {
    ---[API] The table containing all events. You can add functions to this table named after an event to register new ones.
    events = {},
    ---[API] The table containing configuration for the server. You should modify this in `config.lua`
    config = {
        ---[CONFIG] The port the server's TCP socket should bind to.
        tcp_port = 5060,
        ---[CONFIG] The port the server's UDP socket should bind to.
        udp_port = 5060,
        ---[CONFIG] The max amount of players the server should allow to connect.
        max_players = 500,
    },
    ---[API] The table of all connected clients and their data. You can index this with a uuid (string) to access other clients' data.
    clients = {},
}