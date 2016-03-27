//Copyright 2015 <>< Charles Lohr, see LICENSE file.

#include <commonservices.h>
#include <mystuff.h>
#include <i2sduplex.h>

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

	case 'K': case 'k':		//Capture packet
	{
		KeepNextPacket = my_atoi( pusrdata+2 );
		buffend += ets_sprintf( buffend, "CK" );
		return buffend-buffer;
	}

	case 'L': case 'l':		//Check packet capture status.
	{
		int i;
		int gotpak = 0;
		for( i = 0 ; i < STOPKT; i++ )
		{
			if( PacketStoreFlags[i] == 3 )
			{
				gotpak = i+1;
			}
		}
		buffend += ets_sprintf( buffend, "CL:%d", gotpak );
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
		for( i = 0 ; i < STOPKT; i++ )
		{
			if( PacketStoreFlags[i] == 3 )
			{
				gotpak = i+1;
			}
		}

		if( !gotpak ) 
		{
			buffend += ets_sprintf( buffend, "!CM" );
			return buffend-buffer;
		}

		buffend += ets_sprintf( buffend, "CM:%d:32:%d:",wordofs,PacketStoreLength[(gotpak-1)] );

		for( i = 0; i < 32; i++ )
		{
			uint32_t r = PacketStore[STOPKTSIZE * (gotpak-1)+wordofs+i];
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
		for( i = 0 ; i < STOPKT; i++ )
		{
			if( PacketStoreFlags[i] == 3 )
			{
				PacketStoreFlags[i] = 0;
				gotpak = i+1;
			}
		}
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
	}
	return -1;
}
