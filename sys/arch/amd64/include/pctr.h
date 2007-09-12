/*	$OpenBSD: pctr.h,v 1.1 2007/09/12 18:18:27 deraadt Exp $	*/

/*
 * Copyright (c) 2007 Mike Belopuhov
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
 * Pentium performance counter driver for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project by leaving this copyright notice intact.
 */

#ifndef _AMD64_PCTR_H_
#define _AMD64_PCTR_H_

#include <sys/ioccom.h>

#define PCTR_NUM	4
#define PCTR_AMD_NUM	PCTR_NUM
#define PCTR_INTEL_NUM	2		/* Intel supports only 2 counters */

struct pctrst {
	u_int64_t pctr_hwc[PCTR_NUM];	/* Values of the hardware counters */
	u_int64_t pctr_tsc;		/* Free-running 64-bit cycle counter */
	u_int64_t pctr_idl;		/* Iterations of the idle loop */
	u_int32_t pctr_fn[PCTR_NUM];	/* Current settings of counters */
};

/* Bit values in fn fields and PIOCS ioctl's */
#define PCTR_U		0x010000	/* Monitor user-level events */
#define PCTR_K		0x020000	/* Monitor kernel-level events */
#define PCTR_E		0x040000	/* Edge detect */
#define PCTR_EN		0x400000	/* Enable counters (counter 0 only) */
#define PCTR_I		0x800000	/* Invert counter mask */

/* Unit Mask bits */
#define PCTR_UM_M	0x10		/* Modified cache lines */
#define PCTR_UM_O	0x08		/* Owned cache lines */
#define PCTR_UM_E	0x04		/* Exclusive cache lines */
#define PCTR_UM_S	0x02		/* Shared cache lines */
#define PCTR_UM_I	0x01		/* Invalid cache lines */
#define PCTR_UM_MESI	(PCTR_UM_O|PCTR_UM_E|PCTR_UM_S|PCTR_UM_I)
#define PCTR_UM_MOESI	(PCTR_UM_M|PCTR_UM_O|PCTR_UM_E|PCTR_UM_S|PCTR_UM_I)

/* ioctl to set which counter a device tracks. */
#define PCIOCRD	_IOR('c', 1,  struct pctrst)	/* Read counter value */
#define PCIOCS0	_IOW('c', 8,  unsigned int)	/* Set counter 0 function */
#define PCIOCS1 _IOW('c', 9,  unsigned int)	/* Set counter 1 function */
#define PCIOCS2 _IOW('c', 10, unsigned int)	/* Set counter 2 function */
#define PCIOCS3 _IOW('c', 11, unsigned int)	/* Set counter 3 function */

#define _PATH_PCTR	"/dev/pctr"

#ifdef _KERNEL

void	pctrattach(int);
int	pctropen(dev_t, int, int, struct proc *);
int	pctrclose(dev_t, int, int, struct proc *);
int	pctrioctl(dev_t, u_int64_t, caddr_t, int, struct proc *);
int	pctrsel(int fflag, u_int32_t, u_int32_t);

#endif /* _KERNEL */
#endif /* ! _AMD64_PCTR_H_ */
