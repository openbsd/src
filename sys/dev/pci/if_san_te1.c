/*	$OpenBSD: if_san_te1.c,v 1.5 2004/12/07 06:10:24 mcbride Exp $	*/

/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Alex Feldman <al.feldman@sangoma.com> for SAN.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sangoma Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANGOMA TECHNOLOGIES AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_sppp.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/udp.h>
#include <netinet/ip.h>

#include <dev/pci/if_san_common.h>



#define FIRST_SAMPLE	0
#define LAST_SAMPLE	23
#define FIRST_UI	0
#define LAST_UI		4

#define MAX_BUSY_READ	0x05

/* Enabling/Disabling register debugging */
#undef WAN_DEBUG_TE1_REG
#ifdef WAN_DEBUG_TE1_REG

#define TEST_REG(reg,value)						\
{									\
	unsigned char test_value = READ_REG(reg);			\
	if (test_value != value) {					\
		log(LOG_INFO, "%s:%d: Test failed!\n",			\
				__FILE__,__LINE__);			\
		log(LOG_INFO, "%s:%d: Reg=%02x, Val=%02x\n",		\
				__FILE__,__LINE__,reg, value);		\
	}								\
}

#define TEST_RPSC_REG(card,reg,channel,value)				\
{									\
	unsigned char test_value = ReadRPSCReg(card,channel,reg);	\
	if (test_value != value) {					\
		log(LOG_INFO, "%s:%d: RPSC REG Test failed!\n",		\
			__FILE__,__LINE__);				\
		log(LOG_INFO, "%s:%d: Reg=%02x,Channel=%d,Val=%02x!\n",	\
			__FILE__, __LINE__, reg, channel, value);	\
	}								\
}

#define TEST_TPSC_REG(card,reg,channel,value)				\
{									\
	unsigned char test_value = ReadTPSCReg(card,channel,reg);	\
	if (test_value != value) {					\
		log(LOG_INFO, "%s:%d: TPSC REG Test failed!\n",		\
			__FILE__,__LINE__);				\
		log(LOG_INFO, "%s:%d: Reg=%02x,Channel=%d,Val=%02x)!\n",\
			__FILE__, __LINE__, reg, channel, value);	\
	}								\
}

#else

#define TEST_REG(reg,value)
#define TEST_RPSC_REG(card,reg,channel,value)
#define TEST_TPSC_REG(card,reg,channel,value)

#endif

#define READ_RPSC_REG(reg,channel)	ReadRPSCReg(card,reg,channel)
#define READ_TPSC_REG(reg,channel)	ReadTPSCReg(card,reg,channel)
#define READ_SIGX_REG(reg,channel)	ReadSIGXReg(card,reg,channel)
#define WRITE_RPSC_REG(reg,channel,value)				\
	{								\
		WriteRPSCReg(card,reg,channel,(unsigned char)value);	\
		TEST_RPSC_REG(card,reg,channel,(unsigned char)value);	\
	}

#define WRITE_TPSC_REG(reg,channel,value)				\
	{								\
		WriteTPSCReg(card,reg,channel,(unsigned char)value);	\
		TEST_TPSC_REG(card,reg,channe,(unsigned char)value);	\
	}

#if 0
#define WRITE_SIGX_REG(reg,channel,value)				\
	{								\
		WriteSIGXReg(card,reg,channel,(unsigned char)value);	\
		TEST_SIGX_REG(card,reg,channel,(unsigned char)value);	\
	}
#endif

#define IS_T1_ALARM(alarm)		((alarm) &			\
						(			\
						 BIT_RED_ALARM |	\
						 BIT_AIS_ALARM |	\
						 BIT_YEL_ALARM		\
						 ))

#define IS_E1_ALARM(alarm)		((alarm) &			\
						(			\
						 BIT_RED_ALARM  |	\
						 BIT_AIS_ALARM  |	\
						 BIT_ALOS_ALARM		\
						 ))


typedef
unsigned char TX_WAVEFORM[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1];

typedef struct RLPS_EQUALIZER_RAM_T {
	/*unsigned char address;*/
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
	unsigned char byte4;
} RLPS_EQUALIZER_RAM;



/* Transmit Waveform Values for T1 Long Haul (LBO 0db)
** unsigned char t1_tx_waveform_lh_0db
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_lh_0db =
{
	{ 0x00, 0x44, 0x00, 0x00, 0x00 },
	{ 0x0A, 0x44, 0x00, 0x00, 0x00 },
	{ 0x20, 0x43, 0x00, 0x00, 0x00 },
	{ 0x32, 0x43, 0x00, 0x00, 0x00 },
	{ 0x3E, 0x42, 0x00, 0x00, 0x00 },
	{ 0x3D, 0x42, 0x00, 0x00, 0x00 },
	{ 0x3C, 0x41, 0x00, 0x00, 0x00 },
	{ 0x3B, 0x41, 0x00, 0x00, 0x00 },
	{ 0x3A, 0x00, 0x00, 0x00, 0x00 },
	{ 0x39, 0x00, 0x00, 0x00, 0x00 },
	{ 0x39, 0x00, 0x00, 0x00, 0x00 },
	{ 0x38, 0x00, 0x00, 0x00, 0x00 },
	{ 0x37, 0x00, 0x00, 0x00, 0x00 },
	{ 0x36, 0x00, 0x00, 0x00, 0x00 },
	{ 0x34, 0x00, 0x00, 0x00, 0x00 },
	{ 0x29, 0x00, 0x00, 0x00, 0x00 },
	{ 0x4F, 0x00, 0x00, 0x00, 0x00 },
	{ 0x4C, 0x00, 0x00, 0x00, 0x00 },
	{ 0x4A, 0x00, 0x00, 0x00, 0x00 },
	{ 0x49, 0x00, 0x00, 0x00, 0x00 },
	{ 0x47, 0x00, 0x00, 0x00, 0x00 },
	{ 0x47, 0x00, 0x00, 0x00, 0x00 },
	{ 0x46, 0x00, 0x00, 0x00, 0x00 },
	{ 0x46, 0x00, 0x00, 0x00, 0x00 }
};

/* Transmit Waveform Values for T1 Long Haul (LBO 7.5 dB): 
** unsigned char t1_tx_waveform_lh_75db
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_lh_75db =
{
    { 0x00, 0x10, 0x00, 0x00, 0x00 },
    { 0x01, 0x0E, 0x00, 0x00, 0x00 },
    { 0x02, 0x0C, 0x00, 0x00, 0x00 },
    { 0x04, 0x0A, 0x00, 0x00, 0x00 },
    { 0x08, 0x08, 0x00, 0x00, 0x00 },
    { 0x0C, 0x06, 0x00, 0x00, 0x00 },
    { 0x10, 0x04, 0x00, 0x00, 0x00 },
    { 0x16, 0x02, 0x00, 0x00, 0x00 },
    { 0x1A, 0x01, 0x00, 0x00, 0x00 },
    { 0x1E, 0x00, 0x00, 0x00, 0x00 },
    { 0x22, 0x00, 0x00, 0x00, 0x00 },
    { 0x26, 0x00, 0x00, 0x00, 0x00 },
    { 0x2A, 0x00, 0x00, 0x00, 0x00 },
    { 0x2B, 0x00, 0x00, 0x00, 0x00 },
    { 0x2C, 0x00, 0x00, 0x00, 0x00 },
    { 0x2D, 0x00, 0x00, 0x00, 0x00 },
    { 0x2C, 0x00, 0x00, 0x00, 0x00 },
    { 0x28, 0x00, 0x00, 0x00, 0x00 },
    { 0x24, 0x00, 0x00, 0x00, 0x00 },
    { 0x20, 0x00, 0x00, 0x00, 0x00 },
    { 0x1C, 0x00, 0x00, 0x00, 0x00 },
    { 0x18, 0x00, 0x00, 0x00, 0x00 },
    { 0x14, 0x00, 0x00, 0x00, 0x00 },
    { 0x12, 0x00, 0x00, 0x00, 0x00 }
};


/* Transmit Waveform Values for T1 Long Haul (LBO 15 dB)
** unsigned char t1_tx_waveform_lh_15db
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_lh_15db =
{
    { 0x00, 0x2A, 0x09, 0x01, 0x00 },
    { 0x00, 0x28, 0x08, 0x01, 0x00 },
    { 0x00, 0x26, 0x08, 0x01, 0x00 },
    { 0x00, 0x24, 0x07, 0x01, 0x00 },
    { 0x01, 0x22, 0x07, 0x01, 0x00 },
    { 0x02, 0x20, 0x06, 0x01, 0x00 },
    { 0x04, 0x1E, 0x06, 0x01, 0x00 },
    { 0x07, 0x1C, 0x05, 0x00, 0x00 },
    { 0x0A, 0x1B, 0x05, 0x00, 0x00 },
    { 0x0D, 0x19, 0x05, 0x00, 0x00 },
    { 0x10, 0x18, 0x04, 0x00, 0x00 },
    { 0x14, 0x16, 0x04, 0x00, 0x00 },
    { 0x18, 0x15, 0x04, 0x00, 0x00 },
    { 0x1B, 0x13, 0x03, 0x00, 0x00 },
    { 0x1E, 0x12, 0x03, 0x00, 0x00 },
    { 0x21, 0x10, 0x03, 0x00, 0x00 },
    { 0x24, 0x0F, 0x03, 0x00, 0x00 },
    { 0x27, 0x0D, 0x03, 0x00, 0x00 },
    { 0x2A, 0x0D, 0x02, 0x00, 0x00 },
    { 0x2D, 0x0B, 0x02, 0x00, 0x00 },
    { 0x30, 0x0B, 0x02, 0x00, 0x00 },
    { 0x30, 0x0A, 0x02, 0x00, 0x00 },
    { 0x2E, 0x0A, 0x02, 0x00, 0x00 },
    { 0x2C, 0x09, 0x02, 0x00, 0x00 }
};


/* Transmit Waveform Values for T1 Long Haul (LBO 22.5 dB)
** unsigned char t1_tx_waveform_lh_225db
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_lh_225db =
{
    { 0x00, 0x1F, 0x16, 0x06, 0x01 },
    { 0x00, 0x20, 0x15, 0x05, 0x01 },
    { 0x00, 0x21, 0x15, 0x05, 0x01 },
    { 0x00, 0x22, 0x14, 0x05, 0x01 },
    { 0x00, 0x22, 0x13, 0x04, 0x00 },
    { 0x00, 0x23, 0x12, 0x04, 0x00 },
    { 0x01, 0x23, 0x12, 0x04, 0x00 },
    { 0x01, 0x24, 0x11, 0x03, 0x00 },
    { 0x01, 0x23, 0x10, 0x03, 0x00 },
    { 0x02, 0x23, 0x10, 0x03, 0x00 },
    { 0x03, 0x22, 0x0F, 0x03, 0x00 },
    { 0x05, 0x22, 0x0E, 0x03, 0x00 },
    { 0x07, 0x21, 0x0E, 0x02, 0x00 },
    { 0x09, 0x20, 0x0D, 0x02, 0x00 },
    { 0x0B, 0x1E, 0x0C, 0x02, 0x00 },
    { 0x0E, 0x1D, 0x0C, 0x02, 0x00 },
    { 0x10, 0x1B, 0x0B, 0x02, 0x00 },
    { 0x13, 0x1B, 0x0A, 0x02, 0x00 },
    { 0x15, 0x1A, 0x0A, 0x02, 0x00 },
    { 0x17, 0x19, 0x09, 0x01, 0x00 },
    { 0x19, 0x19, 0x08, 0x01, 0x00 },
    { 0x1B, 0x18, 0x08, 0x01, 0x00 },
    { 0x1D, 0x17, 0x07, 0x01, 0x00 },
    { 0x1E, 0x17, 0x06, 0x01, 0x00 }
};


/* Transmit Waveform Values for T1 Short Haul (0 - 110 ft.)
** unsigned char t1_tx_waveform_sh_110ft
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_sh_110ft =
{
    { 0x00, 0x45, 0x00, 0x00, 0x00 },
    { 0x0A, 0x44, 0x00, 0x00, 0x00 },
    { 0x20, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x42, 0x00, 0x00, 0x00 },
    { 0x3F, 0x42, 0x00, 0x00, 0x00 },
    { 0x3C, 0x41, 0x00, 0x00, 0x00 },
    { 0x3B, 0x41, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x39, 0x00, 0x00, 0x00, 0x00 },
    { 0x39, 0x00, 0x00, 0x00, 0x00 },
    { 0x38, 0x00, 0x00, 0x00, 0x00 },
    { 0x37, 0x00, 0x00, 0x00, 0x00 },
    { 0x36, 0x00, 0x00, 0x00, 0x00 },
    { 0x34, 0x00, 0x00, 0x00, 0x00 },
    { 0x29, 0x00, 0x00, 0x00, 0x00 },
    { 0x59, 0x00, 0x00, 0x00, 0x00 },
    { 0x55, 0x00, 0x00, 0x00, 0x00 },
    { 0x50, 0x00, 0x00, 0x00, 0x00 },
    { 0x4D, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x48, 0x00, 0x00, 0x00, 0x00 },
    { 0x46, 0x00, 0x00, 0x00, 0x00 },
    { 0x46, 0x00, 0x00, 0x00, 0x00 }
};


/* Transmit Waveform Values for T1 Short Haul (110 - 220 ft.)
** unsigned char t1_tx_waveform_sh_220ft
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_sh_220ft =
{
    { 0x00, 0x44, 0x00, 0x00, 0x00 },
    { 0x0A, 0x44, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x36, 0x42, 0x00, 0x00, 0x00 },
    { 0x34, 0x42, 0x00, 0x00, 0x00 },
    { 0x30, 0x41, 0x00, 0x00, 0x00 },
    { 0x2F, 0x41, 0x00, 0x00, 0x00 },
    { 0x2E, 0x00, 0x00, 0x00, 0x00 },
    { 0x2D, 0x00, 0x00, 0x00, 0x00 },
    { 0x2C, 0x00, 0x00, 0x00, 0x00 },
    { 0x2B, 0x00, 0x00, 0x00, 0x00 },
    { 0x2A, 0x00, 0x00, 0x00, 0x00 },
    { 0x28, 0x00, 0x00, 0x00, 0x00 },
    { 0x26, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x68, 0x00, 0x00, 0x00, 0x00 },
    { 0x54, 0x00, 0x00, 0x00, 0x00 },
    { 0x4F, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x49, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x46, 0x00, 0x00, 0x00, 0x00 }
};


/* Transmit Waveform Values for T1 Short Haul (220 - 330 ft.)
** unsigned char t1_tx_waveform_sh_330ft
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_sh_330ft =
{
    { 0x00, 0x44, 0x00, 0x00, 0x00 },
    { 0x0A, 0x44, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3A, 0x43, 0x00, 0x00, 0x00 },
    { 0x3A, 0x42, 0x00, 0x00, 0x00 },
    { 0x38, 0x42, 0x00, 0x00, 0x00 },
    { 0x30, 0x41, 0x00, 0x00, 0x00 },
    { 0x2F, 0x41, 0x00, 0x00, 0x00 },
    { 0x2E, 0x00, 0x00, 0x00, 0x00 },
    { 0x2D, 0x00, 0x00, 0x00, 0x00 },
    { 0x2C, 0x00, 0x00, 0x00, 0x00 },
    { 0x2B, 0x00, 0x00, 0x00, 0x00 },
    { 0x2A, 0x00, 0x00, 0x00, 0x00 },
    { 0x29, 0x00, 0x00, 0x00, 0x00 },
    { 0x23, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x6C, 0x00, 0x00, 0x00, 0x00 },
    { 0x60, 0x00, 0x00, 0x00, 0x00 },
    { 0x4F, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x49, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x46, 0x00, 0x00, 0x00, 0x00 }
};


/* Transmit Waveform Values for T1 Short Haul (330 - 440 ft.)
** unsigned char t1_tx_waveform_sh_440ft
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_sh_440ft =
{
    { 0x00, 0x44, 0x00, 0x00, 0x00 },
    { 0x0A, 0x44, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x42, 0x00, 0x00, 0x00 },
    { 0x3F, 0x42, 0x00, 0x00, 0x00 },
    { 0x2F, 0x41, 0x00, 0x00, 0x00 },
    { 0x2E, 0x41, 0x00, 0x00, 0x00 },
    { 0x2D, 0x00, 0x00, 0x00, 0x00 },
    { 0x2C, 0x00, 0x00, 0x00, 0x00 },
    { 0x2B, 0x00, 0x00, 0x00, 0x00 },
    { 0x2A, 0x00, 0x00, 0x00, 0x00 },
    { 0x29, 0x00, 0x00, 0x00, 0x00 },
    { 0x28, 0x00, 0x00, 0x00, 0x00 },
    { 0x19, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x7F, 0x00, 0x00, 0x00, 0x00 },
    { 0x60, 0x00, 0x00, 0x00, 0x00 },
    { 0x4F, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x49, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x46, 0x00, 0x00, 0x00, 0x00 }
};


/* Transmit Waveform Values for T1 Short Haul (440 - 550 ft.)
** unsigned char t1_tx_waveform_sh_550ft
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_sh_550ft =
{
    { 0x00, 0x44, 0x00, 0x00, 0x00 },
    { 0x0A, 0x44, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x42, 0x00, 0x00, 0x00 },
    { 0x3F, 0x42, 0x00, 0x00, 0x00 },
    { 0x30, 0x41, 0x00, 0x00, 0x00 },
    { 0x2B, 0x41, 0x00, 0x00, 0x00 },
    { 0x2A, 0x00, 0x00, 0x00, 0x00 },
    { 0x29, 0x00, 0x00, 0x00, 0x00 },
    { 0x28, 0x00, 0x00, 0x00, 0x00 },
    { 0x27, 0x00, 0x00, 0x00, 0x00 },
    { 0x26, 0x00, 0x00, 0x00, 0x00 },
    { 0x26, 0x00, 0x00, 0x00, 0x00 },
    { 0x24, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x7F, 0x00, 0x00, 0x00, 0x00 },
    { 0x7F, 0x00, 0x00, 0x00, 0x00 },
    { 0x4F, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x49, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x46, 0x00, 0x00, 0x00, 0x00 }
};


/* Transmit Waveform Values for T1 Short Haul (550 - 660 ft.)
** unsigned char t1_tx_waveform_sh_660ft
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM t1_tx_waveform_sh_660ft =
{
    { 0x00, 0x44, 0x00, 0x00, 0x00 },
    { 0x0A, 0x44, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x43, 0x00, 0x00, 0x00 },
    { 0x3F, 0x42, 0x00, 0x00, 0x00 },
    { 0x3F, 0x42, 0x00, 0x00, 0x00 },
    { 0x3F, 0x41, 0x00, 0x00, 0x00 },
    { 0x30, 0x41, 0x00, 0x00, 0x00 },
    { 0x2A, 0x00, 0x00, 0x00, 0x00 },
    { 0x29, 0x00, 0x00, 0x00, 0x00 },
    { 0x28, 0x00, 0x00, 0x00, 0x00 },
    { 0x27, 0x00, 0x00, 0x00, 0x00 },
    { 0x26, 0x00, 0x00, 0x00, 0x00 },
    { 0x25, 0x00, 0x00, 0x00, 0x00 },
    { 0x24, 0x00, 0x00, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00, 0x00, 0x00 },
    { 0x7F, 0x00, 0x00, 0x00, 0x00 },
    { 0x7F, 0x00, 0x00, 0x00, 0x00 },
    { 0x5F, 0x00, 0x00, 0x00, 0x00 },
    { 0x50, 0x00, 0x00, 0x00, 0x00 },
    { 0x49, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x47, 0x00, 0x00, 0x00, 0x00 },
    { 0x46, 0x00, 0x00, 0x00, 0x00 }
};


/* Transmit Waveform Values for E1 120 Ohm
** unsigned char e1_tx_waveform_120
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM e1_tx_waveform_120 =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x0A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3F, 0x00, 0x00, 0x00, 0x00 },
    { 0x3F, 0x00, 0x00, 0x00, 0x00 },
    { 0x39, 0x00, 0x00, 0x00, 0x00 },
    { 0x38, 0x00, 0x00, 0x00, 0x00 },
    { 0x36, 0x00, 0x00, 0x00, 0x00 },
    { 0x36, 0x00, 0x00, 0x00, 0x00 },
    { 0x35, 0x00, 0x00, 0x00, 0x00 },
    { 0x35, 0x00, 0x00, 0x00, 0x00 },
    { 0x35, 0x00, 0x00, 0x00, 0x00 },
    { 0x35, 0x00, 0x00, 0x00, 0x00 },
    { 0x35, 0x00, 0x00, 0x00, 0x00 },
    { 0x35, 0x00, 0x00, 0x00, 0x00 },
    { 0x2D, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 }
};


/* Transmit Waveform Values for E1 75 Ohm
** unsigned char e1_tx_waveform_75
**		[LAST_SAMPLE-FIRST_SAMPLE+1][LAST_UI-FIRST_UI+1] = */
TX_WAVEFORM e1_tx_waveform_75 =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x0A, 0x00, 0x00, 0x00, 0x00 },
    { 0x28, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x3A, 0x00, 0x00, 0x00, 0x00 },
    { 0x32, 0x00, 0x00, 0x00, 0x00 },
    { 0x14, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00 }
};


RLPS_EQUALIZER_RAM t1_rlps_ram_table[] =
{
    { 0x03, 0xFE, 0x18, 0x40 },
    { 0x03, 0xF6, 0x18, 0x40 },
    { 0x03, 0xEE, 0x18, 0x40 },
    { 0x03, 0xE6, 0x18, 0x40 },
    { 0x03, 0xDE, 0x18, 0x40 },
    { 0x03, 0xD6, 0x18, 0x40 },
    { 0x03, 0xD6, 0x18, 0x40 },
    { 0x03, 0xD6, 0x18, 0x40 },
    { 0x03, 0xCE, 0x18, 0x40 },
    { 0x03, 0xCE, 0x18, 0x40 },
    { 0x03, 0xCE, 0x18, 0x40 },
    { 0x03, 0xCE, 0x18, 0x40 },
    { 0x03, 0xC6, 0x18, 0x40 },
    { 0x03, 0xC6, 0x18, 0x40 },
    { 0x03, 0xC6, 0x18, 0x40 },
    { 0x0B, 0xBE, 0x18, 0x40 },
    { 0x0B, 0xBE, 0x18, 0x40 },
    { 0x0B, 0xBE, 0x18, 0x40 },
    { 0x0B, 0xBE, 0x18, 0x40 },
    { 0x0B, 0xB6, 0x18, 0x40 },
    { 0x0B, 0xB6, 0x18, 0x40 },
    { 0x0B, 0xB6, 0x18, 0x40 },
    { 0x0B, 0xB6, 0x18, 0x40 },
    { 0x13, 0xAE, 0x18, 0x38 },
    { 0x13, 0xAE, 0x18, 0x3C },
    { 0x13, 0xAE, 0x18, 0x40 },
    { 0x13, 0xAE, 0x18, 0x40 },
    { 0x13, 0xAE, 0x18, 0x40 },
    { 0x13, 0xAE, 0x18, 0x40 },
    { 0x1B, 0xB6, 0x18, 0xB8 },
    { 0x1B, 0xAE, 0x18, 0xB8 },
    { 0x1B, 0xAE, 0x18, 0xBC },
    { 0x1B, 0xAE, 0x18, 0xC0 },
    { 0x1B, 0xAE, 0x18, 0xC0 },
    { 0x23, 0xA6, 0x18, 0xC0 },
    { 0x23, 0xA6, 0x18, 0xC0 },
    { 0x23, 0xA6, 0x18, 0xC0 },
    { 0x23, 0xA6, 0x18, 0xC0 },
    { 0x23, 0xA6, 0x18, 0xC0 },
    { 0x23, 0x9E, 0x18, 0xC0 },
    { 0x23, 0x9E, 0x18, 0xC0 },
    { 0x23, 0x9E, 0x18, 0xC0 },
    { 0x23, 0x9E, 0x18, 0xC0 },
    { 0x23, 0x9E, 0x18, 0xC0 },
    { 0x2B, 0x96, 0x18, 0xC0 },
    { 0x2B, 0x96, 0x18, 0xC0 },
    { 0x2B, 0x96, 0x18, 0xC0 },
    { 0x33, 0x96, 0x19, 0x40 },
    { 0x37, 0x96, 0x19, 0x40 },
    { 0x37, 0x96, 0x19, 0x40 },
    { 0x37, 0x96, 0x19, 0x40 },
    { 0x3F, 0x9E, 0x19, 0xC0 },
    { 0x3F, 0x9E, 0x19, 0xC0 },
    { 0x3F, 0x9E, 0x19, 0xC0 },
    { 0x3F, 0xA6, 0x1A, 0x40 },
    { 0x3F, 0xA6, 0x1A, 0x40 },
    { 0x3F, 0xA6, 0x1A, 0x40 },
    { 0x3F, 0xA6, 0x1A, 0x40 },
    { 0x3F, 0x96, 0x19, 0xC0 },
    { 0x3F, 0x96, 0x19, 0xC0 },
    { 0x3F, 0x96, 0x19, 0xC0 },
    { 0x3F, 0x96, 0x19, 0xC0 },
    { 0x47, 0x9E, 0x1A, 0x40 },
    { 0x47, 0x9E, 0x1A, 0x40 },
    { 0x47, 0x9E, 0x1A, 0x40 },
    { 0x47, 0x96, 0x1A, 0x40 },
    { 0x47, 0x96, 0x1A, 0x40 },
    { 0x47, 0x96, 0x1A, 0x40 },
    { 0x47, 0x96, 0x1A, 0x40 },
    { 0x4F, 0x8E, 0x1A, 0x40 },
    { 0x4F, 0x8E, 0x1A, 0x40 },
    { 0x4F, 0x8E, 0x1A, 0x40 },
    { 0x4F, 0x8E, 0x1A, 0x40 },
    { 0x4F, 0x8E, 0x1A, 0x40 },
    { 0x57, 0x86, 0x1A, 0x40 },
    { 0x57, 0x86, 0x1A, 0x40 },
    { 0x57, 0x86, 0x1A, 0x40 },
    { 0x57, 0x86, 0x1A, 0x40 },
    { 0x57, 0x86, 0x1A, 0x40 },
    { 0x5F, 0x86, 0x1A, 0xC0 },
    { 0x5F, 0x86, 0x1A, 0xC0 },
    { 0x5F, 0x86, 0x1A, 0xC0 },
    { 0x5F, 0x86, 0x1A, 0xC0 },
    { 0x5F, 0x86, 0x1A, 0xC0 },
    { 0x5F, 0x86, 0x1A, 0xC0 },
    { 0x5F, 0x7E, 0x1A, 0xC0 },
    { 0x5F, 0x7E, 0x1A, 0xC0 },
    { 0x5F, 0x7E, 0x1A, 0xC0 },
    { 0x5F, 0x7E, 0x1A, 0xC0 },
    { 0x5F, 0x7E, 0x1A, 0xC0 },
    { 0x67, 0x7E, 0x2A, 0xC0 },
    { 0x67, 0x7E, 0x2A, 0xC0 },
    { 0x67, 0x7E, 0x2A, 0xC0 },
    { 0x67, 0x7E, 0x2A, 0xC0 },
    { 0x67, 0x76, 0x2A, 0xC0 },
    { 0x67, 0x76, 0x2A, 0xC0 },
    { 0x67, 0x76, 0x2A, 0xC0 },
    { 0x67, 0x76, 0x2A, 0xC0 },
    { 0x67, 0x76, 0x2A, 0xC0 },
    { 0x6F, 0x6E, 0x2A, 0xC0 },
    { 0x6F, 0x6E, 0x2A, 0xC0 },
    { 0x6F, 0x6E, 0x2A, 0xC0 },
    { 0x6F, 0x6E, 0x2A, 0xC0 },
    { 0x77, 0x6E, 0x3A, 0xC0 },
    { 0x77, 0x6E, 0x3A, 0xC0 },
    { 0x77, 0x6E, 0x3A, 0xC0 },
    { 0x77, 0x6E, 0x3A, 0xC0 },
    { 0x7F, 0x66, 0x3A, 0xC0 },
    { 0x7F, 0x66, 0x3A, 0xC0 },
    { 0x7F, 0x66, 0x4A, 0xC0 },
    { 0x7F, 0x66, 0x4A, 0xC0 },
    { 0x7F, 0x66, 0x4A, 0xC0 },
    { 0x7F, 0x66, 0x4A, 0xC0 },
    { 0x87, 0x66, 0x5A, 0xC0 },
    { 0x87, 0x66, 0x5A, 0xC0 },
    { 0x87, 0x66, 0x5A, 0xC0 },
    { 0x87, 0x66, 0x5A, 0xC0 },
    { 0x87, 0x66, 0x5A, 0xC0 },
    { 0x87, 0x5E, 0x5A, 0xC0 },
    { 0x87, 0x5E, 0x5A, 0xC0 },
    { 0x87, 0x5E, 0x5A, 0xC0 },
    { 0x87, 0x5E, 0x5A, 0xC0 },
    { 0x87, 0x5E, 0x5A, 0xC0 },
    { 0x8F, 0x5E, 0x6A, 0xC0 },
    { 0x8F, 0x5E, 0x6A, 0xC0 },
    { 0x8F, 0x5E, 0x6A, 0xC0 },
    { 0x8F, 0x5E, 0x6A, 0xC0 },
    { 0x97, 0x5E, 0x7A, 0xC0 },
    { 0x97, 0x5E, 0x7A, 0xC0 },
    { 0x97, 0x5E, 0x7A, 0xC0 },
    { 0x97, 0x5E, 0x7A, 0xC0 },
    { 0x9F, 0x5E, 0x8A, 0xC0 },
    { 0x9F, 0x5E, 0x8A, 0xC0 },
    { 0x9F, 0x5E, 0x8A, 0xC0 },
    { 0x9F, 0x5E, 0x8A, 0xC0 },
    { 0x9F, 0x5E, 0x8A, 0xC0 },
    { 0xA7, 0x56, 0x9A, 0xC0 },
    { 0xA7, 0x56, 0x9A, 0xC0 },
    { 0xA7, 0x56, 0x9A, 0xC0 },
    { 0xA7, 0x56, 0x9A, 0xC0 },
    { 0xA7, 0x56, 0xAA, 0xC0 },
    { 0xA7, 0x56, 0xAA, 0xC0 },
    { 0xA7, 0x56, 0xAA, 0xC0 },
    { 0xAF, 0x4E, 0xAA, 0xC0 },
    { 0xAF, 0x4E, 0xAA, 0xC0 },
    { 0xAF, 0x4E, 0xAA, 0xC0 },
    { 0xAF, 0x4E, 0xAA, 0xC0 },
    { 0xAF, 0x4E, 0xAA, 0xC0 },
    { 0xB7, 0x46, 0xAA, 0xC0 },
    { 0xB7, 0x46, 0xAA, 0xC0 },
    { 0xB7, 0x46, 0xAA, 0xC0 },
    { 0xB7, 0x46, 0xAA, 0xC0 },
    { 0xB7, 0x46, 0xAA, 0xC0 },
    { 0xB7, 0x46, 0xAA, 0xC0 },
    { 0xB7, 0x46, 0xAA, 0xC0 },
    { 0xB7, 0x46, 0xBA, 0xC0 },
    { 0xB7, 0x46, 0xBA, 0xC0 },
    { 0xB7, 0x46, 0xBA, 0xC0 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBF, 0x4E, 0xBB, 0x40 },
    { 0xBE, 0x46, 0xCB, 0x40 },
    { 0xBE, 0x46, 0xCB, 0x40 },
    { 0xBE, 0x46, 0xCB, 0x40 },
    { 0xBE, 0x46, 0xCB, 0x40 },
    { 0xBE, 0x46, 0xCB, 0x40 },
    { 0xBE, 0x46, 0xCB, 0x40 },
    { 0xBE, 0x46, 0xDB, 0x40 },
    { 0xBE, 0x46, 0xDB, 0x40 },
    { 0xBE, 0x46, 0xDB, 0x40 },
    { 0xC6, 0x3E, 0xCB, 0x40 },
    { 0xC6, 0x3E, 0xCB, 0x40 },
    { 0xC6, 0x3E, 0xDB, 0x40 },
    { 0xC6, 0x3E, 0xDB, 0x40 },
    { 0xC6, 0x3E, 0xDB, 0x40 },
    { 0xC6, 0x44, 0xDB, 0x40 },
    { 0xC6, 0x44, 0xDB, 0x40 },
    { 0xC6, 0x44, 0xDB, 0x40 },
    { 0xC6, 0x44, 0xDB, 0x40 },
    { 0xC6, 0x3C, 0xDB, 0x40 },
    { 0xC6, 0x3C, 0xDB, 0x40 },
    { 0xC6, 0x3C, 0xDB, 0x40 },
    { 0xC6, 0x3C, 0xDB, 0x40 },
    { 0xD6, 0x34, 0xDB, 0x40 },
    { 0xD6, 0x34, 0xDB, 0x40 },
    { 0xD6, 0x34, 0xDB, 0x40 },
    { 0xD6, 0x34, 0xDB, 0x40 },
    { 0xD6, 0x34, 0xDB, 0x40 },
    { 0xDE, 0x2C, 0xDB, 0x3C },
    { 0xDE, 0x2C, 0xDB, 0x3C },
    { 0xDE, 0x2C, 0xDB, 0x3C },
    { 0xE6, 0x2C, 0xDB, 0x40 },
    { 0xE6, 0x2C, 0xDB, 0x40 },
    { 0xE6, 0x2C, 0xDB, 0x40 },
    { 0xE6, 0x2C, 0xDB, 0x40 },
    { 0xE6, 0x2C, 0xDB, 0x40 },
    { 0xE6, 0x2C, 0xEB, 0x40 },
    { 0xE6, 0x2C, 0xEB, 0x40 },
    { 0xE6, 0x2C, 0xEB, 0x40 },
    { 0xEE, 0x2C, 0xFB, 0x40 },
    { 0xEE, 0x2C, 0xFB, 0x40 },
    { 0xEE, 0x2C, 0xFB, 0x40 },
    { 0xEE, 0x2D, 0x0B, 0x40 },
    { 0xEE, 0x2D, 0x0B, 0x40 },
    { 0xEE, 0x2D, 0x0B, 0x40 },
    { 0xEE, 0x2D, 0x0B, 0x40 },
    { 0xEE, 0x2D, 0x0B, 0x40 },
    { 0xF5, 0x25, 0x0B, 0x38 },
    { 0xF5, 0x25, 0x0B, 0x3C },
    { 0xF5, 0x25, 0x0B, 0x40 },
    { 0xF5, 0x25, 0x1B, 0x40 },
    { 0xF5, 0x25, 0x1B, 0x40 },
    { 0xF5, 0x25, 0x1B, 0x40 },
    { 0xF5, 0x25, 0x1B, 0x40 },
    { 0xF5, 0x25, 0x1B, 0x40 },
    { 0xFD, 0x25, 0x2B, 0x40 },
    { 0xFD, 0x25, 0x2B, 0x40 },
    { 0xFD, 0x25, 0x2B, 0x40 },
    { 0xFD, 0x25, 0x2B, 0x40 },
    { 0xFD, 0x25, 0x27, 0x40 },
    { 0xFD, 0x25, 0x27, 0x40 },
    { 0xFD, 0x25, 0x27, 0x40 },
    { 0xFD, 0x25, 0x23, 0x40 },
    { 0xFD, 0x25, 0x23, 0x40 },
    { 0xFD, 0x25, 0x23, 0x40 },
    { 0xFD, 0x25, 0x33, 0x40 },
    { 0xFD, 0x25, 0x33, 0x40 },
    { 0xFD, 0x25, 0x33, 0x40 },
    { 0xFD, 0x25, 0x33, 0x40 },
    { 0xFD, 0x25, 0x33, 0x40 },
    { 0xFD, 0x25, 0x33, 0x40 },
    { 0xFC, 0x25, 0x33, 0x40 },
    { 0xFC, 0x25, 0x33, 0x40 },
    { 0xFC, 0x25, 0x43, 0x40 },
    { 0xFC, 0x25, 0x43, 0x40 },
    { 0xFC, 0x25, 0x43, 0x40 },
    { 0xFC, 0x25, 0x43, 0x44 },
    { 0xFC, 0x25, 0x43, 0x48 },
    { 0xFC, 0x25, 0x43, 0x4C },
    { 0xFC, 0x25, 0x43, 0xBC },
    { 0xFC, 0x25, 0x43, 0xC0 },
    { 0xFC, 0x25, 0x43, 0xC0 },
    { 0xFC, 0x23, 0x43, 0xC0 },
    { 0xFC, 0x23, 0x43, 0xC0 },
    { 0xFC, 0x23, 0x43, 0xC0 },
    { 0xFC, 0x21, 0x43, 0xC0 },
    { 0xFC, 0x21, 0x43, 0xC0 },
    { 0xFC, 0x21, 0x53, 0xC0 },
    { 0xFC, 0x21, 0x53, 0xC0 },
    { 0xFC, 0x21, 0x53, 0xC0 }
};

RLPS_EQUALIZER_RAM t1_rlps_perf_mode_ram_table[] =
{
	{ 0x03, 0xFE, 0x18, 0x40 },
	{ 0x03, 0xFE, 0x18, 0x40 },
	{ 0x03, 0xFE, 0x18, 0x40 },
	{ 0x03, 0xFE, 0x18, 0x40 },
	{ 0x03, 0xFE, 0x18, 0x40 },
	{ 0x03, 0xFE, 0x18, 0x40 },
	{ 0x03, 0xFE, 0x18, 0x40 },
	{ 0x03, 0xFE, 0x18, 0x40 },
	{ 0x03, 0xF6, 0x18, 0x40 },
	{ 0x03, 0xF6, 0x18, 0x40 },
	{ 0x03, 0xF6, 0x18, 0x40 },
	{ 0x03, 0xF6, 0x18, 0x40 },
	{ 0x03, 0xF6, 0x18, 0x40 },
	{ 0x03, 0xF6, 0x18, 0x40 },
	{ 0x03, 0xF6, 0x18, 0x40 },
	{ 0x03, 0xF6, 0x18, 0x40 },
	{ 0x03, 0xEE, 0x18, 0x40 },
	{ 0x03, 0xEE, 0x18, 0x40 },
	{ 0x03, 0xEE, 0x18, 0x40 },
	{ 0x03, 0xEE, 0x18, 0x40 },
	{ 0x03, 0xEE, 0x18, 0x40 },
	{ 0x03, 0xEE, 0x18, 0x40 },
	{ 0x03, 0xEE, 0x18, 0x40 },
	{ 0x03, 0xEE, 0x18, 0x40 },
	{ 0x03, 0xE6, 0x18, 0x40 },
	{ 0x03, 0xE6, 0x18, 0x40 },
	{ 0x03, 0xE6, 0x18, 0x40 },
	{ 0x03, 0xE6, 0x18, 0x40 },
	{ 0x03, 0xE6, 0x18, 0x40 },
	{ 0x03, 0xE6, 0x18, 0x40 },
	{ 0x03, 0xE6, 0x18, 0x40 },
	{ 0x03, 0xE6, 0x18, 0x40 },
	{ 0x03, 0xDE, 0x18, 0x40 },
	{ 0x03, 0xDE, 0x18, 0x40 },
	{ 0x03, 0xDE, 0x18, 0x40 },
	{ 0x03, 0xDE, 0x18, 0x40 },
	{ 0x03, 0xDE, 0x18, 0x40 },
	{ 0x03, 0xDE, 0x18, 0x40 },
	{ 0x03, 0xDE, 0x18, 0x40 },
	{ 0x03, 0xDE, 0x18, 0x40 },
	{ 0x03, 0xD6, 0x18, 0x40 },
	{ 0x03, 0xD6, 0x18, 0x40 },
	{ 0x03, 0xD6, 0x18, 0x40 },
	{ 0x03, 0xD6, 0x18, 0x40 },
	{ 0x03, 0xD6, 0x18, 0x40 },
	{ 0x03, 0xD6, 0x18, 0x40 },
	{ 0x03, 0xD6, 0x18, 0x40 },
	{ 0x03, 0xD6, 0x18, 0x40 },
	{ 0x03, 0xCE, 0x18, 0x40 },
	{ 0x03, 0xCE, 0x18, 0x40 },
	{ 0x03, 0xCE, 0x18, 0x40 },
	{ 0x03, 0xCE, 0x18, 0x40 },
	{ 0x03, 0xCE, 0x18, 0x40 },
	{ 0x03, 0xCE, 0x18, 0x40 },
	{ 0x03, 0xCE, 0x18, 0x40 },
	{ 0x03, 0xCE, 0x18, 0x40 },
	{ 0x03, 0xC6, 0x18, 0x40 },
	{ 0x03, 0xC6, 0x18, 0x40 },
	{ 0x03, 0xC6, 0x18, 0x40 },
	{ 0x03, 0xC6, 0x18, 0x40 },
	{ 0x03, 0xC6, 0x18, 0x40 },
	{ 0x03, 0xC6, 0x18, 0x40 },
	{ 0x03, 0xC6, 0x18, 0x40 },
	{ 0x03, 0xC6, 0x18, 0x40 },
	{ 0x03, 0xBE, 0x18, 0x40 },
	{ 0x03, 0xBE, 0x18, 0x40 },
	{ 0x03, 0xBE, 0x18, 0x40 },
	{ 0x03, 0xBE, 0x18, 0x40 },
	{ 0x03, 0xBE, 0x18, 0x40 },
	{ 0x03, 0xBE, 0x18, 0x40 },
	{ 0x03, 0xBE, 0x18, 0x40 },
	{ 0x03, 0xBE, 0x18, 0x40 },
	{ 0x03, 0xB6, 0x18, 0x40 },
	{ 0x03, 0xB6, 0x18, 0x40 },
	{ 0x03, 0xB6, 0x18, 0x40 },
	{ 0x03, 0xB6, 0x18, 0x40 },
	{ 0x03, 0xB6, 0x18, 0x40 },
	{ 0x03, 0xB6, 0x18, 0x40 },
	{ 0x03, 0xB6, 0x18, 0x40 },
	{ 0x03, 0xB6, 0x18, 0x40 },
	{ 0x03, 0xA6, 0x18, 0x40 },
	{ 0x03, 0xA6, 0x18, 0x40 },
	{ 0x03, 0xA6, 0x18, 0x40 },
	{ 0x03, 0xA6, 0x18, 0x40 },
	{ 0x03, 0xA6, 0x18, 0x40 },
	{ 0x03, 0xA6, 0x18, 0x40 },
	{ 0x03, 0xA6, 0x18, 0x40 },
	{ 0x03, 0xA6, 0x18, 0x40 },
	{ 0x03, 0x9E, 0x18, 0x40 },
	{ 0x03, 0x9E, 0x18, 0x40 },
	{ 0x03, 0x9E, 0x18, 0x40 },
	{ 0x03, 0x9E, 0x18, 0x40 },
	{ 0x03, 0x9E, 0x18, 0x40 },
	{ 0x03, 0x9E, 0x18, 0x40 },
	{ 0x03, 0x9E, 0x18, 0x40 },
	{ 0x03, 0x9E, 0x18, 0x40 },
	{ 0x03, 0x96, 0x18, 0x40 },
	{ 0x03, 0x96, 0x18, 0x40 },
	{ 0x03, 0x96, 0x18, 0x40 },
	{ 0x03, 0x96, 0x18, 0x40 },
	{ 0x03, 0x96, 0x18, 0x40 },
	{ 0x03, 0x96, 0x18, 0x40 },
	{ 0x03, 0x96, 0x18, 0x40 },
	{ 0x03, 0x96, 0x18, 0x40 },
	{ 0x03, 0x8E, 0x18, 0x40 },
	{ 0x03, 0x8E, 0x18, 0x40 },
	{ 0x03, 0x8E, 0x18, 0x40 },
	{ 0x03, 0x8E, 0x18, 0x40 },
	{ 0x03, 0x8E, 0x18, 0x40 },
	{ 0x03, 0x8E, 0x18, 0x40 },
	{ 0x03, 0x8E, 0x18, 0x40 },
	{ 0x03, 0x8E, 0x18, 0x40 },
	{ 0x03, 0x86, 0x18, 0x40 },
	{ 0x03, 0x86, 0x18, 0x40 },
	{ 0x03, 0x86, 0x18, 0x40 },
	{ 0x03, 0x86, 0x18, 0x40 },
	{ 0x03, 0x86, 0x18, 0x40 },
	{ 0x03, 0x86, 0x18, 0x40 },
	{ 0x03, 0x86, 0x18, 0x40 },
	{ 0x03, 0x86, 0x18, 0x40 },
	{ 0x03, 0x7E, 0x18, 0x40 },
	{ 0x03, 0x7E, 0x18, 0x40 },
	{ 0x03, 0x7E, 0x18, 0x40 },
	{ 0x03, 0x7E, 0x18, 0x40 },
	{ 0x03, 0x7E, 0x18, 0x40 },
	{ 0x03, 0x7E, 0x18, 0x40 },
	{ 0x03, 0x7E, 0x18, 0x40 },
	{ 0x03, 0x7E, 0x18, 0x40 },
	{ 0x03, 0x76, 0x18, 0x40 },
	{ 0x03, 0x76, 0x18, 0x40 },
	{ 0x03, 0x76, 0x18, 0x40 },
	{ 0x03, 0x76, 0x18, 0x40 },
	{ 0x03, 0x76, 0x18, 0x40 },
	{ 0x03, 0x76, 0x18, 0x40 },
	{ 0x03, 0x76, 0x18, 0x40 },
	{ 0x03, 0x76, 0x18, 0x40 },
	{ 0x03, 0x6E, 0x18, 0x40 },
	{ 0x03, 0x6E, 0x18, 0x40 },
	{ 0x03, 0x6E, 0x18, 0x40 },
	{ 0x03, 0x6E, 0x18, 0x40 },
	{ 0x03, 0x6E, 0x18, 0x40 },
	{ 0x03, 0x6E, 0x18, 0x40 },
	{ 0x03, 0x6E, 0x18, 0x40 },
	{ 0x03, 0x6E, 0x18, 0x40 },
	{ 0x03, 0x66, 0x18, 0x40 },
	{ 0x03, 0x66, 0x18, 0x40 },
	{ 0x03, 0x66, 0x18, 0x40 },
	{ 0x03, 0x66, 0x18, 0x40 },
	{ 0x03, 0x66, 0x18, 0x40 },
	{ 0x03, 0x66, 0x18, 0x40 },
	{ 0x03, 0x66, 0x18, 0x40 },
	{ 0x03, 0x66, 0x18, 0x40 },
	{ 0x03, 0x5E, 0x18, 0x40 },
	{ 0x03, 0x5E, 0x18, 0x40 },
	{ 0x03, 0x5E, 0x18, 0x40 },
	{ 0x03, 0x5E, 0x18, 0x40 },
	{ 0x03, 0x5E, 0x18, 0x40 },
	{ 0x03, 0x5E, 0x18, 0x40 },
	{ 0x03, 0x5E, 0x18, 0x40 },
	{ 0x03, 0x5E, 0x18, 0x40 },
	{ 0x03, 0x56, 0x18, 0x40 },
	{ 0x03, 0x56, 0x18, 0x40 },
	{ 0x03, 0x56, 0x18, 0x40 },
	{ 0x03, 0x56, 0x18, 0x40 },
	{ 0x03, 0x56, 0x18, 0x40 },
	{ 0x03, 0x56, 0x18, 0x40 },
	{ 0x03, 0x56, 0x18, 0x40 },
	{ 0x03, 0x56, 0x18, 0x40 },
	{ 0x03, 0x4E, 0x18, 0x40 },
	{ 0x03, 0x4E, 0x18, 0x40 },
	{ 0x03, 0x4E, 0x18, 0x40 },
	{ 0x03, 0x4E, 0x18, 0x40 },
	{ 0x03, 0x4E, 0x18, 0x40 },
	{ 0x03, 0x4E, 0x18, 0x40 },
	{ 0x03, 0x4E, 0x18, 0x40 },
	{ 0x03, 0x4E, 0x18, 0x40 },
	{ 0x03, 0x46, 0x18, 0x40 },
	{ 0x03, 0x46, 0x18, 0x40 },
	{ 0x03, 0x46, 0x18, 0x40 },
	{ 0x03, 0x46, 0x18, 0x40 },
	{ 0x03, 0x46, 0x18, 0x40 },
	{ 0x03, 0x46, 0x18, 0x40 },
	{ 0x03, 0x46, 0x18, 0x40 },
	{ 0x03, 0x46, 0x18, 0x40 },
	{ 0x03, 0x3E, 0x18, 0x40 },
	{ 0x03, 0x3E, 0x18, 0x40 },
	{ 0x03, 0x3E, 0x18, 0x40 },
	{ 0x03, 0x3E, 0x18, 0x40 },
	{ 0x03, 0x3E, 0x18, 0x40 },
	{ 0x03, 0x3E, 0x18, 0x40 },
	{ 0x03, 0x3E, 0x18, 0x40 },
	{ 0x03, 0x3E, 0x18, 0x40 },
	{ 0x03, 0x36, 0x18, 0x40 },
	{ 0x03, 0x36, 0x18, 0x40 },
	{ 0x03, 0x36, 0x18, 0x40 },
	{ 0x03, 0x36, 0x18, 0x40 },
	{ 0x03, 0x36, 0x18, 0x40 },
	{ 0x03, 0x36, 0x18, 0x40 },
	{ 0x03, 0x36, 0x18, 0x40 },
	{ 0x03, 0x36, 0x18, 0x40 },
	{ 0x03, 0x2E, 0x18, 0x40 },
	{ 0x03, 0x2E, 0x18, 0x40 },
	{ 0x03, 0x2E, 0x18, 0x40 },
	{ 0x03, 0x2E, 0x18, 0x40 },
	{ 0x03, 0x2E, 0x18, 0x40 },
	{ 0x03, 0x2E, 0x18, 0x40 },
	{ 0x03, 0x2E, 0x18, 0x40 },
	{ 0x03, 0x2E, 0x18, 0x40 },
	{ 0x03, 0x26, 0x18, 0x40 },
	{ 0x03, 0x26, 0x18, 0x40 },
	{ 0x03, 0x26, 0x18, 0x40 },
	{ 0x03, 0x26, 0x18, 0x40 },
	{ 0x03, 0x26, 0x18, 0x40 },
	{ 0x03, 0x26, 0x18, 0x40 },
	{ 0x03, 0x26, 0x18, 0x40 },
	{ 0x03, 0x26, 0x18, 0x40 },
	{ 0x03, 0x1E, 0x18, 0x40 },
	{ 0x03, 0x1E, 0x18, 0x40 },
	{ 0x03, 0x1E, 0x18, 0x40 },
	{ 0x03, 0x1E, 0x18, 0x40 },
	{ 0x03, 0x1E, 0x18, 0x40 },
	{ 0x03, 0x1E, 0x18, 0x40 },
	{ 0x03, 0x1E, 0x18, 0x40 },
	{ 0x03, 0x1E, 0x18, 0x40 },
	{ 0x03, 0x16, 0x18, 0x40 },
	{ 0x03, 0x16, 0x18, 0x40 },
	{ 0x03, 0x16, 0x18, 0x40 },
	{ 0x03, 0x16, 0x18, 0x40 },
	{ 0x03, 0x16, 0x18, 0x40 },
	{ 0x03, 0x16, 0x18, 0x40 },
	{ 0x03, 0x16, 0x18, 0x40 },
	{ 0x03, 0x16, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x0E, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 },
	{ 0x03, 0x06, 0x18, 0x40 }
};

RLPS_EQUALIZER_RAM e1_rlps_ram_table[] =
{
    { 0x07, 0xDE, 0x18, 0x2C },
    { 0x07, 0xDE, 0x18, 0x2C },
    { 0x07, 0xD6, 0x18, 0x2C },
    { 0x07, 0xD6, 0x18, 0x2C },
    { 0x07, 0xD6, 0x18, 0x2C },
    { 0x07, 0xCE, 0x18, 0x2C },
    { 0x07, 0xCE, 0x18, 0x2C },
    { 0x07, 0xCE, 0x18, 0x2C },
    { 0x07, 0xC6, 0x18, 0x2C },
    { 0x07, 0xC6, 0x18, 0x2C },
    { 0x07, 0xC6, 0x18, 0x2C },
    { 0x07, 0xBE, 0x18, 0x2C },
    { 0x07, 0xBE, 0x18, 0x2C },
    { 0x07, 0xBE, 0x18, 0x2C },
    { 0x07, 0xBE, 0x18, 0x2C },
    { 0x07, 0xBE, 0x18, 0x2C },
    { 0x07, 0xB6, 0x18, 0x2C },
    { 0x07, 0xB6, 0x18, 0x2C },
    { 0x07, 0xB6, 0x18, 0x2C },
    { 0x07, 0xB6, 0x18, 0x2C },
    { 0x07, 0xB6, 0x18, 0x2C },
    { 0x07, 0xAE, 0x18, 0x2C },
    { 0x07, 0xAE, 0x18, 0x2C },
    { 0x07, 0xAE, 0x18, 0x2C },
    { 0x07, 0xAE, 0x18, 0x2C },
    { 0x07, 0xAE, 0x18, 0x2C },
    { 0x07, 0xB6, 0x18, 0xAC },
    { 0x07, 0xAE, 0x18, 0xAC },
    { 0x07, 0xAE, 0x18, 0xAC },
    { 0x07, 0xAE, 0x18, 0xAC },
    { 0x07, 0xAE, 0x18, 0xAC },
    { 0x07, 0xA6, 0x18, 0xAC },
    { 0x07, 0xA6, 0x18, 0xAC },
    { 0x07, 0xA6, 0x18, 0xAC },
    { 0x07, 0xA6, 0x18, 0xAC },
    { 0x07, 0x9E, 0x18, 0xAC },
    { 0x07, 0xA6, 0x19, 0x2C },
    { 0x07, 0xA6, 0x19, 0x2C },
    { 0x07, 0xA6, 0x19, 0x2C },
    { 0x0F, 0xA6, 0x19, 0x2C },
    { 0x0F, 0xA6, 0x19, 0x2C },
    { 0x0F, 0x9E, 0x19, 0x2C },
    { 0x0F, 0x9E, 0x19, 0x2C },
    { 0x0F, 0x9E, 0x19, 0x2C },
    { 0x17, 0x9E, 0x19, 0x2C },
    { 0x17, 0xA6, 0x19, 0xAC },
    { 0x17, 0x9E, 0x19, 0xAC },
    { 0x17, 0x9E, 0x19, 0xAC },
    { 0x17, 0x96, 0x19, 0xAC },
    { 0x1F, 0x96, 0x19, 0xAC },
    { 0x1F, 0x96, 0x19, 0xAC },
    { 0x1F, 0x8E, 0x19, 0xAC },
    { 0x1F, 0x8E, 0x19, 0xAC },
    { 0x1F, 0x8E, 0x19, 0xAC },
    { 0x27, 0x8E, 0x19, 0xAC },
    { 0x27, 0x8E, 0x1A, 0x2C },
    { 0x27, 0x8E, 0x1A, 0x2C },
    { 0x27, 0x8E, 0x1A, 0x2C },
    { 0x27, 0x8E, 0x1A, 0x2C },
    { 0x2F, 0x86, 0x1A, 0x2C },
    { 0x2F, 0x86, 0x1A, 0x2C },
    { 0x2F, 0x86, 0x1A, 0x2C },
    { 0x2F, 0x7E, 0x1A, 0x2C },
    { 0x2F, 0x7E, 0x1A, 0x2C },
    { 0x2F, 0x7E, 0x1A, 0x2C },
    { 0x37, 0x7E, 0x1A, 0x2C },
    { 0x37, 0x7E, 0x1A, 0xAC },
    { 0x37, 0x7E, 0x1A, 0xAC },
    { 0x37, 0x7E, 0x1A, 0xAC },
    { 0x37, 0x7E, 0x1A, 0xAC },
    { 0x3F, 0x7E, 0x2A, 0xAC },
    { 0x3F, 0x7E, 0x2A, 0xAC },
    { 0x3F, 0x76, 0x2A, 0xAC },
    { 0x3F, 0x86, 0x2B, 0x2C },
    { 0x3F, 0x7E, 0x2B, 0x2C },
    { 0x47, 0x7E, 0x2B, 0x2C },
    { 0x47, 0x7E, 0x2F, 0x2C },
    { 0x47, 0x7E, 0x2F, 0x2C },
    { 0x47, 0x7E, 0x2F, 0x2C },
    { 0x47, 0x76, 0x2F, 0x2C },
    { 0x4F, 0x76, 0x2F, 0x2C },
    { 0x4F, 0x76, 0x2F, 0x2C },
    { 0x4F, 0x6E, 0x2F, 0x2C },
    { 0x4F, 0x6E, 0x2F, 0x2C },
    { 0x4F, 0x6E, 0x2F, 0x2C },
    { 0x57, 0x6E, 0x2F, 0x2C },
    { 0x57, 0x6E, 0x2F, 0x2C },
    { 0x57, 0x6E, 0x3F, 0x2C },
    { 0x57, 0x6E, 0x3F, 0x2C },
    { 0x57, 0x6E, 0x3F, 0x2C },
    { 0x5F, 0x6E, 0x3F, 0x2C },
    { 0x5F, 0x6E, 0x4F, 0x2C },
    { 0x5F, 0x6E, 0x4F, 0x2C },
    { 0x5F, 0x6E, 0x4F, 0x2C },
    { 0x5F, 0x66, 0x4F, 0x2C },
    { 0x67, 0x66, 0x4F, 0x2C },
    { 0x67, 0x66, 0x4F, 0x2C },
    { 0x67, 0x5E, 0x4F, 0x2C },
    { 0x67, 0x5E, 0x4F, 0x2C },
    { 0x67, 0x66, 0x4F, 0x2C },
    { 0x67, 0x66, 0x4F, 0x2C },
    { 0x67, 0x66, 0x5F, 0x2C },
    { 0x6F, 0x6E, 0x5F, 0x2C },
    { 0x6F, 0x6E, 0x6F, 0x2C },
    { 0x6F, 0x6E, 0x6F, 0x2C },
    { 0x6F, 0x6E, 0x7F, 0x2C },
    { 0x6F, 0x6E, 0x7F, 0x2C },
    { 0x6F, 0x6E, 0x7F, 0x2C },
    { 0x77, 0x66, 0x7F, 0x2C },
    { 0x77, 0x66, 0x7F, 0x2C },
    { 0x77, 0x5E, 0x6F, 0x2C },
    { 0x77, 0x5E, 0x7F, 0x2C },
    { 0x77, 0x5E, 0x7F, 0x2C },
    { 0x7F, 0x5E, 0x7F, 0x2C },
    { 0x7F, 0x5E, 0x8F, 0x2C },
    { 0x7F, 0x5E, 0x8F, 0x2C },
    { 0x7F, 0x5E, 0x8F, 0x2C },
    { 0x87, 0x56, 0x8F, 0x2C },
    { 0x87, 0x56, 0x8F, 0x2C },
    { 0x87, 0x56, 0x8F, 0x2C },
    { 0x87, 0x4E, 0x8F, 0x2C },
    { 0x87, 0x4E, 0x8F, 0x2C },
    { 0x87, 0x4E, 0x8F, 0x2C },
    { 0x8F, 0x4E, 0x9F, 0x2C },
    { 0x8F, 0x4E, 0x9F, 0x2C },
    { 0x8F, 0x4E, 0xAF, 0x2C },
    { 0x8F, 0x4E, 0xAF, 0x2C },
    { 0x8F, 0x4E, 0xAF, 0x2C },
    { 0x97, 0x4E, 0xAF, 0x2C },
    { 0x97, 0x4E, 0xAF, 0x2C },
    { 0x97, 0x4E, 0xAB, 0x2C },
    { 0x97, 0x4E, 0xAB, 0x2C },
    { 0x97, 0x4E, 0xAB, 0x2C },
    { 0x9F, 0x4E, 0xAB, 0x2C },
    { 0x9F, 0x4E, 0xBB, 0x2C },
    { 0x9F, 0x4E, 0xBB, 0x2C },
    { 0x9F, 0x4E, 0xBB, 0x2C },
    { 0x9F, 0x4E, 0xCB, 0x2C },
    { 0xA7, 0x4E, 0xCB, 0x2C },
    { 0xA7, 0x4E, 0xCB, 0x2C },
    { 0xA7, 0x46, 0xCB, 0x2C },
    { 0xA7, 0x46, 0xCB, 0x2C },
    { 0xA7, 0x46, 0xCB, 0x2C },
    { 0xA7, 0x46, 0xDB, 0x2C },
    { 0xAF, 0x46, 0xDB, 0x2C },
    { 0xAF, 0x46, 0xEB, 0x2C },
    { 0xAF, 0x46, 0xEB, 0x2C },
    { 0xAF, 0x4E, 0xEB, 0x2C },
    { 0xAE, 0x4E, 0xEB, 0x2C },
    { 0xAE, 0x4E, 0xEB, 0x2C },
    { 0xB5, 0x46, 0xFB, 0x2C },
    { 0xB5, 0x54, 0xFB, 0x2C },
    { 0xB5, 0x4C, 0xFB, 0x2C },
    { 0xB5, 0x54, 0xFB, 0x2C },
    { 0xB5, 0x54, 0xFB, 0x2C },
    { 0xBD, 0x54, 0xFB, 0x2C },
    { 0xBD, 0x4C, 0xFB, 0x2C },
    { 0xBD, 0x4C, 0xFB, 0x2C },
    { 0xBD, 0x4C, 0xFB, 0x2C },
    { 0xBD, 0x44, 0xEB, 0x2C },
    { 0xC5, 0x44, 0xFB, 0x2C },
    { 0xC5, 0x44, 0xFB, 0x2C },
    { 0xC5, 0x44, 0xFB, 0x2C },
    { 0xC5, 0x45, 0x0B, 0x2C },
    { 0xC5, 0x45, 0x0B, 0x2C },
    { 0xC5, 0x45, 0x0B, 0x2C },
    { 0xCD, 0x45, 0x0B, 0x2C },
    { 0xCD, 0x45, 0x0B, 0x2C },
    { 0xCD, 0x3D, 0x0B, 0x2C },
    { 0xCD, 0x3D, 0x0B, 0x2C },
    { 0xCD, 0x3D, 0x0B, 0x2C },
    { 0xD5, 0x3D, 0x0B, 0x2C },
    { 0xD5, 0x3D, 0x0B, 0x2C },
    { 0xD5, 0x3D, 0x1B, 0x2C },
    { 0xD5, 0x3D, 0x1B, 0x2C },
    { 0xD5, 0x3D, 0x1B, 0x2C },
    { 0xDD, 0x3D, 0x1B, 0x2C },
    { 0xDD, 0x3D, 0x1B, 0x2C },
    { 0xDD, 0x35, 0x1B, 0x2C },
    { 0xDD, 0x35, 0x1B, 0x2C },
    { 0xDD, 0x35, 0x1B, 0x2C },
    { 0xE5, 0x35, 0x1B, 0x2C },
    { 0xE5, 0x35, 0x1B, 0x2C },
    { 0xE5, 0x2D, 0x1B, 0x2C },
    { 0xE5, 0x2D, 0x1B, 0x2C },
    { 0xE5, 0x2D, 0x3B, 0x2C },
    { 0xED, 0x2D, 0x4B, 0x2C },
    { 0xED, 0x2D, 0x1B, 0xA8 },
    { 0xED, 0x2D, 0x1B, 0xAC },
    { 0xED, 0x2D, 0x17, 0xAC },
    { 0xED, 0x2D, 0x17, 0xAC },
    { 0xED, 0x2D, 0x27, 0xAC },
    { 0xF5, 0x2D, 0x27, 0xAC },
    { 0xF5, 0x2D, 0x27, 0xAC },
    { 0xF5, 0x2D, 0x2B, 0xAC },
    { 0xF5, 0x2D, 0x2B, 0xAC },
    { 0xF5, 0x2D, 0x2B, 0xAC },
    { 0xFD, 0x2D, 0x2B, 0xAC },
    { 0xFD, 0x2B, 0x2B, 0xAC },
    { 0xFD, 0x2B, 0x2B, 0xAC },
    { 0xFD, 0x2B, 0x2B, 0xAC },
    { 0xFD, 0x2B, 0x2B, 0xAC },
    { 0xFD, 0x23, 0x2B, 0xAC },
    { 0xFD, 0x23, 0x2B, 0xAC },
    { 0xFD, 0x23, 0x2B, 0xAC },
    { 0xFD, 0x21, 0x2B, 0xAC },
    { 0xFD, 0x21, 0x2B, 0xAC },
    { 0xFD, 0x29, 0x2B, 0xAC },
    { 0xFD, 0x29, 0x2B, 0xAC },
    { 0xFD, 0x29, 0x27, 0xAC },
    { 0xFD, 0x29, 0x37, 0xAC },
    { 0xFD, 0x29, 0x23, 0xAC },
    { 0xFD, 0x29, 0x23, 0xAC },
    { 0xFD, 0x29, 0x23, 0xAC },
    { 0xFD, 0x29, 0x23, 0xAC },
    { 0xFD, 0x21, 0x23, 0xAC },
    { 0xFD, 0x21, 0x23, 0xAC },
    { 0xFD, 0x21, 0x23, 0xAC },
    { 0xFD, 0x21, 0x33, 0xAC },
    { 0xFD, 0x21, 0x33, 0xAC },
    { 0xFD, 0x21, 0x33, 0xAC },
    { 0xFD, 0x21, 0x43, 0xAC },
    { 0xFD, 0x21, 0x43, 0xAC },
    { 0xFD, 0x21, 0x43, 0xAC },
    { 0xFC, 0x21, 0x43, 0xAC },
    { 0xFC, 0x21, 0x43, 0xAC },
    { 0xFC, 0x19, 0x43, 0xAC },
    { 0xFC, 0x19, 0x43, 0xAC },
    { 0xFC, 0x19, 0x43, 0xAC },
    { 0xFC, 0x19, 0x43, 0xAC },
    { 0xFC, 0x19, 0x53, 0xAC },
    { 0xFC, 0x19, 0x53, 0xAC },
    { 0xFC, 0x19, 0x53, 0xAC },
    { 0xFC, 0x19, 0x53, 0xAC },
    { 0xFC, 0x19, 0x63, 0xAC },
    { 0xFC, 0x19, 0x63, 0xAC },
    { 0xFC, 0x19, 0x63, 0xAC },
    { 0xFC, 0x19, 0x73, 0xAC },
    { 0xFC, 0x19, 0x73, 0xAC },
    { 0xFC, 0x19, 0x73, 0xAC },
    { 0xFC, 0x19, 0x73, 0xAC },
    { 0xFC, 0x19, 0x73, 0xAC },
    { 0xFC, 0x19, 0x83, 0xAC },
    { 0xFC, 0x19, 0x83, 0xAC },
    { 0xFC, 0x19, 0x83, 0xAC },
    { 0xFC, 0x19, 0x83, 0xAC },
    { 0xFC, 0x19, 0x83, 0xAC },
    { 0xFC, 0x19, 0x93, 0xAC },
    { 0xFC, 0x19, 0x93, 0xAC },
    { 0xFC, 0x19, 0x93, 0xAC },
    { 0xFC, 0x19, 0xA3, 0xAC },
    { 0xFC, 0x19, 0xA3, 0xAC },
    { 0xFC, 0x19, 0xB3, 0xAC },
    { 0xFC, 0x19, 0xB3, 0xAC },
    { 0xFC, 0x19, 0xB3, 0xAC },
    { 0xFC, 0x19, 0xB3, 0xAC }
};


static void ClearTemplate(sdla_t *);
static unsigned char InitTemplate(sdla_t *);
static void InitLineReceiver(sdla_t *);

static void ClearTPSCReg(sdla_t *);
static void ClearRPSCReg(sdla_t *);

static int WriteTPSCReg(sdla_t *, int, int, unsigned char);
static unsigned char ReadTPSCReg(sdla_t *, int, int);

static int WriteRPSCReg(sdla_t *, int, int, unsigned char);
static unsigned char ReadRPSCReg(sdla_t *, int, int);

static void DisableAllChannels(sdla_t *);
static void EnableAllChannels(sdla_t *);
static int DisableTxChannel(sdla_t *, int);
static int DisableRxChannel(sdla_t *, int);
static int EnableTxChannel(sdla_t *, int);
static int EnableRxChannel(sdla_t *, int);

static void sdla_te_set_intr(sdla_t *);
static void sdla_te_tx_intr(sdla_t *);
static void sdla_te_rx_intr(sdla_t *);
static void sdla_t1_rx_intr(sdla_t *);
static void sdla_e1_rx_intr(sdla_t *);

static void sdla_te_set_status(sdla_t *, unsigned long);
static void sdla_te_enable_timer(sdla_t *, unsigned long);

static int sdla_te_linelb(sdla_t *, unsigned char);
static int sdla_te_paylb(sdla_t *, unsigned char);
static int sdla_te_ddlb(sdla_t *, unsigned char);
static int sdla_te_lb(sdla_t *, unsigned char);


static void
ClearTemplate(sdla_t *card)
{
	int i = 0, j = 0;
	unsigned int indirect_addr = 0x00;

	for (i = FIRST_UI; i <= LAST_UI; i++) {
		for (j = FIRST_SAMPLE; j <= LAST_SAMPLE; j++) {
			indirect_addr = (j << 3) | i;
			/* Set up the indirect address */
			WRITE_REG(REG_XLPG_WAVEFORM_ADDR, indirect_addr);
			WRITE_REG(REG_XLPG_WAVEFORM_DATA, 0x00);
		}
	}
}

static unsigned char
InitTemplate(sdla_t *card)
{
	sdla_te_cfg_t* te_cfg = &card->fe_te.te_cfg;
	int i = 0, j = 0;
	unsigned char indirect_addr = 0x00, xlpg_scale = 0x00;
	TX_WAVEFORM* tx_waveform = NULL;

	if (IS_T1(&card->fe_te.te_cfg)) {
		switch (te_cfg->lbo) {
		case WAN_T1_LBO_0_DB:
			tx_waveform = &t1_tx_waveform_lh_0db;
			xlpg_scale = 0x0C;
			break;
		case WAN_T1_LBO_75_DB:
			tx_waveform = &t1_tx_waveform_lh_75db;
			xlpg_scale = 0x07;
			break;
		case WAN_T1_LBO_15_DB:
			tx_waveform = &t1_tx_waveform_lh_15db;
			xlpg_scale = 0x03;
			break;
		case WAN_T1_LBO_225_DB:
			tx_waveform = &t1_tx_waveform_lh_225db;
			xlpg_scale = 0x02;
			break;
		case WAN_T1_0_110:
			tx_waveform = &t1_tx_waveform_sh_110ft;
			xlpg_scale = 0x0C;
			break;
		case WAN_T1_110_220:
			tx_waveform = &t1_tx_waveform_sh_220ft;
			xlpg_scale = 0x10;
			break;
		case WAN_T1_220_330:
			tx_waveform = &t1_tx_waveform_sh_330ft;
			xlpg_scale = 0x11;
			break;
		case WAN_T1_330_440:
			tx_waveform = &t1_tx_waveform_sh_440ft;
			xlpg_scale = 0x12;
			break;
		case WAN_T1_440_550:
			tx_waveform = &t1_tx_waveform_sh_550ft;
			xlpg_scale = 0x14;
			break;
		case WAN_T1_550_660:
			tx_waveform = &t1_tx_waveform_sh_660ft;
			xlpg_scale = 0x15;
			break;
		default:
			/* Use 0DB as a default value */
			tx_waveform = &t1_tx_waveform_lh_0db;
			xlpg_scale = 0x0C;
			break;
		}
	} else {
		tx_waveform = &e1_tx_waveform_120;
		xlpg_scale = 0x0C;
		/*xlpg_scale = 0x0B; */
	}

	for (i = FIRST_UI; i <= LAST_UI; i++) {
		for (j = FIRST_SAMPLE; j <= LAST_SAMPLE; j++) {
			indirect_addr = (j << 3) | i;
			/* Set up the indirect address */
			WRITE_REG(REG_XLPG_WAVEFORM_ADDR, indirect_addr);
			WRITE_REG(REG_XLPG_WAVEFORM_DATA, (*tx_waveform)[j][i]);
		}
	}
	return xlpg_scale;
}


static void
InitLineReceiver(sdla_t *card)
{
	int			ram_addr = 0x00;
	RLPS_EQUALIZER_RAM	*rlps_ram_table = NULL;

	if (IS_E1(&card->fe_te.te_cfg)) {
		rlps_ram_table = e1_rlps_ram_table;
	} else {
		if (card->fe_te.te_cfg.high_impedance_mode == WAN_YES) {
			log(LOG_INFO, "%s: Setting to High-Impedance Mode!\n",
					card->devname);
			rlps_ram_table = t1_rlps_perf_mode_ram_table;
		} else {
			rlps_ram_table = t1_rlps_ram_table;
		}
	}
	for (ram_addr = 0; ram_addr <= 255; ram_addr++) {
/* ERRATA VVV	*/
		/* Configure a write into the RAM address */
		WRITE_REG(REG_RLPS_EQ_RWB, BIT_RLPS_EQ_RWB);
		/* Initiate write into the specified RAM address */
		WRITE_REG(REG_RLPS_EQ_ADDR, (unsigned char)ram_addr);
		DELAY(100);
/* ERRATA ^^^	*/
		/* Write 1st value from conten column */
		WRITE_REG(REG_RLPS_IND_DATA_1, rlps_ram_table[ram_addr].byte1);
		/* Write 2st value from conten column */
		WRITE_REG(REG_RLPS_IND_DATA_2, rlps_ram_table[ram_addr].byte2);
		/* Write 3st value from conten column */
		WRITE_REG(REG_RLPS_IND_DATA_3, rlps_ram_table[ram_addr].byte3);
		/* Write 4st value from conten column */
		WRITE_REG(REG_RLPS_IND_DATA_4, rlps_ram_table[ram_addr].byte4);
		/* Configure a write into the RAM address */
		WRITE_REG(REG_RLPS_EQ_RWB, 0x00);
		/* Initiate write into the specified RAM address */
		WRITE_REG(REG_RLPS_EQ_ADDR, (unsigned char)ram_addr);
/* ERRATA VVV	*/
		DELAY(100);
/* ERRATA ^^^	*/
	}
}

static void
ClearTPSCReg(sdla_t *card)
{
	int channel = 0;
	int start_channel = 0, stop_channel = 0;

	if (IS_E1(&card->fe_te.te_cfg)) {
		start_channel = 0;
		stop_channel = NUM_OF_E1_TIMESLOTS + 1;
	} else {
		start_channel = 1;
		stop_channel = NUM_OF_T1_CHANNELS;
	}

	for (channel = start_channel; channel <= stop_channel; channel++) {
		WRITE_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel, 0x00);
		WRITE_TPSC_REG(REG_TPSC_IDLE_CODE_BYTE, channel, 0x00);
		WRITE_TPSC_REG(REG_TPSC_SIGNALING_BYTE, channel, 0x00);
	}
	return;
}

static void
ClearRPSCReg(sdla_t *card)
{
	int channel = 0;
	int start_channel = 0, stop_channel = 0;

	if (IS_E1(&card->fe_te.te_cfg)) {
		start_channel = 0;
		stop_channel = NUM_OF_E1_TIMESLOTS + 1;
	} else {
		start_channel = 1;
		stop_channel = NUM_OF_T1_CHANNELS;
	}

	for (channel = start_channel; channel <= stop_channel; channel++) {
		WRITE_RPSC_REG(REG_RPSC_DATA_CTRL_BYTE, channel, 0x00);
		WRITE_RPSC_REG(REG_RPSC_DATA_COND_BYTE, channel, 0x00);
		WRITE_RPSC_REG(REG_RPSC_SIGNALING_BYTE, channel, 0x00);
	}
	return;
}

/*
 * Write value to TPSC indirect register.
 * Arguments:   card	- Pointer to the card structure
 *		reg	- Offset in TPSC indirect space.
 *		channel - Channel number.
 *		value	- New PMC register value.
 * Returns:	0 - success, otherwise - error
 */
static int
WriteTPSCReg(sdla_t *card, int reg, int channel, unsigned char value)
{
	unsigned char	temp = 0x00;
	int		i = 0, busy_flag = 0;
	int		err = 0;

	reg += channel;
	/* Set IND bit to 1 in TPSC to enable indirect access to
	** TPSC register */
	WRITE_REG(REG_TPSC_CFG, BIT_TPSC_IND);
	busy_flag = 1;
	for (i = 0; i < MAX_BUSY_READ; i++) {
		temp = READ_REG(REG_TPSC_MICRO_ACCESS_STATUS);
		if ((temp & BIT_TPSC_BUSY) == 0x0) {
			busy_flag = 0;
			break;
		}
	}
	if (busy_flag == 1) {
		log(LOG_INFO, "%s: Failed to write to TPSC Reg[%02x]<-%02x!\n",
					card->devname, reg, value);
		err = -EBUSY;
		goto write_tpsc_done;
	}

	WRITE_REG(REG_TPSC_CHANNEL_INDIRECT_DATA_BUFFER,
				(unsigned char)value);
	WRITE_REG(REG_TPSC_CHANNEL_INDIRECT_ADDRESS_CONTROL,
				(unsigned char)(reg & 0x7F));

	for (i = 0; i < MAX_BUSY_READ; i++) {
		temp = READ_REG(REG_TPSC_MICRO_ACCESS_STATUS);
		if ((temp & BIT_TPSC_BUSY) == 0x0) {
			err = -EBUSY;
			goto write_tpsc_done;
		}
	}
	log(LOG_INFO, "%s: Failed to write value to TPSC Reg=%02x, val=%02x.\n",
				card->devname, reg, value);
write_tpsc_done:
	/* Set PCCE bit to 1 in TPSC to enable modifing the TPSC register */
	WRITE_REG(REG_TPSC_CFG, BIT_TPSC_IND | BIT_TPSC_PCCE);
	return err;
}

/*
 * Read value from TPSC indirect register.
 *
 * Arguments:   card	- Pointer to the card structure
 *		reg	- Offset in TPSC indirect space.
 *		channel	- Channel number.
 * Returns:	Returns register value.
 */
static unsigned char
ReadTPSCReg(sdla_t *card, int reg, int channel)
{
	unsigned char	tmp = 0x00, value = 0x00;
	int		i = 0, busy_flag = 0;

	reg += channel;
	/* Set IND bit to 1 in TPSC to enable indirect access to
	** TPSC register */
	WRITE_REG(REG_TPSC_CFG, BIT_TPSC_IND);
	busy_flag = 1;
	for (i = 0; i < MAX_BUSY_READ; i++) {
		tmp = READ_REG(REG_TPSC_MICRO_ACCESS_STATUS);
		if ((tmp & BIT_TPSC_BUSY) == 0x0) {
			busy_flag = 0;
			break;
		}
	}
	if (busy_flag == 1) {
		log(LOG_INFO, "%s: Failed to read value to TPSC Reg=%02x!\n",
					card->devname, reg);
		goto read_tpsc_done;
	}

	WRITE_REG(REG_TPSC_CHANNEL_INDIRECT_ADDRESS_CONTROL,
					(unsigned char)(reg | 0x80));

	for (i = 0; i < MAX_BUSY_READ; i++) {
		tmp = READ_REG(REG_TPSC_MICRO_ACCESS_STATUS);
		if ((tmp & BIT_TPSC_BUSY) == 0x0) {
			value = READ_REG(REG_TPSC_CHANNEL_INDIRECT_DATA_BUFFER);
			goto read_tpsc_done;
		}
	}
	log(LOG_INFO, "%s: Failed to read value to TPSC Reg=%02x.\n",
					card->devname, reg);
read_tpsc_done:
	/* Set PCCE bit to 1 in TPSC to enable modifing the TPSC register */
	WRITE_REG(REG_TPSC_CFG, BIT_TPSC_IND | BIT_TPSC_PCCE);
	return value;
}

/*
 * Write value to RPSC indirect register.
 *
 * Arguments:   card	- Pointer to the card structure
 *		reg	- Offset in RPSC indirect space.
 *		channel	- Channel number.
 *		value - New PMC register value.
 * Returns:	0-success, otherwise - error
 */
static int
WriteRPSCReg(sdla_t* card, int reg, int channel, unsigned char value)
{
	unsigned char	temp = 0x00;
	int		i = 0, busy_flag = 0;
	int		err = 0;

	reg += channel;
	/* Set IND bit to 1 in RPSC to enable indirect access to
	** RPSC register*/
	WRITE_REG(REG_RPSC_CFG, BIT_RPSC_IND);
	busy_flag = 1;
	for (i = 0; i < MAX_BUSY_READ; i++) {
		temp = READ_REG(REG_RPSC_MICRO_ACCESS_STATUS);
		if ((temp & BIT_RPSC_BUSY) == 0x0) {
			busy_flag = 0;
			break;
		}
	}
	if (busy_flag == 1) {
		log(LOG_INFO, "%s: Failed to write to RPSC Reg[%02x]<-%02x!\n",
		    card->devname, reg, value);
		err = -EBUSY;
		goto write_rpsc_done;
	}

	WRITE_REG(REG_RPSC_CHANNEL_INDIRECT_DATA_BUFFER, (unsigned char)value);
	WRITE_REG(REG_RPSC_CHANNEL_INDIRECT_ADDRESS_CONTROL,
	    (unsigned char)(reg & 0x7F));

	for (i = 0; i < MAX_BUSY_READ; i++) {
		temp = READ_REG(REG_RPSC_MICRO_ACCESS_STATUS);
		if ((temp & BIT_RPSC_BUSY) == 0x0) {
			err = -EBUSY;
			goto write_rpsc_done;
		}
	}
	log(LOG_INFO, "%s: Failed to write value to RPSC Reg=%02x, val=%02x.\n",
	    card->devname, reg, value);
write_rpsc_done:
	/* Set PCCE bit to 1 in RPSC to enable modifing the RPSC register */
	WRITE_REG(REG_RPSC_CFG, BIT_RPSC_IND | BIT_RPSC_PCCE);
	return err;
}

/*
 * Read value from RPSC indirect register.
 * Arguments:   card	- Pointer to the card structure
 *		reg	- Offset in RPSC indirect space.
 *		channel	- Channel number
 * Returns:	Returns register value.
 */
static unsigned char ReadRPSCReg(sdla_t* card, int reg, int channel)
{
	unsigned char	tmp = 0x00, value = 0x00;
	int		i = 0,busy_flag = 0;

	reg += channel;
	/* Set IND bit to 1 in RPSC to enable indirect access to
	** RPSC register*/
	WRITE_REG(REG_RPSC_CFG, BIT_RPSC_IND);
	busy_flag = 1;
	for (i = 0; i < MAX_BUSY_READ; i++) {
		tmp = READ_REG(REG_RPSC_MICRO_ACCESS_STATUS);
		if ((tmp & BIT_RPSC_BUSY) == 0x0) {
			busy_flag = 0;
			break;
		}
	}
	if (busy_flag == 1) {
		log(LOG_INFO, "%s: Failed to read value to RPSC Reg=%02x!\n",
						card->devname, reg);
		goto read_rpsc_done;
	}

	WRITE_REG(REG_RPSC_CHANNEL_INDIRECT_ADDRESS_CONTROL,
					(unsigned char)(reg | 0x80));

	for (i = 0; i < MAX_BUSY_READ; i++) {
		tmp = READ_REG(REG_RPSC_MICRO_ACCESS_STATUS);
		if ((tmp & BIT_RPSC_BUSY) == 0x0) {
			value = READ_REG(REG_RPSC_CHANNEL_INDIRECT_DATA_BUFFER);
		goto read_rpsc_done;
		}
	}
	log(LOG_INFO, "%s: Failed to read value to RPSC Reg=%02x.\n",
						card->devname, reg);
read_rpsc_done:
	/* Set PCCE bit to 1 in RPSC to enable modifing the RPSC register */
	WRITE_REG(REG_RPSC_CFG, BIT_RPSC_IND | BIT_RPSC_PCCE);
	return value;
}


/*
 * Description: Disable All channels for RX/TX
 * Arguments:	card - Pointer to the card structure.
 * Returns:	none
 */
static void DisableAllChannels(sdla_t* card)
{
	int i = 0;

	if (IS_E1(&card->fe_te.te_cfg)) {
		DisableTxChannel(card, E1_FRAMING_TIMESLOT);
		DisableRxChannel(card, E1_FRAMING_TIMESLOT);
		for (i = 1; i <= NUM_OF_E1_TIMESLOTS; i++) {
			DisableTxChannel(card, i);
			DisableRxChannel(card, i);
		}
	} else {
		for (i = 1; i <= NUM_OF_T1_CHANNELS; i++) {
			DisableTxChannel(card, i);
			DisableRxChannel(card, i);
		}
	}
}

/*
 * Description: Enable All channels.
 * Arguments:	card - Pointer to the card structure.
 * Returns:	none
 */
static void EnableAllChannels(sdla_t* card)
{
	int i = 0;

	if (IS_E1(&card->fe_te.te_cfg)) {
		int first_ts =
			(card->fe_te.te_cfg.frame == WAN_FR_UNFRAMED) ?
					0 : 1;

		DisableTxChannel(card, E1_FRAMING_TIMESLOT);
		DisableRxChannel(card, E1_FRAMING_TIMESLOT);
		for (i = first_ts; i <= NUM_OF_E1_TIMESLOTS; i++) {
			EnableTxChannel(card, i);
			EnableRxChannel(card, i);
		}
	} else {
		for (i = 1; i <= NUM_OF_T1_CHANNELS; i++) {
			EnableTxChannel(card, i);
			EnableRxChannel(card, i);
		}
	}
}

/*
 * Description: Enable Tx for specific channel
 * Arguments:	card	- pointer to the card structure
 *		channel	- channel number
 * Returns:	0-success, otherwise-error
 */
static int EnableTxChannel(sdla_t* card, int channel)
{
	sdla_te_cfg_t*	te_cfg = &card->fe_te.te_cfg;

	if (te_cfg->lcode == WAN_LC_AMI) {
		/* ZCs=1 AMI*/
		WRITE_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel,
		    (((READ_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel) &
		    MASK_TPSC_DATA_CTRL_BYTE) &
		    ~BIT_TPSC_DATA_CTRL_BYTE_IDLE_DS0) |
		    BIT_TPSC_DATA_CTRL_BYTE_ZCS1));
	} else {
		WRITE_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel,
		    ((READ_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel) &
		    MASK_TPSC_DATA_CTRL_BYTE) &
		    ~(BIT_TPSC_DATA_CTRL_BYTE_IDLE_DS0 |
		    BIT_TPSC_DATA_CTRL_BYTE_ZCS1 |
		    BIT_TPSC_DATA_CTRL_BYTE_ZCS0)));
	}

	if (IS_E1(&card->fe_te.te_cfg)) {
		/* Set SUBS=DS[0]=DS[1]=0x0 - no change to PCM timeslot data */
		WRITE_TPSC_REG(REG_TPSC_E1_CTRL_BYTE, channel,
		    (READ_TPSC_REG(REG_TPSC_E1_CTRL_BYTE, channel) &
		    ~(BIT_TPSC_E1_CTRL_BYTE_SUBS |
		    BIT_TPSC_E1_CTRL_BYTE_DS0 |
		    BIT_TPSC_E1_CTRL_BYTE_DS1)));
	} else {
		WRITE_TPSC_REG(REG_TPSC_SIGNALING_BYTE, channel, 0x00);
	}

	/* Erase contents of IDLE code byte */
	WRITE_TPSC_REG(REG_TPSC_IDLE_CODE_BYTE, channel, 0x00);

	return 0;
}
/*
 * Description: Enable Rx for specific channel
 * Arguments:	card	- pointer to the card structure
 *		channel	- channel number
 * Returns:	0-success, otherwise-error
 */
static int EnableRxChannel(sdla_t* card, int channel)
{
	/* Set DTRPC bit to 0 in RPSC */
	WRITE_RPSC_REG(REG_RPSC_DATA_CTRL_BYTE, channel,
		((READ_RPSC_REG(REG_RPSC_DATA_CTRL_BYTE, channel) &
			MASK_RPSC_DATA_CTRL_BYTE) &
				~BIT_RPSC_DATA_CTRL_BYTE_DTRKC));
	return 0;
}

/*
 * Description: Disable Tx for specific channel
 * Arguments:	card	- pointer to the card structure
 *		channel	- channel number
 * Returns:	0-success, otherwise-error
 */
static int DisableTxChannel(sdla_t* card, int channel)
{
	/* Set IDLE_DS0 to 1 for an IDLE code byte will insert and
	 * BTCLK will suppressed
	 */
	WRITE_TPSC_REG(REG_RPSC_DATA_CTRL_BYTE, channel,
	    ((READ_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel) &
	    MASK_TPSC_DATA_CTRL_BYTE) | BIT_TPSC_DATA_CTRL_BYTE_IDLE_DS0));
	if (IS_E1(&card->fe_te.te_cfg)) {
		/* Set SUBS=1, DS0=0 - data substitution on - IDLE code
		** replaces BTPCM timeslot data */
		WRITE_TPSC_REG(REG_TPSC_E1_CTRL_BYTE, channel,
		    ((READ_TPSC_REG(REG_TPSC_E1_CTRL_BYTE, channel) &
		    ~BIT_TPSC_E1_CTRL_BYTE_DS0) | BIT_TPSC_E1_CTRL_BYTE_SUBS));
	} else {
		WRITE_TPSC_REG(REG_TPSC_SIGNALING_BYTE, channel, 0x00);
	}
	/* Erase contents of IDLE code byte */
	WRITE_TPSC_REG(REG_TPSC_IDLE_CODE_BYTE, channel, 0x55);
	return 0;
}

/*
 * Description: Disable Rx for specific channel
 * Arguments:	card	- pointer to the card structure
 *		channel	- channel number
 * Returns:	0-success, otherwise-error
 */
static int DisableRxChannel(sdla_t* card, int channel)
{
	/* Set DTRPC bit to 1 in RPSC to hold low for the duration of
	** the channel */
	WRITE_RPSC_REG(REG_RPSC_DATA_CTRL_BYTE, channel,
	    ((READ_RPSC_REG(REG_RPSC_DATA_CTRL_BYTE, channel) &
	    MASK_RPSC_DATA_CTRL_BYTE) | BIT_RPSC_DATA_CTRL_BYTE_DTRKC));
	return 0;
}

/*
 * Set default T1 configuration
 */
int sdla_te_defcfg(void *pte_cfg)
{
	sdla_te_cfg_t	*te_cfg = (sdla_te_cfg_t*)pte_cfg;

	te_cfg->media = WAN_MEDIA_T1;
	te_cfg->lcode = WAN_LC_B8ZS;
	te_cfg->frame = WAN_FR_ESF;
	te_cfg->lbo = WAN_T1_LBO_0_DB;
	te_cfg->te_clock = WAN_NORMAL_CLK;
	te_cfg->active_ch = ENABLE_ALL_CHANNELS;
	te_cfg->high_impedance_mode = WAN_NO;
	return 0;
}


int sdla_te_setcfg(void *pcard, struct ifmedia *ifm)
{
	sdla_t		*card = (sdla_t*)pcard;
	sdla_te_cfg_t	*te_cfg = (sdla_te_cfg_t*)&card->fe_te.te_cfg;

	switch (ifm->ifm_media) {
	case(IFM_TDM|IFM_TDM_T1):
#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Setting T1 media type!\n",
				card->devname);
#endif /* DEBUG_INIT */
		te_cfg->media = WAN_MEDIA_T1;
		te_cfg->lcode = WAN_LC_B8ZS;
		te_cfg->frame = WAN_FR_ESF;
		break;
	case(IFM_TDM|IFM_TDM_T1_AMI):
#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Setting T1 AMI media type!\n",
				card->devname);
#endif /* DEBUG_INIT */
		te_cfg->media = WAN_MEDIA_T1;
		te_cfg->lcode = WAN_LC_AMI;
		te_cfg->frame = WAN_FR_ESF;
		break;
	case(IFM_TDM|IFM_TDM_E1):
#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Setting E1 media type!\n",
				card->devname);
#endif /* DEBUG_INIT */
		te_cfg->media = WAN_MEDIA_E1;
		te_cfg->lcode = WAN_LC_HDB3;
		te_cfg->frame = WAN_FR_NCRC4;
		break;
	case(IFM_TDM|IFM_TDM_E1_AMI):
#ifdef DEBUG_INIT
		log(LOG_INFO, "%s: Setting E1 AMI media type!\n",
				card->devname);
#endif /* DEBUG_INIT */
		te_cfg->media = WAN_MEDIA_E1;
		te_cfg->lcode = WAN_LC_AMI;
		te_cfg->frame = WAN_FR_NCRC4;
		break;
	default:
		log(LOG_INFO, "%s: Unsupported ifmedia type (%04X)\n",
		    card->devname, ifm->ifm_media);
		return -EINVAL;
	}
	return 0;
}

/*
 * Set timeslot map
 */
void
sdla_te_settimeslot(void* pcard, unsigned long ts_map)
{
	sdla_t	*card = (sdla_t*)pcard;

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: Setting timeslot map to %08lX\n",
			card->devname, ts_map);
#endif /* DEBUG_INIT */
	card->fe_te.te_cfg.active_ch = ts_map;
	return;
}

unsigned long
sdla_te_gettimeslot(void* pcard)
{
	return ((sdla_t*)pcard)->fe_te.te_cfg.active_ch;
}

/*
 * Configure Sangoma TE1 board
 *
 * Arguments:	
 * Returns:	0 - TE1 configred successfully, otherwise -EINVAL.
 */
short
sdla_te_config(void* card_id)
{
	sdla_t		*card = (sdla_t*)card_id;
	sdla_te_cfg_t	*te_cfg = &card->fe_te.te_cfg;
	u_int16_t	 adapter_type;
	unsigned char	 value = 0x00, xlpg_scale = 0x00;
	int		 channel_range = (IS_T1(&card->fe_te.te_cfg)) ?
				NUM_OF_T1_CHANNELS : NUM_OF_E1_TIMESLOTS;
	int i = 0;

	WAN_ASSERT(card == NULL);
	WAN_ASSERT(card->write_front_end_reg == NULL);
	WAN_ASSERT(card->read_front_end_reg == NULL);
	sdla_getcfg(card->hw, SDLA_ADAPTERTYPE, &adapter_type);

#ifdef DEBUG_INIT
	log(LOG_INFO, "%s: Setting %s configuration!\n",
			card->devname,
			IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1");
	if (IS_T1(&card->fe_te.te_cfg)) {
		log(LOG_DEBUG, "%s: Line decoding %s\n",
			card->devname,
			(te_cfg->lcode == WAN_LC_AMI) ? "AMI" : "B8ZS");
		log(LOG_DEBUG, "%s: Frame type %s\n",
			card->devname,
			(te_cfg->frame == WAN_FR_ESF) ? "ESF" :
			(te_cfg->frame == WAN_FR_D4) ? "D4" : "Unframed");
		switch (te_cfg->lbo) {
		case WAN_T1_LBO_0_DB:
			log(LOG_DEBUG, "%s: LBO 0 dB\n", card->devname);
			break;
		case WAN_T1_LBO_75_DB:
			log(LOG_DEBUG, "%s: LBO 7.5 dB\n", card->devname);
			break;
		case WAN_T1_LBO_15_DB:
			log(LOG_DEBUG, "%s: LBO 15 dB\n", card->devname);
			break;
		case WAN_T1_LBO_225_DB:
			log(LOG_DEBUG, "%s: LBO 22.5 dB\n", card->devname);
			break;
		case WAN_T1_0_110:
			log(LOG_DEBUG, "%s: LBO 0-110 ft.\n", card->devname);
			break;
		case WAN_T1_110_220:
			log(LOG_DEBUG, "%s: LBO 110-220 ft.\n", card->devname);
			break;
		case WAN_T1_220_330:
			log(LOG_DEBUG, "%s: LBO 220-330 ft.\n", card->devname);
			break;
		case WAN_T1_330_440:
			log(LOG_DEBUG, "%s: LBO 330-440 ft.\n", card->devname);
			break;
		case WAN_T1_440_550:
			log(LOG_DEBUG, "%s: LBO 440-550 ft.\n", card->devname);
			break;
		case WAN_T1_550_660:
			log(LOG_DEBUG, "%s: LBO 550-660 ft.\n",
					card->devname);
			break;
		}
	} else {
		log(LOG_DEBUG, "%s: Line decoding %s\n", card->devname,
		    (te_cfg->lcode == WAN_LC_AMI) ? "AMI" : "HDB3");
		log(LOG_DEBUG, "%s: Frame type %s\n", card->devname,
		    (te_cfg->frame == WAN_FR_CRC4) ? "CRC4" :
		    (te_cfg->frame == WAN_FR_NCRC4) ? "non-CRC3" :
		    "Unframed");
	}
	log(LOG_DEBUG, "%s: Clock mode %s\n", card->devname,
	    (te_cfg->te_clock == WAN_NORMAL_CLK) ? "Normal" : "Master");
#endif /* DEBUG_INIT */

	/* 1. Initiate software reset of the COMET */
	/* Set RESET=1 to place COMET into RESET */
	WRITE_REG(REG_RESET, BIT_RESET);

	/* Set RESET=0, disable software reset. COMET in default mode. */
	WRITE_REG(REG_RESET, 0x0/*~BIT_RESET*/);

	/* 2.Setup the XLPG(Transmit pulse template) to clear the pulse
	** template */
	ClearTemplate(card);
	xlpg_scale = InitTemplate(card);

	/* Program PMC for T1/E1 mode (Reg 0x00) */
	if (IS_E1(&card->fe_te.te_cfg)) {
		if (adapter_type & A101_ADPTR_T1E1_MASK) {
			WRITE_REG(REG_GLOBAL_CFG,
					BIT_GLOBAL_TRKEN | BIT_GLOBAL_PIO_OE |
					BIT_GLOBAL_E1);
		} else {
			WRITE_REG(REG_GLOBAL_CFG,
					BIT_GLOBAL_PIO_OE | BIT_GLOBAL_E1);
		}
	} else {
		if (adapter_type & A101_ADPTR_T1E1_MASK) {
			WRITE_REG(REG_GLOBAL_CFG,
					BIT_GLOBAL_TRKEN | BIT_GLOBAL_PIO_OE);
		}
	}

	/* Set SCALE[4-0] value in XLPG Line driver Configuration (Reg. 0xF0) */
	WRITE_REG(REG_XLPG_LINE_CFG, xlpg_scale);

	/* Set system clock and XCLK (Reg 0xD6) */
	if (IS_T1(&card->fe_te.te_cfg)) {
		WRITE_REG(REG_CSU_CFG, BIT_CSU_MODE0);
		/*WRITE_REG(REG_CSU_CFG,
		**	BIT_CSU_MODE2 | BIT_CSU_MODE1 | BIT_CSU_MODE0); */
	} else {
		WRITE_REG(REG_CSU_CFG, 0x00);
	}

	/* Set Line decoding (Reg. 0x10) */
	if (te_cfg->lcode == WAN_LC_AMI) {
		WRITE_REG(REG_CDRC_CFG, BIT_CDRC_CFG_AMI);
	} else {
		WRITE_REG(REG_CDRC_CFG, 0x00);
	}

	/* Program the RX-ELST/TX-ELST for the appropriate mode
	** (Reg 0x1C, 0x20)*/
	if (IS_E1(&card->fe_te.te_cfg)) {
		WRITE_REG(REG_RX_ELST_CFG, BIT_RX_ELST_IR | BIT_RX_ELST_OR);
		WRITE_REG(REG_TX_ELST_CFG, BIT_TX_ELST_IR | BIT_RX_ELST_OR);
	} else {
		WRITE_REG(REG_RX_ELST_CFG, 0x00);
		WRITE_REG(REG_TX_ELST_CFG, 0x00);
	}

	value = 0x00;
	if (IS_E1(&card->fe_te.te_cfg)) {
		/* Program the trasmitter framing and line decoding
		** (Reg. 0x80) */
		if (te_cfg->lcode == WAN_LC_AMI) {
			value |= BIT_E1_TRAN_AMI;
		}
		if (te_cfg->frame == WAN_FR_CRC4) {
			value |= BIT_E1_TRAN_GENCRC;
		} else if (te_cfg->frame == WAN_FR_UNFRAMED) {
			value |= BIT_E1_TRAN_FDIS;
		}
		/* E1 TRAN Configuration (Reg 0x80) */
		WRITE_REG(REG_E1_TRAN_CFG, value);
		/* Configure the receive framer (Reg 0x90) */
		value = 0x00;
		if (te_cfg->frame == WAN_FR_CRC4) {
			value |=
				(BIT_E1_FRMR_CRCEN |
				BIT_E1_FRMR_CASDIS |
				BIT_E1_FRMR_REFCRCEN);
		} else if (te_cfg->frame == WAN_FR_NCRC4) {
			value |= BIT_E1_FRMR_CASDIS;
		}
		WRITE_REG(REG_E1_FRMR_CFG, value);
	} else {
		/* Set framing format & line decoding for transmitter
		** (Reg 0x54) */
		if (te_cfg->lcode == WAN_LC_B8ZS) {
			value |= BIT_T1_XBAS_B8ZS;
		} else {
			value |= BIT_T1_XBAS_ZCS0;
		}
		if (te_cfg->frame == WAN_FR_ESF) {
			value |= BIT_T1_XBAS_ESF;
		}
		WRITE_REG(REG_T1_XBAS_CFG, value);

		/* Program framing format for receiving (Reg. 0x48) */
		value = 0x00;
		if (te_cfg->frame == WAN_FR_ESF) {
			value = BIT_T1_FRMR_ESF | BIT_T1_FRMR_ESFFA;
		}
		WRITE_REG(REG_T1_FRMR_CFG, value);

		/* Program the transmitter framing format and line deconding
		** (Reg. 0x60) */
		value = 0x00;
		if (te_cfg->frame == WAN_FR_ESF) {
			value = BIT_T1_ALMI_CFG_ESF;
		}
		WRITE_REG(REG_T1_ALMI_CFG, value);
	}

	/* Configure the SIGX configuration register */
	if (IS_E1(&card->fe_te.te_cfg)) {
		WRITE_REG(REG_SIGX_CFG, 0x00);
	} else {
		value = READ_REG(REG_SIGX_CFG);
		if (te_cfg->frame == WAN_FR_ESF) {
			value |= BIT_SIGX_ESF;
		}
		WRITE_REG(REG_SIGX_CFG, value);
	}
	/* Program the BTIF for the frame pulse mode */
	value = 0x00;
	if (IS_E1(&card->fe_te.te_cfg)) {
		value |= BIT_BTIF_RATE0;
	}
	if (te_cfg->lcode == WAN_LC_AMI) {
		value |= BIT_BTIF_NXDS0_0;
	} else if (te_cfg->frame != WAN_FR_UNFRAMED) {
		value |= BIT_BTIF_NXDS0_1;
	}

	if (adapter_type & A101_ADPTR_T1E1_MASK) {
		value |= (BIT_BTIF_CMODE | BIT_BTIF_DE | BIT_BTIF_FE);
	}
	WRITE_REG(REG_BTIF_CFG, value);
	/* Set the type of frame pulse on the backplane */
	value = 0x00;

	if (adapter_type & A101_ADPTR_T1E1_MASK) {
		value = BIT_BTIF_FPMODE;
	}
	WRITE_REG(REG_BTIF_FR_PULSE_CFG, value);

	/* Program the BRIF for the frame pulse mode */
	value = 0x00;
	if (IS_E1(&card->fe_te.te_cfg)) {
		value |= BIT_BRIF_RATE0;
	}
	if (te_cfg->lcode == WAN_LC_AMI) {
		value |= BIT_BRIF_NXDS0_0;
	} else if (te_cfg->frame != WAN_FR_UNFRAMED) {
		value |= BIT_BRIF_NXDS0_1;
	}
	if (adapter_type & A101_ADPTR_T1E1_MASK) {
		value |= BIT_BRIF_CMODE;
	}
	WRITE_REG(REG_BRIF_CFG, value);
	/* Set the type of frame pulse on the backplane */
	value = 0x00;

	if (adapter_type & A101_ADPTR_T1E1_MASK) {
		value = BIT_BRIF_FPMODE;
	}
	WRITE_REG(REG_BRIF_FR_PULSE_CFG, value);
	/* Program the data integraty checking on the BRIF */
	WRITE_REG(REG_BRIF_DATA_CFG, BIT_BRIF_DATA_TRI_0);

	/* Set TJAT FIFO output clock signal (Reg 0x06) */
	if (te_cfg->te_clock == WAN_NORMAL_CLK) {
		WRITE_REG(REG_TX_TIMING_OPT, BIT_TX_PLLREF1 | BIT_TX_TXELSTBYP);
	} else {
		WRITE_REG(REG_TX_TIMING_OPT,
			BIT_TX_PLLREF1 | BIT_TX_PLLREF0 | BIT_TX_TXELSTBYP);
	}

	/* Set long or short and enable the equalizer (Reg 0xF8) */
	WRITE_REG(REG_RLPS_CFG_STATUS, BIT_RLPS_CFG_STATUS_LONGE);

	/* Select ALOS Detection and Clearance Thresholds (Reg 0xF9) */
	/* NC: Aug 20 2003:
	 *     Set the correct ALSO Detection/Clearance tresholds
	 *     for T1/E1 lines, to get rid of false ALOS alarms.
	 *
	 *     Original incorrect value set was 0x00, for both T1/E1 */
	if (IS_E1(&card->fe_te.te_cfg)) {
		WRITE_REG(REG_RLPS_ALOS_DET_CLR_THR,
				BIT_RLPS_ALOS_DET_THR_2|
				BIT_RLPS_ALOS_DET_THR_1|
				BIT_RLPS_ALOS_DET_THR_0);
	} else {
		WRITE_REG(REG_RLPS_ALOS_DET_CLR_THR,
				BIT_RLPS_ALOS_CLR_THR_2|
				BIT_RLPS_ALOS_CLR_THR_0|
				BIT_RLPS_ALOS_DET_THR_2|
				BIT_RLPS_ALOS_DET_THR_0);
	}

	/* Select ALOS Detection period to set the ALOS alarm (Reg 0xFA) */
	WRITE_REG(REG_RLPS_ALOS_DET_PER, REG_RLPS_ALOS_DET_PER_0);
	/* Select ALOS Clearance period to clear the ALOS alarm (Reg 0xFB) */
	WRITE_REG(REG_RLPS_ALOS_CLR_PER, BIT_RLPS_ALOS_CLR_PER_0);
	/* Program to 0x00 to initiate a microprocessor access to RAM
	** (Reg 0xFC) */
/* ERRATA	WRITE_REG(REG_RLPS_EQ_ADDR, 0x00); */
	/* Write the value 0x80 to this register to select a write to the RAM
	** (Reg 0xFD) */
/* ERRATA	WRITE_REG(REG_RLPS_EQ_RWB, BIT_RLPS_EQ_RWB); */
	/* Program this register to 0x00 to reset the pointer to the RAM
	** (Reg 0xFE) */
	WRITE_REG(REG_RLPS_EQ_STATUS, 0x00);
	/* Configure the Recive line Equalizer (Reg 0xFF) */
	WRITE_REG(REG_RLPS_EQ_CFG,
		BIT_RLPS_EQ_RESERVED | BIT_RLPS_EQ_FREQ_1 | BIT_RLPS_EQ_FREQ_0);

	/* Configure the TJAT FIFO (Reg 0x1B) */
	WRITE_REG(REG_TJAT_CFG, BIT_TJAT_CENT);

	/* Configure the RJAT FIFO (Reg 0x17) */
	WRITE_REG(REG_RJAT_CFG, BIT_RJAT_CENT);
	/* Program Receive Options (Reg 0x02) */
	if (te_cfg->frame == WAN_FR_UNFRAMED) {
		WRITE_REG(REG_RECEIVE_OPT, BIT_RECEIVE_OPT_UNF);
	} else {
		WRITE_REG(REG_RECEIVE_OPT, 0x00);
	}

	/* Configure XLPG Analog Test Positive control (Reg 0xF4) */
	WRITE_REG(REG_XLPG_TPC, BIT_XLPG_TPC_0);
	/* Configure XLPG Analog Test Negative control (Reg 0xF5) */
	WRITE_REG(REG_XLPG_TNC, BIT_XLPG_TNC_0);

	/* Program the RLPS Equalizer Voltage (Reg 0xDC) */
	if (IS_E1(&card->fe_te.te_cfg)) {
		WRITE_REG(REG_EQ_VREF, 0x34);
	} else {
		WRITE_REG(REG_EQ_VREF, 0x2C);
	}
	WRITE_REG(REG_RLPS_FUSE_CTRL_STAT, 0x00);

/* ERRATA WRITE_REG(REG_RLPS_FUSE_CTRL_STAT, 0x00);*/
/* ERRAT VVV */
	WRITE_REG(0xF4, 0x01);
	WRITE_REG(0xF4, 0x01);
	value = READ_REG(0xF4) & 0xFE;
	WRITE_REG(0xF4, value);

	WRITE_REG(0xF5, 0x01);
	WRITE_REG(0xF5, 0x01);
	value = READ_REG(0xF5) & 0xFE;
	WRITE_REG(0xF5, value);

	WRITE_REG(0xF6, 0x01);
/* ERRATA ^^^ */

	InitLineReceiver(card);

	ClearRPSCReg(card);
	ClearTPSCReg(card);


	DisableAllChannels(card);
	if (te_cfg->active_ch == ENABLE_ALL_CHANNELS) {
#ifdef DEBUG_INIT
		log(LOG_DEBUG, "%s: All channels enabled\n", card->devname);
#endif /* DEBUG_INIT */
		EnableAllChannels(card);
	} else {
		for (i = 1; i <= channel_range; i++) {
			if (te_cfg->active_ch & (1 << (i - 1))) {
#ifdef DEBUG_INIT
				log(LOG_DEBUG, "%s: Enable channel %d\n",
						card->devname, i);
#endif /* DEBUG_INIT */
				EnableTxChannel(card, i);
				EnableRxChannel(card, i);
			}
		}
	}

	/* Initialize and start T1/E1 timer */
	card->fe_te.te_timer_cmd = TE_SET_INTR;
	bit_clear((u_int8_t*)&card->fe_te.te_critical,TE_TIMER_KILL);
	timeout_set(&card->fe_te.te_timer, sdla_te_timer, (void*)card);
	sdla_te_enable_timer(card, INTR_TE1_TIMER);

	bit_set((u_int8_t*)&card->fe_te.te_critical, TE_CONFIGURED);

	return 0;
}

/*
 * Enable T1/E1 interrupts.
 */
static void
sdla_te_set_intr(sdla_t* card)
{

	/* Enable LOS interrupt */
	/* WRITE_REG(REG_CDRC_INT_EN, BIT_CDRC_INT_EN_LOSE);*/
	/* Enable ALOS interrupt */
	WRITE_REG(REG_RLPS_CFG_STATUS,
		READ_REG(REG_RLPS_CFG_STATUS) | BIT_RLPS_CFG_STATUS_ALOSE);
	if (IS_T1(&card->fe_te.te_cfg)) {
		/* Enable RBOC interrupt */
		WRITE_REG(REG_T1_RBOC_ENABLE,
				BIT_T1_RBOC_ENABLE_IDLE |
				BIT_T1_RBOC_ENABLE_BOCE);
		/* Enable interrupt on RED, AIS, YEL alarms */
		WRITE_REG(REG_T1_ALMI_INT_EN,
				BIT_T1_ALMI_INT_EN_REDE |
				BIT_T1_ALMI_INT_EN_AISE |
				BIT_T1_ALMI_INT_EN_YELE);
		/* Enable interrupt on OOF alarm */
		/*WRITE_REG(REG_T1_FRMR_INT_EN, BIT_T1_FRMR_INT_EN_INFRE);*/
	} else {
		/* Enable interrupt on RED, AIS alarms */
		WRITE_REG(REG_E1_FRMR_M_A_INT_EN,
				BIT_E1_FRMR_M_A_INT_EN_REDE |
				BIT_E1_FRMR_M_A_INT_EN_AISE);
		/* Enable OOF Interrupt */
		/*WRITE_REG(REG_E1_FRMR_FRM_STAT_INT_EN,
				BIT_E1_FRMR_FRM_STAT_INT_EN_OOFE);*/
	}

#if 0
	if (card->te_signaling_config == NULL) {
		/* Enable SIGE and COSS */
		/* log(LOG_INFO,"%s: Enable SIGX interrupt\n",card->devname);*/
		WRITE_REG(REG_SIGX_CFG,
			READ_REG(REG_SIGX_CFG) | BIT_SIGX_SIGE);
		WRITE_REG(REG_SIGX_CFG,
			READ_REG(REG_SIGX_CFG) | BIT_SIGX_COSS);
	}
#endif
	/* Initialize T1/E1 timer */
	bit_clear((u_int8_t*)&card->fe_te.te_critical,TE_TIMER_KILL);
	/* Start T1/E1 timer */
	card->fe_te.te_timer_cmd = TE_LINKDOWN_TIMER;
	sdla_te_enable_timer(card, POLLING_TE1_TIMER);
	return;
}

/*
 * T1/E1 unconfig.
 */
void sdla_te_unconfig(void* card_id)
{
	sdla_t* card = (sdla_t*)card_id;

	if (!bit_test((u_int8_t*)&card->fe_te.te_critical, TE_CONFIGURED)) {
		return;
	}

	bit_clear((u_int8_t*)&card->fe_te.te_critical, TE_CONFIGURED);
	bit_set((u_int8_t*)&card->fe_te.te_critical, TE_TIMER_KILL);

	timeout_del(&card->fe_te.te_timer);
	return;
}

/*
 * Set T1/E1 status. Enable OOF and LCV interrupt
 * if status changed to disconnected.
 */
static void
sdla_te_set_status(sdla_t *card, unsigned long alarms)
{

	if (IS_T1(&card->fe_te.te_cfg)) {
		if (IS_T1_ALARM(alarms)) {
			if (card->front_end_status != FE_DISCONNECTED) {
				log(LOG_INFO, "%s: T1 disconnected!\n",
				    card->devname);
				card->front_end_status = FE_DISCONNECTED;
			}
		} else {
			if (card->front_end_status != FE_CONNECTED) {
				log(LOG_INFO, "%s: T1 connected!\n",
				    card->devname);
				card->front_end_status = FE_CONNECTED;
			}
		}
	} else {
		if (IS_E1_ALARM(alarms)) {
			if (!bit_test((u_int8_t*)&card->fe_te.te_critical,
			    TE_TIMER_RUNNING)) {
				card->fe_te.te_timer_cmd = TE_LINKDOWN_TIMER;
				sdla_te_enable_timer(card, POLLING_TE1_TIMER);
			}
			if (card->front_end_status != FE_DISCONNECTED) {
				log(LOG_INFO, "%s: E1 disconnected!\n",
				    card->devname);
				card->front_end_status = FE_DISCONNECTED;
			}
		} else {
			if (card->front_end_status != FE_CONNECTED) {
				log(LOG_INFO, "%s: E1 connected!\n",
							card->devname);
				card->front_end_status = FE_CONNECTED;
			}
		}
	}
#if 0
	if (card->te_report_alarms) {
		card->te_report_alarms(card, alarms);
	}
#endif

#if 0
	if (card->front_end_status == FE_CONNECTED) {
		WRITE_REG(REG_CDRC_INT_EN,
			(READ_REG(REG_CDRC_INT_EN) | BIT_CDRC_INT_EN_LOSE));
	} else {
		WRITE_REG(REG_CDRC_INT_EN,
			(READ_REG(REG_CDRC_INT_EN) & ~BIT_CDRC_INT_EN_LOSE));
	}
#endif

	return;
}

/*
 * Read Alram Status for T1/E1 modes.
 *
 * Arguments:
 * Returns:		bit 0 - ALOS	(E1/T1)
 *			bit 1 - LOS	(E1/T1)
 *			bit 2 - ALTLOS	(E1/T1)
 *			bit 3 - OOF	(E1/T1)
 *			bit 4 - RED	(E1/T1)
 *			bit 5 - AIS	(E1/T1)
 *			bit 6 - OOSMF	(E1)
 *			bit 7 - OOCMF	(E1)
 *			bit 8 - OOOF	(E1)
 *			bit 9 - RAI	(E1)
 *			bit A - YEL	(T1)
 */
unsigned long
sdla_te_alarm(void *card_id, int manual_update)
{
	sdla_t *card = (sdla_t*)card_id;
	unsigned long status = 0x00;

	WAN_ASSERT(card->write_front_end_reg == NULL);
	WAN_ASSERT(card->read_front_end_reg == NULL);
	/* Check common alarm for E1 and T1 configuration
	 * 1. ALOS alarm 
	 * Reg 0xFA 
	 * Reg 0xF8 (ALOSI = 1)
	 */
	if (READ_REG(REG_RLPS_ALOS_DET_PER) &&
	    (READ_REG(REG_RLPS_CFG_STATUS) & BIT_RLPS_CFG_STATUS_ALOSV)) {
		status |= BIT_ALOS_ALARM;
	}

	/* 2. LOS alarm 
	 * Reg 0x10
	 * Reg 0xF8 (ALOSI = 1)
	 */
	if ((READ_REG(REG_CDRC_CFG) & (BIT_CDRC_CFG_LOS0|BIT_CDRC_CFG_LOS1)) &&
		(READ_REG(REG_CDRC_INT_STATUS) & BIT_CDRC_INT_STATUS_LOSV)) {
		status |= BIT_LOS_ALARM;
	}

	/* 3. ALTLOS alarm ??????????????????
	 * Reg 0x13
	 */
	if (READ_REG(REG_ALTLOS_STATUS) & BIT_ALTLOS_STATUS_ALTLOS) {
		status |= BIT_ALTLOS_ALARM;
	}

	/* Check specific E1 and T1 alarms */
	if (IS_E1(&card->fe_te.te_cfg)) {
		/* 4. OOF alarm */
		if (READ_REG(REG_E1_FRMR_FR_STATUS) &
		    BIT_E1_FRMR_FR_STATUS_OOFV) {
			status |= BIT_OOF_ALARM;
		}
		/* 5. OOSMF alarm */
		if (READ_REG(REG_E1_FRMR_FR_STATUS) &
		    BIT_E1_FRMR_FR_STATUS_OOSMFV) {
			status |= BIT_OOSMF_ALARM;
		}
		/* 6. OOCMF alarm */
		if (READ_REG(REG_E1_FRMR_FR_STATUS) &
		    BIT_E1_FRMR_FR_STATUS_OOCMFV) {
			status |= BIT_OOCMF_ALARM;
		}
		/* 7. OOOF alarm */
		if (READ_REG(REG_E1_FRMR_FR_STATUS) &
		    BIT_E1_FRMR_FR_STATUS_OOOFV) {
			status |= BIT_OOOF_ALARM;
		}
		/* 8. RAI alarm */
		if (READ_REG(REG_E1_FRMR_MAINT_STATUS) &
		    BIT_E1_FRMR_MAINT_STATUS_RAIV) {
			status |= BIT_RAI_ALARM;
		}
		/* 9. RED alarm
		 * Reg 0x97 (REDD)
		 */
		if (READ_REG(REG_E1_FRMR_MAINT_STATUS) &
		     BIT_E1_FRMR_MAINT_STATUS_RED) {
			status |= BIT_RED_ALARM;
		}
		/* 10. AIS alarm
		 * Reg 0x91 (AISC)
		 * Reg 0x97 (AIS)
		 */
		if ((READ_REG(REG_E1_FRMR_MAINT_OPT) &
		    BIT_E1_FRMR_MAINT_OPT_AISC) &&
		    (READ_REG(REG_E1_FRMR_MAINT_STATUS) &
		    BIT_E1_FRMR_MAINT_STATUS_AIS)) {
			status |= BIT_AIS_ALARM;
		}
	} else {
		/* 4. OOF alarm
		 * Reg 0x4A (INFR=0 T1 mode)
		 */
		if (!(READ_REG(REG_T1_FRMR_INT_STATUS) &
		    BIT_T1_FRMR_INT_STATUS_INFR)) {
			status |= BIT_OOF_ALARM;
		}
		/* 5. AIS alarm
		 * Reg 0x62 (AIS)
		 * Reg 0x63 (AISD)
		 */
		if ((READ_REG(REG_T1_ALMI_INT_STATUS) &
		    BIT_T1_ALMI_INT_STATUS_AIS) &&
		    (READ_REG(REG_T1_ALMI_DET_STATUS) &
		    BIT_T1_ALMI_DET_STATUS_AISD)) {
			status |= BIT_AIS_ALARM;
		}
		/* 6. RED alarm
		 * Reg 0x63 (REDD)	
		 */
		if (READ_REG(REG_T1_ALMI_DET_STATUS) &
		    BIT_T1_ALMI_DET_STATUS_REDD) {
			status |= BIT_RED_ALARM;
		}
		/* 7. YEL alarm
		 * Reg 0x62 (YEL)
		 * Reg 0x63 (YELD)
		 */
		if ((READ_REG(REG_T1_ALMI_INT_STATUS) &
		    BIT_T1_ALMI_INT_STATUS_YEL) &&
		    (READ_REG(REG_T1_ALMI_DET_STATUS) &
		    BIT_T1_ALMI_DET_STATUS_YELD)) {
			status |= BIT_YEL_ALARM;
		}
	}
	if (manual_update) {
		sdla_te_set_status(card, status);
	}
	return status;
}


/*
 * Read PMC performance monitoring counters
 */
void
sdla_te_pmon(void *card_id)
{
	sdla_t *card = (sdla_t*)card_id;
	pmc_pmon_t *pmon = &card->fe_te.te_pmon;

	WAN_ASSERT1(card->write_front_end_reg == NULL);
	WAN_ASSERT1(card->read_front_end_reg == NULL);
	/* Update PMON counters */
	WRITE_REG(REG_PMON_BIT_ERROR, 0x00);
	/* Framing bit for E1/T1 */
	pmon->frm_bit_error +=
	    READ_REG(REG_PMON_BIT_ERROR) & BITS_PMON_BIT_ERROR;

	/* OOF Error for T1 or Far End Block Error for E1 */
	pmon->oof_errors +=
	    ((READ_REG(REG_PMON_OOF_FEB_MSB_ERROR) &
	    BITS_PMON_OOF_FEB_MSB_ERROR) << 8) |
	    READ_REG(REG_PMON_OOF_FEB_LSB_ERROR);

	/* Bit Error for T1 or CRC Error for E1 */
	pmon->bit_errors +=
	    ((READ_REG(REG_PMON_BIT_CRC_MSB_ERROR) &
	    BITS_PMON_BIT_CRC_MSB_ERROR) << 8) |
	    READ_REG(REG_PMON_BIT_CRC_LSB_ERROR);

	/* LCV Error for E1/T1 */
	pmon->lcv += ((READ_REG(REG_PMON_LCV_MSB_COUNT) &
	    BITS_PMON_LCV_MSB_COUNT) << 8) | READ_REG(REG_PMON_LCV_LSB_COUNT);
	return;
}

/*
 * Flush PMC performance monitoring counters
 */
void
sdla_flush_te1_pmon(void *card_id)
{
	sdla_t *card = (sdla_t*)card_id;
	pmc_pmon_t *pmon = &card->fe_te.te_pmon;

	pmon->pmon1 = 0;
	pmon->pmon2 = 0;
	pmon->pmon3 = 0;
	pmon->pmon4 = 0;

	return;
}

static int
SetLoopBackChannel(sdla_t *card, int channel, unsigned char mode)
{
	/* Set IND bit to 1 in TPSC to enable indirect access to TPSC
	** register */
	WRITE_REG(REG_TPSC_CFG, BIT_TPSC_IND);

	/* Set LOOP to 1 for an IDLE code byte (the transmit data is
	 * overwritten with the corresponding channel data from the receive
	 * line. */
	if (mode == LINELB_ACTIVATE_CODE) {
		WRITE_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel,
			((READ_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel) &
				MASK_TPSC_DATA_CTRL_BYTE) |
				BIT_TPSC_DATA_CTRL_BYTE_LOOP));
	} else {
		WRITE_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel,
			((READ_TPSC_REG(REG_TPSC_DATA_CTRL_BYTE, channel) &
				MASK_TPSC_DATA_CTRL_BYTE) &
				~BIT_TPSC_DATA_CTRL_BYTE_LOOP));
	}

	/* Set PCCE bit to 1 in TPSC to enable modifing the TPSC register */
	WRITE_REG(REG_TPSC_CFG,
		((READ_REG(REG_TPSC_CFG) & MASK_TPSC_CFG) | BIT_TPSC_PCCE));

	return 0;
}

/*
 * Check interrupt type.
 * Arguments:	card - pointer to device structure.
 * Returns:	None.
 */
void
sdla_te_intr(void *arg)
{
	sdla_t *card = (sdla_t*)arg;

	WAN_ASSERT1(card->write_front_end_reg == NULL);
	WAN_ASSERT1(card->read_front_end_reg == NULL);
	sdla_te_tx_intr(card);
	sdla_te_rx_intr(card);
	sdla_te_set_status(card, card->fe_te.te_alarm);
}

/*
 * Read tx interrupt.
 *
 * Arguments: card		- pointer to device structure.
 * Returns: None.
 */
static void
sdla_te_tx_intr(sdla_t *card)
{
	unsigned char intr_src1 = 0x00, intr_src2 = 0x00, intr_src3 = 0x00;

	intr_src1 = READ_REG(REG_INT_SRC_1);
	intr_src2 = READ_REG(REG_INT_SRC_2);
	intr_src3 = READ_REG(REG_INT_SRC_3);

	if (intr_src1 == 0 && intr_src2 == 0 && intr_src3 == 0) {
		log(LOG_DEBUG, "%s: Unknown %s interrupt!\n",
				card->devname,
				IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1");
	}
	if (!(intr_src1 & BITS_TX_INT_SRC_1 ||
		intr_src2 & BITS_TX_INT_SRC_2 ||
		intr_src3 & BITS_TX_INT_SRC_3)) {
		return;
	}

#if 0
	if (intr_src1 & BIT_INT_SRC_1_TJAT) {
	}
	if (intr_src1 & BIT_INT_SRC_1_APRM) {
	}
	if (intr_src2 & BIT_INT_SRC_2_TX_ELST) {
	}
	if (intr_src2 & BIT_INT_SRC_2_TDPR_1) {
	}
	if (intr_src2 & BIT_INT_SRC_2_TDPR_2) {
	}
	if (intr_src2 & BIT_INT_SRC_2_TDPR_3) {
	}
	if (intr_src3 & BIT_INT_SRC_3_TRAN) {
	}
	if (intr_src3 & BIT_INT_SRC_3_XPDE) {
	}
	if (intr_src3 & BIT_INT_SRC_3_BTIF) {
	}
#endif
	return;
}


/*
 * Read rx interrupt.
 *
 * Arguments: card		- pointer to device structure.
 * Returns: None.
 */
static void
sdla_te_rx_intr(sdla_t *card)
{
	if (IS_T1(&card->fe_te.te_cfg)) {
		sdla_t1_rx_intr(card);
	} else {
		sdla_e1_rx_intr(card);
	}
	return;
}

/*
 * Read tx interrupt.
 *
 * Arguments: card		- pointer to device structure.
 * Returns: None.
 */
static void
sdla_t1_rx_intr(sdla_t *card)
{
	unsigned char intr_src1 = 0x00, intr_src2 = 0x00, intr_src3 = 0x00;
	unsigned char status = 0x00;

	intr_src1 = READ_REG(REG_INT_SRC_1);
	intr_src2 = READ_REG(REG_INT_SRC_2);
	intr_src3 = READ_REG(REG_INT_SRC_3);

	if (!(intr_src1 & BITS_RX_INT_SRC_1 ||
		intr_src2 & BITS_RX_INT_SRC_2 ||
		intr_src3 & BITS_RX_INT_SRC_3)) {
		return;
	}

	/* 3. PDVD */
	if (intr_src3 & BIT_INT_SRC_3_PDVD) {
		status = READ_REG(REG_PDVD_INT_EN_STATUS);
		if ((status & BIT_PDVD_INT_EN_STATUS_PDVE) &&
		    (status & BIT_PDVD_INT_EN_STATUS_PDVI)) {
			if (status & BIT_PDVD_INT_EN_STATUS_PDV) {
				log(LOG_INFO, "%s: T1 pulse density "
				    "violation detected!\n", card->devname);
			}
		}
		if ((status & BIT_PDVD_INT_EN_STATUS_Z16DE) &&
		    (status & BIT_PDVD_INT_EN_STATUS_Z16DI)) {
			log(LOG_INFO, "%s: T1 16 consecutive zeros detected!\n",
			    card->devname);
		}
	}

	/* 6. ALMI */
	if (intr_src3 & BIT_INT_SRC_3_ALMI) {
		status = READ_REG(REG_T1_ALMI_INT_STATUS);
		if (status & BIT_T1_ALMI_INT_STATUS_YELI) {
			if (status & BIT_T1_ALMI_INT_STATUS_YEL) {
				if (!(card->fe_te.te_alarm & BIT_YEL_ALARM)) {
					log(LOG_INFO, "%s: T1 YELLOW ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_YEL_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_YEL_ALARM) {
					log(LOG_INFO, "%s: T1 YELLOW OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_YEL_ALARM;
				}
			}
		}
		if (status & BIT_T1_ALMI_INT_STATUS_REDI) {
			if (status & BIT_T1_ALMI_INT_STATUS_RED) {
				if (!(card->fe_te.te_alarm & BIT_RED_ALARM)) {
					log(LOG_INFO, "%s: T1 RED ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_RED_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_RED_ALARM) {
					log(LOG_INFO, "%s: T1 RED OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_RED_ALARM;
				}
			}
		}
		if (status & BIT_T1_ALMI_INT_STATUS_AISI) {
			if (status & BIT_T1_ALMI_INT_STATUS_AIS) {
				if (!(card->fe_te.te_alarm & BIT_AIS_ALARM)) {
					log(LOG_INFO, "%s: T1 AIS ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_AIS_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_AIS_ALARM) {
					log(LOG_INFO, "%s: T1 AIS OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_AIS_ALARM;
				}
			}
		}

#if 0
		if (status &
			(BIT_T1_ALMI_INT_STATUS_YELI |
			 BIT_T1_ALMI_INT_STATUS_REDI |
			 BIT_T1_ALMI_INT_STATUS_AISI)) {
			if (status & (BIT_T1_ALMI_INT_STATUS_YEL |
					BIT_T1_ALMI_INT_STATUS_RED |
					BIT_T1_ALMI_INT_STATUS_AIS)) {

				/* Update T1/E1 alarm status */
				if (!(card->fe_te.te_alarm & BIT_YEL_ALARM) &&
				    (status & BIT_T1_ALMI_INT_STATUS_YEL)) {
					log(LOG_INFO, "%s: T1 YELLOW ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_YEL_ALARM;
				}
				if (!(card->fe_te.te_alarm & BIT_RED_ALARM) &&
				    (status & BIT_T1_ALMI_INT_STATUS_RED)) {
					log(LOG_INFO, "%s: T1 RED ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_RED_ALARM;
				}
				if (!(card->fe_te.te_alarm & BIT_AIS_ALARM) &&
				    (status & BIT_T1_ALMI_INT_STATUS_AIS)) {
					log(LOG_INFO, "%s: T1 AIS ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_AIS_ALARM;
				}
			} else {
				/* Update T1/E1 alarm status */
				if ((card->fe_te.te_alarm & BIT_YEL_ALARM) &&
				    !(status & BIT_T1_ALMI_INT_STATUS_YEL)) {
					log(LOG_INFO, "%s: T1 YELLOW OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_YEL_ALARM;
				}
				if ((card->fe_te.te_alarm & BIT_RED_ALARM) &&
				    !(status & BIT_T1_ALMI_INT_STATUS_RED)) {
					log(LOG_INFO, "%s: T1 RED OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_RED_ALARM;
				}
				if ((card->fe_te.te_alarm & BIT_AIS_ALARM) &&
				    !(status & BIT_T1_ALMI_INT_STATUS_AIS)) {
					log(LOG_INFO, "%s: T1 ALMI OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_AIS_ALARM;
				}
			}
		}
#endif
	}

	/* 8. RBOC */
	if (intr_src3 & BIT_INT_SRC_3_RBOC) {
		status = READ_REG(REG_T1_RBOC_CODE_STATUS);
		if (status & BIT_T1_RBOC_CODE_STATUS_BOCI) {
			struct timeval	tv;
			unsigned long	time;

			microtime(&tv);
			time = tv.tv_sec / 1000;
			status &= MASK_T1_RBOC_CODE_STATUS;
			switch (status) {
			case LINELB_ACTIVATE_CODE:
			case LINELB_DEACTIVATE_CODE:
				if (bit_test((u_int8_t *)
				    &card->fe_te.te_critical, LINELB_WAITING) &&
				    bit_test((u_int8_t *)
				    &card->fe_te.te_critical,
				    LINELB_CODE_BIT)) {
					bit_clear((u_int8_t *)
					&card->fe_te.te_critical,
					LINELB_CODE_BIT);
					break;
				}

				log(LOG_DEBUG, "%s: T1 LB %s code received.\n",
				    card->devname,
				    (status == LINELB_ACTIVATE_CODE) ?
				    "activation" : "deactivation");
				card->fe_te.te_rx_lb_cmd = status;
				card->fe_te.te_rx_lb_time = time;
				break;

			case LINELB_DS1LINE_ALL:
				if (bit_test(
				    (u_int8_t *)&card->fe_te.te_critical,
				    LINELB_WAITING) &&
				    bit_test(
				    (u_int8_t *)&card->fe_te.te_critical,
				    LINELB_CHANNEL_BIT)) {
					bit_clear((u_int8_t *)
					    &card->fe_te.te_critical,
					    LINELB_CHANNEL_BIT);
					bit_clear((u_int8_t*)
					    &card->fe_te.te_critical,
					    LINELB_WAITING);
					break;
				}
				if (!card->fe_te.te_rx_lb_cmd)
					break;
				if ((time - card->fe_te.te_rx_lb_time) <
				    LINELB_TE1_TIMER) {
					log(LOG_INFO, "%s: T1 LB %s cancel!\n",
						card->devname,
						(card->fe_te.te_rx_lb_cmd ==
						LINELB_ACTIVATE_CODE)?
						"activatation":
						"deactivation");
				} else {
					unsigned char	reg;
					if (card->fe_te.te_rx_lb_cmd ==
					    LINELB_ACTIVATE_CODE) {
						log(LOG_INFO,
						    "%s: T1 LB activated.\n",
						    card->devname);
						reg=READ_REG(REG_MASTER_DIAG);
						reg|=BIT_MASTER_DIAG_LINELB;
						WRITE_REG(REG_MASTER_DIAG,reg);
					} else {
						log(LOG_INFO,
						    "%s: T1 LB deactivated.\n",
						    card->devname);
						reg=READ_REG(REG_MASTER_DIAG);
						reg&=~BIT_MASTER_DIAG_LINELB;
						WRITE_REG(REG_MASTER_DIAG,reg);
					}
				}
				card->fe_te.te_rx_lb_cmd = 0x00;
				card->fe_te.te_rx_lb_time = 0x00;
				break;

			case LINELB_DS3LINE:
				break;

			case LINELB_DS1LINE_1:
			case LINELB_DS1LINE_2:
			case LINELB_DS1LINE_3:
			case LINELB_DS1LINE_4:
			case LINELB_DS1LINE_5:
			case LINELB_DS1LINE_6:
			case LINELB_DS1LINE_7:
			case LINELB_DS1LINE_8:
			case LINELB_DS1LINE_9:
			case LINELB_DS1LINE_10:
			case LINELB_DS1LINE_11:
			case LINELB_DS1LINE_12:
			case LINELB_DS1LINE_13:
			case LINELB_DS1LINE_14:
			case LINELB_DS1LINE_15:
			case LINELB_DS1LINE_16:
			case LINELB_DS1LINE_17:
			case LINELB_DS1LINE_18:
			case LINELB_DS1LINE_19:
			case LINELB_DS1LINE_20:
			case LINELB_DS1LINE_21:
			case LINELB_DS1LINE_22:
			case LINELB_DS1LINE_23:
			case LINELB_DS1LINE_24:
			case LINELB_DS1LINE_25:
			case LINELB_DS1LINE_26:
			case LINELB_DS1LINE_27:
			case LINELB_DS1LINE_28:
				if (!card->fe_te.te_rx_lb_cmd)
					break;
				if ((time - card->fe_te.te_rx_lb_time) <
				    LINELB_TE1_TIMER) {
					log(LOG_DEBUG, "%s: T1 LB %s cancel!\n",
					    card->devname,
					    (card->fe_te.te_rx_lb_cmd ==
					    LINELB_ACTIVATE_CODE) ?
					    "activatation": "deactivation");
				} else {
					int channel;

					channel = status & LINELB_DS1LINE_MASK;
					log(LOG_INFO, "%s: T1 LB %s ts %d\n",
					    card->devname,
					    (card->fe_te.te_rx_lb_cmd ==
					    LINELB_ACTIVATE_CODE) ?
					    "activated" : "deactivated",
						channel);
					SetLoopBackChannel(card, channel,
						card->fe_te.te_rx_lb_cmd);
				}
				card->fe_te.te_rx_lb_cmd = 0x00;
				card->fe_te.te_rx_lb_time = 0x00;
				break;

			default:
				log(LOG_DEBUG, "%s: Unknown signal (%02x).\n",
				    card->devname, status);
				break;
			}
		}
	}

	/* 7. FRMR */
	if (intr_src1 & BIT_INT_SRC_1_FRMR) {
		status = READ_REG(REG_T1_FRMR_INT_STATUS);
		if ((READ_REG(REG_T1_FRMR_INT_EN) & BIT_T1_FRMR_INT_EN_INFRE) &&
		    (status & BIT_T1_FRMR_INT_STATUS_INFRI)) {
			if (status & BIT_T1_FRMR_INT_STATUS_INFR) {
				if (!(card->fe_te.te_alarm & BIT_OOF_ALARM)) {
					log(LOG_INFO, "%s: T1 OOF ON!\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_OOF_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_OOF_ALARM) {
					log(LOG_INFO, "%s: T1 OOF OFF!\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_OOF_ALARM;
				}
			}
		}
	}

	/* 1. RLPS */
	if (intr_src3 & BIT_INT_SRC_3_RLPS) {
		status = READ_REG(REG_RLPS_CFG_STATUS);
		if ((status & BIT_RLPS_CFG_STATUS_ALOSE) &&
		    (status & BIT_RLPS_CFG_STATUS_ALOSI)) {
			if (status & BIT_RLPS_CFG_STATUS_ALOSV) {
				if (!(card->fe_te.te_alarm & BIT_ALOS_ALARM)) {
					log(LOG_INFO, "%s: T1 ALOS ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_ALOS_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_ALOS_ALARM) {
					log(LOG_INFO, "%s: T1 ALOS OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_ALOS_ALARM;
				}
			}
		}
	}

	/* 2. CDRC */
	if (intr_src1 & BIT_INT_SRC_1_CDRC) {
		status = READ_REG(REG_CDRC_INT_STATUS);
		if ((READ_REG(REG_CDRC_INT_EN) & BIT_CDRC_INT_EN_LOSE) &&
		    (status & BIT_CDRC_INT_STATUS_LOSI)) {
			if (status & BIT_CDRC_INT_STATUS_LOSV) {
				if (!(card->fe_te.te_alarm & BIT_LOS_ALARM)) {
					log(LOG_INFO, "%s: T1 LOS ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_LOS_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_LOS_ALARM) {
					log(LOG_INFO, "%s: T1 LOS OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_LOS_ALARM;
				}
			}
		}
		if ((READ_REG(REG_CDRC_INT_EN) & BIT_CDRC_INT_EN_LCVE) &&
		    (status & BIT_CDRC_INT_STATUS_LCVI)) {
			log(LOG_INFO, "%s: T1 line code violation!\n",
			    card->devname);
		}
		if ((READ_REG(REG_CDRC_INT_EN) & BIT_CDRC_INT_EN_LCSDE) &&
		    (status & BIT_CDRC_INT_STATUS_LCSDI)) {
			log(LOG_INFO, "%s: T1 line code signature detected!\n",
			    card->devname);
		}
		if ((READ_REG(REG_CDRC_INT_EN) & BIT_CDRC_INT_EN_ZNDE) &&
		    (status & BIT_CDRC_INT_STATUS_ZNDI)) {
			log(LOG_INFO, "%s: T1 consecutive zeros detected!\n",
			    card->devname);
		}
		status = READ_REG(REG_ALTLOS_STATUS);
		if ((status & BIT_ALTLOS_STATUS_ALTLOSI) &&
		    (status & BIT_ALTLOS_STATUS_ALTLOSE)) {
			if (status & BIT_ALTLOS_STATUS_ALTLOS) {
				if (!(card->fe_te.te_alarm &
				    BIT_ALTLOS_ALARM)) {
					log(LOG_INFO, "%s: T1 ALTLOS ON\n",
							card->devname);
					card->fe_te.te_alarm |=
							BIT_ALTLOS_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_ALTLOS_ALARM) {
					log(LOG_INFO, "%s: T1 ALTLOS OFF\n",
					    card->devname);
					card->fe_te.te_alarm &=
					    ~BIT_ALTLOS_ALARM;
				}
			}
		}
	}

	/* 14. PMON */
	if (intr_src1 & BIT_INT_SRC_1_PMON) {
		status = READ_REG(REG_PMON_INT_EN_STATUS);
		if (status & BIT_PMON_INT_EN_STATUS_XFER) {
			log(LOG_DEBUG, "%s: T1 Updating PMON counters...\n",
			    card->devname);
			sdla_te_pmon(card);
		}
	}

	/* 9. SIGX */
	if (intr_src1 & BIT_INT_SRC_1_SIGX) {
		unsigned char SIGX_chg_30_25;
		unsigned char SIGX_chg_24_17;
		unsigned char SIGX_chg_16_9;
		unsigned char SIGX_chg_8_1;

		SIGX_chg_30_25 = READ_REG(REG_SIGX_CFG);
		SIGX_chg_24_17= READ_REG(REG_SIGX_TIMESLOT_IND_STATUS);
		SIGX_chg_16_9 = READ_REG(REG_SIGX_TIMESLOT_IND_ACCESS);
		SIGX_chg_8_1 = READ_REG(REG_SIGX_TIMESLOT_IND_DATA_BUFFER);

	}

	/* 5. IBCD */
	card->fe_te.te_alarm &= ~(BIT_LOOPUP_CODE|BIT_LOOPDOWN_CODE);
	if (intr_src3 & BIT_INT_SRC_3_IBCD) {
		status = READ_REG(REG_IBCD_INT_EN_STATUS);
		if (status & BIT_IBCD_INT_EN_STATUS_LBAI) {
			card->fe_te.te_alarm |= BIT_LOOPUP_CODE;
		}
		if (status & BIT_IBCD_INT_EN_STATUS_LBDI) {
			card->fe_te.te_alarm |= BIT_LOOPDOWN_CODE;
		}
	}
#if 0
	/* 4. RJAT */
	if (intr_src1 & BIT_INT_SRC_1_RJAT) {
	}
	/* 10. RX-ELST */
	if (intr_src2 & BIT_INT_SRC_2_RX_ELST) {
	}
	/* 11. RDLC-1 */
	if (intr_src2 & BIT_INT_SRC_2_RDLC_1) {
	}
	/* 12. RDLC-2 */
	if (intr_src2 & BIT_INT_SRC_2_RDLC_2) {
	}
	/* 13. RDLC-3 */
	if (intr_src2 & BIT_INT_SRC_2_RDLC_3) {
	}
#endif

	return;
}


/*
 * Read tx interrupt.
 *
 * Arguments:	card		- pointer to device structure.
 * Returns: None.
 */
static void
sdla_e1_rx_intr(sdla_t *card)
{
	unsigned char intr_src1 = 0x00, intr_src2 = 0x00, intr_src3 = 0x00;
	unsigned char int_status = 0x00, status = 0x00;

	intr_src1 = READ_REG(REG_INT_SRC_1);
	intr_src2 = READ_REG(REG_INT_SRC_2);
	intr_src3 = READ_REG(REG_INT_SRC_3);
	if (!(intr_src1 & BITS_RX_INT_SRC_1 ||
		intr_src2 & BITS_RX_INT_SRC_2 ||
		intr_src3 & BITS_RX_INT_SRC_3))
		return;

	/* 4. FRMR */
	if (intr_src1 & BIT_INT_SRC_1_FRMR) {
		/* Register 0x94h E1 FRMR */
		int_status = READ_REG(REG_E1_FRMR_FRM_STAT_INT_IND);
		/* Register 0x96h E1 FRMR Status */
		status = READ_REG(REG_E1_FRMR_FR_STATUS);
		if ((READ_REG(REG_E1_FRMR_FRM_STAT_INT_EN) &
		    BIT_E1_FRMR_FRM_STAT_INT_EN_OOFE) &&
		    (int_status & BIT_E1_FRMR_FRM_STAT_INT_IND_OOFI)) {
			if (status & BIT_E1_FRMR_FR_STATUS_OOFV) {
				if (!(card->fe_te.te_alarm & BIT_OOF_ALARM)) {
					log(LOG_INFO, "%s: E1 OOF ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_OOF_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_OOF_ALARM) {
					log(LOG_INFO, "%s: E1 OOF OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_OOF_ALARM;
				}
			}
		}

		if ((READ_REG(REG_E1_FRMR_FRM_STAT_INT_EN) &
		    BIT_E1_FRMR_FRM_STAT_INT_EN_OOSMFE) &&
		    (int_status & BIT_E1_FRMR_FRM_STAT_INT_IND_OOSMFI)) {
			if (status & BIT_E1_FRMR_FR_STATUS_OOSMFV) {
				log(LOG_INFO, "%s: E1 OOSMF ON\n",
				    card->devname);
				card->fe_te.te_alarm |= BIT_OOSMF_ALARM;
			} else {
				log(LOG_INFO, "%s: E1 OOSMF OFF\n",
				    card->devname);
				card->fe_te.te_alarm &= ~BIT_OOSMF_ALARM;
			}
		}

		if ((READ_REG(REG_E1_FRMR_FRM_STAT_INT_EN) &
		    BIT_E1_FRMR_FRM_STAT_INT_EN_OOCMFE) &&
		    (int_status & BIT_E1_FRMR_FRM_STAT_INT_IND_OOCMFI)) {
			if (status & BIT_E1_FRMR_FR_STATUS_OOCMFV) {
				log(LOG_INFO, "%s: E1 OOCMF ON\n",
				    card->devname);
				card->fe_te.te_alarm |= BIT_OOCMF_ALARM;
			} else {
				log(LOG_INFO, "%s: E1 OOCMF OFF\n",
				    card->devname);
				card->fe_te.te_alarm &= ~BIT_OOCMF_ALARM;
			}
		}

		/* Register 0x9Fh E1 FRMR */
		status = READ_REG(REG_E1_FRMR_P_A_INT_STAT);
		if ((READ_REG(REG_E1_FRMR_P_A_INT_EN) &
		    BIT_E1_FRMR_P_A_INT_EN_OOOFE) &&
		    (status & BIT_E1_FRMR_P_A_INT_STAT_OOOFI)) {
			if (READ_REG(REG_E1_FRMR_FR_STATUS) &
			    BIT_E1_FRMR_FR_STATUS_OOOFV) {
				log(LOG_INFO, "%s: E1 OOOF ON\n",
				    card->devname);
				card->fe_te.te_alarm |= BIT_OOOF_ALARM;
			} else {
				log(LOG_INFO, "%s: E1 OOOF OFF\n",
				    card->devname);
				card->fe_te.te_alarm &= ~BIT_OOOF_ALARM;
			}
		}

		/* Register 0x95h E1 FRMR */
		int_status = READ_REG(REG_E1_FRMR_M_A_INT_IND);
		if (int_status & (BIT_E1_FRMR_M_A_INT_IND_REDI |
		    BIT_E1_FRMR_M_A_INT_IND_AISI)) {
			status = READ_REG(REG_E1_FRMR_MAINT_STATUS);
			if ((READ_REG(REG_E1_FRMR_M_A_INT_EN) &
			    BIT_E1_FRMR_M_A_INT_EN_REDE) &&
			    (int_status & BIT_E1_FRMR_M_A_INT_IND_REDI)) {
				if (status & BIT_E1_FRMR_MAINT_STATUS_RED) {
					log(LOG_INFO, "%s: E1 RED ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_RED_ALARM;
				} else {
					log(LOG_INFO, "%s: E1 RED OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_RED_ALARM;
				}
			}
			if ((READ_REG(REG_E1_FRMR_M_A_INT_EN) &
			    BIT_E1_FRMR_M_A_INT_EN_AISE) &&
			    (int_status & BIT_E1_FRMR_M_A_INT_IND_AISI)) {
				if (status & BIT_E1_FRMR_MAINT_STATUS_AIS) {
					log(LOG_INFO, "%s: E1 AIS ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_AIS_ALARM;
				} else {
					log(LOG_INFO, "%s: E1 AIS OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_AIS_ALARM;
				}
			}
			if ((READ_REG(REG_E1_FRMR_M_A_INT_EN) &
			    BIT_E1_FRMR_M_A_INT_EN_RAIE) &&
			    (int_status & BIT_E1_FRMR_M_A_INT_IND_RAII)) {
				if (status & BIT_E1_FRMR_MAINT_STATUS_RAIV) {
					log(LOG_INFO, "%s: E1 RAI ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_RAI_ALARM;
				} else {
					log(LOG_INFO, "%s: E1 RAI OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_RAI_ALARM;
				}
			}
		}
	}

	/* 1. RLPS */
	if (intr_src3 & BIT_INT_SRC_3_RLPS) {
		status = READ_REG(REG_RLPS_CFG_STATUS);
		if ((status & BIT_RLPS_CFG_STATUS_ALOSE) &&
		    (status & BIT_RLPS_CFG_STATUS_ALOSI)) {
			if (status & BIT_RLPS_CFG_STATUS_ALOSV) {
				if (!(card->fe_te.te_alarm & BIT_ALOS_ALARM)) {
					log(LOG_INFO, "%s: E1 ALOS ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_ALOS_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_ALOS_ALARM) {
					log(LOG_INFO, "%s: E1 ALOS is OFF\n",
					    card->devname);
					card->fe_te.te_alarm &=
							~BIT_ALOS_ALARM;
				}
			}
		}
	}

	/* 2. CDRC */
	if (intr_src1 & BIT_INT_SRC_1_CDRC) {
		status = READ_REG(REG_CDRC_INT_STATUS);
		if ((READ_REG(REG_CDRC_INT_EN) & BIT_CDRC_INT_EN_LOSE) &&
		    (status & BIT_CDRC_INT_STATUS_LOSI)) {
			if (status & BIT_CDRC_INT_STATUS_LOSV) {
				if (!(card->fe_te.te_alarm & BIT_LOS_ALARM)) {
					log(LOG_INFO, "%s: E1 LOS is ON\n",
					    card->devname);
					card->fe_te.te_alarm |= BIT_LOS_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_LOS_ALARM) {
					log(LOG_INFO, "%s: E1 LOS is OFF\n",
					    card->devname);
					card->fe_te.te_alarm &= ~BIT_LOS_ALARM;
				}
			}
		}
		if ((READ_REG(REG_CDRC_INT_EN) & BIT_CDRC_INT_EN_LCVE) &&
		    (status & BIT_CDRC_INT_STATUS_LCVI)) {
			log(LOG_INFO, "%s: E1 line code violation!\n",
			    card->devname);
		}
		if ((READ_REG(REG_CDRC_INT_EN) & BIT_CDRC_INT_EN_LCSDE) &&
		    (status & BIT_CDRC_INT_STATUS_LCSDI)) {
			log(LOG_INFO, "%s: E1 line code signature detected!\n",
			    card->devname);
		}
		if ((READ_REG(REG_CDRC_INT_EN) & BIT_CDRC_INT_EN_ZNDE) &&
		    (status & BIT_CDRC_INT_STATUS_ZNDI)) {
			log(LOG_INFO, "%s: E1 consecutive zeros detected!\n",
			    card->devname);
		}
		status = READ_REG(REG_ALTLOS_STATUS);
		if ((status & BIT_ALTLOS_STATUS_ALTLOSI) &&
		    (status & BIT_ALTLOS_STATUS_ALTLOSE)) {
			if (status & BIT_ALTLOS_STATUS_ALTLOS) {
				if (!(card->fe_te.te_alarm &
				    BIT_ALTLOS_ALARM)) {
					log(LOG_INFO, "%s: E1 ALTLOS is ON\n",
					    card->devname);
					card->fe_te.te_alarm |=
					    BIT_ALTLOS_ALARM;
				}
			} else {
				if (card->fe_te.te_alarm & BIT_ALTLOS_ALARM) {
					log(LOG_INFO, "%s: E1 ALTLOS is OFF\n",
					    card->devname);
					card->fe_te.te_alarm &=
					    ~BIT_ALTLOS_ALARM;
				}
			}
		}
	}
	/* 11. PMON */
	if (intr_src1 & BIT_INT_SRC_1_PMON) {
		status = READ_REG(REG_PMON_INT_EN_STATUS);
		if (status & BIT_PMON_INT_EN_STATUS_XFER) {
			sdla_te_pmon(card);
		}
	}
#if 0
	/* 3. RJAT */
	if (intr_src1 & BIT_INT_SRC_1_RJAT) {
	}
	/* 5. SIGX */
	if (intr_src1 & BIT_INT_SRC_1_SIGX) {
	}
	/* 6. RX-ELST */
	if (intr_src2 & BIT_INT_SRC_2_RX_ELST) {
	}
	/* 7. PRGD */
	if (intr_src1 & BIT_INT_SRC_1_PRGD) {
	}
	/* 8. RDLC-1 */
	if (intr_src2 & BIT_INT_SRC_2_RDLC_1) {
	}
	/* 9. RDLC-2 */
	if (intr_src2 & BIT_INT_SRC_2_RDLC_2) {
	}
	/* 10. RDLC-3 */
	if (intr_src2 & BIT_INT_SRC_2_RDLC_3) {
	}
#endif
	if (!(READ_REG(REG_RLPS_CFG_STATUS) & BIT_RLPS_CFG_STATUS_ALOSV)) {
		card->fe_te.te_alarm &= ~BIT_ALOS_ALARM;
	}
	return;
}

/*
 * Set T1/E1 loopback modes.
 */
int
sdla_set_te1_lb_modes(void *arg, unsigned char type, unsigned char mode)
{
	sdla_t *card = (sdla_t*)arg;
	int	err = 1;

	WAN_ASSERT(card->write_front_end_reg == NULL);
	WAN_ASSERT(card->read_front_end_reg == NULL);
	switch (type) {
	case WAN_TE1_LINELB_MODE:
		err = sdla_te_linelb(card, mode);
		break;
	case WAN_TE1_PAYLB_MODE:
		err = sdla_te_paylb(card, mode);
		break;
	case WAN_TE1_DDLB_MODE:
		err = sdla_te_ddlb(card, mode);
		break;
	case WAN_TE1_TX_LB_MODE:
		err = sdla_te_lb(card, mode);
		break;
	}

	return err;
}

/*
 * Activate/Deactivate Line Loopback mode.
 */
static int
sdla_te_linelb(sdla_t *card, unsigned char mode)
{
	WAN_ASSERT(card->write_front_end_reg == NULL);
	WAN_ASSERT(card->read_front_end_reg == NULL);
	if (mode == WAN_TE1_ACTIVATE_LB) {
		log(LOG_INFO, "%s: %s Line Loopback mode activated.\n",
			card->devname,
			(IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1"));
		WRITE_REG(REG_MASTER_DIAG,
			READ_REG(REG_MASTER_DIAG) | BIT_MASTER_DIAG_LINELB);
	} else {
		log(LOG_INFO, "%s: %s Line Loopback mode deactivated.\n",
			card->devname,
			(IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1"));
		WRITE_REG(REG_MASTER_DIAG,
			READ_REG(REG_MASTER_DIAG) & ~BIT_MASTER_DIAG_LINELB);
	}
	return 0;
}

/*
 * Activate/Deactivate Payload loopback mode.
 */
static int
sdla_te_paylb(sdla_t *card, unsigned char mode)
{
	WAN_ASSERT(card->write_front_end_reg == NULL);
	WAN_ASSERT(card->read_front_end_reg == NULL);
	if (mode == WAN_TE1_ACTIVATE_LB) {
		log(LOG_INFO, "%s: %s Payload Loopback mode activated.\n",
		    card->devname, (IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1"));
		WRITE_REG(REG_MASTER_DIAG,
			READ_REG(REG_MASTER_DIAG) | BIT_MASTER_DIAG_PAYLB);
	} else {
		log(LOG_INFO, "%s: %s Payload Loopback mode deactivated.\n",
		    card->devname, (IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1"));
		WRITE_REG(REG_MASTER_DIAG,
		    READ_REG(REG_MASTER_DIAG) & ~BIT_MASTER_DIAG_PAYLB);
	}
	return 0;
}

/*
 * Description: Activate/Deactivate Diagnostic Digital loopback mode.
 */
static int
sdla_te_ddlb(sdla_t *card, unsigned char mode)
{
	WAN_ASSERT(card->write_front_end_reg == NULL);
	WAN_ASSERT(card->read_front_end_reg == NULL);
	if (mode == WAN_TE1_ACTIVATE_LB) {
		log(LOG_INFO, "%s: %s Diagnostic Dig. LB mode activated.\n",
		    card->devname, (IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1"));
		WRITE_REG(REG_MASTER_DIAG,
			READ_REG(REG_MASTER_DIAG) | BIT_MASTER_DIAG_DDLB);
	} else {
		log(LOG_INFO, "%s: %s Diagnostic Dig. LB mode deactivated.\n",
		    card->devname, (IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1"));
		WRITE_REG(REG_MASTER_DIAG,
		    READ_REG(REG_MASTER_DIAG) & ~BIT_MASTER_DIAG_DDLB);
	}
	return 0;
}

void
sdla_te_timer(void *card_id)
{
	sdla_t *card = (sdla_t*)card_id;

	if (bit_test((u_int8_t*)&card->fe_te.te_critical,TE_TIMER_KILL)) {
		bit_clear((u_int8_t*)&card->fe_te.te_critical,TE_TIMER_RUNNING);
		return;
	}
	/*WAN_ASSERT1(card->te_enable_timer == NULL); */
	/* Enable hardware interrupt for TE1 */
	if (card->te_enable_timer) {
		card->te_enable_timer(card);
	} else {
		sdla_te_polling(card);
	}

	return;
}

/*
 * Enable software timer interrupt in delay ms.
 */
static void
sdla_te_enable_timer(sdla_t *card, unsigned long delay)
{

	WAN_ASSERT1(card == NULL);
	if (bit_test((u_int8_t*)&card->fe_te.te_critical, TE_TIMER_KILL)) {
		bit_clear((u_int8_t*)&card->fe_te.te_critical,
					TE_TIMER_RUNNING);
		return;
	}
	bit_set((u_int8_t*)&card->fe_te.te_critical, TE_TIMER_RUNNING);

	timeout_add(&card->fe_te.te_timer, delay * hz / 1000);
	return;
}

/*
 * Description: Process T1/E1 polling function.
 */
void
sdla_te_polling(void *card_id)
{
	sdla_t*		card = (sdla_t*)card_id;

	WAN_ASSERT1(card->write_front_end_reg == NULL);
	WAN_ASSERT1(card->read_front_end_reg == NULL);
	bit_clear((u_int8_t*)&card->fe_te.te_critical, TE_TIMER_RUNNING);
	switch (card->fe_te.te_timer_cmd) {
	case TE_LINELB_TIMER:
		if (IS_T1(&card->fe_te.te_cfg)) {
			/* Sending T1 activation/deactivation LB signal */
			if (card->fe_te.te_tx_lb_cnt > 10) {
				WRITE_REG(REG_T1_XBOC_CODE,
					(card->fe_te.te_tx_lb_cmd ==
						WAN_TE1_ACTIVATE_LB) ?
						LINELB_ACTIVATE_CODE :
						LINELB_DEACTIVATE_CODE);
			} else {
				WRITE_REG(REG_T1_XBOC_CODE,
						LINELB_DS1LINE_ALL);
			}
			if (--card->fe_te.te_tx_lb_cnt) {
				sdla_te_enable_timer(card, LINELB_TE1_TIMER);
			} else {
				log(LOG_DEBUG, "%s: TX T1 LB %s signal.\n",
				    card->devname,
				    (card->fe_te.te_tx_lb_cmd ==
				    WAN_TE1_ACTIVATE_LB) ?
				    "activation" : "deactivation");
				card->fe_te.te_tx_lb_cmd = 0x00;
				bit_clear((u_int8_t*)&card->fe_te.te_critical,
				    TE_TIMER_RUNNING);
			}
		}
		break;

	case TE_SET_INTR:
		sdla_te_set_intr(card);
		break;

	case TE_LINKDOWN_TIMER:
		if ((READ_REG(REG_RLPS_ALOS_DET_PER) &&
		    (READ_REG(REG_RLPS_CFG_STATUS) &
		    BIT_RLPS_CFG_STATUS_ALOSV)) ||
		    (IS_E1(&card->fe_te.te_cfg) &&
		    (READ_REG(REG_E1_FRMR_FR_STATUS) &
		    BIT_E1_FRMR_FR_STATUS_OOFV)) ||
		    (IS_T1(&card->fe_te.te_cfg) &&
		    (READ_REG(REG_T1_FRMR_INT_STATUS) &
		    ~BIT_T1_FRMR_INT_STATUS_INFR))) {
			sdla_te_enable_timer(card, POLLING_TE1_TIMER);
		} else {
			/* All other interrupt reports status changed
			 * through interrupts, we don't need to read
			 * these values here */
			sdla_te_set_status(card, card->fe_te.te_alarm);
			if (card->front_end_status == FE_CONNECTED) {
				card->fe_te.te_timer_cmd = TE_LINKUP_TIMER;
				sdla_te_enable_timer(card, POLLING_TE1_TIMER);
			}
		}
		break;

	case TE_LINKUP_TIMER:
		/* ALEX: 
		 * Do not update protocol front end state from 
		 * TE_LINKDOWN_TIMER because it cause to stay
		 * more longer in interrupt handler (critical for XILINX
		 * code) */
		if (card->te_link_state) {
			card->te_link_state(card);
		}
		break;
	}
	return;
}

/*
 * Description: Transmit loopback signal to remote side.
 */
static int
sdla_te_lb(sdla_t *card, unsigned char mode)
{
	WAN_ASSERT(card->write_front_end_reg == NULL);
	WAN_ASSERT(card->read_front_end_reg == NULL);

	if (!IS_T1(&card->fe_te.te_cfg)) {
		return 1;
	}
	if (card->front_end_status != FE_CONNECTED) {
		return 1;
	}
	if (bit_test((u_int8_t*)&card->fe_te.te_critical,TE_TIMER_RUNNING))
		return 1;
	if (bit_test((u_int8_t*)&card->fe_te.te_critical,LINELB_WAITING)) {
		log(LOG_DEBUG, "%s: Waiting for loopback signal!\n",
		    card->devname);
	}
	log(LOG_DEBUG, "%s: Sending %s loopback %s signal...\n",
	    card->devname, (IS_T1(&card->fe_te.te_cfg) ? "T1" : "E1"),
	    (mode == WAN_TE1_ACTIVATE_LB) ?  "activation" : "deactivation");
	card->fe_te.te_tx_lb_cmd = mode;
	card->fe_te.te_tx_lb_cnt = LINELB_CODE_CNT + LINELB_CHANNEL_CNT;
	card->fe_te.te_timer_cmd = TE_LINELB_TIMER;
	bit_set((u_int8_t*)&card->fe_te.te_critical, LINELB_WAITING);
	bit_set((u_int8_t*)&card->fe_te.te_critical, LINELB_CODE_BIT);
	bit_set((u_int8_t*)&card->fe_te.te_critical, LINELB_CHANNEL_BIT);
	sdla_te_enable_timer(card, LINELB_TE1_TIMER);

	return 0;
}

int
sdla_te_udp(void *card_id, void *cmd, unsigned char *data)
{
	sdla_t		*card = (sdla_t*)card_id;
	wan_cmd_t	*udp_cmd = (wan_cmd_t*)cmd;
	int	err = 0;

	switch (udp_cmd->wan_cmd_command) {
	case WAN_GET_MEDIA_TYPE:
		data[0] =
		    IS_T1(&card->fe_te.te_cfg) ? WAN_MEDIA_T1 :
		    IS_E1(&card->fe_te.te_cfg) ? WAN_MEDIA_E1 :
		    WAN_MEDIA_NONE;
		udp_cmd->wan_cmd_return_code = WAN_CMD_OK;
		udp_cmd->wan_cmd_data_len = sizeof(unsigned char);
		break;

	case WAN_FE_SET_LB_MODE:
		/* Activate/Deactivate Line Loopback modes */
		err = sdla_set_te1_lb_modes(card, data[0], data[1]);
		udp_cmd->wan_cmd_return_code =
		    (!err) ? WAN_CMD_OK : WAN_UDP_FAILED_CMD;
		udp_cmd->wan_cmd_data_len = 0x00;
		break;

	case WAN_FE_GET_STAT:
		/* TE1_56K Read T1/E1/56K alarms */
		*(unsigned long *)&data[0] = sdla_te_alarm(card, 0);
		/* TE1 Update T1/E1 perfomance counters */
		sdla_te_pmon(card);
		memcpy(&data[sizeof(unsigned long)],
		    &card->fe_te.te_pmon, sizeof(pmc_pmon_t));
		udp_cmd->wan_cmd_return_code = WAN_CMD_OK;
		udp_cmd->wan_cmd_data_len =
			sizeof(unsigned long) + sizeof(pmc_pmon_t);

		break;

	case WAN_FE_FLUSH_PMON:
		/* TE1 Flush T1/E1 pmon counters */
		sdla_flush_te1_pmon(card);
		udp_cmd->wan_cmd_return_code = WAN_CMD_OK;
		break;

	case WAN_FE_GET_CFG:
		/* Read T1/E1 configuration */
		memcpy(&data[0], &card->fe_te.te_cfg, sizeof(sdla_te_cfg_t));
		udp_cmd->wan_cmd_return_code = WAN_CMD_OK;
		udp_cmd->wan_cmd_data_len = sizeof(sdla_te_cfg_t);
		break;

	default:
		udp_cmd->wan_cmd_return_code = WAN_UDP_INVALID_CMD;
		udp_cmd->wan_cmd_data_len = 0;
		break;
	}
	return 0;
}


void
aft_green_led_ctrl(void *card_id, int mode)
{
	sdla_t *card = (sdla_t*)card_id;
	unsigned char led;

	if (!card->read_front_end_reg ||
	    !card->write_front_end_reg) {
		return;
	}

	led= READ_REG(REG_GLOBAL_CFG);

	if (mode == AFT_LED_ON) {
		led&=~(BIT_GLOBAL_PIO);
	} else if (mode == AFT_LED_OFF) {
		led|=BIT_GLOBAL_PIO;
	} else {
		if (led&BIT_GLOBAL_PIO) {
			led&=~(BIT_GLOBAL_PIO);
		} else {
			led|=BIT_GLOBAL_PIO;
		}
	}

	WRITE_REG(REG_GLOBAL_CFG,led);
}
