#ifndef _LUA_CONFIG_H
#define _LUA_CONFIG_H

#include <alsa/asoundlib.h>
#include "dag.h"

bool GetProcessingGraph(lua_State *L, snd_seq_t *seq_handle,
                        ProcessorDAG *dag);

bool ReadConfigFile(const std::string& lua_filename, lua_State **L);
bool GetClientName(lua_State *L, std::string *client_name);
#endif
