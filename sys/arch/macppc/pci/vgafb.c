/*	$OpenBSD: vgafb.c,v 1.15 2002/07/21 16:31:15 drahn Exp $	*/
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

#include <uvm/uvm_extern.h>

#include <machine/bus.h>

#include <dev/cons.h>
#include <dev/ofw/openfirm.h>
#include <macppc/macppc/ofw_machdep.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

#include <macppc/pci/vgafbvar.h>

struct cfdriver vgafb_cd = {
	NULL, "vgafb", DV_DULL,
};

void vgafb_setcolor(struct vgafb_config *vc, unsigned int index, 
		    u_int8_t r, u_int8_t g, u_int8_t b);
void vgafb_restore_default_colors(struct vgafb_config *vc);

struct vgafb_devconfig {
	struct rasops_info dc_rinfo;    /* raster display data */
	int dc_blanked;			/* currently had video disabled */
};

struct vgafb_devconfig vgafb_console_dc;

struct wsscreen_descr vgafb_stdscreen = {
	"std",
	0, 0,   /* will be filled in -- XXX shouldn't, it's global */
	0,
	0, 0,
	WSSCREEN_UNDERLINE | WSSCREEN_HILIT |
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};
const struct wsscreen_descr *vgafb_scrlist[] = {
	&vgafb_stdscreen,
	/* XXX other formats, graphics screen? */
};
   
struct wsscreen_list vgafb_screenlist = {
	sizeof(vgafb_scrlist) / sizeof(struct wsscreen_descr *), vgafb_scrlist
};

struct wsdisplay_accessops vgafb_accessops = {
	vgafb_ioctl,
	vgafb_mmap,
	vgafb_alloc_screen,
	vgafb_free_screen,
	vgafb_show_screen,
	NULL,		/* load_font */
	NULL,		/* scrollback */
	NULL,		/* getchar */
	vgafb_burn,	/* burner */
};

int	vgafb_getcmap(struct vgafb_config *vc, struct wsdisplay_cmap *cm);
int	vgafb_putcmap(struct vgafb_config *vc, struct wsdisplay_cmap *cm);

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
		if (bus_space_map(iot, mmiobase, mmiosize, 0, &mmioh))
			goto bad;
		gotmmio = 1;
	}

	rv = 1;

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

	/* memsize should only be visible region for console */
	memsize = cons_height * cons_linebytes;
        if (bus_space_map(vc->vc_memt, membase, memsize, 1, &vc->vc_memh))
		panic("vgafb_common_setup: couldn't map memory"); 
	cons_display_mem_h = vc->vc_memh;
	vc->vc_ofh = cons_display_ofh;


	vc->vc_crow = vc->vc_ccol = 0; /* Has to be some onscreen value */
	vc->vc_so = 0;

	/* clear screen, frob cursor, etc.? */
	/*
	*/

	vc->vc_at = 0x00 | 0xf;			/* black bg|white fg */
	vc->vc_so_at = 0x00 | 0xf | 0x80;	/* black bg|white fg|blink */

	if (cons_depth == 8) { 
		vgafb_restore_default_colors(vc);
	}
}

void
vgafb_restore_default_colors(struct vgafb_config *vc)
{ 
	int i;

	for (i = 0; i < 256; i++) {
		const u_char *color;

		color = &rasops_cmap[i * 3];
		vgafb_setcolor(vc, i, color[0], color[1], color[2]);
	}
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

	/* no need to keep the burner function if no hw support */
	if (cons_backlight_available == 0)
		vgafb_accessops.burn_screen = NULL;
	else {
		vc->vc_backlight_on = WSDISPLAYIO_VIDEO_OFF;
		vgafb_burn(vc, WSDISPLAYIO_VIDEO_ON, 0);	/* paranoia */
	}
 
        config_found(parent, &aa, wsemuldisplaydevprint);
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

	case WSDISPLAYIO_SMODE:
		/* track the state of the display,
		 * if returning to WSDISPLAYIO_MODE_EMUL
		 * restore the last palette, workaround for 
		 * bad accellerated X servers that does not restore
		 * the correct palette.
		 */

		if (cons_depth == 8) { 
			vgafb_restore_default_colors(vc);
		}

		/* now that we have done our work, let the wscons
		 * layer handle this ioctl
		 */
		return -1;

	case WSDISPLAYIO_GETPARAM:
	{
		struct wsdisplay_param *dp = (struct wsdisplay_param *)data;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			dp->min = MIN_BRIGHTNESS;
			dp->max = MAX_BRIGHTNESS;
			dp->curval = cons_brightness;
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			if (cons_backlight_available != 0) {
				dp->min = 0;
				dp->max = 1;
				dp->curval = vc->vc_backlight_on;
				return 0;
			} else
				return -1;
		}
	}
		return -1;

	case WSDISPLAYIO_SETPARAM:
	{
		struct wsdisplay_param *dp = (struct wsdisplay_param *)data;

		switch (dp->param) {
		case WSDISPLAYIO_PARAM_BRIGHTNESS:
			of_setbrightness(dp->curval);
			return 0;
		case WSDISPLAYIO_PARAM_BACKLIGHT:
			if (cons_backlight_available != 0) {
				vgafb_burn(vc,
				    dp->curval ? WSDISPLAYIO_VIDEO_ON :
				      WSDISPLAYIO_VIDEO_OFF, 0);
				return 0;
			} else
				return -1;
		}
	}
		return -1;

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
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
	if (offset >= 0x00000 && offset < vc->memsize)
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
	else if (offset >= 0x20000000 && offset < 0x20000000+vc->mmiosize)
		/* mmiosize... */
		h = vc->vc_mmioh + (offset - 0x20000000);
	else if (offset >= vc->membase && (offset < vc->membase+vc->memsize)) {
		/* allow mmapping of memory */
		h = offset;
	} else if (offset >= vc->mmiobase &&
	    (offset < vc->mmiobase+vc->mmiosize)) {
		/* allow mmapping of mmio space */
		h = offset;
	} else {
		h = -1;
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
}

void
vgafb_cnattach(iot, memt, pc, bus, device, function)
	void *pc;
	bus_space_tag_t iot, memt;
	int bus, device, function;
{
        long defattr;

	struct vgafb_devconfig *dc = &vgafb_console_dc;
        struct rasops_info *ri = &dc->dc_rinfo;

	ri->ri_flg = RI_CENTER;
	ri->ri_depth = cons_depth;
	ri->ri_bits = (void *)cons_display_mem_h;
	ri->ri_width = cons_width;
	ri->ri_height = cons_height;
	ri->ri_stride = cons_linebytes;
	ri->ri_hw = dc;

	rasops_init(ri, 160, 160);	/* XXX */

	vgafb_stdscreen.nrows = ri->ri_rows;
	vgafb_stdscreen.ncols = ri->ri_cols;
	vgafb_stdscreen.textops = &ri->ri_ops;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);

	wsdisplay_cnattach(&vgafb_stdscreen, ri, 0, 0, defattr);
}

struct {
	u_int8_t r;
	u_int8_t g;
	u_int8_t b;
} vgafb_color[256];

void
vgafb_setcolor(vc, index, r, g, b) 
	struct vgafb_config *vc;
	unsigned int index;
	u_int8_t r, g, b;
{
	vc->vc_cmap_red[index] = r;
	vc->vc_cmap_green[index] = g;
	vc->vc_cmap_blue[index] = b;

	vgafb_color[index].r = r;
	vgafb_color[index].g = g;
	vgafb_color[index].b = b;
	OF_call_method_1("set-colors", cons_display_ofh, 3,
	    &vgafb_color[index], index, 1);
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
	u_int8_t *r, *g, *b;

	if (index >= 256 || count > 256 || index + count > 256)
		return EINVAL;
	if (!uvm_useracc(cm->red, count, B_READ) ||
	    !uvm_useracc(cm->green, count, B_READ) ||
	    !uvm_useracc(cm->blue, count, B_READ))
		return EFAULT;
	copyin(cm->red,   &(vc->vc_cmap_red[index]),   count);
	copyin(cm->green, &(vc->vc_cmap_green[index]), count);
	copyin(cm->blue,  &(vc->vc_cmap_blue[index]),  count);

	r = &(vc->vc_cmap_red[index]);
	g = &(vc->vc_cmap_green[index]);
	b = &(vc->vc_cmap_blue[index]);

	for (i = 0; i < count; i++) {
		vgafb_color[i].r = *r;
		vgafb_color[i].g = *g;
		vgafb_color[i].b = *b;
		r++, g++, b++;
	}
	OF_call_method_1("set-colors", cons_display_ofh, 3,
	    &vgafb_color, index, count);
	return 0;
}

void
vgafb_burn(v, on, flags)
	void *v;
	u_int on, flags;
{
	struct vgafb_config *vc = v;

	if (vc->vc_backlight_on != on) {
		if (on == WSDISPLAYIO_VIDEO_ON) {
			OF_call_method_1("backlight-on", cons_display_ofh, 0);
		} else {
			OF_call_method_1("backlight-off", cons_display_ofh, 0);
		}
		vc->vc_backlight_on = on;
	}
}
