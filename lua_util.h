#ifndef _LUA_UTIL_H
#define _LUA_UTIL_H
#include <lua5.3/lua.h>

#define RETURN_IF_FALSE(EXPR) if (!(EXPR)) { return false; }
bool GetStringField(lua_State *L, int index, const char* field_name, std::string* value,
                    bool missing_is_error = true);
bool GetIntegerField(lua_State *L, int index, const char* field_name, int* value,
                     bool missing_is_error = true);
void PrintStackTypes(lua_State *L, int num);
#endif
