//ESP8266 I2S Input+Output

#include "slc_register.h"
#include "mystuff.h"
#include <c_types.h>
#include "user_interface.h"
#include "pin_mux_register.h"
#include <dmastuff.h>
#include "i2sduplex.h"
#include "manchestrate.h"

//These contol the speed at which the bus comms.
#define WS_I2S_BCK 2  //Can't be less than 2, if you want to RX
#define WS_I2S_DIV 1

//I2S DMA buffer descriptors
static struct sdio_queue i2sBufDescRX[DMABUFFERDEPTH];
static struct sdio_queue i2sBufDescTX[4];
static struct sdio_queue i2sBufDescNLP;
static uint32_t I2SNLP[4] = { 0x00000000, 0x00000000, 0x0000001f, 0x00000000 };

uint32_t i2sBDRX[I2SDMABUFLEN*DMABUFFERDEPTH];
uint32_t i2sBDTX[I2STXZERO];

uint8_t gotdma;
uint8_t gotlink;

extern uint32_t g_process_paktime;

volatile uint32_t last_unknown_int;
int erx, etx;

volatile uint32_t tx_link_address;
volatile uint8_t i2stxdone;


#ifdef ALLOW_FRAME_DEBUGGING
//For storing packets when they come in, so they can be processed in the main loop.
uint32_t PacketStore[STOPKTSIZE];
uint16_t PacketStoreLength; 
int8_t   KeepNextPacket = 0;
#endif
int8_t   PacketStoreInSitu = 0;


static void	GotNewData( uint32_t * dat, int datlen );

void KickRX()
{
	testi2s_init();
}

void StartI2S()
{
	SET_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
	SET_PERI_REG_MASK(SLC_RX_LINK, SLC_RXLINK_START);
}

void StopI2S()
{
	CLEAR_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
	CLEAR_PERI_REG_MASK(SLC_RX_LINK, SLC_RXLINK_START);
}

LOCAL void slc_isr(void) {
	//portBASE_TYPE HPTaskAwoken=0;
	struct sdio_queue *finishedDesc;
	uint32 slc_intr_status;
	int x;

//int kg = 0;
//keepgoing:

	slc_intr_status = READ_PERI_REG(SLC_INT_STATUS);

	//clear all intr flags
	WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);//slc_intr_status);


//printf( "%08x\n", slc_intr_status );
	if ( (slc_intr_status & SLC_RX_EOF_INT_ST))
	{
		finishedDesc=(struct sdio_queue*)READ_PERI_REG(SLC_RX_EOF_DES_ADDR);

		if( finishedDesc->unused == 0 )
		{
			static uint8_t nonlpcount;

			if( tx_link_address == 0 )
			{
				i2stxdone = 1;

				//Handle NLPs
				nonlpcount++;
				if( nonlpcount > ((40000*16)/(I2STXZERO*32*2)) )
				{
					tx_link_address = (uint32_t)(&i2sBufDescNLP);
					i2stxdone = 0;
					nonlpcount = 0;
				}
			}
			else
			{
				i2sBufDescTX[1].next_link_ptr = tx_link_address;
				tx_link_address = 0;
			}
		}
		else
		{
			i2sBufDescTX[1].next_link_ptr = (int)&i2sBufDescTX[0];
		}




		slc_intr_status &= ~SLC_RX_EOF_INT_ST;
		etx++;
	}
	if ( (slc_intr_status & SLC_TX_EOF_INT_ST))
	{
		finishedDesc=(struct sdio_queue*)READ_PERI_REG(SLC_TX_EOF_DES_ADDR);


//XXX WARNING Why does undeflow detection not work!?!?
#define DETECT_UNDERFLOWS
#ifdef DETECT_UNDERFLOWS
		static struct sdio_queue * expected_next = &i2sBufDescRX[0];
		if( finishedDesc != expected_next ) printf( "U\n" );
		expected_next = (struct sdio_queue *)finishedDesc->next_link_ptr;
#endif
		GotNewData( (uint32_t*) finishedDesc->buf_ptr, I2SDMABUFLEN );
		erx++;

		finishedDesc->owner=1;  //Return to the i2s subsystem

		//Don't know why - but this MUST be done, otherwise everything comes to a screeching halt.
		slc_intr_status &= ~SLC_TX_EOF_INT_ST;
	}
	if( slc_intr_status & SLC_TX_DSCR_ERR_INT_ST ) //RX Fault, maybe owner was not set fast enough?
	{
		KickRX();
		printf( "RXFault\n" );
		slc_intr_status &= ~SLC_TX_DSCR_ERR_INT_ST;
		last_unknown_int++;
	}
	if( slc_intr_status )
	{
		last_unknown_int = slc_intr_status;
		printf( "UI:%08x\n", last_unknown_int );
	}

}

//Initialize I2S subsystem for DMA circular buffer use
void ICACHE_FLASH_ATTR testi2s_init() {
	int x, y;
	//Bits are shifted out

	//Initialize DMA buffer descriptors in such a way that they will form a circular
	//buffer.

	//Reset DMA )
	//SLC_RX_NO_RESTART_CLR = ?????
	//SLC_TX_LOOP_TEST = IF this isn't set, SO will occasionally get unrecoverable errors when you underflow.
	//Originally this little tidbit was found at https://github.com/pvvx/esp8266web/blob/master/info/libs/bios/sip_slc.c
	//
	//I have not tried without SLC_AHBM_RST | SLC_AHBM_FIFO_RST.  I just assume they are useful?
	SET_PERI_REG_MASK(SLC_CONF0, SLC_TX_LOOP_TEST |SLC_RXLINK_RST|SLC_TXLINK_RST|SLC_AHBM_RST | SLC_AHBM_FIFO_RST  );
	//I don't think I need these: SLC_RX_NO_RESTART_CLR | SLC_DATA_BURST_EN | SLC_DSCR_BURST_EN  

	//Clear DMA int flags
	SET_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);


	//Reset pin functions to non-I2S 
	//XXX CHARLES DELETEME????
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);


	for (x=0; x<DMABUFFERDEPTH; x++) {
		i2sBufDescRX[x].owner=1;
		i2sBufDescRX[x].eof=1;
		i2sBufDescRX[x].sub_sof=0;
		i2sBufDescRX[x].datalen=I2SDMABUFLEN*4;
		i2sBufDescRX[x].blocksize=I2SDMABUFLEN*4;
		i2sBufDescRX[x].buf_ptr=(uint32_t)&i2sBDRX[x*I2SDMABUFLEN];
		i2sBufDescRX[x].unused=x;
		i2sBufDescRX[x].next_link_ptr=(int)((x<(DMABUFFERDEPTH-1))?(&i2sBufDescRX[x+1]):(&i2sBufDescRX[0]));
		for( y = 0; y < I2SDMABUFLEN; y++ )
		{
			i2sBDRX[y+x*I2SDMABUFLEN] = 0x00000000;
		}
	}

	i2sBufDescTX[0].owner=1;
	i2sBufDescTX[0].eof=1;
	i2sBufDescTX[0].sub_sof=0;
	i2sBufDescTX[0].datalen=I2STXZERO*4;
	i2sBufDescTX[0].blocksize=I2STXZERO*4;
	i2sBufDescTX[0].buf_ptr=(uint32_t)&i2sBDTX[0];
	i2sBufDescTX[0].unused=0;
	i2sBufDescTX[0].next_link_ptr= (int)(&i2sBufDescTX[1]);

	i2sBufDescTX[1].owner=1;
	i2sBufDescTX[1].eof=1;
	i2sBufDescTX[1].sub_sof=0;
	i2sBufDescTX[1].datalen=I2STXZERO*4;
	i2sBufDescTX[1].blocksize=I2STXZERO*4;
	i2sBufDescTX[1].buf_ptr=(uint32_t)&i2sBDTX[0];
	i2sBufDescTX[1].unused=1;
	i2sBufDescTX[1].next_link_ptr= (int)(&i2sBufDescTX[0]);

	for( y = 0; y < I2STXZERO; y++ )
	{
		i2sBDTX[y] = 0x00000000;
	}

	i2sBufDescNLP.owner=1;
	i2sBufDescNLP.eof=0;
	i2sBufDescNLP.sub_sof=0;
	i2sBufDescNLP.datalen=4*4;
	i2sBufDescNLP.blocksize=4*4;
	i2sBufDescNLP.buf_ptr=(uint32_t)I2SNLP;
	i2sBufDescNLP.unused=6;
	i2sBufDescNLP.next_link_ptr= (int)(&i2sBufDescTX[0]);

	i2stxdone = 1;


	CLEAR_PERI_REG_MASK(SLC_CONF0, SLC_RXLINK_RST|SLC_TXLINK_RST|SLC_AHBM_RST | SLC_AHBM_FIFO_RST);

	//Enable and configure DMA
	CLEAR_PERI_REG_MASK(SLC_CONF0, (SLC_MODE<<SLC_MODE_S));

	SET_PERI_REG_MASK(SLC_CONF0, (1<<SLC_MODE_S));
	SET_PERI_REG_MASK(SLC_RX_DSCR_CONF,SLC_INFOR_NO_REPLACE|SLC_TOKEN_NO_REPLACE); //Do this according to 8p-esp8266_i2s_module ... page 6/9  I can't see any impact.

//	CLEAR_PERI_REG_MASK(SLC_RX_DSCR_CONF,SLC_RX_FILL_EN); //??? Just some junk I tried.
//	SET_PERI_REG_MASK(SLC_RX_DSCR_CONF,SLC_RX_FILL_EN); //??? Just some junk I tried.
//	SET_PERI_REG_MASK(SLC_RX_DSCR_CONF,SLC_RX_FILL_MODE); //??? Just some junk I tried.


	CLEAR_PERI_REG_MASK(SLC_TX_LINK,SLC_TXLINK_DESCADDR_MASK);
	SET_PERI_REG_MASK(SLC_TX_LINK, ((uint32)&i2sBufDescRX[0]) & SLC_TXLINK_DESCADDR_MASK); //any random desc is OK, we don't use TX but it needs something valid
	CLEAR_PERI_REG_MASK(SLC_RX_LINK,SLC_RXLINK_DESCADDR_MASK);
	SET_PERI_REG_MASK(SLC_RX_LINK, ((uint32)&i2sBufDescTX[0]) & SLC_RXLINK_DESCADDR_MASK);

	//Attach the DMA interrupt
	ets_isr_attach(ETS_SLC_INUM, slc_isr);
	WRITE_PERI_REG(SLC_INT_ENA,  SLC_INTEREST_EVENT);

	//clear any interrupt flags that are set
	WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);
	///enable DMA intr in cpu
	ets_isr_unmask(1<<ETS_SLC_INUM);

	//Init pins to i2s functions
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_I2SI_DATA);
//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_I2SI_WS);  //Dunno why - this is needed.  If it's not enabled, nothing will be read.
//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_I2SI_BCK); //

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_I2SO_DATA);
//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_I2SO_WS);
//	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_I2SO_BCK);

	//Enable clock to i2s subsystem
	i2c_writeReg_Mask_def(i2c_bbpll, i2c_bbpll_en_audio_clock_out, 1);

	//Reset I2S subsystem
	CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);
	SET_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);
	CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);

	CLEAR_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN|(I2S_I2S_RX_FIFO_MOD<<I2S_I2S_RX_FIFO_MOD_S)|
			(I2S_I2S_TX_FIFO_MOD<<I2S_I2S_TX_FIFO_MOD_S)|
			(I2S_I2S_TX_DATA_NUM<<I2S_I2S_TX_DATA_NUM_S)|
			(I2S_I2S_RX_DATA_NUM<<I2S_I2S_RX_DATA_NUM_S) );
		//Tried changing various values for I2S_I2S_RX_FIFO_MOD_S, etc... no effect.

	SET_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN);
	WRITE_PERI_REG(I2SRXEOF_NUM, RX_NUM);
/*
	//Trying to play with these... 	  No effect.
	SET_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN |
		(0x10<<I2S_I2S_TX_DATA_NUM_S) | (0x20<<I2S_I2S_RX_DATA_NUM_S)
	);

	//I2S_I2S_RX_FIFO_MOD_S>=4 = no interrupts in at all. Other modes vary.
	SET_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN |
		(0x01<<I2S_I2S_RX_FIFO_MOD_S) | (0x01<<I2S_I2S_TX_FIFO_MOD_S)
	);
	
*/
//No effect :(
//	WRITE_PERI_REG( I2SCONF_SIGLE_DATA, 32*4 );



	//Playing with these shows no impact unless specifically selected to be wrong.  (no effect)
	CLEAR_PERI_REG_MASK(I2SCONF_CHAN, (I2S_TX_CHAN_MOD<<I2S_TX_CHAN_MOD_S)|(I2S_RX_CHAN_MOD<<I2S_RX_CHAN_MOD_S));
	SET_PERI_REG_MASK(I2SCONF_CHAN, (0<<I2S_TX_CHAN_MOD_S)|(0<<I2S_RX_CHAN_MOD_S));

	//Clear int
	SET_PERI_REG_MASK(I2SINT_CLR,   I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
			I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);
	CLEAR_PERI_REG_MASK(I2SINT_CLR, I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
			I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);


	CLEAR_PERI_REG_MASK(I2SCONF, I2S_TRANS_SLAVE_MOD|I2S_RECE_SLAVE_MOD|
						(I2S_BITS_MOD<<I2S_BITS_MOD_S)|
						(I2S_BCK_DIV_NUM <<I2S_BCK_DIV_NUM_S)|
                                    	(I2S_CLKM_DIV_NUM<<I2S_CLKM_DIV_NUM_S));
	
	SET_PERI_REG_MASK(I2SCONF, I2S_RIGHT_FIRST|I2S_MSB_RIGHT|
						I2S_RECE_MSB_SHIFT|I2S_TRANS_MSB_SHIFT|
						((WS_I2S_BCK&I2S_BCK_DIV_NUM )<<I2S_BCK_DIV_NUM_S)|
						((WS_I2S_DIV&I2S_CLKM_DIV_NUM)<<I2S_CLKM_DIV_NUM_S) );



	//Start transmission
	SET_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
	SET_PERI_REG_MASK(SLC_RX_LINK, SLC_RXLINK_START);

	//enable int
	SET_PERI_REG_MASK(I2SINT_ENA,   I2S_I2S_TX_REMPTY_INT_ENA|I2S_I2S_TX_WFULL_INT_ENA |
			I2S_I2S_RX_REMPTY_INT_ENA|I2S_I2S_TX_PUT_DATA_INT_ENA|I2S_I2S_RX_TAKE_DATA_INT_ENA);  //I don't believe these are needed.

	//Start transmission
	SET_PERI_REG_MASK(I2SCONF,I2S_I2S_TX_START|I2S_I2S_RX_START);
}



void SendI2SPacket( uint32_t * pak, uint16_t dwords )
{
	int firstdwords = dwords;
	int seconddwords = 0;
	if( dwords > 1008 )
	{
		firstdwords = 1008;
		seconddwords = dwords-1008;
	}

	while( !i2stxdone );

	i2sBufDescTX[2].owner=1;
	i2sBufDescTX[2].eof=1;
	i2sBufDescTX[2].sub_sof=0;
	i2sBufDescTX[2].datalen=firstdwords*4;
	i2sBufDescTX[2].blocksize=16;
	i2sBufDescTX[2].buf_ptr=(uint32_t)pak;
	i2sBufDescTX[2].unused=2;
	i2sBufDescTX[2].next_link_ptr= (int)(&i2sBufDescTX[0]);

	if( seconddwords )
	{
		i2sBufDescTX[3].owner=1;
		i2sBufDescTX[3].eof=1;
		i2sBufDescTX[3].sub_sof=0;
		i2sBufDescTX[3].datalen=seconddwords*4;
		i2sBufDescTX[3].blocksize=16;
		i2sBufDescTX[3].buf_ptr=(uint32_t)&pak[firstdwords];
		i2sBufDescTX[3].unused=3;
		i2sBufDescTX[3].next_link_ptr= (int)(&i2sBufDescTX[0]);

		i2sBufDescTX[2].next_link_ptr= (int)(&i2sBufDescTX[3]);
	}

	//Link in.
	//i2sBufDescTX[2].next_link_ptr = (int)(&i2sBufDescTX[3]);
	tx_link_address = (uint32_t)&i2sBufDescTX[2];
	//Set our "notdone" flag
	i2stxdone = 0;
}

static void	GotNewData( uint32_t * dat, int datlen )
{
	int i = 0;
	int r;
	static int stripe = 0;

	gotdma=1;

keep_going:
	if( PacketStoreInSitu < 0 )
	{
		//Search for until data is ffffffff or 00000000.  This would be if we think we hit the end of a packet or a bad packet.  Just speed along till the bad dream is over.
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
	if( PacketStoreInSitu == 0 )
	{
		//Quescent state.
		for( ; i < datlen; i++ )
		{
			uint32_t d = dat[i];

			//Look for a set of 3 non null packets.
			if( d != 0xffffffff && d != 0x00000000 )
			{
				gotlink = 1;
				stripe++;
				if( stripe == 4 )
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

		//Nothing interesting happened all packet.
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
		i += 2;

	}

#ifdef ALLOW_FRAME_DEBUGGING
	if( KeepNextPacket > 0 && KeepNextPacket < 3 )
	{
		int start;
		if( i != 0 )
		{
			//Starting a packet.	
			start = i-(4+2);
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
		int trim = (datlen-r);
		trim--;
		if( trim < 0 ) trim = 0;
		if( trim > PacketStoreLength - 10 ) trim = (PacketStoreLength-10);
		PacketStoreLength -= trim;

		//Only release if packet not waiting for clear.
		if( KeepNextPacket == 1 ) KeepNextPacket = 4;

		//Packet knowingly faulted and we were waiting for packet?
		if( KeepNextPacket == 2 && rx_pack_flags[rx_cur] == 0 ) { KeepNextPacket = 4; }
	}
#endif

	goto keep_going;
}



