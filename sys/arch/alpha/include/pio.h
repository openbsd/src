/*	$NetBSD: pio.h,v 1.1 1995/06/28 01:16:33 cgd Exp $	*/

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

/* Prototypes for ISA-ish I/O space access functions. */

/*
 * XXX
 * XXX THIS WILL LIKELY HAVE TO BE COMPLETELY CHANGED.
 * XXX e.g. to take softc for bus.
 * XXX
 */

struct	isa_pio_fcns {
	/* input functions */
	u_int8_t	(*isa_inb) __P((int port));
	void		(*isa_insb) __P((int port, void *addr, int cnt));
	u_int16_t	(*isa_inw) __P((int port));
	void		(*isa_insw) __P((int port, void *addr, int cnt));
	u_int32_t	(*isa_inl) __P((int port));
	void		(*isa_insl) __P((int port, void *addr, int cnt));

	/* output functions */
	void		(*isa_outb) __P((int port, u_int8_t datum));
	void		(*isa_outsb) __P((int port, void *addr, int cnt));
	void		(*isa_outw) __P((int port, u_int16_t datum));
	void		(*isa_outsw) __P((int port, void *addr, int cnt));
	void		(*isa_outl) __P((int port, u_int32_t datum));
	void		(*isa_outsl) __P((int port, void *addr, int cnt));
};

/*
 * Global which tells which set of functions are correct
 * for this machine.
 */
struct isa_pio_fcns *isa_pio_fcns;

/*
 * Individual chipsets' versions.
 */
extern struct isa_pio_fcns apecs_pio_fcns;
extern struct isa_pio_fcns jensen_pio_fcns;


/*
 * macros to use input functions
 */
#define	inb(p)		(*isa_pio_fcns->isa_inb)(p)
#define	insb(p, a, c)	(*isa_pio_fcns->isa_insb)(p, a, c)
#define	inw(p)		(*isa_pio_fcns->isa_inw)(p)
#define	insw(p, a, c)	(*isa_pio_fcns->isa_insw)(p, a, c)
#define	inl(p)		(*isa_pio_fcns->isa_inl)(p)
#define	insl(p, a, c)	(*isa_pio_fcns->isa_insl)(p, a, c)

/*
 * macros to use output functions
 */
#define	outb(p, d)	(*isa_pio_fcns->isa_outb)(p, d)
#define	outsb(p, a, c)	(*isa_pio_fcns->isa_outsb)(p, a, c)
#define	outw(p, d)	(*isa_pio_fcns->isa_outw)(p, d)
#define	outsw(p, a, c)	(*isa_pio_fcns->isa_outsw)(p, a, c)
#define	outl(p, d)	(*isa_pio_fcns->isa_outl)(p, d)
#define	outsl(p, a, c)	(*isa_pio_fcns->isa_outsl)(p, a, c)
