/*	$NetBSD: autoconf.h,v 1.9 1995/01/11 20:38:33 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
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
 */

/*
 * Autoconfiguration information.
 */

/* These are the "bus" types: */
#define	BUS_OBMEM	0	/* "obmem" */
#define	BUS_OBIO	1	/* "obio"  */
#define	BUS_VME16	2	/* "vmes"  */
#define	BUS_VME32	3	/* "vmel"  */
/* These are pseudo buses: */
#define	BUS_OBCTL	4

/*
 * This is the "args" parameter to the bus match/attach functions.
 */
struct confargs {
	int ca_bustype;		/* BUS_OBIO, ... */
	int ca_paddr;		/* physical address */
	int ca_intpri;		/* interrupt priority level */
	int ca_intvec;		/* interrupt vector index */
};

int always_match __P((struct device *, void *, void *));
void bus_scan __P((struct device *, void *, int));
int  bus_print __P((void *, char *));
int  bus_peek __P((int, int, int));
char * bus_mapin __P((int, int, int));
