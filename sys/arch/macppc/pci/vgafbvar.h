/*	$OpenBSD: vgafbvar.h,v 1.2 2001/10/31 12:26:18 art Exp $	*/
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
	int vc_ofh; /* openfirmware handle */
	bus_space_tag_t	vc_iot, vc_memt;
	bus_space_handle_t vc_ioh_b, vc_ioh_c, vc_ioh_d, vc_memh, vc_mmioh;
	paddr_t		vc_paddr; /* physical address */
	/* Colormap */
	u_char vc_cmap_red[256];
	u_char vc_cmap_green[256];
	u_char vc_cmap_blue[256];

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
	paddr_t	(*vc_mmap) __P((void *, off_t, int));
	#if 0
	struct raster   dc_raster;      /* raster description */
	#endif
	struct rcons    dc_rcons;       /* raster blitter control info */

	
};

int	vgafb_common_probe __P((bus_space_tag_t, bus_space_tag_t,
	u_int32_t, size_t, u_int32_t, size_t, u_int32_t, size_t ));
void	vgafb_common_setup __P((bus_space_tag_t, bus_space_tag_t,
	    struct vgafb_config *, u_int32_t, size_t, u_int32_t, size_t,
	    u_int32_t, size_t));
void	vgafb_wscons_attach __P((struct device *, struct vgafb_config *, int));
void	vgafb_wscons_console __P((struct vgafb_config *));
void	vgafb_cnprobe __P((struct consdev *cp));
void	vgafb_cnattach __P((bus_space_tag_t iot, bus_space_tag_t memt,
	    void *pc, int bus, int device, int function));
void	vgafb_wsdisplay_attach __P((struct device *parent,
	    struct vgafb_config *vc, int console));
int	vgafbioctl __P((void *, u_long, caddr_t, int, struct proc *));
paddr_t	vgafbmmap __P((void *, off_t, int));
int	vgafb_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
paddr_t	vgafb_mmap __P((void *, off_t, int));
int	vgafb_alloc_screen __P((void *v, const struct wsscreen_descr *type,
	    void **cookiep, int *curxp, int *curyp, long *attrp));
void	vgafb_free_screen __P((void *v, void *cookie));
int	vgafb_show_screen __P((void *v, void *cookie, int waitok,
	    void (*cb) __P((void *, int, int)), void *cbarg));
