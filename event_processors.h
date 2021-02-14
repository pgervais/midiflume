#ifndef _EVENT_PROCESSORS_H
#define _EVENT_PROCESSORS_H
// Code that actually does the midi event processing.

#include <memory>
#include <iostream>
#include <vector>
#include <alsa/asoundlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lualib.h>

/* All possible note events. */
const snd_seq_event_type_t NOTE_EVENTS[] = {SND_SEQ_EVENT_NOTEON,
                                            SND_SEQ_EVENT_NOTEOFF,
                                            SND_SEQ_EVENT_NOTE,
                                            SND_SEQ_EVENT_KEYPRESS};

// All possible controller events.
const snd_seq_event_type_t CONTROLLER_EVENTS[] = {
  SND_SEQ_EVENT_CONTROLLER,
  SND_SEQ_EVENT_PGMCHANGE,
  SND_SEQ_EVENT_CHANPRESS,
  SND_SEQ_EVENT_PITCHBEND,
  SND_SEQ_EVENT_CONTROL14,
  SND_SEQ_EVENT_NONREGPARAM,
  SND_SEQ_EVENT_REGPARAM,
  SND_SEQ_EVENT_SONGPOS,
  SND_SEQ_EVENT_SONGSEL,
  SND_SEQ_EVENT_QFRAME,
  SND_SEQ_EVENT_TIMESIGN,
  SND_SEQ_EVENT_KEYSIGN
};

class EventProcessor {
public:
  EventProcessor();

  // This is used by ProcessorDAG to do consistency checks.
  virtual bool HasInputs() = 0;
  virtual bool HasOutputs() = 0;
  
  // Does any initialization, returns false in case of error.
  // Responsible for preallocating all necessary memory: ProcessEvent should
  // not allocate memory.
  virtual bool init();

  // Does the actual processing. A given processor can return an arbitrary
  // number of events, they usually return either one or zero (use that for
  // filtering).
  // This method should avoid allocating memory as much as possible.
  virtual std::vector<snd_seq_event_t>* ProcessEvent(const snd_seq_event_t& ev) = 0;
  
protected:
  // Processed events, preallocated by init().
  std::vector<snd_seq_event_t> events_;
};

class MidiInput: public EventProcessor {
public:
  MidiInput(const std::string& name, snd_seq_t *seq_handle):
    name_(name), seq_handle_(seq_handle) {}
  virtual bool init() override;
  
  virtual bool HasInputs() override { return false; }
  virtual bool HasOutputs() override { return true; }

  virtual std::vector<snd_seq_event_t>* ProcessEvent(const snd_seq_event_t& ev) override;
  
private:
  const std::string name_;
  snd_seq_t *seq_handle_;
  int port_num_;
};

class MidiOutput: public EventProcessor {
public:
  MidiOutput(const std::string& name, snd_seq_t *seq_handle):
    name_(name), seq_handle_(seq_handle) {}
  virtual bool init() override;

  virtual bool HasInputs() override { return true; }
  virtual bool HasOutputs() override { return false; }

  virtual std::vector<snd_seq_event_t>* ProcessEvent(const snd_seq_event_t& ev) override;

private:
  const std::string name_;
  snd_seq_t *seq_handle_;
  int port_num_;
};

class NoteSelector: public EventProcessor {
public:

  virtual bool HasInputs() override { return true; }
  virtual bool HasOutputs() override { return true; }

  NoteSelector(unsigned char lowest_note,
               unsigned char highest_note,
               unsigned char lowest_velocity,
               unsigned char highest_velocity):
    lowest_note(lowest_note),highest_note(highest_note),lowest_velocity(lowest_velocity),highest_velocity(highest_velocity) {}
  // Constructs the processor from a lua object.

  NoteSelector() {};
  bool InitFromLua(lua_State *L, int index);
  
  virtual std::vector<snd_seq_event_t>* ProcessEvent(const snd_seq_event_t& ev) override;

  /* Which notes events we want. Empty means all. */
  std::vector<snd_seq_event_type_t> types;
  /* Which channels to keep. Empty means all. */
  std::vector<unsigned char> channels;

 private:
  // TODO: add trailing underscore
  /* Lowest note to keep */
  unsigned char lowest_note = 0;
  /* Highest note to keep */
  unsigned char highest_note = 127;

  /* Lowest velocity to keep */
  unsigned char lowest_velocity = 0;
  /* Highest velocity to keep */
  unsigned char highest_velocity = 127;  
};

class ControllerSelector: public EventProcessor {
public:
  virtual bool HasInputs() override { return true; }
  virtual bool HasOutputs() override { return true; }

  // Constructs the processor from a lua object.
  ControllerSelector() {};
  bool InitFromLua(lua_State *L, int index);
  
  virtual std::vector<snd_seq_event_t>* ProcessEvent(const snd_seq_event_t& ev) override;

  /* Which channels to keep. Empty means all. */
  std::vector<unsigned char> channels_;

 private:
  /* Lowest note to keep */
  unsigned char lowest_controller_ = 0;
  /* Highest note to keep */
  unsigned char highest_controller_ = 127;
};

// Maps controller numbers to other ones.
class ControllerMapping: public EventProcessor {
public:
  virtual bool HasInputs() override { return true; }
  virtual bool HasOutputs() override { return true; }

  // Constructs the processor from a lua object.
  ControllerMapping() {
    // Initializes the mapping to identity.
    for (int i=0; i<128 ; i++) {
      controller_mapping_.push_back(i);
    }
  }
  bool InitFromLua(lua_State *L, int index);
  
  virtual std::vector<snd_seq_event_t>* ProcessEvent(const snd_seq_event_t& ev) override;

  /* Which channels to keep. Empty means all. */
  std::vector<unsigned char> channels_;

private:
  std::vector<unsigned char> controller_mapping_;
};

// Factory function for EventProcessor. Reads the config from
// a 'processor' Lua object.
std::unique_ptr<EventProcessor> MakeProcessorFromLua(lua_State *L, int index,
                                                     const std::string& name,
                                                     snd_seq_t *seq_handle);

#endif

