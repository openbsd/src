/*	$OpenBSD: if_fe.c,v 1.22 2005/02/17 19:05:36 miod Exp $	*/

/*
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions copyright (C) 1993, David Greenman.  This software may be used,
 * modified, copied, distributed, and sold, in both source and binary form
 * provided that the above copyright and these terms are retained.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

#define FE_VERSION "if_fe.c ver. 0.8"

/*
 * Device driver for Fujitsu MB86960A/MB86965A based Ethernet cards.
 * Contributed by M.S. <seki@sysrap.cs.fujitsu.co.jp>
 *
 * This version is intended to be a generic template for various
 * MB86960A/MB86965A based Ethernet cards.  It currently supports
 * Fujitsu FMV-180 series (i.e., FMV-181 and FMV-182) and Allied-
 * Telesis AT1700 series and RE2000 series.  There are some
 * unnecessary hooks embedded, which are primarily intended to support
 * other types of Ethernet cards, but the author is not sure whether
 * they are useful.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/ic/mb86960reg.h>
#include <dev/isa/if_fereg.h>

/*
 * Default settings for fe driver specific options.
 * They can be set in config file by "options" statements.
 */

/*
 * Debug control.
 * 0: No debug at all.  All debug specific codes are stripped off.
 * 1: Silent.  No debug messages are logged except emergent ones.
 * 2: Brief.  Lair events and/or important information are logged.
 * 3: Detailed.  Logs all information which *may* be useful for debugging.
 * 4: Trace.  All actions in the driver is logged.  Super verbose.
 */
#ifndef FE_DEBUG
#define FE_DEBUG		1
#endif

/*
 * Delay padding of short transmission packets to minimum Ethernet size.
 * This may or may not gain performance.  An EXPERIMENTAL option.
 */
#ifndef FE_DELAYED_PADDING
#define FE_DELAYED_PADDING	0
#endif

/*
 * Transmit just one packet per a "send" command to 86960.
 * This option is intended for performance test.  An EXPERIMENTAL option.
 */
#ifndef FE_SINGLE_TRANSMISSION
#define FE_SINGLE_TRANSMISSION	0
#endif

/*
 * Device configuration flags.
 */

/* DLCR6 settings. */
#define FE_FLAGS_DLCR6_VALUE	0x007F

/* Force DLCR6 override. */
#define FE_FLAGS_OVERRIDE_DLCR6	0x0080

/* A cludge for PCMCIA support. */
#define FE_FLAGS_PCMCIA		0x8000

/* Identification of the driver version. */
static char const fe_version[] = FE_VERSION " / " FE_REG_VERSION;

/*
 * Supported hardware (Ethernet card) types
 * This information is currently used only for debugging
 */
enum fe_type {
	/* For cards which are successfully probed but not identified. */
	FE_TYPE_UNKNOWN,

	/* Fujitsu FMV-180 series. */
	FE_TYPE_FMV181,
	FE_TYPE_FMV182,

	/* Allied-Telesis AT1700 series and RE2000 series. */
	FE_TYPE_AT1700T,
	FE_TYPE_AT1700BT,
	FE_TYPE_AT1700FT,
	FE_TYPE_AT1700AT,
	FE_TYPE_RE2000,

	/* PCMCIA by Fujitsu. */
	FE_TYPE_MBH10302,
	FE_TYPE_MBH10304,
};

/*
 * fe_softc: per line info and status
 */
struct fe_softc {
	struct	device sc_dev;
	void	*sc_ih;

	struct	arpcom sc_arpcom;	/* ethernet common */

	/* Set by probe() and not modified in later phases. */
	enum	fe_type type;	/* interface type code */
	char	*typestr;	/* printable name of the interface. */
	int	sc_iobase;	/* MB86960A I/O base address */

	u_char	proto_dlcr4;	/* DLCR4 prototype. */
	u_char	proto_dlcr5;	/* DLCR5 prototype. */
	u_char	proto_dlcr6;	/* DLCR6 prototype. */
	u_char	proto_dlcr7;	/* DLCR7 prototype. */
	u_char	proto_bmpr13;	/* BMPR13 prototype. */

	/* Vendor specific hooks. */
	void	(*init)(struct fe_softc *); /* Just before fe_init(). */
	void	(*stop)(struct fe_softc *); /* Just after fe_stop(). */

	/* Transmission buffer management. */
	u_short	txb_size;	/* total bytes in TX buffer */
	u_short	txb_free;	/* free bytes in TX buffer */
	u_char	txb_count;	/* number of packets in TX buffer */
	u_char	txb_sched;	/* number of scheduled packets */
	u_char	txb_padding;	/* number of delayed padding bytes */

	/* Multicast address filter management. */
	u_char	filter_change;	/* MARs must be changed ASAP. */
	u_char	filter[FE_FILTER_LEN];	/* new filter value. */
};

/* Frequently accessed members in arpcom. */
#define sc_enaddr	sc_arpcom.ac_enaddr

/* Standard driver entry points.  These can be static. */
int	feprobe(struct device *, void *, void *);
void	feattach(struct device *, struct device *, void *);
int	feintr(void *);
void	fe_init(struct fe_softc *);
int	fe_ioctl(struct ifnet *, u_long, caddr_t);
void	fe_start(struct ifnet *);
void	fe_reset(struct fe_softc *);
void	fe_watchdog(struct ifnet *);

/* Local functions.  Order of declaration is confused.  FIXME. */
int	fe_probe_fmv(struct fe_softc *, struct isa_attach_args *);
int	fe_probe_ati(struct fe_softc *, struct isa_attach_args *);
int	fe_probe_mbh(struct fe_softc *, struct isa_attach_args *);
void	fe_init_mbh(struct fe_softc *);
int	fe_get_packet(struct fe_softc *, int);
void	fe_stop(struct fe_softc *);
void	fe_tint(/*struct fe_softc *, u_char*/);
void	fe_rint(/*struct fe_softc *, u_char*/);
static inline
void	fe_xmit(struct fe_softc *);
void	fe_write_mbufs(struct fe_softc *, struct mbuf *);
void	fe_getmcaf(struct arpcom *, u_char *);
void	fe_setmode(struct fe_softc *);
void	fe_loadmar(struct fe_softc *);
#if FE_DEBUG >= 1
void	fe_dump(int, struct fe_softc *);
#endif

struct cfattach fe_ca = {
	sizeof(struct fe_softc), feprobe, feattach
};

struct cfdriver fe_cd = {
	NULL, "fe", DV_IFNET
};

/* Ethernet constants.  To be defined in if_ehter.h?  FIXME. */
#define ETHER_MIN_LEN	60	/* with header, without CRC. */
#define ETHER_MAX_LEN	1514	/* with header, without CRC. */

/*
 * Fe driver specific constants which relate to 86960/86965.
 */

/* Interrupt masks. */
#define FE_TMASK (FE_D2_COLL16 | FE_D2_TXDONE)
#define FE_RMASK (FE_D3_OVRFLO | FE_D3_CRCERR | \
		  FE_D3_ALGERR | FE_D3_SRTPKT | FE_D3_PKTRDY)

/* Maximum number of iterrations for a receive interrupt. */
#define FE_MAX_RECV_COUNT ((65536 - 2048 * 2) / 64)
	/* Maximum size of SRAM is 65536,
	 * minimum size of transmission buffer in fe is 2x2KB,
	 * and minimum amount of received packet including headers
	 * added by the chip is 64 bytes.
	 * Hence FE_MAX_RECV_COUNT is the upper limit for number
	 * of packets in the receive buffer. */

/*
 * Convenient routines to access contiguous I/O ports.
 */

static inline void
inblk (int addr, u_char * mem, int len)
{
	while (--len >= 0) {
		*mem++ = inb(addr++);
	}
}

static inline void
outblk (int addr, u_char const * mem, int len)
{
	while (--len >= 0) {
		outb(addr++, *mem++);
	}
}

/*
 * Hardware probe routines.
 */

/*
 * Determine if the device is present.
 */
int
feprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct fe_softc *sc = match;
	struct isa_attach_args *ia = aux;

#if FE_DEBUG >= 2
	log(LOG_INFO, "%s: %s\n", sc->sc_dev.dv_xname, fe_version);
#endif

	/* Probe an address. */
	sc->sc_iobase = ia->ia_iobase;

	if (fe_probe_fmv(sc, ia))
		return (1);
	if (fe_probe_ati(sc, ia))
		return (1);
	if (fe_probe_mbh(sc, ia))
		return (1);
	return (0);
}

/*
 * Check for specific bits in specific registers have specific values.
 */
struct fe_simple_probe_struct {
	u_char port;	/* Offset from the base I/O address. */
	u_char mask;	/* Bits to be checked. */
	u_char bits;	/* Values to be compared against. */
};

static inline int
fe_simple_probe (int addr, struct fe_simple_probe_struct const * sp)
{
	struct fe_simple_probe_struct const * p;

	for (p = sp; p->mask != 0; p++) {
		if ((inb(addr + p->port) & p->mask) != p->bits) {
			return (0);
		}
	}
	return (1);
}

/*
 * Routines to read all bytes from the config EEPROM through MB86965A.
 * I'm not sure what exactly I'm doing here...  I was told just to follow
 * the steps, and it worked.  Could someone tell me why the following
 * code works?  (Or, why all similar codes I tried previously doesn't
 * work.)  FIXME.
 */

static inline void
strobe (int bmpr16)
{
	/*
	 * Output same value twice.  To speed-down execution?
	 */
	outb(bmpr16, FE_B16_SELECT);
	outb(bmpr16, FE_B16_SELECT);
	outb(bmpr16, FE_B16_SELECT | FE_B16_CLOCK);
	outb(bmpr16, FE_B16_SELECT | FE_B16_CLOCK);
	outb(bmpr16, FE_B16_SELECT);
	outb(bmpr16, FE_B16_SELECT);
}

void
fe_read_eeprom(sc, data)
	struct fe_softc *sc;
	u_char *data;
{
	int iobase = sc->sc_iobase;
	int bmpr16 = iobase + FE_BMPR16;
	int bmpr17 = iobase + FE_BMPR17;
	u_char n, val, bit;

	/* Read bytes from EEPROM; two bytes per an iterration. */
	for (n = 0; n < FE_EEPROM_SIZE / 2; n++) {
		/* Reset the EEPROM interface. */
		outb(bmpr16, 0x00);
		outb(bmpr17, 0x00);
		outb(bmpr16, FE_B16_SELECT);

		/* Start EEPROM access. */
		outb(bmpr17, FE_B17_DATA);
		strobe(bmpr16);

		/* Pass the iterration count to the chip. */
		val = 0x80 | n;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			outb(bmpr17, (val & bit) ? FE_B17_DATA : 0);
			strobe(bmpr16);
		}
		outb(bmpr17, 0x00);

		/* Read a byte. */
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			strobe(bmpr16);
			if (inb(bmpr17) & FE_B17_DATA)
				val |= bit;
		}
		*data++ = val;

		/* Read one more byte. */
		val = 0;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			strobe(bmpr16);
			if (inb(bmpr17) & FE_B17_DATA)
				val |= bit;
		}
		*data++ = val;
	}

#if FE_DEBUG >= 3
	/* Report what we got. */
	data -= FE_EEPROM_SIZE;
	log(LOG_INFO, "%s: EEPROM at %04x:"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x -"
	    " %02x%02x%02x%02x %02x%02x%02x%02x\n",
	    sc->sc_dev.dv_xname, iobase,
	    data[ 0], data[ 1], data[ 2], data[ 3],
	    data[ 4], data[ 5], data[ 6], data[ 7],
	    data[ 8], data[ 9], data[10], data[11],
	    data[12], data[13], data[14], data[15],
	    data[16], data[17], data[18], data[19],
	    data[20], data[21], data[22], data[23],
	    data[24], data[25], data[26], data[27],
	    data[28], data[29], data[30], data[31]);
#endif
}

/*
 * Hardware (vendor) specific probe routines.
 */

/*
 * Probe and initialization for Fujitsu FMV-180 series boards
 */
int
fe_probe_fmv(sc, ia)
	struct fe_softc *sc;
	struct isa_attach_args *ia;
{
	int i, n;
	int iobase = sc->sc_iobase;
	int irq;

	static int const iomap[8] =
		{ 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x300, 0x340 };
	static int const irqmap[4] =
		{ 3, 7, 10, 15 };

	static struct fe_simple_probe_struct const probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Doesn't work. */

		{ FE_FMV0, FE_FMV0_MAGIC_MASK,  FE_FMV0_MAGIC_VALUE },
		{ FE_FMV1, FE_FMV1_CARDID_MASK, FE_FMV1_CARDID_ID   },
		{ FE_FMV3, FE_FMV3_EXTRA_MASK,  FE_FMV3_EXTRA_VALUE },
#if 1
	/*
	 * Test *vendor* part of the station address for Fujitsu.
	 * The test will gain reliability of probe process, but
	 * it rejects FMV-180 clone boards manufactured by other vendors.
	 * We have to turn the test off when such cards are made available.
	 */
		{ FE_FMV4, 0xFF, 0x00 },
		{ FE_FMV5, 0xFF, 0x00 },
		{ FE_FMV6, 0xFF, 0x0E },
#else
	/*
	 * We can always verify the *first* 2 bits (in Ethernet
	 * bit order) are "no multicast" and "no local" even for
	 * unknown vendors.
	 */
		{ FE_FMV4, 0x03, 0x00 },
#endif
		{ 0 }
	};

#if 0
	/*
	 * Dont probe at all if the config says we are PCMCIA...
	 */
	if ((cf->cf_flags & FE_FLAGS_PCMCIA) != 0)
		return (0);
#endif

	/*
	 * See if the sepcified address is possible for FMV-180 series.
	 */
	for (i = 0; i < 8; i++) {
		if (iomap[i] == iobase)
			break;
	}
	if (i == 8)
		return (0);

	/* Simple probe. */
	if (!fe_simple_probe(iobase, probe_table))
		return (0);

	/* Check if our I/O address matches config info on EEPROM. */
	n = (inb(iobase + FE_FMV2) & FE_FMV2_ADDR) >> FE_FMV2_ADDR_SHIFT;
	if (iomap[n] != iobase)
		return (0);

	/* Determine the card type. */
	switch (inb(iobase + FE_FMV0) & FE_FMV0_MODEL) {
	case FE_FMV0_MODEL_FMV181:
		sc->type = FE_TYPE_FMV181;
		sc->typestr = "FMV-181";
		break;
	case FE_FMV0_MODEL_FMV182:
		sc->type = FE_TYPE_FMV182;
		sc->typestr = "FMV-182";
		break;
	default:
	  	/* Unknown card type: maybe a new model, but... */
		return (0);
	}

	/*
	 * An FMV-180 has successfully been proved.
	 * Determine which IRQ to be used.
	 *
	 * In this version, we always get an IRQ assignment from the
	 * FMV-180's configuration EEPROM, ignoring that specified in
	 * config file.
	 */
	n = (inb(iobase + FE_FMV2) & FE_FMV2_IRQ) >> FE_FMV2_IRQ_SHIFT;
	irq = irqmap[n];

	if (ia->ia_irq != IRQUNK) {
		if (ia->ia_irq != irq) {
			printf("%s: irq mismatch; kernel configured %d != board configured %d\n",
			    sc->sc_dev.dv_xname, ia->ia_irq, irq);
			return (0);
		}
	} else
		ia->ia_irq = irq;

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM. */
	inblk(iobase + FE_FMV4, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((sc->sc_enaddr[0] & 0x03) != 0x00
	  || (sc->sc_enaddr[0] == 0x00
	    && sc->sc_enaddr[1] == 0x00
	    && sc->sc_enaddr[2] == 0x00))
		return (0);

	/* Register values which depend on board design. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/*
	 * Minimum initialization of the hardware.
	 * We write into registers; hope I/O ports have no
	 * overlap with other boards.
	 */

	/* Initialize ASIC. */
	outb(iobase + FE_FMV3, 0);
	outb(iobase + FE_FMV10, 0);

	/* Wait for a while.  I'm not sure this is necessary.  FIXME. */
	delay(200);

	/* Initialize 86960. */
	outb(iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(200);

	/* Disable all interrupts. */
	outb(iobase + FE_DLCR2, 0);
	outb(iobase + FE_DLCR3, 0);

	/* Turn the "master interrupt control" flag of ASIC on. */
	outb(iobase + FE_FMV3, FE_FMV3_ENABLE_FLAG);

	/*
	 * That's all.  FMV-180 occupies 32 I/O addresses, by the way.
	 */
	ia->ia_iosize = 32;
	ia->ia_msize = 0;
	return (1);
}

/*
 * Probe and initialization for Allied-Telesis AT1700/RE2000 series.
 */
int
fe_probe_ati(sc, ia)
	struct fe_softc *sc;
	struct isa_attach_args *ia;
{
	int i, n;
	int iobase = sc->sc_iobase;
	u_char eeprom[FE_EEPROM_SIZE];
	u_char save16, save17;
	int irq;

	static int const iomap[8] =
		{ 0x260, 0x280, 0x2A0, 0x240, 0x340, 0x320, 0x380, 0x300 };
	static int const irqmap[4][4] = {
		{  3,  4,  5,  9 },
		{ 10, 11, 12, 15 },
		{  3, 11,  5, 15 },
		{ 10, 11, 14, 15 },
	};
	static struct fe_simple_probe_struct const probe_table[] = {
		{ FE_DLCR2,  0x70, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
		{ FE_DLCR5,  0x80, 0x00 },
#if 0
		{ FE_BMPR16, 0x1B, 0x00 },
		{ FE_BMPR17, 0x7F, 0x00 },
#endif
		{ 0 }
	};

#if 0
	/*
	 * Don't probe at all if the config says we are PCMCIA...
	 */
	if ((cf->cf_flags & FE_FLAGS_PCMCIA) != 0)
		return (0);
#endif

#if FE_DEBUG >= 4
	log(LOG_INFO, "%s: probe (0x%x) for ATI\n", sc->sc_dev.dv_xname, iobase);
	fe_dump(LOG_INFO, sc);
#endif

	/*
	 * See if the sepcified address is possible for MB86965A JLI mode.
	 */
	for (i = 0; i < 8; i++) {
		if (iomap[i] == iobase)
			break;
	}
	if (i == 8)
		return (0);

	/*
	 * We should test if MB86965A is on the base address now.
	 * Unfortunately, it is very hard to probe it reliably, since
	 * we have no way to reset the chip under software control.
	 * On cold boot, we could check the "signature" bit patterns
	 * described in the Fujitsu document.  On warm boot, however,
	 * we can predict almost nothing about register values.
	 */
	if (!fe_simple_probe(iobase, probe_table))
		return (0);

	/* Save old values of the registers. */
	save16 = inb(iobase + FE_BMPR16);
	save17 = inb(iobase + FE_BMPR17);

	/* Check if our I/O address matches config info on 86965. */
	n = (inb(iobase + FE_BMPR19) & FE_B19_ADDR) >> FE_B19_ADDR_SHIFT;
	if (iomap[n] != iobase)
		goto fail;

	/*
	 * We are now almost sure we have an AT1700 at the given
	 * address.  So, read EEPROM through 86965.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presense of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	fe_read_eeprom(sc, eeprom);

	/* Make sure the EEPROM is turned off. */
	outb(iobase + FE_BMPR16, 0);
	outb(iobase + FE_BMPR17, 0);

	/* Make sure that config info in EEPROM and 86965 agree. */
	if (eeprom[FE_EEPROM_CONF] != inb(iobase + FE_BMPR19))
		goto fail;

	/*
	 * Determine the card type.
	 */
	switch (eeprom[FE_ATI_EEP_MODEL]) {
	case FE_ATI_MODEL_AT1700T:
		sc->type = FE_TYPE_AT1700T;
		sc->typestr = "AT-1700T";
		break;
	case FE_ATI_MODEL_AT1700BT:
		sc->type = FE_TYPE_AT1700BT;
		sc->typestr = "AT-1700BT";
		break;
	case FE_ATI_MODEL_AT1700FT:
		sc->type = FE_TYPE_AT1700FT;
		sc->typestr = "AT-1700FT";
		break;
	case FE_ATI_MODEL_AT1700AT:
		sc->type = FE_TYPE_AT1700AT;
		sc->typestr = "AT-1700AT";
		break;
	default:
		sc->type = FE_TYPE_RE2000;
		sc->typestr = "unknown (RE-2000?)";
		break;
	}

	/*
	 * Try to determine IRQ settings.
	 * Different models use different ranges of IRQs.
	 */
	n = (inb(iobase + FE_BMPR19) & FE_B19_IRQ) >> FE_B19_IRQ_SHIFT;
	switch (eeprom[FE_ATI_EEP_REVISION] & 0xf0) {
	case 0x30:
		irq = irqmap[3][n];
		break;
	case 0x10:
	case 0x50:
		irq = irqmap[2][n];
		break;
	case 0x40:
	case 0x60:
		if (eeprom[FE_ATI_EEP_MAGIC] & 0x04) {
			irq = irqmap[1][n];
			break;
		}
	default:
		irq = irqmap[0][n];
		break;
	}

	if (ia->ia_irq != IRQUNK) {
		if (ia->ia_irq != irq) {
			printf("%s: irq mismatch; kernel configured %d != board configured %d\n",
			    sc->sc_dev.dv_xname, ia->ia_irq, irq);
			return (0);
		}
	} else
		ia->ia_irq = irq;

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM. */
	bcopy(eeprom + FE_ATI_EEP_ADDR, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((sc->sc_enaddr[0] & 0x03) != 0x00
	  || (sc->sc_enaddr[0] == 0x00
	    && sc->sc_enaddr[1] == 0x00
	    && sc->sc_enaddr[2] == 0x00))
		goto fail;

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;  /* FIXME */
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;
#if 0	/* XXXX Should we use this? */
	sc->proto_bmpr13 = eeprom[FE_ATI_EEP_MEDIA];
#else
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;
#endif

	/*
	 * Program the 86965 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: ATI found\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/* Initialize 86965. */
	outb(iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(200);

	/* Disable all interrupts. */
	outb(iobase + FE_DLCR2, 0);
	outb(iobase + FE_DLCR3, 0);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: end of fe_probe_ati()\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/*
	 * That's all.  AT1700 occupies 32 I/O addresses, by the way.
	 */
	ia->ia_iosize = 32;
	ia->ia_msize = 0;
	return (1);

fail:
	/* Restore register values, in the case we had no 86965. */
	outb(iobase + FE_BMPR16, save16);
	outb(iobase + FE_BMPR17, save17);
	return (0);
}

/*
 * Probe and initialization for Fujitsu MBH10302 PCMCIA Ethernet interface.
 */
int
fe_probe_mbh(sc, ia)
	struct fe_softc *sc;
	struct isa_attach_args *ia;
{
	int iobase = sc->sc_iobase;

	static struct fe_simple_probe_struct probe_table[] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well. */
#if 0
	/*
	 * Test *vendor* part of the address for Fujitsu.
	 * The test will gain reliability of probe process, but
	 * it rejects clones by other vendors, or OEM product
	 * supplied by resalers other than Fujitsu.
	 */
		{ FE_MBH10, 0xFF, 0x00 },
		{ FE_MBH11, 0xFF, 0x00 },
		{ FE_MBH12, 0xFF, 0x0E },
#else
	/*
	 * We can always verify the *first* 2 bits (in Ethernet
	 * bit order) are "global" and "unicast" even for
	 * unknown vendors.
	 */
		{ FE_MBH10, 0x03, 0x00 },
#endif
        /* Just a gap?  Seems reliable, anyway. */
		{ 0x12, 0xFF, 0x00 },
		{ 0x13, 0xFF, 0x00 },
		{ 0x14, 0xFF, 0x00 },
		{ 0x15, 0xFF, 0x00 },
		{ 0x16, 0xFF, 0x00 },
		{ 0x17, 0xFF, 0x00 },
		{ 0x18, 0xFF, 0xFF },
		{ 0x19, 0xFF, 0xFF },

		{ 0 }
	};

#if 0
	/*
	 * We need a PCMCIA flag.
	 */
	if ((cf->cf_flags & FE_FLAGS_PCMCIA) == 0)
		return (0);
#endif

	/*
	 * We need explicit IRQ and supported address.
	 */
	if (ia->ia_irq == IRQUNK || (iobase & ~0x3E0) != 0)
		return (0);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: top of fe_probe_mbh()\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/*
	 * See if MBH10302 is on its address.
	 * I'm not sure the following probe code works.  FIXME.
	 */
	if (!fe_simple_probe(iobase, probe_table))
		return (0);

	/* Determine the card type. */
	sc->type = FE_TYPE_MBH10302;
	sc->typestr = "MBH10302 (PCMCIA)";

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM. */
	inblk(iobase + FE_MBH10, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((sc->sc_enaddr[0] & 0x03) != 0x00
	  || (sc->sc_enaddr[0] == 0x00
	    && sc->sc_enaddr[1] == 0x00
	    && sc->sc_enaddr[2] == 0x00))
		return (0);

	/* Should find all register prototypes here.  FIXME. */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_NICE;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Setup hooks.  We need a special initialization procedure. */
	sc->init = fe_init_mbh;

	/*
	 * Minimum initialization.
	 */

	/* Wait for a while.  I'm not sure this is necessary.  FIXME. */
	delay(200);

	/* Minimul initialization of 86960. */
	outb(iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(200);

	/* Disable all interrupts. */
	outb(iobase + FE_DLCR2, 0);
	outb(iobase + FE_DLCR3, 0);

#if 1	/* FIXME. */
	/* Initialize system bus interface and encoder/decoder operation. */
	outb(iobase + FE_MBH0, FE_MBH0_MAGIC | FE_MBH0_INTR_DISABLE);
#endif

	/*
	 * That's all.  MBH10302 occupies 32 I/O addresses, by the way.
	 */
	ia->ia_iosize = 32;
	ia->ia_msize = 0;
	return (1);
}

/* MBH specific initialization routine. */
void
fe_init_mbh(sc)
	struct fe_softc *sc;
{

	/* Probably required after hot-insertion... */

	/* Wait for a while.  I'm not sure this is necessary.  FIXME. */
	delay(200);

	/* Minimul initialization of 86960. */
	outb(sc->sc_iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(200);

	/* Disable all interrupts. */
	outb(sc->sc_iobase + FE_DLCR2, 0);
	outb(sc->sc_iobase + FE_DLCR3, 0);

	/* Enable master interrupt flag. */
	outb(sc->sc_iobase + FE_MBH0, FE_MBH0_MAGIC | FE_MBH0_INTR_ENABLE);
}

/*
 * Install interface into kernel networking data structures
 */
void
feattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fe_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct cfdata *cf = sc->sc_dev.dv_cfdata;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	/* Stop the 86960. */
	fe_stop(sc);

	/* Initialize ifnet structure. */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = fe_start;
	ifp->if_ioctl = fe_ioctl;
	ifp->if_watchdog = fe_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: feattach()\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

#if FE_SINGLE_TRANSMISSION
	/* Override txb config to allocate minimum. */
	sc->proto_dlcr6 &= ~FE_D6_TXBSIZ
	sc->proto_dlcr6 |=  FE_D6_TXBSIZ_2x2KB;
#endif

	/* Modify hardware config if it is requested. */
	if ((cf->cf_flags & FE_FLAGS_OVERRIDE_DLCR6) != 0)
		sc->proto_dlcr6 = cf->cf_flags & FE_FLAGS_DLCR6_VALUE;

	/* Find TX buffer size, based on the hardware dependent proto. */
	switch (sc->proto_dlcr6 & FE_D6_TXBSIZ) {
	case FE_D6_TXBSIZ_2x2KB:
		sc->txb_size = 2048;
		break;
	case FE_D6_TXBSIZ_2x4KB:
		sc->txb_size = 4096;
		break;
	case FE_D6_TXBSIZ_2x8KB:
		sc->txb_size = 8192;
		break;
	default:
		/* Oops, we can't work with single buffer configuration. */
#if FE_DEBUG >= 2
		log(LOG_WARNING, "%s: strange TXBSIZ config; fixing\n",
		    sc->sc_dev.dv_xname);
#endif
		sc->proto_dlcr6 &= ~FE_D6_TXBSIZ;
		sc->proto_dlcr6 |=  FE_D6_TXBSIZ_2x2KB;
		sc->txb_size = 2048;
		break;
	}

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	/* Print additional info when attached. */
	printf(": address %s, type %s\n",
	    ether_sprintf(sc->sc_arpcom.ac_enaddr), sc->typestr);
#if FE_DEBUG >= 3
	{
		int buf, txb, bbw, sbw, ram;

		buf = txb = bbw = sbw = ram = -1;
		switch (sc->proto_dlcr6 & FE_D6_BUFSIZ) {
		case FE_D6_BUFSIZ_8KB:
			buf = 8;
			break;
		case FE_D6_BUFSIZ_16KB:
			buf = 16;
			break;
		case FE_D6_BUFSIZ_32KB:
			buf = 32;
			break;
		case FE_D6_BUFSIZ_64KB:
			buf = 64;
			break;
		}
		switch (sc->proto_dlcr6 & FE_D6_TXBSIZ) {
		case FE_D6_TXBSIZ_2x2KB:
			txb = 2;
			break;
		case FE_D6_TXBSIZ_2x4KB:
			txb = 4;
			break;
		case FE_D6_TXBSIZ_2x8KB:
			txb = 8;
			break;
		}
		switch (sc->proto_dlcr6 & FE_D6_BBW) {
		case FE_D6_BBW_BYTE:
			bbw = 8;
			break;
		case FE_D6_BBW_WORD:
			bbw = 16;
			break;
		}
		switch (sc->proto_dlcr6 & FE_D6_SBW) {
		case FE_D6_SBW_BYTE:
			sbw = 8;
			break;
		case FE_D6_SBW_WORD:
			sbw = 16;
			break;
		}
		switch (sc->proto_dlcr6 & FE_D6_SRAM) {
		case FE_D6_SRAM_100ns:
			ram = 100;
			break;
		case FE_D6_SRAM_150ns:
			ram = 150;
			break;
		}
		printf("%s: SRAM %dKB %dbit %dns, TXB %dKBx2, %dbit I/O\n",
		    sc->sc_dev.dv_xname, buf, bbw, ram, txb, sbw);
	}
#endif

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_NET, feintr, sc, sc->sc_dev.dv_xname);
}

/*
 * Reset interface.
 */
void
fe_reset(sc)
	struct fe_softc *sc;
{
	int s;

	s = splnet();
	fe_stop(sc);
	fe_init(sc);
	splx(s);
}

/*
 * Stop everything on the interface.
 *
 * All buffered packets, both transmitting and receiving,
 * if any, will be lost by stopping the interface.
 */
void
fe_stop(sc)
	struct fe_softc *sc;
{

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: top of fe_stop()\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/* Disable interrupts. */
	outb(sc->sc_iobase + FE_DLCR2, 0x00);
	outb(sc->sc_iobase + FE_DLCR3, 0x00);

	/* Stop interface hardware. */
	delay(200);
	outb(sc->sc_iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE);
	delay(200);

	/* Clear all interrupt status. */
	outb(sc->sc_iobase + FE_DLCR0, 0xFF);
	outb(sc->sc_iobase + FE_DLCR1, 0xFF);

	/* Put the chip in stand-by mode. */
	delay(200);
	outb(sc->sc_iobase + FE_DLCR7, sc->proto_dlcr7 | FE_D7_POWER_DOWN);
	delay(200);

	/* MAR loading can be delayed. */
	sc->filter_change = 0;

	/* Call a hook. */
	if (sc->stop)
		sc->stop(sc);

#if DEBUG >= 3
	log(LOG_INFO, "%s: end of fe_stop()\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif
}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
void
fe_watchdog(ifp)
	struct ifnet *ifp;
{
	struct fe_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
#if FE_DEBUG >= 3
	fe_dump(LOG_INFO, sc);
#endif

	/* Record how many packets are lost by this accident. */
	sc->sc_arpcom.ac_if.if_oerrors += sc->txb_sched + sc->txb_count;

	fe_reset(sc);
}

/*
 * Drop (skip) a packet from receive buffer in 86960 memory.
 */
static inline void
fe_droppacket(sc)
	struct fe_softc *sc;
{

	outb(sc->sc_iobase + FE_BMPR14, FE_B14_FILTER | FE_B14_SKIP);
}

/*
 * Initialize device.
 */
void
fe_init(sc)
	struct fe_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int i;

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: top of fe_init()\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/* Reset transmitter flags. */
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;

	sc->txb_free = sc->txb_size;
	sc->txb_count = 0;
	sc->txb_sched = 0;

	/* Call a hook. */
	if (sc->init)
		sc->init(sc);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: after init hook\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/*
	 * Make sure to disable the chip, also.
	 * This may also help re-programming the chip after
	 * hot insertion of PCMCIAs.
	 */
	outb(sc->sc_iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE);

	/* Power up the chip and select register bank for DLCRs. */
	delay(200);
	outb(sc->sc_iobase + FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_DLCR | FE_D7_POWER_UP);
	delay(200);

	/* Feed the station address. */
	outblk(sc->sc_iobase + FE_DLCR8, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Select the BMPR bank for runtime register access. */
	outb(sc->sc_iobase + FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP);

	/* Initialize registers. */
	outb(sc->sc_iobase + FE_DLCR0, 0xFF);	/* Clear all bits. */
	outb(sc->sc_iobase + FE_DLCR1, 0xFF);	/* ditto. */
	outb(sc->sc_iobase + FE_DLCR2, 0x00);
	outb(sc->sc_iobase + FE_DLCR3, 0x00);
	outb(sc->sc_iobase + FE_DLCR4, sc->proto_dlcr4);
	outb(sc->sc_iobase + FE_DLCR5, sc->proto_dlcr5);
	outb(sc->sc_iobase + FE_BMPR10, 0x00);
	outb(sc->sc_iobase + FE_BMPR11, FE_B11_CTRL_SKIP);
	outb(sc->sc_iobase + FE_BMPR12, 0x00);
	outb(sc->sc_iobase + FE_BMPR13, sc->proto_bmpr13);
	outb(sc->sc_iobase + FE_BMPR14, FE_B14_FILTER);
	outb(sc->sc_iobase + FE_BMPR15, 0x00);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: just before enabling DLC\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/* Enable interrupts. */
	outb(sc->sc_iobase + FE_DLCR2, FE_TMASK);
	outb(sc->sc_iobase + FE_DLCR3, FE_RMASK);

	/* Enable transmitter and receiver. */
	delay(200);
	outb(sc->sc_iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_ENABLE);
	delay(200);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: just after enabling DLC\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/*
	 * Make sure to empty the receive buffer.
	 *
	 * This may be redundant, but *if* the receive buffer were full
	 * at this point, the driver would hang.  I have experienced
	 * some strange hangups just after UP.  I hope the following
	 * code solve the problem.
	 *
	 * I have changed the order of hardware initialization.
	 * I think the receive buffer cannot have any packets at this
	 * point in this version.  The following code *must* be
	 * redundant now.  FIXME.
	 */
	for (i = 0; i < FE_MAX_RECV_COUNT; i++) {
		if (inb(sc->sc_iobase + FE_DLCR5) & FE_D5_BUFEMP)
			break;
		fe_droppacket(sc);
	}
#if FE_DEBUG >= 1
	if (i >= FE_MAX_RECV_COUNT) {
		log(LOG_ERR, "%s: cannot empty receive buffer\n",
		    sc->sc_dev.dv_xname);
	}
#endif
#if FE_DEBUG >= 3
	if (i < FE_MAX_RECV_COUNT) {
		log(LOG_INFO, "%s: receive buffer emptied (%d)\n",
		    sc->sc_dev.dv_xname, i);
	}
#endif

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: after ERB loop\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/* Do we need this here? */
	outb(sc->sc_iobase + FE_DLCR0, 0xFF);	/* Clear all bits. */
	outb(sc->sc_iobase + FE_DLCR1, 0xFF);	/* ditto. */

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: after FIXME\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/* Set 'running' flag. */
	ifp->if_flags |= IFF_RUNNING;

	/*
	 * At this point, the interface is runnung properly,
	 * except that it receives *no* packets.  we then call
	 * fe_setmode() to tell the chip what packets to be
	 * received, based on the if_flags and multicast group
	 * list.  It completes the initialization process.
	 */
	fe_setmode(sc);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: after setmode\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/* ...and attempt to start output. */
	fe_start(ifp);

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: end of fe_init()\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif
}

/*
 * This routine actually starts the transmission on the interface
 */
static inline void
fe_xmit(sc)
	struct fe_softc *sc;
{

	/*
	 * Set a timer just in case we never hear from the board again.
	 * We use longer timeout for multiple packet transmission.
	 * I'm not sure this timer value is appropriate.  FIXME.
	 */
	sc->sc_arpcom.ac_if.if_timer = 1 + sc->txb_count;

	/* Update txb variables. */
	sc->txb_sched = sc->txb_count;
	sc->txb_count = 0;
	sc->txb_free = sc->txb_size;

#if FE_DELAYED_PADDING
	/* Omit the postponed padding process. */
	sc->txb_padding = 0;
#endif

	/* Start transmitter, passing packets in TX buffer. */
	outb(sc->sc_iobase + FE_BMPR10, sc->txb_sched | FE_B10_START);
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splnet _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
void
fe_start(ifp)
	struct ifnet *ifp;
{
	struct fe_softc *sc = ifp->if_softc;
	struct mbuf *m;

#if FE_DEBUG >= 1
	/* Just a sanity check. */
	if ((sc->txb_count == 0) != (sc->txb_free == sc->txb_size)) {
		/*
		 * Txb_count and txb_free co-works to manage the
		 * transmission buffer.  Txb_count keeps track of the
		 * used potion of the buffer, while txb_free does unused
		 * potion.  So, as long as the driver runs properly,
		 * txb_count is zero if and only if txb_free is same
		 * as txb_size (which represents whole buffer.)
		 */
		log(LOG_ERR, "%s: inconsistent txb variables (%d, %d)\n",
		    sc->sc_dev.dv_xname, sc->txb_count, sc->txb_free);
		/*
		 * So, what should I do, then?
		 *
		 * We now know txb_count and txb_free contradicts.  We
		 * cannot, however, tell which is wrong.  More
		 * over, we cannot peek 86960 transmission buffer or
		 * reset the transmission buffer.  (In fact, we can
		 * reset the entire interface.  I don't want to do it.)
		 *
		 * If txb_count is incorrect, leaving it as is will cause
		 * sending of gabages after next interrupt.  We have to
		 * avoid it.  Hence, we reset the txb_count here.  If
		 * txb_free was incorrect, resetting txb_count just loose
		 * some packets.  We can live with it.
		 */
		sc->txb_count = 0;
	}
#endif

#if FE_DEBUG >= 1
	/*
	 * First, see if there are buffered packets and an idle
	 * transmitter - should never happen at this point.
	 */
	if ((sc->txb_count > 0) && (sc->txb_sched == 0)) {
		log(LOG_ERR, "%s: transmitter idle with %d buffered packets\n",
		    sc->sc_dev.dv_xname, sc->txb_count);
		fe_xmit(sc);
	}
#endif

	/*
	 * Stop accepting more transmission packets temporarily, when
	 * a filter change request is delayed.  Updating the MARs on
	 * 86960 flushes the transmisstion buffer, so it is delayed
	 * until all buffered transmission packets have been sent
	 * out.
	 */
	if (sc->filter_change) {
		/*
		 * Filter change requst is delayed only when the DLC is
		 * working.  DLC soon raise an interrupt after finishing
		 * the work.
		 */
		goto indicate_active;
	}

	for (;;) {
		/*
		 * See if there is room to put another packet in the buffer.
		 * We *could* do better job by peeking the send queue to
		 * know the length of the next packet.  Current version just
		 * tests against the worst case (i.e., longest packet).  FIXME.
		 * 
		 * When adding the packet-peek feature, don't forget adding a
		 * test on txb_count against QUEUEING_MAX.
		 * There is a little chance the packet count exceeds
		 * the limit.  Assume transmission buffer is 8KB (2x8KB
		 * configuration) and an application sends a bunch of small
		 * (i.e., minimum packet sized) packets rapidly.  An 8KB
		 * buffer can hold 130 blocks of 62 bytes long...
		 */
		if (sc->txb_free < ETHER_MAX_LEN + FE_DATA_LEN_LEN) {
			/* No room. */
			goto indicate_active;
		}

#if FE_SINGLE_TRANSMISSION
		if (sc->txb_count > 0) {
			/* Just one packet per a transmission buffer. */
			goto indicate_active;
		}
#endif

		/*
		 * Get the next mbuf chain for a packet to send.
		 */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == 0) {
			/* No more packets to send. */
			goto indicate_inactive;
		}

#if NBPFILTER > 0
		/* Tap off here if there is a BPF listener. */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/*
		 * Copy the mbuf chain into the transmission buffer.
		 * txb_* variables are updated as necessary.
		 */
		fe_write_mbufs(sc, m);

		m_freem(m);

		/* Start transmitter if it's idle. */
		if (sc->txb_sched == 0)
			fe_xmit(sc);
	}

indicate_inactive:
	/*
	 * We are using the !OACTIVE flag to indicate to
	 * the outside world that we can accept an
	 * additional packet rather than that the
	 * transmitter is _actually_ active.  Indeed, the
	 * transmitter may be active, but if we haven't
	 * filled all the buffers with data then we still
	 * want to accept more.
	 */
	ifp->if_flags &= ~IFF_OACTIVE;
	return;

indicate_active:
	/*
	 * The transmitter is active, and there are no room for
	 * more outgoing packets in the transmission buffer.
	 */
	ifp->if_flags |= IFF_OACTIVE;
	return;
}

/*
 * Transmission interrupt handler
 * The control flow of this function looks silly.  FIXME.
 */
void
fe_tint(sc, tstat)
	struct fe_softc *sc;
	u_char tstat;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int left;
	int col;

	/*
	 * Handle "excessive collision" interrupt.
	 */
	if (tstat & FE_D0_COLL16) {
		/*
		 * Find how many packets (including this collided one)
		 * are left unsent in transmission buffer.
		 */
		left = inb(sc->sc_iobase + FE_BMPR10);

#if FE_DEBUG >= 2
		log(LOG_WARNING, "%s: excessive collision (%d/%d)\n",
		    sc->sc_dev.dv_xname, left, sc->txb_sched);
#endif
#if FE_DEBUG >= 3
		fe_dump(LOG_INFO, sc);
#endif

		/*
		 * Update statistics.
		 */
		ifp->if_collisions += 16;
		ifp->if_oerrors++;
		ifp->if_opackets += sc->txb_sched - left;

		/*
		 * Collision statistics has been updated.
		 * Clear the collision flag on 86960 now to avoid confusion.
		 */
		outb(sc->sc_iobase + FE_DLCR0, FE_D0_COLLID);

		/*
		 * Restart transmitter, skipping the
		 * collided packet.
		 *
		 * We *must* skip the packet to keep network running
		 * properly.  Excessive collision error is an
		 * indication of the network overload.  If we
		 * tried sending the same packet after excessive
		 * collision, the network would be filled with
		 * out-of-time packets.  Packets belonging
		 * to reliable transport (such as TCP) are resent
		 * by some upper layer.
		 */
		outb(sc->sc_iobase + FE_BMPR11,
		    FE_B11_CTRL_SKIP | FE_B11_MODE1);
		sc->txb_sched = left - 1;
	}

	/*
	 * Handle "transmission complete" interrupt.
	 */
	if (tstat & FE_D0_TXDONE) {
		/*
		 * Add in total number of collisions on last
		 * transmission.  We also clear "collision occurred" flag
		 * here.
		 *
		 * 86960 has a design flow on collision count on multiple
		 * packet transmission.  When we send two or more packets
		 * with one start command (that's what we do when the
		 * transmission queue is clauded), 86960 informs us number
		 * of collisions occurred on the last packet on the
		 * transmission only.  Number of collisions on previous
		 * packets are lost.  I have told that the fact is clearly
		 * stated in the Fujitsu document.
		 *
		 * I considered not to mind it seriously.  Collision
		 * count is not so important, anyway.  Any comments?  FIXME.
		 */

		if (inb(sc->sc_iobase + FE_DLCR0) & FE_D0_COLLID) {
			/* Clear collision flag. */
			outb(sc->sc_iobase + FE_DLCR0, FE_D0_COLLID);

			/* Extract collision count from 86960. */
			col = inb(sc->sc_iobase + FE_DLCR4) & FE_D4_COL;
			if (col == 0) {
				/*
				 * Status register indicates collisions,
				 * while the collision count is zero.
				 * This can happen after multiple packet
				 * transmission, indicating that one or more
				 * previous packet(s) had been collided.
				 *
				 * Since the accurate number of collisions
				 * has been lost, we just guess it as 1;
				 * Am I too optimistic?  FIXME.
				 */
				col = 1;
			} else
				col >>= FE_D4_COL_SHIFT;
			ifp->if_collisions += col;
#if FE_DEBUG >= 4
			log(LOG_WARNING, "%s: %d collision%s (%d)\n",
			    sc->sc_dev.dv_xname, col, col == 1 ? "" : "s",
			    sc->txb_sched);
#endif
		}

		/*
		 * Update total number of successfully
		 * transmitted packets.
		 */
		ifp->if_opackets += sc->txb_sched;
		sc->txb_sched = 0;
	}

	if (sc->txb_sched == 0) {
		/*
		 * The transmitter is no more active.
		 * Reset output active flag and watchdog timer. 
		 */
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;

		/*
		 * If more data is ready to transmit in the buffer, start
		 * transmitting them.  Otherwise keep transmitter idle,
		 * even if more data is queued.  This gives receive
		 * process a slight priority.
		 */
		if (sc->txb_count > 0)
			fe_xmit(sc);
	}
}

/*
 * Ethernet interface receiver interrupt.
 */
void
fe_rint(sc, rstat)
	struct fe_softc *sc;
	u_char rstat;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int len;
	u_char status;
	int i;

	/*
	 * Update statistics if this interrupt is caused by an error.
	 */
	if (rstat & (FE_D1_OVRFLO | FE_D1_CRCERR |
		     FE_D1_ALGERR | FE_D1_SRTPKT)) {
#if FE_DEBUG >= 3
		log(LOG_WARNING, "%s: receive error: %b\n",
		    sc->sc_dev.dv_xname, rstat, FE_D1_ERRBITS);
#endif
		ifp->if_ierrors++;
	}

	/*
	 * MB86960 has a flag indicating "receive queue empty."
	 * We just loop cheking the flag to pull out all received
	 * packets.
	 *
	 * We limit the number of iterrations to avoid infinite loop.
	 * It can be caused by a very slow CPU (some broken
	 * peripheral may insert incredible number of wait cycles)
	 * or, worse, by a broken MB86960 chip.
	 */
	for (i = 0; i < FE_MAX_RECV_COUNT; i++) {
		/* Stop the iterration if 86960 indicates no packets. */
		if (inb(sc->sc_iobase + FE_DLCR5) & FE_D5_BUFEMP)
			break;

		/*
		 * Extract A receive status byte.
		 * As our 86960 is in 16 bit bus access mode, we have to
		 * use inw() to get the status byte.  The significant
		 * value is returned in lower 8 bits.
		 */
		status = (u_char)inw(sc->sc_iobase + FE_BMPR8);
#if FE_DEBUG >= 4
		log(LOG_INFO, "%s: receive status = %02x\n",
		    sc->sc_dev.dv_xname, status);
#endif

		/*
		 * If there was an error, update statistics and drop
		 * the packet, unless the interface is in promiscuous
		 * mode.
		 */
		if ((status & 0xF0) != 0x20) {	/* XXXX ? */
			if ((ifp->if_flags & IFF_PROMISC) == 0) {
				ifp->if_ierrors++;
				fe_droppacket(sc);
				continue;
			}
		}

		/*
		 * Extract the packet length.
		 * It is a sum of a header (14 bytes) and a payload.
		 * CRC has been stripped off by the 86960.
		 */
		len = inw(sc->sc_iobase + FE_BMPR8);

		/*
		 * MB86965 checks the packet length and drop big packet
		 * before passing it to us.  There are no chance we can
		 * get [crufty] packets.  Hence, if the length exceeds
		 * the specified limit, it means some serious failure,
		 * such as out-of-sync on receive buffer management.
		 *
		 * Is this statement true?  FIXME.
		 */
		if (len > ETHER_MAX_LEN || len < ETHER_HDR_SIZE) {
#if FE_DEBUG >= 2
			log(LOG_WARNING,
			    "%s: received a %s packet? (%u bytes)\n",
			    sc->sc_dev.dv_xname,
			    len < ETHER_HDR_SIZE ? "partial" : "big", len);
#endif
			ifp->if_ierrors++;
			fe_droppacket(sc);
			continue;
		}

		/*
		 * Check for a short (RUNT) packet.  We *do* check
		 * but do nothing other than print a message.
		 * Short packets are illegal, but does nothing bad
		 * if it carries data for upper layer.
		 */
#if FE_DEBUG >= 2
		if (len < ETHER_MIN_LEN) {
			log(LOG_WARNING,
			     "%s: received a short packet? (%u bytes)\n",
			     sc->sc_dev.dv_xname, len);
		}
#endif 

		/*
		 * Go get a packet.
		 */
		if (!fe_get_packet(sc, len)) {
			/* Skip a packet, updating statistics. */
#if FE_DEBUG >= 2
			log(LOG_WARNING,
			    "%s: out of mbufs; dropping packet (%u bytes)\n",
			    sc->sc_dev.dv_xname, len);
#endif
			ifp->if_ierrors++;
			fe_droppacket(sc);

			/*
			 * We stop receiving packets, even if there are
			 * more in the buffer.  We hope we can get more
			 * mbufs next time.
			 */
			return;
		}

		/* Successfully received a packet.  Update stat. */
		ifp->if_ipackets++;
	}
}

/*
 * Ethernet interface interrupt processor
 */
int
feintr(arg)
	void *arg;
{
	struct fe_softc *sc = arg;
	u_char tstat, rstat;

#if FE_DEBUG >= 4
	log(LOG_INFO, "%s: feintr()\n", sc->sc_dev.dv_xname);
	fe_dump(LOG_INFO, sc);
#endif

	/*
	 * Get interrupt conditions, masking unneeded flags.
	 */
	tstat = inb(sc->sc_iobase + FE_DLCR0) & FE_TMASK;
	rstat = inb(sc->sc_iobase + FE_DLCR1) & FE_RMASK;
	if (tstat == 0 && rstat == 0)
		return (0);

	/*
	 * Loop until there are no more new interrupt conditions.
	 */
	for (;;) {
		/*
		 * Reset the conditions we are acknowledging.
		 */
		outb(sc->sc_iobase + FE_DLCR0, tstat);
		outb(sc->sc_iobase + FE_DLCR1, rstat);

		/*
		 * Handle transmitter interrupts. Handle these first because
		 * the receiver will reset the board under some conditions.
		 */
		if (tstat != 0)
			fe_tint(sc, tstat);

		/*
		 * Handle receiver interrupts.
		 */
		if (rstat != 0)
			fe_rint(sc, rstat);

		/*
		 * Update the multicast address filter if it is
		 * needed and possible.  We do it now, because
		 * we can make sure the transmission buffer is empty,
		 * and there is a good chance that the receive queue
		 * is empty.  It will minimize the possibility of
		 * packet lossage.
		 */
		if (sc->filter_change &&
		    sc->txb_count == 0 && sc->txb_sched == 0) {
			fe_loadmar(sc);
			sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;
		}

		/*
		 * If it looks like the transmitter can take more data,
		 * attempt to start output on the interface. This is done
		 * after handling the receiver interrupt to give the
		 * receive operation priority.
		 */
		if ((sc->sc_arpcom.ac_if.if_flags & IFF_OACTIVE) == 0)
			fe_start(&sc->sc_arpcom.ac_if);

		/*
		 * Get interrupt conditions, masking unneeded flags.
		 */
		tstat = inb(sc->sc_iobase + FE_DLCR0) & FE_TMASK;
		rstat = inb(sc->sc_iobase + FE_DLCR1) & FE_RMASK;
		if (tstat == 0 && rstat == 0)
			return (1);
	}
}

/*
 * Process an ioctl request.  This code needs some work - it looks pretty ugly.
 */
int
fe_ioctl(ifp, command, data)
	register struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct fe_softc *sc = ifp->if_softc;
	register struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: ioctl(%x)\n", sc->sc_dev.dv_xname, command);
#endif

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch (command) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			fe_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			fe_init(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			fe_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			fe_init(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			fe_setmode(sc);
		}
#if DEBUG >= 1
		/* "ifconfig fe0 debug" to print register dump. */
		if (ifp->if_flags & IFF_DEBUG) {
			log(LOG_INFO, "%s: SIOCSIFFLAGS(DEBUG)\n", sc->sc_dev.dv_xname);
			fe_dump(LOG_DEBUG, sc);
		}
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Update our multicast list. */
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				fe_setmode(sc);
			error = 0;
		}
		break;

	default:
		error = EINVAL;
	}

	splx(s);
	return (error);
}

/*
 * Retreive packet from receive buffer and send to the next level up via
 * ether_input(). If there is a BPF listener, give a copy to BPF, too.
 * Returns 0 if success, -1 if error (i.e., mbuf allocation failure).
 */
int
fe_get_packet(sc, len)
	struct fe_softc *sc;
	int len;
{
	struct mbuf *m;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	/* Allocate a header mbuf. */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = len;

	/* The following silliness is to make NFS happy. */
#define	EROUND	((sizeof(struct ether_header) + 3) & ~3)
#define	EOFF	(EROUND - sizeof(struct ether_header))

	/*
	 * Our strategy has one more problem.  There is a policy on
	 * mbuf cluster allocation.  It says that we must have at
	 * least MINCLSIZE (208 bytes) to allocate a cluster.  For a
	 * packet of a size between (MHLEN - 2) to (MINCLSIZE - 2),
	 * our code violates the rule...
	 * On the other hand, the current code is short, simle,
	 * and fast, however.  It does no harmful thing, just waists
	 * some memory.  Any comments?  FIXME.
	 */

	/* Attach a cluster if this packet doesn't fit in a normal mbuf. */
	if (len > MHLEN - EOFF) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			return (0);
		}
	}

	/*
	 * The following assumes there is room for the ether header in the
	 * header mbuf.
	 */
	m->m_data += EOFF;

	/* Set the length of this packet. */
	m->m_len = len;

	/* Get a packet. */
	insw(sc->sc_iobase + FE_BMPR8, m->m_data, (len + 1) >> 1);

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.  If so, hand off
	 * the raw packet to bpf.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m);
#endif

	ether_input_mbuf(ifp, m);
	return (1);
}

/*
 * Write an mbuf chain to the transmission buffer memory using 16 bit PIO.
 * Returns number of bytes actually written, including length word.
 *
 * If an mbuf chain is too long for an Ethernet frame, it is not sent.
 * Packets shorter than Ethernet minimum are legal, and we pad them
 * before sending out.  An exception is "partial" packets which are 
 * shorter than mandatory Ethernet header.
 *
 * I wrote a code for an experimental "delayed padding" technique.
 * When employed, it postpones the padding process for short packets.
 * If xmit() occurred at the moment, the padding process is omitted, and
 * garbages are sent as pad data.  If next packet is stored in the
 * transmission buffer before xmit(), write_mbuf() pads the previous
 * packet before transmitting new packet.  This *may* gain the
 * system performance (slightly).
 */
void
fe_write_mbufs(sc, m)
	struct fe_softc *sc;
	struct mbuf *m;
{
	int bmpr8 = sc->sc_iobase + FE_BMPR8;
	struct mbuf *mp;
	u_char *data;
	u_short savebyte;	/* WARNING: Architecture dependent! */
	int totlen, len, wantbyte;

#if FE_DELAYED_PADDING
	/* Do the "delayed padding." */
	len = sc->txb_padding >> 1;
	if (len > 0) {
		while (--len >= 0)
			outw(bmpr8, 0);
		sc->txb_padding = 0;
	}
#endif

	/* We need to use m->m_pkthdr.len, so require the header */
	if ((m->m_flags & M_PKTHDR) == 0)
	  	panic("fe_write_mbufs: no header mbuf");

#if FE_DEBUG >= 2
	/* First, count up the total number of bytes to copy. */
	for (totlen = 0, mp = m; mp != 0; mp = mp->m_next)
		totlen += mp->m_len;
	/* Check if this matches the one in the packet header. */
	if (totlen != m->m_pkthdr.len)
		log(LOG_WARNING, "%s: packet length mismatch? (%d/%d)\n",
		    sc->sc_dev.dv_xname, totlen, m->m_pkthdr.len);
#else
	/* Just use the length value in the packet header. */
	totlen = m->m_pkthdr.len;
#endif

#if FE_DEBUG >= 1
	/*
	 * Should never send big packets.  If such a packet is passed,
	 * it should be a bug of upper layer.  We just ignore it.
	 * ... Partial (too short) packets, neither.
	 */
	if (totlen > ETHER_MAX_LEN || totlen < ETHER_HDR_SIZE) {
		log(LOG_ERR, "%s: got a %s packet (%u bytes) to send\n",
		    sc->sc_dev.dv_xname,
		    totlen < ETHER_HDR_SIZE ? "partial" : "big", totlen);
		sc->sc_arpcom.ac_if.if_oerrors++;
		return;
	}
#endif

	/*
	 * Put the length word for this frame.
	 * Does 86960 accept odd length?  -- Yes.
	 * Do we need to pad the length to minimum size by ourselves?
	 * -- Generally yes.  But for (or will be) the last
	 * packet in the transmission buffer, we can skip the
	 * padding process.  It may gain performance slightly.  FIXME.
	 */
	outw(bmpr8, max(totlen, ETHER_MIN_LEN));

	/*
	 * Update buffer status now.
	 * Truncate the length up to an even number, since we use outw().
	 */
	totlen = (totlen + 1) & ~1;
	sc->txb_free -= FE_DATA_LEN_LEN + max(totlen, ETHER_MIN_LEN);
	sc->txb_count++;

#if FE_DELAYED_PADDING
	/* Postpone the packet padding if necessary. */
	if (totlen < ETHER_MIN_LEN)
		sc->txb_padding = ETHER_MIN_LEN - totlen;
#endif

	/*
	 * Transfer the data from mbuf chain to the transmission buffer. 
	 * MB86960 seems to require that data be transferred as words, and
	 * only words.  So that we require some extra code to patch
	 * over odd-length mbufs.
	 */
	wantbyte = 0;
	for (; m != 0; m = m->m_next) {
		/* Ignore empty mbuf. */
		len = m->m_len;
		if (len == 0)
			continue;

		/* Find the actual data to send. */
		data = mtod(m, caddr_t);

		/* Finish the last byte. */
		if (wantbyte) {
			outw(bmpr8, savebyte | (*data << 8));
			data++;
			len--;
			wantbyte = 0;
		}

		/* Output contiguous words. */
		if (len > 1)
			outsw(bmpr8, data, len >> 1);

		/* Save remaining byte, if there is one. */
		if (len & 1) {
			data += len & ~1;
			savebyte = *data;
			wantbyte = 1;
		}
	}

	/* Spit the last byte, if the length is odd. */
	if (wantbyte)
		outw(bmpr8, savebyte);

#if ! FE_DELAYED_PADDING
	/*
	 * Pad the packet to the minimum length if necessary.
	 */
	len = (ETHER_MIN_LEN >> 1) - (totlen >> 1);
	while (--len >= 0)
		outw(bmpr8, 0);
#endif
}

/*
 * Compute the multicast address filter from the
 * list of multicast addresses we need to listen to.
 */
void
fe_getmcaf(ac, af)
	struct arpcom *ac;
	u_char *af;
{
	struct ifnet *ifp = &ac->ac_if;
	struct ether_multi *enm;
	register u_char *cp, c;
	register u_long crc;
	register int i, len;
	struct ether_multistep step;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	if ((ifp->if_flags & IFF_PROMISC) != 0)
		goto allmulti;

	af[0] = af[1] = af[2] = af[3] = af[4] = af[5] = af[6] = af[7] = 0x00;
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
		    sizeof(enm->enm_addrlo)) != 0) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		cp = enm->enm_addrlo;
		crc = 0xffffffff;
		for (len = sizeof(enm->enm_addrlo); --len >= 0;) {
			c = *cp++;
			for (i = 8; --i >= 0;) {
				if ((crc & 0x01) ^ (c & 0x01)) {
					crc >>= 1;
					crc ^= 0xedb88320;
				} else
					crc >>= 1;
				c >>= 1;
			}
		}
		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Turn on the corresponding bit in the filter. */
		af[crc >> 3] |= 1 << (crc & 7);

		ETHER_NEXT_MULTI(step, enm);
	}
	ifp->if_flags &= ~IFF_ALLMULTI;
	return;

allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	af[0] = af[1] = af[2] = af[3] = af[4] = af[5] = af[6] = af[7] = 0xff;
}

/*
 * Calculate a new "multicast packet filter" and put the 86960
 * receiver in appropriate mode.
 */
void
fe_setmode(sc)
	struct fe_softc *sc;
{
	int flags = sc->sc_arpcom.ac_if.if_flags;

	/*
	 * If the interface is not running, we postpone the update
	 * process for receive modes and multicast address filter
	 * until the interface is restarted.  It reduces some
	 * complicated job on maintaining chip states.  (Earlier versions
	 * of this driver had a bug on that point...)
	 *
	 * To complete the trick, fe_init() calls fe_setmode() after
	 * restarting the interface.
	 */
	if ((flags & IFF_RUNNING) == 0)
		return;

	/*
	 * Promiscuous mode is handled separately.
	 */
	if ((flags & IFF_PROMISC) != 0) {
		/*
		 * Program 86960 to receive all packets on the segment
		 * including those directed to other stations.
		 * Multicast filter stored in MARs are ignored
		 * under this setting, so we don't need to update it.
		 *
		 * Promiscuous mode is used solely by BPF, and BPF only
		 * listens to valid (no error) packets.  So, we ignore
		 * errornous ones even in this mode.
		 */
		outb(sc->sc_iobase + FE_DLCR5,
		    sc->proto_dlcr5 | FE_D5_AFM0 | FE_D5_AFM1);
		sc->filter_change = 0;

#if FE_DEBUG >= 3
		log(LOG_INFO, "%s: promiscuous mode\n", sc->sc_dev.dv_xname);
#endif
		return;
	}

	/*
	 * Turn the chip to the normal (non-promiscuous) mode.
	 */
	outb(sc->sc_iobase + FE_DLCR5, sc->proto_dlcr5 | FE_D5_AFM1);

	/*
	 * Find the new multicast filter value.
	 */
	fe_getmcaf(&sc->sc_arpcom, sc->filter);
	sc->filter_change = 1;

#if FE_DEBUG >= 3
	log(LOG_INFO,
	    "%s: address filter: [%02x %02x %02x %02x %02x %02x %02x %02x]\n",
	    sc->sc_dev.dv_xname,
	    sc->filter[0], sc->filter[1], sc->filter[2], sc->filter[3],
	    sc->filter[4], sc->filter[5], sc->filter[6], sc->filter[7]);
#endif

	/*
	 * We have to update the multicast filter in the 86960, A.S.A.P.
	 *
	 * Note that the DLC (Data Linc Control unit, i.e. transmitter
	 * and receiver) must be stopped when feeding the filter, and
	 * DLC trushes all packets in both transmission and receive
	 * buffers when stopped.
	 *
	 * ... Are the above sentenses correct?  I have to check the
	 *     manual of the MB86960A.  FIXME.
	 *
	 * To reduce the packet lossage, we delay the filter update
	 * process until buffers are empty.
	 */
	if (sc->txb_sched == 0 && sc->txb_count == 0 &&
	    (inb(sc->sc_iobase + FE_DLCR1) & FE_D1_PKTRDY) == 0) {
		/*
		 * Buffers are (apparently) empty.  Load
		 * the new filter value into MARs now.
		 */
		fe_loadmar(sc);
	} else {
		/*
		 * Buffers are not empty.  Mark that we have to update
		 * the MARs.  The new filter will be loaded by feintr()
		 * later.
		 */
#if FE_DEBUG >= 4
		log(LOG_INFO, "%s: filter change delayed\n", sc->sc_dev.dv_xname);
#endif
	}
}

/*
 * Load a new multicast address filter into MARs.
 *
 * The caller must have splnet'ed befor fe_loadmar.
 * This function starts the DLC upon return.  So it can be called only
 * when the chip is working, i.e., from the driver's point of view, when
 * a device is RUNNING.  (I mistook the point in previous versions.)
 */
void
fe_loadmar(sc)
	struct fe_softc *sc;
{

	/* Stop the DLC (transmitter and receiver). */
	outb(sc->sc_iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE);

	/* Select register bank 1 for MARs. */
	outb(sc->sc_iobase + FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_MAR | FE_D7_POWER_UP);

	/* Copy filter value into the registers. */
	outblk(sc->sc_iobase + FE_MAR8, sc->filter, FE_FILTER_LEN);

	/* Restore the bank selection for BMPRs (i.e., runtime registers). */
	outb(sc->sc_iobase + FE_DLCR7,
	    sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP);

	/* Restart the DLC. */
	outb(sc->sc_iobase + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_ENABLE);

	/* We have just updated the filter. */
	sc->filter_change = 0;

#if FE_DEBUG >= 3
	log(LOG_INFO, "%s: address filter changed\n", sc->sc_dev.dv_xname);
#endif
}

#if FE_DEBUG >= 1
void
fe_dump(level, sc)
	int level;
	struct fe_softc *sc;
{
	int iobase = sc->sc_iobase;
	u_char save_dlcr7;

	save_dlcr7 = inb(iobase + FE_DLCR7);

	log(level, "\tDLCR = %02x %02x %02x %02x %02x %02x %02x %02x",
	    inb(iobase + FE_DLCR0),  inb(iobase + FE_DLCR1),
	    inb(iobase + FE_DLCR2),  inb(iobase + FE_DLCR3),
	    inb(iobase + FE_DLCR4),  inb(iobase + FE_DLCR5),
	    inb(iobase + FE_DLCR6),  inb(iobase + FE_DLCR7));

	outb(iobase + FE_DLCR7, (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_DLCR);
	log(level, "\t       %02x %02x %02x %02x %02x %02x %02x %02x,",
	    inb(iobase + FE_DLCR8),  inb(iobase + FE_DLCR9),
	    inb(iobase + FE_DLCR10), inb(iobase + FE_DLCR11),
	    inb(iobase + FE_DLCR12), inb(iobase + FE_DLCR13),
	    inb(iobase + FE_DLCR14), inb(iobase + FE_DLCR15));

	outb(iobase + FE_DLCR7, (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_MAR);
	log(level, "\tMAR  = %02x %02x %02x %02x %02x %02x %02x %02x,",
	    inb(iobase + FE_MAR8),   inb(iobase + FE_MAR9),
	    inb(iobase + FE_MAR10),  inb(iobase + FE_MAR11),
	    inb(iobase + FE_MAR12),  inb(iobase + FE_MAR13),
	    inb(iobase + FE_MAR14),  inb(iobase + FE_MAR15));

	outb(iobase + FE_DLCR7, (save_dlcr7 & ~FE_D7_RBS) | FE_D7_RBS_BMPR);
	log(level, "\tBMPR = xx xx %02x %02x %02x %02x %02x %02x %02x %02x xx %02x.",
	    inb(iobase + FE_BMPR10), inb(iobase + FE_BMPR11),
	    inb(iobase + FE_BMPR12), inb(iobase + FE_BMPR13),
	    inb(iobase + FE_BMPR14), inb(iobase + FE_BMPR15),
	    inb(iobase + FE_BMPR16), inb(iobase + FE_BMPR17),
	    inb(iobase + FE_BMPR19));

	outb(iobase + FE_DLCR7, save_dlcr7);
}
#endif
