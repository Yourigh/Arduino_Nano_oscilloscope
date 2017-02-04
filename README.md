# Arduino_Nano_oscilloscope
Oscilloscope for Arduino nano, using 128x64 display tailoring ug8 library for graphics. Edited code from semifluid.com

#Source
THe source code was taken from this article and updated for Arduino nano V3 board with ATMega 328 on board. 
http://www.semifluid.com/2013/05/28/arduino-fio-lcd-oscilloscope/

#Adjustments
Voltage ranges were adjusted from 3.3V to 5V. Sampling was off for Arduino nano, this board is capable of sampling frequency 65 kHz! 3 buttons were added. Two for setting the sampling (increase and decrease) and one button for changing the range.

Adding a new variable caused instability and design didn't work well anymore. Memory space is maxed out. There is still lot of room for program.

#Application specific adjustments
You need to change declaration for your display and its connection. See ug8 documentation.

#Compatibility
This code should work on AtMega 328 based boards Uno and Nano. There will be no problem to run this on Arduino MEGA. Leave a comment from your testing.

#Video
comming soon...

#Credits
original code (not for Arduino Nano)
http://www.semifluid.com/2013/05/28/arduino-fio-lcd-oscilloscope/
