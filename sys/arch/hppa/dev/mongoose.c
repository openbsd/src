/*	$OpenBSD: mongoose.c,v 1.5 2000/01/28 20:20:47 mickey Exp $	*/

/*
 * Copyright (c) 1998,1999 Michael Shalayeff
 * All rights reserved.
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */



#define MONGOOSE_DEBUG 9

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>
#include <hppa/dev/viper.h>

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

/* EISA Bus Adapter registers definitions */
#define	MONGOOSE_MONGOOSE	0x10000
struct mongoose_regs {
	u_int8_t	version;
	u_int8_t	lock;
	u_int8_t	liowait;
	u_int8_t	clock;
	u_int8_t	reserved[0xf000 - 4];
	u_int8_t	intack;
};

#define	MONGOOSE_CTRL		0x00000
struct mongoose_ctrl {
	struct dma0 {
		struct {
			u_int32_t	addr : 8;
			u_int32_t	count: 8;
		} ch[4];
		u_int8_t	command;
		u_int8_t	request;
		u_int8_t	mask_channel;
		u_int8_t	mode;
		u_int8_t	clr_byte_ptr;
		u_int8_t	master_clear;
		u_int8_t	mask_clear;
		u_int8_t	master_write;
		u_int8_t	pad[8];
	}	dma0;

	u_int8_t	irr0;		/* 0x20 */
	u_int8_t	imr0;
	u_int8_t	iack;		/* 0x22 -- 2 b2b reads generate
					(e)isa Iack cycle & returns int level */
	u_int8_t	pad0[29];

	struct timers {
		u_int8_t	sysclk;
		u_int8_t	refresh;
		u_int8_t	spkr;
		u_int8_t	ctrl;
		u_int32_t	pad;
	}	tmr[2];			/* 0x40 -- timers control */
	u_int8_t	pad1[16];

	u_int16_t	inmi;		/* 0x60 NMI control */
	u_int8_t	pad2[30];
	struct {
		u_int8_t	pad0;
		u_int8_t	ch2;
		u_int8_t	ch3;
		u_int8_t	ch1;
		u_int8_t	pad1;
		u_int8_t	pad2[3];
		u_int8_t	ch0;
		u_int8_t	pad4;
		u_int8_t	ch6;
		u_int8_t	ch7;
		u_int8_t	ch5;
		u_int8_t	pad5[3];
		u_int8_t	pad6[16];
	} pr;				/* 0x80 */

	u_int8_t	irr1;		/* 0xa0 */
	u_int8_t	imr1;
	u_int8_t	pad3[30];

	struct dma1 {
		struct {
			u_int32_t	addr : 8;
			u_int32_t	pad0 : 8;
			u_int32_t	count: 8;
			u_int32_t	pad1 : 8;
		} ch[4];
		u_int8_t	command;
		u_int8_t	pad0;
		u_int8_t	request;
		u_int8_t	pad1;
		u_int8_t	mask_channel;
		u_int8_t	pad2;
		u_int8_t	mode;
		u_int8_t	pad3;
		u_int8_t	clr_byte_ptr;
		u_int8_t	pad4;
		u_int8_t	master_clear;
		u_int8_t	pad5;
		u_int8_t	mask_clear;
		u_int8_t	pad6;
		u_int8_t	master_write;
		u_int8_t	pad7;
	}	dma1;			/* 0xc0 */

	u_int8_t	master_req;	/* 0xe0 master request register */
	u_int8_t	pad4[31];

	u_int8_t	pad5[0x3d0];	/* 0x4d0 */
	u_int8_t	pic0;		/* 0 - edge, 1 - level */
	u_int8_t	pic1;
	u_int8_t	pad6[0x460];
	u_int8_t	nmi;
	u_int8_t	nmi_ext;
#define	MONGOOSE_NMI_BUSRESET	0x01
#define	MONGOOSE_NMI_IOPORT_EN	0x02
#define	MONGOOSE_NMI_EN		0x04
#define	MONGOOSE_NMI_MTMO_EN	0x08
#define	MONGOOSE_NMI_RES4	0x10
#define	MONGOOSE_NMI_IOPORT_INT	0x20
#define	MONGOOSE_NMI_MASTER_INT	0x40
#define	MONGOOSE_NMI_INT	0x80
};

#define	MONGOOSE_IOMAP	0x100000

struct mongoose_softc {
	struct  device sc_dev;
	void *sc_ih;

	bus_space_tag_t sc_bt;
	volatile struct mongoose_regs *sc_regs;
	volatile struct mongoose_ctrl *sc_ctrl;
	bus_addr_t sc_iomap;

	struct hppa_eisa_chipset sc_ec;
	struct hppa_isa_chipset sc_ic;
	struct hppa_bus_space_tag sc_eiot;
	struct hppa_bus_space_tag sc_ememt;
	struct hppa_bus_dma_tag sc_edmat;
	struct hppa_bus_space_tag sc_iiot;
	struct hppa_bus_space_tag sc_imemt;
	struct hppa_bus_dma_tag sc_idmat;
};

union mongoose_attach_args {
	char *mongoose_name;
	struct eisabus_attach_args mongoose_eisa;
	struct isabus_attach_args mongoose_isa;
};

int	mgmatch __P((struct device *, void *, void *));
void	mgattach __P((struct device *, struct device *, void *));

struct cfattach mongoose_ca = {
	sizeof(struct mongoose_softc), mgmatch, mgattach
};

struct cfdriver mongoose_cd = {
	NULL, "mg", DV_DULL
};

/* IC guts */
int mgprint __P((void *aux, const char *pnp));
void	mg_eisa_attach_hook __P((struct device *, struct device *,
				 struct eisabus_attach_args *));
int	mg_intr_map __P((void *, u_int, eisa_intr_handle_t *));
const char *mg_intr_string __P((void *, int));
void	*mg_intr_establish __P((void *, int, int, int,
				int (*) __P((void *)), void *, char *));
void	mg_intr_disestablish __P((void *, void *));

void	mg_isa_attach_hook __P((struct device *, struct device *,
				struct isabus_attach_args *));
int	mg_intr_check __P((void *, int, int));
int	mg_intr __P((void *v));

/* BUS guts */
int  mg_eisa_iomap __P((void *v, bus_addr_t addr, bus_size_t size,
			int cacheable, bus_space_handle_t *bshp));
int  mg_eisa_memmap __P((void *v, bus_addr_t addr, bus_size_t size,
			 int cacheable, bus_space_handle_t *bshp));
void mg_eisa_memunmap __P((void *v, bus_space_handle_t bsh,
			   bus_size_t size));

int  mg_isa_iomap __P((void *v, bus_addr_t addr, bus_size_t size,
		       int cacheable, bus_space_handle_t *bshp));
int  mg_isa_memmap __P((void *v, bus_addr_t addr, bus_size_t size,
			int cacheable, bus_space_handle_t *bshp));
void mg_isa_memunmap __P((void *v, bus_space_handle_t bsh,
			  bus_size_t size));

/* TODO: DMA guts */

int
mgmatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	register struct confargs *ca = aux;
	/* struct cfdata *cf = cfdata; */
	bus_space_handle_t ioh;

	if (ca->ca_type.iodc_type != HPPA_TYPE_BHA ||
	    ca->ca_type.iodc_sv_model != HPPA_BHA_EISA)
		return 0;

	if (bus_space_map(ca->ca_iot, ca->ca_hpa + MONGOOSE_MONGOOSE, IOMOD_HPASIZE,
			  0, &ioh))
		return 0;

	/* XXX check EISA signature */

	bus_space_unmap(ca->ca_iot, ioh, IOMOD_HPASIZE);

	return 1;
}

void
mgattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct mongoose_softc *sc = (struct mongoose_softc *)self;
	union mongoose_attach_args ea;
	char brid[EISA_IDSTRINGLEN];

	sc->sc_bt = ca->ca_iot;
	sc->sc_iomap = ca->ca_hpa;
	sc->sc_regs = (struct mongoose_regs *)(ca->ca_hpa + MONGOOSE_MONGOOSE);
	sc->sc_ctrl = (struct mongoose_ctrl *)(ca->ca_hpa + MONGOOSE_CTRL);

	viper_eisa_en();

	/* BUS RESET */
	sc->sc_ctrl->nmi_ext = MONGOOSE_NMI_BUSRESET;
	DELAY(1);
	sc->sc_ctrl->nmi_ext = 0;
	DELAY(100);

	/* determine eisa board id */
	{
		u_int8_t id[4], *p;
		p = (u_int8_t *)(ca->ca_hpa + EISA_SLOTOFF_VID);
		id[0] = *p++;
		id[1] = *p++;
		id[2] = *p++;
		id[3] = *p++;

		brid[0] = EISA_VENDID_0(id);
		brid[1] = EISA_VENDID_1(id);
		brid[2] = EISA_VENDID_2(id);
		brid[3] = EISA_PRODID_0(id + 2);
		brid[4] = EISA_PRODID_1(id + 2);
		brid[5] = EISA_PRODID_2(id + 2);
		brid[6] = EISA_PRODID_3(id + 2);
		brid[7] = '\0';
	}

	printf (": %s rev %d, %d MHz\n", brid, sc->sc_regs->version,
		(sc->sc_regs->clock? 33 : 25));
	sc->sc_regs->liowait = 1;	/* disable isa wait states */
	sc->sc_regs->lock    = 1;	/* bus unlock */

	/* attach EISA */
	sc->sc_ec.ec_v = sc;
	sc->sc_ec.ec_attach_hook = mg_eisa_attach_hook;
	sc->sc_ec.ec_intr_establish = mg_intr_establish;
	sc->sc_ec.ec_intr_disestablish = mg_intr_disestablish;
	sc->sc_ec.ec_intr_string = mg_intr_string;
	sc->sc_ec.ec_intr_map = mg_intr_map;
	/* inherit the bus tags for eisa from the mainbus */
	sc->sc_eiot = *ca->ca_iot;
	sc->sc_ememt = *ca->ca_iot;
	sc->sc_eiot.hbt_cookie = sc->sc_ememt.hbt_cookie = sc;
	sc->sc_eiot.hbt_map = mg_eisa_iomap;
	sc->sc_ememt.hbt_map = mg_eisa_memmap;
	sc->sc_ememt.hbt_unmap = mg_eisa_memunmap;
	/* attachment guts */
	ea.mongoose_eisa.eba_busname = "eisa";
	ea.mongoose_eisa.eba_iot = &sc->sc_eiot;
	ea.mongoose_eisa.eba_memt = &sc->sc_ememt;
	ea.mongoose_eisa.eba_dmat = NULL /* &sc->sc_edmat */;
	ea.mongoose_eisa.eba_ec = &sc->sc_ec;
	config_found(self, &ea.mongoose_eisa, mgprint);

#if 0
	/* TODO: attach ISA */
	sc->sc_ic.ic_v = sc;
	sc->sc_ic.ic_attach_hook = mg_isa_attach_hook;
	sc->sc_ic.ic_intr_establish = mg_intr_establish;
	sc->sc_ic.ic_intr_disestablish = mg_intr_disestablish;
	sc->sc_ic.ic_intr_check = mg_intr_check;
	/* inherit the bus tags for eisa from the mainbus */
	sc->sc_iiot = *ca->ca_iot;
	sc->sc_imemt = *ca->ca_iot;
	sc->sc_iiot.hbt_cookie = sc->sc_imemt.hbt_cookie = sc;
	sc->sc_iiot.hbt_map = mg_isa_iomap;
	sc->sc_imemt.hbt_map = mg_isa_memmap;
	sc->sc_imemt.hbt_unmap = mg_isa_memunmap;
	/* TODO: DMA tags */
	/* attachment guts */
	ea.mongoose_isa.iba_busname = "isa";
	ea.mongoose_isa.iba_iot = &sc->sc_iiot;
	ea.mongoose_isa.iba_memt = &sc->sc_imemt;
#if NISADMA > 0
	ea.mongoose_isa.iba_dmat = &sc->sc_idmat;
#endif
	ea.mongoose_isa.iba_ic = &sc->sc_ic;
	config_found(self, &ea.mongoose_isa, mgprint);
#endif

	/* attach interrupt */
	sc->sc_ih = cpu_intr_establish(IPL_HIGH, ca->ca_irq,
				       mg_intr, sc, &sc->sc_dev);
}

int
mgprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	union mongoose_attach_args *ea = aux;

	if (pnp)
		printf ("%s at %s", ea->mongoose_name, pnp);

	return (UNCONF);
}

void
mg_eisa_attach_hook(parent, self, mg)
	struct device *parent;
	struct device *self;
	struct eisabus_attach_args *mg;
{
}

int
mg_intr_map(v, irq, ehp)
	void *v;
	u_int irq;
	eisa_intr_handle_t *ehp;
{
	*ehp = irq;
	return 0;
}

const char *
mg_intr_string(v, irq)
	void *v;
	int irq;
{
	static char buf[16];

	sprintf (buf, "isa irq %d", irq);
	return buf;
}

void *
mg_intr_establish(v, irq, type, level, fn, arg, name)
	void *v;
	int irq;
	int type;
	int level;
	int (*fn) __P((void *));
	void *arg;
	char *name;
{
	void *cookie = "cookie";

	
	return cookie;
}

void
mg_intr_disestablish(v, cookie)
	void *v;
	void *cookie;
{

}

void
mg_isa_attach_hook(parent, self, iba)
	struct device *parent;
	struct device *self;
	struct isabus_attach_args *iba;
{

}

int
mg_intr_check(v, irq, type)
	void *v;
	int irq;
	int type;
{
	return 0;
}

int
mg_intr(v)
	void *v;
{
	return 0;
}

int
mg_eisa_iomap(v, addr, size, cacheable, bshp)
	void *v;
	bus_addr_t addr;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	struct mongoose_softc *sc = v;

	return (sc->sc_bt->hbt_map)(NULL, sc->sc_iomap + addr, size,
				    cacheable, bshp);
}

int
mg_eisa_memmap(v, addr, size, cacheable, bshp)
	void *v;
	bus_addr_t addr;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	/* TODO: eisa memory map */
	return -1;
}

void
mg_eisa_memunmap(v, bsh, size)
	void *v;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	/* TODO: eisa memory unmap */
}


