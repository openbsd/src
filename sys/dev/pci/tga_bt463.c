/* $NetBSD: tga_bt463.c,v 1.4 1999/04/30 02:47:42 nathanw Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <vm/vm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/tgareg.h>
#include <dev/pci/tgavar.h>
#include <dev/ic/bt463reg.h>

#include <dev/wscons/wsconsio.h>

/*
 * Functions exported via the RAMDAC configuration table.
 */
void	tga_bt463_init __P((struct tga_devconfig *, int));
int	tga_bt463_intr __P((void *));
int	tga_bt463_set_cmap __P((struct tga_devconfig *,
	    struct wsdisplay_cmap *));
int	tga_bt463_get_cmap __P((struct tga_devconfig *,
	    struct wsdisplay_cmap *));

int	tga_bt463_check_curcmap __P((struct tga_devconfig *,
	    struct wsdisplay_cursor *cursorp));
void	tga_bt463_set_curcmap __P((struct tga_devconfig *,
	    struct wsdisplay_cursor *cursorp));
int	tga_bt463_get_curcmap __P((struct tga_devconfig *,
	    struct wsdisplay_cursor *cursorp));

const struct tga_ramdac_conf tga_ramdac_bt463 = {
	"Bt463",
	tga_bt463_init,
	tga_bt463_intr,
	tga_bt463_set_cmap,
	tga_bt463_get_cmap,
	tga_builtin_set_cursor,
	tga_builtin_get_cursor,
	tga_builtin_set_curpos,
	tga_builtin_get_curpos,
	tga_builtin_get_curmax,
	tga_bt463_check_curcmap,
	tga_bt463_set_curcmap,
	tga_bt463_get_curcmap,
};

/*
 * Private data.
 */
struct bt463data {
	int	changed;			/* what changed; see below */
	char curcmap_r[2];			/* cursor colormap */
	char curcmap_g[2];
	char curcmap_b[2];
	char cmap_r[BT463_NCMAP_ENTRIES];	/* colormap */
	char cmap_g[BT463_NCMAP_ENTRIES];
	char cmap_b[BT463_NCMAP_ENTRIES];
	int window_type[16]; /* 16 24-bit window type table entries */
};

#define	DATA_CURCMAP_CHANGED	0x01	/* cursor colormap changed */
#define	DATA_CMAP_CHANGED	0x02	/* colormap changed */
#define	DATA_WTYPE_CHANGED	0x04	/* window type table changed */
#define	DATA_ALL_CHANGED	0x07

/*
 * Internal functions.
 */
inline void tga_bt463_wr_d __P((bus_space_tag_t, bus_space_handle_t,
		u_int, u_int8_t));
inline u_int8_t tga_bt463_rd_d __P((bus_space_tag_t, bus_space_handle_t,
		u_int));
inline void tga_bt463_wraddr __P((bus_space_tag_t, bus_space_handle_t,
		u_int16_t));

void	tga_bt463_update __P((bus_space_tag_t, bus_space_handle_t, 
		struct bt463data *));

#define	tga_bt463_sched_update(tag,regs)					\
	bus_space_write_4((tag), (regs), (TGA_REG_SISR*4), (0x00010000))

/*****************************************************************************/

/*
 * Functions exported via the RAMDAC configuration table.
 */

void
tga_bt463_init(dc, alloc)
	struct tga_devconfig *dc;
	int alloc;
{
	struct bt463data tmp, *data;
	bus_space_tag_t tag;
	bus_space_handle_t regs;
	int i;

	/*
	 * Init the BT463 for normal operation.
	 */

	tag = dc->dc_memt;
	bus_space_subregion(tag, dc->dc_vaddr, TGA_MEM_CREGS, 512, 
						&regs);
	/*
	 * Setup:
	 * reg 0: 4:1 multiplexing, 25/75 blink.
	 * reg 1: Overlay mapping: mapped to common palette, 
	 *        14 window type entries, 24-plane configuration mode,
	 *        4 overlay planes, underlays disabled, no cursor. 
	 * reg 2: sync-on-green disabled, pedestal enabled.
	 */
	tga_bt463_wraddr(tag, regs, BT463_IREG_COMMAND_0);
	tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 0x40);

	tga_bt463_wraddr(tag, regs, BT463_IREG_COMMAND_1);
	tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 0x48);

	tga_bt463_wraddr(tag, regs, BT463_IREG_COMMAND_2);
	tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 0x40);

	/*
	 * Initialize the read mask.
	 */
	tga_bt463_wraddr(tag, regs, BT463_IREG_READ_MASK_P0_P7);
	for (i = 0; i < 3; i++)
		tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 0xff);

	/*
	 * Initialize the blink mask.
	 */
	tga_bt463_wraddr(tag, regs, BT463_IREG_BLINK_MASK_P0_P7);
	for (i = 0; i < 3; i++)
		tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 0);

	/*
	 * Clear test register
	 */
	tga_bt463_wraddr(tag, regs, BT463_IREG_TEST);
	tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 0);

	/*
	 * If we should allocate a new private info struct, do so.
	 * Otherwise, use the one we have (if it's there), or
	 * use the temporary one on the stack.
	 */
	if (alloc) {
		if (dc->dc_ramdac_private != NULL)
			panic("tga_bt463_init: already have private struct");
		dc->dc_ramdac_private = malloc(sizeof *data, M_DEVBUF,
		    M_WAITOK);
	}
	if (dc->dc_ramdac_private != NULL)
		data = dc->dc_ramdac_private;
	else
		data = &tmp;

	/*
	 * Initalize the RAMDAC info struct to hold all of our
	 * data, and fill it in.
	 */
	data->changed = DATA_ALL_CHANGED;

	/* initial cursor colormap: 0 is black, 1 is white */
	data->curcmap_r[0] = data->curcmap_g[0] = data->curcmap_b[0] = 0;
	data->curcmap_r[1] = data->curcmap_g[1] = data->curcmap_b[1] = 0xff;

	/* Initial colormap: 0 is black, everything else is white */
	data->cmap_r[0] = data->cmap_g[0] = data->cmap_b[0] = 0;
	for (i = 1; i < 256; i++)
		data->cmap_r[i] = data->cmap_g[i] = data->cmap_b[i] = 255;


	/* Initialize the window type table:
	 * Entry 0: 8-plane pseudocolor in the bottom 8 bits, 
	 *          overlays disabled, colormap starting at 0. 
	 *
	 *  Lookup table bypass:       no (    0 << 23 & 0x800000)       0
	 *  Colormap address:       0x000 (0x000 << 17 & 0x7e0000)       0 
	 *  Overlay mask:             0x0 (    0 << 13 & 0x01e000)       0
	 *  Overlay location:    P<27:24> (    0 << 12 & 0x001000)       0
	 *  Display mode:     Pseudocolor (    1 <<  9 & 0x000e00)   0x200
	 *  Number of planes:           8 (    8 <<  5 & 0x0001e0)   0x100
	 *  Plane shift:               16 (   16 <<  0 & 0x00001f)      10
	 *                                                        --------
	 *                                                           0x310
	 */	  
	data->window_type[0] = 0x310;
	/* The colormap interface to the world only supports one colormap, 
	 * so having an entry for the 'alternate' colormap in the bt463 
	 * probably isn't useful.
	 */
	/* Entry 1: 24-plane truecolor, overlays disabled. */
	data->window_type[1] = 0x200;

	/* Fill the remaining table entries with clones of entry 0 until we 
	 * figure out a better use for them.
	 */

	for (i = 2; i < BT463_NWTYPE_ENTRIES; i++) {
		data->window_type[i] = 0x310;
	}

	/* The Bt463 won't accept window type table data
	 * except during a blanking interval. Normally we would 
	 * do this by scheduling an interrupt, but this is run 
	 * during autoconfiguration, when interrupts are disabled. 
	 * So we spin on the end-of-frame interrupt bit. 
	 */

	bus_space_write_4(tag, regs, TGA_REG_SISR*4, 0x00010001);
	bus_space_barrier(tag, regs, TGA_REG_SISR*4, 4, BUS_BARRIER_WRITE);
	while ((bus_space_read_4(tag, regs, (TGA_REG_SISR * 4))& 0x00000001) == 0);

	tga_bt463_update(tag, regs, data);

	bus_space_write_4(tag, regs, TGA_REG_SISR*4, 0x00000001);
	bus_space_barrier(tag, regs, TGA_REG_SISR*4, 4, BUS_BARRIER_WRITE);

	tga_bt463_sched_update(tag, regs);

}

int
tga_bt463_set_cmap(dc, cmapp)
	struct tga_devconfig *dc;
	struct wsdisplay_cmap *cmapp;
{
	struct bt463data *data = dc->dc_ramdac_private;
	bus_space_tag_t tag = dc->dc_memt;
	bus_space_handle_t regs;
	int count, index, s;

	bus_space_subregion(tag, dc->dc_vaddr, TGA_MEM_CREGS, 512, 
						&regs);
	

	if ((u_int)cmapp->index >= BT463_NCMAP_ENTRIES ||
	    ((u_int)cmapp->index + (u_int)cmapp->count) > BT463_NCMAP_ENTRIES)
		return (EINVAL);
#ifdef UVM
	if (!uvm_useracc(cmapp->red, cmapp->count, B_READ) ||
	    !uvm_useracc(cmapp->green, cmapp->count, B_READ) ||
	    !uvm_useracc(cmapp->blue, cmapp->count, B_READ))
		return (EFAULT);
#else
	if (!useracc(cmapp->red, cmapp->count, B_READ) ||
	    !useracc(cmapp->green, cmapp->count, B_READ) ||
	    !useracc(cmapp->blue, cmapp->count, B_READ))
		return (EFAULT);
#endif

	s = spltty();

	index = cmapp->index;
	count = cmapp->count;
	copyin(cmapp->red, &data->cmap_r[index], count);
	copyin(cmapp->green, &data->cmap_g[index], count);
	copyin(cmapp->blue, &data->cmap_b[index], count);

	data->changed |= DATA_CMAP_CHANGED;

	tga_bt463_sched_update(tag, regs);
	splx(s);

	return (0);
}

int
tga_bt463_get_cmap(dc, cmapp)
	struct tga_devconfig *dc;
	struct wsdisplay_cmap *cmapp;
{
	struct bt463data *data = dc->dc_ramdac_private;
	int error, count, index;

	if ((u_int)cmapp->index >= BT463_NCMAP_ENTRIES ||
	    ((u_int)cmapp->index + (u_int)cmapp->count) > BT463_NCMAP_ENTRIES)
		return (EINVAL);

	count = cmapp->count;
	index = cmapp->index;

	error = copyout(&data->cmap_r[index], cmapp->red, count);
	if (error)
		return (error);
	error = copyout(&data->cmap_g[index], cmapp->green, count);
	if (error)
		return (error);
	error = copyout(&data->cmap_b[index], cmapp->blue, count);
	return (error);
}

int
tga_bt463_check_curcmap(dc, cursorp)
	struct tga_devconfig *dc;
	struct wsdisplay_cursor *cursorp;
{
	int count;

	if ((u_int)cursorp->cmap.index > 2 ||
	    ((u_int)cursorp->cmap.index +
	     (u_int)cursorp->cmap.count) > 2)
		return (EINVAL);
	count = cursorp->cmap.count; 
#ifdef UVM
	if (!uvm_useracc(cursorp->cmap.red, count, B_READ) ||
	    !uvm_useracc(cursorp->cmap.green, count, B_READ) ||
	    !uvm_useracc(cursorp->cmap.blue, count, B_READ))
		return (EFAULT);
#else
	if (!useracc(cursorp->cmap.red, count, B_READ) ||
	    !useracc(cursorp->cmap.green, count, B_READ) ||
	    !useracc(cursorp->cmap.blue, count, B_READ))
		return (EFAULT);
#endif
	return (0);
}

void
tga_bt463_set_curcmap(dc, cursorp)
	struct tga_devconfig *dc;
	struct wsdisplay_cursor *cursorp;
{
	struct bt463data *data = dc->dc_ramdac_private;
	int count, index;
	bus_space_tag_t tag = dc->dc_memt;
	bus_space_handle_t regs;

	bus_space_subregion(tag, dc->dc_vaddr, TGA_MEM_CREGS, 512, 
						&regs);

	/* can't fail; parameters have already been checked. */
	count = cursorp->cmap.count;
	index = cursorp->cmap.index;
	copyin(cursorp->cmap.red, &data->curcmap_r[index], count);
	copyin(cursorp->cmap.green, &data->curcmap_g[index], count);
	copyin(cursorp->cmap.blue, &data->curcmap_b[index], count);
	data->changed |= DATA_CURCMAP_CHANGED;
	tga_bt463_sched_update(tag, regs);
}

int
tga_bt463_get_curcmap(dc, cursorp)
	struct tga_devconfig *dc;
	struct wsdisplay_cursor *cursorp;
{
	struct bt463data *data = dc->dc_ramdac_private;
	int error;

	cursorp->cmap.index = 0;	/* DOCMAP */
	cursorp->cmap.count = 2;
	if (cursorp->cmap.red != NULL) {
		error = copyout(data->curcmap_r, cursorp->cmap.red, 2);
		if (error)
			return (error);
	}
	if (cursorp->cmap.green != NULL) {
		error = copyout(data->curcmap_g, cursorp->cmap.green, 2);
		if (error)
			return (error);
	}
	if (cursorp->cmap.blue != NULL) {
		error = copyout(data->curcmap_b, cursorp->cmap.blue, 2);
		if (error)
			return (error);
	}
	return (0);
}

int
tga_bt463_intr(v)
	void *v;
{
	struct tga_devconfig *dc = v;
	bus_space_tag_t tag = dc->dc_memt;
	bus_space_handle_t regs;

	bus_space_subregion(tag, dc->dc_vaddr, TGA_MEM_CREGS, 512, 
						&regs);

	if ( (bus_space_read_4(tag, regs, TGA_REG_SISR*4) & 0x00010001) != 
		 0x00010001) {
		printf("Spurious interrupt");
		return 0;
	}

	tga_bt463_update(tag, regs, dc->dc_ramdac_private);

	bus_space_write_4(tag, regs, TGA_REG_SISR*4, 0x00000001);
	bus_space_barrier(tag, regs, TGA_REG_SISR*4, 4, BUS_BARRIER_WRITE);
	return (1);
}

/*****************************************************************************/

/*
 * Internal functions.
 */

inline void
tga_bt463_wr_d(tag, regs, btreg, val)
	 bus_space_tag_t tag;
	 bus_space_handle_t regs;
	u_int btreg;
	u_int8_t val;
{

	if (btreg > BT463_REG_MAX)
		panic("tga_bt463_wr_d: reg %d out of range\n", btreg);

	/* 
	 * In spite of the 21030 documentation, to set the MPU bus bits for
	 * a write, you set them in the upper bits of EPDR, not EPSR.
	 */
	
	/* 
	 * Strobe CE# (high->low->high) since status and data are latched on
	 * the falling and rising edges of this active-low signal.
	 */
	   
	bus_space_barrier(tag, regs, TGA_REG_EPDR*4, 4, BUS_BARRIER_WRITE);
	bus_space_write_4(tag, regs, TGA_REG_EPDR*4, (btreg << 10 ) | 0x100 | val);
	bus_space_barrier(tag, regs, TGA_REG_EPDR*4, 4, BUS_BARRIER_WRITE);
	bus_space_write_4(tag, regs, TGA_REG_EPDR*4, (btreg << 10 ) | 0x000 | val);
	bus_space_barrier(tag, regs, TGA_REG_EPDR*4, 4, BUS_BARRIER_WRITE);
	bus_space_write_4(tag, regs, TGA_REG_EPDR*4, (btreg << 10 ) | 0x100 | val);
}

inline u_int8_t
tga_bt463_rd_d(tag, regs, btreg)
	 bus_space_tag_t tag;
	 bus_space_handle_t regs;
	u_int btreg;
{
	tga_reg_t rdval;

	if (btreg > BT463_REG_MAX)
		panic("tga_bt463_rd_d: reg %d out of range\n", btreg);

	/* 
	 * Strobe CE# (high->low->high) since status and data are latched on 
	 * the falling and rising edges of this active-low signal.
	 */

	bus_space_barrier(tag, regs, TGA_REG_EPSR*4, 4, BUS_BARRIER_WRITE);
	bus_space_write_4(tag, regs, TGA_REG_EPSR*4, (btreg << 2) | 2 | 1);
	bus_space_barrier(tag, regs, TGA_REG_EPSR*4, 4, BUS_BARRIER_WRITE);
	bus_space_write_4(tag, regs, TGA_REG_EPSR*4, (btreg << 2) | 2 | 0);

	bus_space_barrier(tag, regs, TGA_REG_EPSR*4, 4, BUS_BARRIER_READ);

	rdval = bus_space_read_4(tag, regs, TGA_REG_EPDR*4);
	bus_space_barrier(tag, regs, TGA_REG_EPSR*4, 4, BUS_BARRIER_WRITE);
	bus_space_write_4(tag, regs, TGA_REG_EPSR*4, (btreg << 2) | 2 | 1);

	return (rdval >> 16) & 0xff;
}

inline void
tga_bt463_wraddr(tag, regs, ireg)
	 bus_space_tag_t tag;
	 bus_space_handle_t regs;
	u_int16_t ireg;
{
	tga_bt463_wr_d(tag, regs, BT463_REG_ADDR_LOW, ireg & 0xff);
	tga_bt463_wr_d(tag, regs, BT463_REG_ADDR_HIGH, (ireg >> 8) & 0xff);
}

void
tga_bt463_update(tag, regs, data)
	 bus_space_tag_t tag;
	 bus_space_handle_t regs;
	struct bt463data *data;
{
	int i, v, valid, blanked;

	v = data->changed;

	/* The Bt463 won't accept window type data except during a blanking
	 * interval. So we (1) do this early in the interrupt, in case it 
	 * takes a long time, and (2) blank the screen while doing so, in case
	 * we run out normal vertical blanking. 
	 */
	if (v & DATA_WTYPE_CHANGED) {
		valid = bus_space_read_4(tag, regs, TGA_REG_VVVR*4);
		blanked = valid | VVR_BLANK;
		bus_space_write_4(tag, regs, TGA_REG_VVVR*4, blanked);
		bus_space_barrier(tag, regs, TGA_REG_VVVR*4, 4, 
			BUS_BARRIER_WRITE);
		/* spit out the window type data */
		for (i = 0; i < BT463_NWTYPE_ENTRIES; i++) {
			tga_bt463_wraddr(tag, regs, BT463_IREG_WINDOW_TYPE_TABLE + i);
			tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 
				(data->window_type[i]) & 0xff);         /* B0-7   */
			tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 
				(data->window_type[i] >> 8) & 0xff);    /* B8-15  */
			tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA, 
				(data->window_type[i] >> 16) & 0xff);   /* B16-23 */

		}
		bus_space_write_4(tag, regs, TGA_REG_VVVR*4, valid);
		bus_space_barrier(tag, regs, TGA_REG_VVVR*4, 4, 
			BUS_BARRIER_WRITE);
	}

	if (v & DATA_CURCMAP_CHANGED) {
		tga_bt463_wraddr(tag, regs, BT463_IREG_CURSOR_COLOR_0);
		/* spit out the cursor data */
		for (i = 0; i < 2; i++) {
			tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA,
			    data->curcmap_r[i]);
			tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA,
			    data->curcmap_g[i]);
			tga_bt463_wr_d(tag, regs, BT463_REG_IREG_DATA,
			    data->curcmap_b[i]);
		}
	}

	if (v & DATA_CMAP_CHANGED) {

		tga_bt463_wraddr(tag, regs, BT463_IREG_CPALETTE_RAM);
		/* spit out the colormap data */
		for (i = 0; i < BT463_NCMAP_ENTRIES; i++) {
			tga_bt463_wr_d(tag, regs, BT463_REG_CMAP_DATA,
			    data->cmap_r[i]);
			tga_bt463_wr_d(tag, regs, BT463_REG_CMAP_DATA,
			    data->cmap_g[i]);
			tga_bt463_wr_d(tag, regs, BT463_REG_CMAP_DATA,
			    data->cmap_b[i]);
		}
	}

	data->changed = 0;


}
