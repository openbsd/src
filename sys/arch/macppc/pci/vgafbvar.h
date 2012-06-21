/*	$OpenBSD: vgafbvar.h,v 1.14 2012/06/21 10:08:16 mpi Exp $	*/
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
	bus_space_handle_t vc_memh, vc_mmioh;
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

	int	(*vc_ioctl)(void *, u_long,
		    caddr_t, int, struct proc *);
	paddr_t	(*vc_mmap)(void *, off_t, int);

	struct rasops_info    dc_rinfo;       /* raster display data*/

	bus_addr_t	membase;
	bus_size_t	memsize;

	bus_addr_t	mmiobase;
	bus_size_t	mmiosize;

	int vc_backlight_on;
	int nscreens;
	u_int vc_mode;
};

void	vgafb_init(bus_space_tag_t, bus_space_tag_t,
	    struct vgafb_config *, u_int32_t, size_t, u_int32_t, size_t);
void	vgafb_wscons_attach(struct device *, struct vgafb_config *, int);
void	vgafb_wscons_console(struct vgafb_config *);
int	vgafb_cnattach(bus_space_tag_t, bus_space_tag_t, int, int);
void	vgafb_wsdisplay_attach(struct device *parent,
	    struct vgafb_config *vc, int console);
int	vgafbioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	vgafbmmap(void *, off_t, int);
int	vgafb_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	vgafb_mmap(void *, off_t, int);
int	vgafb_alloc_screen(void *v, const struct wsscreen_descr *type,
	    void **cookiep, int *curxp, int *curyp, long *attrp);
void	vgafb_free_screen(void *v, void *cookie);
int	vgafb_show_screen(void *v, void *cookie, int waitok,
	    void (*cb)(void *, int, int), void *cbarg);
void	vgafb_burn(void *v, u_int on, u_int flags);
