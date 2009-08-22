/*	$OpenBSD: i80321var.h,v 1.4 2009/08/22 02:54:50 mk Exp $	*/
/*	$NetBSD: i80321var.h,v 1.10 2005/12/15 01:44:00 briggs Exp $	*/

/*
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
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

#ifndef _ARM_XSCALE_I80321VAR_H_
#define	_ARM_XSCALE_I80321VAR_H_

#include <sys/queue.h>
#include <sys/gpio.h>
#include <sys/evcount.h>
#include <dev/pci/pcivar.h>
#include <dev/gpio/gpiovar.h>

/*
 * There are roughly 32 interrupt sources.
 */
#define	NIRQ		32

struct intrhand {
	TAILQ_ENTRY(intrhand) ih_list;	/* link on intrq list */
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_ipl;			/* IPL_* */
	int ih_irq;			/* IRQ number */
	struct evcount  	ih_count;
	const char *ih_name;
};

struct intrq {
	TAILQ_HEAD(, intrhand) iq_list;	/* handler list */
	int iq_irq;			/* IRQ to mask while handling */
	int iq_levels;			/* IPL_*'s this IRQ has */
	int iq_ist;			/* share type */
};

struct config_bus_space {
	u_int32_t bus_base;
	u_int32_t bus_size;
	int bus_io;
};

struct i80321_softc {
	struct device sc_dev;		/* generic device glue */

	int sc_is_host;			/* indicates if we're a host or
					   plugged into another host */

	/*
	 * This is the bus_space and handle used to access the
	 * i80321 itself.  This is filled in by the board-specific
	 * front-end.
	 */
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;

	/* Handles for the various subregions. */
	bus_space_handle_t sc_atu_sh;
	bus_space_handle_t sc_mcu_sh;

#ifdef BULLSHIT
	/*
	 * We expect the board-specific front-end to have already mapped
	 * the PCI I/O space .. it is only 64K, and I/O mappings tend to
	 * be smaller than a page size, so it's generally more efficient
	 * to map them all into virtual space in one fell swoop.
	 */
	vaddr_t	sc_iow_vaddr;		/* I/O window vaddr */
#else
	bus_space_handle_t sc_io_sh;
#endif

	/*
	 * Variables that define the Inbound windows.  The base address of
	 * 0-2 are configured by a host via BARs.  The xlate variable
	 * defines the start of the local address space that it maps to.
	 * The size variable defines the byte size.
	 *
	 * The first 3 windows are for incoming PCI memory read/write
	 * cycles from a host.  The 4th window, not configured by the
	 * host (as it outside the normal BAR range) is the inbound
	 * window for PCI devices controlled by the i80321.
	 */
	struct {
		uint32_t iwin_base_hi;
		uint32_t iwin_base_lo;
		uint32_t iwin_xlate;
		uint32_t iwin_size;
	} sc_iwin[4];

	/*
	 * Variables that define the Outbound windows.
	 */
	struct {
		uint32_t owin_xlate_lo;
		uint32_t owin_xlate_hi;
	} sc_owin[2];

	/*
	 * This is the PCI address that the Outbound I/O window maps to.
         * The offset is to keep the actual used I/O address away from 0,
	 * which can be bad if, say, an i8254x gig-e chip gets mapped there.
	 * The 0 value apparently looks like "unconfigured" to the controller
	 * and it ignores writes to that region (it doesn't cause a bus fault,
	 * it just ignores them--leading to a non-functional controller).  The
	 * wm(4) driver usually uses memory-mapped regions, but does use the
	 * I/O-mapped region for reset operations in order to work around a
	 * bug in the chip.
	 * Iyonix, while using sc_ioout_xlate 0 needs an offset of 0, too, in
	 * order to function properly.  These values are both set in the
	 * port-specific i80321_mainbus_attach() routine.
	 */
	uint32_t sc_ioout_xlate;
	uint32_t sc_ioout_xlate_offset;

	/* Bus space, DMA, and PCI tags for the PCI bus (private devices). */
	struct bus_space sc_pci_iot;
	struct bus_space sc_pci_memt;
	struct arm32_bus_dma_tag sc_pci_dmat;
	struct arm32_pci_chipset sc_pci_chipset;

	/* DMA window info for PCI DMA. */
	struct arm32_dma_range sc_pci_dma_range;

	/* GPIO state */
	uint8_t sc_gpio_dir;	/* GPIO pin direction (1 == output) */
	uint8_t sc_gpio_val;	/* GPIO output pin value */

#define I80219_GPIO_NPINS 8
	/* GPIO for 80219 -XXX */
	struct gpio_chipset_tag sc_gpio_gc;
        struct gpio_pin sc_gpio_pins[I80219_GPIO_NPINS];

	/* DMA tag for local devices. */
	struct arm32_bus_dma_tag sc_local_dmat;

	/* Structures to do bus fixup */
	int nbogus;
	struct extent *extent_mem;
	struct extent *extent_port;
	struct config_bus_space sc_membus_space;
	struct config_bus_space sc_iobus_space;


};

/*
 * Arguments used to attach IOP built-ins.
 */
struct iopxs_attach_args {
	const char *ia_name;	/* name of device */
	bus_space_tag_t ia_st;	/* space tag */
	bus_space_handle_t ia_sh;/* handle of IOP base */
	bus_dma_tag_t ia_dmat;	/* DMA tag */
	bus_addr_t ia_offset;	/* offset of device from IOP base */
	bus_size_t ia_size;	/* size of sub-device */
};

extern struct bus_space i80321_bs_tag;
extern struct i80321_softc *i80321_softc;

extern void (*i80321_hardclock_hook)(void);

void	i80321_sdram_bounds(bus_space_tag_t, bus_space_handle_t,
	    paddr_t *, psize_t *);

void	i80321_calibrate_delay(void);

void	i80321intc_init(void);
void	i80321intc_intr_init(void);
void	*i80321intc_establish(int, int, int (*)(void *), void *, char *);
void	i80321intc_disestablish(void *);

void	i80321_gpio_set_direction(uint8_t, uint8_t);
void	i80321_gpio_set_val(uint8_t, uint8_t);
uint8_t	i80321_gpio_get_val(void);

void	i80321_bs_init(bus_space_tag_t, void *);
void	i80321_io_bs_init(bus_space_tag_t, void *);
void	i80321_mem_bs_init(bus_space_tag_t, void *);

void	i80321_local_dma_init(struct i80321_softc *sc);

void	i80321_pci_init(pci_chipset_tag_t, void *);

void	i80321_attach(struct i80321_softc *);
#endif /* _ARM_XSCALE_I80321VAR_H_ */
