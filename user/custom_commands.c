//Copyright 2015-2016 <>< Charles Lohr, see LICENSE file.  This file is licensed under the 2-Clause BSD License or the MIT/x11 license.

#include <commonservices.h>
#include <mystuff.h>
#include <i2sduplex.h>

//These are commands that come in through UDP port 7878 or via the websocket.
//The rest of the commands lie in common/commonservices.c.
//
//This file was programmed for use with espthernet.

extern uint32_t g_process_paktime;

void ICACHE_FLASH_ATTR ReinitSettings()
{
}

void ICACHE_FLASH_ATTR SettingsLoaded()
{
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

#ifdef ALLOW_FRAME_DEBUGGING
	case 'K': case 'k':		//Capture packet
	{
		PacketStoreLength = 0;
		KeepNextPacket = my_atoi( pusrdata+2 ); //2 = wait for error, 1 = just next packet.
		g_process_paktime = 0;
		buffend += ets_sprintf( buffend, "CK" );
		return buffend-buffer;
	}

	case 'L': case 'l':		//Check packet capture status.
	{
		int i;
		int gotpak = 0;
		if( KeepNextPacket == 4 ) gotpak = 1;
		buffend += ets_sprintf( buffend, "CL:%d:%d", gotpak, g_process_paktime );
		return buffend-buffer;
	}

	case 'M': case 'm':		//Read out 512 bits from this packet.
	{
		int i;
		int gotpak = 0;
		int wordofs;
		if( ets_strlen( pusrdata ) <= 3 )
		{
			buffend += ets_sprintf( buffend, "!CM" );
			return buffend-buffer;
		}
		wordofs = my_atoi(&pusrdata[3]);

		if( KeepNextPacket == 4 ) gotpak = 1;

		if( !gotpak ) 
		{
			buffend += ets_sprintf( buffend, "!CM" );
			return buffend-buffer;
		}

		buffend += ets_sprintf( buffend, "CM:%d:32:%d:",wordofs,PacketStoreLength+1 );

		for( i = 0; i < 32; i++ )
		{
			uint32_t r = PacketStore[wordofs+i];
			*(buffend++) = tohex1((r>>28)&15);
			*(buffend++) = tohex1((r>>24)&15);
			*(buffend++) = tohex1((r>>20)&15);
			*(buffend++) = tohex1((r>>16)&15);
			*(buffend++) = tohex1((r>>12)&15);
			*(buffend++) = tohex1((r>>8)&15);
			*(buffend++) = tohex1((r>>4)&15);
			*(buffend++) = tohex1((r)&15);
		}

		//Capture packet
		return buffend - buffer;
	}

	case 'N': case 'n':		//Releasee packet.
	{
		int i;
		int gotpak = 0;
		KeepNextPacket = 0;
		if( KeepNextPacket == 4 ) gotpak = 1;
		if( gotpak )
		{
			buffend += ets_sprintf( buffend, "CN" );
		}
		else
		{
			buffend += ets_sprintf( buffend, "!CN" );
		}
		return buffend-buffer;
	}
#endif

	}
	return -1;
}
