/*	$OpenBSD: cyreg.h,v 1.1 1996/06/20 11:39:11 deraadt Exp $	*/
/*	$FreeBSD: cyreg.h,v 1.1 1995/07/05 12:15:51 bde Exp $	*/

/*-
 * Copyright (c) 1995 Bruce Evans.
 * All rights reserved.
 *
 * Modified by Timo Rossi, 1996
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Definitions for Cyclades Cyclom-Y serial boards.
 */

#define	CY8_SVCACKR		0x100
#define	CY8_SVCACKT		0x200
#define	CY8_SVCACKM		0x300

/* twice this in PCI mode (shifed BUSTYPE bits left) */
#define	CY_CD1400_MEMSPACING	0x400

/* adjustment value for accessing the last 4 cd1400s on Cyclom-32 */
#define CY32_ADDR_FIX           0xe00

#define	CY16_RESET		0x1400
#define	CY_CLEAR_INTR		0x1800	/* intr ack address */

#define	CY_MAX_CD1400s		8	/* for Cyclom-32 */

/* I/O location for enabling interrupts on PCI Cyclom cards */
#define CY_PCI_INTENA           0x68

#define	CY_CLOCK		25000000	/* baud rate clock */

/*
 * bustype is actually the shift count for the offset
 * ISA card addresses are multiplied by 2 (shifted 1 bit)
 * and PCI addresses multiplied by 4 (shifted 2 bits)
 */
#define CY_BUSTYPE_ISA 0
#define CY_BUSTYPE_PCI 1

