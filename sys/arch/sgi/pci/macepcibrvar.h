/*	$OpenBSD: macepcibrvar.h,v 1.2 2004/08/10 19:16:18 deraadt Exp $ */

/*
 * Copyright (c) 2003-2004 Opsycon AB (www.opsycon.se)
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

#ifndef _PCIBRVAR_H_
#define _PCIBRVAR_H_

#define	MACE_PCI_IO_BASE	0x18000000
#define	MACE_PCI_IO_SIZE	0x02000000
#define	MACE_PCI_MEM_BASE	0x1a000000
#define	MACE_PCI_MEM_SIZE	0x02000000

struct pcibr_softc {
	struct device	sc_dev;
	struct mips_bus_space *sc_mem_bus_space;
	struct mips_bus_space *sc_io_bus_space;
	struct mips_pci_chipset sc_pc;
	bus_space_tag_t sc_memt;
	bus_space_handle_t sc_memh;
};

u_int8_t pcib_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t pcib_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t pcib_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t pcib_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void pcib_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int8_t);
void pcib_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int16_t);
void pcib_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int32_t);
void pcib_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t, u_int64_t);

int pcib_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void pcib_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int pcib_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t, bus_size_t, bus_space_handle_t *);

#endif
