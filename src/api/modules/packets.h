#pragma once
#include "modules.h"

int api_packets_send_tcp(lua_State *L);
int api_packets_broadcast_tcp(lua_State *L);

int api_packets_send_udp(lua_State *L);
int api_packets_broadcast_udp(lua_State *L);

int api_packets_reply(lua_State *L);