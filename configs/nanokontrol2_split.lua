-- Setup to use a nanoKontrol2 to drive both zynaddsubfx and jamulus.
-- The input is to be connected to nanoKontrol2.
-- Two outputs are created:
--
-- * "zynadd_ctrl" - Meant to be connected to ZynAddSubFX
-- The top knobs are mapped to all default GM controllers that Zynaddsubfx uses.
-- The rightmost fader is number 7 that controls volume.
--
-- * "jamulus_ctrl" - Meant to be connected to Jamulus
-- The 6 leftmost faders are sent to this output.
--
-- All the other events are discarded.

-- nanoKontrol2 default setup:
-- Top knobs: controllers 16 to 23.
-- Faders: controllers 0 to 7.
-- Solo buttons: controllers 32 to 39.
-- Mute buttons: controllers 48 to 55.
-- Record buttons: controllers 64 to 71.
-- Rewind: controller 43
-- Forward: controller 44
-- Stop: controller 42
-- Play: controller 41
-- Record: controller 45
-- Cycle: controller 46
-- Set Marker: controller 60
-- Previous Marker: controller 61
-- Next Marker: controller 62
-- Previous Track: controller 58
-- Next Track: controller 59

local mflib = require 'mflib'

config = mflib.make_empty_config()
mflib.set_client_name(config, 'nano2_split')
input = mflib.add_input(config, "nanokontrol2")

-- Setup for Zynaddsubfx
zynadd_output = mflib.add_output(config, "zynadd_ctrl")
knobs_selector = mflib.add_controller_selector(config, "pan_knobs",
                                              {lowest_controller=16,
                                               highest_controller=23})
fader_selector = mflib.add_controller_selector(config, "last_fader",
                                              {lowest_controller=7,
                                               highest_controller=7})
mapping = mflib.add_controller_mapping(
   config, "mapping",
   {mapping={
       [16]=11,
       [17]=71,
       [18]=74,
       [19]=75,
       [20]=76,
       [21]=77,
       [22]=78,
}})
mflib.connect(config, input, knobs_selector)
mflib.connect(config, input, fader_selector)
mflib.connect(config, knobs_selector, mapping)
mflib.connect(config, fader_selector, mapping)
mflib.connect(config, mapping, zynadd_output)

-- Setup for Jamulus
jamulus_output = mflib.add_output(config, "jamulus_ctrl")
jamulus_faders = mflib.add_controller_selector(config, "jamulus_faders",
                                               {lowest_controller=0,
                                                highest_controller=6})
mflib.connect(config, input, jamulus_faders)
mflib.connect(config, jamulus_faders, jamulus_output)

print(mflib.debug_dump(config))

