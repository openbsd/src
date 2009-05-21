/*	$OpenBSD: autoconf.h,v 1.17 2009/05/21 16:28:11 miod Exp $ */

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

/*
 * Definitions used by autoconfiguration.
 */

#ifndef _MACHINE_AUTOCONF_H_
#define _MACHINE_AUTOCONF_H_

#include <machine/bus.h>

/*
 * Structure holding all misc config information.
 */
#define MAX_CPUS	4

struct sys_rec {
	int	system_type;

	struct cpuinfo {
		u_int16_t type;
		u_int8_t  vers_maj;
		u_int8_t  vers_min;
		u_int16_t fptype;
		u_int8_t  fpvers_maj;
		u_int8_t  fpvers_min;
		u_int32_t clock;
		u_int32_t clock_bus;
		u_int32_t tlbsize;
		u_int32_t tlbwired;
	} cpu[MAX_CPUS];

	/* Published cache operations. */
	void    (*_SyncCache)(void);
	void    (*_InvalidateICache)(vaddr_t, int);
	void    (*_InvalidateICachePage)(vaddr_t);
	void    (*_SyncDCachePage)(vaddr_t);
	void    (*_HitSyncDCache)(vaddr_t, int);
	void    (*_IOSyncDCache)(vaddr_t, int, int);
	void    (*_HitInvalidateDCache)(vaddr_t, int);

	/* Serial console configuration. */
	struct mips_bus_space console_io;
};

extern struct sys_rec sys_config;

/**/
struct confargs;

struct confargs {
	char		*ca_name;	/* Device name. */
	bus_space_tag_t ca_iot;
	bus_space_tag_t ca_memt;
	bus_dma_tag_t	ca_dmat;
	int32_t		ca_intr;
	bus_addr_t	ca_baseaddr;
};

void	enaddr_aton(const char *, u_int8_t *);

void	ip27_setup(void);
void	ip30_setup(void);
void	ip32_setup(void);

extern char osloadpartition[256];

#endif /* _MACHINE_AUTOCONF_H_ */
