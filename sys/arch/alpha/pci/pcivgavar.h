/*	$NetBSD: pcivgavar.h,v 1.2 1995/08/03 01:17:21 cgd Exp $	*/

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

struct pcivga_devconfig {
	pcitag_t	dc_pcitag;	/* PCI tag */

	u_int16_t	*dc_crtat;	/* VGA screen memory */
	int		dc_iobase;	/* VGA I/O address */

	int		dc_ncol, dc_nrow; /* screen width & height */
	int		dc_ccol, dc_crow; /* current cursor position */

	char		dc_so;		/* in standout mode? */
	char		dc_at;		/* normal attributes */
	char		dc_so_at;	/* standout attributes */

	struct ansicons	dc_ansicons;	/* ansi console emulator info XXX */
};
	
struct pcivga_softc {
	struct	device sc_dev;

	struct	pcivga_devconfig *sc_dc; /* device configuration */
	void	*sc_intr;		/* interrupt handler info */
};

#define	PCIVGA_CURSOR_OFF	-1	/* pass to pcivga_cpos to disable */

void	pcivga_cursor __P((void *, int, int));
void	pcivga_putstr __P((void *, int, int, char *, int));
void	pcivga_copycols __P((void *, int, int, int,int));
void	pcivga_erasecols __P((void *, int, int, int));
void	pcivga_copyrows __P((void *, int, int, int));
void	pcivga_eraserows __P((void *, int, int));
