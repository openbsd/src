/*	$OpenBSD: auxioreg.h,v 1.5 2005/07/08 12:36:38 miod Exp $	*/
/*	$NetBSD: auxreg.h,v 1.7 1997/05/17 17:52:52 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)auxreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Sun-4c Auxiliary I/O register.  This register talks to the floppy
 * (if it exists) and the front-panel LED.
 */

#define	AUXIO4C_MB1	0xf0		/* must be set on write */
#define	AUXIO4C_FHD	0x20		/* floppy: high density (unreliable?)*/
#define	AUXIO4C_FDC	0x10		/* floppy: diskette was changed */
#define	AUXIO4C_FDS	0x08		/* floppy: drive select */
#define	AUXIO4C_FTC	0x04		/* floppy: drives Terminal Count pin */
#define	AUXIO4C_FEJ	0x02		/* floppy: eject disk */
#define	AUXIO4C_LED	0x01		/* front panel LED */

#define	AUXIO4M_MB1	0xc0		/* must be set on write? */
#define	AUXIO4M_FHD	0x20		/* floppy: high density (unreliable?)*/
#define	AUXIO4M_LTE	0x08		/* link-test enable */
#define	AUXIO4M_MMX	0x04		/* Monitor/Mouse MUX; what is it? */
#define	AUXIO4M_FTC	0x02		/* floppy: drives Terminal Count pin */
#define	AUXIO4M_LED	0x01		/* front panel LED */

/*
 * Tadpole Auxiliary I/O registers.  Those control various power management
 * features.
 */

#define	AUXIO_MODEM		0x01	/* enable modem power */
#define	AUXIO_ISDN		0x04	/* enable ISDN power */
#define	AUXIO_MODEM_RESET	0x08	/* reset modem line (active low) */
#define	AUXIO_TFT		0x80	/* S3000 XT tft power */

#define	AUXIO2_SERIAL		0x01	/* enable serial ports power */

/*
 * We use a fixed virtual address for the register because we use it for
 * timing short sections of code (via external hardware attached to the LED).
 */
#define	AUXIO4C_REG	((volatile u_char *)(AUXREG_VA + 3))
#define	AUXIO4M_REG	((volatile u_char *)(AUXREG_VA))

#define LED_ON		do {						\
	if (CPU_ISSUN4M) {						\
		auxio_regval |= AUXIO4M_LED;				\
		*AUXIO4M_REG = auxio_regval;				\
	} else {							\
		auxio_regval |= AUXIO4C_LED;				\
		*AUXIO4C_REG = auxio_regval;				\
	}								\
} while(0)

#define LED_OFF		do {						\
	if (CPU_ISSUN4M) {						\
		auxio_regval &= ~AUXIO4M_LED;				\
		*AUXIO4M_REG = auxio_regval;				\
	} else {							\
		auxio_regval &= ~AUXIO4C_LED;				\
		*AUXIO4C_REG = auxio_regval;				\
	}								\
} while(0)

#define LED_FLIP	do {						\
	if (CPU_ISSUN4M) {						\
		auxio_regval ^= AUXIO4M_LED;				\
		*AUXIO4M_REG = auxio_regval;				\
	} else {							\
		auxio_regval ^= AUXIO4C_LED;				\
		*AUXIO4C_REG = auxio_regval;				\
	}								\
} while(0)

#define FTC_FLIP	do {						\
	if (CPU_ISSUN4M) {						\
		auxio_regval |= AUXIO4M_FTC;				\
		*AUXIO4M_REG = auxio_regval;				\
		*AUXIO4M_REG = *AUXIO4M_REG | AUXIO4M_MB1 | AUXIO4M_FTC;\
	} else {							\
		auxio_regval |= AUXIO4C_FTC;				\
		*AUXIO4C_REG = auxio_regval;				\
		DELAY(10);						\
		auxio_regval &= ~AUXIO4C_FTC;				\
		*AUXIO4C_REG = auxio_regval;				\
	}								\
} while(0)

#define	AUXIO_BITS	(						\
	CPU_ISSUN4M							\
		? "\20\6FHD\4LTE\3MMX\2FTC\1LED"			\
		: "\20\6FHD\5FDC\4FDS\3FTC\2FEJ\1LED"			\
)

#ifndef _LOCORE
extern u_char auxio_regval;
unsigned int auxregbisc(int, int);
unsigned int sb_auxregbisc(int, int, int);
#endif

