/*	$OpenBSD: footbridge.c,v 1.4 2004/08/17 19:40:45 drahn Exp $	*/
/*	$NetBSD: footbridge.c,v 1.7 2002/05/16 01:01:33 thorpej Exp $	*/

/*
 * Copyright (c) 1997,1998 Mark Brinicombe.
 * Copyright (c) 1997,1998 Causality Limited
 * All rights reserved.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <dev/pci/pcivar.h>
#define _ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/cpuconf.h>
#include <arm/cpufunc.h>

#include <arm/mainbus/mainbus.h>
#include <arm/footbridge/footbridgevar.h>
#include <arm/footbridge/dc21285reg.h>
#include <arm/footbridge/dc21285mem.h>
#include <arm/footbridge/footbridge.h>
 
/*
 * DC21285 'Footbridge' device
 *
 * This probes and attaches the footbridge device
 * It then configures any children
 */

/* Declare prototypes */

static int footbridge_match	(struct device *parent, void *cf,
	                             void *aux);
static void footbridge_attach	(struct device *parent, struct device *self,
        	                     void *aux);
static int footbridge_print	(void *aux, const char *pnp);
static int footbridge_intr	(void *arg);

/* Driver and attach structures */
struct cfattach footbridge_ca = {
	sizeof(struct footbridge_softc), footbridge_match, footbridge_attach
};

struct cfdriver footbridge_cd = {
	NULL, "footbridge", DV_DULL
};

/* Various bus space tags */
extern struct bus_space footbridge_bs_tag;
extern void footbridge_create_io_bs_tag(bus_space_tag_t t, void *cookie);
extern void footbridge_create_mem_bs_tag(bus_space_tag_t t, void *cookie);
struct bus_space footbridge_csr_tag;
struct bus_space footbridge_pci_io_bs_tag;
struct bus_space footbridge_pci_mem_bs_tag;
extern struct arm32_pci_chipset footbridge_pci_chipset;
extern struct arm32_bus_dma_tag footbridge_pci_bus_dma_tag;

/* Used in footbridge_clock.c */
struct footbridge_softc *clock_sc;

/* Set to non-zero to enable verbose reporting of footbridge system ints */
int footbridge_intr_report = 0;

int footbridge_found;

void
footbridge_pci_bs_tag_init(void)
{
	/* Set up the PCI bus tags */
	footbridge_create_io_bs_tag(&footbridge_pci_io_bs_tag,
	    (void *)DC21285_PCI_IO_VBASE);
	footbridge_create_mem_bs_tag(&footbridge_pci_mem_bs_tag,
	    (void *)DC21285_PCI_MEM_BASE);
}

/*
 * int footbridgeprint(void *aux, const char *name)
 *
 * print configuration info for children
 */

static int
footbridge_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	union footbridge_attach_args *fba = aux;

	if (pnp)
		printf("%s at %s", fba->fba_name, pnp);
	if (strcmp(fba->fba_name, "pci") == 0)
		printf(" bus %d", fba->fba_pba.pba_bus);
	return(UNCONF);
}

/*
 * int footbridge_match(struct device *parent, struct cfdata *cf, void *aux)
 *
 * Just return ok for this if it is device 0
 */ 
 
static int
footbridge_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;
	struct cfdata *cf = (struct cfdata *)vcf;

	return (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0);
}


/*
 * void footbridge_attach(struct device *parent, struct device *dev, void *aux)
 *
 */
  
static void
footbridge_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct footbridge_softc *sc = (struct footbridge_softc *)self;
	union footbridge_attach_args fba;
	int vendor, device, rev;

	/* There can only be 1 footbridge. */
	footbridge_found = 1;

	clock_sc = sc;

	sc->sc_iot = &footbridge_bs_tag;

	/* Map the Footbridge */
	if (bus_space_map(sc->sc_iot, DC21285_ARMCSR_VBASE,
	     DC21285_ARMCSR_VSIZE, 0, &sc->sc_ioh))
		panic("%s: Cannot map registers", self->dv_xname);

	/* Read the ID to make sure it is what we think it is */
	vendor = bus_space_read_2(sc->sc_iot, sc->sc_ioh, VENDOR_ID);
	device = bus_space_read_2(sc->sc_iot, sc->sc_ioh, DEVICE_ID);
	rev = bus_space_read_1(sc->sc_iot, sc->sc_ioh, REVISION);
	if (vendor != DC21285_VENDOR_ID && device != DC21285_DEVICE_ID)
		panic("%s: Unrecognised ID", self->dv_xname);

	printf(": DC21285 rev %d\n", rev);

	/* Disable all interrupts from the footbridge */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, IRQ_ENABLE_CLEAR, 0xffffffff);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FIQ_ENABLE_CLEAR, 0xffffffff);

/*	bus_space_write_4(sc->sc_iot, sc->sc_ioh, 0x18, 0x40000000);*/

	/* Install a generic handler to catch a load of system interrupts */
	sc->sc_serr_ih = footbridge_intr_claim(IRQ_SERR, IPL_HIGH,
	    "serr", footbridge_intr, sc);
	sc->sc_sdram_par_ih = footbridge_intr_claim(IRQ_SDRAM_PARITY, IPL_HIGH,
	    "sdram_parity", footbridge_intr, sc);
	sc->sc_data_par_ih = footbridge_intr_claim(IRQ_DATA_PARITY, IPL_HIGH,
	    "data_parity", footbridge_intr, sc);
	sc->sc_master_abt_ih = footbridge_intr_claim(IRQ_MASTER_ABORT, IPL_HIGH,
	    "mast_abt", footbridge_intr, sc);
	sc->sc_target_abt_ih = footbridge_intr_claim(IRQ_TARGET_ABORT, IPL_HIGH,
	    "targ_abt", footbridge_intr, sc);
	sc->sc_parity_ih = footbridge_intr_claim(IRQ_PARITY, IPL_HIGH,
	    "parity", footbridge_intr, sc);
	
	/* Set up the PCI bus tags */
	footbridge_create_io_bs_tag(&footbridge_pci_io_bs_tag,
	    (void *)DC21285_PCI_IO_VBASE);
	footbridge_create_mem_bs_tag(&footbridge_pci_mem_bs_tag,
	    (void *)DC21285_PCI_MEM_BASE);

	/* calibrate the delay loop */
	calibrate_delay();
	/* Attach the PCI bus */
	fba.fba_pba.pba_busname = "pci";
	fba.fba_pba.pba_pc = &footbridge_pci_chipset;
	fba.fba_pba.pba_iot = &footbridge_pci_io_bs_tag;
	fba.fba_pba.pba_memt = &footbridge_pci_mem_bs_tag;
	fba.fba_pba.pba_dmat = &footbridge_pci_bus_dma_tag;
	/*
	fba.fba_pba.pba_flags = PCI_FLAGS_IO_ENABLED | PCI_FLAGS_MEM_ENABLED;
	*/
	fba.fba_pba.pba_bus = 0;
	config_found(self, &fba.fba_pba, footbridge_print);

	/* Attach uart device */
	fba.fba_fca.fca_name = "fcom";
	fba.fba_fca.fca_iot = sc->sc_iot;
	fba.fba_fca.fca_ioh = sc->sc_ioh;
	fba.fba_fca.fca_rx_irq = IRQ_SERIAL_RX;
	fba.fba_fca.fca_tx_irq = IRQ_SERIAL_TX;
	config_found(self, &fba.fba_fca, footbridge_print); 
	
	/* Setup fast SA110 cache clean area */
#ifdef CPU_SA110
	if (cputype == CPU_ID_SA110)
		footbridge_sa110_cc_setup();
#endif	/* CPU_SA110 */

}

/* Generic footbridge interrupt handler */

int
footbridge_intr(arg)
	void *arg;
{
	struct footbridge_softc *sc = arg;
	u_int ctrl, intr;

	/*
	 * Read the footbridge control register and check for
	 * SERR and parity errors
	 */
	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SA_CONTROL);
	intr = ctrl & (RECEIVED_SERR | SA_SDRAM_PARITY_ERROR |
	    PCI_SDRAM_PARITY_ERROR | DMA_SDRAM_PARITY_ERROR);
	if (intr) {
		/* Report the interrupt if reporting is enabled */
		if (footbridge_intr_report)
			printf("footbridge_intr: ctrl=%08x\n", intr);
		/* Clear the interrupt state */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, SA_CONTROL,
		    ctrl | intr);
	}
	/*
	 * Read the PCI status register and check for errors
	 */
	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_ioh, PCI_COMMAND_STATUS_REG);
	intr = ctrl & (PCI_STATUS_PARITY_ERROR | PCI_STATUS_MASTER_TARGET_ABORT
	    | PCI_STATUS_MASTER_ABORT | PCI_STATUS_SPECIAL_ERROR
	    | PCI_STATUS_PARITY_DETECT);
	if (intr) {
		/* Report the interrupt if reporting is enabled */
		if (footbridge_intr_report)
			printf("footbridge_intr: pcistat=%08x\n", intr);
		/* Clear the interrupt state */
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    PCI_COMMAND_STATUS_REG, ctrl | intr);
	}
	return(intr != 0);
}

/* End of footbridge.c */
