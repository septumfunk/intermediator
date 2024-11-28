ds.events.test = function(event)
    ds.net.send_tcp(event.client.uuid, "test", event)
end