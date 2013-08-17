/*	$OpenBSD: vgafbvar.h,v 1.21 2013/08/17 09:15:47 mpi Exp $	*/
/*	$NetBSD: vgavar.h,v 1.2 1996/11/23 06:06:43 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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

struct vga_config {
	/*
	 * Filled in by front-ends.
	 */
	bus_space_tag_t		vc_memt;
	bus_space_handle_t	vc_memh;

	/* Colormap */
	u_char vc_cmap_red[256];
	u_char vc_cmap_green[256];
	u_char vc_cmap_blue[256];

	struct rasops_info	ri;

	bus_addr_t	membase;
	bus_size_t	memsize;

	bus_addr_t	mmiobase;
	bus_size_t	mmiosize;

	int vc_backlight_on;
	u_int vc_mode;
};

int	vgafb_cnattach(bus_space_tag_t, bus_space_tag_t, int, int);
int	vgafb_is_console(int);

void	vgafb_wsdisplay_attach(struct device *, struct vga_config *, int);
