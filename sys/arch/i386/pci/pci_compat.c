/*	$NetBSD: pci_compat.c,v 1.1 1996/03/27 04:01:13 cgd Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Compatibility functions, for use with old NetBSD/i386 PCI code.
 *
 * These should go away when all drivers are converted to the new
 * interfaces.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <vm/vm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

__warn_references(pci_map_int,
    "uses pci_map_int() compatibility interface");

void *
pci_map_int(tag, level, func, arg)
	pcitag_t tag;
	int level;
	int (*func) __P((void *));
	void *arg;
{
	pci_intr_handle_t ih;
	pcireg_t data;
	int pin, line;
	const char *intrstr;
	void *rv;

	data = pci_conf_read(NULL, tag, PCI_INTERRUPT_REG);

	pin = PCI_INTERRUPT_PIN(data);
	line = PCI_INTERRUPT_LINE(data);

	if (pci_intr_map(NULL, tag, pin, line, &ih))
		return NULL;
	intrstr = pci_intr_string(NULL, ih);
	rv = pci_intr_establish(NULL, ih, level, func, arg, NULL);
	if (rv == NULL)
		printf("pci_map_int: failed to map interrupt\n");
	else if (intrstr != NULL)
		printf("pci_map_int: interrupting at %s\n", intrstr);
	return (rv);
}

__warn_references(pci_map_io,
    "uses pci_map_io() compatibility interface");

int
pci_map_io(tag, reg, iobasep)
	pcitag_t tag;
	int reg;
	int *iobasep;
{
	bus_io_addr_t ioaddr;
	bus_io_size_t iosize;
	bus_io_handle_t ioh;

	if (pci_io_find(NULL, tag, reg, &ioaddr, &iosize))
		return (1);
	if (bus_io_map(NULL, ioaddr, iosize, &ioh))
		return (1);

	*iobasep = ioh;

	return 0;
}

__warn_references(pci_map_mem,
    "uses pci_map_mem() compatibility interface");

int
pci_map_mem(tag, reg, vap, pap)
	pcitag_t tag;
	int reg;
	vm_offset_t *vap, *pap;
{
	bus_mem_addr_t memaddr;
	bus_mem_size_t memsize;
	bus_mem_handle_t memh;
	int cacheable;

	if (pci_mem_find(NULL, tag, reg, &memaddr, &memsize, &cacheable))
		return (1);
	if (bus_mem_map(NULL, memaddr, memsize, cacheable, &memh))
		return (1);

	*vap = (vm_offset_t)memh;
	*pap = memaddr;

	return 0;
}
