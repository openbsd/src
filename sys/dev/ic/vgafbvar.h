/*	$OpenBSD: vgafbvar.h,v 1.1 1998/09/27 03:55:59 rahnds Exp $	*/
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

struct vgafb_config {
	/*
	 * Filled in by front-ends.
	 */
	bus_space_tag_t	vc_iot, vc_memt;
	bus_space_handle_t vc_ioh_b, vc_ioh_c, vc_ioh_d, vc_memh;

	/*
	 * Private to back-end.
	 */
	int		vc_ncol, vc_nrow; /* screen width & height */
	int		vc_ccol, vc_crow; /* current cursor position */

	char		vc_so;		/* in standout mode? */
	char		vc_at;		/* normal attributes */
	char		vc_so_at;	/* standout attributes */

	int	(*vc_ioctl) __P((void *, u_long,
		    caddr_t, int, struct proc *));
	int	(*vc_mmap) __P((void *, off_t, int));
	
};

int	vgafb_common_probe __P((bus_space_tag_t, bus_space_tag_t, u_int32_t, 
	    u_int32_t, size_t ));
void	vgafb_common_setup __P((bus_space_tag_t, bus_space_tag_t,
	    struct vgafb_config *, u_int32_t, u_int32_t, size_t));
void	vgafb_wscons_attach __P((struct device *, struct vgafb_config *, int));
void	vgafb_wscons_console __P((struct vgafb_config *));
int	vgafbioctl __P((void *, u_long, caddr_t, int, struct proc *));
int	vgafbmmap __P((void *, off_t, int));
