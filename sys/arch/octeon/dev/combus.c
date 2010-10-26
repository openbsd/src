/*	$OpenBSD: combus.c,v 1.2 2010/10/26 00:02:01 syuu Exp $ */

/*
 * Copyright (c) 2000-2004 Opsycon AB  (www.opsycon.se)
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
 * This is a combus, for OCTEON UART.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <mips64/archtype.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/atomic.h>

#include <octeon/dev/octeonreg.h>
#include <octeon/dev/combusvar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/ns16550reg.h>
#define	com_lcr	com_cfcr

int	 combusmatch(struct device *, void *, void *);
void	 combusattach(struct device *, struct device *, void *);
int	 combusprint(void *, const char *);
int	 combussubmatch(struct device *, void *, void *);

u_int8_t combus_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t combus_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t combus_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t combus_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void	 combus_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int8_t);
void	 combus_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int16_t);
void	 combus_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int32_t);
void	 combus_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int64_t);

void	 combus_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 combus_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 combus_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 combus_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 combus_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 combus_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);

int	 combus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	 combus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	 combus_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);

void	*combus_space_vaddr(bus_space_tag_t, bus_space_handle_t);

bus_addr_t combus_pa_to_device(paddr_t);
paddr_t	 combus_device_to_pa(bus_addr_t);

bus_size_t combus_get_read_reg(bus_size_t);
bus_size_t combus_get_write_reg(bus_size_t, u_int8_t);

static int lcr = 0;

struct cfattach combus_ca = {
	sizeof(struct device), combusmatch, combusattach
};

struct cfdriver combus_cd = {
	NULL, "combus", DV_DULL
};

bus_space_t combus_tag = {
	PHYS_TO_XKPHYS(0, CCA_NC),
	NULL,
	combus_read_1, combus_write_1,
	combus_read_2, combus_write_2,
	combus_read_4, combus_write_4,
	combus_read_8, combus_write_8,
	combus_read_raw_2, combus_write_raw_2,
	combus_read_raw_4, combus_write_raw_4,
	combus_read_raw_8, combus_write_raw_8,
	combus_space_map, combus_space_unmap, combus_space_region,
	combus_space_vaddr
};

/*
 * List of combus child devices.
 */

#define	COMBUSDEV(name, addr, i) \
	{ name, &combus_tag, &combus_tag, addr, i }
struct combus_attach_args combus_children[] = {
	COMBUSDEV("com", OCTEON_UART0_BASE, CIU_INT_UART0),
	COMBUSDEV("com", OCTEON_UART1_BASE, CIU_INT_UART1),
};
#undef	COMBUSDEV



/*
 * Match bus only to targets which have this bus.
 */
int
combusmatch(struct device *parent, void *match, void *aux)
{
	return (1);
}

int
combusprint(void *aux, const char *combus)
{
	struct combus_attach_args *cba = aux;

	if (combus != NULL)
		printf("%s at %s", cba->cba_name, combus);

	if (cba->cba_baseaddr != 0)
		printf(" base 0x%llx", cba->cba_baseaddr);
	if (cba->cba_intr >= 0)
		printf(" irq %d", cba->cba_intr);

	return (UNCONF);
}

int
combussubmatch(struct device *parent, void *vcf, void *args)
{
	struct cfdata *cf = vcf;
	struct combus_attach_args *cba = args;

	if (strcmp(cf->cf_driver->cd_name, cba->cba_name) != 0)
		return 0;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != (int)cba->cba_baseaddr)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, cf, cba);
}

void
combusattach(struct device *parent, struct device *self, void *aux)
{
	uint i;

	printf("\n");

	/*
	 * Attach subdevices.
	 */
	for (i = 0; i < nitems(combus_children); i++)
		config_found_sm(self, combus_children + i,
		    combusprint, combussubmatch);
}

/*
 * Bus access primitives. These are really ugly...
 */

u_int8_t
combus_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	o = combus_get_read_reg(o);
	return (u_int8_t)(volatile uint64_t)*(volatile uint64_t *)(h + o);
}

u_int16_t
combus_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	panic(__func__);
}

u_int32_t
combus_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	panic(__func__);
}

u_int64_t
combus_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	panic(__func__);
}

void
combus_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	o = combus_get_write_reg(o, 1);
	*(volatile uint64_t *)(h + o) = (volatile uint64_t)v;
}

void
combus_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int16_t v)
{
	panic(__func__);
}

void
combus_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int32_t v)
{
	panic(__func__);
}

void
combus_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int64_t v)
{
	panic(__func__);
}

void
combus_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
combus_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
combus_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
combus_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
combus_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
combus_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

int
combus_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	*bshp = t->bus_base + offs;

	return 0;
}

void
combus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
}

int
combus_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void *
combus_space_vaddr(bus_space_tag_t t, bus_space_handle_t h)
{
	return (void *)h;
}

/*
 * combus bus_dma helpers.
 */

bus_addr_t
combus_pa_to_device(paddr_t pa)
{
	return (bus_addr_t)pa;
}

paddr_t
combus_device_to_pa(bus_addr_t addr)
{
	return (paddr_t)addr;
}

bus_size_t
combus_get_read_reg(bus_size_t o)
{
	if (lcr && LCR_DLAB)
		switch(o) {
			case com_dlbl:
				return (bus_size_t)0x80;
			case com_dlbh:
				return (bus_size_t)0x88;
		}

	return (bus_size_t)(o << 3);
}

bus_size_t
combus_get_write_reg(bus_size_t o, u_int8_t v)
{
	if (o == com_lcr)
		lcr = v;

	switch(o) {
		case com_data:
			return (bus_size_t)0x40;
		case com_fifo:
			return (bus_size_t)0x50;
	}

	if (lcr && LCR_DLAB)
		switch(o) {
			case com_dlbl:
				return (bus_size_t)0x80;
			case com_dlbh:
				return (bus_size_t)0x88;
		}

	return (bus_size_t)(o << 3);
}
