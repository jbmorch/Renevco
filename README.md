# Renevco
Reverse engineering a scoreboard controller

## Summary
This is a hardware and embedded software project to create a shot clock for a Nevco scoreboard system that is installed in a gym in San Diego, California, USA. The current system includes two wall-mounted large format scoreboard displays and a Nevco MPCW-6 controller. The controller connects to the displays via RG-6 cables with BNC connectors. The controller has two RJ-11 ports on either side for connected a handheld switch that controls the clock and the horn.

### Version 1
The first round of the project will be to create a shot clock that is independent of the Nevco system, integrating a display and a user interface. The goal is to make it easier for a single person to keep score and manage the shot clock.

**Requirements**
+ Physical dimesions approximately 20x15x10 cm (WxHxD), but depends on proof of concept testing
+ Runs on 5v DC power and ideally less than 1A, supplied by a wall wart power supply
+ There should be easy access to the firmware loading port (USB-C ideally) and/or SD card for updating the firmware.
+ Incluides two displays that counts seconds and tenths of a second
+ Large format display facing the court that can be read by the players
+ Small format display on the rear that can be read by the timekeeper
+ A rocker switch to select between 25 and 15 second countdown (possibly a 3-position switch to include OFF)
+ A reset button that restored the timers to 25 or 15 seconds
+ A start/stop button or switch (this switch or button should be the most prominent and easy to find tactilely without looking down)
+ A knob or set of buttons to manucally increment or decrement the time for arbitrary adjustments (up to 99 seconds or down to 1, with the 10ths zeroed out after adjusting)
+ Includes a speaker to play a shot-clock-expired tone, like a horn sound, when the clock reaches 0.0

**Considerations for future features**
+ include 2 RJ11 receiver jacks to allow for connection to the Nevco system (eitherinline with the the handheld switch or connected to the controller separately)
+ Include 5 LED "bulbs" on top of the unit to visually indicate the last 5 seconds of the shot clock at a quick glance from the court
+ Wireless capability so that 2 or more of these units can be placed on the wall behind each basket and synchronized from the scorekeeper's unit
+ The speaker could play tones counting down the last 5 seconds as well


### Version 2
The second version should build upon the first.

**Requirements**
+ Optimised user interface based on feedback from using version 1
+ The version 2 requirements include all of the requirements from version 1
+ Added robustness to protect against the unit falling off a table, getting hit by a ball, or getting kicked by a player (the design should also prevent injury to a player that falls onto it, i.e. padded all around)
+ Integrated with the Nevco handheld such that the handheld clock control on/off switch also controls the shot clock
+ The shot clock unit plays the horn through the Nevco system when the shot clock reaches 0.0

 ## Design ideas

 + ESP32-based microcontroller
 + 3d printed housing
