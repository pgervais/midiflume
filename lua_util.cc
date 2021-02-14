
#include <iostream>
#include <lua5.3/lua.h>
#include "lua_util.h"

bool GetStringField(lua_State *L, int index, const char* field_name, std::string* value,
                    bool missing_is_error) {
  // Returns the string obtained from field 'field_name' extracted from table
  // at index 'index'.
  lua_getfield(L, index, field_name);
  bool is_string = lua_isstring(L, -1);
  if (!is_string) {
    if (missing_is_error) {
      std::cerr << "field \"" << field_name << "\" is not a string\n";
    }
    return false;
  }
  *value = lua_tolstring(L, -1, nullptr);
  lua_pop(L, 1);

  return true;
}

bool GetIntegerField(lua_State *L, int index, const char* field_name, int* value,
                     bool missing_is_error) {
  // Returns the integer obtained from field 'field_name' extracted from table
  // at index 'index'.
  lua_getfield(L, index, field_name);
  if (!lua_isinteger(L, -1)) {
    if (missing_is_error) {
      std::cerr << "field \"" << field_name << "\" is not an integer\n";
    }
    lua_pop(L, 1);
    return false;
  }
  *value = lua_tointeger(L, -1);
  lua_pop(L, 1);
  return true;
}

void PrintStackTypes(lua_State *L, int num) {
  std::cerr << "= Lua stack types\n";
  for (int i = 1; i < num+1; i++) {
    std::cerr << -i << ": " << lua_typename(L, lua_type(L, -i)) << "\n";
  }
}
