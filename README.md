#espthernet

*It's pronounced "e-s-peethernet" - a party trick, not a legitimate engineering solution.*

ESP8266 I2S + Software-based 10-Base-T Ethernet Driver.  

## Hardware

Option 1 (The almost right)


## Overall Discussion
This product was created by Charles Lohr, but, will likely fall out of support by original author quickly.  Additional contributers would be welcome.

espthernet, though designed for the ESP8266 should be relatively portable, as most of the code is limited to [manchestrate.c](user/manchestrate.c) and related files.  Since it is designed to operate at full duplex, it is expecting a 40MHz-in, and 40MHz-out stream. 

### Raw stream in/stream out

The system

### (De)manchestration



## Future Work
 * Switch to half-duplex option, or separate streams.  By using 20 MHz TX, and 32-36 MHz RX, you can save a lot of time since you'd have to process less data, and remove the requirement for signal bias. 
 * Add Hook to internal IP stack of whatever is being used.
 * Use FLPs instead of NLPs to negotiate for 10BaseT-FD instead of -HD  (Stack handles full duplex comms, no reason not to comm at it)
 * Use a pseudo-PLL for decoding packets rather than a transition finder.  It may be better?
