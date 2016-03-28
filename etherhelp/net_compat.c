#include "net_compat.h"
#include <string.h>
#include "packetmater.h"
#include <i2sduplex.h>
#include <manchestrate.h>
#include <osapi.h>

void SendNLP();

unsigned char ETbuffer[ETBUFFERSIZE+RX_BUFFER_SIZE] __attribute__((aligned(32)));

unsigned short ETsendplace;
uint16_t sendbaseaddress;
unsigned short ETchecksum;

uint32_t g_process_paktime;

unsigned char MyMAC[6];
unsigned char MyIP[4] = { 10, 1, 10, 5 };
unsigned char MyMask[4] = { 255, 0, 0, 0 };


//Internal functs

ICACHE_FLASH_ATTR uint32_t crc32b(uint32_t crc, unsigned char *message, int len) {
   int i, j;
   uint32_t mask;
	uint8_t byte;

   i = 0;
//   crc = 0xFFFFFFFF;
	crc = ~crc;
   while (i < len) {
      byte = message[i];            // Get next byte.
      crc = crc ^ byte;
      for (j = 7; j >= 0; j--) {    // Do eight times.
         mask = -(crc & 1);
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
      i = i + 1;
   }
   return ~crc;
}

ICACHE_FLASH_ATTR uint16_t internet_checksum( const unsigned char * start, uint16_t len )
{
	uint16_t i;
	const uint16_t * wptr = (uint16_t*) start;
	uint32_t csum = 0;
	for (i=1;i<len;i+=2)
	{
		csum = csum + (uint32_t)(*(wptr++));	
	}
	if( len & 1 )  //See if there's an odd number of bytes?
	{
		uint8_t * tt = (uint8_t*)wptr;
		csum += *tt;
	}
	while (csum>>16)
		csum = (csum & 0xFFFF)+(csum >> 16);
	csum = (csum>>8) | ((csum&0xff)<<8);
	return ~csum;
}


#if 0
void et_backend_tick_quick()
{
	int i;
	//First, check to see if we already have a kept packet.
	if( KeepNextPacket )
	{
		for( i = 0; i < STOPKT; i++ )
		{
			if( KeepNextPacket && PacketStoreFlags[i] == 3 )
			{
				KeepNextPacket = 0;
			}
		}
	}

	for( i = 0; i < STOPKT; i++ )
	{
		if( PacketStoreFlags[i] == 2 )
		{
			int r;
			int j;
			//Found a complete packet.  Decode it.
			uint32_t * dat = &PacketStore[i*STOPKTSIZE];
			uint16_t len = PacketStoreLength[i];

//#define DEBUG_RAW
#ifdef DEBUG_RAW
			for( j = 0; j < len; j++ )
			{
				uint32_t rr = dat[j];
				for( r = 0; r < 32; r++ )
				{
					printf( (rr & (1<<(31-r)))?"1":"0" );
				}
			}
#endif
			ResetPacketInternal( dat[1] );

			//This secton detects if a new packet is present and runs the demanchester code on it.

			if( KeepNextPacket == 1 )
			{
				ets_intr_lock();
				g_process_paktime = system_get_time();
			}

			r = DecodePacket( &dat[1], len-1 ); //Skip first byte.

			if( KeepNextPacket == 1 )
			{
				g_process_paktime = system_get_time() - g_process_paktime;
				ets_intr_unlock();
			}





			//Careful: If we have a bad packet, and we've been told to keep our hands on the last failed packet this is it!
			if( r < 0 && KeepNextPacket == 2 )
			{
				PacketStoreFlags[i] = 3;
				KeepNextPacket = 0;
			}
			else
			{
				//Tell DMA code we've gotten the packet out of the hands of the DMA engine.
				PacketStoreFlags[i] = (KeepNextPacket==1)?3:0;
				if( KeepNextPacket == 1 )
				{
					KeepNextPacket = 0;
				}
			}

			if( r > 0 )
			{
				//Got a packet!

//#define PRINTPACK
#ifdef PRINTPACK
				int i;
				for( i = 0; i < current_packet_rec_place; i++ )
				{
					printf( "%02x", ((uint8_t*)current_packet)[i] );
				}
				printf( "\n" );
#endif
				int byr = current_packet_rec_place;
				uint32_t cmpcrc = crc32b( 0, ETbuffer, byr - 4 );
				uint32_t checkcrc = (ETbuffer[byr-1] << 24)|(ETbuffer[byr-2] << 16)|(ETbuffer[byr-3] << 8)|(ETbuffer[byr-4]);

				if( cmpcrc == checkcrc )
				{
					//If you ever get to this code, a miracle has happened.
					//Pray that many more continue.
					ETsendplace = 0;
//					printf( "ETCB %d\n", byr );
					et_receivecallback( byr - 4 );
				}
				else
				{
					printf( "CRCERR\n" );
				}
			}
		}
	}
}

#else

void et_backend_tick_quick()
{
	int i;
	for( i = 0; i < RXBUFS; i++ )
	{
		if( rx_pack_flags[i] != 2 ) continue;

		uint16_t byr = rx_pack_lens[i];
		uint16_t sendplace = PTR_TO_RX_BUF(i);

		uint32_t cmpcrc = crc32b( 0, ETbuffer+sendplace, byr - 4 );

		uint32_t checkcrc = (ETbuffer[sendplace+byr-1] << 24)|(ETbuffer[sendplace+byr-2] << 16)|(ETbuffer[sendplace+byr-3] << 8)|(ETbuffer[sendplace+byr-4]);

		if( cmpcrc == checkcrc )
		{
			//If you ever get to this code, a miracle has happened.
			//Pray that many more continue.
			ETsendplace = sendplace;
			et_receivecallback( byr - 4 );
		}
		else
		{
			printf( "CRCERR\n" );
		}

		rx_pack_flags[i] = 0; //Release packet.
	}
}


#endif

ICACHE_FLASH_ATTR void et_backend_tick_slow()
{
	if( gotdma > 5 )
	{
		printf( "I2S Down.  Restarting\n" );	
		testi2s_init();
		gotdma = 0;
		return;
	}
	if( gotlink > 250 )
	{
		printf( "Link Down too much.  Restarting\n" );	
		testi2s_init();
		gotlink = 0;
		return;
	}
	gotlink++;
	gotdma++;
}

static volatile os_timer_t nlp_timer;

#ifdef FULL_DUPLEX_FLP

//Full-duplex link.

//XXX TODO This functionality doesn't actually work.
//It likely has to do with timing inaccuracies, but I don't have
//the equipment available now to determin what's going on.

uint8_t nlpxstate = 0;

const uint8_t linkcodeword[] = { 
	1, 1, //802.3
	1, 0,
	1, 0,
	1, 0,
	1, 0,
	1, 1, //10BaseT
	1, 1, //Full duplex
	1, 0,
	1, 0, 
	1, 0, 
 	1, 0,
	1, 0, 
	1, 0, 
	1, 0, //Remote fault
	1, 1, //Acknowledge
	1, 0, //Next page
 };

void SendNLPX()
{
	uint8_t sendpulse = 0;
	static uint32_t nlpsend[5] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000f };
	if( !i2stxdone ) { nlpxstate = 128; } //well after any more link pulses.

	if( nlpxstate < sizeof( linkcodeword ) )
		if( linkcodeword[nlpxstate] )
			SendI2SPacket( nlpsend, sizeof( nlpsend ) / 4 );
	
	nlpxstate++;
}

ICACHE_FLASH_ATTR void ConfigNLP()
{
	os_timer_disarm(&nlp_timer);
	os_timer_setfn(&nlp_timer, (os_timer_func_t *)SendNLPX, NULL);
	os_timer_arm_us(&nlp_timer, 62, 1);
}

#else

//Send out an NLP pulse. This NLP unfortunately identifies us at 10-base-T, half-duplex.
ICACHE_FLASH_ATTR void SendNLP()
{
	static uint32_t nlpsend[5] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000f };
	if( i2stxdone )
	{
		SendI2SPacket( nlpsend, sizeof( nlpsend ) / 4 );
	}
}

ICACHE_FLASH_ATTR void ConfigNLP()
{
	os_timer_disarm(&nlp_timer);
	os_timer_setfn(&nlp_timer, (os_timer_func_t *)SendNLP, NULL);
	os_timer_arm(&nlp_timer, 16, 1);
}

#endif



//
uint16_t et_pop16()
{
	uint16_t ret = et_pop8();
	return (ret<<8) | et_pop8();
}

void et_popblob( uint8_t * data, uint8_t len )
{
	while( len-- )
	{
		*(data++) = et_pop8();
	}
}

void et_pushpgmstr( const char * msg )
{
	uint8_t r;
	do
	{
		r = pgm_read_byte(msg++);
		if( !r ) break;
		et_push8( r );
	} while( 1 );
}

void et_pushpgmblob( const uint8_t * data, uint8_t len )
{
	while( len-- )
	{
		et_push8( pgm_read_byte(data++) );
	}
}


void et_pushstr( const char * msg )
{
	for( ; *msg; msg++ ) 
		et_push8( *msg );
}

void et_pushblob( const uint8_t * data, uint8_t len )
{
	while( len-- )
	{
		et_push8( *(data++) );
	}
}

void et_push16( uint16_t p )
{
	et_push8( p>>8 );
	et_push8( p&0xff );
}


//return 0 if OK, otherwise nonzero.
int8_t et_init( const unsigned char * macaddy )
{
	MyMAC[0] = macaddy[0]+0x80; //Make sure this MAC is different from the MAC in the ESP.
	MyMAC[1] = macaddy[1];
	MyMAC[2] = macaddy[2];
	MyMAC[3] = macaddy[3];
	MyMAC[4] = macaddy[4];
	MyMAC[5] = macaddy[5];

	testi2s_init();

	ConfigNLP();

	return 0;
}

int8_t et_xmitpacket( uint16_t start, uint16_t len )
{
	//If we're here, ETbuffer[start] points to the first byte (dst MAC address)
	//Gotta calculate the checksum.

	//First, round up the length and make sure it meets minimum requirements.
	if( len < 60 ) len = 60;
	len = ((len-1) & 0xfffc) + 4; //round up to 4.

	uint8_t  * buffer = &ETbuffer[start];
	uint32_t crc = crc32b( 0, buffer, len );
	uint16_t i = start + len;
	
	buffer[i++] = crc & 0xff;
	buffer[i++] = (crc>>8) & 0xff;
	buffer[i++] = (crc>>16) & 0xff;
	buffer[i++] = (crc>>24) & 0xff;

	//Actually emit the packet.
	SendPacketData( buffer, (len>>2) + 1 ); //Don't forget the CRC!

	return 0;
}

unsigned short et_recvpack()
{
	//Stub function.
	return 0;
}

void et_start_checksum( uint16_t start, uint16_t len )
{
	uint16_t i;
	const uint16_t * wptr = (uint16_t*)&ETbuffer[start];
	uint32_t csum = 0;
	for (i=1;i<len;i+=2)
	{
		csum = csum + (uint32_t)(*(wptr++));	
	}
	if( len & 1 )  //See if there's an odd number of bytes?
	{
		uint8_t * tt = (uint8_t*)wptr;
		csum += *tt;
	}
	while (csum>>16)
		csum = (csum & 0xFFFF)+(csum >> 16);
	csum = (csum>>8) | ((csum&0xff)<<8);
	ETchecksum = ~csum;
}



void et_copy_memory( uint16_t to, uint16_t from, uint16_t length, uint16_t range_start, uint16_t range_end )
{
	uint16_t i;
	if( to == from )
	{
		return;
	}
	else if( to < from )
	{
		for( i = 0; i < length; i++ )
		{
			ETbuffer[to++] = ETbuffer[from++];
		}
	}
	else
	{
		to += length;
		from += length;
		for( i = 0; i < length; i++ )
		{
			ETbuffer[to--] = ETbuffer[from--];
		}
	}
}

void et_write_ctrl_reg16( uint8_t addy, uint16_t value )
{
	switch (addy )
	{
		case EERXRDPTL:
		case EEGPWRPTL:
			ETsendplace = value;
		default:
			break;
	}
}

uint16_t et_read_ctrl_reg16( uint8_t addy )
{
	switch( addy )
	{
		case EERXRDPTL:
		case EEGPWRPTL:
			return ETsendplace;
		default:
			return 0;
	}
}



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
