/*	$OpenBSD: glxsb.c,v 1.1 2006/10/26 08:37:14 tom Exp $	*/

/*
 * Copyright (c) 2006 Tom Cosgrove <tom@openbsd.org>
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
 * Driver for the security block on the AMD Geode LX processors
 * http://www.amd.com/files/connectivitysolutions/geode/geode_lx/33234d_lx_ds.pdf
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/types.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/pctr.h>

#include <dev/rndvar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define SB_GLD_MSR_CAP		0x58002000	/* RO - Capabilities */
#define SB_GLD_MSR_CONFIG	0x58002001	/* RW - Master Config */
#define SB_GLD_MSR_SMI		0x58002002	/* RW - SMI */
#define SB_GLD_MSR_ERROR	0x58002003	/* RW - Error */
#define SB_GLD_MSR_PM		0x58002004	/* RW - Power Mgmt */
#define SB_GLD_MSR_DIAG		0x58002005	/* RW - Diagnostic */
#define SB_GLD_MSR_CTRL		0x58002006	/* RW - Security Block Cntrl */

						/* For GLD_MSR_CTRL: */
#define SB_GMC_DIV0		0x0000		/* AES update divisor values */
#define SB_GMC_DIV1		0x0001
#define SB_GMC_DIV2		0x0002
#define SB_GMC_DIV3		0x0003
#define SB_GMC_DIV_MASK		0x0003
#define SB_GMC_SBI		0x0004		/* AES swap bits */
#define SB_GMC_SBY		0x0008		/* AES swap bytes */
#define SB_GMC_TW		0x0010		/* Time write (EEPROM) */
#define SB_GMC_T_SEL0		0x0000		/* RNG post-proc: none */
#define SB_GMC_T_SEL1		0x0100		/* RNG post-proc: LFSR */
#define SB_GMC_T_SEL2		0x0200		/* RNG post-proc: whitener */
#define SB_GMC_T_SEL3		0x0300		/* RNG LFSR+whitener */
#define SB_GMC_T_SEL_MASK	0x0300
#define SB_GMC_T_NE		0x0400		/* Noise (generator) Enable */
#define SB_GMC_T_TM		0x0800		/* RNG test mode */
						/*     (deterministic) */

/* Security Block configuration/control registers (offsets from base) */

#define SB_CTL_A		0x0000		/* RW - SB Control A */
#define SB_CTL_B		0x0004		/* RW - SB Control B */
#define SB_AES_INT		0x0008		/* RW - SB AES Interrupt */
#define SB_SOURCE_A		0x0010		/* RW - Source A */
#define SB_DEST_A		0x0014		/* RW - Destination A */
#define SB_LENGTH_A		0x0018		/* RW - Length A */
#define SB_SOURCE_B		0x0020		/* RW - Source B */
#define SB_DEST_B		0x0024		/* RW - Destination B */
#define SB_LENGTH_B		0x0028		/* RW - Length B */
#define SB_WKEY_0		0x0030		/* WO - Writable Key 0 */
#define SB_WKEY_1		0x0034		/* WO - Writable Key 1 */
#define SB_WKEY_2		0x0038		/* WO - Writable Key 2 */
#define SB_WKEY_3		0x003C		/* WO - Writable Key 3 */
#define SB_CBC_IV_0		0x0040		/* RW - CBC IV 0 */
#define SB_CBC_IV_1		0x0044		/* RW - CBC IV 1 */
#define SB_CBC_IV_2		0x0048		/* RW - CBC IV 2 */
#define SB_CBC_IV_3		0x004C		/* RW - CBC IV 3 */
#define SB_RANDOM_NUM		0x0050		/* RW - Random Number */
#define SB_RANDOM_NUM_STATUS	0x0054		/* RW - Random Number Status */
#define SB_EEPROM_COMM		0x0800		/* RW - EEPROM Command */
#define SB_EEPROM_ADDR		0x0804		/* RW - EEPROM Address */
#define SB_EEPROM_DATA		0x0808		/* RW - EEPROM Data */
#define SB_EEPROM_SEC_STATE	0x080C		/* RW - EEPROM Security State */

#define SB_RNS_TRNG_VALID	0x0001		/* in SB_RANDOM_NUM_STATUS */

#define SB_MEM_SIZE		0x0810		/* Size of memory block */

struct glxsb_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct timeout		sc_to;
};

int	glxsb_match(struct device *, void *, void *);
void	glxsb_attach(struct device *, struct device *, void *);
void	glxsb_rnd(void *);

struct cfattach glxsb_ca = {
	sizeof(struct glxsb_softc), glxsb_match, glxsb_attach
};

struct cfdriver glxsb_cd = {
	NULL, "glxsb", DV_DULL
};


int
glxsb_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AMD &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_GEODE_LX_CRYPTO)
		return (1);

	return (0);
}

void
glxsb_attach(struct device *parent, struct device *self, void *aux)
{
	struct glxsb_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	bus_addr_t membase;
	bus_size_t memsize;
	uint64_t msr;

	msr = rdmsr(SB_GLD_MSR_CAP);
	if ((msr & 0xFFFF00) != 0x130400) {
		printf(": unknown ID 0x%x\n", (int) ((msr & 0xFFFF00) >> 16));
		return;
	}

	/* printf(": revision %d", (int) (msr & 0xFF)); */

	/* Map in the security block configuration/control registers */
	if (pci_mapreg_map(pa, PCI_MAPREG_START,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0, &sc->sc_iot,
	    &sc->sc_ioh, &membase, &memsize, SB_MEM_SIZE)) {
		printf(": can't find mem space\n");
		return;
	}

	/*
	 * Configure the Security Block.
	 *
	 * We want to enable the noise generator (T_NE), and enable the
	 * linear feedback shift register and whitener post-processing
	 * (T_SEL = 3).  Also ensure that test mode (deterministic values)
	 * is disabled.
	 */
	msr = rdmsr(SB_GLD_MSR_CTRL);
	msr &= ~(SB_GMC_T_TM | SB_GMC_T_SEL_MASK);
	msr |= SB_GMC_T_NE | SB_GMC_T_SEL3;
	wrmsr(SB_GLD_MSR_CTRL, msr);

	/* Install a periodic collector for the "true" (AMD's word) RNG */
	timeout_set(&sc->sc_to, glxsb_rnd, sc);
	glxsb_rnd(sc);
	printf(": RNG\n");
}

void
glxsb_rnd(void *v)
{
	struct glxsb_softc *sc = v;
	uint32_t status, value;
	extern int hz;

	status = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SB_RANDOM_NUM_STATUS);
	if (status & SB_RNS_TRNG_VALID) {
		value = bus_space_read_4(sc->sc_iot, sc->sc_ioh, SB_RANDOM_NUM);
		add_true_randomness(value);
	}

	timeout_add(&sc->sc_to, (hz > 100) ? (hz / 100) : 1);
}
