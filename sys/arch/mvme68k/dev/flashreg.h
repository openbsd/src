/*	$OpenBSD: flashreg.h,v 1.6 2003/06/02 05:09:14 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define FLCMD_RESET		0xff
#define FLCMD_READII		0x90
#define FLCMD_READSTAT		0x70
#define FLCMD_CLEARSTAT		0x50
#define FLCMD_ESETUP		0x20
#define FLCMD_ECONFIRM		0xd0
#define FLCMD_ESUSPEND		0xb0
#define FLCMD_ERESUME		0xd0
#define FLCMD_WSETUP		0x40
#define FLCMD_AWSETUP		0x10

#define FLSR_WSMS		0x80	/* write state machine status */
#define FLSR_ESS		0x40	/* erase suspend status */
#define FLSR_ES			0x20	/* erase status */
#define FLSR_BWS		0x10	/* byte write status */
#define FLSR_VPPS		0x08	/* Vpp status */

/* manufacturers */
#define FLMANU_INTEL		0x89

/* intel parts */
#define FLII_INTEL_28F020	0xbd
#define FLII_INTEL_28F008SA	0xa1
#define FLII_INTEL_28F008SA_L	0xa2
#define FLII_INTEL_28F016SA	0xa0

struct flashii {
	char	*name;
	u_char	ii;
	int	size;
	int	zonesize;
};
