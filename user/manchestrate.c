//Copyright 2015-2016 <>< Charles Lohr, see LICENSE file.  This file is licensed under the 2-Clause BSD License.

#include "manchestrate.h"
#include <mystuff.h>
#include "i2sduplex.h"
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


int8_t   PacketStoreInSitu = 0;
#ifdef ALLOW_FRAME_DEBUGGING
//For storing packets when they come in, so they can be processed in the main loop.
uint32_t PacketStore[STOPKTSIZE];
uint16_t PacketStoreLength; 
int8_t   KeepNextPacket = 0;
#endif
extern uint32_t g_process_paktime;



//This is called before manchester coded data is shifted into DecodePacket.
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


//Returns 0 if packet was exhausted before finding an end-of-packet marker.
//Returns n if packet contained invalid codes (like an end of packet) within this function.
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
					in_preamble = 0;
					nibble-=4;
					break;
				}
				state1 &= 0x3f0;
			}

			if( !in_preamble )
			{
				//When we get here, we expect to have gotten two shorts... but, it could be three TOTAL.
				//If it's two, all is well.. if it's 3... we have to be a little more careful.
				//We can see if it was 3 by seeing if intonation in was set.  If it was, then we know
				//We're on the 3rd.
				if( ( state1 & 0x800 ) )  //Indicates two bits were just outputted in the last nibble.
				{
					dataoutplace = 1;
					dataoutword = 0;
					state1 &=~(1<<9);
				}
				else
				{
					//Only had 2 values sent, always set polarity to high.
					//Set 'polarity'
					state1 |= 0x200;
				}

				state1 &= 0x3f0;

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
		current_packet_rec_place = cpr - (uint8_t*)current_packet;
		gl_state1 = state1;
		return 0;
	}

}

//Process a bundle from the i2s engine. 
//
//This is called from within the i2s interrupt. 
void	GotNewI2SData( uint32_t * dat, int datlen )
{
	int i = 0;
	int r;
	static int stripe = 0;

	gotdma=1;

keep_going:


	//Search for until data is ffffffff or 00000000.
	//This would be if we think we hit the end of a packet or a bad packet.
	//Just speed along till the bad dream is over.
	if( PacketStoreInSitu < 0 )
	{
		for( ; i < datlen; i++ )
		{
			uint32_t d = dat[i];

			//Look for a set of 3 non null packets.
			if( d != 0xffffffff && d != 0x00000000 )
			{
				PacketStoreInSitu = 0;
				gotlink = 1;
				stripe = 1;
				break;
			}
		}

		//Still searching?  If so come back in here next time.
		if( PacketStoreInSitu ) return;
	}

	//Otherwise we're looking for several non-zero packets in a row.
	//This would indicate a start of a packet.
	if( PacketStoreInSitu == 0 )
	{
		//Quescent state.
		for( ; i < datlen; i+=2 )
		{
			uint32_t d = dat[i];

			//Look for a set of 3 non null packets.
			if( d != 0xffffffff && d != 0x00000000 )
			{
				gotlink = 1;
				stripe++;
				if( stripe == 3 )
				{
					PacketStoreInSitu = 1;
					break;
				}
			}
			else
			{
				stripe = 0;
			}
		}

		//Nothing interesting happened all packet.  Return back to the host.
		if( !PacketStoreInSitu )
			return;

		stripe = 0;
		//Something happened... 

		r = ResetPacketInternal(1);

		//Make sure we can get a free packet.
		if( r < 0 )
		{
			//Otherwise, we have to dump this on-wire packet.
			PacketStoreInSitu = -1;
			goto keep_going;
		}

		//Good to go and start processing data.

#ifdef ALLOW_FRAME_DEBUGGING
		if( KeepNextPacket > 0 && KeepNextPacket < 3 )
		{
			PacketStoreLength = 0;
			g_process_paktime = 0;
		}
#endif
		//We can safely skip a free word since preambles are so long.
		//Do this for performance reasons.
		i ++;
	}

#ifdef ALLOW_FRAME_DEBUGGING
	if( KeepNextPacket > 0 && KeepNextPacket < 3 )
	{
		int start;
		if( i != 0 )
		{
			//Starting a packet.	
			start = i-(4);
			if( start < 0 ) start = 0;
		}
		else
		{
			//Middle of packet
			start = i;
		}

		int k;
		for( k = start; k < datlen && PacketStoreLength < STOPKTSIZE; k++ )
		{
			PacketStore[PacketStoreLength++] = dat[k];
		}

		g_process_paktime -= system_get_time();
	}
#endif

	//Start processing a packet.
	if( i < datlen )
	{
		r = DecodePacket( &dat[i], datlen - i );
	}
	i += r;

#ifdef ALLOW_FRAME_DEBUGGING
	if( KeepNextPacket > 0 && KeepNextPacket < 3 )
	{
		g_process_paktime += system_get_time();
	}
#endif

	//Done with this segment, next one to come.
	if( r == 0 )
	{
		gotlink = 1;
		return;
	}

	//Packet is complete, or error in packet.  No matter what, we have to finish off the packet next time.
	PacketStoreInSitu = -1;

#ifdef ALLOW_FRAME_DEBUGGING
	if( KeepNextPacket > 0 && KeepNextPacket < 3 )
	{
		int trim = (datlen-i);
		trim--;

		if( trim < 0 ) trim = 0;
		if( trim > PacketStoreLength - 10 ) trim = (PacketStoreLength-10);
		PacketStoreLength -= trim;

		//Only release if packet not waiting for clear.
		if( KeepNextPacket == 1 ) KeepNextPacket = 4;

		//Packet knowingly faulted and we were waiting for packet?
		if( KeepNextPacket == 2 && rx_pack_flags[rx_cur] == 0 ) KeepNextPacket = 4;
	}
#endif

	goto keep_going;
}




///////////////////////////////////////////////////////////////////////////////
//// Sending (Manchestration) Portion
///////////////////////////////////////////////////////////////////////////////






//The code down here is for sending, Manchester Encoding data.
extern volatile uint8_t i2stxdone;

uint32_t sendDMAbuffer[MAX_FRAMELEN+8+6+6] __attribute__ ((aligned (16)));
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

	//For some reason the ESP's DMA engine trashes something in the beginning here, Don't send the preamble until after the first 128 bits.

	*(sDMA++) = 0x00;
	*(sDMA++) = 0x00;
	*(sDMA++) = 0x00;
	*(sDMA++) = 0x00;

	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0x55 );
	PushManch( 0xD5 );

	while(!i2stxdone);

	const unsigned char * endc = c + len;
	while( c != endc )
	{
		char g = *(c++);
		PushManch( g );
	}

	//Ok, this last part seems super tricky.
	//If we're hooked up to magnetics through a line driver, it should be  0x00000000
	//If we're hooked up directly, should be 0xffffffff
	//This appeared to be a good compromise.
	*(sDMA++) = 0xfff00000;
	*(sDMA++) = 0x00;
	*(sDMA++) = 0x00;
	*(sDMA++) = 0x00;

	SendI2SPacket( sendDMAbuffer, sDMA - sendDMAbuffer );
}



