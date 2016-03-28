#ifndef _ETH_CONFIG_H
#define _ETH_CONFIG_H

#include <mystuff.h>

#define MAX_FRAMELEN (ETBUFFERSIZE)

#define ETBUFFERSIZE (600) //576 is the IP minimum MTU
#define TX_SCRATCHES 1
#define RX_BUFFER_START 0
#define RX_BUFFER_END   MAX_FRAMELEN

//ESP middleground
#define pgm_read_byte( x ) (*((uint8_t*)(x)))

#define INCLUDE_UDP

#define ALLOW_FRAME_DEBUGGING


#define RXBUFS 2
#define PTR_TO_RX_BUF( x ) ( ETBUFFERSIZE + x * MAX_FRAMELEN )
#define RX_BUFFER_SIZE (RXBUFS*MAX_FRAMELEN)
extern unsigned char ETbuffer[ETBUFFERSIZE+RX_BUFFER_SIZE] __attribute__((aligned(32)));


#endif

/*
     Modification of "avrcraft" IP Stack.
    Copyright (C) 2014 <>< Charles Lohr
		CRC Code from: http://www.hackersdelight.org/hdcodetxt/crc.c.txt

    Permission is hereby granted, free of charge, to any person obtaining a
	copy of this software and associated documentation files (the "Software"),
	to deal in the Software without restriction, including without limitation
	the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
