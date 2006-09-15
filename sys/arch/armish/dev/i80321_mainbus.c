/*	$OpenBSD: i80321_mainbus.c,v 1.10 2006/09/15 23:36:11 drahn Exp $ */
/*	$NetBSD: i80321_mainbus.c,v 1.16 2005/12/15 01:44:00 briggs Exp $ */

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IQ80321 front-end for the i80321 I/O Processor.  We take care
 * of setting up the i80321 memory map, PCI interrupt routing, etc.,
 * which are all specific to the board the i80321 is wired up to.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>

#include <machine/bus.h>

#include <arm/mainbus/mainbus.h>
#include <armish/dev/iq80321reg.h>
#include <armish/dev/iq80321var.h>

#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/gpio/gpiovar.h>

#include "gpio.h"

int	i80321_mainbus_match(struct device *, void *, void *);
void	i80321_mainbus_attach(struct device *, struct device *, void *);

void i80321_gpio_init (struct i80321_softc *sc);
int i80321_gpio_pin_read (void *, int);
void i80321_gpio_pin_write (void *, int, int);
void i80321_gpio_pin_ctl (void *, int, int);

/* XXX */
#define I80219_REG_GPOE	0x7C4
#define I80219_REG_GPID 0x7C8
#define I80219_REG_GPOD 0x7CC

struct cfattach iopxs_mainbus_ca = {
	sizeof(struct i80321_softc), i80321_mainbus_match,
	i80321_mainbus_attach
};

struct cfdriver iopxs_cd = {
	NULL, "iopxs", DV_DULL
};


/* There can be only one. */
int	i80321_mainbus_found;

int
i80321_mainbus_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	struct cfdata *cf = match;

	if (i80321_mainbus_found)
		return (0);

	if (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0)
		return (1);

	return (0);
}

/* XXX */
bus_space_handle_t sc_pld_sh;

void
i80321_mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct i80321_softc *sc = (void *) self;
        struct gpiobus_attach_args gba;
	pcireg_t b0u, b0l, b1u, b1l;
	paddr_t memstart;
	psize_t memsize;
	pcireg_t atumembase;
	pcireg_t atuiobase;

	i80321_mainbus_found = 1;

	/*
	 * Fill in the space tag for the i80321's own devices,
	 * and hand-craft the space handle for it (the device
	 * was mapped during early bootstrap).
	 */
	i80321_bs_init(&i80321_bs_tag, sc);
	sc->sc_st = &i80321_bs_tag;
	if (bus_space_map(sc->sc_st, VERDE_PMMR_BASE, VERDE_PMMR_SIZE, 0,
	    &sc->sc_sh))
		panic("%s: unable to map VERDE registers", sc->sc_dev.dv_xname);

	/*
	 * Slice off a subregion for the Memory Controller -- we need it
	 * here in order read the memory size.
	 */
	if (bus_space_subregion(sc->sc_st, sc->sc_sh, VERDE_MCU_BASE,
	    VERDE_MCU_SIZE, &sc->sc_mcu_sh))
		panic("%s: unable to subregion MCU registers",
		    sc->sc_dev.dv_xname);

	if (bus_space_subregion(sc->sc_st, sc->sc_sh, VERDE_ATU_BASE,
	    VERDE_ATU_SIZE, &sc->sc_atu_sh))
		panic("%s: unable to subregion ATU registers",
		    sc->sc_dev.dv_xname);

	if(bus_space_map(sc->sc_st, VERDE_OUT_XLATE_IO_WIN0_BASE,
	    VERDE_OUT_XLATE_IO_WIN_SIZE, 0, &sc->sc_io_sh))
		panic("%s: unable to map IOW registers", sc->sc_dev.dv_xname);

//	printf ("PIRSR %x\n", bus_space_read_4(sc->sc_st, sc->sc_sh, ICU_PIRSR));

//	printf("mapping bus io to %x - %x\n", sc->sc_io_sh, sc->sc_io_sh+VERDE_OUT_XLATE_IO_WIN_SIZE);

	/*
	 * Initialize the interrupt part of our PCI chipset tag.
	 */
	iq80321_pci_init(&sc->sc_pci_chipset, sc);

	/* Initialize the PCI chipset tag. */
	i80321_pci_init(&sc->sc_pci_chipset, sc);

	iq80321_pci_init2(&sc->sc_pci_chipset, sc);

	atumembase = bus_space_read_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_MAPREG_START + 0x08);
	atuiobase = bus_space_read_4(sc->sc_st, sc->sc_atu_sh,
	    ATU_OIOWTVR);

	sc->sc_membus_space.bus_base = PCI_MAPREG_MEM_ADDR(atumembase);
	sc->sc_membus_space.bus_size = 0x04000000;
	sc->sc_iobus_space.bus_base = PCI_MAPREG_IO_ADDR(atuiobase);
	sc->sc_iobus_space.bus_size = 0x00010000;
	pci_addr_fixup(sc, 2/*XXX*/);

	/*
	 * Check the configuration of the ATU to see if another BIOS
	 * has configured us.  If a PC BIOS didn't configure us, then:
	 *	IQ80321: BAR0 00000000.0000000c BAR1 is 00000000.8000000c.
	 *	IQ31244: BAR0 00000000.00000004 BAR1 is 00000000.0000000c.
	 * If a BIOS has configured us, at least one of those should be
	 * different.  This is pretty fragile, but it's not clear what
	 * would work better.
	 */
	b0l = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, PCI_MAPREG_START+0x0);
	b0u = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, PCI_MAPREG_START+0x4);
	b1l = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, PCI_MAPREG_START+0x8);
	b1u = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, PCI_MAPREG_START+0xc);
	b0l &= PCI_MAPREG_MEM_ADDR_MASK;
	b0u &= PCI_MAPREG_MEM_ADDR_MASK;
	b1l &= PCI_MAPREG_MEM_ADDR_MASK;
	b1u &= PCI_MAPREG_MEM_ADDR_MASK;

	printf(": i80321 I/O Processor\n");

	i80321_sdram_bounds(sc->sc_st, sc->sc_mcu_sh, &memstart, &memsize);

	/*
	 * We set up the Inbound Windows as follows:
	 *
	 *	0	Access to i80321 PMMRs
	 *
	 *	1	Reserve space for private devices
	 *
	 *	2	RAM access
	 *
	 *	3	Unused.
	 *
	 * This chunk needs to be customized for each IOP321 application.
	 */

	atumembase = bus_space_read_4(sc->sc_st, sc->sc_atu_sh,
	    PCI_MAPREG_START + 0x08);

	if (atumembase == 0x8000000c) {
		/* iodata: intel std config */

		/* map device registers */
		sc->sc_iwin[0].iwin_base_lo =	0x00000004;
		sc->sc_iwin[0].iwin_base_hi =	0x00000000;
		sc->sc_iwin[0].iwin_xlate = 	0xff000000;
		sc->sc_iwin[0].iwin_size = 	0x01000000;
		
		/* Map PCI:Local 1:1. */
		sc->sc_iwin[1].iwin_base_lo = VERDE_OUT_XLATE_MEM_WIN0_BASE |
		    PCI_MAPREG_MEM_PREFETCHABLE_MASK |
		    PCI_MAPREG_MEM_TYPE_64BIT;
		sc->sc_iwin[1].iwin_base_hi = 0;

		sc->sc_iwin[1].iwin_xlate = VERDE_OUT_XLATE_MEM_WIN0_BASE;
		sc->sc_iwin[1].iwin_size = VERDE_OUT_XLATE_MEM_WIN_SIZE;


		sc->sc_iwin[2].iwin_base_lo = memstart |
		    PCI_MAPREG_MEM_PREFETCHABLE_MASK |
		    PCI_MAPREG_MEM_TYPE_64BIT;
		sc->sc_iwin[2].iwin_base_hi = 0;

		sc->sc_iwin[2].iwin_xlate = memstart;
		sc->sc_iwin[2].iwin_size = memsize;

		sc->sc_iwin[3].iwin_base_lo = 0;
#if 0
		    PCI_MAPREG_MEM_PREFETCHABLE_MASK |
		    PCI_MAPREG_MEM_TYPE_64BIT;
#endif

		sc->sc_iwin[3].iwin_base_hi = 0;
		sc->sc_iwin[3].iwin_xlate = 0;
		sc->sc_iwin[3].iwin_size = 0;

		/*
		 * We set up the Outbound Windows as follows:
		 *
		 *	0	Access to private PCI space.
		 *
		 *	1	Unused.
		 */
		sc->sc_owin[0].owin_xlate_lo =
		    PCI_MAPREG_MEM_ADDR(sc->sc_iwin[1].iwin_base_lo);
		sc->sc_owin[0].owin_xlate_hi = sc->sc_iwin[1].iwin_base_hi;

		/*
		 * Set the Secondary Outbound I/O window to map
		 * to PCI address 0 for all 64K of the I/O space.
		 */
		sc->sc_ioout_xlate = 0x90000000;
		sc->sc_ioout_xlate_offset = 0x1000;
	} else if (atumembase == 0x40000004) {
		/* thecus */

		/* dont map device registers */
		sc->sc_iwin[0].iwin_base_lo = 0;
		sc->sc_iwin[0].iwin_base_hi = 0;
		sc->sc_iwin[0].iwin_xlate = 0;
		sc->sc_iwin[0].iwin_size = 0;

		/* Map PCI:Local 1:1. */
		sc->sc_iwin[1].iwin_base_lo = 0x40000000 |
	#if 0
		    PCI_MAPREG_MEM_PREFETCHABLE_MASK |
		    PCI_MAPREG_MEM_TYPE_64BIT;
	#else
		    0;
	#endif
		sc->sc_iwin[1].iwin_base_hi = 0;

		sc->sc_iwin[1].iwin_xlate = 0;
		sc->sc_iwin[1].iwin_size = 0x08000000;

		sc->sc_iwin[2].iwin_base_lo = 0 |
		    PCI_MAPREG_MEM_PREFETCHABLE_MASK |
		    PCI_MAPREG_MEM_TYPE_64BIT;
		sc->sc_iwin[2].iwin_base_hi = 0;

		sc->sc_iwin[2].iwin_xlate = memstart;
		sc->sc_iwin[2].iwin_size = memsize;

		sc->sc_iwin[3].iwin_base_lo = 0;
		sc->sc_iwin[3].iwin_base_hi = 0;
		sc->sc_iwin[3].iwin_xlate = 0;
		sc->sc_iwin[3].iwin_size = 0;

		/*
		 * We set up the Outbound Windows as follows:
		 *
		 *	0	Access to private PCI space.
		 *
		 *	1	Unused.
		 */
		sc->sc_owin[0].owin_xlate_lo =
		    PCI_MAPREG_MEM_ADDR(sc->sc_iwin[1].iwin_base_lo);
		sc->sc_owin[0].owin_xlate_hi = sc->sc_iwin[1].iwin_base_hi;

		/*
		 * Set the Secondary Outbound I/O window to map
		 * to PCI address 0 for all 64K of the I/O space.
		 */
		sc->sc_ioout_xlate = 0x90000000;
		sc->sc_ioout_xlate_offset = 0x1000;

	}

	i80321_attach(sc);

	i80321_gpio_init (sc);

	/* if 80219 */ {
		gba.gba_name = "gpio";
		gba.gba_gc = &sc->sc_gpio_gc;
		gba.gba_pins = sc->sc_gpio_pins;
		gba.gba_npins = I80219_GPIO_NPINS;
#if NGPIO > 0
		config_found(&sc->sc_dev, &gba, gpiobus_print);
#endif
	}
	{
#define I80321_PLD 0xfe8d0000UL
#define I80321_PLD_SIZE 0x1000

#define	PLD_LED		0
#define	PLD_PLED	1
#define	PLD_BTN		2
#define	PLD_INTEN	3
#define	PLD_PWRMNG	4

#if 0
	uint8_t val;
#endif

		if (bus_space_map(sc->sc_st, I80321_PLD, I80321_PLD_SIZE, 0,
		    /* &sc->sc_pld_sh */ &sc_pld_sh))
			panic("%s: unable to map PLD registers",
			    sc->sc_dev.dv_xname);

#if 0
		printf("dlectl %x\n", bus_space_read_1(sc->sc_st, sc_pld_sh,
		    PLD_LED));
		val = bus_space_read_1(sc->sc_st, sc_pld_sh, PLD_LED);
		val |= 0x3;
		bus_space_write_1(sc->sc_st, sc_pld_sh, PLD_LED, val);
		printf("dlectl %x\n", bus_space_read_1(sc->sc_st, sc_pld_sh,
		    PLD_PLED));
		printf("dlectl %x\n", bus_space_read_1(sc->sc_st, sc_pld_sh,
		    PLD_BTN));
#endif
	}
	{
		extern struct cfdriver pcaled_cd;
		void pcaled_gpio_pin_write(void *arg, int pin, int value);
		if (pcaled_cd.cd_ndevs > 0 && pcaled_cd.cd_devs[0] != NULL) {
			pcaled_gpio_pin_write(pcaled_cd.cd_devs[0], 13, 0);
			pcaled_gpio_pin_write(pcaled_cd.cd_devs[0], 14, 0);
		}
	}
}

void
i80321_gpio_init (struct i80321_softc *sc)
{
	int i;
	for (i = 0; i < I80219_GPIO_NPINS; i++) {
		sc->sc_gpio_pins[i].pin_num = i;
		sc->sc_gpio_pins[i].pin_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

		sc->sc_gpio_pins[i].pin_flags =
		    bus_space_read_4(sc->sc_st, sc->sc_sh, I80219_REG_GPOE) &
		    (1 << i) ? GPIO_PIN_INPUT :  GPIO_PIN_OUTPUT;
		sc->sc_gpio_pins[i].pin_state = i80321_gpio_pin_read(sc, i) ?
		    GPIO_PIN_HIGH : GPIO_PIN_LOW;
	}
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = i80321_gpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = i80321_gpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = i80321_gpio_pin_ctl;
}

int
i80321_gpio_pin_read (void *arg, int pin)
{
	struct i80321_softc *sc = arg;
	u_int32_t regval;
	int reg;

	if (bus_space_read_4(sc->sc_st, sc->sc_sh, I80219_REG_GPOE)
	    & (1 << pin)) {
		reg = I80219_REG_GPID;
	} else {
		reg = I80219_REG_GPOD;
	}
	regval =  bus_space_read_4(sc->sc_st, sc->sc_sh, reg);
#if 0
	printf("read %x gpio %x\n", reg);
	printf("gpio state %x %x %x\n",
	    bus_space_read_4(sc->sc_st, sc->sc_sh, I80219_REG_GPID),
	    bus_space_read_4(sc->sc_st, sc->sc_sh, I80219_REG_GPOD),
	    bus_space_read_4(sc->sc_st, sc->sc_sh, I80219_REG_GPOE));
#endif
	return ((regval >> pin) & 1);
}

void
i80321_gpio_pin_write (void *arg, int pin, int value)
{
	struct i80321_softc *sc = arg;
	u_int32_t regval;

	regval =  bus_space_read_4(sc->sc_st, sc->sc_sh, I80219_REG_GPOD);
	regval = (regval & ~(1 << pin)) | ((value & 1) << pin);
#if 0
	printf("writing %x to gpioO %x\n", regval);
#endif
	bus_space_write_4(sc->sc_st, sc->sc_sh, I80219_REG_GPOD, regval);
}
void
i80321_gpio_pin_ctl (void *arg, int pin, int flags)
{
	struct i80321_softc *sc = arg;
	u_int32_t regval;
	int value = (flags == GPIO_PIN_INPUT) ? 1 : 0;

	regval =  bus_space_read_4(sc->sc_st, sc->sc_sh, I80219_REG_GPOE);
	regval = (regval & ~(1 << pin)) | ((value & 1) << pin);
#if 0
	printf("writing %x to ctl %x\n", regval, value);
#endif
	bus_space_write_4(sc->sc_st, sc->sc_sh, I80219_REG_GPOE, regval);
}


void board_reset(void); /* XXX */
void
board_reset()
{
	struct i80321_softc *sc = i80321_softc;
	uint32_t val;

	printf("attempting reset\n");
	val = bus_space_read_4(sc->sc_st, sc->sc_sh, 0x7CC);
	val &=  ~0x10;
	bus_space_write_4(sc->sc_st, sc->sc_sh, 0x7CC, val);
	val = bus_space_read_4(sc->sc_st, sc->sc_sh, 0x7C4);
	val &=  ~0x10;
	bus_space_write_4(sc->sc_st, sc->sc_sh, 0x7C4, val);

	bus_space_write_1(sc->sc_st, sc_pld_sh, PLD_PWRMNG, 0x2);

}

void board_powerdown(void); /* XXX */
void
board_powerdown(void)
{
	void pcaled_gpio_pin_write(void *arg, int pin, int value);
	extern struct cfdriver pcaled_cd;

	if (pcaled_cd.cd_ndevs > 0 && pcaled_cd.cd_devs[0] != NULL) {
		pcaled_gpio_pin_write(pcaled_cd.cd_devs[0], 8, 1);
		delay(500000);
	}
}
