/*	$OpenBSD: octeon_uartbus.c,v 1.1 2011/05/08 13:39:30 syuu Exp $ */

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
 * This is a uartbus, for OCTEON UART.
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
#include <octeon/dev/uartbusvar.h>

#include <dev/ic/comreg.h>
#include <dev/ic/ns16550reg.h>
#define	com_lcr	com_cfcr

int	 uartbusmatch(struct device *, void *, void *);
void	 uartbusattach(struct device *, struct device *, void *);
int	 uartbusprint(void *, const char *);
int	 uartbussubmatch(struct device *, void *, void *);

u_int8_t uartbus_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int16_t uartbus_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int32_t uartbus_read_4(bus_space_tag_t, bus_space_handle_t, bus_size_t);
u_int64_t uartbus_read_8(bus_space_tag_t, bus_space_handle_t, bus_size_t);

void	 uartbus_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int8_t);
void	 uartbus_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int16_t);
void	 uartbus_write_4(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int32_t);
void	 uartbus_write_8(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    u_int64_t);

void	 uartbus_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 uartbus_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 uartbus_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 uartbus_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);
void	 uartbus_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    u_int8_t *, bus_size_t);
void	 uartbus_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const u_int8_t *, bus_size_t);

int	 uartbus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
void	 uartbus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int	 uartbus_space_region(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    bus_size_t, bus_space_handle_t *);

void	*uartbus_space_vaddr(bus_space_tag_t, bus_space_handle_t);

bus_addr_t uartbus_pa_to_device(paddr_t);
paddr_t	 uartbus_device_to_pa(bus_addr_t);

bus_size_t uartbus_get_read_reg(bus_size_t);
bus_size_t uartbus_get_write_reg(bus_size_t, u_int8_t);

static int lcr = 0;

struct cfattach uartbus_ca = {
	sizeof(struct device), uartbusmatch, uartbusattach
};

struct cfdriver uartbus_cd = {
	NULL, "uartbus", DV_DULL
};

bus_space_t uartbus_tag = {
	PHYS_TO_XKPHYS(0, CCA_NC),
	NULL,
	uartbus_read_1, uartbus_write_1,
	uartbus_read_2, uartbus_write_2,
	uartbus_read_4, uartbus_write_4,
	uartbus_read_8, uartbus_write_8,
	uartbus_read_raw_2, uartbus_write_raw_2,
	uartbus_read_raw_4, uartbus_write_raw_4,
	uartbus_read_raw_8, uartbus_write_raw_8,
	uartbus_space_map, uartbus_space_unmap, uartbus_space_region,
	uartbus_space_vaddr
};

/*
 * List of uartbus child devices.
 */

#define	UARTBUSDEV(name, addr, i) \
	{ name, &uartbus_tag, addr, i }
struct uartbus_attach_args uartbus_children[] = {
	UARTBUSDEV("com", OCTEON_UART0_BASE, CIU_INT_UART0),
	UARTBUSDEV("com", OCTEON_UART1_BASE, CIU_INT_UART1),
};
#undef	UARTBUSDEV



/*
 * Match bus only to targets which have this bus.
 */
int
uartbusmatch(struct device *parent, void *match, void *aux)
{
	return (1);
}

int
uartbusprint(void *aux, const char *uartbus)
{
	struct uartbus_attach_args *uba = aux;

	if (uartbus != NULL)
		printf("%s at %s", uba->uba_name, uartbus);

	if (uba->uba_baseaddr != 0)
		printf(" base 0x%llx", uba->uba_baseaddr);
	if (uba->uba_intr >= 0)
		printf(" irq %d", uba->uba_intr);

	return (UNCONF);
}

int
uartbussubmatch(struct device *parent, void *vcf, void *args)
{
	struct cfdata *cf = vcf;
	struct uartbus_attach_args *uba = args;

	if (strcmp(cf->cf_driver->cd_name, uba->uba_name) != 0)
		return 0;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != (int)uba->uba_baseaddr)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, cf, uba);
}

void
uartbusattach(struct device *parent, struct device *self, void *aux)
{
	uint i;

	printf("\n");

	/*
	 * Attach subdevices.
	 */
	for (i = 0; i < nitems(uartbus_children); i++)
		config_found_sm(self, uartbus_children + i,
		    uartbusprint, uartbussubmatch);
}

/*
 * Bus access primitives. These are really ugly...
 */

u_int8_t
uartbus_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	o = uartbus_get_read_reg(o);
	return (u_int8_t)(volatile uint64_t)*(volatile uint64_t *)(h + o);
}

u_int16_t
uartbus_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	panic(__func__);
}

u_int32_t
uartbus_read_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	panic(__func__);
}

u_int64_t
uartbus_read_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	panic(__func__);
}

void
uartbus_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	o = uartbus_get_write_reg(o, 1);
	*(volatile uint64_t *)(h + o) = (volatile uint64_t)v;
}

void
uartbus_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int16_t v)
{
	panic(__func__);
}

void
uartbus_write_4(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int32_t v)
{
	panic(__func__);
}

void
uartbus_write_8(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o, u_int64_t v)
{
	panic(__func__);
}

void
uartbus_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
uartbus_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
uartbus_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
uartbus_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
uartbus_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

void
uartbus_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const u_int8_t *buf, bus_size_t len)
{
	panic(__func__);
}

int
uartbus_space_map(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	*bshp = t->bus_base + offs;

	return 0;
}

void
uartbus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{
}

int
uartbus_space_region(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

void *
uartbus_space_vaddr(bus_space_tag_t t, bus_space_handle_t h)
{
	return (void *)h;
}

/*
 * uartbus bus_dma helpers.
 */

bus_addr_t
uartbus_pa_to_device(paddr_t pa)
{
	return (bus_addr_t)pa;
}

paddr_t
uartbus_device_to_pa(bus_addr_t addr)
{
	return (paddr_t)addr;
}

bus_size_t
uartbus_get_read_reg(bus_size_t o)
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
uartbus_get_write_reg(bus_size_t o, u_int8_t v)
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
