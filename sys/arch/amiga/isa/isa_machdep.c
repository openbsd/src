/*	$OpenBSD: isa_machdep.c,v 1.4 1999/03/05 19:17:44 niklas Exp $	*/

/*
 * Copyright (c) 1999 Niklas Hallqvist
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
 *      This product includes software developed by Niklas Hallqvist.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

/*
 * Just check to see if an IRQ is available/can be shared.
 * 0 = interrupt not available
 * 1 = interrupt shareable
 * 2 = interrupt all to ourself
 */
int
__isa_intr_check(irq, type, intrtype)
	int irq;
	int type;
	int *intrtype;
{
	if (type == IST_NONE)
		return (0);

	switch (intrtype[irq]) {
	case IST_NONE:
		return (2);
		break;
	case IST_LEVEL:
		if (type != intrtype[irq])
			return (0);
		return (1);
		break;
	case IST_EDGE:
	case IST_PULSE:
		if (type != IST_NONE)
			return (0);
	}
	return (1);
}

