/* Midiflume: something like midish but hopefully easier to use. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <unistd.h>

#include <lua5.3/lua.h>
#include "lua_config.h"
#include "event_processors.h"
#include "dag.h"

// TODO: move this function elsewhere (in a test e.g.)
bool GetTestProcessingGraph(snd_seq_t *seq_handle, ProcessorDAG *dag) {
  size_t input1_index = dag->AddProcessor(std::make_unique<MidiInput>("midiflume_0",
                                                                      seq_handle));
  size_t input2_index = dag->AddProcessor(std::make_unique<MidiInput>("midiflume_1",
                                                                      seq_handle));
  size_t output1_index = dag->AddProcessor(std::make_unique<MidiOutput>("midiflume_0",
                                                                        seq_handle));
  size_t output2_index = dag->AddProcessor(std::make_unique<MidiOutput>("midiflume_1",
                                                                        seq_handle));
  size_t note1_index = dag->AddProcessor(std::make_unique<NoteSelector>(0,64,0,127));
  size_t note2_index = dag->AddProcessor(std::make_unique<NoteSelector>(65,127,0,127));
  
  bool ok = true;
  ok = dag->AddConnection(input1_index, output1_index);
  if (!ok) { return false; }
  ok = dag->AddConnection(input2_index, note1_index);
  if (!ok) { return false; }
  ok = dag->AddConnection(input2_index, note2_index);
  if (!ok) { return false; }
  ok = dag->AddConnection(note1_index, output1_index);
  if (!ok) { return false; }
  ok = dag->AddConnection(note2_index, output2_index);
  if (!ok) { return false; }

  dag->Finalize();
  return true;
}

/* Opens ALSA sequencer.
   The sequencer handle is returned. */  
bool open_seq(snd_seq_t **seq_handle, const char* client_name) {
  if (snd_seq_open(seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
    std::cerr << "Error opening ALSA sequencer.\n";
    return false;
  }
  snd_seq_set_client_name(*seq_handle, client_name);
  return true;
}

// The main processing loop.
void ProcessEvents(snd_seq_t *seq_handle,
                  ProcessorDAG& processing_graph) {

  int npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
  struct pollfd *pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
  snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);
  
  while (true) {
    if (poll(pfd, npfd, 100000) > 0) {
      do {
        snd_seq_event_t *ev;
        snd_seq_event_input(seq_handle, &ev);
    
        // Filters out connection events which we don't want to process.
        if (ev->type >= SND_SEQ_EVENT_CLIENT_START && ev->type < SND_SEQ_EVENT_USR0) {
          snd_seq_free_event(ev);
          continue;
        }
        if (!processing_graph.ProcessEvent(*ev)) {
          std::cerr << "Error processing event.\n";
        }
        snd_seq_free_event(ev);
      } while (snd_seq_event_input_pending(seq_handle, 0) > 0);
    }  
  }
}


bool ParseFlags(int argc, char *argv[],
                std::string* client_name,
                std::string* lua_config_filename) {
  std::string input = "";

  // FIXME: make -c mandatory.
  if ( (argc <= 1) || (argv[argc-1] == NULL) || (argv[argc-1][0] == '-') ) {
    // There is NO input...
    std::cerr << "Usage: midi_flume -n <filename.lua>\n";
    return false;
  }
  opterr = 0;

  // FIXME: return false in case of unknown option.
  int opt;
  while ( (opt = getopt(argc, argv, "c:n:")) != -1 ) {
    switch (opt) {
    case 'n':
      *client_name = optarg;
      break;
    case 'c':
      *lua_config_filename = optarg;
      break;
    }
  }

  return true;
}


int main(int argc, char *argv[]) {

  // Default config file value.
  // TODO: make that a path under ~/.config/midiflume
  std::string lua_config_filename = "midiflume.lua";

  // Default client name
  std::string client_name = "midiflume";
  
  std::string flag_client_name;
  if (!ParseFlags(argc, argv,
                  &flag_client_name,
                  &lua_config_filename)) {
    return 1;
  }

  lua_State *L;
  if (!ReadConfigFile(lua_config_filename, &L)) {
    return 1;
  }

  std::string config_client_name;
  if (!GetClientName(L, &config_client_name)) {
    return 1;
  }

  // Override the client name but config value first, then flag value.
  if (!config_client_name.empty()) {
    client_name = config_client_name;
  }
  if (!flag_client_name.empty()) {
    client_name = flag_client_name;
  }
  
  std::cerr << "config_client_name = " << config_client_name << std::endl;
  std::cerr << "flag_client_name = " << config_client_name << std::endl;
  std::cerr << "client_name = " << client_name << std::endl;
  std::cerr << "lua_config_filename = " << lua_config_filename << std::endl;

  snd_seq_t *seq_handle;
  if (!open_seq(&seq_handle, client_name.c_str())) {
    lua_close(L);
    exit(1);
  }
  
  ProcessorDAG processing_graph;
  if (!GetProcessingGraph(L, seq_handle, &processing_graph)) {
    std::cerr << "Error getting processing graph\n";
    lua_close(L);
    exit(1);
  }
  ProcessEvents(seq_handle, processing_graph);
  lua_close(L);
}
