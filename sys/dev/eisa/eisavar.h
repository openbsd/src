/*	$OpenBSD: eisavar.h,v 1.3 1996/04/18 23:47:13 niklas Exp $	*/
/*	$NetBSD: eisavar.h,v 1.4 1996/03/08 20:25:22 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *      for the NetBSD Project.
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

#ifndef __DEV_EISA_EISAVAR_H__
#define	__DEV_EISA_EISAVAR_H__

/*
 * Definitions for EISA autoconfiguration.
 *
 * This file describes types, constants, and functions which are used
 * for EISA autoconfiguration.
 */

#include <machine/bus.h>
#include <dev/eisa/eisareg.h>		/* For ID register & string info. */


typedef int	eisa_slot_t;		/* really only needs to be 4 bits */


/*
 * EISA bus attach arguments.
 */
struct eisabus_attach_args {
	char		*eba_busname;		/* XXX should be common */
	bus_chipset_tag_t eba_bc;		/* XXX should be common */
};


/*
 * EISA device attach arguments.
 */
struct eisa_attach_args {
	bus_chipset_tag_t ea_bc;

	eisa_slot_t	ea_slot;
	u_int8_t	ea_vid[EISA_NVIDREGS];
	u_int8_t	ea_pid[EISA_NPIDREGS];
	char		ea_idstring[EISA_IDSTRINGLEN];
};


/*
 * Easy to remember names for EISA device locators.
 */

#define	eisacf_slot		cf_loc[0]		/* slot */


/*
 * EISA device locator values that mean "unknown" or "unspecified."
 * Note that not all are supplied by 'config' and should be filled
 * in by the device if appropriate.
 */

#define	EISA_UNKNOWN_SLOT	((eisa_slot_t)-1)

/*
 * The EISA bus cfdriver, so that subdevices can more easily tell
 * what bus they're on.
 */

extern struct cfdriver eisacd;

/*
 * XXX interrupt attachment, etc., is done by using the ISA interfaces.
 * XXX THIS SHOULD CHANGE.
 */

#include <dev/isa/isavar.h>

#define	eisa_intr_establish	isa_intr_establish		/* XXX */
#define	eisa_intr_disestablish	isa_intr_disestablish		/* XXX */

#endif /* !__DEV_EISA_EISAVAR_H__ */
