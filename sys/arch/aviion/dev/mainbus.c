/*	$OpenBSD: mainbus.c,v 1.3 2010/04/20 22:53:24 miod Exp $ */
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 2004, Miodrag Vallat.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/extent.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/prom.h>

void	mainbus_attach(struct device *, struct device *, void *);
int 	mainbus_match(struct device *, void *, void *);
int	mainbus_print(void *, const char *);
int	mainbus_scan(struct device *, void *, void *);

/*
 * bus_space routines for 1:1 obio mappings
 */

int	mainbus_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	mainbus_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	mainbus_subregion(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);
void	*mainbus_vaddr(bus_space_tag_t, bus_space_handle_t);

const struct aviion_bus_space_tag mainbus_bustag = {
	._space_map = mainbus_map,
	._space_unmap = mainbus_unmap,
	._space_subregion = mainbus_subregion,
	._space_vaddr = mainbus_vaddr,
	._space_read_1 = generic_space_read_1,
	._space_write_1 = generic_space_write_1,
	._space_read_2 = generic_space_read_2,
	._space_write_2 = generic_space_write_2,
	._space_read_4 = generic_space_read_4,
	._space_write_4 = generic_space_write_4,
	._space_read_raw_2 = generic_space_read_raw_2,
	._space_write_raw_2 = generic_space_write_raw_2,
	._space_read_raw_4 = generic_space_read_raw_4,
	._space_write_raw_4 = generic_space_write_raw_4,
};

/*
 * Obio (internal IO) space is mapped 1:1 (see pmap_bootstrap() for details).
 */

int
mainbus_map(bus_space_tag_t tag, bus_addr_t addr, bus_size_t size, int flags,
    bus_space_handle_t *ret)
{
	*ret = (bus_space_handle_t)addr;
	return 0;
}

void
mainbus_unmap(bus_space_tag_t tag, bus_space_handle_t handle, bus_size_t size)
{
	/* nothing to do */
}

int
mainbus_subregion(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_addr_t offset, bus_size_t size, bus_space_handle_t *ret)
{
	*ret = handle + offset;
	return (0);
}

void *
mainbus_vaddr(bus_space_tag_t tag, bus_space_handle_t handle)
{
	return (void *)handle;
}

/*
 * Configuration glue
 */

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mainbus_match(struct device *parent, void *cf, void *args)
{
	return (mainbus_cd.cd_ndevs == 0);
}

void
mainbus_attach(struct device *parent, struct device *self, void *args)
{
	extern char cpu_model[];
	extern int32_t cpuid, sysid;

	printf(": %s, cpuid 0x%x", cpu_model, cpuid);
	if (sysid != -1)
		printf(", sysid %x", sysid);
	printf("\n");

	/*
	 * Display cpu/mmu details for the main processor.
	 */
	cpu_configuration_print(1);

	(void)config_search(mainbus_scan, self, args);
}

int
mainbus_scan(struct device *parent, void *child, void *args)
{
	struct cfdata *cf = child;
	struct confargs oca;

	oca.ca_iot = &mainbus_bustag;
	oca.ca_paddr = (paddr_t)cf->cf_loc[0];
	oca.ca_offset = (paddr_t)-1;
	oca.ca_ipl = (u_int)-1;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);

	config_attach(parent, cf, &oca, mainbus_print);
	return (1);
}

int
mainbus_print(void *args, const char *bus)
{
	struct confargs *ca = args;

	if (ca->ca_paddr != (paddr_t)-1)
		printf(" addr 0x%08x", ca->ca_paddr);
	return (UNCONF);
}
