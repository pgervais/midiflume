-- Midi routing function library.
local mflib = {}

-- Internal functions

function make_set(list)
   local set = {}
   for _, l in ipairs(list) do set[l] = true end
   return set
end

function check_args(options, name_set)
   -- Checks that 'options' only contains keys that are present in 'names'
   for name in next, options do
      if name_set[name] == nil then
         error("Invalid argument name: " .. name, 2)
      end
   end
end

function check_name(config, name)
   if config.processors[name] ~= nil then
      error("Processor name already used: " .. name, 2)
   end
end

function merge_tables(destination, update)
   for k,v in pairs(update) do
      destination[k] = v
   end
   return destination
end

-- Debugging functions

function mflib.debug_dump(o, level)
   level = level or 0
   if type(o) == 'table' then
      local s = '{ \n'
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. string.rep(" ", level+3) .. '['..k..'] = ' .. mflib.debug_dump(v, level+3) .. ',\n'
      end
      return s .. string.rep(" ", level) .. '}'
   else
      return tostring(o)
   end
end

-- Actual API

function mflib.set_client_name(config, name)
   config.client_name = name
end

function mflib.make_empty_config()
   -- set all the default values here.
   return {
      processors = {},
      connections = {},
   }
end

function mflib.add_input(config, name)
   check_name(config, name)
   config.processors[name] = {
      _obtype = "processor",
      processor_type="midi_input"
   }
   return name
end

function mflib.add_output(config, name)
   check_name(config, name)
   config.processors[name] = {
      _obtype = "processor",
      processor_type="midi_output"
   }
   return name
end


function mflib.add_note_selector(config, name, options)
   check_args(options, make_set{"lowest_note", "highest_note",
                                "lowest_velocity", "highest_velocity",
                                "channels", "types"})

   config.processors[name] = merge_tables(
      {
         _obtype = "processor",
         processor_type="note_selector",
      },
      options)
   return name   
end

function mflib.add_controller_selector(config, name, options)
   check_args(options, make_set{"lowest_controller", "highest_controller"})

   config.processors[name] = merge_tables(
      {
         _obtype = "processor",
         processor_type="controller_selector",
      },
      options)
   return name   
end

function mflib.add_controller_mapping(config, name, options)
   check_args(options, make_set{"mapping"})

   config.processors[name] = merge_tables(
      {
         _obtype = "processor",
         processor_type="controller_mapping",
      },
      options)
   return name   
end

function mflib.connect(config, input, output)
   table.insert(config.connections, {
      _objtype = "connection",
      input = input,
      output = output
   })
end

return mflib
