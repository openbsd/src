/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Alex Feldman <al.feldman@sangoma.com> for SAN.
 * The code is derived from permitted modifications to software created
 * by Nenad Corbic (ncorbic@sangoma.com).
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

#ifndef __IF_SAN_FRONT_END_H_
#define __IF_SAN_FRONT_END_H_

/*
*************************************************************************
*			  DEFINES AND MACROS				*	
*************************************************************************
*/
/* The hardware media */
#define WANOPT_MEDIA_NONE       0x00    /* Regular card */
#define WANOPT_MEDIA_T1         0x01    /* T1 connection */
#define WANOPT_MEDIA_E1         0x02    /* E1 connection */
#define WANOPT_MEDIA_56K        0x03    /* 56K connection */

/* settings for the 'adapter_type' */
#define S508_ADPTR			0x0001	/* S508 */
#define S5141_ADPTR_1_CPU_SERIAL	0x0011	/* S5141, single CPU, serial */
#define S5142_ADPTR_2_CPU_SERIAL	0x0012	/* S5142, dual CPU, serial */
#define S5143_ADPTR_1_CPU_FT1		0x0013	/* S5143, single CPU, FT1 */
#define S5144_ADPTR_1_CPU_T1E1		0x0014	/* S5144, single CPU, T1/E1 */
#define S5145_ADPTR_1_CPU_56K		0x0015	/* S5145, single CPU, 56K */
#define S5147_ADPTR_2_CPU_T1E1		0x0017  /* S5147, dual CPU, T1/E1 */
#define S5148_ADPTR_1_CPU_T1E1		0x0018	/* S5148, single CPU, T1/E1 */

#define S518_ADPTR_1_CPU_ADSL		0x0018	/* S518, adsl card */

#define A101_ADPTR_T1E1_MASK		0x0040	/* T1/E1 type mask  */
#define A101_ADPTR_1TE1			0x0041	/* 1 Channel T1/E1  */
#define A101_ADPTR_2TE1			0x0042	/* 2 Channels T1/E1 */

#define A100_ADPTR_T3E3_MASK		0x0080	/* T3/E3  type mask */
#define A100_ADPTR_1_CHN_T3E3		0x0081	/* 1 Channel T3/E3 (Prototype) */
#define A105_ADPTR_1_CHN_T3E3		0x0082	/* 1 Channel T3/E3 */

#define OPERATE_T1E1_AS_SERIAL		0x8000  /* For bitstreaming only 
						 * Allow the applicatoin to 
						 * E1 front end */

#define SDLA_ADPTR_DECODE(adapter_type)			\
		(adapter_type == S5141_ADPTR_1_CPU_SERIAL) ? "S514-1-PCI" : \
		(adapter_type == S5142_ADPTR_2_CPU_SERIAL) ? "S514-2-PCI" : \
		(adapter_type == S5143_ADPTR_1_CPU_FT1)    ? "S514-3-PCI" : \
		(adapter_type == S5144_ADPTR_1_CPU_T1E1)   ? "S514-4-PCI" : \
		(adapter_type == S5145_ADPTR_1_CPU_56K)    ? "S514-5-PCI" : \
		(adapter_type == S5147_ADPTR_2_CPU_T1E1)   ? "S514-7-PCI" : \
		(adapter_type == S518_ADPTR_1_CPU_ADSL)    ? "S518-PCI  " : \
		(adapter_type == A101_ADPTR_1TE1) 	   ? "AFT-A101  " : \
		(adapter_type == A101_ADPTR_2TE1)	   ? "AFT-A102  " : \
		(adapter_type == A105_ADPTR_1_CHN_T3E3)    ? "A105-1-PCI" : \
		(adapter_type == A105_ADPTR_1_CHN_T3E3)    ? "A105-2    " : \
							     "UNKNOWN   "

/* front-end UDP command */
#define WAN_FE_GET_STAT			(WAN_FE_UDP_CMD_START + 0)
#define WAN_FE_SET_LB_MODE		(WAN_FE_UDP_CMD_START + 1)
#define WAN_FE_FLUSH_PMON		(WAN_FE_UDP_CMD_START + 2)
#define WAN_FE_GET_CFG			(WAN_FE_UDP_CMD_START + 3)

/* front-end configuration and access interface commands */
#define READ_FRONT_END_REGISTER		(WAN_FE_CMD_START+0)	/* 0x90 read from front-end register */
#define WRITE_FRONT_END_REGISTER	(WAN_FE_CMD_START+1)	/* 0x91 write to front-end register */
#define READ_FRONT_END_STATISTICS	(WAN_FE_CMD_START+2)	/* 0x92 read the front-end statistics */
#define FLUSH_FRONT_END_STATISTICS	(WAN_FE_CMD_START+3)	/* 0x93 flush the front-end statistics */

#ifdef _KERNEL

/* adapter configuration interface commands */
#define SET_ADAPTER_CONFIGURATION	(WAN_INTERFACE_CMD_START+0)	/* 0xA0 set adapter configuration */
#define READ_ADAPTER_CONFIGURATION	(WAN_INTERFACE_CMD_START+1)	/* 0xA1 read adapter configuration */

/* front-end command */
#define WAN_FE_GET_STAT			(WAN_FE_UDP_CMD_START + 0)
#define WAN_FE_SET_LB_MODE		(WAN_FE_UDP_CMD_START + 1)
#define WAN_FE_FLUSH_PMON		(WAN_FE_UDP_CMD_START + 2)
#define WAN_FE_GET_CFG			(WAN_FE_UDP_CMD_START + 3)

/* return codes from interface commands */
#define LGTH_FE_CFG_DATA_INVALID       0x91 /* the length of the FE_RX_DISC_TX_IDLE_STRUCT is invalid */
#define LGTH_ADAPTER_CFG_DATA_INVALID  0x91 /* the length of the passed configuration data is invalid */
#define INVALID_FE_CFG_DATA            0x92 /* the passed SET_FE_RX_DISC_TX_IDLE_CFG data is invalid */
#define ADPTR_OPERATING_FREQ_INVALID   0x92 /* an invalid adapter operating frequency was selected */
#define PROT_CFG_BEFORE_FE_CFG         0x93 /* set the protocol-level configuration before setting the FE configuration */

#define SET_FE_RX_DISC_TX_IDLE_CFG      0x98 /* set the front-end Rx discard/Tx idle configuration */
#define READ_FE_RX_DISC_TX_IDLE_CFG     0x99 /* read the front-end Rx discard/Tx idle configuration */
#define SET_TE1_SIGNALING_CFG		0x9A /* set the T1/E1 signaling configuration */
#define READ_TE1_SIGNALING_CFG	0x9B /* read the T1/E1 signaling configuration */


#define COMMAND_INVALID_FOR_ADAPTER    0x9F /* the command is invalid for the adapter type */

 
/* ---------------------------------------------------------------------------------
 * Constants for the SET_FE_RX_DISC_TX_IDLE_CFG/READ_FE_RX_DISC_TX_IDLE_CFG commands
 * --------------------------------------------------------------------------------*/

#define NO_ACTIVE_RX_TIME_SLOTS_T1   24 /* T1 - no active time slots used for reception */
#define NO_ACTIVE_TX_TIME_SLOTS_T1   24 /* T1 - no active time slots used for transmission */
#define NO_ACTIVE_RX_TIME_SLOTS_E1   32 /* E1 - no active time slots used for reception */
#define NO_ACTIVE_TX_TIME_SLOTS_E1   31 /* E1 - no active time slots used for transmission (channel 0 reserved for framing) */

/* Read/Write to front-end register */
#define READ_REG(reg)		card->read_front_end_reg(card, reg)
#define WRITE_REG(reg, value)	card->write_front_end_reg(card, reg, (unsigned char)(value))

/* the structure used for the SET_FE_RX_DISC_TX_IDLE_CFG/READ_FE_RX_DISC_TX_IDLE_CFG command */
#pragma pack(1)
typedef struct {
        unsigned short lgth_Rx_disc_bfr; /* the length of the Rx discard buffer */
        unsigned short lgth_Tx_idle_bfr; /* the length of the Tx idle buffer */
                                                /* the transmit idle data buffer */
        unsigned char Tx_idle_data_bfr[NO_ACTIVE_TX_TIME_SLOTS_E1];
} FE_RX_DISC_TX_IDLE_STRUCT;
#pragma pack()
                                         

/* ----------------------------------------------------------------------------
 *                       Constants for front-end access
 * --------------------------------------------------------------------------*/

/* the structure used for the READ_FRONT_END_REGISTER/WRITE_FRONT_END_REGISTER command */
#pragma pack(1)
typedef struct {
	unsigned short register_number; /* the register number to be read from or written to */
	unsigned char register_value;	/* the register value read/written */
} FRONT_END_REG_STRUCT;
#pragma pack()


/* -----------------------------------------------------------------------------
 *            Constants for the READ_FRONT_END_STATISTICS command
 * ---------------------------------------------------------------------------*/

/* the front-end statistics structure */
#pragma pack(1)
typedef struct {
	unsigned long FE_interrupt_count;   /* the number of front-end interrupts generated */
	unsigned long FE_app_timeout_count; /* the number of front-end interrupt application timeouts */
} FE_STATISTICS_STRUCT;
#pragma pack()



/* --------------------------------------------------------------------------------
 * Constants for the SET_ADAPTER_CONFIGURATION/READ_ADAPTER_CONFIGURATION commands
 * -------------------------------------------------------------------------------*/

/* the adapter configuration structure */
#pragma pack(1)
typedef struct {
	unsigned short adapter_type;	/* type of adapter */
	unsigned short adapter_config;	/* miscellaneous adapter configuration options */
	unsigned long operating_frequency;	/* adapter operating frequency */
} ADAPTER_CONFIGURATION_STRUCT;
#pragma pack()



typedef unsigned char (WRITE_FRONT_END_REG_T)(void*, unsigned short, unsigned char);
typedef unsigned char (READ_FRONT_END_REG_T)(void*, unsigned short);


enum {
   AFT_LED_ON,
   AFT_LED_OFF,
   AFT_LED_TOGGLE
};


/*
** Sangoma Front-End interface structure 
*/
typedef struct {
	unsigned long	(*get_fe_service_status)(void*);	/* In-Service or Not (T1/E1/56K) */
	void		(*print_fe_alarm)(void*,unsigned long);	/* Print Front-End alarm (T1/E1/56K) */
	char*		(*print_fe_act_channels)(void*);	/* Print Front-End alarm (T1/E1/56K) */
	void		(*set_fe_alarm)(void*,unsigned long);	/* Set Front-End alarm (T1/E1) */
} sdla_fe_iface_t;


#endif	/* _KERNEL */

#endif
