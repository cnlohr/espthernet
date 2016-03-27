//This is the "legacy" or code-based demanchester function.  It is much, much slower, however, it does not require use of a buffer.

//This file is included in "manchestrate.c"

extern uint8_t manch_last_state;
extern int8_t manch_last_bits_at_state;


#define RECPLACE_NOTHING  -5
#define RECPLACE_STATE    -4
#define RECPLACE_PREAMB   -3
#define RECPLACE_IN_PREAMB -2
extern int16_t current_packet_rec_place;



#ifdef ZERO_BIAS
	#define LAST_ZERO_SHORT 3
	#define LAST_ZERO_LONG  7
	#define LAST_ONE_SHORT 2
	#define LAST_ONE_LONG 7
	#define BITTABLE { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, }
#else
	#define LAST_ZERO_SHORT 2
	#define LAST_ZERO_LONG  7
	#define LAST_ONE_SHORT 3
	#define LAST_ONE_LONG 7
	#define BITTABLE { 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, }
#endif


int16_t current_packet_rec_place = RECPLACE_NOTHING;


uint8_t manch_last_state = 0;
int8_t manch_last_bits_at_state = 0;

uint8_t bytebuilder = 0;
uint8_t bytebuilderpos = 0;
uint8_t lastbit = 0;
uint8_t shortflag = 0;

uint32_t packsrxraw;

uint16_t ct0;
uint16_t ct1;

#define __TEST__

void ResetPacketInternal( uint32_t first )
{
	current_packet_rec_place = RECPLACE_PREAMB;
	manch_last_state = first>>31;
	manch_last_bits_at_state = 0;
}

//This is run for EVERY SINGLE bit transitions.  This code can probably be optimized heavily.  Should probably do that.
inline static int HandleTransition( uint8_t is_long )
{
	//use: manch_last_state, manch_last_bits_at_state, current_packet_rec_place

#ifdef __TEST__
//printf( "Transition: %d/%d %d SF: %d\n", is_long, current_packet_rec_place, RECPLACE_PREAMB, shortflag );
#endif

	if( current_packet_rec_place < 0 )
	{
		if( current_packet_rec_place == RECPLACE_PREAMB )
		{
			current_packet_rec_place = RECPLACE_IN_PREAMB;
			return 0;
		}
		if( current_packet_rec_place == RECPLACE_IN_PREAMB )
		{
			if( !is_long )
				current_packet_rec_place++;

			return 0;
		}
		else
		{
			if( is_long )
			{
	#ifdef __TEST__
				printf( "Got long out of place\n" );
	#endif
				return -1;
			}
			current_packet_rec_place++;
			lastbit = 1;
			shortflag = 0;
			bytebuilder = 0;
			bytebuilderpos = 0;
			ct0 = 0;
			ct1 = 0;
			return 0;
		}
	}

	uint8_t mark_emit = 0;

	if( is_long )
	{
		if( shortflag )
		{
#ifdef __TEST__
			printf( "Short intonation wrong\n" );
#endif

			return -2;
		}
		//invert bit.
		lastbit = !lastbit;

		mark_emit = 1;
	}
	else
	{
		if( shortflag )
		{
			mark_emit = 1;
			shortflag = 0;
		}
		else
			shortflag = 1;
	}

	if( mark_emit )
	{
		if( lastbit )
			bytebuilder |= (1<<bytebuilderpos);
		bytebuilderpos++;
		if( bytebuilderpos == 8 )
		{
			((uint8_t*)current_packet)[current_packet_rec_place++] = bytebuilder;
			bytebuilder = 0;
			bytebuilderpos = 0;
		}
	}

	return 0;
}

uint8_t bittable[] = BITTABLE;

//Return -1 if terminate search, -2 if good packet?
//Return 0 of bytes read if good.
inline static int8_t Demanchestrate( uint32_t v )
{
	int i = 0;

	for( i = 31; i >= 0; i-- )
	{
		uint8_t bit = (v>>i)&1;
		if( bit != manch_last_state )
		{
			if( manch_last_bits_at_state > 7 ) return 1;
//			if( bit ) ct1+=manch_last_bits_at_state; else ct0+=manch_last_bits_at_state;

#ifdef BITTABLE
			uint8_t l = bittable[manch_last_state * 8 + manch_last_bits_at_state];
			if( HandleTransition( l ) < 0 ) return 1;
#else
			if( manch_last_state )
			{
				if( HandleTransition( l > LAST_ONE_SHORT ) < 0 ) return 1;
			}
			else
			{
				if( HandleTransition( l > LAST_ZERO_SHORT ) < 0 ) return 1;
			}
#endif

			manch_last_state = bit;
			manch_last_bits_at_state = 1;
		}
		else
		{
			manch_last_bits_at_state++;
		}
	}

	if( manch_last_bits_at_state > 7 ) return 1;

	return 0;
}

int8_t DecodePacket( uint32_t * dat, uint16_t len )
{
	int r, j;
	for( j = 0; j < len; j++ ) 
	{
		r = Demanchestrate( dat[j] );
		if( r != 0 )
			break;
	}
	return r;
}

