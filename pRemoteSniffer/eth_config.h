#ifndef _ETH_CONFIG_H
#define _ETH_CONFIG_H

#include <mystuff.h>

#define MAX_FRAMELEN (ETBUFFERSIZE)

#define ETBUFFERSIZE (1548+4) 
//576 is the IP minimum MTU
//1536 is the Ethernet MTU, +4 is for CRCs

#define TX_SCRATCHES 1  //Cannot be anything else!!!

#define TX_SCRATCHPAD_END MAX_FRAMELEN
#define TCP_BUFFERSIZE MAX_FRAMELEN
#define RX_BUFFER_START (MAX_FRAMELEN+TCP_BUFFERS)
#define RX_BUFFER_END   (MAX_FRAMELEN+TCP_BUFFERS+RX_BUFFER_SIZE)

//ESP middleground
#define pgm_read_byte( x ) (*((uint8_t*)(x)))

#define INCLUDE_UDP

//#define ALLOW_FRAME_DEBUGGING

#define INCLUDE_TCP
#define TCP_SOCKETS 5
#define TCP_BUFFERS ((TCP_SOCKETS-1)*MAX_FRAMELEN)

#define TCP_TICKS_BEFORE_RESEND 9
#define TCP_MAX_RETRIES 80

#define RXBUFS 2
#define PTR_TO_RX_BUF( x ) ( ETBUFFERSIZE + TCP_BUFFERS + (x * MAX_FRAMELEN) )
#define RX_BUFFER_SIZE (RXBUFS*MAX_FRAMELEN)
extern unsigned char ETbuffer[RX_BUFFER_END] __attribute__((aligned(32)));

#define TABLE_CRC

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
