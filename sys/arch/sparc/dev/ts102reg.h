/*	$OpenBSD: ts102reg.h,v 1.8 2008/06/26 05:42:13 ray Exp $	*/
/*	$NetBSD: ts102reg.h,v 1.7 2002/09/29 23:23:58 wiz Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _SPARC_DEV_TS102REG_H
#define	_SPARC_DEV_TS102REG_H

/* The TS102 consumes a 256MB region of the SPARCbook 3's address space.
 */
#define TS102_OFFSET_REGISTERS		0x02000000
#define TS102_OFFSET_CARD_A_ATTR_SPACE	0x04000000
#define TS102_OFFSET_CARD_B_ATTR_SPACE	0x05000000
#define TS102_SIZE_ATTR_SPACE		0x01000000
#define TS102_OFFSET_CARD_A_IO_SPACE	0x06000000
#define TS102_OFFSET_CARD_B_IO_SPACE	0x07000000
#define TS102_SIZE_IO_SPACE		0x01000000
#define TS102_OFFSET_CARD_A_MEM_SPACE	0x08000000
#define TS102_OFFSET_CARD_B_MEM_SPACE	0x0c000000
#define TS102_SIZE_MEM_SPACE		0x04000000

/* There are two separate register blocks within the TS102.  The first
 * gives access to PCMCIA card specific resources, and the second gives
 * access to the microcontroller interface
 */
#define	TS102_REG_CARD_A_INT	0x0000	/* Card A Interrupt Register */
#define	TS102_REG_CARD_A_STS	0x0004	/* Card A Status Register */
#define	TS102_REG_CARD_A_CTL	0x0008	/* Card A Control Register */
#define	TS102_REG_CARD_B_INT	0x0010	/* Card B Interrupt Register */
#define	TS102_REG_CARD_B_STS	0x0014	/* Card B Status Register */
#define	TS102_REG_CARD_B_CTL	0x0018	/* Card B Control Register */
#define	TS102_REG_UCTRL_INT	0x0020	/* Microcontroller Interrupt Register */
#define	TS102_REG_UCTRL_DATA	0x0024	/* Microcontroller Data Register */
#define	TS102_REG_UCTRL_STS	0x0028	/* Microcontroller Status Register */

struct uctrl_regs {
	volatile u_int8_t	intr;	/* Microcontroller Interrupt Reg */
	volatile u_int8_t	filler0[3];
	volatile u_int8_t	data;	/* Microcontroller Data Reg */
	volatile u_int8_t	filler1[3];
	volatile u_int8_t	stat;	/* Microcontroller Status Reg */
	volatile u_int8_t	filler2[3];
};

/* TS102 Card Interrupt Register definitions.
 *
 * There is one 16-bit interrupt register for each card.  Each register
 * contains interrupt status (read) and clear (write) bits and an 
 * interrupt mask for each of the four interrupt sources.
 *
 * The request bit is the logical AND of the status and the mask bit,
 * and indicated and an interrupt is being requested.  The mask bits 
 * allow masking of individual interrupts.  An interrupt is enabled when
 * the mask is set to 1 and is clear by write a 1 to the associated
 * request bit.
 *
 * The card interrupt register also contain the soft reset flag.
 * Setting this bit to 1 will the SPARCbook 3 to be reset.
 */
#define	TS102_CARD_INT_RQST_IRQ				0x0001
#define	TS102_CARD_INT_RQST_WP_STATUS_CHANGED		0x0002
#define	TS102_CARD_INT_RQST_BATTERY_STATUS_CHANGED	0x0004
#define	TS102_CARD_INT_RQST_CARDDETECT_STATUS_CHANGED	0x0008
#define	TS102_CARD_INT_STATUS_IRQ			0x0010
#define	TS102_CARD_INT_STATUS_WP_STATUS_CHANGED		0x0020
#define	TS102_CARD_INT_STATUS_BATTERY_STATUS_CHANGED	0x0040
#define	TS102_CARD_INT_STATUS_CARDDETECT_STATUS_CHANGED	0x0080
#define	TS102_CARD_INT_MASK_IRQ				0x0100
#define	TS102_CARD_INT_MASK_WP_STATUS			0x0200
#define	TS102_CARD_INT_MASK_BATTERY_STATUS		0x0400
#define	TS102_CARD_INT_MASK_CARDDETECT_STATUS		0x0800
#define	TS102_CARD_INT_SOFT_RESET			0x1000

/* TS102 Card Status Register definitions.  The Card Status Register
 * contains card status and control bit.
 */
#define	TS102_CARD_STS_PRES		0x0001	/* Card Present (1) */
#define	TS102_CARD_STS_IO		0x0002	/* (1) I/O Card, (0) = Mem Card */
#define	TS102_CARD_STS_TYPE3		0x0004	/* Type-3 PCMCIA card (disk) */
#define	TS102_CARD_STS_VCC		0x0008	/* Vcc (0=5V, 1=3.3V) */
#define	TS102_CARD_STS_VPP1_MASK	0x0030	/* Programming Voltage Control2 */
#define	TS102_CARD_STS_VPP1_NC		0x0030	/*    NC */
#define	TS102_CARD_STS_VPP1_VCC		0x0020	/*    Vcc (3.3V or 5V) */
#define	TS102_CARD_STS_VPP1_VPP		0x0010	/*    Vpp (12V) */
#define	TS102_CARD_STS_VPP1_0V		0x0000	/*    0V */
#define	TS102_CARD_STS_VPP2_MASK	0x00c0	/* Programming Voltage Control1 */
#define	TS102_CARD_STS_VPP2_NC		0x00c0	/*    NC */
#define	TS102_CARD_STS_VPP2_VCC		0x0080	/*    Vcc (3.3V or 5V) */
#define	TS102_CARD_STS_VPP2_VPP		0x0040	/*    Vpp (12V) */
#define	TS102_CARD_STS_VPP2_0V		0x0000	/*    0V */
#define	TS102_CARD_STS_WP		0x0100	/* Write Protect (1) */
#define	TS102_CARD_STS_BVD_MASK		0x0600	/* Battery Voltage Detect */
#define	TS102_CARD_STS_BVD_GOOD		0x0600	/*    Battery good */
#define	TS102_CARD_STS_BVD_LOW_OK	0x0400	/*    Battery low, data OK */
#define	TS102_CARD_STS_BVD_LOW_SUSPECT1	0x0200	/*    Battery low, data suspect */
#define	TS102_CARD_STS_BVD_LOW_SUSPECT0	0x0000	/*    Battery low, data suspect */
#define	TS102_CARD_STS_LVL		0x0800	/* Level (1) / Edge */
#define	TS102_CARD_STS_RDY		0x1000	/* Ready (1) / Not Busy */
#define	TS102_CARD_STS_VCCEN		0x2000	/* Powered Up (0) */
#define	TS102_CARD_STS_RIEN		0x4000	/* Not Supported */
#define	TS102_CARD_STS_ACEN		0x8000	/* Access Enabled (1) */

/* TS102 Card Control Register definitions
 */
#define	TS102_CARD_CTL_AA_MASK		0x0003	/* Attribute Address A[25:24] */
#define	TS102_CARD_CTL_IA_MASK		0x000c	/* I/O Address A[25:24] */
#define	TS102_CARD_CTL_IA_BITPOS	2	/* */
#define	TS102_CARD_CTL_CES_MASK		0x0070	/* CE/address setup time */
#define	TS102_CARD_CTL_CES_BITPOS	4	/* n+1 clocks */
#define	TS102_CARD_CTL_OWE_MASK		0x0380	/* OE/WE width */
#define	TS102_CARD_CTL_OWE_BITPOS	7	/* n+2 clocks */
#define	TS102_CARD_CTL_CEH		0x0400	/* Chip enable hold time */
						/* (0) - 1 clock */
						/* (1) - 2 clocks */
#define	TS102_CARD_CTL_SBLE		0x0800	/* SBus little endian */
#define	TS102_CARD_CTL_PCMBE		0x1000	/* PCMCIA big endian */
#define	TS102_CARD_CTL_RAHD		0x2000	/* Read ahead enable */
#define	TS102_CARD_CTL_INCDIS		0x4000	/* Address increment disable */
#define	TS102_CARD_CTL_PWRD		0x8000	/* Power down */

/* Microcontroller Interrupt Register
 */
#define	TS102_UCTRL_INT_TXE_REQ		0x01	/* transmit FIFO empty */
#define	TS102_UCTRL_INT_TXNF_REQ	0x02	/* transmit FIFO not full */
#define	TS102_UCTRL_INT_RXNE_REQ	0x04	/* receive FIFO not empty */
#define	TS102_UCTRL_INT_RXO_REQ		0x08	/* receive FIFO overflow */
#define	TS102_UCTRL_INT_TXE_MSK		0x10	/* transmit FIFO empty */
#define	TS102_UCTRL_INT_TXNF_MSK	0x20	/* transmit FIFO not full */
#define	TS102_UCTRL_INT_RXNE_MSK	0x40	/* receive FIFO not empty */
#define	TS102_UCTRL_INT_RXO_MSK		0x80	/* receive FIFO overflow */

/* TS102 Microcontroller Data Register (only 8 bits are significant).
 */
#define	TS102_UCTRL_DATA_MASK		0xff

/* TS102 Microcontroller Status Register.
 *	read 1 if asserted
 *	write 1 to clear 
 */
#define	TS102_UCTRL_STS_TXE_STA		0x01	/* transmit FIFO empty */
#define	TS102_UCTRL_STS_TXNF_STA	0x02	/* transmit FIFO not full */
#define	TS102_UCTRL_STS_RXNE_STA	0x04	/* receive FIFO not empty */
#define	TS102_UCTRL_STS_RXO_STA		0x08	/* receive FIFO overflow */
#define	TS102_UCTRL_STS_MASK		0x0f	/* Only 4 bits significant */

enum ts102_opcode {			/* Argument	Returned */
    TS102_OP_RD_SERIAL_NUM=0x01,	/* none		ack + 4 bytes */
    TS102_OP_RD_ETHER_ADDR=0x02,	/* none		ack + 6 bytes */
    TS102_OP_RD_HW_VERSION=0x03,	/* none		ack + 2 bytes */
    TS102_OP_RD_UCTLR_VERSION=0x04,	/* none		ack + 2 bytes */
    TS102_OP_RD_MAX_TEMP=0x05,		/* none		ack + 1 bytes */
    TS102_OP_RD_MIN_TEMP=0x06,		/* none		ack + 1 bytes */
    TS102_OP_RD_CURRENT_TEMP=0x07,	/* none		ack + 1 bytes */
    TS102_OP_RD_SYSTEM_VARIANT=0x08,	/* none		ack + 4 bytes */
    TS102_OP_RD_POWERON_CYCLES=0x09,	/* none		ack + 4 bytes */
    TS102_OP_RD_POWERON_SECONDS=0x0a,	/* none		ack + 4 bytes */
    TS102_OP_RD_RESET_STATUS=0x0b,	/* none		ack + 1 bytes */
#define	TS102_RESET_STATUS_RESERVED0	0x00
#define	TS102_RESET_STATUS_POWERON	0x01
#define	TS102_RESET_STATUS_KEYBOARD	0x02
#define	TS102_RESET_STATUS_WATCHDOG	0x03
#define	TS102_RESET_STATUS_TIMEOUT	0x04
#define	TS102_RESET_STATUS_SOFTWARE	0x05
#define	TS102_RESET_STATUS_BROWNOUT	0x06
#define	TS102_RESET_STATUS_RESERVED1	0x07
    TS102_OP_RD_EVENT_STATUS=0x0c,	/* none		ack + 2 bytes */
#define	TS102_EVENT_STATUS_SHUTDOWN_REQUEST			0x0001
#define	TS102_EVENT_STATUS_LOW_POWER_WARNING			0x0002
/* Internal Warning Changed 0x0002 */
#define	TS102_EVENT_STATUS_VERY_LOW_POWER_WARNING		0x0004
/* Discharge Event 0x0004 */
#define	TS102_EVENT_STATUS_BATT_CHANGED				0x0008
/* Internal Status Changed 0x0008 */
#define	TS102_EVENT_STATUS_EXT_KEYBOARD_STATUS_CHANGE		0x0010
#define	TS102_EVENT_STATUS_EXT_MOUSE_STATUS_CHANGE		0x0020
#define	TS102_EVENT_STATUS_EXTERNAL_VGA_STATUS_CHANGE		0x0040
#define	TS102_EVENT_STATUS_LID_STATUS_CHANGE			0x0080
#define	TS102_EVENT_STATUS_MICROCONTROLLER_ERROR		0x0100
#define	TS102_EVENT_STATUS_RESERVED				0x0200
/* Wakeup 0x0200 */
#define	TS102_EVENT_STATUS_EXT_BATT_STATUS_CHANGE		0x0400
#define	TS102_EVENT_STATUS_EXT_BATT_CHARGING_STATUS_CHANGE	0x0800
#define	TS102_EVENT_STATUS_EXT_BATT_LOW_POWER			0x1000
#define	TS102_EVENT_STATUS_DC_STATUS_CHANGE			0x2000
#define	TS102_EVENT_STATUS_CHARGING_STATUS_CHANGE		0x4000
#define	TS102_EVENT_STATUS_POWERON_BTN_PRESSED			0x8000
    TS102_OP_RD_REAL_TIME_CLK=0x0d,	/* none		ack + 7 bytes */
    TS102_OP_RD_EXT_VGA_PORT=0x0e,	/* none		ack + 1 bytes */
    TS102_OP_RD_UCTRL_ROM_CKSUM=0x0f,	/* none		ack + 2 bytes */
    TS102_OP_RD_ERROR_STATUS=0x10,	/* none		ack + 2 bytes */
#define	TS102_ERROR_STATUS_NO_ERROR				0x00
#define	TS102_ERROR_STATUS_COMMAND_ERROR			0x01
#define	TS102_ERROR_STATUS_EXECUTION_ERROR			0x02
#define	TS102_ERROR_STATUS_PHYSICAL_ERROR			0x04
    TS102_OP_RD_EXT_STATUS=0x11,	/* none		ack + 2 bytes */
#define	TS102_EXT_STATUS_MAIN_POWER_AVAILABLE			0x0001
#define	TS102_EXT_STATUS_INTERNAL_BATTERY_ATTACHED		0x0002
#define	TS102_EXT_STATUS_EXTERNAL_BATTERY_ATTACHED		0x0004
#define	TS102_EXT_STATUS_EXTERNAL_VGA_ATTACHED			0x0008
#define	TS102_EXT_STATUS_EXTERNAL_KEYBOARD_ATTACHED		0x0010
#define	TS102_EXT_STATUS_EXTERNAL_MOUSE_ATTACHED		0x0020
#define	TS102_EXT_STATUS_LID_DOWN				0x0040
#define	TS102_EXT_STATUS_INTERNAL_BATTERY_CHARGING		0x0080
#define	TS102_EXT_STATUS_EXTERNAL_BATTERY_CHARGING		0x0100
#define	TS102_EXT_STATUS_INTERNAL_BATTERY_DISCHARGING		0x0200
#define	TS102_EXT_STATUS_EXTERNAL_BATTERY_DISCHARGING		0x0400
    TS102_OP_RD_USER_CONFIG=0x12,	/* none		ack + 2 bytes */
    TS102_OP_RD_UCTRL_VLT=0x13,		/* none		ack + 1 bytes */
    TS102_OP_RD_INT_BATT_VLT=0x14,	/* none		ack + 1 bytes */
    TS102_OP_RD_DC_IN_VLT=0x15,		/* none		ack + 1 bytes */
    TS102_OP_RD_HORZ_PRT_VLT=0x16,	/* none		ack + 1 bytes */
    TS102_OP_RD_VERT_PTR_VLT=0x17,	/* none		ack + 1 bytes */
    TS102_OP_RD_INT_CHARGE_RATE=0x18,	/* none		ack + 1 bytes */
    TS102_OP_RD_EXT_CHARGE_RATE=0x19,	/* none		ack + 1 bytes */
    TS102_OP_RD_RTC_ALARM=0x1a,		/* none		ack + 7 bytes */
    TS102_OP_RD_EVENT_STATUS_NO_RESET=0x1b,	/* none		ack + 2 bytes */
    TS102_OP_RD_INT_KBD_LAYOUT=0x1c,	/* none		ack + 2 bytes */
    TS102_OP_RD_EXT_KBD_LAYOUT=0x1d,	/* none		ack + 2 bytes */
    TS102_OP_RD_EEPROM_STATUS=0x1e,	/* none		ack + 2 bytes */
#define	TS102_EEPROM_STATUS_FACTORY_AREA_CHECKSUM_FAIL		0x01
#define	TS102_EEPROM_STATUS_CONSUMER_AREA_CHECKSUM_FAIL		0x02
#define	TS102_EEPROM_STATUS_USER_AREA_CHECKSUM_FAIL		0x04
#define	TS102_EEPROM_STATUS_VPD_AREA_CHECKSUM_FAIL		0x08
 
    /* Read/Write/Modify Commands
     */
    TS102_OP_CTL_LCD=0x20,		/* 4 byte mask	ack + 4 bytes */
#define	TS102_LCD_CAPS_LOCK		0x0001
#define	TS102_LCD_SCROLL_LOCK		0x0002
#define	TS102_LCD_NUMLOCK		0x0004
#define	TS102_LCD_DISK_ACTIVE		0x0008
#define	TS102_LCD_LAN_ACTIVE		0x0010
#define	TS102_LCD_WAN_ACTIVE		0x0020
#define	TS102_LCD_PCMCIA_ACTIVE		0x0040
#define	TS102_LCD_DC_OK			0x0080
#define	TS102_LCD_COMPOSE		0x0100
    TS102_OP_CTL_BITPORT=0x21,		/* mask		ack + 1 byte */
#define	TS102_BITPORT_TFTPWR		0x01	/* TFT power (low) */
#define	TS102_BITPORT_SYNCINVA		0x02	/* ext. monitor sync (low) */
#define	TS102_BITPORT_SYNCINVB		0x04	/* ext. monitor sync (low) */
#define	TS102_BITPORT_BP_DIS		0x08	/* no bootprom from pcmcia (high) */
						/* boot from pcmcia (low) */
#define	TS102_BITPORT_ENCSYNC		0x10	/* enab composite sync (low) */
#define	TS102_BITPORT_DISK_POWER	0x20	/* internal disk power (low) */
    TS102_OP_CTL_DEV=0x22,		/* mask 	ack + 1 byte */
#define TS102_DEVCTL_CHARGE_DISABLE	0x01	/* dis/en charging */
#define TS102_DEVCTL_POINTER_DISABLE	0x04	/* dis/en pointer */
#define TS102_DEVCTL_KEYCLICK		0x08	/* keyclick? */
#define TS102_DEVCTL_INT_BTNCLICK	0x10	/* internal button click? */
#define TS102_DEVCTL_EXT_BTNCLICK	0x20	/* ext. button click?? */
    TS102_OP_CTL_SPEAKER_VOLUME=0x23,	/* mask		ack + 1 byte */
    TS102_OP_CTL_TFT_BRIGHTNESS=0x24,	/* mask		ack + 1 byte */
    TS102_OP_CTL_WATCHDOG=0x25,		/* mask		ack + 1 byte */
    TS102_OP_CTL_FCTRY_EEPROM=0x26,	/* mask		ack + 1 byte */
    TS102_OP_CTL_SECURITY_KEY=0x27,	/* no idea */
    TS102_OP_CTL_KDB_TIME_UNTL_RTP=0x28, /* mask 	ack + 1 byte */
    TS102_OP_CTL_KBD_TIME_BTWN_RPTS=0x29, /* mask	ack + 1 byte */
    TS102_OP_CTL_TIMEZONE=0x2a,		/* mask		ack + 1 byte */
    TS102_OP_CTL_MARK_SPACE_RATIO=0x2b,	/* mask		ack + 1 byte */
    TS102_OP_CTL_MOUSE_SENS=0x2c, 	/* mask		ack + 1 byte */
    TS102_OP_CTL_MOUSE_SCAN=0x2d,	/* no idea invalid?*/
    TS102_OP_CTL_DIAGNOSTIC_MODE=0x2e,	/* mask		ack + 1 byte */
#define	TS102_DIAGNOSTIC_MODE_CMD_DIAG_ON_LCD	0x01
#define	TS102_DIAGNOSTIC_MODE_KDB_MS_9600	0x02
    TS102_OP_CTL_SCREEN_CONTRAST=0x2f,	/* mask		ack + 1 byte */

    /* Commands returning no status
     */
    TS102_OP_CMD_RING_BELL=0x30,	/* msb,lsb	ack */
    TS102_OP_RD_INPUT_SOURCE=0x31,	/* no idea */
    TS102_OP_CMD_DIAGNOSTIC_STATUS=0x32, /* msb,lsb	ack */
    TS102_OP_CMD_CLR_KEY_COMBO_TBL=0x33, /* none	ack */
    TS102_OP_CMD_SOFTWARE_RESET=0x34,	/* none		ack */
    TS102_OP_CMD_SET_RTC=0x35,		/* smhddmy	ack */
    TS102_OP_CMD_RECAL_PTR=0x36,	/* none		ack */
    TS102_OP_CMD_SET_BELL_FREQ=0x37,	/* msb,lsb	ack */
    TS102_OP_CMD_SET_INT_BATT_RATE=0x39, /* charge-lvl	ack */
    TS102_OP_CMD_SET_EXT_BATT_RATE=0x3a, /* charge-lvl	ack */
    TS102_OP_CMD_SET_RTC_ALARM=0x3b,	/* smhddmy	ack */

    /* Block transfer commands
     */
    TS102_OP_BLK_RD_EEPROM=0x40,	/* len off		ack <data> */
    TS102_OP_BLK_WR_EEPROM=0x41,	/* len off <data>	ack */
    TS102_OP_BLK_WR_STATUS=0x42,	/* len off <data>	ack */
    TS102_OP_BLK_DEF_SPCL_CHAR=0x43,	/* len off <8b data>	ack */
#define	TS102_BLK_OFF_DEF_WAN1			0
#define	TS102_BLK_OFF_DEF_WAN2			1
#define	TS102_BLK_OFF_DEF_LAN1			2
#define	TS102_BLK_OFF_DEF_LAN2			3
#define	TS102_BLK_OFF_DEF_PCMCIA		4
#define	TS102_BLK_OFF_DEF_DC_GOOD		5
#define	TS102_BLK_OFF_DEF_BACKSLASH		6

    /* Generic commands
     */
    TS102_OP_GEN_DEF_KEY_COMBO_ENT=0x50, /* seq com-length	ack */
    TS102_OP_GEN_DEF_STRING_TBL_ENT=0x51, /* str-code len <str>	ack */
    TS102_OP_GEN_DEF_STS_CTRN_DISP=0x52, /* len <msg>		ack */

    /* Generic commands with optional status
     */
    TS102_OP_GEN_STS_EMU_COMMAND=0x64,	/* <command>	ack */
    TS102_OP_GEN_STS_RD_EMU_REGISTER=0x65, /* reg	ack + 1 byte */
    TS102_OP_GEN_STS_WR_EMU_REGISTER=0x66, /* reg,val	ack */
    TS102_OP_GEN_STS_RD_EMU_RAM=0x67,	/* addr		ack + 1 byte */
    TS102_OP_GEN_STS_WR_EMU_RAM=0x68,	/* addr,val	ack */
    TS102_OP_GEN_STS_RD_BQ_REGISTER=0x69, /* reg	ack + 1 byte */
    TS102_OP_GEN_STS_WR_BQ_REGISTER=0x6a, /* reg,val	ack */

    /* Administration commands
     */
    TS102_OP_ADMIN_SET_USER_PASS=0x70,	/* len <pass>   ack */
    TS102_OP_ADMIN_VRFY_USER_PASS=0x71,	/* len <pass>   ack + status */
    TS102_OP_ADMIN_GET_SYSTEM_PASS=0x72, /* none	ack + <7bytekey> */
    TS102_OP_ADMIN_VRFY_SYSTEM_PASS=0x73, /* len <pass>   ack + status */
    TS102_OP_RD_INT_CHARGE_LEVEL=0x7a,	/* ack + 2 byte */
    TS102_OP_RD_EXT_CHARGE_LEVEL=0x7b,	/* ack + 2 byte */
#define	TS102_CHARGE_UNKNOWN	0xfa
    TS102_OP_SLEEP=0x80, 		/* supposedly sleeps, not sure */
    TS102_OP_ADMIN_POWER_OFF=0x82,	/* len <pass>	none */
    TS102_OP_ADMIN_POWER_RESTART=0x83,	/* msb,xx,lsb	none */
};

#define	TS102_UCTRL_ACK		0xfe
#define	TS102_UCTRL_NACK	0xfc
#define	TS102_UCTRL_INTR	0xfa

#endif /* _SPARC_DEV_TS102REG_H */
