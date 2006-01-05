/*	$OpenBSD: amdpmreg.h,v 1.4 2006/01/05 08:54:27 grange Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Enami Tsugutomo.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#define	AMDPM_CONFREG	0x40

/* 0x40: General Configuration 1 Register */
#define	AMDPM_RNGEN	0x00000080	/* random number generator enable */
#define	AMDPM_STOPTMR	0x00000040	/* stop free-running timer */

/* 0x41: General Configuration 2 Register */
#define	AMDPM_PMIOEN	0x00008000	/* system management IO space enable */
#define	AMDPM_TMRRST	0x00004000	/* reset free-running timer */
#define	AMDPM_TMR32	0x00000800	/* extended (32 bit) timer enable */

/* 0x42: SCI Interrupt Configuration Register */
/* 0x43: Previous Power State Register */

#define	AMDPM_PMPTR	0x58		/* PMxx System Management IO space
					   Pointer */
#define	AMDPM_PMBASE(x)	((x) & 0xff00)	/* PMxx base address */
#define	AMDPM_PMSIZE	256		/* PMxx space size */

/* Registers in PMxx space */
#define	AMDPM_TMR	0x08		/* 24/32 bit timer register */

#define	AMDPM_RNGDATA	0xf0		/* 32 bit random data register */
#define	AMDPM_RNGSTAT	0xf4		/* RNG status register */
#define	AMDPM_RNGDONE	0x00000001	/* Random number generation complete */

#define AMDPM_SMBSTAT	0xe0		/* SMBus status */
#define AMDPM_SMBSTAT_ABRT	(1 << 0)	/* transfer abort */
#define AMDPM_SMBSTAT_COL	(1 << 1)	/* collision */
#define AMDPM_SMBSTAT_PRERR	(1 << 2)	/* protocol error */
#define AMDPM_SMBSTAT_HBSY	(1 << 3)	/* host controller busy */
#define AMDPM_SMBSTAT_CYC	(1 << 4)	/* cycle complete */
#define AMDPM_SMBSTAT_TO	(1 << 5)	/* timeout */
#define AMDPM_SMBSTAT_SNP	(1 << 8)	/* snoop address match */
#define AMDPM_SMBSTAT_SLV	(1 << 9)	/* slave address match */
#define AMDPM_SMBSTAT_SMBA	(1 << 10)	/* SMBALERT# asserted */
#define AMDPM_SMBSTAT_BSY	(1 << 11)	/* bus busy */
#define AMDPM_SMBSTAT_BITS	"\020\001ABRT\002COL\003PRERR\004HBSY\005CYC\006TO\011SNP\012SLV\013SMBA\014BSY"
#define AMDPM_SMBCTL	0xe2		/* SMBus control */
#define AMDPM_SMBCTL_CMD_QUICK	0		/* QUICK command */
#define AMDPM_SMBCTL_CMD_BYTE	1		/* BYTE command */
#define AMDPM_SMBCTL_CMD_BDATA	2		/* BYTE DATA command */
#define AMDPM_SMBCTL_CMD_WDATA	3		/* WORD DATA command */
#define AMDPM_SMBCTL_CMD_PCALL	4		/* PROCESS CALL command */
#define AMDPM_SMBCTL_CMD_BLOCK	5		/* BLOCK command */
#define AMDPM_SMBCTL_START	(1 << 3)	/* start transfer */
#define AMDPM_SMBCTL_CYCEN	(1 << 4)	/* intr on cycle complete */
#define AMDPM_SMBCTL_ABORT	(1 << 5)	/* abort transfer */
#define AMDPM_SMBCTL_SNPEN	(1 << 8)	/* intr on snoop addr match */
#define AMDPM_SMBCTL_SLVEN	(1 << 9)	/* intr on slave addr match */
#define AMDPM_SMBCTL_SMBAEN	(1 << 10)	/* intr on SMBALERT# */
#define AMDPM_SMBADDR	0xe4		/* SMBus address */
#define AMDPM_SMBADDR_READ	(1 << 0)	/* read direction */
#define AMDPM_SMBADDR_ADDR(x)	(((x) & 0x7f) << 1) /* 7-bit address */
#define AMDPM_SMBDATA	0xe6		/* SMBus data */
#define AMDPM_SMBCMD	0xe8		/* SMBus command */
