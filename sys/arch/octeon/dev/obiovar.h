/*	$OpenBSD: obiovar.h,v 1.2 2010/11/23 18:46:29 syuu Exp $	*/

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

#ifndef	_OBIOVAR_H_
#define	_OBIOVAR_H_

#include <machine/bus.h>

extern bus_space_t obio_tag;
extern struct machine_bus_dma_tag obio_dma_tag;

struct obio_attach_args {
	char		*oba_name;

	bus_space_tag_t  oba_iot;
	bus_space_tag_t  oba_memt;
	bus_dma_tag_t	 oba_dmat;
	bus_addr_t	 oba_baseaddr;
	int	 	 oba_intr;
};

void	 obio_setintrmask(int);
void   *obio_intr_establish(int, int, int (*)(void *),
	    void *, const char *);
void	obio_intr_disestablish(void *);
void	obio_intr_init(void);

#endif	/* _OBIOVAR_H_ */
