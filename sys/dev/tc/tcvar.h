/*	$OpenBSD: tcvar.h,v 1.2 1996/04/18 23:48:24 niklas Exp $	*/
/*	$NetBSD: tcvar.h,v 1.3 1996/02/27 01:37:33 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

#ifndef __DEV_TC_TCVAR_H__
#define __DEV_TC_TCVAR_H__

/*
 * TurboChannel autoconfiguration definitions.
 */

#include <dev/tc/tcreg.h>
#include <machine/tc_machdep.h>

/*
 * Interrupt levels.  XXX should be common, elsewhere.
 */
typedef enum {
	TC_IPL_NONE,			/* block only this interrupt */
	TC_IPL_BIO,			/* block disk interrupts */
	TC_IPL_NET,			/* block network interrupts */
	TC_IPL_TTY,			/* block terminal interrupts */
	TC_IPL_CLOCK,			/* block clock interrupts */
} tc_intrlevel_t;

/*
 * Arguments used to attach TurboChannel busses.
 */
struct tcbus_attach_args {
	char		*tba_busname;		/* XXX should be common */

	/* Bus information */
	u_int		tba_speed;		/* see TC_SPEED_* below */
	u_int		tba_nslots;
	struct tc_slotdesc *tba_slots;
	u_int		tba_nbuiltins;
	const struct tc_builtin *tba_builtins;
	

	/* TC bus resource management; XXX will move elsewhere eventually. */
	void	(*tba_intr_establish) __P((struct device *, void *,
		    tc_intrlevel_t, int (*)(void *), void *));
	void	(*tba_intr_disestablish) __P((struct device *, void *));
};

/*
 * Arguments used to attach TurboChannel devices.
 */
struct tc_attach_args {
	char		ta_modname[TC_ROM_LLEN+1];
	u_int		ta_slot;
	tc_offset_t	ta_offset;
	tc_addr_t	ta_addr;
	void		*ta_cookie;
	u_int		ta_busspeed;		/* see TC_SPEED_* below */
};

/*
 * Description of TurboChannel slots, provided by machine-dependent
 * code to the TurboChannel bus driver.
 */
struct tc_slotdesc {
	tc_addr_t	tcs_addr;
	void		*tcs_cookie;
	int		tcs_used;
};

/*
 * Description of built-in TurboChannel devices, provided by
 * machine-dependent code to the TurboChannel bus driver.
 */
struct tc_builtin {
	char		*tcb_modname;
	u_int		tcb_slot;
	tc_offset_t	tcb_offset;
	void		*tcb_cookie;
};

/*
 * Interrupt establishment functions.
 */
void	tc_intr_establish __P((struct device *, void *, tc_intrlevel_t,
	    int (*)(void *), void *));
void	tc_intr_disestablish __P((struct device *, void *));

/*
 * Easy to remember names for TurboChannel device locators.
 */
#define	tccf_slot	cf_loc[0]		/* slot */
#define	tccf_offset	cf_loc[1]		/* offset */

#define	TCCF_SLOT_UNKNOWN	-1
#define	TCCF_OFFSET_UNKNOWN	-1

/*
 * Miscellaneous definitions.
 */
#define	TC_SPEED_12_5_MHZ	0		/* 12.5MHz TC bus */
#define	TC_SPEED_25_MHZ		1		/* 25MHz TC bus */

/*
 * The TurboChannel bus cfdriver, so that subdevices can more
 * easily tell what bus they're on.
 */
extern struct cfdriver tccd;

#endif /* __DEV_TC_TCVAR_H__ */
