/*	$OpenBSD: pcibrvar.h,v 1.3 2002/07/23 17:53:25 drahn Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

struct pcibr_config {
	bus_space_tag_t lc_memt;
	bus_space_tag_t lc_iot;
	bus_space_handle_t	ioh_cf8;
	bus_space_handle_t	ioh_cfc;
	struct ppc_pci_chipset lc_pc;
	int	config_type;
	int	bus;
	int	pci_init_done;
	int     node;
};

struct pcibr_softc {
	struct device	sc_dev;
	struct pcibr_config *sc_pcibr;
	struct ppc_bus_space sc_membus_space;
	struct ppc_bus_space sc_iobus_space;
	struct powerpc_bus_dma_tag sc_dmatag;
	struct pcibr_config	pcibr_config;
	struct extent *extent_mem;
	struct extent *extent_port;
	u_int32_t mem_alloc_start;
	u_int32_t port_alloc_start;
	int nbogus;
};

struct pci_reserve_mem {
	bus_addr_t start;
	bus_size_t size;
	char *name;
};

void pci_addr_fixup(struct pcibr_softc *, pci_chipset_tag_t, int);

#define PCIADDR_SEARCH_IO  0
#define PCIADDR_SEARCH_MEM 1


struct extent * pciaddr_search(int mem_port, struct device *,
    bus_addr_t *startp, bus_size_t size);
