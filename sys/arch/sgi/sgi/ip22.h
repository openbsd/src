/*	$OpenBSD: ip22.h,v 1.10 2015/09/05 21:14:07 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * IP20/IP22/IP24 definitions
 */

/* IP22/IP24 system types */

#define	IP22_INDY	0	/* IP24 Indy */
#define	IP22_CHALLS	1	/* IP24 Challenge S */
#define	IP22_INDIGO2	2	/* IP22 Indigo 2, Challenge M */

/* Interrupt handling priority */

#ifdef CPU_R8000
#define	INTPRI_BUSERR_TCC	(INTPRI_CLOCK + 1)
#define	INTPRI_BUSERR		(INTPRI_BUSERR_TCC + 1)
#else
#define	INTPRI_BUSERR		(INTPRI_CLOCK + 1)
#endif
#define	INTPRI_L1		(INTPRI_BUSERR + 1)
#define	INTPRI_L0		(INTPRI_L1 + 1)

extern int hpc_old;	/* nonzero if at least one HPC 1.x device found */
extern int bios_year;
extern int ip22_ecc;	/* nonzero if runinng with an ECC memory system */

void	ip22_ConfigCache(struct cpu_info *);
extern void (*ip22_extsync)(struct cpu_info *, paddr_t, size_t, int);
