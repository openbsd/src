/*	$NetBSD: pcivga.c,v 1.8 1996/04/17 21:49:58 cgd Exp $	*/

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
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/pte.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <alpha/pci/pcivgavar.h>
#include <alpha/wscons/wsconsvar.h>

#define	PCIVGA_6845_ADDR	0x24
#define	PCIVGA_6845_DATA	0x25

int	pcivgamatch __P((struct device *, void *, void *));
void	pcivgaattach __P((struct device *, struct device *, void *));
int	pcivgaprint __P((void *, char *));

struct cfattach pcivga_ca = {
	sizeof(struct pcivga_softc), pcivgamatch, pcivgaattach,
};

struct cfdriver pcivga_cd = {
	NULL, "pcivga", DV_DULL,
};

void	pcivga_getdevconfig __P((bus_chipset_tag_t, pci_chipset_tag_t,
	    pcitag_t, struct pcivga_devconfig *dc));

struct pcivga_devconfig pcivga_console_dc;

void	pcivga_cursor __P((void *, int, int, int));
void	pcivga_putstr __P((void *, int, int, char *, int));
void	pcivga_copycols __P((void *, int, int, int,int));
void	pcivga_erasecols __P((void *, int, int, int));
void	pcivga_copyrows __P((void *, int, int, int));
void	pcivga_eraserows __P((void *, int, int));

struct wscons_emulfuncs pcivga_emulfuncs = {
	pcivga_cursor,
	pcivga_putstr,
	pcivga_copycols,
	pcivga_erasecols,
	pcivga_copyrows,
	pcivga_eraserows,
};

int	pcivgaioctl __P((struct device *, u_long, caddr_t, int,
	    struct proc *));
int	pcivgammap __P((struct device *, off_t, int));

int
pcivgamatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;

	/*
	 * If it's prehistoric/vga or display/vga, we match.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_PREHISTORIC &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_PREHISTORIC_VGA)
		return (1);
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	     PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA)
		return (1);

	return (0);
}

void
pcivga_getdevconfig(bc, pc, tag, dc)
	bus_chipset_tag_t bc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	struct pcivga_devconfig *dc;
{
	bus_io_handle_t ioh;
	int cpos;

	dc->dc_bc = bc;
	dc->dc_pc = pc;
	dc->dc_pcitag = tag;

	/* XXX deal with mapping foo */

	if (bus_mem_map(bc, 0xb8000, 0x8000, 0, &dc->dc_memh))
		panic("pcivga_getdevconfig: couldn't map memory");
	if (bus_io_map(bc, 0x3b0, 0x30, &ioh))
		panic("pcivga_getdevconfig: couldn't map io");
	dc->dc_ioh = ioh;

	dc->dc_nrow = 25;
	dc->dc_ncol = 80;

	dc->dc_ccol = dc->dc_crow = 0;

	bus_io_write_1(bc, ioh, PCIVGA_6845_ADDR, 14);
	cpos = bus_io_read_1(bc, ioh, PCIVGA_6845_DATA) << 8;
	bus_io_write_1(bc, ioh, PCIVGA_6845_ADDR, 15);
	cpos |= bus_io_read_1(bc, ioh, PCIVGA_6845_DATA);

	dc->dc_crow = cpos / dc->dc_ncol;
	dc->dc_ccol = cpos % dc->dc_ncol;

	dc->dc_so = 0;
#if 0
	dc->dc_at = 0x00 | 0xf;		  /* black bg | white fg */
	dc->dc_so_at = 0x00 | 0xf | 0x80; /* black bg | white fg | blink */

	/* clear screen, frob cursor, etc.? */
	pcivga_eraserows(dc, 0, dc->dc_nrow);
#endif
	/*
	 * XXX DEC HAS SWITCHED THE CODES FOR BLUE AND RED!!!
	 * XXX Therefore, though the comments say "blue bg", the code uses
	 * XXX the value for a red background!
	 */
	dc->dc_at = 0x40 | 0x0f;		/* blue bg | white fg */
	dc->dc_so_at = 0x40 | 0x0f | 0x80;	/* blue bg | white fg | blink */
}

void
pcivgaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct pcivga_softc *sc = (struct pcivga_softc *)self;
	struct wscons_attach_args waa;
	struct wscons_odev_spec *wo;
	char devinfo[256];
	int console;

	console = (pa->pa_tag == pcivga_console_dc.dc_pcitag);
	if (console)
		sc->sc_dc = &pcivga_console_dc;
	else {
		sc->sc_dc = (struct pcivga_devconfig *)
		    malloc(sizeof(struct pcivga_devconfig), M_DEVBUF, M_WAITOK);
		pcivga_getdevconfig(pa->pa_bc, pa->pa_pc, pa->pa_tag,
		    sc->sc_dc);
	}

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo,
	    PCI_REVISION(pa->pa_class));

#if 0
	if (sc->sc_dc->dc_tgaconf == NULL) {
		printf("unknown board configuration\n");
		return;
	}
	printf("board type %s\n", sc->sc_dc->dc_tgaconf->tgac_name);
	printf("%s: %d x %d, %dbpp, %s RAMDAC\n", sc->sc_dev.dv_xname,
	    sc->sc_dc->dc_wid, sc->sc_dc->dc_ht,
	    sc->sc_dc->dc_tgaconf->tgac_phys_depth, 
	    sc->sc_dc->dc_tgaconf->tgac_ramdac->tgar_name);
#endif

#if 0
	pci_intrdata = pci_conf_read(sc->sc_pcitag, PCI_INTERRUPT_REG);
	if (PCI_INTERRUPT_PIN(pci_intrdata) != PCI_INTERRUPT_PIN_NONE) {
		sc->sc_intr = pci_map_int(sc->sc_pcitag, IPL_TTY, tgaintr, sc);
		if (sc->sc_intr == NULL)
			printf("%s: WARNING: couldn't map interrupt\n",
			    sc->sc_dev.dv_xname);
	}
#endif

	waa.waa_isconsole = console;
	wo = &waa.waa_odev_spec;
	wo->wo_ef = &pcivga_emulfuncs;
	wo->wo_efa = sc->sc_dc;
	wo->wo_nrows = sc->sc_dc->dc_nrow;
	wo->wo_ncols = sc->sc_dc->dc_ncol;
	wo->wo_crow = sc->sc_dc->dc_crow;
	wo->wo_ccol = sc->sc_dc->dc_ccol;
	wo->wo_ioctl = pcivgaioctl;
	wo->wo_mmap = pcivgammap;

	config_found(self, &waa, pcivgaprint);
}

int
pcivgaprint(aux, pnp)
	void *aux;
	char *pnp;
{

	if (pnp)
		printf("wscons at %s", pnp);
	return (UNCONF);
}

int
pcivgaioctl(dev, cmd, data, flag, p)
	struct device *dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{

	return -1; /* XXX */
}

int
pcivgammap(dev, offset, prot)
	struct device *dev;
	off_t offset;
	int prot;
{
	struct pcivga_softc *sc = (struct pcivga_softc *)dev;
	int rv;

	rv = -1;
#if 0 /* XXX */
	if (offset >= 0 && offset < 0x100000) {		/* 1MB */
		/* Deal with mapping the VGA memory */
		if (offset >= 0xb8000 && offset < 0xc0000) {
			offset -= 0xb8000;
			rv = alpha_btop(k0segtophys(sc->sc_dc->dc_crtat) +
			    offset);
		}
	} else {
		/* XXX should do something with PCI memory */
		rv = -1;
	}
#endif
	return rv;
}

void
pcivga_console(bc, pc, bus, device, function)
	bus_chipset_tag_t bc;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	struct pcivga_devconfig *dcp = &pcivga_console_dc;
	struct wscons_odev_spec wo;

	pcivga_getdevconfig(bc, pc,
	    pci_make_tag(pc, bus, device, function), dcp);

        wo.wo_ef = &pcivga_emulfuncs;
        wo.wo_efa = dcp;
        wo.wo_nrows = dcp->dc_nrow;
        wo.wo_ncols = dcp->dc_ncol;
	wo.wo_crow = dcp->dc_crow;
	wo.wo_ccol = dcp->dc_ccol;
        /* ioctl and mmap are unused until real attachment. */

        wscons_attach_console(&wo);
}

/*
 * The following functions implement the MI ANSI terminal emulation on
 * a VGA display.
 */
void
pcivga_cursor(id, on, row, col)
	void *id;
	int on, row, col;
{
	struct pcivga_devconfig *dc = id;
	bus_chipset_tag_t bc = dc->dc_bc;
	bus_io_handle_t ioh = dc->dc_ioh;
	int pos;

#if 0
	printf("pcivga_cursor: %d %d\n", row, col);
#endif
        /* turn the cursor off */
        if (!on) {
		/* XXX disable cursor how??? */
		dc->dc_crow = dc->dc_ccol = -1;
        } else {
		dc->dc_crow = row;
		dc->dc_ccol = col;
        }

	pos = row * dc->dc_ncol + col;

        bus_io_write_1(bc, ioh, PCIVGA_6845_ADDR, 14);
        bus_io_write_1(bc, ioh, PCIVGA_6845_DATA, pos >> 8);
        bus_io_write_1(bc, ioh, PCIVGA_6845_ADDR, 15);
        bus_io_write_1(bc, ioh, PCIVGA_6845_DATA, pos);
}

void
pcivga_putstr(id, row, col, cp, len)
	void *id;
	int row, col;
	char *cp;
	int len;
{
	struct pcivga_devconfig *dc = id;
	bus_chipset_tag_t bc = dc->dc_bc;
	bus_mem_handle_t memh = dc->dc_memh;
	char *dcp;
	int i, off;

	off = (row * dc->dc_ncol + col) * 2;
	for (i = 0; i < len; i++, cp++, off += 2) {
		bus_mem_write_1(bc, memh, off, *cp);
		bus_mem_write_1(bc, memh, off + 1,
		    dc->dc_so ? dc->dc_so_at : dc->dc_at);
	}
}

void
pcivga_copycols(id, row, srccol, dstcol, ncols)
	void *id;
	int row, srccol, dstcol, ncols;
{
	struct pcivga_devconfig *dc = id;
	bus_chipset_tag_t bc = dc->dc_bc;
	bus_mem_handle_t memh = dc->dc_memh;
	bus_mem_size_t srcoff, srcend, dstoff;

	/*
	 * YUCK.  Need bus copy functions.
	 */
	srcoff = (row * dc->dc_ncol + srccol) * 2;
	srcend = srcoff + ncols * 2;
	dstoff = (row * dc->dc_ncol + dstcol) * 2;

	for (; srcoff < srcend; srcoff += 2, dstoff += 2)
		bus_mem_write_2(bc, memh, dstoff,
		    bus_mem_read_2(bc, memh, srcoff));
}

void
pcivga_erasecols(id, row, startcol, ncols)
	void *id;
	int row, startcol, ncols;
{
	struct pcivga_devconfig *dc = id;
	bus_chipset_tag_t bc = dc->dc_bc;
	bus_mem_handle_t memh = dc->dc_memh;
	bus_mem_size_t off, endoff;
	u_int16_t val;

	/*
	 * YUCK.  Need bus 'set' functions.
	 */
	off = (row * dc->dc_ncol + startcol) * 2;
	endoff = off + ncols * 2;
	val = (dc->dc_at << 8) | ' ';

	for (; off < endoff; off += 2)
		bus_mem_write_2(bc, memh, off, val);
}

void
pcivga_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct pcivga_devconfig *dc = id;
	bus_chipset_tag_t bc = dc->dc_bc;
	bus_mem_handle_t memh = dc->dc_memh;
	bus_mem_size_t srcoff, srcend, dstoff;

	/*
	 * YUCK.  Need bus copy functions.
	 */
	srcoff = (srcrow * dc->dc_ncol + 0) * 2;
	srcend = srcoff + (nrows * dc->dc_ncol * 2);
	dstoff = (dstrow * dc->dc_ncol + 0) * 2;

	for (; srcoff < srcend; srcoff += 2, dstoff += 2)
		bus_mem_write_2(bc, memh, dstoff,
		    bus_mem_read_2(bc, memh, srcoff));
}

void
pcivga_eraserows(id, startrow, nrows)
	void *id;
	int startrow, nrows;
{
	struct pcivga_devconfig *dc = id;
	bus_chipset_tag_t bc = dc->dc_bc;
	bus_mem_handle_t memh = dc->dc_memh;
	bus_mem_size_t off, endoff;
	u_int16_t val;

	/*
	 * YUCK.  Need bus 'set' functions.
	 */
	off = (startrow * dc->dc_ncol + 0) * 2;
	endoff = off + (nrows * dc->dc_ncol) * 2;
	val = (dc->dc_at << 8) | ' ';

	for (; off < endoff; off += 2)
		bus_mem_write_2(bc, memh, off, val);
}
