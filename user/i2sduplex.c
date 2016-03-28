//ESP8266 I2S Input+Output

#include "slc_register.h"
#include "mystuff.h"
#include <c_types.h>
#include "user_interface.h"
#include "pin_mux_register.h"
#include <dmastuff.h>
#include "i2sduplex.h"

//These contol the speed at which the bus comms.
#define WS_I2S_BCK 2  //Can't be less than 2, if you want to RX
#define WS_I2S_DIV 1

//I2S DMA buffer descriptors
static struct sdio_queue i2sBufDescRX[DMABUFFERDEPTH];
static struct sdio_queue i2sBufDescTX[3];
uint32_t i2sBDRX[I2SDMABUFLEN*DMABUFFERDEPTH];
uint32_t i2sBDTX[I2STXZERO];

uint8_t gotdma;
uint8_t gotlink;


volatile uint32_t last_unknown_int;
int fxcycle;
int erx, etx;

volatile uint8_t i2stxdone;

static void	GotNewData( uint32_t * dat, int datlen );

void KickRX()
{
/*	int x;
	SET_PERI_REG_MASK(SLC_CONF0, SLC_TXLINK_RST);
	CLEAR_PERI_REG_MASK(SLC_CONF0, SLC_TXLINK_RST);

	CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_TX_START);

	CLEAR_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
	SET_PERI_REG_MASK(SLC_CONF0, SLC_TXLINK_RST);

	CLEAR_PERI_REG_MASK(SLC_CONF0, SLC_TXLINK_RST);

	for (x=0; x<DMABUFFERDEPTH; x++) {
		i2sBufDescRX[x].owner=1;
		i2sBufDescRX[x].eof=1;
		i2sBufDescRX[x].sub_sof=0;
		i2sBufDescRX[x].datalen=I2SDMABUFLEN*4;
		i2sBufDescRX[x].blocksize=I2SDMABUFLEN*4;
		i2sBufDescRX[x].buf_ptr=(uint32_t)&i2sBDRX[x*I2SDMABUFLEN];
		i2sBufDescRX[x].unused=0;
		i2sBufDescRX[x].next_link_ptr=(int)((x<(DMABUFFERDEPTH-1))?(&i2sBufDescRX[x+1]):(&i2sBufDescRX[0]));
	}


	CLEAR_PERI_REG_MASK(SLC_TX_LINK,SLC_TXLINK_DESCADDR_MASK);
	SET_PERI_REG_MASK(SLC_TX_LINK, ((uint32)&i2sBufDescRX[0]) & SLC_RXLINK_DESCADDR_MASK);
	SET_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
	SET_PERI_REG_MASK(I2SCONF,I2S_I2S_TX_START);
	//Doesn't seem to work.

*/
	testi2s_init();
}

void StartI2S()
{
//	testi2s_init();
	SET_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
	SET_PERI_REG_MASK(SLC_RX_LINK, SLC_RXLINK_START);

}

void StopI2S()
{
	CLEAR_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
	CLEAR_PERI_REG_MASK(SLC_RX_LINK, SLC_RXLINK_START);
//	WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);//slc_intr_status);
//	SET_PERI_REG_MASK(SLC_CONF0, SLC_TX_LOOP_TEST |SLC_RXLINK_RST|SLC_TXLINK_RST|SLC_AHBM_RST | SLC_AHBM_FIFO_RST  );
}

LOCAL void slc_isr(void) {
	//portBASE_TYPE HPTaskAwoken=0;
	struct sdio_queue *finishedDesc;
	uint32 slc_intr_status;
	int x;
	fxcycle++;

	slc_intr_status = READ_PERI_REG(SLC_INT_STATUS);
	//clear all intr flags
	WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);//slc_intr_status);

//printf( "%08x\n", slc_intr_status );
	if ( (slc_intr_status & SLC_RX_EOF_INT_ST))
	{
		finishedDesc=(struct sdio_queue*)READ_PERI_REG(SLC_RX_EOF_DES_ADDR);

		if( finishedDesc->unused == 1 )
		{
			i2sBufDescTX[1].next_link_ptr=(int)(&i2sBufDescTX[0]);
			i2stxdone = 1;
		}

		slc_intr_status &= ~SLC_RX_EOF_INT_ST;
		etx++;
	}
	if ( (slc_intr_status & SLC_TX_EOF_INT_ST))
	{
		finishedDesc=(struct sdio_queue*)READ_PERI_REG(SLC_TX_EOF_DES_ADDR);

		GotNewData( (uint32_t*) finishedDesc->buf_ptr, I2SDMABUFLEN );
		finishedDesc->owner=1;

		//Don't know why - but this MUST be done, otherwise everything comes to a screeching halt.

		slc_intr_status &= ~SLC_TX_EOF_INT_ST;

		erx++;
	}
	if( slc_intr_status & SLC_TX_DSCR_ERR_INT_ST ) //RX Fault, maybe owner was not set fast enough?
	{
		KickRX();
		slc_intr_status &= ~SLC_TX_DSCR_ERR_INT_ST;
		last_unknown_int++;
	}
	if( slc_intr_status )
	{
		last_unknown_int = slc_intr_status;
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
	//XXX CHARLES DELETEME
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

	i2sBufDescTX[2].owner=1;
	i2sBufDescTX[2].eof=1;
	i2sBufDescTX[2].sub_sof=0;
	i2sBufDescTX[2].datalen=I2STXZERO*4;
	i2sBufDescTX[2].blocksize=I2STXZERO*4;
	i2sBufDescTX[2].buf_ptr=(uint32_t)&i2sBDTX[0];
	i2sBufDescTX[2].unused=1;
	i2sBufDescTX[2].next_link_ptr= (int)(&i2sBufDescTX[0]);

	i2stxdone = 1;


	CLEAR_PERI_REG_MASK(SLC_CONF0, SLC_RXLINK_RST|SLC_TXLINK_RST|SLC_AHBM_RST | SLC_AHBM_FIFO_RST);

	//Enable and configure DMA
	CLEAR_PERI_REG_MASK(SLC_CONF0, (SLC_MODE<<SLC_MODE_S));


	SET_PERI_REG_MASK(SLC_CONF0, (1<<SLC_MODE_S));

	
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

	CLEAR_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN|(I2S_I2S_RX_FIFO_MOD<<I2S_I2S_RX_FIFO_MOD_S)|(I2S_I2S_TX_FIFO_MOD<<I2S_I2S_TX_FIFO_MOD_S));
	SET_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN);
	WRITE_PERI_REG(I2SRXEOF_NUM, RX_NUM);

	CLEAR_PERI_REG_MASK(I2SCONF_CHAN, (I2S_TX_CHAN_MOD<<I2S_TX_CHAN_MOD_S)|(I2S_RX_CHAN_MOD<<I2S_RX_CHAN_MOD_S));

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
	i2stxdone = 0;
	i2sBufDescTX[2].owner=1;
	i2sBufDescTX[2].eof=1;
	i2sBufDescTX[2].sub_sof=0;
	i2sBufDescTX[2].datalen=dwords*4;
	i2sBufDescTX[2].blocksize=dwords*4;
	i2sBufDescTX[2].buf_ptr=(uint32_t)pak;
	i2sBufDescTX[2].unused=1;
	i2sBufDescTX[2].next_link_ptr= (int)(&i2sBufDescTX[0]);
	i2sBufDescTX[1].next_link_ptr= (int)(&i2sBufDescTX[2]);
}

//For storing packets when they come in, so they can be processed in the main loop.
uint32_t PacketStore[STOPKTSIZE];
uint16_t PacketStoreLength; 
int8_t   KeepNextPacket = 0;
int8_t   PacketStoreInSitu = 0;

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
				i++;
				stripe = 1;
				break;
			}
		}

		//Still searching?  If so come back in here next time.
		if( PacketStoreInSitu ) return;
	}

	//Otherwise we're looking for 3 d's
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
			}
			else
			{
				stripe = 0;
			}

			if( stripe == 3 )
			{
				PacketStoreInSitu = 1;
				break;
			}
		}

		//Nothing interesting happened all packet.
		if( !PacketStoreInSitu )
			return;

		//Something happened... 
		i++;

		r = ResetPacketInternal(1);

		//Make sure we can get a free packet.
		if( r < 0 )
		{
			//Otherwise, we have to dump this on-wire packet.
			PacketStoreInSitu = -1;
			goto keep_going;
		}

		//Good to go and start processing data.
	}

	//Start processing a packet.
	if( i < datlen )
	{
		r = DecodePacket( &dat[i], datlen - i );
	}

	//Done with this segment, next one to come.
	if( r == 0 )
	{
		gotlink = 1;
		return;
	}

	//Packet is complete, or error in packet.
	PacketStoreInSitu = -1;

	return;
//Can't keep going, since we don't know how far DecodePacket read.
//	goto keep_going;
}



