/*	$OpenBSD: pcibrvar.h,v 1.7 2009/05/03 21:30:09 kettenis Exp $ */

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
	struct extent *sc_ioex;
	struct extent *sc_memex;
	char sc_ioex_name[32];
	char sc_memex_name[32];
};
