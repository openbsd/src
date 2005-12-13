/*	$OpenBSD: ki2cvar.h,v 1.4 2005/12/13 19:21:45 kettenis Exp $	*/

/*-
 * Copyright (c) 2001 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef KI2CVAR_H
#define KI2CVAR_H

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <dev/i2c/i2cvar.h>

/* Keywest I2C Register offsets */
#define MODE	0
#define CONTROL	1
#define STATUS	2
#define ISR	3
#define IER	4
#define ADDR	5
#define SUBADDR	6
#define DATA	7

/* MODE */
#define I2C_SPEED	0x03	/* Speed mask */
#define  I2C_100kHz	0x00
#define  I2C_50kHz	0x01
#define  I2C_25kHz	0x02
#define I2C_MODE	0x0c	/* Mode mask */
#define  I2C_DUMBMODE	0x00	/*  Dumb mode */
#define  I2C_STDMODE	0x04	/*  Standard mode */
#define  I2C_STDSUBMODE	0x08	/*  Standard mode + sub address */
#define  I2C_COMBMODE	0x0c	/*  Combined mode */
#define I2C_PORT	0xf0	/* Port mask */
#define  I2C_BUS1	0x10	/* choose Bus 1 */

/* CONTROL */
#define I2C_CT_AAK	0x01	/* Send AAK */
#define I2C_CT_ADDR	0x02	/* Send address(es) */
#define I2C_CT_STOP	0x04	/* Send STOP */
#define I2C_CT_START	0x08	/* Send START */

/* STATUS */
#define I2C_ST_BUSY	0x01	/* Busy */
#define I2C_ST_LASTAAK	0x02	/* Last AAK */
#define I2C_ST_LASTRW	0x04	/* Last R/W */
#define I2C_ST_SDA	0x08	/* SDA */
#define I2C_ST_SCL	0x10	/* SCL */

/* ISR/IER */
#define I2C_INT_DATA	0x01	/* Data byte sent/received */
#define I2C_INT_ADDR	0x02	/* Address sent */
#define I2C_INT_STOP	0x04	/* STOP condition sent */
#define I2C_INT_START	0x08	/* START condition sent */

/* I2C flags */
#define I2C_BUSY	0x01
#define I2C_READING	0x02
#define I2C_ERROR	0x04

#define KI2C_MAX_BUSSES	2

struct ki2c_softc {
	struct device sc_dev;
	paddr_t sc_paddr;
	u_char *sc_reg;
	int sc_regstep;

	struct lock sc_buslock;
	struct ki2c_bus {
		struct ki2c_softc *sc;
		struct i2c_controller i2c_tag;
		u_int32_t reg;
	} sc_bus[KI2C_MAX_BUSSES];

	int sc_flags;
	u_char *sc_data;
	int sc_resid;
};

#endif
