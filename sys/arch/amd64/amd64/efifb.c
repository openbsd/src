/*	$OpenBSD: efifb.c,v 1.18 2018/09/22 17:41:52 kettenis Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
 * Copyright (c) 2016 joshua stein <jcs@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>
#include <machine/bus.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/pci/pcivar.h>

#include <machine/biosvar.h>
#include <machine/efifbvar.h>

/* coreboot tables */

struct cb_header {
	union {
		uint8_t signature[4]; /* "LBIO" */
		uint32_t signature32;
	};
	uint32_t	header_bytes;
	uint32_t	header_checksum;
	uint32_t	table_bytes;
	uint32_t	table_checksum;
	uint32_t	table_entries;
};

struct cb_framebuffer {
	uint64_t	physical_address;
	uint32_t	x_resolution;
	uint32_t	y_resolution;
	uint32_t	bytes_per_line;
	uint8_t		bits_per_pixel;
	uint8_t		red_mask_pos;
	uint8_t		red_mask_size;
	uint8_t		green_mask_pos;
	uint8_t		green_mask_size;
	uint8_t		blue_mask_pos;
	uint8_t		blue_mask_size;
	uint8_t		reserved_mask_pos;
	uint8_t		reserved_mask_size;
};

struct cb_entry {
	uint32_t	tag;
#define CB_TAG_VERSION          0x0004
#define CB_TAG_FORWARD          0x0011
#define CB_TAG_FRAMEBUFFER      0x0012
	uint32_t	size;
	union {
		char	string[0];
		uint64_t forward;
		struct cb_framebuffer fb;
	} u;
};

struct efifb {
	struct rasops_info	 rinfo;
	int			 depth;
	paddr_t			 paddr;
	psize_t			 psize;
	int			 detached;

	struct cb_framebuffer	 cb_table_fb;
};

struct efifb_softc {
	struct device		 sc_dev;
	struct efifb		*sc_fb;
};

int	 efifb_match(struct device *, void *, void *);
void	 efifb_attach(struct device *, struct device *, void *);
void	 efifb_rasops_preinit(struct efifb *);
int	 efifb_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	 efifb_mmap(void *, off_t, int);
int	 efifb_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	 efifb_free_screen(void *, void *);
int	 efifb_show_screen(void *, void *, int, void (*cb) (void *, int, int),
	    void *);
int	 efifb_list_font(void *, struct wsdisplay_font *);
int	 efifb_load_font(void *, void *, struct wsdisplay_font *);
void	 efifb_scrollback(void *, void *, int lines);
void	 efifb_efiinfo_init(struct efifb *);
void	 efifb_cnattach_common(void);

struct cb_framebuffer *cb_find_fb(paddr_t);

extern int	(*ws_get_param)(struct wsdisplay_param *);
extern int	(*ws_set_param)(struct wsdisplay_param *);

const struct cfattach efifb_ca = {
	sizeof(struct efifb_softc), efifb_match, efifb_attach, NULL
};

#define	EFIFB_WIDTH	100
#define	EFIFB_HEIGHT	31

struct wsscreen_descr efifb_std_descr = { "std" };

const struct wsscreen_descr *efifb_descrs[] = {
	&efifb_std_descr
};

const struct wsscreen_list efifb_screen_list = {
	nitems(efifb_descrs), efifb_descrs
};

struct wsdisplay_accessops efifb_accessops = {
	.ioctl = efifb_ioctl,
	.mmap = efifb_mmap,
	.alloc_screen = efifb_alloc_screen,
	.free_screen = efifb_free_screen,
	.show_screen = efifb_show_screen,
	.load_font = efifb_load_font,
	.list_font = efifb_list_font,
	.scrollback = efifb_scrollback,
};

struct cfdriver efifb_cd = {
	NULL, "efifb", DV_DULL
};

struct efifb efifb_console;
struct wsdisplay_charcell efifb_bs[EFIFB_HEIGHT * EFIFB_WIDTH];

int
efifb_match(struct device *parent, void *cf, void *aux)
{
	struct efifb_attach_args *eaa = aux;

	if (strcmp(eaa->eaa_name, efifb_cd.cd_name) == 0) {
		if (efifb_console.paddr != 0) {
			if (efifb_console.detached)
				return (0);
			return (1);
		}
		if (bios_efiinfo != NULL && bios_efiinfo->fb_addr != 0)
			return (1);
	}

	return (0);
}

void
efifb_attach(struct device *parent, struct device *self, void *aux)
{
	struct efifb		*fb;
	struct efifb_softc	*sc = (struct efifb_softc *)self;
	struct wsemuldisplaydev_attach_args aa;
	struct rasops_info 	*ri;
	int			 console = 0, ccol = 0, crow = 0;
	bus_space_tag_t		 iot = X86_BUS_SPACE_MEM;
	bus_space_handle_t	 ioh;

	if (efifb_console.paddr != 0) {
		fb = &efifb_console;
		ri = &fb->rinfo;
		console = 1;
	} else {
		KASSERT(bios_efiinfo != NULL && bios_efiinfo->fb_addr != 0);

		if ((fb = malloc(sizeof(*fb), M_DEVBUF, M_ZERO | M_NOWAIT))
		    == NULL)
			return;

		ri = &fb->rinfo;
		efifb_efiinfo_init(fb);

		if (bus_space_map(iot, fb->paddr, fb->psize,
		    BUS_SPACE_MAP_PREFETCHABLE | BUS_SPACE_MAP_LINEAR,
		    &ioh) != 0) {
			free(fb, M_DEVBUF, sizeof(*fb));
			return;
		}
		ri->ri_bits = bus_space_vaddr(iot, ioh);
		efifb_rasops_preinit(fb);
		ri->ri_flg = RI_VCONS | RI_CENTER | RI_WRONLY;
		rasops_init(ri, EFIFB_HEIGHT, EFIFB_WIDTH);
		efifb_std_descr.ncols = ri->ri_cols;
		efifb_std_descr.nrows = ri->ri_rows;
		efifb_std_descr.textops = &ri->ri_ops;
		efifb_std_descr.fontwidth = ri->ri_font->fontwidth;
		efifb_std_descr.fontheight = ri->ri_font->fontheight;
		efifb_std_descr.capabilities = ri->ri_caps;
	}

	sc->sc_fb = fb;
	printf(": %dx%d, %dbpp\n", ri->ri_width, ri->ri_height, ri->ri_depth);

	if (console) {
		long	 defattr = 0;

		ccol = ri->ri_ccol;
		crow = ri->ri_crow;

		efifb_rasops_preinit(fb);
		ri->ri_flg &= ~RI_CLEAR;
		ri->ri_flg |= RI_VCONS | RI_WRONLY;

		rasops_init(ri, efifb_std_descr.nrows, efifb_std_descr.ncols);

		ri->ri_ops.alloc_attr(ri->ri_active, 0, 0, 0, &defattr);
		wsdisplay_cnattach(&efifb_std_descr, ri->ri_active, ccol, crow,
		    defattr);
	}

	memset(&aa, 0, sizeof(aa));
	aa.console = console;
	aa.scrdata = &efifb_screen_list;
	aa.accessops = &efifb_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	config_found_sm(self, &aa, wsemuldisplaydevprint,
	    wsemuldisplaydevsubmatch);
}

void
efifb_rasops_preinit(struct efifb *fb)
{
#define bmnum(_x) (fls(_x) - ffs(_x) + 1)
#define bmpos(_x) (ffs(_x) - 1)
	struct rasops_info	*ri = &fb->rinfo;

	if (efifb_console.cb_table_fb.x_resolution) {
		ri->ri_width = efifb_console.cb_table_fb.x_resolution;
		ri->ri_height = efifb_console.cb_table_fb.y_resolution;
		ri->ri_depth = fb->depth;
		ri->ri_stride = efifb_console.cb_table_fb.bytes_per_line;
		ri->ri_rnum = efifb_console.cb_table_fb.red_mask_size;
		ri->ri_rpos = efifb_console.cb_table_fb.red_mask_pos;
		ri->ri_gnum = efifb_console.cb_table_fb.green_mask_size;
		ri->ri_gpos = efifb_console.cb_table_fb.green_mask_pos;
		ri->ri_bnum = efifb_console.cb_table_fb.blue_mask_size;
		ri->ri_bpos = efifb_console.cb_table_fb.blue_mask_pos;
	} else {
		ri->ri_width = bios_efiinfo->fb_width;
		ri->ri_height = bios_efiinfo->fb_height;
		ri->ri_depth = fb->depth;
		ri->ri_stride = bios_efiinfo->fb_pixpsl * (fb->depth / 8);
		ri->ri_rnum = bmnum(bios_efiinfo->fb_red_mask);
		ri->ri_rpos = bmpos(bios_efiinfo->fb_red_mask);
		ri->ri_gnum = bmnum(bios_efiinfo->fb_green_mask);
		ri->ri_gpos = bmpos(bios_efiinfo->fb_green_mask);
		ri->ri_bnum = bmnum(bios_efiinfo->fb_blue_mask);
		ri->ri_bpos = bmpos(bios_efiinfo->fb_blue_mask);
	}
	ri->ri_bs = efifb_bs;
}

int
efifb_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct efifb_softc	*sc = v;
	struct efifb		*fb = sc->sc_fb;
	struct rasops_info 	*ri = &fb->rinfo;
	struct wsdisplay_fbinfo	*wdf;

	switch (cmd) {
	case WSDISPLAYIO_GETPARAM:
		if (ws_get_param != NULL)
			return (*ws_get_param)((struct wsdisplay_param *)data);
		else
			return (-1);
	case WSDISPLAYIO_SETPARAM:
		if (ws_set_param != NULL)
			return (*ws_set_param)((struct wsdisplay_param *)data);
		else
			return (-1);
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_EFIFB;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->width = ri->ri_width;
		wdf->height = ri->ri_height;
		wdf->depth = ri->ri_depth;
		wdf->cmsize = 0;	/* color map is unavailable */
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = ri->ri_stride;
		break;
	case WSDISPLAYIO_SMODE:
		break;
	case WSDISPLAYIO_GETSUPPORTEDDEPTH:
		switch (ri->ri_depth) {
		case 32:
			*(u_int *)data = WSDISPLAYIO_DEPTH_24_32;
			break;
		case 24:
			*(u_int *)data = WSDISPLAYIO_DEPTH_24_24;
			break;
		case 16:
			*(u_int *)data = WSDISPLAYIO_DEPTH_16;
			break;
		case 15:
			*(u_int *)data = WSDISPLAYIO_DEPTH_15;
			break;
		case 8:
			*(u_int *)data = WSDISPLAYIO_DEPTH_8;
			break;
		default:
			return (-1);
		}
		break;
	default:
		return (-1);
	}

	return (0);
}

paddr_t
efifb_mmap(void *v, off_t off, int prot)
{
	struct efifb_softc	*sc = v;

	if (off < 0 || off >= sc->sc_fb->psize)
		return (-1);

	return ((sc->sc_fb->paddr + off) | PMAP_WC);
}

int
efifb_alloc_screen(void *v, const struct wsscreen_descr *descr,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct efifb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_fb->rinfo;

	return rasops_alloc_screen(ri, cookiep, curxp, curyp, attrp);
}

void
efifb_free_screen(void *v, void *cookie)
{
	struct efifb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_fb->rinfo;

	rasops_free_screen(ri, cookie);
}

int
efifb_show_screen(void *v, void *cookie, int waitok,
    void (*cb) (void *, int, int), void *cb_arg)
{
	struct efifb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_fb->rinfo;

	return rasops_show_screen(ri, cookie, waitok, cb, cb_arg);
}

int
efifb_load_font(void *v, void *cookie, struct wsdisplay_font *font)
{
	struct efifb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_fb->rinfo;

	return (rasops_load_font(ri, cookie, font));
}

int
efifb_list_font(void *v, struct wsdisplay_font *font)
{
	struct efifb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_fb->rinfo;

	return (rasops_list_font(ri, font));
}

void
efifb_scrollback(void *v, void *cookie, int lines)
{
	struct efifb_softc	*sc = v;
	struct rasops_info	*ri = &sc->sc_fb->rinfo;

	rasops_scrollback(ri, cookie, lines);
}

int
efifb_cnattach(void)
{
	if (bios_efiinfo == NULL || bios_efiinfo->fb_addr == 0)
		return (-1);

	memset(&efifb_console, 0, sizeof(efifb_console));
	efifb_efiinfo_init(&efifb_console);
	efifb_cnattach_common();

	return (0);
}

void
efifb_efiinfo_init(struct efifb *fb)
{
	fb->paddr = bios_efiinfo->fb_addr;
	fb->depth = max(fb->depth, fls(bios_efiinfo->fb_red_mask));
	fb->depth = max(fb->depth, fls(bios_efiinfo->fb_green_mask));
	fb->depth = max(fb->depth, fls(bios_efiinfo->fb_blue_mask));
	fb->depth = max(fb->depth, fls(bios_efiinfo->fb_reserved_mask));
	fb->psize = bios_efiinfo->fb_height *
	    bios_efiinfo->fb_pixpsl * (fb->depth / 8);
}

void
efifb_cnattach_common(void)
{
	struct efifb		*fb = &efifb_console;
	struct rasops_info	*ri = &fb->rinfo;
	long			 defattr = 0;

	ri->ri_bits = (u_char *)PMAP_DIRECT_MAP(fb->paddr);

	efifb_rasops_preinit(fb);

	ri->ri_flg = RI_CLEAR | RI_CENTER | RI_WRONLY;
	rasops_init(ri, EFIFB_HEIGHT, EFIFB_WIDTH);
	efifb_std_descr.ncols = ri->ri_cols;
	efifb_std_descr.nrows = ri->ri_rows;
	efifb_std_descr.textops = &ri->ri_ops;
	efifb_std_descr.fontwidth = ri->ri_font->fontwidth;
	efifb_std_descr.fontheight = ri->ri_font->fontheight;
	efifb_std_descr.capabilities = ri->ri_caps;

	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&efifb_std_descr, ri, 0, 0, defattr);
}

void
efifb_cnremap(void)
{
	struct efifb		*fb = &efifb_console;
	struct rasops_info	*ri = &fb->rinfo;
	bus_space_tag_t		 iot = X86_BUS_SPACE_MEM;
	bus_space_handle_t	 ioh;

	if (fb->paddr == 0)
		return;

	if (_bus_space_map(iot, fb->paddr, fb->psize,
	    BUS_SPACE_MAP_PREFETCHABLE | BUS_SPACE_MAP_LINEAR, &ioh) == 0)
		ri->ri_origbits = bus_space_vaddr(iot, ioh);

	efifb_rasops_preinit(fb);
	ri->ri_flg &= ~RI_CLEAR;
	ri->ri_flg |= RI_CENTER | RI_WRONLY;

	rasops_init(ri, efifb_std_descr.nrows, efifb_std_descr.ncols);
}

int
efifb_is_console(struct pci_attach_args *pa)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	pcireg_t type;
	bus_addr_t base;
	bus_size_t size;
	int reg;

	for (reg = PCI_MAPREG_START; reg < PCI_MAPREG_END; reg += 4) {
		if (!pci_mapreg_probe(pc, tag, reg, &type))
			continue;

		if (type == PCI_MAPREG_TYPE_IO)
			continue;

		if (pci_mapreg_info(pc, tag, reg, type, &base, &size, NULL))
			continue;

		if (efifb_console.paddr >= base &&
		    efifb_console.paddr < base + size)
			return 1;

		if (type & PCI_MAPREG_MEM_TYPE_64BIT)
			reg += 4;
	}

	return 0;
}

void
efifb_cndetach(void)
{
	efifb_console.detached = 1;
}

void
efifb_cnreattach(void)
{
	efifb_console.detached = 0;
}

int
efifb_cb_cnattach(void)
{
	struct cb_framebuffer *cb_fb = cb_find_fb((paddr_t)0x0);

	if (cb_fb == NULL || !cb_fb->x_resolution)
		return (-1);

	memset(&efifb_console, 0, sizeof(efifb_console));
	memcpy(&efifb_console.cb_table_fb, cb_fb,
	    sizeof(struct cb_framebuffer));

	efifb_console.paddr = cb_fb->physical_address;
	efifb_console.depth = cb_fb->bits_per_pixel;
	efifb_console.psize = cb_fb->y_resolution * cb_fb->bytes_per_line;

	efifb_cnattach_common();

	return (0);
}

int
efifb_cb_found(void)
{
	return (efifb_console.paddr && efifb_console.cb_table_fb.x_resolution);
}

static uint16_t
cb_checksum(const void *addr, unsigned size)
{
	const uint16_t *p = addr;
	unsigned i, n = size / 2;
	uint32_t sum = 0;

	for (i = 0; i < n; i++)
		sum += p[i];

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	sum = ~sum & 0xffff;

	return (uint16_t)sum;
}

struct cb_framebuffer *
cb_find_fb(paddr_t addr)
{
	int i, j;

	for (i = 0; i < (4 * 1024); i += 16) {
		struct cb_header *cbh;
		struct cb_entry *cbe;
		paddr_t cbtable;

		cbh = (struct cb_header *)(PMAP_DIRECT_MAP(addr + i));
		if (memcmp(cbh->signature, "LBIO", 4) != 0)
			continue;

		if (!cbh->header_bytes)
			continue;

		if (cb_checksum(cbh, sizeof(*cbh)) != 0)
			return NULL;

		cbtable = PMAP_DIRECT_MAP(addr + i + cbh->header_bytes);

		for (j = 0; j < cbh->table_bytes; j += cbe->size) {
			cbe = (struct cb_entry *)((char *)cbtable + j);

			switch (cbe->tag) {
			case CB_TAG_FORWARD:
				return cb_find_fb(cbe->u.forward);

			case CB_TAG_FRAMEBUFFER:
				return &cbe->u.fb;
			}
		}
	}

	return NULL;
}
