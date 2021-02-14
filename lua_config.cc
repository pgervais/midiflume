/* Functions related to the lua configuration file. */

#include <iostream>
#include <memory>
#include <alsa/asoundlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lualib.h>

#include "event_processors.h"
#include "lua_util.h"
#include "lua_config.h"
#include "dag.h"


// Check that field "k" is a table in object at position 'index'.
bool CheckFieldIsTable(lua_State *L, int index,  const char* k) {
  lua_getfield(L, index, k);
  bool is_table = lua_istable(L, -1);
  if (!is_table) {
    std::cerr << "field \"" << k << "\" is not a table\n";
  }
  lua_pop(L, 1);
  return is_table;
}

// Creates processors based on the info from the table at position 'index'.
bool AddProcessors(lua_State *L, int index, ProcessorDAG *dag, snd_seq_t *seq_handle) {
  std::string name;
  
  // Iterate over processors.
  lua_pushnil(L);  /* first key */
  while (lua_next(L, index-1) != 0) {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    if (lua_isstring(L, -2)) {
      name = lua_tolstring(L, index-1, nullptr);
      std::cerr << "Adding processor: " << name << "\n";

      auto processor = MakeProcessorFromLua(L, -1, name, seq_handle);

      if (processor == nullptr) {
        lua_pop(L, 1);
        return false;
      }
      dag->AddProcessor(std::move(processor), name);
    }

    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }
  return true;
}

// Creates connections between processor based on the info from the table
// at position 'index'
bool AddConnections(lua_State *L, int index, ProcessorDAG *dag) {
  // Iterate over connections.
  lua_pushnil(L);  /* first key */
  while (lua_next(L, index-1) != 0) {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    if (lua_istable(L, -1)) {
      std::string input_name;
      RETURN_IF_FALSE(GetStringField(L, -1, "input", &input_name));
      std::string output_name;
      RETURN_IF_FALSE(GetStringField(L, -1, "output", &output_name));
      std::cerr << "Adding connection: " << input_name
                << " -> " << output_name << "\n";
      RETURN_IF_FALSE(dag->AddConnection(input_name, output_name));
    } else {
      std::cerr << "Unexpected non-table value for connection information.\n";
      PrintStackTypes(L, 1);
      return false;
    }
    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }
  return true;
}

bool GetProcessingGraph(lua_State *L,
                        snd_seq_t *seq_handle,
                        ProcessorDAG *dag) {

  // Load the config object and do some basic checks.
  lua_getglobal(L, "config");
  if (!lua_istable(L, -1)) {
    std::cerr << "The 'config' value obtained from the lua config "
              << "file is either not a table or not defined.";
    return false;
  }

  // Now we know that config is a table.
  RETURN_IF_FALSE(CheckFieldIsTable(L, -1, "connections"));
  RETURN_IF_FALSE(CheckFieldIsTable(L, -1, "processors"));
  
  // Now we know that config.connections and config.processors are tables.
  //  PrintStackTypes(L, 4);
  lua_getfield(L, -1, "processors");  
  RETURN_IF_FALSE(AddProcessors(L, -1, dag, seq_handle));
  lua_pop(L, 1);

  lua_getfield(L, -1, "connections");  
  RETURN_IF_FALSE(AddConnections(L, -1, dag));
  lua_pop(L, 1);

  return dag->Finalize();
}

bool ReadConfigFile(const std::string& lua_filename, lua_State **L) {
  // TODO: automatically preload the mrlib library?
  *L = luaL_newstate();
  luaL_openlibs(*L);
  if (luaL_loadfile(*L, lua_filename.c_str())) {
    std::cerr << "\nError during loading of lua file\n";
    if (lua_isstring(*L, -1)) {
      std::cerr << lua_tolstring(*L, -1, nullptr) << "\n";
    }
    lua_close(*L);
    return false;
  }
  std::cerr << "lua file loaded successfully\n";

  int error = lua_pcall(*L, 0, 0, 0);
  if (error != 0) {
    std::cerr << "\nError during execution of lua file\n";
    if (lua_isstring(*L, -1)) {
      std::cerr << lua_tolstring(*L, -1, nullptr) << "\n";
    }
    lua_close(*L);
    return false;
  }
  
  return true;  
}

bool GetClientName(lua_State *L, std::string *client_name) {
  // Reads config.client_name

  lua_getglobal(L, "config");
  if (!lua_istable(L, -1)) {
    std::cerr << "The 'config' value obtained from the lua config "
              << "file is either not a table or not defined.";
    lua_pop(L, 1);
    return false;
  }

  GetStringField(L, -1, "client_name", client_name, false);
  lua_pop(L, 1);
  return true;
}
