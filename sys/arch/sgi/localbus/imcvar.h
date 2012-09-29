/*	$OpenBSD: imcvar.h,v 1.3 2012/09/29 21:46:02 miod Exp $	*/
/*	$NetBSD: imcvar.h,v 1.1 2006/08/30 23:44:52 rumble Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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

struct imc_attach_args {
	const char	*iaa_name;
	bus_space_tag_t	 iaa_st;
	bus_dma_tag_t	 iaa_dmat;
};

void	imc_bus_reset(void);
int	imc_gio64_arb_config(int, uint32_t);
void	imc_disable_sysad_parity(void);
void	imc_enable_sysad_parity(void);
int	imc_is_sysad_parity_enabled(void);

#define	imc_read(o) \
	*(volatile uint32_t *)PHYS_TO_XKPHYS(IMC_BASE + (o), CCA_NC)
#define	imc_write(o,v) \
	*(volatile uint32_t *)PHYS_TO_XKPHYS(IMC_BASE + (o), CCA_NC) = (v)

uint8_t  imc_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
uint16_t imc_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	 imc_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	 imc_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t, uint8_t);
void	 imc_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t, uint16_t);
void	 imc_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
uint32_t imc_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
uint64_t imc_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	 imc_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t, uint32_t);
void	 imc_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t, uint64_t);
void	 imc_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	 imc_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
void	 imc_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	 imc_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
int	 imc_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	 imc_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	 imc_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);
void	*imc_space_vaddr(bus_space_tag_t, bus_space_handle_t);

extern struct machine_bus_dma_tag imc_bus_dma_tag;
extern bus_space_t imcbus_tag;
