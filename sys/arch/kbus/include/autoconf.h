/*	$OpenBSD: autoconf.h,v 1.1.1.1 1997/10/14 07:25:31 gingold Exp $	*/
/*	$NetBSD: autoconf.h,v 1.12 1996/11/20 18:57:05 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Autoconfiguration information.
 */

/* These are the "bus" types: */
#define	BUS_KBUS	0	/* "kbus" */
#define	BUS_VME32	1	/* "vme32" */
#define	BUS_VME24	2	/* "vme24" */
#define BUS_VME16	3	/* "vme16" */

/*
 * This is the "args" parameter to the bus match/attach functions.
 */
struct confargs {
	int ca_bustype;		/* BUS_OBIO, ... */
	int ca_paddr;		/* physical address */
	int ca_intpri;		/* interrupt priority level */
	int ca_intvec;		/* interrupt vector index */
};

/* Defined in configure.c  */
void configure __P((void));
/* Defined in swapgeneric.c  */
void setconf __P((void));

struct device;
int bus_scan __P((struct device *, void *, void *));
int bus_print __P((void *, const char *));
int bus_peek __P((int, int, int));
char *bus_mapin __P ((int bustype, u_long phys, int size));

/* Bus-error tolerant access to mapped address. */
int     peek_byte __P((caddr_t));
int     peek_word __P((caddr_t));
