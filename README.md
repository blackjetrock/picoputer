# picoputer
Raspberry Pi Pico Transputer Emulator

This is a port of Julian Highfield's transputer emulator (via https://github.com/pahihu/t4). the emulator runs on a raspberry Pi Pico and talks down links that are attached to PIO hardware. The PIOs run code that uses the 10MHz transputer link protocol. In the original setup these links run to an INMOS IMSC011 which is attached to an Arduino Mega. The Mage runs a (very inefficient) protocol that communicates over USB with a host.

The Arduino runs the program ard-link-monitor.ino
The host PC runs the program ardlink-server

This code is very messy, full of warnings and probably bugs. I have had it run some simple occam and the first few stages of the Minix demo. It can calculate primes as well.

I am having to compile Occam using the geekdot transputer SDK (http://www.geekdot.com/category/software/transputer-software/) using a virtual floppy file to transfer files in and out of the VM.

I haven't managed to get the TDS3 to run on the t4 emulator and do anything useful, but I think it sould, and that would be a better way to compile Occam I think.

The Arduino/IMSC011 circuit needs a POB, if you hand wir eit then you just need to use the GPIO settings in the cod eto work out the wiring. I have also used level shifters between the IMSC011 and the Pico as they run at different voltages. As I used transistors the PIO code inverts the signalsso the IMSC011 gets the correct polarity.

The PIO code treats the ACK packet as an 11 bit packet like the data packets, this isn't right and could do with fixing. It seems to work, though, probably because my protocol is so inefficient.

You could probably bypass all the IMSC011 stuff and attach the server program to the Pico over USB and load code that way. I wanted to be able to attach real transputer hardware as well, though.

You could use the PIO link code and program the Pico natively in C if you wanted the full speed of the Pico, but I wanted to program in Occam.

Once more:  This code is very messy, full of warnings and probably bugs.

picoputer and picoputer-eclipse are the directories that build the picoputer code in eclipse. They need to be at the same level as the RP Pico SDK and examples directories for them to build correctly.


ard-link-monitor.ino
--------------------

This program is for an Arduino Mega. I used a Mega embedded, but that was because i had one and it fits nicely on 0.1" perfboard. It's a standard Arduino program so should run on any Mega.


ardlink-server
--------------

This is a standard C program that at the moment is only capable of sending the boot file and responding to a subset of SP protocol commands. (Just enough to run the examples).

To build it:

./m


Example command line:
sudo ./ardlink-server -d /dev/ttyUSB0 -b SIMPLE.BTL

This sends the SIMPLE.BTL boot file to the picoputer which will then run it.  SIMPLE.BTL is the compiled simple.occ program. I compile dthis using the geekdot SDK.





