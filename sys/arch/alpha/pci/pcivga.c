/*	$NetBSD: pcivga.c,v 1.2 1995/08/03 01:17:17 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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
#include <machine/pio.h>

#include <dev/pseudo/ansicons.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <alpha/pci/pcivgavar.h>

int	pcivgamatch __P((struct device *, void *, void *));
void	pcivgaattach __P((struct device *, struct device *, void *));

struct cfdriver pcivgacd = {
	NULL, "pcivga", pcivgamatch, pcivgaattach, DV_DULL,
	    sizeof(struct pcivga_softc)
};

struct pcivga_devconfig pcivga_console_dc;

void	pcivga_bell __P((void *));			/* XXX */

struct ansicons_functions pcivga_acf = {
	pcivga_bell,
	pcivga_cursor,
	pcivga_putstr,
	pcivga_copycols,
	pcivga_erasecols,
	pcivga_copyrows,
	pcivga_eraserows,
};

#define	PCIVGAUNIT(dev)	minor(dev)

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
	if ((PCI_CLASS(pa->pa_class) == PCI_CLASS_PREHISTORIC &&
	     PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_PREHISTORIC_VGA) ||
	    (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	     PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA))
		return (1);

	return (0);
}

void
pcivga_getdevconfig(tag, dc)
	pcitag_t tag;
	struct pcivga_devconfig *dc;
{

	dc->dc_pcitag = tag;

#if 0
	vm_offset_t pciva, pcipa;
	int i;

	dc->dc_vaddr = 0;
	if (pci_map_mem(tag, 0x10, &dc->dc_vaddr, &pcipa))
		return;
#endif

#if 0
	int i;
	pcireg_t old;

	printf("\n");
	for (i = PCI_MAP_REG_START; i < PCI_MAP_REG_END; i += 4) {
		old = pci_conf_read(tag, i);
		pci_conf_write(tag, i, 0xffffffff);
		printf("pcivga_getdevconfig: ");
		printf("mapping reg @ %d = 0x%x (mask 0x%x)\n",
			i, old, pci_conf_read(tag, i));
		pci_conf_write(tag, i, old);
	}
	printf("foo");
#endif

	dc->dc_crtat = (u_short *)phystok0seg(0xb8000 | (3L << 32)); /* XXX */
	dc->dc_iobase = 0x3d4;			/* XXX */

	dc->dc_nrow = 25;
	dc->dc_ncol = 80;
	dc->dc_ccol = dc->dc_crow = 0;

	dc->dc_so = 0;
	dc->dc_at = 0x00 | 0xf;		  /* black bg | white fg */
	dc->dc_so_at = 0x00 | 0xf | 0x80; /* black bg | white fg | blink */

	/* clear screen, frob cursor, etc.? */
	pcivga_eraserows(dc, 0, dc->dc_nrow);
}

void
pcivgaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct pcivga_softc *sc = (struct pcivga_softc *)self;
	char devinfo[256];
	int console;

	console = (pa->pa_tag == pcivga_console_dc.dc_pcitag);
	if (console)
		sc->sc_dc = &pcivga_console_dc;
	else {
		sc->sc_dc = (struct pcivga_devconfig *)
		    malloc(sizeof(struct pcivga_devconfig), M_DEVBUF, M_WAITOK);
		pcivga_getdevconfig(pa->pa_tag, sc->sc_dc);
	}
	if (sc->sc_dc->dc_crtat == NULL) {
		printf(": couldn't map memory space; punt!\n");
		return;
	}

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, NULL);
	printf(": %s (revision 0x%x)\n", devinfo, PCI_REVISION(pa->pa_class));

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
		sc->sc_intr = pci_map_int(sc->sc_pcitag, PCI_IPL_TTY,
		    tgaintr, sc);
		if (sc->sc_intr == NULL)
			printf("%s: WARNING: couldn't map interrupt\n",
			    sc->sc_dev.dv_xname);
	}
#endif

	if (!wscattach_output(self, console, &sc->sc_dc->dc_ansicons,
	    &pcivga_acf, sc->sc_dc, sc->sc_dc->dc_nrow, sc->sc_dc->dc_ncol,
	    0, 0)) {
		panic("pcivgaattach: wscattach failed");
		/* NOTREACHED */
	}
}

#if 0
int
tgammap(dev, offset, nprot)
	dev_t dev;
	int offset;
	int nprot;
{
	struct pcivga_softc *sc = pcivgacd.cd_devs[TGAUNIT(dev)];

	if (offset > sc->sc_dc->dc_pcivgaconf->pcivgac_cspace_size)
		return -1;
	return alpha_btop(sc->sc_dc->dc_paddr + offset);
}
#endif

void
pcivga_console(bus, device, function)
	int bus, device, function;
{
	struct pcivga_devconfig *dcp = &pcivga_console_dc;

	pcivga_getdevconfig(pci_make_tag(bus, device, function), dcp);

	/* sanity checks */
	if (dcp->dc_crtat == NULL)
		panic("pcivga_console(%d, %d, %d): couldn't map memory space",
		    bus, device, function);
#if 0
	if (dcp->dc_pcivgaconf == NULL)
		panic("pcivga_console(%d, %d, %d): unknown board configuration",
		    bus, device, function);
#endif

	wsc_console(&dcp->dc_ansicons, &pcivga_acf, dcp,
	    dcp->dc_nrow, dcp->dc_ncol, 0, 0);
}

/*
 * The following functions implement the MI ANSI terminal emulation on
 * a VGA display.
 */
void							/* XXX */
pcivga_bell(id)						/* XXX */
	void *id;					/* XXX */
{							/* XXX */
							/* XXX */
	printf("pcivga_bell: unimplemented\n");		/* XXX */
}							/* XXX */

void
pcivga_cursor(id, row, col)
	void *id;
	int row, col;
{
	struct pcivga_devconfig *dc = id;
	int pos;

#if 0
	printf("pcivga_cursor: %d %d\n", row, col);
#endif
        /* turn the cursor off */
        if (row == -1 || col == -1) {
		dc->dc_crow = dc->dc_ccol = PCIVGA_CURSOR_OFF;

		/* XXX disable cursor??? */
        } else {
		dc->dc_crow = row;
		dc->dc_ccol = col;
        }

	pos = row * dc->dc_ncol + col;

	outb(dc->dc_iobase, 14);
	outb(dc->dc_iobase+1, pos >> 8);
	outb(dc->dc_iobase, 15);
	outb(dc->dc_iobase+1, pos);
}

void
pcivga_putstr(id, row, col, cp, len)
	void *id;
	int row, col;
	char *cp;
	int len;
{
	struct pcivga_devconfig *dc = id;
	char *dcp;
	int i;

	for (i = 0; i < len; i++, cp++) {
		dcp = (char *)&dc->dc_crtat[row * dc->dc_ncol + col];
#if 0
printf("*cp = %c, attr = 0x%x\n", *cp, dc->dc_so ? dc->dc_so_at : dc->dc_at);
printf("was: %c/", *dcp);
#endif
		*dcp++ = *cp;
#if 0
printf("0x%x\n", *dcp);
#endif
		*dcp++ = dc->dc_so ? dc->dc_so_at : dc->dc_at;
	}
}

void
pcivga_copycols(id, row, srccol, dstcol, ncols)
	void *id;
	int row, srccol, dstcol, ncols;
{
	struct pcivga_devconfig *dc = id;
	u_short *ssp, *dsp;
	int nclr;

#if 0
	printf("pcivga_copycols: row %d: %d, %d -> %d\n", row, srccol, ncols,
	    dstcol);
#endif
	ssp = &dc->dc_crtat[row * dc->dc_ncol + srccol];
	dsp = &dc->dc_crtat[row * dc->dc_ncol + dstcol];
	bcopy(ssp, dsp, ncols * sizeof(u_short));
}

void
pcivga_erasecols(id, row, startcol, ncols)
	void *id;
	int row, startcol, ncols;
{
	struct pcivga_devconfig *dc = id;
	u_short *ssp;
	int i;

#if 0
	printf("pcivga_erasecols: row %d: %d, %d\n", row, startcol, ncols);
#endif
	ssp = &dc->dc_crtat[row * dc->dc_ncol + startcol];
	for (i = 0; i < ncols; i++)
		*ssp++ = (dc->dc_at << 8) | ' ';
}

void
pcivga_copyrows(id, srcrow, dstrow, nrows)
	void *id;
	int srcrow, dstrow, nrows;
{
	struct pcivga_devconfig *dc = id;
	u_short *ssp, *dsp;
	int nclr;

#if 0
	printf("pcivga_copyrows: %d, %d -> %d\n", srcrow, nrows, dstrow);
#endif
	ssp = &dc->dc_crtat[srcrow * dc->dc_ncol + 0];
	dsp = &dc->dc_crtat[dstrow * dc->dc_ncol + 0];
	bcopy(ssp, dsp, nrows * dc->dc_ncol * sizeof(u_short));
}

void
pcivga_eraserows(id, startrow, nrows)
	void *id;
	int startrow, nrows;
{
	struct pcivga_devconfig *dc = id;
	u_short *ssp;
	int i;

#if 0
	printf("pcivga_eraserows: %d, %d\n", startrow, nrows);
#endif
	ssp = &dc->dc_crtat[startrow * dc->dc_ncol + 0];
	for (i = 0; i < nrows * dc->dc_ncol; i++)
		*ssp++ = (dc->dc_at << 8) | ' ';
}
