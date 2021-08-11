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
