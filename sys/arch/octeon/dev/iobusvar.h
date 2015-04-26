/*	$OpenBSD: iobusvar.h,v 1.2 2015/04/26 12:24:03 jmatthew Exp $	*/

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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

#ifndef	_IOBUSVAR_H_
#define	_IOBUSVAR_H_

#include <machine/bus.h>

extern bus_space_t iobus_tag;
extern struct machine_bus_dma_tag iobus_dma_tag;

struct iobus_unit {
	bus_addr_t addr;
	int irq;
};

struct iobus_attach_args {
	char		*aa_name;
	int			aa_unitno;
	
	const struct iobus_unit *aa_unit;
	
	bus_space_tag_t  aa_bust;
	bus_dma_tag_t	 aa_dmat;
};

int	 iobus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	 iobus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	 iobus_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);

void	*iobus_space_vaddr(bus_space_tag_t, bus_space_handle_t);

#endif	/* _IOBUSVAR_H_ */
