/*	$OpenBSD: vgafb.c,v 1.3 1998/09/28 01:10:34 rahnds Exp $	*/
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
#include <dev/ic/vgafbvar.h>


struct cfdriver vgafb_cd = {
	NULL, "vgafb", DV_DULL,
};

void	vgafb_cursor __P((void *, int, int, int));
void	vgafb_putstr __P((void *, int, int, char *, int));
void	vgafb_copycols __P((void *, int, int, int, int));
void	vgafb_erasecols __P((void *, int, int, int));
void	vgafb_copyrows __P((void *, int, int, int));
void	vgafb_eraserows __P((void *, int, int));
void	vgafb_set_attr __P((void *, int));

static void drawChar ( struct vgafb_config *vc, char ch, int cx,
	int cy, char at);
static void setPixel( struct vgafb_config *vc, int x, int y, int v);
static void vgafb_invert_char ( struct vgafb_config *vc, int cx, int cy);
extern const char fontdata_8x16[];

struct wscons_emulfuncs vgafb_emulfuncs = {
	vgafb_cursor,
	vgafb_putstr,
	vgafb_copycols,
	vgafb_erasecols,
	vgafb_copyrows,
	vgafb_eraserows,
	vgafb_set_attr,
};

int	vgafb_print __P((void *, const char *));

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

/*
 * The following functions implement back-end configuration grabbing
 * and attachment.
 */
int
vgafb_common_probe(iot, memt, iobase, membase, memsize)
	bus_space_tag_t iot, memt;
	u_int32_t iobase, membase;
	size_t memsize;
{
	bus_space_handle_t ioh_b, ioh_c, ioh_d, memh;
	u_int16_t vgadata;
	int gotio_b, gotio_c, gotio_d, gotmem, rv;
	int width;

	gotio_b = gotio_c = gotio_d = gotmem = rv = 0;

	if (bus_space_map(iot, iobase+0x3b0, 0xc, 0, &ioh_b))
		goto bad;
	gotio_b = 1;
	if (bus_space_map(iot, iobase+0x3c0, 0x10, 0, &ioh_c))
		goto bad;
	gotio_c = 1;
	if (bus_space_map(iot, iobase+0x3d0, 0x10, 0, &ioh_d))
		goto bad;
	gotio_d = 1;
	if (bus_space_map(memt, membase, memsize, 0, &memh))
		goto bad;
	gotmem = 1;

#if 0
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
		bus_space_unmap(memt, memh, memsize);

	return (rv);
}

void
vgafb_common_setup(iot, memt, vc, iobase, membase, memsize)
	bus_space_tag_t iot, memt;
	struct vgafb_config *vc;
	u_int32_t iobase, membase;
	size_t memsize;
{
	int cpos;
	int width, height;

        vc->vc_iot = iot;
        vc->vc_memt = memt;

        if (bus_space_map(vc->vc_iot, iobase+0x3b0, 0xc, 0, &vc->vc_ioh_b))
                panic("vgafb_common_setup: couldn't map io b");
        if (bus_space_map(vc->vc_iot, iobase+0x3c0, 0x10, 0, &vc->vc_ioh_c))
                panic("vgafb_common_setup: couldn't map io c");
        if (bus_space_map(vc->vc_iot, iobase+0x3d0, 0x10, 0, &vc->vc_ioh_d))
                panic("vgafb_common_setup: couldn't map io d");
        if (bus_space_map(vc->vc_memt, membase, memsize, 0, &vc->vc_memh))
                panic("vgafb_common_setup: couldn't map memory"); 

	/* CR1 - Horiz. Display End */
	bus_space_write_1(iot, vc->vc_ioh_d, 4, 0x1);
	width = bus_space_read_1(iot, vc->vc_ioh_d, 5);
	/* (stored value + 1) * depth -> pixel width */
	width = ( width + 1 ) * 8;   
	vc->vc_ncol = width / FONT_WIDTH;

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
	}
	vc->vc_nrow = height / FONT_HEIGHT;

#if 0
	/* assume resolution is 640x480 */
	vc->vc_nrow = 25;
	vc->vc_ncol = 80;

	bus_space_write_1(iot, vc->vc_ioh_d, VGA_IO_D_6845_ADDR, 14); 
	cpos = bus_space_read_1(iot, vc->vc_ioh_d, VGA_IO_D_6845_DATA) << 8;
	bus_space_write_1(iot, vc->vc_ioh_d, VGA_IO_D_6845_ADDR, 15);
	cpos |= bus_space_read_1(iot, vc->vc_ioh_d, VGA_IO_D_6845_DATA);
	vc->vc_crow = cpos / vc->vc_ncol;
	vc->vc_ccol = cpos % vc->vc_ncol;
#endif

	vc->vc_crow = vc->vc_ccol = 0; /* Has to be some onscreen value */
	vc->vc_so = 0;

	/* clear screen, frob cursor, etc.? */
	vgafb_eraserows(vc, 0, vc->vc_nrow);

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
vgafb_wscons_attach(parent, vc, console)
	struct device *parent;
	struct vgafb_config *vc;
	int console;
{
	struct wscons_attach_args waa;
	struct wscons_odev_spec *wo;

        waa.waa_isconsole = console;
        wo = &waa.waa_odev_spec;

        wo->wo_emulfuncs = &vgafb_emulfuncs;
	wo->wo_emulfuncs_cookie = vc;

        wo->wo_ioctl = vc->vc_ioctl;
        wo->wo_mmap = vc->vc_mmap;
        wo->wo_miscfuncs_cookie = vc;

        wo->wo_nrows = vc->vc_nrow;
        wo->wo_ncols = vc->vc_ncol;
        wo->wo_crow = vc->vc_crow;
        wo->wo_ccol = vc->vc_ccol;
 
        config_found(parent, &waa, vgafb_print);
}

void
vgafb_wscons_console(vc)
	struct vgafb_config *vc;
{
	struct wscons_odev_spec wo;

        wo.wo_emulfuncs = &vgafb_emulfuncs;
	wo.wo_emulfuncs_cookie = vc;

	/* ioctl and mmap are unused until real attachment. */

        wo.wo_nrows = vc->vc_nrow;
        wo.wo_ncols = vc->vc_ncol;
        wo.wo_crow = vc->vc_crow;
        wo.wo_ccol = vc->vc_ccol;
 
        wscons_attach_console(&wo);
}

int
vgafb_print(aux, pnp)
	void *aux;
	const char *pnp;
{

	if (pnp)
		printf("wscons at %s", pnp);
	return (UNCONF);
}

int
vgafbioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	/*struct vgafb_config *vc = v;*/

	/* XXX */
	return -1;
}

int
vgafbmmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	struct vgafb_config *vc = v;
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
#elif defined(__powerpc__)
	return powerpc_btop(port);
#endif
}

/*
 * The following functions implement the MI ANSI terminal emulation on
 * a VGA display.
 */
void
vgafb_cursor(id, on, row, col)
	void *id;
	int on, row, col;
{
	struct vgafb_config *vc = id;
	bus_space_tag_t iot = vc->vc_iot;
	bus_space_handle_t ioh_d = vc->vc_ioh_d;
	int pos;

#if 0
	printf("vgafb_cursor: %d %d\n", row, col);
#endif
	if (( vc->vc_crow != -1) && (vc->vc_ccol != -1)) {
		vgafb_invert_char (vc, vc->vc_ccol, vc->vc_crow);
	}
	/* turn the cursor off */
	if (!on) {
		/* XXX disable cursor how??? */
		vc->vc_crow = vc->vc_ccol = -1;
	} else {
		vc->vc_crow = row;

		vc->vc_ccol = col;
		vgafb_invert_char (vc, col, row);
	}

#if 0
	pos = row * vc->vc_ncol + col;

	bus_space_write_1(iot, ioh_d, VGA_IO_D_6845_ADDR, 14);
	bus_space_write_1(iot, ioh_d, VGA_IO_D_6845_DATA, pos >> 8);
	bus_space_write_1(iot, ioh_d, VGA_IO_D_6845_ADDR, 15);
	bus_space_write_1(iot, ioh_d, VGA_IO_D_6845_DATA, pos);
#endif
}

void
vgafb_putstr(id, row, col, cp, len)
	void *id;
	int row, col;
	char *cp;
	int len;
{
	struct vgafb_config *vc = id;
	bus_space_tag_t memt = vc->vc_memt;
	bus_space_handle_t memh = vc->vc_memh;
	int i, off;

	for (i = 0; i < len; i++, cp++, col++) {
		drawChar(vc, *cp, col, row, vc->vc_so ? vc->vc_so_at : vc->vc_at);
	}
}

void
vgafb_copycols(id, row, srccol, dstcol, ncols)
	void *id;
	int row, srccol, dstcol, ncols;
{
	struct vgafb_config *vc = id;
	bus_size_t srcoff, dstoff;
	int i;

	for (i = 0; i < FONT_HEIGHT; i++) {
		srcoff = ((row*FONT_HEIGHT) * (vc->vc_ncol*FONT_WIDTH)
			+ srccol*FONT_WIDTH) + (i * (vc->vc_ncol*FONT_WIDTH));
		dstoff = ((row*FONT_HEIGHT) * (vc->vc_ncol*FONT_WIDTH)
			+ dstcol*FONT_WIDTH) + (i * (vc->vc_ncol*FONT_WIDTH));

/* this could be sped up using bus_space_copy_4, but the copies tend
 * to be slow and it didn't work correctly on ppc???
 */
		bus_space_copy_1(vc->vc_memt, vc->vc_memh, srcoff,
			vc->vc_memh, dstoff, ncols*FONT_WIDTH);
	}
}

void
vgafb_erasecols(id, row, startcol, ncols)
	void *id;
	int row, startcol, ncols;
{
	struct vgafb_config *vc = id;
	bus_size_t off;
	int i;

	for (i=0; i < ncols; i++) {
		drawChar( vc, ' ', startcol+i, row, vc->vc_at);
	}
}

void
vgafb_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct vgafb_config *vc = id;
	bus_size_t srcoff, dstoff;

	srcoff = ((srcrow*FONT_HEIGHT) * (vc->vc_ncol*FONT_WIDTH) + 0);
	dstoff = ((dstrow*FONT_HEIGHT) * (vc->vc_ncol*FONT_WIDTH) + 0);

#if 0
	bus_space_copy_1(vc->vc_memt, vc->vc_memh, srcoff, vc->vc_memh, dstoff,
	    (nrows*FONT_HEIGHT) * (vc->vc_ncol*FONT_WIDTH));
#else
	/* since pci memory space base address is a power of two, and
	 * the screen is a mulitple of 4 (since FONT_WIDTH is 8)
	 * aka The Screen base is always 4 byte aligned RIGHT?
	 * copying words will be faster than bytes.
	 */
	bus_space_copy_4(vc->vc_memt, vc->vc_memh, srcoff, vc->vc_memh, dstoff,
	    (nrows*FONT_HEIGHT) * (vc->vc_ncol*FONT_WIDTH)/4);
#endif
}

void
vgafb_eraserows(id, startrow, nrows)
	void *id;
	int startrow, nrows;
{
	struct vgafb_config *vc = id;
	bus_size_t off, count;
	u_int16_t val;

	off = (startrow*FONT_HEIGHT) * (vc->vc_ncol*FONT_WIDTH) + 0;
	count = (nrows*FONT_HEIGHT) * (vc->vc_ncol*FONT_WIDTH);

	val = (vc->vc_at & 0xf);

	bus_space_set_region_1(vc->vc_memt, vc->vc_memh, off, val, count);
}

void
vgafb_set_attr(id, val)
	void *id;
	int val;
{
	struct vgafb_config *vc = id;

	vc->vc_so = val;
}

static void
setPixel(vc, x, y, v)
	struct vgafb_config *vc;
	int x, y, v;
{
	bus_space_tag_t memt = vc->vc_memt;
	bus_space_handle_t memh = vc->vc_memh;

	bus_space_write_1(memt, memh, x+(y*(vc->vc_ncol*FONT_WIDTH)), v);
}
static void 
invertPixel(vc, x, y)
	struct vgafb_config *vc;
	int x, y;
{
	bus_space_tag_t memt = vc->vc_memt;
	bus_space_handle_t memh = vc->vc_memh;
	u_int8_t v;

	v = bus_space_read_1(memt, memh, x+(y*(vc->vc_ncol*FONT_WIDTH)));
	v = ~v & 0xf;
	bus_space_write_1(memt, memh, x+(y*(vc->vc_ncol*FONT_WIDTH)), v);
}

static void
drawChar ( vc, ch, cx, cy, at)
	struct vgafb_config *vc;
	char ch;
	int cx, cy;
	char at;
{
   const char *cp;
   unsigned char mask;
   int i, j;
   int x, y;

   i = ch * FONT_HEIGHT;
   cp = &fontdata_8x16[ i ];

   if ((cx == vc->vc_ccol) && (cy == vc->vc_crow)) {
	char tmp = at;
	at =  ((at  & 0xf) << 4) | ((at >> 4) & 0xf);
   }
   x = cx*FONT_WIDTH; y = cy*FONT_HEIGHT;

   for ( i = 0; i < FONT_HEIGHT; i++ )
   {
     for ( j = 0; j < FONT_WIDTH; j++)
     {
       mask = 0x80 >> j;
       if ( *cp & mask )
	 setPixel(vc, x, y, (at >> 4) & 0xf );
       else
	 setPixel(vc, x, y,  at & 0xf );
       x++;
     }
     cp++; y++; x = cx*FONT_WIDTH;
   }
}
static void
vgafb_invert_char ( vc, cx, cy)
	struct vgafb_config *vc;
	int cx, cy;
{
   const char *cp;
   unsigned char mask;
   int i, j;
   int x, y;

   x = cx*FONT_WIDTH; y = cy*FONT_HEIGHT;

   for ( i = 0; i < FONT_HEIGHT; i++ )
   {
     for ( j = 0; j < FONT_WIDTH; j++)
     {
       invertPixel(vc, x, y);
       x++;
     }
     cp++; y++; x = cx*FONT_WIDTH;
   }
}
