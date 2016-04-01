//Copyright 2015-2016 <>< Charles Lohr, see LICENSE file.  This file is licensed under the 2-Clause BSD License.

#ifndef _MANCHESTRATE_H
#define _MANCHESTRATE_H

#include <c_types.h>
#include <eth_config.h>

//Assuming operating frequency of 40 MHz.
//It must have a BIAS.  If BIAS is toward 0, set ZERO_BIAS
#define ZERO_BIAS

void SendPacketData( const unsigned char * c, uint16_t len );

//0: Normal decode operation.
//1: Does not actually decode packet, rather places raw manchester data in payload.
//2: Do not receive any more packets for now.
extern uint8_t * current_packet;
extern int16_t current_packet_rec_place;
extern uint16_t rx_pack_lens[RXBUFS];
extern uint8_t  rx_pack_flags[RXBUFS]; //0 = unused, 1 = in transit, 2 = complete.
extern uint8_t rx_cur;

extern uint32_t packsrxraw;
extern uint8_t gotdma;
extern uint8_t gotlink;

//"newdata" must be in MSB first format.
int ResetPacketInternal( );  //First is currently unused.  If nonzero, failed.
int8_t VerifyEtherlinkCRC(); //-1 = FAIL.  0 = PASS.

//return vales:
// + = suspected error or end of packet.  Return is # of bytes read.
// 0 = Give me more data.
//Progress not updated on data continuation.
int32_t DecodePacket( uint32_t * pak, uint16_t len );

#endif

