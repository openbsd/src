/*	$OpenBSD: tgavar.h,v 1.4 1996/07/29 23:01:07 niklas Exp $	*/
/*	$NetBSD: tgavar.h,v 1.5 1996/04/12 06:09:21 cgd Exp $	*/

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

#include <machine/tgareg.h>
#include <dev/rcons/raster.h>
#include <alpha/wscons/wsconsvar.h>
#include <alpha/wscons/wscons_raster.h>

struct tga_devconfig;
struct fbcmap;
struct fbcursor;
struct fbcurpos;

struct tga_ramdac_conf {
	char	*tgar_name;
	void	(*tgar_init) __P((struct tga_devconfig *, int));
	int	(*tgar_intr) __P((void *));
	int	(*tgar_set_cmap) __P((struct tga_devconfig *,
		    struct fbcmap *));
	int	(*tgar_get_cmap) __P((struct tga_devconfig *,
		    struct fbcmap *));
	int	(*tgar_set_cursor) __P((struct tga_devconfig *,
		    struct fbcursor *));
	int	(*tgar_get_cursor) __P((struct tga_devconfig *,
		    struct fbcursor *));
	int	(*tgar_set_curpos) __P((struct tga_devconfig *,
		    struct fbcurpos *));
	int	(*tgar_get_curpos) __P((struct tga_devconfig *,
		    struct fbcurpos *));
	int	(*tgar_get_curmax) __P((struct tga_devconfig *,
		    struct fbcurpos *));
};

struct tga_conf {
	char	    *tgac_name;		/* name for this board type */

	const struct tga_ramdac_conf
		    *tgac_ramdac;	/* the RAMDAC type; see above */
	int	    tgac_phys_depth;	/* physical frame buffer depth */
	vm_size_t   tgac_cspace_size;	/* core space size */
	vm_size_t   tgac_vvbr_units;	/* what '1' in the VVBR means */

	int	    tgac_ndbuf;		/* number of display buffers */
	vm_offset_t tgac_dbuf[2];	/* display buffer offsets/addresses */
	vm_size_t   tgac_dbufsz[2];	/* display buffer sizes */

	int	    tgac_nbbuf;		/* number of display buffers */
	vm_offset_t tgac_bbuf[2];	/* back buffer offsets/addresses */
	vm_size_t   tgac_bbufsz[2];	/* back buffer sizes */
};

struct tga_devconfig {
	bus_chipset_tag_t dc_bc;
	pci_chipset_tag_t dc_pc;

	pcitag_t   	 dc_pcitag;	/* PCI tag */
	bus_mem_addr_t	 dc_pcipaddr;	/* PCI phys addr. */

	tga_reg_t   *dc_regs;		/* registers; XXX: need aliases */

	int	    dc_tga_type;	/* the device type; see below */
	const struct tga_conf *dc_tgaconf; /* device buffer configuration */

	vm_offset_t dc_vaddr;		/* memory space virtual base address */
	vm_offset_t dc_paddr;		/* memory space physical base address */

	int	    dc_wid;		/* width of frame buffer */
	int	    dc_ht;		/* height of frame buffer */
	int	    dc_rowbytes;	/* bytes in a FB scan line */

	vm_offset_t dc_videobase;	/* base of flat frame buffer */

	struct raster	dc_raster;	/* raster description */
	struct rcons	dc_rcons;	/* raster blitter control info */

	int	    dc_blanked;		/* currently had video disabled */
	void	    *dc_ramdac_private; /* RAMDAC private storage */
};
	
struct tga_softc {
	struct	device sc_dev;

	struct	tga_devconfig *sc_dc;	/* device configuration */
	void	*sc_intr;		/* interrupt handler info */
	/* XXX should record intr fns/arg */
};

#define	TGA_TYPE_T8_01		0	/* 8bpp, 1MB */
#define	TGA_TYPE_T8_02		1	/* 8bpp, 2MB */
#define	TGA_TYPE_T8_22		2	/* 8bpp, 4MB */
#define	TGA_TYPE_T8_44		3	/* 8bpp, 8MB */
#define	TGA_TYPE_T32_04		4	/* 32bpp, 4MB */
#define	TGA_TYPE_T32_08		5	/* 32bpp, 8MB */
#define	TGA_TYPE_T32_88		6	/* 32bpp, 16MB */
#define	TGA_TYPE_UNKNOWN	7	/* unknown */

#define	DEVICE_IS_TGA(class, id)					\
	    ((PCI_VENDOR(id) == PCI_VENDOR_DEC &&			\
	     PCI_PRODUCT(id) == PCI_PRODUCT_DEC_21030) ? 10 : 0)

void    tga_console __P((bus_chipset_tag_t, pci_chipset_tag_t, int, int, int));

int	tga_identify __P((tga_reg_t *));
const struct tga_conf *tga_getconf __P((int));

extern const struct tga_ramdac_conf tga_ramdac_bt463;
extern const struct tga_ramdac_conf tga_ramdac_bt485;

int     tga_builtin_set_cursor __P((struct tga_devconfig *, struct fbcursor *));
int     tga_builtin_get_cursor __P((struct tga_devconfig *, struct fbcursor *));
int     tga_builtin_set_curpos __P((struct tga_devconfig *, struct fbcurpos *));
int     tga_builtin_get_curpos __P((struct tga_devconfig *, struct fbcurpos *));
int     tga_builtin_get_curmax __P((struct tga_devconfig *, struct fbcurpos *));
