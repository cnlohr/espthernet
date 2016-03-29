//Copyright 2016 <>< Charles Lohr, see LICENSE file.

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "espconn.h"
#include "mystuff.h"
#include "i2sduplex.h"
#include "commonservices.h"
#include "manchestrate.h"
#include <mdns.h>
#include <net_compat.h>
#include <iparpetc.h>

#define PORT 7777

#define procTaskPrio        0
#define procTaskQueueLen    1

static volatile os_timer_t some_timer;

//#define DUMP_DEBUG_ETHERNET

//int ICACHE_FLASH_ATTR StartMDNS();

void user_rf_pre_init(void)
{
	//nothing.
}

char * strcat( char * dest, char * src )
{
	return strcat(dest, src );
}


//Tasks that happen all the time.

os_event_t    procTaskQueue[procTaskQueueLen];


static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{

	et_backend_tick_quick();

	CSTick( 0 );
	system_os_post(procTaskPrio, 0, 0 );
}

//Timer event.


static void ICACHE_FLASH_ATTR myTimer(void *arg)
{
	et_backend_tick_slow();

	CSTick( 1 );
}

void ICACHE_FLASH_ATTR HandleUDP( uint16_t len )
{
	et_pop16(); //Discard checksum.  Already CRC32'd the ethernet.

	if( localport != 7878 )
		return;

	char  __attribute__ ((aligned (32))) retbuf[1300];
	int r = issue_command( retbuf, 1300, &ETbuffer[ETsendplace], len-8 );
	et_finish_callback_now();

	if( r > 0 )
	{
		//Using avrcraft, this is how you send "reply" UDP packets manually.
		et_startsend( 0x0000 );
		send_etherlink_header( 0x0800 );
		send_ip_header( 0x00, ipsource, 17 ); //UDP (will fill in size and checksum later)
		et_push16( localport );
		et_push16( remoteport );
		et_push16( 0 ); //length for later
		et_push16( 0 ); //csum for later

		ets_memcpy( &ETbuffer[ETsendplace], retbuf, r );
		ETsendplace += r;

		util_finish_udp_packet();
	}

}


void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	uart0_sendStr("\r\nesp8266 driver\r\n");


//Uncomment this to force a system restore.
//	system_restore();

#define FORCE_SSID 1

#if FORCE_SSID

#include "ssid_info.h"

#endif

	//Override wifi.
#if FORCE_SSID
	{
		struct station_config stationConf;
		wifi_station_get_config(&stationConf);
		os_strcpy((char*)&stationConf.ssid, SSID );
		os_strcpy((char*)&stationConf.password, PSWD );
		stationConf.bssid_set = 0;
		wifi_station_set_config(&stationConf);
		wifi_set_opmode(1);
	}
#endif

	CSSettingsLoad( 0 );
	CSPreInit();

	//Override wifi.
#if FORCE_SSID
	{
		struct station_config stationConf;
		wifi_station_get_config(&stationConf);
		os_strcpy((char*)&stationConf.ssid, SSID );
		os_strcpy((char*)&stationConf.password, PSWD );
		stationConf.bssid_set = 0;
		wifi_station_set_config(&stationConf);
		wifi_set_opmode(1);
	}
#endif

	CSInit();

	//Note: MDNS is currently disabled in this project.
	SetServiceName( "espthernet" );
	AddMDNSName( "cn8266" );
	AddMDNSName( "espthernet" );
	AddMDNSService( "_http._tcp", "An ESP8266 Webserver", 80 );
	AddMDNSService( "_cn8266._udp", "ESP8266 Backend", 7878 );

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	//Timer example
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&some_timer, 10, 1);

	{
		uint8_t mac[6];
	    wifi_get_macaddr(SOFTAP_IF, mac);
		et_init( mac );
	}

	system_update_cpu_freq( SYS_CPU_160MHZ );

	system_os_post(procTaskPrio, 0, 0 );
}


//There is no code in this project that will cause reboots if interrupts are disabled.
void EnterCritical()
{
}

void ExitCritical()
{
}

