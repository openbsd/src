/*	$OpenBSD: udl.c,v 1.30 2009/09/05 14:09:35 miod Exp $ */

/*
 * Copyright (c) 2009 Marcus Glocker <mglocker@openbsd.org>
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

/*
 * Driver for the ``DisplayLink DL-120 / DL-160'' graphic chips based
 * on the reversed engineered specifications of Florian Echtler
 * <floe@butterbrot.org>:
 *
 * 	http://floe.butterbrot.org/displaylink/doku.php
 *
 * This driver has been inspired by the cfxga(4) driver because we have
 * to deal with similar challenges, like no direct access to the video
 * memory.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <uvm/uvm.h>

#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <dev/usb/udl.h>

/*
 * Defines.
 */
#if 0
#define UDL_DEBUG
#endif
#ifdef UDL_DEBUG
int udl_debug = 1;
#define DPRINTF(l, x...) do { if ((l) <= udl_debug) printf(x); } while (0)
#else
#define DPRINTF(l, x...)
#endif

#define DN(sc)		((sc)->sc_dev.dv_xname)
#define FUNC		__func__

/*
 * Prototypes.
 */
int		udl_match(struct device *, void *, void *);
void		udl_attach(struct device *, struct device *, void *);
void		udl_attach_hook(void *);
int		udl_detach(struct device *, int);
int		udl_activate(struct device *, enum devact);

int		udl_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t		udl_mmap(void *, off_t, int);
int		udl_alloc_screen(void *, const struct wsscreen_descr *,
		    void **, int *, int *, long *);
void		udl_free_screen(void *, void *);
int		udl_show_screen(void *, void *, int,
		    void (*)(void *, int, int), void *);
void		udl_burner(void *, u_int, u_int);

int		udl_copycols(void *, int, int, int, int);
int		udl_copyrows(void *, int, int, int);
int		udl_erasecols(void *, int, int, int, long);
int		udl_eraserows(void *, int, int, long);
int		udl_putchar(void *, int, int, u_int, long);
int		udl_do_cursor(struct rasops_info *);

usbd_status	udl_ctrl_msg(struct udl_softc *, uint8_t, uint8_t,
		    uint16_t, uint16_t, uint8_t *, size_t);
usbd_status	udl_poll(struct udl_softc *, uint32_t *);
usbd_status	udl_read_1(struct udl_softc *, uint16_t, uint8_t *);
usbd_status	udl_write_1(struct udl_softc *, uint16_t, uint8_t);
usbd_status	udl_read_edid(struct udl_softc *, uint8_t *);
usbd_status	udl_set_enc_key(struct udl_softc *, uint8_t *, uint8_t);
usbd_status	udl_set_decomp_table(struct udl_softc *, uint8_t *, uint16_t);

int		udl_load_huffman(struct udl_softc *);
void		udl_free_huffman(struct udl_softc *);
usbd_status	udl_cmd_alloc_xfer(struct udl_softc *);
void		udl_cmd_free_xfer(struct udl_softc *);
int		udl_cmd_alloc_buf(struct udl_softc *);
void		udl_cmd_free_buf(struct udl_softc *);
void		udl_cmd_insert_int_1(struct udl_softc *, uint8_t);
void		udl_cmd_insert_int_2(struct udl_softc *, uint16_t);
void		udl_cmd_insert_int_3(struct udl_softc *, uint32_t);
void		udl_cmd_insert_int_4(struct udl_softc *, uint32_t);
void		udl_cmd_insert_buf(struct udl_softc *, uint8_t *, uint32_t);
int		udl_cmd_insert_buf_comp(struct udl_softc *, uint8_t *,
		    uint32_t);
int		udl_cmd_insert_head_comp(struct udl_softc *, uint32_t);
void		udl_cmd_insert_check(struct udl_cmd_buf *, int);
void		udl_cmd_write_reg_1(struct udl_softc *, uint8_t, uint8_t);
void		udl_cmd_write_reg_3(struct udl_softc *, uint8_t, uint32_t);
usbd_status	udl_cmd_send(struct udl_softc *);
usbd_status	udl_cmd_send_async(struct udl_softc *);
void		udl_cmd_send_async_cb(usbd_xfer_handle, usbd_private_handle,
		    usbd_status);

usbd_status	udl_init_chip(struct udl_softc *);
void		udl_init_fb_offsets(struct udl_softc *, uint32_t, uint32_t,
		    uint32_t, uint32_t);
usbd_status	udl_init_resolution(struct udl_softc *, uint8_t *, uint8_t);
void		udl_fb_off_write(struct udl_softc *, uint16_t, uint32_t,
		    uint16_t);
void		udl_fb_line_write(struct udl_softc *, uint16_t, uint32_t,
		    uint32_t, uint32_t);
void		udl_fb_block_write(struct udl_softc *, uint16_t, uint32_t,
		    uint32_t, uint32_t, uint32_t);
void		udl_fb_buf_write(struct udl_softc *, uint8_t *, uint32_t,
		    uint32_t, uint16_t);
void		udl_fb_off_copy(struct udl_softc *, uint32_t, uint32_t,
		    uint16_t);
void		udl_fb_line_copy(struct udl_softc *, uint32_t, uint32_t,
		    uint32_t, uint32_t, uint32_t);
void		udl_fb_block_copy(struct udl_softc *, uint32_t, uint32_t,
		    uint32_t, uint32_t, uint32_t, uint32_t);
void		udl_fb_off_write_comp(struct udl_softc *, uint16_t, uint32_t,
		    uint16_t);
void		udl_fb_line_write_comp(struct udl_softc *, uint16_t, uint32_t,
		    uint32_t, uint32_t);
void		udl_fb_block_write_comp(struct udl_softc *, uint16_t, uint32_t,
		    uint32_t, uint32_t, uint32_t);
void		udl_fb_buf_write_comp(struct udl_softc *, uint8_t *, uint32_t,
		    uint32_t, uint16_t);
void		udl_fb_off_copy_comp(struct udl_softc *, uint32_t, uint32_t,
		    uint16_t);
void		udl_fb_line_copy_comp(struct udl_softc *, uint32_t, uint32_t,
		    uint32_t, uint32_t, uint32_t);
void		udl_fb_block_copy_comp(struct udl_softc *, uint32_t, uint32_t,
		    uint32_t, uint32_t, uint32_t, uint32_t);
void		udl_draw_char(struct udl_softc *, uint16_t, uint16_t, u_int,
		    uint32_t, uint32_t);
#ifdef UDL_DEBUG
void		udl_hexdump(void *, int, int);
usbd_status	udl_init_test(struct udl_softc *);
#endif

/*
 * Driver glue.
 */
struct cfdriver udl_cd = {
	NULL, "udl", DV_DULL
};

const struct cfattach udl_ca = {
	sizeof(struct udl_softc),
	udl_match,
	udl_attach,
	udl_detach,
	udl_activate
};

/*
 * wsdisplay glue.
 */
struct wsscreen_descr udl_stdscreen = {
	"std",			/* name */
	0, 0,			/* ncols, nrows */
	NULL,			/* textops */
	0, 0,			/* fontwidth, fontheight */
	WSSCREEN_WSCOLORS	/* capabilities */
};

const struct wsscreen_descr *udl_scrlist[] = {
	&udl_stdscreen
};

struct wsscreen_list udl_screenlist = {
	sizeof(udl_scrlist) / sizeof(struct wsscreen_descr *), udl_scrlist
};

struct wsdisplay_accessops udl_accessops = {
	udl_ioctl,
	udl_mmap,
	udl_alloc_screen,
	udl_free_screen,
	udl_show_screen,
	NULL,
	NULL,
	NULL,
	udl_burner
};

/*
 * Matching devices.
 */
static const struct usb_devno udl_devs[] = {
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LCD4300U },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LCD8000U },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_LD220 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_VCUD60 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_DLDVI },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_VGA10 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_WSDVI },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_EC008 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_HPDOCK },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_M01061 },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_SWDVI },
	{ USB_VENDOR_DISPLAYLINK, USB_PRODUCT_DISPLAYLINK_UM7X0 }
};

int
udl_match(struct device *parent, void *match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	if (usb_lookup(udl_devs, uaa->vendor, uaa->product) != NULL)
		return (UMATCH_VENDOR_PRODUCT);

	return (UMATCH_NONE);
}

void
udl_attach(struct device *parent, struct device *self, void *aux)
{
	struct udl_softc *sc = (struct udl_softc *)self;
	struct usb_attach_arg *uaa = aux;
	struct wsemuldisplaydev_attach_args aa;
	usbd_status error;
	int err;

	sc->sc_udev = uaa->device;

	/*
	 * Set device configuration descriptor number.
	 */
	error = usbd_set_config_no(sc->sc_udev, 1, 0);
	if (error != USBD_NORMAL_COMPLETION)
		return;

	/*
	 * Create device handle to interface descriptor.
	 */
	error = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface);
	if (error != USBD_NORMAL_COMPLETION)
		return;

	/*
	 * Allocate bulk command xfer.
	 */
	error = udl_cmd_alloc_xfer(sc);
	if (error != USBD_NORMAL_COMPLETION)
		return;

	/*
	 * Allocate command buffer.
	 */
	err = udl_cmd_alloc_buf(sc);
	if (err != 0)
		return;

	/*
	 * Open bulk TX pipe.
	 */
	error = usbd_open_pipe(sc->sc_iface, 0x01, USBD_EXCLUSIVE_USE,
	    &sc->sc_tx_pipeh);
	if (error != USBD_NORMAL_COMPLETION)
		return;

	/*
	 * Initialize chip.
	 */
	error = udl_init_chip(sc);
	if (error != USBD_NORMAL_COMPLETION)
		return;

	/*
	 * Initialize resolution.
	 */
	sc->sc_width = 800;	/* XXX shouldn't we do this somewhere else? */
	sc->sc_height = 600;
	sc->sc_depth = 16;

	error = udl_init_resolution(sc, udl_reg_vals_800,
	    sizeof(udl_reg_vals_800));
	if (error != USBD_NORMAL_COMPLETION)
		return;

	/*
	 * Attach wsdisplay.
	 */
	aa.console = 0;
	aa.scrdata = &udl_screenlist;
	aa.accessops = &udl_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	sc->sc_wsdisplay = config_found(self, &aa, wsemuldisplaydevprint);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, &sc->sc_dev);

	/*
	 * Load Huffman table.
	 */
	if (rootvp == NULL)
		mountroothook_establish(udl_attach_hook, sc);
	else
		udl_attach_hook(sc);
}

void
udl_attach_hook(void *arg)
{
	struct udl_softc *sc = arg;

	if (udl_load_huffman(sc) != 0) {
		/* compression not possible */
		printf("%s: run in uncompressed mode\n", DN(sc));
		sc->udl_fb_off_write = udl_fb_off_write;
		sc->udl_fb_line_write = udl_fb_line_write;
		sc->udl_fb_block_write = udl_fb_block_write;
		sc->udl_fb_buf_write = udl_fb_buf_write;
		sc->udl_fb_off_copy = udl_fb_off_copy;
		sc->udl_fb_line_copy = udl_fb_line_copy;
		sc->udl_fb_block_copy = udl_fb_block_copy;
	} else {
		/* compression possible */
		sc->udl_fb_off_write = udl_fb_off_write_comp;
		sc->udl_fb_line_write = udl_fb_line_write_comp;
		sc->udl_fb_block_write = udl_fb_block_write_comp;
		sc->udl_fb_buf_write = udl_fb_buf_write_comp;
		sc->udl_fb_off_copy = udl_fb_off_copy_comp;
		sc->udl_fb_line_copy = udl_fb_line_copy_comp;
		sc->udl_fb_block_copy = udl_fb_block_copy_comp;
	}
#ifdef UDL_DEBUG
	udl_init_test(sc);
#endif
}

int
udl_detach(struct device *self, int flags)
{
	struct udl_softc *sc = (struct udl_softc *)self;

	/*
	 * Close bulk TX pipe.
	 */
	if (sc->sc_tx_pipeh != NULL) {
		usbd_abort_pipe(sc->sc_tx_pipeh);
		usbd_close_pipe(sc->sc_tx_pipeh);
	}

	/*
	 * Free command buffer.
	 */
	udl_cmd_free_buf(sc);

	/*
	 * Free command xfer.
	 */
	udl_cmd_free_xfer(sc);

	/*
	 * Free Huffman table.
	 */
	udl_free_huffman(sc);

	/*
	 * Detach wsdisplay.
	 */
	if (sc->sc_wsdisplay != NULL)
		config_detach(sc->sc_wsdisplay, DETACH_FORCE);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, &sc->sc_dev);

	return (0);
}

int
udl_activate(struct device *self, enum devact act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		break;
	case DVACT_DEACTIVATE:
		break;
	}

	return (0);
}

/* ---------- */

int
udl_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct udl_softc *sc;

	sc = v;

	DPRINTF(1, "%s: %s: ('%c', %d, %d)\n",
	    DN(sc), FUNC, IOCGROUP(cmd), cmd & 0xff, IOCPARM_LEN(cmd));

	/* TODO */
	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_DL;
		break;
	default:
		return (-1);
	}

	return (0);
}

paddr_t
udl_mmap(void *v, off_t off, int prot)
{
	struct udl_softc *sc;

	sc = v;

	DPRINTF(1, "%s: %s\n", DN(sc), FUNC);

	return (-1);
}

int
udl_alloc_screen(void *v, const struct wsscreen_descr *type,
    void **cookiep, int *curxp, int *curyp, long *attrp)
{
	struct udl_softc *sc = v;
	struct wsdisplay_font *font;

	DPRINTF(1, "%s: %s\n", DN(sc), FUNC);

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	/*
	 * Initialize rasops.
	 */
	sc->sc_ri.ri_depth = sc->sc_depth;
	sc->sc_ri.ri_bits = NULL;
	sc->sc_ri.ri_width = sc->sc_width;
	sc->sc_ri.ri_height = sc->sc_height;
	sc->sc_ri.ri_stride = sc->sc_width * sc->sc_height / 8;
	sc->sc_ri.ri_hw = (void *)sc;
	sc->sc_ri.ri_flg = 0;

	/* swap B and R at 16 bpp */
	if (sc->sc_depth == 16) {
		sc->sc_ri.ri_rnum = 5;
		sc->sc_ri.ri_rpos = 11;
		sc->sc_ri.ri_gnum = 6;
		sc->sc_ri.ri_gpos = 5;
		sc->sc_ri.ri_bnum = 5;
		sc->sc_ri.ri_bpos = 0;
	}

	rasops_init(&sc->sc_ri, 100, 100);

	sc->sc_ri.ri_ops.copycols = udl_copycols;
	sc->sc_ri.ri_ops.copyrows = udl_copyrows;
	sc->sc_ri.ri_ops.erasecols = udl_erasecols;
	sc->sc_ri.ri_ops.eraserows = udl_eraserows;
	sc->sc_ri.ri_ops.putchar = udl_putchar;
	sc->sc_ri.ri_do_cursor = udl_do_cursor;

	sc->sc_ri.ri_ops.alloc_attr(&sc->sc_ri, 0, 0, 0, attrp);

	udl_stdscreen.nrows = sc->sc_ri.ri_rows;
	udl_stdscreen.ncols = sc->sc_ri.ri_cols;
	udl_stdscreen.textops = &sc->sc_ri.ri_ops;
	udl_stdscreen.fontwidth = sc->sc_ri.ri_font->fontwidth;
	udl_stdscreen.fontheight = sc->sc_ri.ri_font->fontheight;
	udl_stdscreen.capabilities = sc->sc_ri.ri_caps;

	*cookiep = &sc->sc_ri;
	*curxp = 0;
	*curyp = 0;

	sc->sc_nscreens++;

	font = sc->sc_ri.ri_font;
	DPRINTF(1, "%s: %s: using font %s (%dx%d)\n",
	    DN(sc), FUNC, font->name, sc->sc_ri.ri_cols, sc->sc_ri.ri_rows);

	return (0);
}

void
udl_free_screen(void *v, void *cookie)
{
	struct udl_softc *sc;

	sc = v;

	DPRINTF(1, "%s: %s\n", DN(sc), FUNC);
}

int
udl_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct udl_softc *sc;

	sc = v;

	DPRINTF(1, "%s: %s\n", DN(sc), FUNC);

	return (0);
}

void
udl_burner(void *v, u_int on, u_int flags)
{
	struct udl_softc *sc;

	sc = v;

	DPRINTF(1, "%s: %s: screen %s\n", DN(sc), FUNC, on ? "ON" : "OFF");

	if (on)
		udl_cmd_write_reg_1(sc, UDL_REG_SCREEN, UDL_REG_SCREEN_ON);
	else
		udl_cmd_write_reg_1(sc, UDL_REG_SCREEN, UDL_REG_SCREEN_OFF);

	udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);

	(void)udl_cmd_send_async(sc);
}

/* ---------- */

int
udl_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc;
	int sx, sy, dx, dy, cx, cy;

	sc = ri->ri_hw;

	DPRINTF(2, "%s: %s: row=%d, src=%d, dst=%d, num=%d\n",
	    DN(sc), FUNC, row, src, dst, num);

	sx = src * ri->ri_font->fontwidth;
	sy = row * ri->ri_font->fontheight;
	dx = dst * ri->ri_font->fontwidth;
	dy = row * ri->ri_font->fontheight;
	cx = num * ri->ri_font->fontwidth;
	cy = ri->ri_font->fontheight;

	(sc->udl_fb_block_copy)(sc, sx, sy, dx, dy, cx, cy);

	(void)udl_cmd_send_async(sc);

	return 0;
}

int
udl_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc;
	int sy, dy, cx, cy;

	sc = ri->ri_hw;

	DPRINTF(2, "%s: %s: src=%d, dst=%d, num=%d\n",
	    DN(sc), FUNC, src, dst, num);

	sy = src * sc->sc_ri.ri_font->fontheight;
	dy = dst * sc->sc_ri.ri_font->fontheight;
	cx = sc->sc_ri.ri_emuwidth;
	cy = num * sc->sc_ri.ri_font->fontheight;

	/* copy row block to off-screen first to fix overlay-copy problem */
	(sc->udl_fb_block_copy)(sc, 0, sy, 0, sc->sc_ri.ri_emuheight, cx, cy);

	/* copy row block back from off-screen now */
	(sc->udl_fb_block_copy)(sc, 0, sc->sc_ri.ri_emuheight, 0, dy, cx, cy);

	(void)udl_cmd_send_async(sc);

	return 0;
}

int
udl_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc = ri->ri_hw;
	uint16_t bgc;
	int fg, bg;
	int x, y, cx, cy;

	sc = ri->ri_hw;

	DPRINTF(2, "%s: %s: row=%d, col=%d, num=%d\n",
	    DN(sc), FUNC, row, col, num);

	sc->sc_ri.ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	bgc = (uint16_t)sc->sc_ri.ri_devcmap[bg];

	x = col * sc->sc_ri.ri_font->fontwidth;
	y = row * sc->sc_ri.ri_font->fontheight;
	cx = num * sc->sc_ri.ri_font->fontwidth;
	cy = sc->sc_ri.ri_font->fontheight;

	udl_fb_block_write(sc, bgc, x, y, cx, cy);

	(void)udl_cmd_send_async(sc);

	return 0;
}

int
udl_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc;
	uint16_t bgc;
	int fg, bg;
	int x, y, cx, cy;

	sc = ri->ri_hw;

	DPRINTF(2, "%s: %s: row=%d, num=%d\n", DN(sc), FUNC, row, num);

	sc->sc_ri.ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	bgc = (uint16_t)sc->sc_ri.ri_devcmap[bg];

	x = 0;
	y = row * sc->sc_ri.ri_font->fontheight;
	cx = sc->sc_ri.ri_emuwidth;
	cy = num * sc->sc_ri.ri_font->fontheight;

	udl_fb_block_write(sc, bgc, x, y, cx, cy);

	(void)udl_cmd_send_async(sc);

	return 0;
}

int
udl_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct udl_softc *sc = ri->ri_hw;
	uint16_t fgc, bgc;
	uint32_t x, y, fg, bg;

	DPRINTF(2, "%s: %s\n", DN(sc), FUNC);

	sc->sc_ri.ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);
	fgc = (uint16_t)sc->sc_ri.ri_devcmap[fg];
	bgc = (uint16_t)sc->sc_ri.ri_devcmap[bg];

	x = col * ri->ri_font->fontwidth;
	y = row * ri->ri_font->fontheight;

	if (uc == ' ') {
		/*
		 * Writting a block for the space character instead rendering
		 * it from font bits is more slim.
		 */
		(sc->udl_fb_block_write)(sc, bgc, x, y,
		    ri->ri_font->fontwidth, ri->ri_font->fontheight);
	} else {
		/* render a character from font bits */
		udl_draw_char(sc, fgc, bgc, uc, x, y);
	}

	/*
	 * We don't call udl_cmd_send_async() here, since sending each
	 * character by itself gets the performance down bad.  Instead the
	 * character will be buffered until another rasops function flush
	 * the buffer.
	 */

	return 0;
}

int
udl_do_cursor(struct rasops_info *ri)
{
	struct udl_softc *sc = ri->ri_hw;
	uint32_t x, y;

	/*
	 * XXX
	 * We can't draw a transparent cursor yet because the chip
	 * doesn't offer an XOR command nor a read command for screen
	 * regions.  Maybe this gets fixed once when wscons(4) is able
	 * to remember the on-screen characters.
	 */

	DPRINTF(2, "%s: %s: ccol=%d, crow=%d\n",
	    DN(sc), FUNC, ri->ri_ccol, ri->ri_crow);

	x = ri->ri_ccol * ri->ri_font->fontwidth;
	y = ri->ri_crow * ri->ri_font->fontheight;

	if (sc->sc_cursor_on == 0) {
		/* safe the last character block to off-screen */
		(sc->udl_fb_block_copy)(sc, x, y, 0, sc->sc_ri.ri_emuheight,
		    ri->ri_font->fontwidth, ri->ri_font->fontheight);

		/* draw cursor */
		(sc->udl_fb_block_write)(sc, 0xffff, x, y,
		    ri->ri_font->fontwidth, ri->ri_font->fontheight);

		sc->sc_cursor_on = 1;
	} else {
		/* restore the last safed character from off-screen */
		(sc->udl_fb_block_copy)(sc, 0, sc->sc_ri.ri_emuheight, x, y,
		    ri->ri_font->fontwidth, ri->ri_font->fontheight);

		sc->sc_cursor_on = 0;
	}

	(void)udl_cmd_send_async(sc);

	return 0;
}

/* ---------- */

usbd_status
udl_ctrl_msg(struct udl_softc *sc, uint8_t rt, uint8_t r,
    uint16_t index, uint16_t value, uint8_t *buf, size_t len)
{
	usb_device_request_t req;
	usbd_status error;

	req.bmRequestType = rt;
	req.bRequest = r;
	USETW(req.wIndex, index);
	USETW(req.wValue, value);
	USETW(req.wLength, len);

	error = usbd_do_request(sc->sc_udev, &req, buf);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(error));
		return (error);
	}

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
udl_poll(struct udl_softc *sc, uint32_t *buf)
{
	uint8_t lbuf[4];
	usbd_status error;

	error = udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE,
	    UDL_CTRL_CMD_POLL, 0x0000, 0x0000, lbuf, 4);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(error));
		return (error);
	}
	*buf = *(uint32_t *)lbuf;

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
udl_read_1(struct udl_softc *sc, uint16_t addr, uint8_t *buf)
{
	uint8_t lbuf[1];
	usbd_status error;

	error = udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE,
	    UDL_CTRL_CMD_READ_1, addr, 0x0000, lbuf, 1);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(error));
		return (error);
	}
	*buf = *(uint8_t *)lbuf;

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
udl_write_1(struct udl_softc *sc, uint16_t addr, uint8_t buf)
{
	usbd_status error;

	error = udl_ctrl_msg(sc, UT_WRITE_VENDOR_DEVICE,
	    UDL_CTRL_CMD_WRITE_1, addr, 0x0000, &buf, 1);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(error));
		return (error);
	}

	return (USBD_NORMAL_COMPLETION);
}

usbd_status
udl_read_edid(struct udl_softc *sc, uint8_t *buf)
{
	uint8_t lbuf[64];
	uint16_t offset;
	usbd_status error;

	offset = 0;

	error = udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE,
	    UDL_CTRL_CMD_READ_EDID, 0x00a1, (offset << 8), lbuf, 64);
	if (error != USBD_NORMAL_COMPLETION)
		goto fail;
	bcopy(lbuf + 1, buf + offset, 63);
	offset += 63;

	error = udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE,
	    UDL_CTRL_CMD_READ_EDID, 0x00a1, (offset << 8), lbuf, 64);
	if (error != USBD_NORMAL_COMPLETION)
		goto fail;
	bcopy(lbuf + 1, buf + offset, 63);
	offset += 63;

	error = udl_ctrl_msg(sc, UT_READ_VENDOR_DEVICE,
	    UDL_CTRL_CMD_READ_EDID, 0x00a1, (offset << 8), lbuf, 3);
	if (error != USBD_NORMAL_COMPLETION)
		goto fail;
	bcopy(lbuf + 1, buf + offset, 2);

	return (USBD_NORMAL_COMPLETION);
fail:
	printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(error));
	return (error);
}

usbd_status
udl_set_enc_key(struct udl_softc *sc, uint8_t *buf, uint8_t len)
{
	usbd_status error;

	error = udl_ctrl_msg(sc, UT_WRITE_VENDOR_DEVICE,
	    UDL_CTRL_CMD_SET_KEY, 0x0000, 0x0000, buf, len);
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(error));
		return (error);
	}
	
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
udl_set_decomp_table(struct udl_softc *sc, uint8_t *buf, uint16_t len)
{
	int err;

	udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
	udl_cmd_insert_int_1(sc, UDL_BULK_CMD_DECOMP);
	udl_cmd_insert_int_4(sc, 0x263871cd);	/* magic number */
	udl_cmd_insert_int_4(sc, 0x00000200);	/* 512 byte chunks */
	udl_cmd_insert_buf(sc, buf, len);

	err = udl_cmd_send(sc);
	if (err != 0)
		return (USBD_INVAL);

	return (USBD_NORMAL_COMPLETION);
}

/* ---------- */

int
udl_load_huffman(struct udl_softc *sc)
{
	const char *name = "udl_huffman";
	int error;

	if (sc->sc_huffman == NULL) {
		error = loadfirmware(name, &sc->sc_huffman,
		    &sc->sc_huffman_size);
		if (error != 0) {
			printf("%s: error %d, could not read huffman table "
			    "%s!\n", DN(sc), error, name);
			return (EIO);
		}
	}

	DPRINTF(1, "%s: huffman table %s allocated\n", DN(sc), name);

	return (0);
}

void
udl_free_huffman(struct udl_softc *sc)
{
	if (sc->sc_huffman != NULL) {
		free(sc->sc_huffman, M_DEVBUF);
		sc->sc_huffman = NULL;
		sc->sc_huffman_size = 0;
		DPRINTF(1, "%s: huffman table freed\n", DN(sc));
	}
}

usbd_status
udl_cmd_alloc_xfer(struct udl_softc *sc)
{
	int i;

	for (i = 0; i < UDL_CMD_XFER_COUNT; i++) {
		struct udl_cmd_xfer *cx = &sc->sc_cmd_xfer[i];

		cx->sc = sc;

		cx->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (cx->xfer == NULL) {
			printf("%s: %s: can't allocate xfer handle!\n",
			    DN(sc), FUNC);
			return (USBD_NOMEM);
		}

		cx->buf = usbd_alloc_buffer(cx->xfer, UDL_CMD_MAX_XFER_SIZE);
		if (cx->buf == NULL) {
			printf("%s: %s: can't allocate xfer buffer!\n",
			    DN(sc), FUNC);
			return (USBD_NOMEM);
		}
	}

	return (USBD_NORMAL_COMPLETION);
}

void
udl_cmd_free_xfer(struct udl_softc *sc)
{
	int i;

	for (i = 0; i < UDL_CMD_XFER_COUNT; i++) {
		struct udl_cmd_xfer *cx = &sc->sc_cmd_xfer[i];

		if (cx->xfer != NULL) {
			usbd_free_xfer(cx->xfer);
			cx->xfer = NULL;
		}
	}
}

int
udl_cmd_alloc_buf(struct udl_softc *sc)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;

	cb->buf = malloc(UDL_CMD_MAX_XFER_SIZE, M_DEVBUF, 0);
	if (cb->buf == NULL) {
		printf("%s: %s: can't allocate buffer!\n",
		    DN(sc), FUNC);
		return (ENOMEM);
	}
	cb->off = 0;
	cb->compblock = 0;

	return (0);
}

void
udl_cmd_free_buf(struct udl_softc *sc)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;

	if (cb->buf != NULL) {
		free(cb->buf, M_DEVBUF);
		cb->buf = NULL;
	}
	cb->off = 0;
}

void
udl_cmd_insert_int_1(struct udl_softc *sc, uint8_t value)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;

	udl_cmd_insert_check(cb, 1);

	cb->buf[cb->off] = value;

	cb->off += 1;
}

void
udl_cmd_insert_int_2(struct udl_softc *sc, uint16_t value)
{
	uint16_t lvalue;
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;

	udl_cmd_insert_check(cb, 2);

	lvalue = htobe16(value);
	bcopy(&lvalue, cb->buf + cb->off, 2);

	cb->off += 2;
}

void
udl_cmd_insert_int_3(struct udl_softc *sc, uint32_t value)
{
	uint32_t lvalue;
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;

	udl_cmd_insert_check(cb, 3);
#if BYTE_ORDER == BIG_ENDIAN
	lvalue = htobe32(value) << 8;
#else
	lvalue = htobe32(value) >> 8;
#endif
	bcopy(&lvalue, cb->buf + cb->off, 3);

	cb->off += 3;
}

void
udl_cmd_insert_int_4(struct udl_softc *sc, uint32_t value)
{
	uint32_t lvalue;
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;

	udl_cmd_insert_check(cb, 4);

	lvalue = htobe32(value);
	bcopy(&lvalue, cb->buf + cb->off, 4);

	cb->off += 4;
}

void
udl_cmd_insert_buf(struct udl_softc *sc, uint8_t *buf, uint32_t len)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;

	udl_cmd_insert_check(cb, len);

	bcopy(buf, cb->buf + cb->off, len);

	cb->off += len;
}

int
udl_cmd_insert_buf_comp(struct udl_softc *sc, uint8_t *buf, uint32_t len)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;
	struct udl_huffman *h;
	uint8_t bit_pos;
	uint16_t *pixels, prev;
	int16_t diff;
	uint32_t bit_count, bit_pattern, bit_cur;
	int i, j, bytes, eob, padding, next;

	udl_cmd_insert_check(cb, len);

	pixels = (uint16_t *)buf;
	bit_pos = bytes = eob = padding = 0;

	/*
	 * If the header doesn't fit into the 512 byte main-block anymore,
	 * skip the header and finish up the main-block.  We return zero
	 * to signal our caller that the header has been skipped.
	 */
	if (cb->compblock > UDL_CB_RESTART_SIZE) {
		cb->off -= UDL_CMD_WRITE_HEAD_SIZE;
		cb->compblock -= UDL_CMD_WRITE_HEAD_SIZE;
		eob = 1;
	}

	/*
	 * Generate a sub-block with maximal 256 pixels compressed data.
	 */
	for (i = 0; i < len / 2 && eob == 0; i++) {
		/* get difference between current and previous pixel */
		if (i > 0)
			prev = betoh16(pixels[i - 1]);
		else
			prev = 0;

		/* get the huffman difference bit sequence */
		diff = betoh16(pixels[i]) - prev;
		h = (struct udl_huffman *)(sc->sc_huffman + UDL_HUFFMAN_BASE);
		h += diff;
		bit_count = h->bit_count;
		bit_pattern = betoh32(h->bit_pattern);


		/* we are near the end of the main-block, so quit loop */
		if (bit_count % 8 == 0)
			next = bit_count / 8;
		else
			next = (bit_count / 8) + 1;

		if (cb->compblock + next >= UDL_CB_BODY_SIZE) {
			eob = 1;
			break;
		}

		/* generate one pixel compressed data */
		for (j = 0; j < bit_count; j++) {
			if (bit_pos == 0)
				cb->buf[cb->off] = 0;
			bit_cur = (bit_pattern >> j) & 1;
			cb->buf[cb->off] |= (bit_cur << bit_pos);
			bit_pos++;

			if (bit_pos == 8) {
				bit_pos = 0;
				cb->off++;
				cb->compblock++;
			}
		}
		bytes += 2;
	}

	/*
	 * If we have bits left in our last byte, round up to the next
	 * byte, so we don't overwrite them.
	 */
	if (bit_pos != 0) {
		cb->off++;
		cb->compblock++;
	}

	/*
	 * Finish up a 512 byte main-block.  The leftover space gets
	 * padded to zero.  Finally terminate the block by writting the
	 * 0xff-into-UDL_REG_SYNC-register sequence.
	 */
	if (eob == 1) {
		padding = (UDL_CB_BODY_SIZE - cb->compblock);
		for (i = 0; i < padding; i++) {
			cb->buf[cb->off] = 0;
			cb->off++;
			cb->compblock++;
		}
		udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);
		cb->compblock = 0;
	}

	/* return how many bytes we have compressed */
	return (bytes);
}

int
udl_cmd_insert_head_comp(struct udl_softc *sc, uint32_t len)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;
	int i, padding;

	udl_cmd_insert_check(cb, len);

	if (cb->compblock > UDL_CB_BODY_SIZE) {
		cb->off -= UDL_CMD_COPY_HEAD_SIZE;
		cb->compblock -= UDL_CMD_COPY_HEAD_SIZE;

		padding = (UDL_CB_BODY_SIZE - cb->compblock);
		for (i = 0; i < padding; i++) {
			cb->buf[cb->off] = 0;
			cb->off++;
			cb->compblock++;
		}
		udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);
		cb->compblock = 0;
		return (0);
	}

	return (len);
}

void
udl_cmd_insert_check(struct udl_cmd_buf *cb, int len)
{
	int total;

	total = cb->off + len;

	if (total >=  UDL_CMD_MAX_XFER_SIZE) {
		/* XXX */
		panic("udl_cmd_insert_check: command buffer is full");
	}
}

void
udl_cmd_write_reg_1(struct udl_softc *sc, uint8_t reg, uint8_t val)
{
	udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
	udl_cmd_insert_int_1(sc, UDL_BULK_CMD_REG_WRITE_1);
	udl_cmd_insert_int_1(sc, reg);
	udl_cmd_insert_int_1(sc, val);
}

void
udl_cmd_write_reg_3(struct udl_softc *sc, uint8_t reg, uint32_t val)
{
	udl_cmd_write_reg_1(sc, reg + 0, (val >> 16) & 0xff);
	udl_cmd_write_reg_1(sc, reg + 1, (val >> 8) & 0xff);
	udl_cmd_write_reg_1(sc, reg + 2, (val >> 0) & 0xff);
}

usbd_status
udl_cmd_send(struct udl_softc *sc)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;
	struct udl_cmd_xfer *cx = &sc->sc_cmd_xfer[0];
	int len;
	usbd_status error;

	/* mark end of command stack */
	udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
	udl_cmd_insert_int_1(sc, UDL_BULK_CMD_EOC);

	bcopy(cb->buf, cx->buf, cb->off);

	len = cb->off;
	error = usbd_bulk_transfer(cx->xfer, sc->sc_tx_pipeh,
	    USBD_NO_COPY | USBD_SHORT_XFER_OK, 1000, cx->buf, &len,
	    "udl_bulk_xmit");
	if (error != USBD_NORMAL_COMPLETION) {
		printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(error));
		/* we clear our buffer now to avoid growing out of bounds */
		goto fail;
	}
	DPRINTF(1, "%s: %s: sent %d of %d bytes\n",
	    DN(sc), FUNC, len, cb->off);
fail:
	cb->off = 0;
	cb->compblock = 0;

	return (error);
}

usbd_status
udl_cmd_send_async(struct udl_softc *sc)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;
	struct udl_cmd_xfer *cx;
	usbd_status error;
	int i, s;

	/* if the ring buffer is full, wait until it's flushed completely */
	if (sc->sc_cmd_xfer_cnt == UDL_CMD_XFER_COUNT) {
		DPRINTF(2, "%s: %s: ring buffer full, wait until flushed\n",
		    DN(sc), FUNC);
		/*
		 * XXX
		 * Yes, this is ugly.  But since we can't tsleep() here, I
		 * have no better idea how we can delay rasops so it doesn't
		 * blow up our command buffer.
		 */
		while (sc->sc_cmd_xfer_cnt > 0)
			delay(100);
	}

	s = splusb();	/* no callbacks please until accounting is done */

	/* find a free ring buffer */
	for (i = 0; i < UDL_CMD_XFER_COUNT; i++) {
		if (sc->sc_cmd_xfer[i].busy == 0)
			break;
	}
	if (i == UDL_CMD_XFER_COUNT) {
		/* XXX this shouldn't happen */
		panic("udl_cmd_send_async: buffer full");
	}
	cx = &sc->sc_cmd_xfer[i];

	/* mark end of command stack */
	udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
	udl_cmd_insert_int_1(sc, UDL_BULK_CMD_EOC);

	/* copy command buffer to xfer buffer */
	bcopy(cb->buf, cx->buf, cb->off);

	/* do xfer */
	usbd_setup_xfer(cx->xfer, sc->sc_tx_pipeh, cx, cx->buf, cb->off,
	     USBD_NO_COPY, 1000, udl_cmd_send_async_cb);
	error = usbd_transfer(cx->xfer);
	if (error != 0 && error != USBD_IN_PROGRESS) {
		printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(error));
		return (error);
	}
	DPRINTF(2, "%s: %s: sending %d bytes from buffer no. %d\n",
	    DN(sc), FUNC, cb->off, i);

	/* free command buffer, lock xfer buffer */
	cb->off = 0;
	cb->compblock = 0;
	cx->busy = 1;
	sc->sc_cmd_xfer_cnt++;

	splx(s);

	return (USBD_NORMAL_COMPLETION);
}

void
udl_cmd_send_async_cb(usbd_xfer_handle xfer, usbd_private_handle priv,
    usbd_status status)
{
	struct udl_cmd_xfer *cx = priv;
	struct udl_softc *sc = cx->sc;
	int len;

	if (status != USBD_NORMAL_COMPLETION) {
		printf("%s: %s: %s!\n", DN(sc), FUNC, usbd_errstr(status));

		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_tx_pipeh);
		goto skip;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	DPRINTF(2, "%s: %s: sent %d bytes\n", DN(sc), FUNC, len);
skip:
	/* free xfer buffer */
	cx->busy = 0;
	sc->sc_cmd_xfer_cnt--;
}

/* ---------- */

usbd_status
udl_init_chip(struct udl_softc *sc)
{
	uint8_t ui8;
	uint32_t ui32;
	int8_t edid[128];
	usbd_status error;

	error = udl_poll(sc, &ui32);
	if (error != USBD_NORMAL_COMPLETION)
		return (error);
	DPRINTF(1, "%s: %s: poll=0x%08x\n", DN(sc), FUNC, ui32);

	error = udl_read_1(sc, 0xc484, &ui8);
	if (error != USBD_NORMAL_COMPLETION)
		return (error);
	DPRINTF(1, "%s: %s: read 0x%02x from 0xc484\n", DN(sc), FUNC, ui8);

	error = udl_write_1(sc, 0xc41f, 0x01);
	if (error != USBD_NORMAL_COMPLETION)
		return (error);
	DPRINTF(1, "%s: %s: write 0x01 to 0xc41f\n", DN(sc), FUNC);

	error = udl_read_edid(sc, edid);
	if (error != USBD_NORMAL_COMPLETION)
		return (error);
	DPRINTF(1, "%s: %s: read EDID=\n", DN(sc), FUNC);
#ifdef UDL_DEBUG
	udl_hexdump(edid, sizeof(edid), 0);
#endif
	error = udl_set_enc_key(sc, udl_null_key_1, sizeof(udl_null_key_1));
	if (error != USBD_NORMAL_COMPLETION)
		return (error);
	DPRINTF(1, "%s: %s: set encryption key\n", DN(sc), FUNC);

	error = udl_write_1(sc, 0xc40b, 0x00);
	if (error != USBD_NORMAL_COMPLETION)
		return (error);
	DPRINTF(1, "%s: %s: write 0x00 to 0xc40b\n", DN(sc), FUNC, ui8);

	error = udl_set_decomp_table(sc, udl_decomp_table,
	    sizeof(udl_decomp_table));
	if (error != USBD_NORMAL_COMPLETION)
		return (error);
	DPRINTF(1, "%s: %s: set decompression table\n", DN(sc), FUNC);

	return (USBD_NORMAL_COMPLETION);
}

void
udl_init_fb_offsets(struct udl_softc *sc, uint32_t start16, uint32_t stride16,
    uint32_t start8, uint32_t stride8)
{
	udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0x00);
	udl_cmd_write_reg_3(sc, UDL_REG_ADDR_START16, start16);
	udl_cmd_write_reg_3(sc, UDL_REG_ADDR_STRIDE16, stride16);
	udl_cmd_write_reg_3(sc, UDL_REG_ADDR_START8, start8);
	udl_cmd_write_reg_3(sc, UDL_REG_ADDR_STRIDE8, stride8);
	udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);
}

usbd_status
udl_init_resolution(struct udl_softc *sc, uint8_t *buf, uint8_t len)
{
	int i;
	usbd_status error;

	/* write resolution values and set video memory offsets */
	udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0x00);
	for (i = 0; i < len; i++)
		udl_cmd_write_reg_1(sc, i, buf[i]);
	udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);

	udl_init_fb_offsets(sc, 0x000000, 0x000a00, 0x555555, 0x000500);
	error = udl_cmd_send(sc);
	if (error != USBD_NORMAL_COMPLETION)
		return (error);

	/* clear screen */
	udl_fb_block_write(sc, 0x0000, 0, 0, sc->sc_width, sc->sc_height);
	error = udl_cmd_send(sc);
	if (error != USBD_NORMAL_COMPLETION)
		return (error);

	/* show framebuffer content */
	udl_cmd_write_reg_1(sc, UDL_REG_SCREEN, UDL_REG_SCREEN_ON);
	udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);
	error = udl_cmd_send(sc);
	if (error != USBD_NORMAL_COMPLETION)
		return (error);

	return (USBD_NORMAL_COMPLETION);
}

void
udl_fb_off_write(struct udl_softc *sc, uint16_t rgb16, uint32_t off,
    uint16_t width)
{
	uint8_t buf[UDL_CMD_MAX_DATA_SIZE];
	uint16_t lwidth, lrgb16;
	uint32_t loff;
	int i;

	loff = off * 2;
	lwidth = width * 2;

	udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
	udl_cmd_insert_int_1(sc, UDL_BULK_CMD_FB_WRITE | UDL_BULK_CMD_FB_WORD);
	udl_cmd_insert_int_3(sc, loff);
	udl_cmd_insert_int_1(sc, width >= UDL_CMD_MAX_PIXEL_COUNT ? 0 : width);

	for (i = 0; i < lwidth; i += 2) {
		lrgb16 = htobe16(rgb16);
		bcopy(&lrgb16, buf + i, 2);
	}

	udl_cmd_insert_buf(sc, buf, lwidth);
}

void
udl_fb_line_write(struct udl_softc *sc, uint16_t rgb16, uint32_t x,
    uint32_t y, uint32_t width)
{
	uint32_t off, block;

	off = (y * sc->sc_width) + x;

	while (width) {
		if (width > UDL_CMD_MAX_PIXEL_COUNT)	
			block = UDL_CMD_MAX_PIXEL_COUNT;
		else
			block = width;

		udl_fb_off_write(sc, rgb16, off, block);

		off += block;
		width -= block;
	}
}

void
udl_fb_block_write(struct udl_softc *sc, uint16_t rgb16, uint32_t x,
    uint32_t y, uint32_t width, uint32_t height)
{
	uint32_t i;

	for (i = 0; i < height; i++)
		udl_fb_line_write(sc, rgb16, x, y + i, width);
}

void
udl_fb_buf_write(struct udl_softc *sc, uint8_t *buf, uint32_t x,
    uint32_t y, uint16_t width)
{
	uint16_t lwidth;
	uint32_t off;

	off = ((y * sc->sc_width) + x) * 2;
	lwidth = width * 2;

	udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
	udl_cmd_insert_int_1(sc, UDL_BULK_CMD_FB_WRITE | UDL_BULK_CMD_FB_WORD);
	udl_cmd_insert_int_3(sc, off);
	udl_cmd_insert_int_1(sc, width >= UDL_CMD_MAX_PIXEL_COUNT ? 0 : width);

	udl_cmd_insert_buf(sc, buf, lwidth);
}

void
udl_fb_off_copy(struct udl_softc *sc, uint32_t src_off, uint32_t dst_off,
    uint16_t width)
{
	uint32_t ldst_off, lsrc_off;

	ldst_off = dst_off * 2;
	lsrc_off = src_off * 2;

	udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
	udl_cmd_insert_int_1(sc, UDL_BULK_CMD_FB_COPY | UDL_BULK_CMD_FB_WORD);
	udl_cmd_insert_int_3(sc, ldst_off);
	udl_cmd_insert_int_1(sc, width >= UDL_CMD_MAX_PIXEL_COUNT ? 0 : width);
	udl_cmd_insert_int_3(sc, lsrc_off);
}

void
udl_fb_line_copy(struct udl_softc *sc, uint32_t src_x, uint32_t src_y,
    uint32_t dst_x, uint32_t dst_y, uint32_t width)
{
	uint32_t src_off, dst_off, block;

	src_off = (src_y * sc->sc_width) + src_x;
	dst_off = (dst_y * sc->sc_width) + dst_x;

	while (width) {
		if (width > UDL_CMD_MAX_PIXEL_COUNT)
			block = UDL_CMD_MAX_PIXEL_COUNT;
		else
			block = width;

		udl_fb_off_copy(sc, src_off, dst_off, block);

		src_off += block;
		dst_off += block;
		width -= block;
	}
}

void
udl_fb_block_copy(struct udl_softc *sc, uint32_t src_x, uint32_t src_y,
    uint32_t dst_x, uint32_t dst_y, uint32_t width, uint32_t height)
{
	int i;

	for (i = 0; i < height; i++)
		udl_fb_line_copy(sc, src_x, src_y + i, dst_x, dst_y + i, width);
}

void
udl_fb_off_write_comp(struct udl_softc *sc, uint16_t rgb16, uint32_t off,
    uint16_t width)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;
	uint8_t buf[UDL_CMD_MAX_DATA_SIZE];
	uint8_t *count;
	uint16_t lwidth, lrgb16;
	uint32_t loff;
	int i, r, sent;

	loff = off * 2;
	lwidth = width * 2;

	for (i = 0; i < lwidth; i += 2) {
		lrgb16 = htobe16(rgb16);
		bcopy(&lrgb16, buf + i, 2);
	}

	/*
	 * A new compressed stream needs the 0xff-into-UDL_REG_SYNC-register
	 * sequence always as first command.
	 */
	if (cb->off == 0)
		udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);

	r = sent = 0;
	while (sent < lwidth) {
		udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
		udl_cmd_insert_int_1(sc,
		    UDL_BULK_CMD_FB_WRITE |
		    UDL_BULK_CMD_FB_WORD |
		    UDL_BULK_CMD_FB_COMP);
		udl_cmd_insert_int_3(sc, loff + sent);
		udl_cmd_insert_int_1(sc,
		    width >= UDL_CMD_MAX_PIXEL_COUNT ? 0 : width);
		cb->compblock += UDL_CMD_WRITE_HEAD_SIZE;

		count = &cb->buf[cb->off - 1];
		r = udl_cmd_insert_buf_comp(sc, buf + sent, lwidth - sent);
		if (r > 0 && r != (lwidth - sent)) {
			*count = r / 2;
			width -= r / 2;
		}
		sent += r;
	}
}

void
udl_fb_line_write_comp(struct udl_softc *sc, uint16_t rgb16, uint32_t x,
    uint32_t y, uint32_t width)
{
	uint32_t off, block;

	off = (y * sc->sc_width) + x;

	while (width) {
		if (width > UDL_CMD_MAX_PIXEL_COUNT)	
			block = UDL_CMD_MAX_PIXEL_COUNT;
		else
			block = width;

		(sc->udl_fb_off_write)(sc, rgb16, off, block);

		off += block;
		width -= block;
	}
}

void
udl_fb_block_write_comp(struct udl_softc *sc, uint16_t rgb16, uint32_t x,
    uint32_t y, uint32_t width, uint32_t height)
{
	uint32_t i;

	for (i = 0; i < height; i++)
		udl_fb_line_write_comp(sc, rgb16, x, y + i, width);
}

void
udl_fb_buf_write_comp(struct udl_softc *sc, uint8_t *buf, uint32_t x,
    uint32_t y, uint16_t width)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;
	uint8_t *count;
	uint16_t lwidth;
	uint32_t off;
	int r, sent;

	off = ((y * sc->sc_width) + x) * 2;
	lwidth = width * 2;

	/*
	 * A new compressed stream needs the 0xff-into-UDL_REG_SYNC-register
	 * sequence always as first command.
	 */
	if (cb->off == 0)
		udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);

	r = sent = 0;
	while (sent < lwidth) {
		udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
		udl_cmd_insert_int_1(sc,
		    UDL_BULK_CMD_FB_WRITE |
		    UDL_BULK_CMD_FB_WORD |
		    UDL_BULK_CMD_FB_COMP);
		udl_cmd_insert_int_3(sc, off + sent);
		udl_cmd_insert_int_1(sc,
		    width >= UDL_CMD_MAX_PIXEL_COUNT ? 0 : width);
		cb->compblock += UDL_CMD_WRITE_HEAD_SIZE;

		count = &cb->buf[cb->off - 1];
		r = udl_cmd_insert_buf_comp(sc, buf + sent, lwidth - sent);
		if (r > 0 && r != (lwidth - sent)) {
			*count = r / 2;
			width -= r / 2;
		}
		sent += r;
	}
}

void
udl_fb_off_copy_comp(struct udl_softc *sc, uint32_t src_off, uint32_t dst_off,
    uint16_t width)
{
	struct udl_cmd_buf *cb = &sc->sc_cmd_buf;
	uint32_t ldst_off, lsrc_off;
	int r;

	ldst_off = dst_off * 2;
	lsrc_off = src_off * 2;

	/*
	 * A new compressed stream needs the 0xff-into-UDL_REG_SYNC-register
	 * sequence always as first command.
	 */
	if (cb->off == 0)
		udl_cmd_write_reg_1(sc, UDL_REG_SYNC, 0xff);

	r = 0;
	while (r < 1) {
		udl_cmd_insert_int_1(sc, UDL_BULK_SOC);
		udl_cmd_insert_int_1(sc,
		    UDL_BULK_CMD_FB_COPY | UDL_BULK_CMD_FB_WORD);
		udl_cmd_insert_int_3(sc, ldst_off);
		udl_cmd_insert_int_1(sc,
		    width >= UDL_CMD_MAX_PIXEL_COUNT ? 0 : width);
		udl_cmd_insert_int_3(sc, lsrc_off);
		cb->compblock += UDL_CMD_COPY_HEAD_SIZE;

		r = udl_cmd_insert_head_comp(sc, UDL_CMD_COPY_HEAD_SIZE);
	}
}

void
udl_fb_line_copy_comp(struct udl_softc *sc, uint32_t src_x, uint32_t src_y,
    uint32_t dst_x, uint32_t dst_y, uint32_t width)
{
	uint32_t src_off, dst_off, block;

	src_off = (src_y * sc->sc_width) + src_x;
	dst_off = (dst_y * sc->sc_width) + dst_x;

	while (width) {
		if (width > UDL_CMD_MAX_PIXEL_COUNT)
			block = UDL_CMD_MAX_PIXEL_COUNT;
		else
			block = width;

		udl_fb_off_copy_comp(sc, src_off, dst_off, block);

		src_off += block;
		dst_off += block;
		width -= block;
	}
}

void
udl_fb_block_copy_comp(struct udl_softc *sc, uint32_t src_x, uint32_t src_y,
    uint32_t dst_x, uint32_t dst_y, uint32_t width, uint32_t height)
{
	int i;

	for (i = 0; i < height; i++)
		udl_fb_line_copy_comp(sc, src_x, src_y + i, dst_x, dst_y + i,
		    width);
}

void
udl_draw_char(struct udl_softc *sc, uint16_t fg, uint16_t bg, u_int uc,
    uint32_t x, uint32_t y)
{
	int i, j, ly;
	uint8_t *fontchar, fontbits, luc;
	uint8_t buf[UDL_CMD_MAX_DATA_SIZE];
	uint16_t *line, lrgb16;
	struct wsdisplay_font *font = sc->sc_ri.ri_font;

	fontchar = (uint8_t *)(font->data + (uc - font->firstchar) *
	    sc->sc_ri.ri_fontscale);

	ly = y;
	for (i = 0; i < font->fontheight; i++) {
		fontbits = *fontchar;
		line = (uint16_t *)buf;

		for (j = (font->fontwidth - 1); j != -1; j--) {
			luc = 1 << j;
			if (fontbits & luc)
				lrgb16 = htobe16(fg);
			else
				lrgb16 = htobe16(bg);
			bcopy(&lrgb16, line, 2);
			line++;
		}
		(sc->udl_fb_buf_write)(sc, buf, x, ly, font->fontwidth);
		ly++;

		fontchar += font->stride;
	}
}

/* ---------- */
#ifdef UDL_DEBUG
void
udl_hexdump(void *buf, int len, int quiet)
{
	int i;

	for (i = 0; i < len; i++) {
		if (quiet == 0) {
			if (i % 16 == 0)
				printf("%s%5i:", i ? "\n" : "", i);
			if (i % 4 == 0)
				printf(" ");
		}
		printf("%02x", (int)*((u_char *)buf + i));
	}
	printf("\n");
}

usbd_status
udl_init_test(struct udl_softc *sc)
{
	int i, j, parts, loops;
	uint16_t color;
	uint16_t rgb24[3] = { 0xf800, 0x07e0, 0x001f };

	loops = (sc->sc_width * sc->sc_height) / UDL_CMD_MAX_PIXEL_COUNT;
	parts = loops / 3;
	color = rgb24[0];

	j = 1;
	for (i = 0; i < loops; i++) {
		if (i == parts) {
			color = rgb24[j];
			parts += parts;
			j++;
		}
		(sc->udl_fb_off_write)(sc, color, i * UDL_CMD_MAX_PIXEL_COUNT,
		    UDL_CMD_MAX_PIXEL_COUNT);
	}
	(void)udl_cmd_send(sc);

	return (USBD_NORMAL_COMPLETION);
}
#endif
