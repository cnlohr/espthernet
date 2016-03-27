#ifndef _MANCHESTRATE_H
#define _MANCHESTRATE_H

#include <c_types.h>
#include <eth_config.h>

//Do this to use a table-based manchester approach.  It is significantly faster, but takes 2kB of ram.
#define TABLE_BASED

//Assuming operating frequency of 40 MHz.
//It must have a BIAS.  If BIAS is toward 0, set ZERO_BIAS
#define ZERO_BIAS

void SendPacketData( const unsigned char * c, uint16_t len );

//0: Normal decode operation.
//1: Does not actually decode packet, rather places raw manchester data in payload.
//2: Do not receive any more packets for now.
//extern uint32_t current_packet[(MTU_BYTES+3)/4];
extern unsigned char ETbuffer[ETBUFFERSIZE] __attribute__((aligned(32)));
#define current_packet ETbuffer
extern int16_t current_packet_rec_place;

extern uint32_t packsrxraw;
extern uint8_t gotdma;
extern uint8_t gotlink;

//"newdata" must be in MSB first format.
void ResetPacketInternal( uint32_t first );
int8_t HandleWord32( uint32_t v ); //1 = Packet complete. Negative = Problem.  0 = Packet processing.
int8_t VerifyEtherlinkCRC(); //-1 = FAIL.  0 = PASS.

int8_t DecodePacket( uint32_t * pak, uint16_t len );

#endif

