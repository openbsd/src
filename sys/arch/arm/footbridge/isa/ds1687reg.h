/*	$OpenBSD: ds1687reg.h,v 1.1 2004/02/01 05:09:49 drahn Exp $	*/
/*	$NetBSD: ds1687reg.h,v 1.1 2002/02/10 12:26:01 chris Exp $	*/

/*
 * Copyright (c) 1998 Mark Brinicombe.
 * Copyright (c) 1998 Causality Limited.
 * All rights reserved.
 *
 * Written by Mark Brinicombe, Causality Limited
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUASLITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define RTC_ADDR		0x72
#define RTC_ADDR_REG		0x00
#define RTC_DATA_REG		0x01

#define	RTC_SECONDS		0x00
#define	RTC_SECONDS_ALARM	0x01
#define RTC_MINUTES		0x02
#define RTC_MINUTES_ALARM	0x03
#define	RTC_HOURS		0x04
#define	RTC_HOURS_ALARM		0x05
#define	RTC_DAYOFWEEK		0x06
#define	RTC_DAYOFMONTH		0x07
#define	RTC_MONTH		0x08
#define RTC_YEAR		0x09

#define RTC_REG_A		0x0a
#define  RTC_REG_A_UIP		0x80	/* Update In Progress */
#define  RTC_REG_A_DV2		0x40	/* Countdown CHain */
#define  RTC_REG_A_DV1		0x20	/* Oscillator Enable */
#define  RTC_REG_A_DV0		0x10	/* Bank Select */
#define  RTC_REG_A_BANK_MASK	RTC_REG_A_DV0
#define  RTC_REG_A_BANK1	RTC_REG_A_DV0
#define  RTC_REG_A_BANK0	0x00
#define  RTC_REG_A_RS_MASK	0x0f	/* Rate select mask */
#define  RTC_REG_A_RS_NONE	0x00
#define  RTC_REG_A_RS_256HZ_1	0x01
#define  RTC_REG_A_RS_128HZ_1	0x02
#define  RTC_REG_A_RS_8192HZ	0x03
#define  RTC_REG_A_RS_4096HZ	0x04
#define  RTC_REG_A_RS_2048HZ	0x05
#define  RTC_REG_A_RS_1024HZ	0x06
#define  RTC_REG_A_RS_512HZ	0x07
#define  RTC_REG_A_RS_256HZ	0x08
#define  RTC_REG_A_RS_128HZ	0x09
#define  RTC_REG_A_RS_64HZ	0x0A
#define  RTC_REG_A_RS_32HZ	0x0B
#define  RTC_REG_A_RS_16HZ	0x0C
#define  RTC_REG_A_RS_8HZ	0x0D
#define  RTC_REG_A_RS_4HZ	0x0E
#define  RTC_REG_A_RS_2HZ	0x0F

#define RTC_REG_B		0x0b
#define  RTC_REG_B_SET		0x80	/* Inhibit update */
#define  RTC_REG_B_PIE		0x40	/* Periodic Interrupt Enable */
#define  RTC_REG_B_AIE		0x20	/* Alarm Interrupt Enable */
#define  RTC_REG_B_UIE		0x10	/* Updated Ended Interrupt Enable */
#define  RTC_REG_B_SQWE		0x08	/* Square Wave Enable */
#define  RTC_REG_B_DM		0x04	/* Data Mode */
#define  RTC_REG_B_BINARY	RTC_REG_B_DM
#define  RTC_REG_B_BCD		0
#define  RTC_REG_B_24_12	0x02	/* Hour format */
#define  RTC_REG_B_24_HOUR	RTC_REG_B_24_12
#define  RTC_REG_B_12_HOUR	0
#define  RTC_REG_B_DSE		0x01	/* Daylight Savings Enable */

#define RTC_REG_C		0x0c
#define  RTC_REG_C_IRQF		0x80	/* Interrupt Request Flag */
#define  RTC_REG_C_PF		0x40	/* Periodic Interrupt Flag */
#define  RTC_REG_C_AF		0x20	/* Alarm Interrupt Flag */
#define  RTC_REG_C_UF		0x10	/* Update Ended Flags */

#define RTC_REG_D		0x0d
#define  RTC_REG_D_VRT		0x80	/* Valid RAM and Time */

#define RTC_PC_RAM_START	0x0e
#define RTC_PC_RAM_SIZE		50

#define RTC_BANK0_RAM_START	0x40
#define RTC_BANK0_RAM_SIZE	0x40

#define RTC_MODEL		0x40
#define RTC_SERIAL_1		0x41
#define RTC_SERIAL_2		0x42
#define RTC_SERIAL_3		0x43
#define RTC_SERIAL_4		0x44
#define RTC_SERIAL_5		0x45
#define RTC_SERIAL_6		0x46
#define RTC_CRC			0x47
#define RTC_CENTURY		0x48
#define RTC_DATE_ALARM		0x49
#define RTC_REG_4A		0x4a
#define  RTC_REG_4A_VRT2	0x80
#define  RTC_REG_4A_INCR	0x40
#define  RTC_REG_4A_PAB		0x08
#define  RTC_REG_4A_RF		0x04
#define RTC_REG_4B		0x4b
#define RTC_EXT_RAM_ADDRESS	0x50
#define RTC_EXT_RAM_DATA	0x53
#define RTC_EXT_RAM_START	0x00
#define RTC_EXT_RAM_SIZE	0x80
