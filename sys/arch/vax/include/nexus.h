/*	$OpenBSD: nexus.h,v 1.16 2011/07/06 18:32:59 miod Exp $	*/
/*	$NetBSD: nexus.h,v 1.17 2000/06/04 17:58:19 ragge Exp $	*/

/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	@(#)nexus.h	7.3 (Berkeley) 5/9/91
 */

#ifndef _MACHINE_NEXUS_H_
#define _MACHINE_NEXUS_H_

#include <machine/bus.h>

struct	mainbus_attach_args {
	int	maa_bustype;
};

/*
 * Values for bus (or pseudo-bus) types
 */
#define	VAX_VSBUS	7	/* Virtual vaxstation bus */
#define	VAX_IBUS	8	/* Internal Microvax bus */
#define	VAX_VXTBUS	10	/* Pseudo VXT2000 bus */
#define	VAX_MBUS	11	/* M-bus (KA60) */

#define	VAX_LEDS	0x42	/* pseudo value to attach led0 */

#ifdef _KERNEL

struct bp_conf {
	char *type;
	int bp_addr;
};

#endif

/* Memory recover defines */
#define	MCHK_PANIC	-1
#define	MCHK_RECOVERED	0

#endif /* _MACHINE_NEXUS_H_ */
