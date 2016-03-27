#include "manchestrate.h"
#include <mystuff.h>


//The code up here is for decoding manchester encoded packets.

#ifdef TABLE_BASED


#include "demanchestrate_table.h"

uint16_t  gl_state1;
uint8_t   gl_in_preamble;
uint16_t  pams;
uint32_t  gl_dataoutword;
uint8_t   gl_dataoutplace;
int16_t   current_packet_rec_place;


void ResetPacketInternal( uint32_t first )
{
	gl_state1 = 0;
	gl_in_preamble = 2;
	current_packet_rec_place = 0;
	gl_dataoutword = 0;
	gl_dataoutplace = 0;
	pams = 0;
}

//Return -1 if terminate search... -2 if good packet.
//Return 0 of bytes read if good.

inline static int8_t Demanchestrate( uint32_t v )
{
	uint16_t  state1 = gl_state1;
	uint8_t   in_preamble = gl_in_preamble;
	int8_t nibble = 28;

	pams++;

	if( in_preamble )
	{
		uint8_t i;
		if( in_preamble == 2 )
		{
			//Deal with the pre-preamble
			uint8_t b0 = v&1; //Most recent bit.
			for( i = 1; i < 32; i++ )
			{
				if( ((v>>i)&1) != b0 ) break;
			}

			if( i > 6 )
			{
				//Faulty data.
				gl_state1 = state1;
				gl_in_preamble = in_preamble;
				return -1;
			}

			state1 = (1<<9) | ((v&1)<<8) | ( (i-1) << 5 );
			gl_state1 = state1;
			gl_in_preamble = 1;
			return 0;
		}
		else
		{
			//In regular preamble.

			if( in_preamble )
			{
				//Wait for preamble to complete.
				for( ; nibble >= 0; nibble-=4 )
				{
					state1 |= (v>>nibble)&0x0f;
					state1 = ManchesterTable1[state1];
					if( (state1 & 0x400) ) { in_preamble = false; nibble-=4; break; }	//Break in the preamble!
					state1 &= 0x3f0;
				}
			}

			gl_in_preamble = in_preamble;

			if( in_preamble )
			{
				gl_state1 = state1;
				return 0;
			}

			//Handle messy logic of going into regular from preamble.
			//XXX TODO: These two lines are INCORRECT
			state1 &= 0x3f0;
			state1 |= 0x200;
		}
	}

	{
		uint32_t dataoutword = gl_dataoutword;
		uint8_t dataoutplace = gl_dataoutplace;
		uint16_t lcl_current_packet_rec_place = current_packet_rec_place;

		//Regular in-packet.
		for( ; nibble >= 0; nibble-=4 )
		{
			state1 |= (v>>nibble)&0x0f;
			state1 = ManchesterTable1[state1];
			dataoutword |= (state1&3)<<dataoutplace;
			dataoutplace += (state1>>2)&3;
			state1 &= 0x3f0;
		}

		while( dataoutplace >= 8 )
		{
			((uint8_t*)current_packet)[lcl_current_packet_rec_place++] = dataoutword;
			dataoutword >>= 8;
			dataoutplace -= 8;
		}

		gl_dataoutword = dataoutword;
		gl_dataoutplace = dataoutplace;
		current_packet_rec_place = lcl_current_packet_rec_place;
		if( (state1 & 0xE0) == 0xE0 )
		{
			gl_state1 = state1;
			return -2;
		}
		gl_state1 = state1;

		return 0;
	}
}

int8_t DecodePacket( uint32_t * dat, uint16_t len )
{
	int r, j;
	for( j = 1; j < len; j++ ) 
	{
		r = HandleWord32( dat[j] );
		if( r != 0 )
			break;
	}

	return r;
}


#else

#include "legacy_manchester.h"


int8_t DecodePacket( uint32_t * dat, uint16_t len )
{
	int r, j;
	for( j = 1; j < len; j++ ) 
	{
		r = HandleWord32( dat[j] );
		if( r != 0 )
			break;
	}
	return r;
}

#endif

int8_t HandleWord32( uint32_t v )
{
	int r = Demanchestrate( v );

	if( r == -2 && current_packet_rec_place > 16 )
	{
		return 1;
	}

	if( r )
	{
		return -6;
	}
	return 0;
}

//The code down here is for sending, Manchester Encoding data.

extern volatile uint8_t i2stxdone;

uint32_t sendDMAbuffer[MAX_FRAMELEN+9+4+4];
uint16_t sendDMAplace;

uint16_t ManchesterTable[16] __attribute__ ((aligned (16))) = {
	0b1100110011001100, 0b0011110011001100, 0b1100001111001100, 0b0011001111001100,
	0b1100110000111100, 0b0011110000111100, 0b1100001100111100, 0b0011001100111100,
	0b1100110011000011, 0b0011110011000011, 0b1100001111000011, 0b0011001111000011,
	0b1100110000110011, 0b0011110000110011, 0b1100001100110011, 0b0011001100110011,
};

void PushManch( unsigned char c )
{
	sendDMAbuffer[sendDMAplace++] = (ManchesterTable[c>>4])|(ManchesterTable[c&0x0f]<<16);
}

void SendPacketData( const unsigned char * c, uint16_t len )
{
	if( len > MAX_FRAMELEN )
	{
		printf( "Sending packet too big.\n" );
		return;
	}
	len*=4;
	int i;
	while(!i2stxdone);

	//For some reason the ESP's DMA engine trashes something in the beginning here, let's set it all to 0 to be safe.
	sendDMAplace = 4;
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0xD5 );
	for( i = 0; i < len; i++ )
	{
		PushManch( c[i] );
	}
	ets_memset( &sendDMAbuffer[sendDMAplace], 0, 4*4 );  //Zero pad the end.
	SendI2SPacket( sendDMAbuffer, sendDMAplace+4 );
}

