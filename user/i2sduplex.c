//Copyright 2015-2016 <>< Charles Lohr, see LICENSE file.  This file is licensed under the 2-Clause BSD License.

//ESP8266 I2S Input+Output
//This is BASED OFF OF, but DIFFERENT THAN the github project http://github.com/cnlohr/esp8266duplexi2s

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
static struct sdio_queue i2sBufDescTX[6];
static struct sdio_queue i2sBufDescNLP;
static uint32_t I2SNLP[4] = { 0x00000000, 0x00000000, 0x0000001f, 0x00000000 };


//State machine:
//   [0] -> [1] -> [0]
//   [2] -> [3] -> [2]
//
// When inserting a new packet:
//   [3] -> [2]
//   [0] -> [1] -> NEW PACKET -> [2] -> [3] -> [2] -> [3]
// Then, in the interrupt we repair it and setup for reset...  [1] -> [0], [3]->[0]
//
// NLPs also point to 2 when inserted.

uint32_t i2sBDRX[I2SDMABUFLEN*DMABUFFERDEPTH];
uint32_t i2sBDTX[I2STXZERO];

uint8_t gotdma;
uint8_t gotlink;

volatile uint32_t last_unknown_int;
int erx, etx;

volatile uint32_t tx_link_address;
volatile uint8_t i2stxdone;

static uint32_t nlpcount;  //Bytes that have passed since last NLP (in 200ns units)
static uint8_t insend;     //0 = Not sending, 1 = Sending NLP, 2 = Sending Data.



void	GotNewI2SData( uint32_t * dat, int datlen );

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

	slc_intr_status = READ_PERI_REG(SLC_INT_STATUS);

	//clear all intr flags
	WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);//slc_intr_status);

	if ( (slc_intr_status & SLC_RX_EOF_INT_ST))
	{
		//NLPs every 16ms
		#define NLP_EVERY ((40000000/8)/62.5)
		finishedDesc=(struct sdio_queue*)READ_PERI_REG(SLC_RX_EOF_DES_ADDR);

		//Watch how many bits we're sending to determine when we need to send an NLP.
		nlpcount += finishedDesc->datalen;

		//Currently done transmitting a packet/nlp.
		//Warning: This code may be executed multiple times per packet.
	//	printf( "%d", finishedDesc->unused );
		if( finishedDesc->unused == 3 && insend )
		{
			//Relink the arrays, all back to the 0/1 loop.
			i2sBufDescTX[1].next_link_ptr= (int)(&i2sBufDescTX[0]);
			i2sBufDescTX[3].next_link_ptr= (int)(&i2sBufDescTX[0]);

			//If it was an NLP, clear the NLP.
			if( insend == 2 )
			{
				i2stxdone = 1;
			}
			insend = 0;
		}

		//Warning: This code may be called multiple times per packet.
		if( finishedDesc->unused == 1 && !insend )
		{
			i2sBufDescTX[3].next_link_ptr= (int)(&i2sBufDescTX[2]);
			if( nlpcount > NLP_EVERY )	
			{
				i2sBufDescTX[1].next_link_ptr= (int)(&i2sBufDescNLP);
				insend = 1;
				nlpcount = 0;
			}

			if( tx_link_address && !insend )
			{
				i2sBufDescTX[1].next_link_ptr= (int)(tx_link_address);
				tx_link_address = 0;
				insend = 2;
			}
		}

		slc_intr_status &= ~SLC_RX_EOF_INT_ST;
		etx++;
	}
	if ( (slc_intr_status & SLC_TX_EOF_INT_ST))
	{
		finishedDesc=(struct sdio_queue*)READ_PERI_REG(SLC_TX_EOF_DES_ADDR);

//#define DETECT_UNDERFLOWS
#ifdef DETECT_UNDERFLOWS
		static struct sdio_queue * expected_next = &i2sBufDescRX[0];
		if( finishedDesc != expected_next ) uart0_sendStr( "U" );
		expected_next = (struct sdio_queue *)finishedDesc->next_link_ptr;
#endif

#ifdef DUMMY_DONT_DO_DATA
		gotdma=1;
		gotlink = 1;
#else

//#define DETECT_UNDERFLOWS
#ifdef PROFILE_GOTNEWDATA
		static int i = 0;
		uint32_t k;
		i++;
		if( i == 1000 )
		{
			k = system_get_time();
		}
#endif
		GotNewI2SData( (uint32_t*) finishedDesc->buf_ptr, I2SDMABUFLEN );
#ifdef PROFILE_GOTNEWDATA
		if( i == 1000 )
		{
			k -=  system_get_time();
			printf( "RT: %d\n",  k );
			i = 0;
		}
#endif
#endif

		erx++;

		finishedDesc->owner=1;  //Return to the i2s subsystem

		//Don't know why - but this MUST be done, otherwise everything comes to a screeching halt.
		slc_intr_status &= ~SLC_TX_EOF_INT_ST;
	}
	if( slc_intr_status & SLC_TX_DSCR_ERR_INT_ST ) //RX Fault, maybe owner was not set fast enough?
	{
		KickRX();
		//printf( "RXFault\n" );
		slc_intr_status &= ~SLC_TX_DSCR_ERR_INT_ST;
		last_unknown_int++;
	}
	if( slc_intr_status )
	{
		last_unknown_int = slc_intr_status;
		//printf( "UI:%08x\n", last_unknown_int );
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
		i2sBufDescRX[x].unused=0;
		i2sBufDescRX[x].next_link_ptr=(int)((x<(DMABUFFERDEPTH-1))?(&i2sBufDescRX[x+1]):(&i2sBufDescRX[0]));
		for( y = 0; y < I2SDMABUFLEN; y++ )
		{
			i2sBDRX[y+x*I2SDMABUFLEN] = 0x00000000;
		}
	}

	static const uint8_t bufferpointers[] = { 1, 0, 3, 2, 2, 2 };

	for( x = 0; x < 6; x++ )
	{
		i2sBufDescTX[x].owner=1;
		i2sBufDescTX[x].eof=1;
		i2sBufDescTX[x].sub_sof=0;
		i2sBufDescTX[x].datalen=I2STXZERO*4;
		i2sBufDescTX[x].blocksize=I2STXZERO*4;
		i2sBufDescTX[x].buf_ptr=(uint32_t)&i2sBDTX[0];
		i2sBufDescTX[x].unused=x;
		i2sBufDescTX[x].next_link_ptr= (int)(&i2sBufDescTX[bufferpointers[x]]);
	}


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
	i2sBufDescNLP.next_link_ptr= (int)(&i2sBufDescTX[2]);

	i2stxdone = 1;
	nlpcount = 0;
	insend = 0;
	tx_link_address = 0;


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

	i2sBufDescTX[4].owner=1;
	i2sBufDescTX[4].eof=1;
	i2sBufDescTX[4].sub_sof=0;
	i2sBufDescTX[4].datalen=firstdwords*4;
	i2sBufDescTX[4].blocksize=16;
	i2sBufDescTX[4].buf_ptr=(uint32_t)pak;
	i2sBufDescTX[4].unused=2;
	i2sBufDescTX[4].next_link_ptr= (int)(&i2sBufDescTX[2]);

	if( seconddwords )
	{
		i2sBufDescTX[5].owner=1;
		i2sBufDescTX[5].eof=1;
		i2sBufDescTX[5].sub_sof=0;
		i2sBufDescTX[5].datalen=seconddwords*4;
		i2sBufDescTX[5].blocksize=16;
		i2sBufDescTX[5].buf_ptr=(uint32_t)&pak[firstdwords];
		i2sBufDescTX[5].unused=3;
		i2sBufDescTX[5].next_link_ptr= (int)(&i2sBufDescTX[2]);

		i2sBufDescTX[4].next_link_ptr= (int)(&i2sBufDescTX[5]);
	}

	//Link in.
	tx_link_address = (uint32_t)&i2sBufDescTX[4];

	//Set our "notdone" flag
	i2stxdone = 0;
}



