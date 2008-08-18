/*	$OpenBSD: ka820.c,v 1.13 2008/08/18 23:05:38 miod Exp $	*/
/*	$NetBSD: ka820.c,v 1.22 2000/06/04 02:19:27 matt Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ka820.c	7.4 (Berkeley) 12/16/90
 */

/*
 * KA820 specific CPU code.  (Note that the VAX8200 uses a KA820, not
 * a KA8200.  Sigh.)
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h> 

#include <machine/ka820.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/nexus.h>
#include <machine/clock.h>
#include <machine/scb.h>
#include <machine/bus.h>

#include <vax/bi/bireg.h>
#include <vax/bi/bivar.h>

#include <vax/vax/crx.h>

struct ka820port *ka820port_ptr;
struct rx50device *rx50device_ptr;
static volatile struct ka820clock *ka820_clkpage;

static int ka820_match(struct device *, struct cfdata *, void *);
static void ka820_attach(struct device *, struct device *, void *);
static void ka820_memerr(void);
static void ka820_conf(void);
static int ka820_mchk(caddr_t);
static int ka820_clkread(time_t base);
static void ka820_clkwrite(void);
static void rxcdintr(void *);
static void vaxbierr(void *);

struct	cpu_dep ka820_calls = {
	0,
	ka820_mchk,
	ka820_memerr,
	ka820_conf,
	ka820_clkread,
	ka820_clkwrite,
	3,      /* ~VUPS */
	5,	/* SCB pages */
	NULL,
	NULL,
	NULL,
	NULL,
	hardclock
};

struct cfattach cpu_bi_ca = {
	sizeof(struct device), ka820_match, ka820_attach
};

#ifdef notyet
extern pt_entry_t BRAMmap[];
extern pt_entry_t EEPROMmap[];
char bootram[KA820_BRPAGES * VAX_NBPG];
char eeprom[KA820_EEPAGES * VAX_NBPG];
#endif

int
ka820_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void	*aux;
{
	struct bi_attach_args *ba = aux;

	if (bus_space_read_2(ba->ba_iot, ba->ba_ioh, BIREG_DTYPE) != BIDT_KA820)
		return 0;

	if (ba->ba_nodenr != mastercpu)
		return 0;

	if (cf->cf_loc[BICF_NODE] != BICF_NODE_DEFAULT &&
	    cf->cf_loc[BICF_NODE] != ba->ba_nodenr)
		return 0;

	return 1;
}

void
ka820_attach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	struct bi_attach_args *ba = aux;
	register int csr;
	u_short rev;

	rev = bus_space_read_4(ba->ba_iot, ba->ba_ioh, BIREG_DTYPE) >> 16;
	strlcpy(cpu_model, "VAX 8200", sizeof cpu_model);
	cpu_model[6] = rev & 0x8000 ? '5' : '0';
	printf(": ka82%c (%s) cpu rev %d, u patch rev %d, sec patch %d\n",
	    cpu_model[6], mastercpu == ba->ba_nodenr ? "master" : "slave",
	    ((rev >> 11) & 15), ((rev >> 1) &1023), rev & 1);

	/* reset the console and enable the RX50 */
	ka820port_ptr = (void *)vax_map_physmem(KA820_PORTADDR, 1);
	csr = ka820port_ptr->csr;
	csr &= ~KA820PORT_RSTHALT;	/* ??? */
	csr |= KA820PORT_CONSCLR | KA820PORT_CRDCLR | KA820PORT_CONSEN |
		KA820PORT_RXIE;
	ka820port_ptr->csr = csr;
	bus_space_write_4(ba->ba_iot, ba->ba_ioh,
	    BIREG_INTRDES, ba->ba_intcpu);
	bus_space_write_4(ba->ba_iot, ba->ba_ioh, BIREG_VAXBICSR,
	    bus_space_read_4(ba->ba_iot, ba->ba_ioh, BIREG_VAXBICSR) |
	    BICSR_SEIE | BICSR_HEIE);

}

void
ka820_conf()
{
	/*
	 * Setup parameters necessary to read time from clock chip.
	 */
	ka820_clkpage = (void *)vax_map_physmem(KA820_CLOCKADDR, 1);

	/* Steal the interrupt vectors that are unique for us */
	scb_vecalloc(KA820_INT_RXCD, rxcdintr, NULL, SCB_ISTACK, NULL);
	scb_vecalloc(0x50, vaxbierr, NULL, SCB_ISTACK, NULL);

	/* XXX - should be done somewhere else */
	scb_vecalloc(SCB_RX50, crxintr, NULL, SCB_ISTACK, NULL);
	rx50device_ptr = (void *)vax_map_physmem(KA820_RX50ADDR, 1);
}

void
vaxbierr(void *arg)
{
	if (cold == 0)
		panic("vaxbierr");
}

#ifdef notdef
/*
 * MS820 support.
 */
struct ms820regs {
	struct	biiregs biic;		/* BI interface chip */
	u_long	ms_gpr[4];		/* the four gprs (unused) */
	int	ms_csr1;		/* control/status register 1 */
	int	ms_csr2;		/* control/status register 2 */
};
#endif

#define	MEMRD(reg) bus_space_read_4(sc->sc_iot, sc->sc_ioh, (reg))
#define MEMWR(reg, val) bus_space_write_4(sc->sc_iot, sc->sc_ioh, (reg), (val))

#define	MSREG_CSR1	0x100
#define	MSREG_CSR2	0x104
/*
 * Bits in CSR1.
 */
#define MS1_ERRSUM	0x80000000	/* error summary (ro) */
#define MS1_ECCDIAG	0x40000000	/* ecc diagnostic (rw) */
#define MS1_ECCDISABLE	0x20000000	/* ecc disable (rw) */
#define MS1_MSIZEMASK	0x1ffc0000	/* mask for memory size (ro) */
#define MS1_RAMTYMASK	0x00030000	/* mask for ram type (ro) */
#define MS1_RAMTY64K	0x00000000	/* 64K chips */
#define MS1_RAMTY256K	0x00010000	/* 256K chips */
#define MS1_RAMTY1MB	0x00020000	/* 1MB chips */
					/* type 3 reserved */
#define MS1_CRDINH	0x00008000	/* inhibit crd interrupts (rw) */
#define MS1_MEMVALID	0x00004000	/* memory has been written (ro) */
#define MS1_INTLK	0x00002000	/* interlock flag (ro) */
#define MS1_BROKE	0x00001000	/* broken (rw) */
#define MS1_MBZ		0x00000880	/* zero */
#define MS1_MWRITEERR	0x00000400	/* rds during masked write (rw) */
#define MS1_CNTLERR	0x00000200	/* internal timing busted (rw) */
#define MS1_INTLV	0x00000100	/* internally interleaved (ro) */
#define MS1_DIAGC	0x0000007f	/* ecc diagnostic bits (rw) */

/*
 * Bits in CSR2.
 */
#define MS2_RDSERR	0x80000000	/* rds error (rw) */
#define MS2_HIERR	0x40000000	/* high error rate (rw) */
#define MS2_CRDERR	0x20000000	/* crd error (rw) */
#define MS2_ADRSERR	0x10000000	/* rds due to addr par err (rw) */
#define MS2_MBZ		0x0f000080	/* zero */
#define MS2_ADDR	0x00fffe00	/* address in error (relative) (ro) */
#define MS2_INTLVADDR	0x00000100	/* error was in bank 1 (ro) */
#define MS2_SYN		0x0000007f	/* error syndrome (ro, rw diag) */

static int ms820_match(struct device *, struct cfdata *, void *);
static void ms820_attach(struct device *, struct device *, void *);

struct mem_bi_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
};

struct cfattach mem_bi_ca = {
	sizeof(struct mem_bi_softc), ms820_match, ms820_attach
};

static int
ms820_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void	*aux;
{
	struct bi_attach_args *ba = aux;

	if (bus_space_read_2(ba->ba_iot, ba->ba_ioh, BIREG_DTYPE) != BIDT_MS820)
		return 0;

	if (cf->cf_loc[BICF_NODE] != BICF_NODE_DEFAULT &&
	    cf->cf_loc[BICF_NODE] != ba->ba_nodenr)
		return 0;

	return 1;
}

static void
ms820_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct mem_bi_softc *sc = (void *)self;
	struct bi_attach_args *ba = aux;

	sc->sc_iot = ba->ba_iot;
	sc->sc_ioh = ba->ba_ioh;

	if ((MEMRD(BIREG_VAXBICSR) & BICSR_STS) == 0)
		printf(": failed self test\n");
	else
		printf(": size %dMB, %s chips\n", ((MEMRD(MSREG_CSR1) & 
		    MS1_MSIZEMASK) >> 20), (MEMRD(MSREG_CSR1) & MS1_RAMTYMASK
		    ? MEMRD(MSREG_CSR1) & MS1_RAMTY256K ? "256K":"1M":"64K"));

	MEMWR(BIREG_INTRDES, ba->ba_intcpu);
	MEMWR(BIREG_VAXBICSR, MEMRD(BIREG_VAXBICSR) | BICSR_SEIE | BICSR_HEIE);

	MEMWR(MSREG_CSR1, MS1_MWRITEERR | MS1_CNTLERR);
	MEMWR(MSREG_CSR2, MS2_RDSERR | MS2_HIERR | MS2_CRDERR | MS2_ADRSERR);
}

void
ka820_memerr()
{
	struct mem_bi_softc *sc;
	int m, hard, csr1, csr2;
	char *type;
static char b1[] = "\20\40ERRSUM\37ECCDIAG\36ECCDISABLE\20CRDINH\17VALID\
\16INTLK\15BROKE\13MWRITEERR\12CNTLERR\11INTLV";
static char b2[] = "\20\40RDS\37HIERR\36CRD\35ADRS";

	for (m = 0; m < mem_cd.cd_ndevs; m++) {
		sc = mem_cd.cd_devs[m];
		if (sc == NULL)
			continue;
		csr1 = MEMRD(MSREG_CSR1);
		csr2 = MEMRD(MSREG_CSR2);
		printf("%s: csr1=%b csr2=%b\n", sc->sc_dev.dv_xname,
		    csr1, b1, csr2, b2);
		if ((csr1 & MS1_ERRSUM) == 0)
			continue;
		hard = 1;
		if (csr1 & MS1_BROKE)
			type = "broke";
		else if (csr1 & MS1_CNTLERR)
			type = "cntl err";
		else if (csr2 & MS2_ADRSERR)
			type = "address parity err";
		else if (csr2 & MS2_RDSERR)
			type = "rds err";
		else if (csr2 & MS2_CRDERR) {
			hard = 0;
			type = "";
		} else
			type = "mysterious error";
		printf("%s: %s%s%s addr %x bank %x syn %x\n",
		    sc->sc_dev.dv_xname, hard ? "hard error: " : "soft ecc",
		    type, csr2 & MS2_HIERR ?  " (+ other rds or crd err)" : "",
		    ((csr2 & MS2_ADDR) + MEMRD(BIREG_SADR)) >> 9,
		    (csr2 & MS2_INTLVADDR) != 0, csr2 & MS2_SYN);
		MEMWR(MSREG_CSR1, csr1 | MS1_CRDINH);
		MEMWR(MSREG_CSR2, csr2);
	}
}

/* these are bits 0 to 6 in the summary field */
char *mc8200[] = {
	"cpu bad ipl",		"ucode lost err",
	"ucode par err",	"DAL par err",
	"BI bus err",		"BTB tag par",
	"cache tag par",
};
#define MC8200_BADIPL	0x01
#define MC8200_UERR	0x02
#define MC8200_UPAR	0x04
#define MC8200_DPAR	0x08
#define MC8200_BIERR	0x10
#define MC8200_BTAGPAR	0x20
#define MC8200_CTAGPAR	0x40

struct mc8200frame {
	int	mc82_bcnt;		/* byte count == 0x20 */
	int	mc82_summary;		/* summary parameter */
	int	mc82_param1;		/* parameter 1 */
	int	mc82_va;		/* va register */
	int	mc82_vap;		/* va prime register */
	int	mc82_ma;		/* memory address */
	int	mc82_status;		/* status word */
	int	mc82_epc;		/* error pc */
	int	mc82_upc;		/* micro pc */
	int	mc82_pc;		/* current pc */
	int	mc82_psl;		/* current psl */
};

int
ka820_mchk(cmcf)
	caddr_t cmcf;
{
	register struct mc8200frame *mcf = (struct mc8200frame *)cmcf;
	register int i, type = mcf->mc82_summary;

	/* ignore BI bus errors during configuration */
	if (cold && type == MC8200_BIERR) {
		mtpr(PR_MCESR, 0xf);
		return (MCHK_RECOVERED);
	}

	/*
	 * SOME ERRORS ARE RECOVERABLE
	 * do it later
	 */
	printf("machine check %x: ", type);
	for (i = 0; i < sizeof (mc8200) / sizeof (mc8200[0]); i++)
		if (type & (1 << i))
			printf(" %s,", mc8200[i]);
	printf(" param1 %x\n", mcf->mc82_param1);
	printf(
"\tva %x va' %x ma %x pc %x psl %x\n\tstatus %x errpc %x upc %x\n",
		mcf->mc82_va, mcf->mc82_vap, mcf->mc82_ma,
		mcf->mc82_pc, mcf->mc82_psl,
		mcf->mc82_status, mcf->mc82_epc, mcf->mc82_upc);
	return (MCHK_PANIC);
}

/*
 * Receive a character from logical console.
 */
void
rxcdintr(arg)
	void *arg;
{
	register int c = mfpr(PR_RXCD);

	/* not sure what (if anything) to do with these */
	printf("rxcd node %x c=0x%x\n", (c >> 8) & 0xf, c & 0xff);
}

int
ka820_clkread(time_t base)
{
	struct clock_ymdhms c;
	int s;

	while (ka820_clkpage->csr0 & KA820CLK_0_BUSY)
		;
	s = splhigh();
	c.dt_sec = ka820_clkpage->sec;
	c.dt_min = ka820_clkpage->min;
	c.dt_hour = ka820_clkpage->hr;
	c.dt_wday = ka820_clkpage->dayofwk;
	c.dt_day = ka820_clkpage->day;
	c.dt_mon = ka820_clkpage->mon;
	c.dt_year = ka820_clkpage->yr;
	splx(s);

	/* strange conversion */
	c.dt_sec = ((c.dt_sec << 7) | (c.dt_sec >> 1)) & 0377;
	c.dt_min = ((c.dt_min << 7) | (c.dt_min >> 1)) & 0377;
	c.dt_hour = ((c.dt_hour << 7) | (c.dt_hour >> 1)) & 0377;
	c.dt_wday = ((c.dt_wday << 7) | (c.dt_wday >> 1)) & 0377;
	c.dt_day = ((c.dt_day << 7) | (c.dt_day >> 1)) & 0377;
	c.dt_mon = ((c.dt_mon << 7) | (c.dt_mon >> 1)) & 0377;
	c.dt_year = ((c.dt_year << 7) | (c.dt_year >> 1)) & 0377;

	time.tv_sec = clock_ymdhms_to_secs(&c);
	return CLKREAD_OK;
}

void
ka820_clkwrite(void)
{
	struct clock_ymdhms c;

	clock_secs_to_ymdhms(time.tv_sec, &c);

	ka820_clkpage->csr1 = KA820CLK_1_SET;
	ka820_clkpage->sec = ((c.dt_sec << 1) | (c.dt_sec >> 7)) & 0377;
	ka820_clkpage->min = ((c.dt_min << 1) | (c.dt_min >> 7)) & 0377;
	ka820_clkpage->hr = ((c.dt_hour << 1) | (c.dt_hour >> 7)) & 0377;
	ka820_clkpage->dayofwk = ((c.dt_wday << 1) | (c.dt_wday >> 7)) & 0377;
	ka820_clkpage->day = ((c.dt_day << 1) | (c.dt_day >> 7)) & 0377;
	ka820_clkpage->mon = ((c.dt_mon << 1) | (c.dt_mon >> 7)) & 0377;
	ka820_clkpage->yr = ((c.dt_year << 1) | (c.dt_year >> 7)) & 0377;

	ka820_clkpage->csr1 = KA820CLK_1_GO;
}
