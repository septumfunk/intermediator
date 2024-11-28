#pragma once
#include "modules.h"
#include <lua.h>
#include <lua_all.h>

int api_net_send_tcp(lua_State *L);
int api_net_broadcast_tcp(lua_State *L);