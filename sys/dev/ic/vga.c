/*	$OpenBSD: vga.c,v 1.12 1998/09/28 01:07:34 rahnds Exp $	*/
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
#include <machine/bus.h>

#include <dev/wscons/wsconsvar.h>
#include <dev/ic/vgavar.h>

#define	VGA_IO_D_6845_ADDR	0x4
#define	VGA_IO_D_6845_DATA	0x5

struct cfdriver vga_cd = {
	NULL, "vga", DV_DULL,
};

void	vga_cursor __P((void *, int, int, int));
void	vga_putstr __P((void *, int, int, char *, int));
void	vga_copycols __P((void *, int, int, int, int));
void	vga_erasecols __P((void *, int, int, int));
void	vga_copyrows __P((void *, int, int, int));
void	vga_eraserows __P((void *, int, int));
void	vga_set_attr __P((void *, int));

struct wscons_emulfuncs vga_emulfuncs = {
	vga_cursor,
	vga_putstr,
	vga_copycols,
	vga_erasecols,
	vga_copyrows,
	vga_eraserows,
	vga_set_attr,
};

int	vgaprint __P((void *, const char *));

/*
 * The following functions implement back-end configuration grabbing
 * and attachment.
 */
int
vga_common_probe(iot, memt)
	bus_space_tag_t iot, memt;
{
	bus_space_handle_t ioh_b, ioh_c, ioh_d, memh;
	u_int16_t vgadata;
	int gotio_b, gotio_c, gotio_d, gotmem, rv;

	gotio_b = gotio_c = gotio_d = gotmem = rv = 0;

	if (bus_space_map(iot, 0x3b0, 0xc, 0, &ioh_b))
		goto bad;
	gotio_b = 1;
	if (bus_space_map(iot, 0x3c0, 0x10, 0, &ioh_c))
		goto bad;
	gotio_c = 1;
	if (bus_space_map(iot, 0x3d0, 0x10, 0, &ioh_d))
		goto bad;
	gotio_d = 1;
	if (bus_space_map(memt, 0xb8000, 0x8000, 0, &memh))
		goto bad;
	gotmem = 1;

	vgadata = bus_space_read_2(memt, memh, 0);
	bus_space_write_2(memt, memh, 0, 0xa55a);
	rv = (bus_space_read_2(memt, memh, 0) == 0xa55a);
	bus_space_write_2(memt, memh, 0, vgadata);

bad:
	if (gotio_b)
		bus_space_unmap(iot, ioh_b, 0xc);
	if (gotio_c)
		bus_space_unmap(iot, ioh_c, 0x10);
	if (gotio_d)
		bus_space_unmap(iot, ioh_d, 0x10);
	if (gotmem)
		bus_space_unmap(memt, memh, 0x8000);

	return (rv);
}

void
vga_common_setup(iot, memt, vc)
	bus_space_tag_t iot, memt;
	struct vga_config *vc;
{
	int cpos;

        vc->vc_iot = iot;
        vc->vc_memt = memt;

        if (bus_space_map(vc->vc_iot, 0x3b0, 0xc, 0, &vc->vc_ioh_b))
                panic("vga_common_setup: couldn't map io b");
        if (bus_space_map(vc->vc_iot, 0x3c0, 0x10, 0, &vc->vc_ioh_c))
                panic("vga_common_setup: couldn't map io c");
        if (bus_space_map(vc->vc_iot, 0x3d0, 0x10, 0, &vc->vc_ioh_d))
                panic("vga_common_setup: couldn't map io d");
        if (bus_space_map(vc->vc_memt, 0xb8000, 0x8000, 0, &vc->vc_memh))
                panic("vga_common_setup: couldn't map memory"); 

	vc->vc_nrow = 25;
	vc->vc_ncol = 80;

	bus_space_write_1(iot, vc->vc_ioh_d, VGA_IO_D_6845_ADDR, 14); 
	cpos = bus_space_read_1(iot, vc->vc_ioh_d, VGA_IO_D_6845_DATA) << 8;
	bus_space_write_1(iot, vc->vc_ioh_d, VGA_IO_D_6845_ADDR, 15);
	cpos |= bus_space_read_1(iot, vc->vc_ioh_d, VGA_IO_D_6845_DATA);
	vc->vc_crow = cpos / vc->vc_ncol;
	vc->vc_ccol = cpos % vc->vc_ncol;

	vc->vc_so = 0;
#if 0
	vc->vc_at = 0x00 | 0xf;			/* black bg|white fg */
	vc->vc_so_at = 0x00 | 0xf | 0x80;	/* black bg|white fg|blink */

	/* clear screen, frob cursor, etc.? */
	pcivga_eraserows(vc, 0, vc->vc_nrow);
#endif
	/*
	 * XXX DEC HAS SWITCHED THE CODES FOR BLUE AND RED!!!
	 * XXX Therefore, though the comments say "blue bg", the code uses
	 * XXX the value for a red background!
	 */
	vc->vc_at = 0x40 | 0x0f;		/* blue bg|white fg */
	vc->vc_so_at = 0x40 | 0x0f | 0x80;	/* blue bg|white fg|blink */
}

void
vga_wscons_attach(parent, vc, console)
	struct device *parent;
	struct vga_config *vc;
	int console;
{
	struct wscons_attach_args waa;
	struct wscons_odev_spec *wo;

        waa.waa_isconsole = console;
        wo = &waa.waa_odev_spec;

        wo->wo_emulfuncs = &vga_emulfuncs;
	wo->wo_emulfuncs_cookie = vc;

        wo->wo_ioctl = vc->vc_ioctl;
        wo->wo_mmap = vc->vc_mmap;
        wo->wo_miscfuncs_cookie = vc;

        wo->wo_nrows = vc->vc_nrow;
        wo->wo_ncols = vc->vc_ncol;
        wo->wo_crow = vc->vc_crow;
        wo->wo_ccol = vc->vc_ccol;
 
        config_found(parent, &waa, vgaprint);
}

void
vga_wscons_console(vc)
	struct vga_config *vc;
{
	struct wscons_odev_spec wo;

        wo.wo_emulfuncs = &vga_emulfuncs;
	wo.wo_emulfuncs_cookie = vc;

	/* ioctl and mmap are unused until real attachment. */

        wo.wo_nrows = vc->vc_nrow;
        wo.wo_ncols = vc->vc_ncol;
        wo.wo_crow = vc->vc_crow;
        wo.wo_ccol = vc->vc_ccol;
 
        wscons_attach_console(&wo);
}

int
vgaprint(aux, pnp)
	void *aux;
	const char *pnp;
{

	if (pnp)
		printf("wscons at %s", pnp);
	return (UNCONF);
}

int
vgaioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	/*struct vga_config *vc = v;*/

	/* XXX */
	return -1;
}

int
vgammap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct vga_config *vc = v;
	bus_space_handle_t h;
	u_int32_t *port;

	if (offset >= 0x00000 && offset < 0x100000)	/* 1MB of mem */
		h = vc->vc_memh + offset;
	else if (offset >= 0x10000 && offset < 0x140000) /* 256KB of iohb */
		h = vc->vc_ioh_b;
	else if (offset >= 0x140000 && offset < 0x180000) /* 256KB of iohc */
		h = vc->vc_ioh_c;
	else if (offset >= 0x180000 && offset < 0x1c0000) /* 256KB of iohd */
		h = vc->vc_ioh_d;
	else
		return (-1);

	port = (u_int32_t *)(h << 5);
#ifdef alpha
	return alpha_btop(port);		/* XXX */
#elif defined(i386)
	return i386_btop(port);
#elif defined(powerpc)
	return powerpc_btop(port);
#endif
}

/*
 * The following functions implement the MI ANSI terminal emulation on
 * a VGA display.
 */
void
vga_cursor(id, on, row, col)
	void *id;
	int on, row, col;
{
	struct vga_config *vc = id;
	bus_space_tag_t iot = vc->vc_iot;
	bus_space_handle_t ioh_d = vc->vc_ioh_d;
	int pos;

#if 0
	printf("vga_cursor: %d %d\n", row, col);
#endif
	/* turn the cursor off */
	if (!on) {
		/* XXX disable cursor how??? */
		vc->vc_crow = vc->vc_ccol = -1;
	} else {
		vc->vc_crow = row;
		vc->vc_ccol = col;
	}

	pos = row * vc->vc_ncol + col;

	bus_space_write_1(iot, ioh_d, VGA_IO_D_6845_ADDR, 14);
	bus_space_write_1(iot, ioh_d, VGA_IO_D_6845_DATA, pos >> 8);
	bus_space_write_1(iot, ioh_d, VGA_IO_D_6845_ADDR, 15);
	bus_space_write_1(iot, ioh_d, VGA_IO_D_6845_DATA, pos);
}

void
vga_putstr(id, row, col, cp, len)
	void *id;
	int row, col;
	char *cp;
	int len;
{
	struct vga_config *vc = id;
	bus_space_tag_t memt = vc->vc_memt;
	bus_space_handle_t memh = vc->vc_memh;
	int i, off;

	off = (row * vc->vc_ncol + col) * 2;
	for (i = 0; i < len; i++, cp++, off += 2) {
		bus_space_write_1(memt, memh, off, *cp);
		bus_space_write_1(memt, memh, off + 1,
		    vc->vc_so ? vc->vc_so_at : vc->vc_at);
	}
}

void
vga_copycols(id, row, srccol, dstcol, ncols)
	void *id;
	int row, srccol, dstcol, ncols;
{
	struct vga_config *vc = id;
	bus_size_t srcoff, dstoff;

	srcoff = (row * vc->vc_ncol + srccol) * 2;
	dstoff = (row * vc->vc_ncol + dstcol) * 2;

	bus_space_copy_2(vc->vc_memt, vc->vc_memh, srcoff, vc->vc_memh, dstoff,
	    ncols);
}

void
vga_erasecols(id, row, startcol, ncols)
	void *id;
	int row, startcol, ncols;
{
	struct vga_config *vc = id;
	bus_size_t off;
	u_int16_t val;

	off = (row * vc->vc_ncol + startcol) * 2;

	val = (vc->vc_at << 8) | ' ';

	bus_space_set_region_2(vc->vc_memt, vc->vc_memh, off, val, ncols);
}

void
vga_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct vga_config *vc = id;
	bus_size_t srcoff, dstoff;

	srcoff = (srcrow * vc->vc_ncol + 0) * 2;
	dstoff = (dstrow * vc->vc_ncol + 0) * 2;

	bus_space_copy_2(vc->vc_memt, vc->vc_memh, srcoff, vc->vc_memh, dstoff,
	    nrows * vc->vc_ncol);
}

void
vga_eraserows(id, startrow, nrows)
	void *id;
	int startrow, nrows;
{
	struct vga_config *vc = id;
	bus_size_t off, count;
	u_int16_t val;

	off = (startrow * vc->vc_ncol + 0) * 2;
	count = nrows * vc->vc_ncol;

	val = (vc->vc_at << 8) | ' ';

	bus_space_set_region_2(vc->vc_memt, vc->vc_memh, off, val, count);
}

void
vga_set_attr(id, val)
	void *id;
	int val;
{
	struct vga_config *vc = id;

	vc->vc_so = val;
}
