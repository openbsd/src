/*	$OpenBSD: vgafb.c,v 1.2 2001/09/13 13:38:45 drahn Exp $	*/
/*	$NetBSD: vga.c,v 1.3 1996/12/02 22:24:54 cgd Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <vm/vm.h>
#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <dev/cons.h>
#include <dev/ofw/openfirm.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rcons/raster.h>

#include <arch/macppc/pci/vgafbvar.h>

/* parameters set by OF to detect console */
extern int cons_displaytype;
extern bus_space_tag_t cons_membus;
extern bus_space_handle_t cons_display_mem_h;
extern bus_space_handle_t cons_display_ctl_h;
extern int cons_width;
extern int cons_linebytes;
extern int cons_height;
extern int cons_depth;
extern int cons_display_ofh;

struct cfdriver vgafb_cd = {
	NULL, "vgafb", DV_DULL,
};

void	vgafb_cursor __P((void *, int, int, int));
void	vgafb_putchar __P((void *, int, int, u_int, long));
void	vgafb_copycols __P((void *, int, int, int, int));
void	vgafb_erasecols __P((void *, int, int, int));
void	vgafb_copyrows __P((void *, int, int, int));
void	vgafb_eraserows __P((void *, int, int));
void	vgafb_alloc_attr __P((void *c, int fg, int bg, int flags, long *));

void vgafb_setcolor __P((unsigned int index, u_int8_t r, u_int8_t g, u_int8_t b));
extern const char fontdata_8x16[];

struct vgafb_devconfig {
	struct rcons dc_ri;
};
struct raster vgafb_raster;


struct vgafb_devconfig vgafb_console_dc;

struct wsscreen_descr vgafb_stdscreen = {
	"std",
	0, 0,   /* will be filled in -- XXX shouldn't, it's global */
	0,
	0, 0,
	WSSCREEN_REVERSE
};
const struct wsscreen_descr *vgafb_scrlist[] = {
	&vgafb_stdscreen,
	/* XXX other formats, graphics screen? */
};
   
struct wsscreen_list vgafb_screenlist = {
	sizeof(vgafb_scrlist) / sizeof(struct wsscreen_descr *), vgafb_scrlist
};


struct wsdisplay_emulops vgafb_emulops = {
	rcons_cursor,
	rcons_mapchar,
	rcons_putchar,
	rcons_copycols,
	rcons_erasecols,
	rcons_copyrows,
	rcons_eraserows,
	rcons_alloc_attr
};

struct wsdisplay_accessops vgafb_accessops = {
	vgafb_ioctl,
	vgafb_mmap,
	vgafb_alloc_screen,
	vgafb_free_screen,
	vgafb_show_screen,
	0 /* load_font */
};

int	vgafb_print __P((void *, const char *));
int	vgafb_getcmap __P((struct vgafb_config *vc, struct wsdisplay_cmap *cm));
int	vgafb_putcmap __P((struct vgafb_config *vc, struct wsdisplay_cmap *cm));

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

/*
 * The following functions implement back-end configuration grabbing
 * and attachment.
 */
int
vgafb_common_probe(iot, memt, iobase, iosize, membase, memsize, mmiobase, mmiosize)
	bus_space_tag_t iot, memt;
	u_int32_t iobase, membase, mmiobase;
	size_t iosize, memsize, mmiosize;
{
	bus_space_handle_t ioh_b, ioh_c, ioh_d, memh, mmioh;
	int gotio_b, gotio_c, gotio_d, gotmem, gotmmio, rv;

	gotio_b = gotio_c = gotio_d = gotmem = gotmmio = rv = 0;

	if (iosize != 0) {
		if (bus_space_map(iot, iobase+0x3b0, 0xc, 0, &ioh_b))
			goto bad;
		gotio_b = 1;
		if (bus_space_map(iot, iobase+0x3c0, 0x10, 0, &ioh_c))
			goto bad;
		gotio_c = 1;
		if (bus_space_map(iot, iobase+0x3d0, 0x10, 0, &ioh_d))
			goto bad;
		gotio_d = 1;
	}
	if (mmiosize != 0) {
		printf("vgafb_common_probe, mmio base %x size %x\n",
			mmiobase, mmiosize);
		if (bus_space_map(iot, mmiobase, mmiosize, 0, &mmioh))
			goto bad;
		printf("vgafb_common_probe, mmio done\n");
		gotmmio = 1;
	}
#if 0
	printf("vgafb_common_probe, mem base %x size %x memt %x\n",
		membase, memsize, memt);
#endif

#if 0
	if (bus_space_map(memt, membase, memsize, 0, &memh))
		goto bad;
	gotmem = 1;

	/* CR1 - Horiz. Display End */
	bus_space_write_1(iot, ioh_d, 4, 0x1);
	width = bus_space_read_1(iot, ioh_d, 5);
	/* this is not bit width yet */

	/* use CR17 - mode control for this?? */
	if ((width != 0xff) && (width < 600)) {
		/* not accessable or in graphics mode? */
		goto bad;
	}
#endif

#if 0
	vgadata = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, 0xa55a);
	rv = (bus_space_read_2(memt, memh, 0) == 0xa55a);
	bus_space_write_2(memt, memh, 0, vgadata);
#else
	rv = 1;
#endif


bad:
	if (gotio_b)
		bus_space_unmap(iot, ioh_b, 0xc);
	if (gotio_c)
		bus_space_unmap(iot, ioh_c, 0x10);
	if (gotio_d)
		bus_space_unmap(iot, ioh_d, 0x10);
	if (gotmmio)
		bus_space_unmap(memt, mmioh, mmiosize);
	if (gotmem)
		bus_space_unmap(memt, memh, memsize);

	return (rv);
}

void
vgafb_common_setup(iot, memt, vc, iobase, iosize, membase, memsize, mmiobase, mmiosize)
	bus_space_tag_t iot, memt;
	struct vgafb_config *vc;
	u_int32_t iobase, membase, mmiobase;
	size_t iosize, memsize, mmiosize;
{

        vc->vc_iot = iot;
        vc->vc_memt = memt;
	vc->vc_paddr = membase;

	if (iosize != 0) {
           if (bus_space_map(vc->vc_iot, iobase+0x3b0, 0xc, 0, &vc->vc_ioh_b))
		panic("vgafb_common_setup: couldn't map io b");
           if (bus_space_map(vc->vc_iot, iobase+0x3c0, 0x10, 0, &vc->vc_ioh_c))
		panic("vgafb_common_setup: couldn't map io c");
           if (bus_space_map(vc->vc_iot, iobase+0x3d0, 0x10, 0, &vc->vc_ioh_d))
		panic("vgafb_common_setup: couldn't map io d");
	}
	if (mmiosize != 0) {
           if (bus_space_map(vc->vc_memt, mmiobase, mmiosize, 0, &vc->vc_mmioh))
		panic("vgafb_common_setup: couldn't map mmio");
	}
#if 0
	printf("commons setup mapping mem base %x size %x\n", membase, memsize);
#endif
	/* memsize  should only be visable region for console */
	memsize = cons_height * cons_linebytes;
        if (bus_space_map(vc->vc_memt, membase, memsize, 0, &vc->vc_memh))
		panic("vgafb_common_setup: couldn't map memory"); 
	cons_display_mem_h = vc->vc_memh;
	vc->vc_ofh = cons_display_ofh;
#if 0
	printf("display_mem_h %x\n", cons_display_mem_h );
#endif

#if 0
	if (iosize != 0) {
		/* CR1 - Horiz. Display End */
		bus_space_write_1(iot, vc->vc_ioh_d, 4, 0x1);
		width = bus_space_read_1(iot, vc->vc_ioh_d, 5);
		/* (stored value + 1) * depth -> pixel width */
		width = ( width + 1 ) * 8;   

		/* CR1 - Horiz. Display End */
		bus_space_write_1(iot, vc->vc_ioh_d, 4, 0x12);
		{ 
			u_int8_t t1, t2, t3;
			bus_space_write_1(iot, vc->vc_ioh_d, 4, 0x12);
			t1 = bus_space_read_1(iot, vc->vc_ioh_d, 5);

			bus_space_write_1(iot, vc->vc_ioh_d, 4, 0x7);
			t2 = bus_space_read_1(iot, vc->vc_ioh_d, 5);
			height = t1 + ((t2&0x40) << 3) 
				    + ((t2&0x02) << 7) + 1; 
			bus_space_write_1(iot, vc->vc_ioh_d, 4, 0x17);
			t3 = bus_space_read_1(iot, vc->vc_ioh_d, 5);
			if (t3 & 0x04) {
				height *= 2;
			}
			if (t1 == 0xff && t2 == 0xff && t3 == 0xff) {
				/* iospace not working??? */
				/* hope, better guess than 2048x2048 */
				width = 640;
				height = 480;
			}
		}
		vc->vc_ncol = width / FONT_WIDTH;
		vc->vc_nrow = height / FONT_HEIGHT;
	} else {
		/* iosize == 0
		 * default to 640x480 and hope 
		 */
		vc->vc_ncol = 640 / FONT_WIDTH;
		vc->vc_nrow = 480 / FONT_HEIGHT;
	}
	vc->vc_ncol = cons_width / FONT_WIDTH;
	vc->vc_nrow = cons_height / FONT_HEIGHT;
	printf(", %dx%d", vc->vc_ncol, vc->vc_nrow);
#endif

	vc->vc_crow = vc->vc_ccol = 0; /* Has to be some onscreen value */
	vc->vc_so = 0;

	/* clear screen, frob cursor, etc.? */
	/*
	*/

#if defined(alpha)
	/*
	 * XXX DEC HAS SWITCHED THE CODES FOR BLUE AND RED!!!
	 * XXX Therefore, though the comments say "blue bg", the code uses
	 * XXX the value for a red background!
	 */
	vc->vc_at = 0x40 | 0x0f;		/* blue bg|white fg */
	vc->vc_so_at = 0x40 | 0x0f | 0x80;	/* blue bg|white fg|blink */
#else
	vc->vc_at = 0x00 | 0xf;			/* black bg|white fg */
	vc->vc_so_at = 0x00 | 0xf | 0x80;	/* black bg|white fg|blink */
#endif
}

void
vgafb_wsdisplay_attach(parent, vc, console)
	struct device *parent;
	struct vgafb_config *vc;
	int console;
{
	struct wsemuldisplaydev_attach_args aa;

        aa.console = console;
	aa.scrdata = &vgafb_screenlist;
	aa.accessops = &vgafb_accessops;
	aa.accesscookie = vc;
 
        config_found(parent, &aa, wsemuldisplaydevprint);
}


int
vgafb_print(aux, pnp)
	void *aux;
	const char *pnp;
{

	if (pnp)
		printf("wsdisplay at %s", pnp);
	return (UNCONF);
}

int
vgafb_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct vgafb_config *vc = v;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_PCIVGA;
		return 0;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = cons_height;
		wdf->width  = cons_width;
		wdf->depth  = cons_depth;
		wdf->cmsize = 256;
		return 0;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = cons_linebytes;
		return 0;

	case WSDISPLAYIO_GETCMAP:
		return vgafb_getcmap(vc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_PUTCMAP:
		return vgafb_putcmap(vc, (struct wsdisplay_cmap *)data);

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
		return -1; /* not supported yet */
	}
	
        /* XXX */
        return -1;
}

paddr_t
vgafb_mmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct vgafb_config *vc = v;
	bus_space_handle_t h;

	/* memsize... */
	if (offset >= 0x00000 && offset < 0x800000)	/* 8MB of mem??? */
		h = vc->vc_paddr + offset;
	/* XXX the following are probably wrong. we want physical addresses 
	   here, not virtual ones */
	else if (offset >= 0x10000000 && offset < 0x10040000 )
		/* 256KB of iohb */
		h = vc->vc_ioh_b;
	else if (offset >= 0x10040000 && offset < 0x10080000)
		/* 256KB of iohc */
		h = vc->vc_ioh_c;
	else if (offset >= 0x18880000 && offset < 0x100c0000)
		/* 256KB of iohd */
		h = vc->vc_ioh_d;
	else if (offset >= 0x20000000 && offset < 0x30000000)
		/* mmiosize... */
		h = vc->vc_mmioh + (offset - 0x20000000);
	else {
		/* XXX - allow mapping of the actual physical
		 * device address, if the address is read from
		 * pci bus config space
		 */

		/* NEEDS TO BE RESTRICTED to valid addresses for this device */
		 
		h = offset;
	}

#ifdef alpha
	port = (u_int32_t *)(h << 5);
	return alpha_btop(port);		/* XXX */
#elif defined(i386)
	port = (u_int32_t *)(h << 5);
	return i386_btop(port);
#elif defined(__powerpc__)
	{
	/* huh ??? */
	return h;
	/*
	return powerpc_btop(port);
	*/
	}
#endif
}


void
vgafb_cnprobe(cp)
	struct consdev *cp;
{
	if (cons_displaytype != 1) {
		cp->cn_pri = CN_DEAD;
		return;
	} 

	cp->cn_pri = CN_REMOTE;
	#if 0
	for (j = 0; j < 2; j++) {
		for (i = 0; i < cons_width * cons_height; i++) {
			bus_space_write_1(cons_membus,
				cons_display_mem_h, i, j);

		}
	}
	#endif

}

extern struct raster_font fontdata8x16;
void
vgafb_cnattach(iot, memt, pc, bus, device, function)
	void * pc;
	bus_space_tag_t iot, memt;
	int bus, device, function;
{
        long defattr;

	struct vgafb_devconfig *dc = &vgafb_console_dc;
        struct rcons *ri = &dc->dc_ri;
	ri->rc_sp = &vgafb_raster;

	ri->rc_sp->width = cons_width;
	ri->rc_sp->height = cons_height;
	ri->rc_sp->depth = cons_depth;
	ri->rc_sp->linelongs = cons_linebytes /4; /* XXX */
	ri->rc_sp->pixels = (void *)cons_display_mem_h;
	ri->rc_crow = ri->rc_ccol = -1;
	ri->rc_crowp = &ri->rc_crow;
	ri->rc_ccolp = &ri->rc_ccol;

	rcons_init(ri, 160, 160);

	vgafb_stdscreen.nrows = ri->rc_maxrow;
	vgafb_stdscreen.ncols = ri->rc_maxcol;
	vgafb_stdscreen.textops = &vgafb_emulops;
	rcons_alloc_attr(ri, 0, 0, 0, &defattr);

	#if 0
	{
		int i;
		for (i = 0; i < cons_width * cons_height; i++) {
			bus_space_write_1(cons_membus,
				cons_display_mem_h, i, 0x1);

		}
	}
	#endif
	{ 
	  int i;
	  for (i = 0; i < 256; i++) {
	     vgafb_setcolor(i, 255,255,255);
	  }
	}
	vgafb_setcolor(WSCOL_BLACK, 0, 0, 0);
	vgafb_setcolor(255, 255, 255, 255);
	vgafb_setcolor(WSCOL_RED, 255, 0, 0);
	vgafb_setcolor(WSCOL_GREEN, 0, 255, 0);
	vgafb_setcolor(WSCOL_BROWN, 154, 85, 46);
	vgafb_setcolor(WSCOL_BLUE, 0, 0, 255);
	vgafb_setcolor(WSCOL_MAGENTA, 255, 255, 0);
	vgafb_setcolor(WSCOL_CYAN, 0, 255, 255);
	vgafb_setcolor(WSCOL_WHITE, 255, 255, 255);
	wsdisplay_cnattach(&vgafb_stdscreen, ri, 0, 0, defattr);
}

void
vgafb_setcolor(index, r, g, b) 
	unsigned int index;
	u_int8_t r, g, b;
{
	OF_call_method_1("color!", cons_display_ofh, 4, r, g, b, index);
}

int
vgafb_getcmap(vc, cm)
	struct vgafb_config *vc;
	struct wsdisplay_cmap *cm;
{
	u_int index = cm->index;
	u_int count = cm->count;
	int error;

	if (index >= 256 || count > 256 || index + count > 256)
		return EINVAL;

	error = copyout(&vc->vc_cmap_red[index],   cm->red,   count);
	if (error)
		return error;
	error = copyout(&vc->vc_cmap_green[index], cm->green, count);
	if (error)
		return error;
	error = copyout(&vc->vc_cmap_blue[index],  cm->blue,  count);
	if (error)
		return error;

	return 0;
}

int
vgafb_putcmap(vc, cm)
	struct vgafb_config *vc;
	struct wsdisplay_cmap *cm;
{
	int index = cm->index;
	int count = cm->count;
	int i;
	u_char *r, *g, *b;

	if (cm->index >= 256 || cm->count > 256 ||
	    (cm->index + cm->count) > 256)
		return EINVAL;
	if (!uvm_useracc(cm->red, cm->count, B_READ) ||
	    !uvm_useracc(cm->green, cm->count, B_READ) ||
	    !uvm_useracc(cm->blue, cm->count, B_READ))
		return EFAULT;
	copyin(cm->red,   &(vc->vc_cmap_red[index]),   count);
	copyin(cm->green, &(vc->vc_cmap_green[index]), count);
	copyin(cm->blue,  &(vc->vc_cmap_blue[index]),  count);

	r = &(vc->vc_cmap_red[index]);
	g = &(vc->vc_cmap_green[index]);
	b = &(vc->vc_cmap_blue[index]);

	for (i = 0; i < count; i++) {
		OF_call_method_1("color!", vc->vc_ofh, 4, *r, *g, *b, index);
		r++, g++, b++, index++;
	}
	return 0;
}
