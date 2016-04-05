//Copyright 2015-2016 <>< Charles Lohr, see LICENSE file.  This file is licensed under the 2-Clause BSD License.

//This file is based off of the i2sduplex in https://github.com/cnlohr/esp8266duplexi2s

#ifndef _I2SDUPLEX_TEST
#define _I2SDUPLEX_TEST


//Stuff that should be for the header:

#include <eth_config.h>
#include <c_types.h>

//General notes:
// Operate at 40 MHz bus speed.
// Bias down toward 0!  You cannot have a symmetric signal, otherwise you will get ambiguous states.
// 40 MHz allows us to operate at full duplex.

#define DMABUFFERDEPTH 3
#define I2SDMABUFLEN (256)
#define RX_NUM (I2SDMABUFLEN)
#define LINE32LEN I2SDMABUFLEN

#define I2STXZERO 128

extern uint32_t i2sBDRX[I2SDMABUFLEN*DMABUFFERDEPTH];

extern volatile uint8_t i2stxdone;
extern volatile uint32_t last_unknown_int;

extern int fxcycle;
extern int erx, etx;

void ICACHE_FLASH_ATTR testi2s_init();

void ICACHE_FLASH_ATTR SendI2SPacket( uint32_t * pak, uint16_t dwords );

//You, as the user must provide, this will be called from an ISR, so be careful not to take long!
void	GotNewI2SData( uint32_t * dat, int datlen );


//For storing packets when they come in, so they can be processed in the main loop.

/*
//By having two packets, we can be receiving one packet while the user is processing another.
//This is also useful because if the user wants to hold onto a packet for a while, we can still keep working.
#define STOPKT 2

//# of bytes for MTU + Preamble + CRC, * 8 = bits * 4 = samples / 32 = words.  Needs to be a little bigger, though.  Preamble and all.
#define STOPKTSIZE (MAX_FRAMELEN+15)  
extern uint32_t PacketStore[STOPKTSIZE*STOPKT];
extern uint8_t  PacketStoreFlags[STOPKT]; //0 = free, 1 = in progress, 2 = good to go (to be received and processed), 3 = currently in some custom process.
extern uint16_t PacketStoreLength[STOPKT]; 
extern int8_t   PacketStoreInSitu; //-1 = unassociated, -2 = faulted on this packet, + or 0 = in this packet.
*/

//Stored packets, for debugging.

#ifdef ALLOW_FRAME_DEBUGGING
#define STOPKTSIZE (MAX_FRAMELEN+15)  
extern uint32_t PacketStore[STOPKTSIZE];
extern uint16_t PacketStoreLength; 
extern int8_t   KeepNextPacket;
#endif

extern uint8_t gotdma;
extern uint8_t gotlink;

void StopI2S();
void StartI2S();


#endif

