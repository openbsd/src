/*	$NetBSD: autoconf.h,v 1.8 1997/05/25 06:10:47 jonathan Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Machine-dependent structures of autoconfiguration
 */

#include <machine/tc_machdep.h>

struct confargs;


/* Handle device interrupt for  given unit of a driver */

typedef void* intr_arg_t;		/* pointer to some softc */
typedef int (*intr_handler_t) __P((intr_arg_t));
/*
 * XXX Establish interrupt on an arbitrary decstation/decsystem bus.
 */
extern void
generic_intr_establish __P(( void * parent, void * cookie,
			 int level,
			 intr_handler_t handler, intr_arg_t arg));


#define KN02_ASIC_NAME "KN02    "	/* ROM name in 3max system slot */
	
#define	INTR_ESTABLISH(parent, cookie, level, handler, val)			\
    generic_intr_establish((parent), (cookie), (level), (handler), (val))

#define	BUS_INTR_ESTABLISH(ca,  handler, val)			\
    generic_intr_establish( ((struct device*)(val))->dv_parent, \
    			   (void*)(ca)->ca_slotpri, 0, (handler), (val))


struct confargs {
	char	*ca_name;		/* Device name. */
	int	ca_slot;		/* Device slot (table entry). */
	int	ca_offset;		/* Offset into slot. */
	tc_addr_t ca_addr;		/* Device address. */
	int	ca_slotpri;		/* Device interrupt "priority" */
};

extern caddr_t baseboard_cvtaddr __P((struct confargs *)); /*XXX*/

#ifndef pmax
void	set_clockintr __P((void (*)(struct clockframe *)));
#endif
void	set_iointr __P((void (*)(void *, int)));
int	badaddr			__P((void *, u_int));
void	configure __P((void));
void	makebootdev __P((char *cp));
