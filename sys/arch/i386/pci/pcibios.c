/*	$OpenBSD: pcibios.c,v 1.2 2000/03/27 08:35:22 brad Exp $	*/
/*	$NetBSD: pcibios.c,v 1.1 1999/11/17 01:16:37 thorpej Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1999, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/*
 * Interface to the PCI BIOS and PCI Interrupt Routing table.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/isa/isareg.h>
#include <i386/isa/isa_machdep.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <i386/pci/pcibios.h>
#ifdef PCIBIOS_INTR_FIXUP
#include <i386/pci/pci_intr_fixup.h>
#endif
#ifdef PCIBIOS_BUS_FIXUP
#include <i386/pci/pci_bus_fixup.h>
#endif

#ifdef __NetBSD__
#include <machine/bios32.h>
#elif __OpenBSD__
#include <machine/biosvar.h>
#endif

int pcibios_present;

struct pcibios_pir_header pcibios_pir_header;
struct pcibios_intr_routing *pcibios_pir_table;
int pcibios_pir_table_nentries;
int pcibios_max_bus;

struct bios32_entry pcibios_entry;

void	pcibios_pir_init __P((void));

int	pcibios_get_status __P((u_int32_t *, u_int32_t *, u_int32_t *,
	    u_int32_t *, u_int32_t *, u_int32_t *, u_int32_t *));
int	pcibios_get_intr_routing __P((struct pcibios_intr_routing *,
	    int *, u_int16_t *));

int	pcibios_return_code __P((u_int16_t, const char *));

void	pcibios_print_exclirq __P((void));
#ifdef PCIINTR_DEBUG
void	pcibios_print_pir_table __P((void));
#endif

#define	PCI_IRQ_TABLE_START	0xf0000
#define	PCI_IRQ_TABLE_END	0xfffff

void
pcibios_init()
{
	struct bios32_entry_info ei;
	u_int32_t rev_maj, rev_min, mech1, mech2, scmech1, scmech2;

	if (bios32_service(BIOS32_MAKESIG('$', 'P', 'C', 'I'),
	    &pcibios_entry, &ei) == 0) {
		/*
		 * No PCI BIOS found; will fall back on old
		 * mechanism.
		 */
		return;
	}

	/*
	 * We've located the PCI BIOS service; get some information
	 * about it.
	 */
	if (pcibios_get_status(&rev_maj, &rev_min, &mech1, &mech2,
	    &scmech1, &scmech2, &pcibios_max_bus) != PCIBIOS_SUCCESS) {
		/*
		 * We can't use the PCI BIOS; will fall back on old
		 * mechanism.
		 */
		return;
	}

	printf("PCI BIOS rev. %d.%d found at 0x%lx\n", rev_maj, rev_min >> 4,
	    ei.bei_entry);
#ifdef PCIBIOSVERBOSE
	printf("pcibios: config mechanism %s%s, special cycles %s%s, "
	    "last bus %d\n",
	    mech1 ? "[1]" : "[x]",
	    mech2 ? "[2]" : "[x]",
	    scmech1 ? "[1]" : "[x]",
	    scmech2 ? "[2]" : "[x]",
	    pcibios_max_bus);

#endif

	/*
	 * The PCI BIOS tells us the config mechanism; fill it in now
	 * so that pci_mode_detect() doesn't have to look for it.
	 */
	pci_mode = mech1 ? 1 : 2;

	pcibios_present = 1;

	/*
	 * Find the PCI IRQ Routing table.
	 */
	pcibios_pir_init();

#ifdef PCIBIOS_INTR_FIXUP
	if (pcibios_pir_table != NULL) {
		int rv;
		u_int16_t pciirq;

		/*
		 * Fixup interrupt routing.
		 */
		rv = pci_intr_fixup(NULL, I386_BUS_SPACE_IO, &pciirq);
		switch (rv) {
		case -1:
			/* Non-fatal error. */
			printf("Warning: unable to fix up PCI interrupt "
			    "routing\n");
			break;

		case 1:
			/* Fatal error. */
			panic("pcibios_init: interrupt fixup failed");
			break;
		}

		/*
		 * XXX Clear `pciirq' from the ISA interrupt allocation
		 * XXX mask.
		 */
	}
#endif

#ifdef PCIBIOS_BUS_FIXUP
	pcibios_max_bus = pci_bus_fixup(NULL, 0);
#ifdef PCIBIOSVERBOSE
	printf("PCI bus #%d is the last bus\n", pcibios_max_bus);
#endif
#endif
}

void
pcibios_pir_init()
{
	char devinfo[256];
	paddr_t pa;
	caddr_t p;
	unsigned char cksum;
	u_int16_t tablesize;
	u_int8_t rev_maj, rev_min;
	int i;

	for (pa = PCI_IRQ_TABLE_START; pa < PCI_IRQ_TABLE_END; pa += 16) {
		p = (caddr_t)ISA_HOLE_VADDR(pa);
		if (*(int *)p != BIOS32_MAKESIG('$', 'P', 'I', 'R'))
			continue;
		
		rev_min = *(p + 4);
		rev_maj = *(p + 5);
		tablesize = *(u_int16_t *)(p + 6);

		cksum = 0;
		for (i = 0; i < tablesize; i++)
			cksum += *(unsigned char *)(p + i);

		printf("PCI IRQ Routing Table rev. %d.%d found at 0x%lx, "
		    "size %d bytes (%d entries)\n", rev_maj, rev_min, pa,
		    tablesize, (tablesize - 32) / 16);

		if (cksum != 0) {
			printf("pcibios_pir_init: bad IRQ table checksum\n");
			continue;
		}

		if (tablesize < 32 || (tablesize % 16) != 0) {
			printf("pcibios_pir_init: bad IRQ table size\n");
			continue;
		}

		if (rev_maj != 1 || rev_min != 0) {
			printf("pcibios_pir_init: unsupported IRQ table "
			    "version\n");
			continue;
		}

		/*
		 * We can handle this table!  Make a copy of it.
		 */
		bcopy(p, &pcibios_pir_header, 32);
		pcibios_pir_table = malloc(tablesize - 32, M_DEVBUF,
		    M_NOWAIT);
		if (pcibios_pir_table == NULL) {
			printf("pcibios_pir_init: no memory for $PIR\n");
			return;
		}
		bcopy(p + 32, pcibios_pir_table, tablesize - 32);
		pcibios_pir_table_nentries = (tablesize - 32) / 16;

		printf("PCI Interrupt Router at %03d:%02d:%01d",
		    pcibios_pir_header.router_bus,
		    (pcibios_pir_header.router_devfunc >> 3) & 0x1f,
		    pcibios_pir_header.router_devfunc & 7);
		if (pcibios_pir_header.compat_router != 0) {
			pci_devinfo(pcibios_pir_header.compat_router, 0, 0,
			    devinfo);
			printf(" (%s)", devinfo);
		}
		printf("\n");
		pcibios_print_exclirq();
#ifdef PCIINTR_DEBUG
		pcibios_print_pir_table();
#endif
		return;
	}

	/*
	 * If there was no PIR table found, try using the PCI BIOS
	 * Get Interrupt Routing call.
	 *
	 * XXX The interface to this call sucks; just allocate enough
	 * XXX room for 32 entries.
	 */
	pcibios_pir_table_nentries = 32;
	pcibios_pir_table = malloc(pcibios_pir_table_nentries *
	    sizeof(*pcibios_pir_table), M_DEVBUF, M_NOWAIT);
	if (pcibios_pir_table == NULL) {
		printf("pcibios_pir_init: no memory for $PIR\n");
		return;
	}
	if (pcibios_get_intr_routing(pcibios_pir_table,
	    &pcibios_pir_table_nentries,
	    &pcibios_pir_header.exclusive_irq) != PCIBIOS_SUCCESS) {
		printf("No PCI IRQ Routing information available.\n");
		free(pcibios_pir_table, M_DEVBUF);
		pcibios_pir_table = NULL;
		pcibios_pir_table_nentries = 0;
		return;
	}
	printf("PCI BIOS has %d Interrupt Routing table entries\n",
	    pcibios_pir_table_nentries);
	pcibios_print_exclirq();
#ifdef PCIINTR_DEBUG
	pcibios_print_pir_table();
#endif
}

int
pcibios_get_status(rev_maj, rev_min, mech1, mech2, scmech1, scmech2, maxbus)
	u_int32_t *rev_maj, *rev_min, *mech1, *mech2, *scmech1, *scmech2,
	    *maxbus;
{
	u_int16_t ax, bx, cx;
	u_int32_t edx;
	int rv;

	__asm __volatile("lcall (%%edi)					; \
			jc 1f						; \
			xor %%ah, %%ah					; \
		1:"
		: "=a" (ax), "=b" (bx), "=c" (cx), "=d" (edx)
		: "0" (0xb101), "D" (&pcibios_entry));

	rv = pcibios_return_code(ax, "pcibios_get_status");
	if (rv != PCIBIOS_SUCCESS)
		return (rv);

	if (edx != BIOS32_MAKESIG('P', 'C', 'I', ' '))
		return (PCIBIOS_SERVICE_NOT_PRESENT);	/* XXX */

	/*
	 * Fill in the various pieces if info we're looking for.
	 */
	*mech1 = ax & 1;
	*mech2 = ax & (1 << 1);
	*scmech1 = ax & (1 << 4);
	*scmech2 = ax & (1 << 5);
	*rev_maj = (bx >> 8) & 0xff;
	*rev_min = bx & 0xff;
	*maxbus = cx & 0xff;

	return (PCIBIOS_SUCCESS);
}

int
pcibios_get_intr_routing(table, nentries, exclirq)
	struct pcibios_intr_routing *table;
	int *nentries;
	u_int16_t *exclirq;
{
	u_int16_t ax, bx;
	int rv;
	struct {
		u_int16_t size;
		caddr_t offset;
		u_int16_t segment;
	} __attribute__((__packed__)) args;

	args.size = *nentries * sizeof(*table);
	args.offset = (caddr_t)table;
	args.segment = GSEL(GDATA_SEL, SEL_KPL);

	memset(table, 0, args.size);

	__asm __volatile("lcall (%%esi)					; \
			jc 1f						; \
			xor %%ah, %%ah					; \
		1:	movw %w2, %%ds					; \
			movw %w2, %%es"
		: "=a" (ax), "=b" (bx)
		: "r" GSEL(GDATA_SEL, SEL_KPL), "0" (0xb10e), "1" (0),
		  "D" (&args), "S" (&pcibios_entry));

	rv = pcibios_return_code(ax, "pcibios_get_intr_routing");
	if (rv != PCIBIOS_SUCCESS)
		return (rv);

	*nentries = args.size / sizeof(*table);
	*exclirq = bx;

	return (PCIBIOS_SUCCESS);
}

int
pcibios_return_code(ax, func)
	u_int16_t ax;
	const char *func;
{
	const char *errstr;
	int rv = ax >> 8;

	switch (rv) {
	case PCIBIOS_SUCCESS:
		return (PCIBIOS_SUCCESS);

	case PCIBIOS_SERVICE_NOT_PRESENT:
		errstr = "service not present";
		break;

	case PCIBIOS_FUNCTION_NOT_SUPPORTED:
		errstr = "function not supported";
		break;

	case PCIBIOS_BAD_VENDOR_ID:
		errstr = "bad vendor ID";
		break;

	case PCIBIOS_DEVICE_NOT_FOUND:
		errstr = "device not found";
		break;

	case PCIBIOS_BAD_REGISTER_NUMBER:
		errstr = "bad register number";
		break;

	case PCIBIOS_SET_FAILED:
		errstr = "set failed";
		break;

	case PCIBIOS_BUFFER_TOO_SMALL:
		errstr = "buffer too small";
		break;

	default:
		printf("%s: unknown return code 0x%x\n", func, rv);
		return (rv);
	}

	printf("%s: %s\n", func, errstr);
	return (rv);
}

void
pcibios_print_exclirq()
{
	int i;

	if (pcibios_pir_header.exclusive_irq) {
		printf("PCI Exclusive IRQs:");
		for (i = 0; i < 16; i++) {
			if (pcibios_pir_header.exclusive_irq & (1 << i))
				printf(" %d", i);
		}
		printf("\n");
	}
}

#ifdef PCIINTR_DEBUG
void
pcibios_print_pir_table()
{
	int i, j;

	for (i = 0; i < pcibios_pir_table_nentries; i++) {
		printf("PIR Entry %d:\n", i);
		printf("\tBus: %d  Device: %d\n",
		    pcibios_pir_table[i].bus,
		    pcibios_pir_table[i].device >> 3);
		for (j = 0; j < 4; j++) {
			printf("\t\tINT%c: link 0x%02x bitmap 0x%04x\n",
			    'A' + j,
			    pcibios_pir_table[i].linkmap[j].link,
			    pcibios_pir_table[i].linkmap[j].bitmap);
		}
	}
}
#endif
