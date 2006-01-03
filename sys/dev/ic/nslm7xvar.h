/*	$OpenBSD: nslm7xvar.h,v 1.8 2006/01/03 19:30:09 kettenis Exp $	*/
/*	$NetBSD: nslm7xvar.h,v 1.10 2002/11/15 14:55:42 ad Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Squier.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _DEV_ISA_NSLM7XVAR_H_
#define _DEV_ISA_NSLM7XVAR_H_

/* ctl registers */

#define LMC_ADDR	0x05
#define LMC_DATA	0x06

/* data registers */

#define LMD_SENSORBASE	0x20	/* Sensors occupy 0x20 -- 0x2a */

#define LMD_CONFIG	0x40	/* Configuration */ 
#define LMD_ISR1	0x41	/* Interrupt Status 1 */
#define LMD_ISR2	0x42	/* Interrupt Status 2 */
#define LMD_SMI1	0x43	/* SMI Mask 1 */
#define LMD_SMI2	0x44	/* SMI Mask 2 */
#define LMD_NMI1	0x45	/* NMI Mask 1 */
#define LMD_NMI2	0x46	/* NMI Mask 2 */
#define LMD_VIDFAN	0x47	/* VID/Fan Divisor */
#define LMD_SBUSADDR	0x48	/* Serial Bus Address */
#define LMD_CHIPID	0x49	/* Chip Reset/ID */

/* misc constants */

#define LM_NUM_SENSORS	11
#define LM_ID_LM78	0x00
#define LM_ID_LM78J	0x40
#define LM_ID_LM79	0xC0
#define LM_ID_LM81	0x80
#define LM_ID_MASK	0xFE

/*
 * additional registers for the Winbond chips:
 * WB83781D: mostly lm7x compatible; extra temp sensors in bank1 & 2
 * WB83782D & WB83627HF: voltage sensors needs different handling, more FAN
 *                       dividers; mode voltage sensors, more temp sensors.
 */
#define WB_T23ADDR	0x4A	/* temp sens 2/3 I2C addr */
#define WB_PIN		0x4B	/* pin & fan3 divider */
#define WB_BANKSEL	0x4E	/* banck select register */
#define WB_BANKSEL_B0	0x00	/* select bank 0 */
#define WB_BANKSEL_B1	0x01	/* select bank 1 */
#define WB_BANKSEL_B2	0x02	/* select bank 2 */
#define WB_BANKSEL_B3	0x03	/* select bank 3 */
#define WB_BANKSEL_B4	0x04	/* select bank 4 */
#define WB_BANKSEL_B5	0x05	/* select bank 5 */
#define WB_BANKSEL_HBAC	0x80	/* hight byte access */

#define WB_VENDID	0x4F	/* vendor ID register */
#define WB_VENDID_WINBOND 0x5CA3
#define WB_VENDID_ASUS    0x12C3
/* Bank0 regs */
#define WB_BANK0_CHIPID	0x58
#define WB_CHIPID_83781		0x10
#define WB_CHIPID_83781_2	0x11
#define AS_CHIPID_99127		0x31 /* Asus W83781D clone */
#define WB_CHIPID_83782		0x30
#define WB_CHIPID_83627		0x21
#define WB_CHIPID_83627THF	0x90
#define WB_CHIPID_83697		0x60
#define WB_CHIPID_83791		0x71
#define WB_CHIPID_83791_2	0x72
#define WB_BANK0_FANBAT	0x5D
/* Bank1 regs */
#define WB_BANK1_T2H	0x50
#define WB_BANK1_T2L	0x51

/* Bank2 regs */
#define WB_BANK2_T3H	0x50
#define WB_BANK2_T3L	0x51

/* Bank4 regs 83782/83627 only */
#define WB_BANK4_T1OFF	0x54
#define WB_BANK4_T2OFF	0x55
#define WB_BANK4_T3OFF	0x56

/* Bank5 regs 83782/83627 only */
#define WB_BANK5_5VSB	0x50
#define WB_BANK5_VBAT	0x51

#define WB83781_NUM_SENSORS	13
#define WB83697_NUM_SENSORS	13
#define WB_NUM_SENSORS	15

struct lm_softc {
	struct	device sc_dev;

	int	lm_iobase;
	bus_space_tag_t lm_iot;
	bus_space_handle_t lm_ioh;

	int	sc_flags;
	struct sensor sensors[WB_NUM_SENSORS];
	u_int numsensors;
	void (*refresh_sensor_data) (struct lm_softc *);

	u_int8_t (*lm_readreg)(struct lm_softc *, int);
	void (*lm_writereg)(struct lm_softc *, int, int);
};

void lm_attach(struct lm_softc *);
int  lm_probe(bus_space_tag_t, bus_space_handle_t);

#endif /* _DEV_ISA_NSLM7XVAR_H_ */
