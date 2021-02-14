# Midiflume

## Overview
Midiflume is a small tool to route and possibly modify midi
events in realtime. The processing is defined by plugging together
preexisting `processors` between an arbitrary number of inputs and
outputs.

Targeted use cases are things like:
- keyboard split: events from a keyboard go in, the left half goes to
  one output, the right half to another.
- remap controllers from a hardware device that can't be programmed by
  rewriting the controller number in the midi message.
- note transposition.
- event masking: when your keyboard is emitting a midi signal you wish
  it didn't, but have no way to disable.

If the use case does not imply some modification or filtering of midi
events then using a midi patchbay is what you're looking for.
midiflume is meant to be used with an external patchbay like
qjackctl. It has no way to connect itself to anything.

Non goals:
- recording or playing back events. This is best achieved with a midi
  recorder (Ardour, Rosegarden, Muse, etc.). 
- being an arpeggiator. There are dedicated tools for this: for ex.
  QMidiArp. 

Similar tools:
- Midish https://midish.org/
- MidiFlow (non free) iOS app https://www.midiflow.com/

## Usage

Launch the tool with:

    midiflume config.lua

Then use a patchbay to connect the new inputs and outputs to other
things.

The `config.lua` file is a plain Lua file that is executed by
midiflume upon startup. Its only job it to define a global variable
`config` containing the defining of the processing graph. The
mflib.lua module hides the details under a user friendly interface.

Example:

    local mflib = require 'mflib'
    
    config = mflib.make_empty_config()
    input = mflib.add_input(config, "input")
    selector = mflib.add_controller_selector(config, "knobs_selector",
                                             {lowest_controller=16,
                                              highest_controller=23})
    output = mflib.add_output(config, "knobs")
    mflib.connect(config, input, selector)
    mflib.connect(config, selector, output)

This configuration defines one input and one output (named resp.
"input" and "knobs"), and connects them through a processor that
keeps only controller events, and only those with numbers between 16
and 23 (these happen to correspond to the 8 knobs at the top of a
nanoKontrol2, hence the name).

When this configuration is used, all events coming through "input" are
discarded, except if they match the filtering criteria (controller
between 16 and 23). Anything connected to "knobs" will only see these
8 controllers and nothing else.

We can make the config more useful by also mapping the faders and
sending them to a second output:


    local mflib = require 'mflib'
    
    config = mflib.make_empty_config()
    input = mflib.add_input(config, "input")

    -- Knobs
    knob_selector = mflib.add_controller_selector(config, "knobs_selector",
                                             {lowest_controller=16,
                                              highest_controller=23})
    knob_output = mflib.add_output(config, "knobs")
    mflib.connect(config, input, knob_selector)
    mflib.connect(config, knob_selector, knob_output)

    -- Faders
    fader_selector = mflib.add_controller_selector(config, "fader_selector",
                                                      {lowest_controller=0,
                                                       highest_controller=7})
    fader_output = mflib.add_output(config, "faders")
    mflib.connect(config, input, fader_selector)
    mflib.connect(config, fader_selector, fader_output)

With this configuration, `midiflume` has two outputs, one for the
controller events from knobs, and the other for faders (again using
the nanoKontrol2 as an example here).


## Processors
Here's the exhaustive list of processors. In all cases `config` is the
table created by mflib.make_empty_config(), and `name` is a unique
name for this particular processor.


### Input

Defines a named midi input.

    mflib.add_input(config, name)

`name` is both the processor name as known to midiflume and the
name of the midi socket visible to other programs.


### Output

Defines a named midi output.

    mflib.add_output(config, name)

`name` is both the processor name as known to midiflume and the
name of the midi socket visible to other programs.


### Controller selector

Lets through only controller events within a certain range.

Signature:

    mflib.add_controller_selector(config, name, options)

With `options` a table with keys:

- lowest_controller: smallest controller number to keep (0-127).
- highest_controller: highest controller number to keep (0-127).


### Note selector

Lets through only note events within a certain range of pitch and
velocity.

Signature:

    mflib.add_note_selector(config, name, options)

With `options` a table with keys:

- lowest_note: smallest controller number to keep (0-127).
- highest_note: highest controller number to keep (0-127).
- lowest_velocity: smallest velocity value to let through (0-127).
- highest_velocity: highest velocity value to let through (0-127).

All keys are optional. By default all notes and all velocity go through.


### Controller mapping

Changes the controller number on controller events. This can be used
to remap a knob to a controller number accepted by a software synth or
a plugin. All other events than controller are let through unchanged.

    mflib.add_controller_mapping(config, name, options)

With `options` containing the mapping between controller numbers.
`options` is a table with a key "mapping" which is itself a table
mapping the original controller number to the new one.

Example:

    options = { mapping={[0]=7, [5]=8}}

This will renumber all events from controller 0 to controller 7
(without changing the controller value), and controller 5 to
controller 8. All other controller events are let through unchanged.


## Development

### Adding a new processor

- Add the processor in event_processors.cc|h, as a class derived from
  EventProcessor. The constructor must not fail so it must not contain
  e.g. code that does memory allocation. If some memory allocation is
  necessary, use the init() method (see MidiInput::init()). Add the
  actual processing code in the ProcessEvent() method, whose return
  value must not be null (return an empty array if no event has been
  generated). Finally implement InitFromLua(). Upon calling, the Lua
  stack will contain the table for the configuration for this
  particular processor, and the stack must be in the same state when
  InitFromLua ends.

- Add a new branch to instantiate the new processor in
  MakeProcessorFromLua (in event_processors.cc).

- Add a new function to mflib.lua to create the processor object. The
  naming convention is add_<processor_name>(). A processor object must
  contain a field "processor_type" with the value that has been used
  in MakeProcessorFromLua. All other fieds are arbitrary and must only
  match what the InitFromLua method expects.

### Adding tests

Midiflume uses Catch2 v2 as testing harness. Please refer to the
documentation at https://github.com/catchorg/Catch2/tree/v2.x for
details. For simplicity the single-header catch.hpp file has been
vendored under third_party/.
