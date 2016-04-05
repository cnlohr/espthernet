//Copyright 2015-2016 <>< Charles Lohr, see LICENSE file.  This file is licensed under the 2-Clause BSD License or the MIT/x11 license.

//This file also contains the logic for the application we are using.

#include <commonservices.h>
#include <mystuff.h>
#include <tcp.h>
#include <user_interface.h>
#include <espconn.h>
#include <i2sduplex.h>

//These are commands that come in through UDP port 7878 or via the websocket.
//The rest of the commands lie in common/commonservices.c.
//
//This file was programmed for use with espthernet.

//This file in particular handles all of the packet capture stuff.
// Thanks to: https://github.com/ernacktob/esp8266_wifi_raw for figuring a lot of that out.
//
//TODO: Figure out if https://github.com/cnlohr/esp8266rawpackets/blob/master/user/esp_rawsend.c can be applied.

int current_channel = 0;
uint32_t packetct = 0;

extern uint32_t g_process_paktime;

void ICACHE_FLASH_ATTR ReinitSettings()
{
}

void ICACHE_FLASH_ATTR SettingsLoaded()
{
}

extern struct espconn interop_conns[TCP_SOCKETS];
extern struct _esp_tcp interop_tcps[TCP_SOCKETS];


struct RxControl {
    signed rssi:8;
    unsigned rate:4;
    unsigned is_group:1;
    unsigned:1;
    unsigned sig_mode:2;
    unsigned legacy_length:12;
    unsigned damatch0:1;
    unsigned damatch1:1;
    unsigned bssidmatch0:1;
    unsigned bssidmatch1:1;
    unsigned MCS:7;
    unsigned CWB:1;
    unsigned HT_length:16;
    unsigned Smoothing:1;
    unsigned Not_Sounding:1;
    unsigned:1;
    unsigned Aggregation:1;
    unsigned STBC:2;
    unsigned FEC_CODING:1;
    unsigned SGI:1;
    unsigned rxend_state:8;
    unsigned ampdu_cnt:8;
    unsigned channel:4;
    unsigned:12;
};
 
struct LenSeq{ 
	u16 len; // length of packet 
    u16 seq; // serial number of packet, the high 12bits are serial number,
             //    low 14 bits are Fragment number (usually be 0)  
    u8 addr3[6]; // the third address in packet  
}; 


#define CLIENTENTRIES 20

struct ClientEntry
{
	uint8_t mac[6];
	uint32_t time;
	int8_t last;
};

struct ClientEntry cle[CLIENTENTRIES];




struct sniffer_buf {
	struct RxControl rx_ctrl; 
	u8 buf[36 ]; // head of ieee80211 packet 
    u16 cnt;     // number count of packet  
    struct LenSeq lenseq[1];  //length of packet  
};

struct sniffer_buf2{
    struct RxControl rx_ctrl; 
    u8 buf[112]; 
    u16 cnt;   
    u16 len;  //length of packet  
};

int gHandlingTCP = 0;

#define STORESIZE 1300
uint16_t sendtop = 0;
uint8_t sendbuffer[STORESIZE];

uint32_t  tv_sec;
uint32_t  tv_usec;
uint32_t  ctl;
void ICACHE_FLASH_ATTR UpdateTime()
{
	uint32_t ct = system_get_time();
	uint32_t diff = ct - ctl;
	ctl = ct;
	tv_usec += diff;
	tv_sec += tv_usec/1000000;
	tv_usec %= 1000000;
}

void ICACHE_FLASH_ATTR TickPacketSet( int slow )
{
	UpdateTime();

	if( !gHandlingTCP )
		return;

	if( !et_TCPCanSend( gHandlingTCP ) )
		return;

	if( sendtop )
	{
		TCPs[gHandlingTCP].sendtype = ACKBIT | PSHBIT;
		et_StartTCPWrite( gHandlingTCP );
		et_pushblob( sendbuffer,sendtop ); sendtop = 0;
		et_EndTCPWrite( gHandlingTCP );
	}
}

inline void ICACHE_FLASH_ATTR AddBL1( uint8_t by )
{
	sendbuffer[sendtop++] = by;
}
void ICACHE_FLASH_ATTR AddBL2( uint16_t by )
{
	sendbuffer[sendtop++] = by&0xff;
	sendbuffer[sendtop++] = by>>8;
}
void ICACHE_FLASH_ATTR AddBL4( uint32_t by )
{
	AddBL2( by & 0xffff );
	AddBL2( by >> 16 );
}




void ICACHE_FLASH_ATTR packetportrcv(void *arg, char *pusrdata, unsigned short length)
{
	//Do something with the incoming packet???
	printf( "Got something...\n" );
}


void ICACHE_FLASH_ATTR pkcapt_close(void * arg)
{
	gHandlingTCP = 0;
}

uint32_t frequencies[] = {2412,2417,2422,2427,2432,2437,2442,2447,2452,2457,2462,2467,2472,2484 };

//Operating on 6648

/* Listens communication between AP and client */
static void ICACHE_FLASH_ATTR promisc_cb(uint8_t *buf, uint16_t len)
{
//    struct RxControl *rx = (struct RxControl*) buf;
//	printf( "RX: %d %d %d\n", len, rx->MCS, rx->channel );
	int i;
    struct RxControl * rx_ctrl;
	int mdata = len;
	uint8_t * ldat;
	int padct = 0;

    if (len == 12){
		//Packet too rough to get real data.
		return;
    } else if (len == 128) {
        struct sniffer_buf2 *sniffer = (struct sniffer_buf2*) buf;
	    rx_ctrl = &sniffer->rx_ctrl;
		ldat = sniffer->buf;
		mdata = 112;
		padct = sniffer->len - 112;
		if( padct < 0 ) padct = 0;
    } else {
        struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
	    rx_ctrl = &sniffer->rx_ctrl;
		ldat = sniffer->buf;
		mdata = 36;
    }

	#define RADIOTAPLEN (8+4+3)

	if( ldat && mdata )
	{
		int i;
		int match = 0;

		uint32_t dt = 0;
		uint32_t now = system_get_time();
		for( i = 0; i < CLIENTENTRIES; i++ )
		{
			struct ClientEntry * ce = &cle[i];
			uint32_t diff = now - ce->time;
			if( diff > dt )
			{
				dt = diff;
				match = i;
			}
			if( ets_memcmp( ce->mac, ldat+10, 6 ) == 0 )
			{
				match = i;
				dt = 0xffffffff;
			}
		}

		ets_memcpy( cle[match].mac, ldat+10, 6 );
		cle[match].time = now;
		cle[match].last = rx_ctrl->rssi;
	}


	if( ldat && mdata && ( STORESIZE - sendtop > (RADIOTAPLEN+16+mdata+padct) ) )
	{
		packetct++;
		UpdateTime();

		AddBL4( tv_sec );
		AddBL4( tv_usec );
		AddBL4( padct+mdata+RADIOTAPLEN );
		AddBL4( padct+mdata+RADIOTAPLEN );

		AddBL1( 0 );
		AddBL1( 0 );

		AddBL2( RADIOTAPLEN );

		uint32_t flags = (1<<2) | //Rate
			(1<<3) | //Channel
			(1<<5) | //RSSI
			0;
		AddBL4( flags );
		//8 so far...
		AddBL2( rx_ctrl->rate );
		AddBL2( frequencies[current_channel] );
		AddBL2( 0x0080 );
		AddBL1( (uint8_t)rx_ctrl->rssi );
		ets_memcpy( &sendbuffer[sendtop], ldat, mdata );
		sendtop += mdata;
		ets_memset( &sendbuffer[sendtop], 0, padct );
		sendtop += padct;
	}
}


void ICACHE_FLASH_ATTR sniffer_system_init_done(void)
{
    // Set up promiscuous callback
    wifi_set_channel(1);current_channel = 0;
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(promisc_cb);
    wifi_promiscuous_enable(1);
	printf( "Listening on ch1\n" );
}

int ICACHE_FLASH_ATTR HandleIncomingEthernetSyn( int portno )
{
	if( portno == 6648 )
	{
		int ret = et_GetFreeConnection();

		ret = et_GetFreeConnection();
		if( !ret ) return 0;

		if( gHandlingTCP ) 
		{
			et_RequestClosure( gHandlingTCP );
		}

		gHandlingTCP = ret;

		static const uint8_t glbhdr[] = {
			0xd4, 0xc3, 0xb2, 0xa1, 0x02, 0x00, 0x04, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			(1536)&0xff, (1536)>>8, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00 }; //0x7f = radiotap header

		ets_memcpy( sendbuffer, glbhdr, sizeof( glbhdr ) );
		sendtop = sizeof( glbhdr );

		interop_conns[ret].recv_callback = packetportrcv;
		interop_conns[ret].proto.tcp = &interop_tcps[ret];
		interop_conns[ret].proto.tcp->disconnect_callback = pkcapt_close;
		return ret;
	}

	return 0;
}

int ICACHE_FLASH_ATTR CustomCommand(char * buffer, int retsize, char *pusrdata, unsigned short len)
{
	char * buffend = buffer;

	switch( pusrdata[1] )
	{
	case 'C': case 'c': //Custom command test
	{
		buffend += ets_sprintf( buffend, "CC" );
		return buffend-buffer;
	}
	case 'S': case 's':
	{
		int i;
		uint32_t now = system_get_time();
		buffend += ets_sprintf( buffend, "CS:%d:%d:%lu:%lu\n", gHandlingTCP, packetct, tv_sec, tv_usec );

		for( i = 0; i < CLIENTENTRIES; i++ )
		{
			struct ClientEntry * ce = &cle[i];
			int diff = now - ce->time;
			if( diff > 10000000 ) continue;
			buffend += ets_sprintf( buffend, "%02x:%02x:%02x:%02x:%02x:%02x\t%lu\t%d\n", 
				ce->mac[0], ce->mac[1], ce->mac[2], ce->mac[3], ce->mac[4], ce->mac[5], 
				diff, ce->last );
		}

		return buffend-buffer;
	}
	case 'H': case 'h':
	{
		//Get Channel

		buffend += ets_sprintf( buffend, "CH:%d", wifi_get_channel() );
		return buffend-buffer;
	}
	case 'I': case 'i':
	{
		int ch = my_atoi( pusrdata+3 ); //2 = wait for error, 1 = just next packet.

		//Change channel
		wifi_set_channel(ch); current_channel = ch-1;
	    wifi_promiscuous_enable(0);
	    wifi_set_promiscuous_rx_cb(promisc_cb);
	    wifi_promiscuous_enable(1);

		printf( "Change to channel: %d\n", ch );
		buffend += ets_sprintf( buffend, "CI:%d", wifi_get_channel() );
		return buffend-buffer;
	}
	}
	return -1;
}

