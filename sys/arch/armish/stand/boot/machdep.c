/*	$OpenBSD: machdep.c,v 1.4 2007/11/24 12:59:28 jmc Exp $	*/

/*
 * Copyright (c) 2006 Mark Kettenis
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

#include <sys/types.h>
#include <arm/pte.h>
#include <dev/pci/pcireg.h>

#include "libsa.h"

#define L1_IDX(va)	(((uint32_t)(va)) >> L1_S_SHIFT)

#define ATU_OIOWTVR	0xffffe15c
#define ATU_ATUCR	0xffffe180
#define ATU_PCSR	0xffffe184
#define ATU_OCCAR	0xffffe1a4
#define ATU_OCCDR	0xffffe1ac

#define ATUCR_OUT_EN	(1U << 1)

#define	PCSR_RIB	(1U << 5)
#define	PCSR_RPB	(1U << 4)

void
machdep(void)
{
	uint32_t *pde;
	uint32_t va;

	/*
	 * Clean up the mess that RedBoot left us in, amd make sure we
	 * can access the PCI bus.
	 */

	*((volatile uint32_t *)(ATU_ATUCR)) = ATUCR_OUT_EN;

        __asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r" (pde));
	pde = (uint32_t *)((uint32_t)pde & 0x0fffffff);

	va = *((volatile uint32_t *)(ATU_OIOWTVR));
	pde[L1_IDX(va)] = (va & L1_ADDR_BITS) | L1_S_AP(AP_KRWUR) | L1_TYPE_S;

	/* Start timer */
	__asm volatile ("mcr p6, 0, %0, c3, c1, 0" :: "r" (0xffffffff));
	__asm volatile ("mcr p6, 0, %0, c1, c1, 0" :: "r" (0x00000032));

	cninit();

{
	/*
	 * this code does a device probe on pci space, 
	 * It looks for a wd compatible controller.
	 * however when it reads the device register, it does
	 * not check if a bus fault occurs on the access.
	 * Since the bootloader doesn't handle faults, this
	 * crashes the bootloader if it reads a non-existent
	 * device.
	 * The tag computation comes from arm/xscale/i80321_pci.c
	 * i80321_pci_conf_setup()
	 */
	int device, bar;
	for (device = 1; device < 4; device++) {
		u_int32_t tag, result, size;
		volatile u_int32_t *occar =  (u_int32_t *)ATU_OCCAR;
		volatile u_int32_t *occdr =  (u_int32_t *)ATU_OCCDR;

		tag =  1 << (device + 16) | (device << 11);
		*occar =  tag;
		result = *occdr;
		if (result == ~0)
			continue;
		*occar =  tag | PCI_CLASS_REG;
		result = *occdr;

		if (PCI_CLASS(result) != PCI_CLASS_MASS_STORAGE)
			continue;
		if (PCI_SUBCLASS(result) != PCI_SUBCLASS_MASS_STORAGE_ATA &&
		    PCI_SUBCLASS(result) != PCI_SUBCLASS_MASS_STORAGE_SATA &&
		    PCI_SUBCLASS(result) != PCI_SUBCLASS_MASS_STORAGE_MISC)
			continue;

		*occar =  tag | PCI_MAPREG_START;
		result = *occdr;

		/* verify result is an IO BAR */
		if (PCI_MAPREG_TYPE(result) == PCI_MAPREG_TYPE_IO) {
			extern u_int32_t wdc_base_addr;
			wdc_base_addr = PCI_MAPREG_MEM_ADDR(result);
			DPRINTF(("setting wdc_base addr to %x\n",
			    wdc_base_addr));
		}
	}
}

}

int
main(void)
{
	boot(0);
	return 0;
}

void
_rtt(void)
{
	*((volatile uint32_t *)(ATU_PCSR)) = PCSR_RIB | PCSR_RPB;

	printf("RESET FAILED\n");
	for (;;) ;
}
