#include "manchestrate.h"
#include <mystuff.h>


//The code up here is for decoding manchester encoded packets.

#ifdef TABLE_BASED


#include "demanchestrate_table.h"

uint16_t  gl_state1;
uint8_t   gl_in_preamble;
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
}


#if 1

//Return -6 if terminate search... end of packet: 1
//Return 0 of bytes read if good.

//inline static int8_t Demanchestrate( uint32_t v )
int8_t DecodePacket( uint32_t * dat, uint16_t len )
{
	uint16_t  state1 = gl_state1;
	uint8_t   in_preamble = gl_in_preamble;
	int8_t nibble = 28;
	uint32_t v;
	uint16_t k = 0;
	uint8_t i;

	if( in_preamble == 2 )
	{
		v = dat[k];

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
			return -6;
		}

		state1 = (1<<9) | ((v&1)<<8) | ( (i-1) << 5 );
		in_preamble = 1;
		k = 1;
	}

	if( in_preamble == 1 )
	{
		for( ; k < len; k++ )
		{
			v = dat[k];
			//Wait for preamble to complete.
			for( ; nibble >= 0; nibble-=4 )
			{
				state1 |= (v>>nibble)&0x0f;
				state1 = ManchesterTable1[state1];

				if( (state1 & 0x400) )  //Break in the preamble!
				{
					state1 &= 0x3f0;
					in_preamble = 0;
					nibble-=4;
					break;
				}
				state1 &= 0x3f0;
			}

			if( !in_preamble )
			{
				//Handle messy logic of going into regular from preamble.
				//XXX TODO: It is possible this line may be insufficient.
				state1 |= 0x200;
				break;
			}

			nibble = 28;
		}

		if( k == len )
		{
			gl_in_preamble = in_preamble;
			gl_state1 = state1;
			return 0;
		}
	}

	gl_in_preamble = in_preamble;

	//Not in premable, this does actual packet decoding.
	{
		uint32_t dataoutword = gl_dataoutword;
		uint8_t dataoutplace = gl_dataoutplace;
		uint16_t lcl_current_packet_rec_place = current_packet_rec_place;

		for( ; k < len; k++ )
		{
			v = dat[k];

#define nibblet( x )\
				state1 |= (v>>x)&0x0f;\
				state1 = ManchesterTable1[state1];\
				dataoutword |= (state1&3)<<dataoutplace;\
				dataoutplace += (state1>>2)&3;\
				state1 &= 0x3f0;\

			switch( nibble )
			{
			case 28:nibblet(28);
			case 24:nibblet(24);
			case 20:nibblet(20);
			case 16:nibblet(16);
			case 12:nibblet(12);
			case 8:nibblet(8);
			case 4:nibblet(4);
			case 0:nibblet(0);
			}

/*			//Regular in-packet.
			for( ; nibble >= 0; nibble-=4 )
			{
				state1 |= (v>>nibble)&0x0f;
				state1 = ManchesterTable1[state1];
				dataoutword |= (state1&3)<<dataoutplace;
				dataoutplace += (state1>>2)&3;
				state1 &= 0x3f0;
			}*/

			while( dataoutplace >= 8 )
			{
				((uint8_t*)current_packet)[lcl_current_packet_rec_place++] = dataoutword;
				dataoutword >>= 8;
				dataoutplace -= 8;
			}

			if( (state1 & 0xE0) == 0xE0 )
			{
				gl_dataoutword = dataoutword;
				gl_dataoutplace = dataoutplace;
				current_packet_rec_place = lcl_current_packet_rec_place;
				gl_state1 = state1;
				return 1;
			}

			nibble = 28;
		}

		gl_dataoutword = dataoutword;
		gl_dataoutplace = dataoutplace;
		current_packet_rec_place = lcl_current_packet_rec_place;
		gl_state1 = state1;
		return 0;
	}

}
#endif

#else

#include "legacy_manchester.h"

#endif




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

