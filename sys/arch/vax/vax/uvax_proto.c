/*	$NetBSD: uvax_proto.c,v 1.3 1996/10/13 03:36:06 christos Exp $	*/
/*-
 * Copyright (c) 1982, 1988, 1990, 1993
 * 	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

/*
 * MicroVAX and VAXstation and their different models have many 
 * similarities and also many very specific implementations/solutions.
 * Thus this is a trial to have generic and prototypic routines
 * which can be used instead of specific routines whenever possible.
 *
 * usually there are groups of machines using the same CPU chips, eg.
 *	MicroVAX II
 *	MicroVAX 2000
 *	VAXstation 2000
 * have the same CPU and thus can share the CPU dependent code.
 *
 * On the other hand the above machines are quite differnet wrt. the
 * way the board-specific details (system-bus, NVRAM layout, etc.)
 * and thus can't share this code. 
 *
 * It's also possible to find groups of machines which have enough
 * similarities wrt. to board-specific implementations, to share some
 * code between them. Eg.
 *	VAXstation 2000
 *	VAXstation 3100 (which models ???)
 *	VAXstation 4000 (which models ???)
 * use the same (nonexistent "virtual") system-bus and thus can share
 * some pieces of code...
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>

#include <machine/mtpr.h>

static int
uVAX_clkread(base)
	time_t base;
{
	register struct uVAX_clock *claddr = uVAX_clkptr;
	struct chiptime c;
	int timeout = 1<<15, rv;

	claddr->csr1 = uVAX_CLKSET;
	while ((claddr->csr0 & uVAX_CLKUIP) != 0)
		if (--timeout == 0) {
			printf ("TOY clock timed out");
			return CLKREAD_BAD;
		}

	c.sec = claddr->sec;
	c.min = claddr->min;
	c.hour = claddr->hr;
	c.day = claddr->day;
	c.mon = claddr->mon;
	c.year = claddr->yr;

	/* If the clock is valid, use it. */
	if ((claddr->csr3 & uVAX_CLKVRT) != 0 &&
	    (claddr->csr1 & uVAX_CLKENABLE) == uVAX_CLKENABLE) {
		/* simple sanity checks */
		time.tv_sec = chiptotime(&c);
		if (c.mon < 1 || c.mon > 12 ||
		    c.day < 1 || c.day > 31) {
			printf("WARNING: preposterous clock chip time");
			rv = CLKREAD_WARN;
		} else
			rv = CLKREAD_OK;

		claddr->csr0 = uVAX_CLKRATE;
		claddr->csr1 = uVAX_CLKENABLE;
		return rv;
	}

	printf("WARNING: TOY clock invalid");
	return CLKREAD_BAD;
}

/* Set the time of day clock, called via. stime system call.. */
static void
uVAX_clkwrite()
{
	register struct uVAX_clock *claddr = uVAX_clkptr;
	struct chiptime c;
	int timeout = 1<<15;
	int s;

	timetochip(&c);

	s = splhigh();

	claddr->csr1 = uVAX_CLKSET;
	while ((claddr->csr0 & uVAX_CLKUIP) != 0)
		if (--timeout == 0) {
			printf("Trouble saving date, TOY clock timed out\n");
			break;
		}
 
	claddr->sec = c.sec;
	claddr->min = c.min;
	claddr->hr  = c.hour;
	claddr->day = c.day;
	claddr->mon = c.mon;
	claddr->yr  = c.year;

	claddr->csr0 = uVAX_CLKRATE;
	claddr->csr1 = uVAX_CLKENABLE;

	splx(s);
}
