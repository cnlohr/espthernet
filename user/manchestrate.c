#include "manchestrate.h"
#include <mystuff.h>
#include "c_types.h"

//The code up here is for decoding manchester encoded packets.

#include "demanchestrate_table.h"

uint16_t  gl_state1;
uint8_t   gl_in_preamble;
uint32_t  gl_dataoutword;
uint8_t   gl_dataoutplace;
int16_t   current_packet_rec_place;
uint8_t * current_packet;

uint8_t  rx_cur;
//uint8_t  rx_pack_data[RXBUFS*MAX_FRAMELEN];
uint16_t rx_pack_lens[RXBUFS];
uint8_t  rx_pack_flags[RXBUFS];

int ResetPacketInternal()
{
	int i;

	if( rx_pack_flags[rx_cur] != 2 )
	{
		rx_pack_flags[rx_cur] = 0;
	}

	for( i = 0; i < RXBUFS; i++ )
	{ 
		if( rx_pack_flags[i] == 0 ) break;
	}

	if( i == RXBUFS ) return -1;

	rx_pack_flags[i] = 1;
 	current_packet = &ETbuffer[PTR_TO_RX_BUF(i)];
	rx_cur = i;

	gl_state1 = 0;
	gl_in_preamble = 2;
	current_packet_rec_place = 0;
	gl_dataoutword = 0;
	gl_dataoutplace = 0;

	return i;
}



//Return -6 if terminate search... end of packet: 1
//Return 0 of bytes read if good.

//inline static int8_t Demanchestrate( uint32_t v )
int32_t DecodePacket( uint32_t * dat, uint16_t len )
{
	//XXX TRICKY: GCC does an extra operation to downconvert to bits when operating with this value.  It is safe to leave as a 32-bit value.
	//This applies to "nibble" and  "dataoutplace"

	uint16_t  state1 = gl_state1;
	uint8_t   in_preamble = gl_in_preamble;
	int32_t nibble = 28;
	uint32_t v;
	uint32_t k = 0;

	uint32_t dataoutword = gl_dataoutword;
	uint32_t dataoutplace = gl_dataoutplace; 

	if( in_preamble == 2 )
	{
		v = dat[0];

		//Deal with the pre-preamble
		uint8_t b0 = v&1; //Most recent bit.
		//Not actually nibble, re-pruposed.
		for( nibble = 1; nibble < 32; nibble++ )
		{
			if( ((v>>nibble)&1) != b0 ) break;
		}

		if( nibble > 6 )
		{
			//Faulty data.
			gl_state1 = state1;
			gl_in_preamble = in_preamble;
			rx_pack_flags[rx_cur] = 0;
			return 0;
		}

		state1 = (1<<9) | ((v&1)<<8) | ( (nibble-1) << 5 );
		in_preamble = 1;
		nibble = 28;
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
				//XXX TODO: It is possible (likely) this line may be insufficient.
				state1 |= 0x200;
				break;
			}

			nibble = 28;
		}


		//Didn't leave preamble, but no more data left.
		if( k == len )
		{
			gl_in_preamble = in_preamble;
			gl_state1 = state1;
			return 0;
		}



#define nibblet( x )\
				state1 |= (v>>x)&0x0f;\
				state1 = ManchesterTable1[state1];\
				dataoutword |= (state1&3)<<dataoutplace;\
				dataoutplace += (state1>>2)&3;\
				state1 &= 0x3f0;\

		//Since we hit the end of the preamble, we have to finish out the
		//32-bit word...  We do that here.

		//Neat trick: By dividing by 4 then switching based off that, saves 107 bytes of code space!
		nibble>>=2;
		switch( nibble )
		{
		case 7:nibblet(28);
		case 6:nibblet(24);
		case 5:nibblet(20);
		case 4:nibblet(16);
		case 3:nibblet(12);
		case 2:nibblet(8);
		case 1:nibblet(4);
		case 0:nibblet(0);
		}

		k++;
	}

	gl_in_preamble = in_preamble;


	//Not in premable, this does actual packet decoding.
	{
		//uint16_t lcl_current_packet_rec_place = current_packet_rec_place;

		uint8_t * cpr = &((uint8_t*)current_packet)[current_packet_rec_place];
		uint8_t * cprm = &((uint8_t*)current_packet)[MAX_FRAMELEN];

		for( ; k < len; k++ )
		{
			v = dat[k];

			nibblet(28);
			nibblet(24);
			nibblet(20);
			nibblet(16);
			nibblet(12);
			nibblet(8);
			nibblet(4);
			nibblet(0);

			//It is possible that we might be a byte behind at some point.  That is okay.  We'll do another check before being done.
			if( dataoutplace >= 8 )
			{
				if( cpr >= cprm ) { rx_pack_flags[rx_cur] = 0; return k+1; }
				*cpr++ = dataoutword;
				dataoutword >>= 8;
				dataoutplace -= 8;
			}

			//End of data (may be good or bad)
			if( (state1 & 0xE0) == 0xE0 )
			{
				gl_dataoutword = dataoutword;
				gl_dataoutplace = dataoutplace;
				current_packet_rec_place = cpr - (uint8_t*)current_packet;
				gl_state1 = state1;

				//If we're a byte behind, take it out here.
				//Does this need to be a while????
				while( dataoutplace >= 8 )
				{
					if( cpr >= cprm ) { rx_pack_flags[rx_cur] = 0; return k+1; }
					*cpr++ = dataoutword;
					dataoutword >>= 8;
					dataoutplace -= 8;
				}

				rx_pack_lens[rx_cur] = current_packet_rec_place;


				//I've tried wording this different ways, always produces more GCC code.
				if( current_packet_rec_place > 9 )
				{
					//Probs a good packet?
					rx_pack_flags[rx_cur] = 2;
				}
				else
				{
					//Super runt packet (def bad)
					rx_pack_flags[rx_cur] = 0;
				}
				return k+1;
			}
		}

		//More data is to come.
		gl_dataoutword = dataoutword;
		gl_dataoutplace = dataoutplace;
//		current_packet_rec_place = lcl_current_packet_rec_place;
		current_packet_rec_place = cpr - (uint8_t*)current_packet;
		gl_state1 = state1;
		return 0;
	}

}


#define TURBOSEND

#ifdef TURBOSEND

//The code down here is for sending, Manchester Encoding data.
extern volatile uint8_t i2stxdone;

uint32_t sendDMAbuffer[MAX_FRAMELEN+8+4+4] __attribute__ ((aligned (16)));
uint32_t * sDMA;

static const uint16_t ManchesterTable[16] __attribute__ ((aligned (16))) = {
	0b1100110011001100, 0b0011110011001100, 0b1100001111001100, 0b0011001111001100,
	0b1100110000111100, 0b0011110000111100, 0b1100001100111100, 0b0011001100111100,
	0b1100110011000011, 0b0011110011000011, 0b1100001111000011, 0b0011001111000011,
	0b1100110000110011, 0b0011110000110011, 0b1100001100110011, 0b0011001100110011,
};

//#define PushManch( k )	{ *(sDMA++) = (ManchesterTable[(k)>>4])|(ManchesterTable[(k)&0x0f]<<16); }
void PushManch( unsigned char k ) {	*(sDMA++) = (ManchesterTable[(k)>>4])|(ManchesterTable[(k)&0x0f]<<16); }

void ICACHE_FLASH_ATTR SendPacketData( const unsigned char * c, uint16_t len )
{

	if( len > MAX_FRAMELEN )
	{
		printf( "Sending packet too big.\n" );
		return;
	}

	sDMA = &sendDMAbuffer[0];

	len*=4;


	*(sDMA++) = 0;
	*(sDMA++) = 0;
	*(sDMA++) = 0;
	*(sDMA++) = 0;
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0xD5 );

	while(!i2stxdone);

	//For some reason the ESP's DMA engine trashes something in the beginning here, Don't send the preamble until after the first 128 bits.


	const unsigned char * endc = c + len;
	while( c != endc )
	{
		char g = *(c++);
		PushManch( g );
	}
	*(sDMA++) = 0;
	*(sDMA++) = 0;
	*(sDMA++) = 0;
	*(sDMA++) = 0;

	SendI2SPacket( sendDMAbuffer, sDMA - sendDMAbuffer );
}


#else

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

void ICACHE_FLASH_ATTR PushManch( unsigned char c )
{
	sendDMAbuffer[sendDMAplace++] = (ManchesterTable[c>>4])|(ManchesterTable[c&0x0f]<<16);
}

void ICACHE_FLASH_ATTR SendPacketData( const unsigned char * c, uint16_t len )
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

#endif
