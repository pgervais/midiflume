// Code that actually does the midi event processing.

#include <memory>
#include <iostream>
#include <vector>
#include <alsa/asoundlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lualib.h>

#include "lua_util.h"
#include "event_processors.h"

// EventProcessor
EventProcessor::EventProcessor() {}
bool EventProcessor::init() {
  // Most processors will return either one or zero elements, so use that
  // as the default.
  events_.reserve(1);  
  return true;
}

// MidiInput
bool MidiInput::init() {
  events_.reserve(1);
  if (seq_handle_ == nullptr) {
    std::cerr << "MidiInput: Null seq_handle, ignoring init. "
              << "This is intended for testing only\n";
    return true;
  }
  
  if ((port_num_ = snd_seq_create_simple_port(
           seq_handle_, name_.c_str(),
           SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
           SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
    std::cerr << "Error creating sequencer input port " << name_ << "\n";
    return false;
  }
  std::cerr << "Created input " << port_num_ << " (" << name_ << ")\n";
  return true;
}

std::vector<snd_seq_event_t>*
MidiInput::ProcessEvent(const snd_seq_event_t& ev) {
  events_.clear();
  if (ev.dest.port != port_num_) {
    return &events_;
  }
  events_.push_back(ev);
  return &events_;
}

// MidiOutput
bool MidiOutput::init() {
  // Not reserving any memory in events_ because we don't need it.
  if (seq_handle_ == nullptr) {
    std::cerr << "MidiOutput: Null seq_handle, ignoring init. "
              << "This is intended for testing only\n";
    return true;
  }

  if ((port_num_ = snd_seq_create_simple_port(
           seq_handle_, name_.c_str(),
           SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
           SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
    std::cerr << "Error creating sequencer output port " << name_ << "\n";
    return false;
  }
  std::cerr << "Created output " << port_num_ << " (" << name_ << ")\n";
  return true;
}

std::vector<snd_seq_event_t>*
MidiOutput::ProcessEvent(const snd_seq_event_t& ev) {
  snd_seq_event_t event = ev;
  snd_seq_ev_set_subs(&event);
  snd_seq_ev_set_direct(&event);
  snd_seq_ev_set_source(&event, port_num_);
  snd_seq_event_output_direct(seq_handle_, &event);
  // We want to return an empty vector.
  events_.clear();
  return &events_;
}

// NoteSelector
bool NoteSelector::InitFromLua(lua_State *L, int index) {
  int value;
  if (GetIntegerField(L, index, "lowest_note", &value)) {
    lowest_note = static_cast<unsigned char>(value);
  }
  if (GetIntegerField(L, index, "highest_note", &value)) {
    highest_note = static_cast<unsigned char>(value);
  }
  if (GetIntegerField(L, index, "lowest_velocity", &value)) {
    lowest_velocity = static_cast<unsigned char>(value);
  }
  if (GetIntegerField(L, index, "highest_velocity", &value)) {
    highest_velocity = static_cast<unsigned char>(value);
  }
  return true;
}

std::vector<snd_seq_event_t>*
NoteSelector::ProcessEvent(const snd_seq_event_t& ev) {
  events_.clear();
  for (const snd_seq_event_type_t ev_type : NOTE_EVENTS) {
    if (ev.type == ev_type) {
      bool keep = true;

      if (!channels.empty()) {
        keep = false;
        for (const auto channel: channels) {
          if (ev.data.note.channel == channel) {
            keep = true;
            break;
          }
        }
      }
        
      if (keep && !types.empty()) {
        keep = false;
        for (const auto type: types) {
          if (ev.type == type) {
            keep = true;
            break;
          }
        }
      }
        
      if (keep && ((ev.data.note.note > highest_note)
                   || (ev.data.note.note < lowest_note))) {
        keep = false;
      }
      if (keep && ((ev.data.note.velocity > highest_velocity)
                   || (ev.data.note.velocity < lowest_velocity))) {
        keep = false;
      }

      if (!keep) {
        return &events_;
      }
    }
  }
  events_.push_back(ev);
  return &events_;
}

// ControllerSelector
bool ControllerSelector::InitFromLua(lua_State *L, int index) {  
  int value;
  if (GetIntegerField(L, index, "lowest_controller", &value)) {
    lowest_controller_ = static_cast<unsigned char>(value);
  }
  if (GetIntegerField(L, index, "highest_controller", &value)) {
    highest_controller_ = static_cast<unsigned char>(value);
  }
  return true;
}

std::vector<snd_seq_event_t>*
ControllerSelector::ProcessEvent(const snd_seq_event_t& ev) {
  events_.clear();
  if (ev.type == SND_SEQ_EVENT_CONTROLLER) {
    bool keep = true;
    
    if (!channels_.empty()) {
      keep = false;
      for (const auto channel: channels_) {
        if (ev.data.note.channel == channel) {
          keep = true;
          break;
        }
      }
    }
    
    if (keep && ((ev.data.control.param > highest_controller_)
                 || (ev.data.control.param < lowest_controller_))) {
      keep = false;
    }
    
    if (!keep) {
      return &events_;
    }
  }
  events_.push_back(ev);
  return &events_;
}

// ControllerMapping
bool ControllerMapping::InitFromLua(lua_State *L, int index) {
  lua_getfield(L, index, "mapping");
  lua_pushnil(L);  /* first key */

  while (lua_next(L, -2) != 0) {
    if (!lua_isinteger(L, -1)) { return false; }
    if (!lua_isinteger(L, -2)) { return false; }
    // Sets the mapping.
    int out_controller = lua_tointeger(L, -1);
    int in_controller = lua_tointeger(L, -2);
    
    if (in_controller < 0 || in_controller > 127) {
      std::cerr << "Controller number outside [0,127]: " << in_controller;
      return false;
    }
    if (out_controller < 0 || out_controller > 127) {
      std::cerr << "New controller number outside [0,127]: " << out_controller;
      return false;
    }
    controller_mapping_[static_cast<size_t>(in_controller)] =
      static_cast<unsigned char>(out_controller);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  return true;
}

std::vector<snd_seq_event_t>*
ControllerMapping::ProcessEvent(const snd_seq_event_t& ev) {
  events_.clear();
  if (ev.type == SND_SEQ_EVENT_CONTROLLER) {
    events_.emplace_back(ev);
    events_.back().data.control.param =
      controller_mapping_[ev.data.control.param];
  }
  return &events_;
}

// Factory for all processors from a Lua object.
// Expects a 'processor' table at index 'index'.
std::unique_ptr<EventProcessor> MakeProcessorFromLua(lua_State *L, int index,
                                                     const std::string& name,
                                                     snd_seq_t *seq_handle) {
  std::string type;
  if (!GetStringField(L, -1, "processor_type", &type)) {
    return nullptr;
  }

  if (type == "midi_input") {
    return std::make_unique<MidiInput>(name, seq_handle);
  } else if (type == "midi_output") {
    return std::make_unique<MidiOutput>(name, seq_handle);    
  } else if (type == "note_selector") {
    auto processor = std::make_unique<NoteSelector>();
    processor->InitFromLua(L, index);
    return processor;
  } else if (type == "controller_selector") {
    auto processor = std::make_unique<ControllerSelector>();
    processor->InitFromLua(L, index);
    return processor;
  } else if (type == "controller_mapping") {
    auto processor = std::make_unique<ControllerMapping>();
    processor->InitFromLua(L, index);
    return processor;
  }

  std::cerr << "Unknown processor type: " << type << "\n";
  return nullptr;
}


