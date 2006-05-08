/*	$OpenBSD: dartvar.h,v 1.1.1.1 2006/05/08 18:44:10 miod Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
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

#define	NDARTPORTS	2	/* Number of ports */

struct dart_info {
	struct tty		*tty;
	u_char			dart_swflags;
};

/* saved registers */
struct dart_sv_reg {
	u_int8_t	sv_mr1[NDARTPORTS];
	u_int8_t	sv_mr2[NDARTPORTS];
	u_int8_t	sv_csr[NDARTPORTS];
	u_int8_t	sv_cr[NDARTPORTS];
	u_int8_t	sv_opr;
	u_int8_t	sv_imr;
};

struct dartsoftc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct intrhand		sc_ih;

	int			sc_console;

	struct dart_sv_reg	*sc_sv_reg;
	struct dart_sv_reg	sc_sv_reg_storage;
	struct dart_info	sc_dart[NDARTPORTS];
};

void	dart_common_attach(struct dartsoftc *);
int	dartintr(void *);

#define	DART_SIZE	0x40
