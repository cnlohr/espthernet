//Copyright 2014-2016 <>< Charles Lohr, see LICENSE file.  This file may be licensed under the 2-Clause BSD License or the MIT/x11 Licenses.

//This is a modification of the avrcraft IP stack.

#include "net_compat.h"
#include <string.h>
#include "packetmater.h"
#include <i2sduplex.h>
#include <manchestrate.h>
#include <osapi.h>
#include "crc32.h"

unsigned char ETbuffer[RX_BUFFER_END] __attribute__((aligned(32)));

unsigned short ETsendplace;
uint16_t sendbaseaddress;
unsigned short ETchecksum;

uint32_t g_process_paktime;

unsigned char MyMAC[6];
unsigned char MyIP[4] = { 10, 1, 10, 5 };
unsigned char MyMask[4] = { 255, 0, 0, 0 };

//Internal functs

ICACHE_FLASH_ATTR uint16_t internet_checksum( const unsigned char * start, uint16_t len )
{
	uint16_t i;
	const uint16_t * wptr = (uint16_t*) start;
	uint32_t csum = 0;
	for (i=1;i<len;i+=2)
		csum += (uint32_t)(*(wptr++));	
	if( len & 1 )  //See if there's an odd number of bytes?
		csum += *(uint8_t*)wptr;
	if (csum>>16)
		csum = (csum & 0xFFFF)+(csum >> 16);
	csum = (csum>>8) | ((csum&0xff)<<8);
	return ~csum;
}


void ICACHE_FLASH_ATTR et_backend_tick_quick()
{
	int i;

	for( i = 0; i < RXBUFS; i++ )
	{
		if( rx_pack_flags[i] != 2 ) continue;

		uint16_t byr = rx_pack_lens[i];
		uint16_t sendplace = PTR_TO_RX_BUF(i);

		uint32_t cmpcrc = crc32( ETbuffer+sendplace, byr - 4 );

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
#ifdef ALLOW_FRAME_DEBUGGING
			//If we're debugging, be sure to capture this packet as a failure.
			if( KeepNextPacket == 2 )
			{
				KeepNextPacket = 4;
			}
#endif
			printf( "!CRC\n" );
		}

		rx_pack_flags[i] = 0; //Release packet.
	}
}


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

void et_pushpgmblob( const uint8_t * data, uint16_t len )
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

void et_pushblob( const uint8_t * data, uint16_t len )
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

	return 0;
}

void ICACHE_FLASH_ATTR et_endsend()
{
	et_xmitpacket( sendbaseaddress, ETsendplace - sendbaseaddress );
}

int8_t ICACHE_FLASH_ATTR et_xmitpacket( uint16_t start, uint16_t len )
{
	//If we're here, ETbuffer[start] points to the first byte (dst MAC address)
	//Gotta calculate the checksum.

	//First, round up the length and make sure it meets minimum requirements.
	if( len < 60 ) len = 60;
	len = ((len-1) & 0xfffc) + 4; //round up to 4.

	uint8_t  * buffer = &ETbuffer[start];
	uint32_t crc = crc32( buffer, len );
	uint16_t i = len;
	
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

void ICACHE_FLASH_ATTR et_start_checksum( uint16_t start, uint16_t len )
{
	uint16_t i;
	const uint16_t * wptr = (uint16_t*)&ETbuffer[start+sendbaseaddress];
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



void ICACHE_FLASH_ATTR et_copy_memory( uint16_t to, uint16_t from, uint16_t length, uint16_t range_start, uint16_t range_end )
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




