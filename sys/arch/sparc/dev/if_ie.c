/*	$OpenBSD: if_ie.c,v 1.25 2002/11/10 21:23:06 miod Exp $	*/
/*	$NetBSD: if_ie.c,v 1.33 1997/07/29 17:55:38 fair Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995 Charles Hannum.
 * Copyright (c) 1992, 1993, University of Vermont and State
 *  Agricultural College.
 * Copyright (c) 1992, 1993, Garrett A. Wollman.
 *
 * Portions:
 * Copyright (c) 1994, 1995, Rafal K. Boni
 * Copyright (c) 1990, 1991, William F. Jolitz
 * Copyright (c) 1990, The Regents of the University of California
 *
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
 *	This product includes software developed by Charles Hannum, by the
 *	University of Vermont and State Agricultural College and Garrett A.
 *	Wollman, by William F. Jolitz, and by the University of California,
 *	Berkeley, Lawrence Berkeley Laboratory, and its contributors.
 * 4. Neither the names of the Universities nor the names of the authors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR AUTHORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Intel 82586 Ethernet chip
 * Register, bit, and structure definitions.
 *
 * Original StarLAN driver written by Garrett Wollman with reference to the
 * Clarkson Packet Driver code for this chip written by Russ Nelson and others.
 *
 * BPF support code taken from hpdev/if_le.c, supplied with tcpdump.
 *
 * 3C507 support is loosely based on code donated to NetBSD by Rafal Boni.
 *
 * Majorly cleaned up and 3C507 code merged by Charles Hannum.
 *
 * Converted to SUN ie driver by Charles D. Cranor,
 *		October 1994, January 1995.
 * This sun version based on i386 version 1.30.
 */

/*
 * The i82586 is a very painful chip, found in sun3's, sun-4/100's
 * sun-4/200's, and VME based suns.  The byte order is all wrong for a
 * SUN, making life difficult.  Programming this chip is mostly the same,
 * but certain details differ from system to system.  This driver is
 * written so that different "ie" interfaces can be controled by the same
 * driver.
 */

/*
Mode of operation:

   We run the 82586 in a standard Ethernet mode.  We keep NFRAMES
   received frame descriptors around for the receiver to use, and
   NRXBUF associated receive buffer descriptors, both in a circular
   list.  Whenever a frame is received, we rotate both lists as
   necessary.  (The 586 treats both lists as a simple queue.)  We also
   keep a transmit command around so that packets can be sent off
   quickly.

   We configure the adapter in AL-LOC = 1 mode, which means that the
   Ethernet/802.3 MAC header is placed at the beginning of the receive
   buffer rather than being split off into various fields in the RFD.
   This also means that we must include this header in the transmit
   buffer as well.

   By convention, all transmit commands, and only transmit commands,
   shall have the I (IE_CMD_INTR) bit set in the command.  This way,
   when an interrupt arrives at ieintr(), it is immediately possible
   to tell what precisely caused it.  ANY OTHER command-sending
   routines should run at splnet(), and should post an acknowledgement
   to every interrupt they generate.

*/

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>
#include <net/route.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <uvm/uvm_extern.h>
#include <uvm/uvm_map.h>

/*
 * ugly byte-order hack for SUNs
 */

#define SWAP(x)         ((u_short)(XSWAP((u_short)(x))))
#define XSWAP(y)        ( ((y) >> 8) | ((y) << 8) )

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/pmap.h>

#include <sparc/dev/if_ie.h>
#include <sparc/dev/i82586.h>

static struct mbuf *last_not_for_us;
struct vm_map *ie_map; /* for obio */

#define	IED_RINT	0x01
#define	IED_TINT	0x02
#define	IED_RNR		0x04
#define	IED_CNA		0x08
#define	IED_READFRAME	0x10
#define	IED_ALL		0x1f

#define B_PER_F         3               /* recv buffers per frame */
#define MXFRAMES        300             /* max number of recv frames */
#define	MXRXBUF		(MXFRAMES*B_PER_F) /* number of buffers to allocate */
#define	IE_RBUF_SIZE	256		/* size of each receive buffer;
						MUST BE POWER OF TWO */
#define	NTXBUF		2		/* number of transmit commands */
#define	IE_TBUF_SIZE	ETHER_MAX_LEN	/* length of transmit buffer */


enum ie_hardware {
  IE_VME,             /* multibus to VME card */
  IE_OBIO,            /* on board */
  IE_VME3E,           /* sun 3e VME card */
  IE_UNKNOWN
};

const char *ie_hardware_names[] = {
	"multibus/vme",
	"onboard",
	"3e/vme",
	"Unknown"
};

/*
 * Ethernet status, per interface.
 *
 * hardware addresses/sizes to know (all KVA):
 *   sc_iobase = base of chip's 24 bit address space
 *   sc_maddr  = base address of chip RAM as stored in ie_base of iscp
 *   sc_msize  = size of chip's RAM
 *   sc_reg    = address of card dependent registers
 *
 * the chip uses two types of pointers: 16 bit and 24 bit
 *   16 bit pointers are offsets from sc_maddr/ie_base
 *      KVA(16 bit offset) = offset + sc_maddr
 *   24 bit pointers are offset from sc_iobase in KVA
 *      KVA(24 bit address) = address + sc_iobase
 *
 * on the vme/multibus we have the page map to control where ram appears
 * in the address space.   we choose to have RAM start at 0 in the
 * 24 bit address space.   this means that sc_iobase == sc_maddr!
 * to get the phyiscal address of the board's RAM you must take the
 * top 12 bits of the physical address of the register address
 * and or in the 4 bits from the status word as bits 17-20 (remember that
 * the board ignores the chip's top 4 address lines).
 * For example:
 *   if the register is @ 0xffe88000, then the top 12 bits are 0xffe00000.
 *   to get the 4 bits from the the status word just do status & IEVME_HADDR.
 *   suppose the value is "4".   Then just shift it left 16 bits to get
 *   it into bits 17-20 (e.g. 0x40000).    Then or it to get the
 *   address of RAM (in our example: 0xffe40000).   see the attach routine!
 *
 * on the onboard ie interface the 24 bit address space is hardwired
 * to be 0xff000000 -> 0xffffffff of KVA.   this means that sc_iobase
 * will be 0xff000000.   sc_maddr will be where ever we allocate RAM
 * in KVA.    note that since the SCP is at a fixed address it means
 * that we have to allocate a fixed KVA for the SCP.
 */

struct ie_softc {
	struct device sc_dev;   /* device structure */
	struct intrhand sc_ih;  /* interrupt info */

	caddr_t sc_iobase;      /* KVA of base of 24 bit addr space */
	caddr_t sc_maddr;       /* KVA of base of chip's RAM (16bit addr sp.)*/
	u_int sc_msize;         /* how much RAM we have/use */
	caddr_t sc_reg;         /* KVA of car's register */

	struct arpcom sc_arpcom;/* system arpcom structure */

	void (*reset_586)(struct ie_softc *);
				/* card dependent reset function */
	void (*chan_attn)(struct ie_softc *);
				/* card dependent attn function */
	void (*run_586)(struct ie_softc *);
				/* card depenent "go on-line" function */
	void (*memcopy)(const void *, void *, u_int);
	                        /* card dependent memory copy function */
        void (*memzero)(void *, u_int);
	                        /* card dependent memory zero function */


	enum ie_hardware hard_type;     /* card type */

	int want_mcsetup;       /* mcsetup flag */
	int promisc;            /* are we in promisc mode? */

	/*
	 * pointers to the 3 major control structures
	 */

	volatile struct ie_sys_conf_ptr *scp;
	volatile struct ie_int_sys_conf_ptr *iscp;
	volatile struct ie_sys_ctl_block *scb;

	/*
	 * pointer and size of a block of KVA where the buffers
	 * are to be allocated from
	 */

	caddr_t buf_area;
	int buf_area_sz;

	/*
	 * the actual buffers (recv and xmit)
	 */

	volatile struct ie_recv_frame_desc *rframes[MXFRAMES];
	volatile struct ie_recv_buf_desc *rbuffs[MXRXBUF];
	volatile char *cbuffs[MXRXBUF];
        int rfhead, rftail, rbhead, rbtail;

	volatile struct ie_xmit_cmd *xmit_cmds[NTXBUF];
	volatile struct ie_xmit_buf *xmit_buffs[NTXBUF];
	u_char *xmit_cbuffs[NTXBUF];
	int xmit_busy;
	int xmit_free;
	int xchead, xctail;

	struct ie_en_addr mcast_addrs[MAXMCAST + 1];
	int mcast_count;

	int nframes;      /* number of frames in use */
	int nrxbuf;       /* number of recv buffs in use */

#ifdef IEDEBUG
	int sc_debug;
#endif
};

static void ie_obreset(struct ie_softc *);
static void ie_obattend(struct ie_softc *);
static void ie_obrun(struct ie_softc *);
static void ie_vmereset(struct ie_softc *);
static void ie_vmeattend(struct ie_softc *);
static void ie_vmerun(struct ie_softc *);

void iewatchdog(struct ifnet *);
int ieintr(void *);
int ieinit(struct ie_softc *);
int ieioctl(struct ifnet *, u_long, caddr_t);
void iestart(struct ifnet *);
void iereset(struct ie_softc *);
static void ie_readframe(struct ie_softc *, int);
static void ie_drop_packet_buffer(struct ie_softc *);
int ie_setupram(struct ie_softc *);
static int command_and_wait(struct ie_softc *, int,
    void volatile *, int);
/*static*/ void ierint(struct ie_softc *);
/*static*/ void ietint(struct ie_softc *);
static int ieget(struct ie_softc *, struct mbuf **,
		      struct ether_header *, int *);
static void setup_bufs(struct ie_softc *);
static int mc_setup(struct ie_softc *, void *);
static void mc_reset(struct ie_softc *);
static __inline int ether_equal(u_char *, u_char *);
static __inline void ie_ack(struct ie_softc *, u_int);
static __inline void ie_setup_config(volatile struct ie_config_cmd *,
					  int, int);
static __inline int check_eh(struct ie_softc *, struct ether_header *,
				  int *);
static __inline int ie_buflen(struct ie_softc *, int);
static __inline int ie_packet_len(struct ie_softc *);
static __inline void iexmit(struct ie_softc *);
static __inline caddr_t Align(caddr_t);

static void chan_attn_timeout(void *);
static void run_tdr(struct ie_softc *, struct ie_tdr_cmd *);
static void iestop(struct ie_softc *);

#ifdef IEDEBUG
void print_rbd(volatile struct ie_recv_buf_desc *);

int in_ierint = 0;
int in_ietint = 0;
#endif

int iematch(struct device *, void *, void *);
void ieattach(struct device *, struct device *, void *);

struct cfattach ie_ca = {
	sizeof(struct ie_softc), iematch, ieattach
};

struct cfdriver ie_cd = {
	NULL, "ie", DV_IFNET
};

/*
 * address generation macros
 *   MK_24 = KVA -> 24 bit address in SUN byte order
 *   MK_16 = KVA -> 16 bit address in INTEL byte order
 *   ST_24 = store a 24 bit address in SUN byte order to INTEL byte order
 */
#define MK_24(base, ptr) ((caddr_t)((u_long)ptr - (u_long)base))
#define MK_16(base, ptr) SWAP((u_short)( ((u_long)(ptr)) - ((u_long)(base)) ))
#define ST_24(to, from) { \
                            u_long fval = (u_long)(from); \
                            u_char *t = (u_char *)&(to), *f = (u_char *)&fval; \
                            t[0] = f[3]; t[1] = f[2]; t[2] = f[1]; /*t[3] = f[0]
;*/ \
                        }
/*
 * Here are a few useful functions.  We could have done these as macros, but
 * since we have the inline facility, it makes sense to use that instead.
 */
static __inline void
ie_setup_config(cmd, promiscuous, manchester)
	volatile struct ie_config_cmd *cmd;
	int promiscuous, manchester;
{

	cmd->ie_config_count = 0x0c;
	cmd->ie_fifo = 8;
	cmd->ie_save_bad = 0x40;
	cmd->ie_addr_len = 0x2e;
	cmd->ie_priority = 0;
	cmd->ie_ifs = 0x60;
	cmd->ie_slot_low = 0;
	cmd->ie_slot_high = 0xf2;
	cmd->ie_promisc = !!promiscuous | manchester << 2;
	cmd->ie_crs_cdt = 0;
	cmd->ie_min_len = 64;
	cmd->ie_junk = 0xff;
}

static __inline void
ie_ack(sc, mask)
	struct ie_softc *sc;
	u_int mask;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;

	command_and_wait(sc, scb->ie_status & mask, 0, 0);
}


int
iematch(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void   *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))	/* correct name? */
		return (0);

	switch (ca->ca_bustype) {
	case BUS_SBUS:
	default:
		return (0);
	case BUS_OBIO:
		if (probeget(ra->ra_vaddr, 1) != -1)
			return (1);
		break;
	case BUS_VME16:
	case BUS_VME32:
		if (probeget(ra->ra_vaddr, 2) != -1)
			return (1);
		break;
	}
	return (0);
}

/*
 * MULTIBUS/VME support
 */
void
ie_vmereset(sc)
	struct ie_softc *sc;
{
	volatile struct ievme *iev = (struct ievme *) sc->sc_reg;
	iev->status = IEVME_RESET;
	delay(100);		/* XXX could be shorter? */
	iev->status = 0;
}

void
ie_vmeattend(sc)
	struct ie_softc *sc;
{
	volatile struct ievme *iev = (struct ievme *) sc->sc_reg;

	iev->status |= IEVME_ATTEN;	/* flag! */
	iev->status &= ~IEVME_ATTEN;	/* down. */
}

void
ie_vmerun(sc)
	struct ie_softc *sc;
{
	volatile struct ievme *iev = (struct ievme *) sc->sc_reg;

	iev->status |= (IEVME_ONAIR | IEVME_IENAB | IEVME_PEINT);
}

/*
 * onboard ie support
 */
void
ie_obreset(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;
	ieo->obctrl = 0;
	delay(100);			/* XXX could be shorter? */
	ieo->obctrl = IEOB_NORSET;
}
void
ie_obattend(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;

	ieo->obctrl |= IEOB_ATTEN;	/* flag! */
	ieo->obctrl &= ~IEOB_ATTEN;	/* down. */
}

void
ie_obrun(sc)
	struct ie_softc *sc;
{
	volatile struct ieob *ieo = (struct ieob *) sc->sc_reg;

	ieo->obctrl |= (IEOB_ONAIR|IEOB_IENAB|IEOB_NORSET);
}

/*
 * Taken almost exactly from Bill's if_is.c, then modified beyond recognition.
 */
void
ieattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void   *aux;
{
	struct ie_softc *sc = (void *) self;
	struct confargs *ca = aux;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	extern void myetheraddr(u_char *);	/* should be elsewhere */
	struct bootpath *bp;
	int     pri = ca->ca_ra.ra_intr[0].int_pri;

	/*
	 * *note*: we don't detect the difference between a VME3E and
	 * a multibus/vme card.   if you want to use a 3E you'll have
	 * to fix this.
	 */

	switch (ca->ca_bustype) {
	case BUS_OBIO:
	    {
		volatile struct ieob *ieo;
		paddr_t pa;

		sc->hard_type = IE_OBIO;
		sc->reset_586 = ie_obreset;
		sc->chan_attn = ie_obattend;
		sc->run_586 = ie_obrun;
		sc->memcopy = bcopy;
		sc->memzero = bzero;
		sc->sc_msize = 65536; /* XXX */
		sc->sc_reg = mapiodev(ca->ca_ra.ra_reg, 0, sizeof(struct ieob));
		ieo = (volatile struct ieob *) sc->sc_reg;

		/*
		 * the rest of the IE_OBIO case needs to be cleaned up
		 * XXX
		 */

		ie_map = uvm_map_create(pmap_kernel(), (vaddr_t)IEOB_ADBASE,
		    (vaddr_t)IEOB_ADBASE + sc->sc_msize, VM_MAP_INTRSAFE);
		if (ie_map == NULL) panic("ie_map");
		sc->sc_maddr = (caddr_t) uvm_km_alloc(ie_map, sc->sc_msize);
		if (sc->sc_maddr == NULL) panic("ie kmem_alloc");
		kvm_uncache(sc->sc_maddr, sc->sc_msize >> PGSHIFT);
		if (((u_long)sc->sc_maddr & ~(NBPG-1)) != (u_long)sc->sc_maddr)
			panic("unaligned dvmamalloc breaks");
		sc->sc_iobase = (caddr_t)IEOB_ADBASE; /* 24 bit base addr */
		(sc->memzero)(sc->sc_maddr, sc->sc_msize);
		sc->iscp = (volatile struct ie_int_sys_conf_ptr *)
			sc->sc_maddr; /* @ location zero */
		sc->scb = (volatile struct ie_sys_ctl_block *)
		    sc->sc_maddr + sizeof(struct ie_int_sys_conf_ptr);
		/* scb follows iscp */

		/*
		 * SCP: the scp must appear at KVA IEOB_ADBASE.  The
		 * ROM seems to have page up there, but I'm not sure all
		 * ROMs will have it there.  Also, I'm not sure if that
		 * page is on some free list somewhere or not.   Let's
		 * map the first page of the buffer we just allocated
		 * to IEOB_ADBASE to be safe.
		 */

		if (pmap_extract(pmap_kernel(), (vaddr_t)sc->sc_maddr, &pa) == FALSE)
			panic("ie pmap_extract");
		pmap_enter(pmap_kernel(), trunc_page(IEOB_ADBASE+IE_SCP_ADDR),
                    (paddr_t)pa | PMAP_NC /*| PMAP_IOC*/,
                    VM_PROT_READ | VM_PROT_WRITE, PMAP_WIRED);
		pmap_update(pmap_kernel());

		sc->scp = (volatile struct ie_sys_conf_ptr *)
			(IEOB_ADBASE + IE_SCP_ADDR);

		/*
		 * rest of first page is unused (wasted!), rest of ram
		 * for buffers
		 */
		sc->buf_area = sc->sc_maddr + NBPG;
		sc->buf_area_sz = sc->sc_msize - NBPG;
		break;
	    }
	case BUS_VME16:
	    {
		volatile struct ievme *iev;
		u_long  rampaddr;
		int     lcv;

		sc->hard_type = IE_VME;
		sc->reset_586 = ie_vmereset;
		sc->chan_attn = ie_vmeattend;
		sc->run_586 = ie_vmerun;
		sc->memcopy = wcopy;
		sc->memzero = wzero;
		sc->sc_msize = 65536;	/* XXX */
		sc->sc_reg = mapiodev(ca->ca_ra.ra_reg, 0,
				      sizeof(struct ievme));
		iev = (volatile struct ievme *) sc->sc_reg;
		/* top 12 bits */
		rampaddr = (u_long)ca->ca_ra.ra_paddr & 0xfff00000;
		/* 4 more */
		rampaddr = rampaddr | ((iev->status & IEVME_HADDR) << 16);
		rampaddr -= (u_long)ca->ca_ra.ra_paddr;
		sc->sc_maddr = mapiodev(ca->ca_ra.ra_reg, rampaddr,
					sc->sc_msize);
		sc->sc_iobase = sc->sc_maddr;
		iev->pectrl = iev->pectrl | IEVME_PARACK; /* clear to start */

		/*
		 * set up mappings, direct map except for last page
		 * which is mapped at zero and at high address (for
		 * scp), zero ram
		 */

		for (lcv = 0; lcv < IEVME_MAPSZ - 1; lcv++)
			iev->pgmap[lcv] = IEVME_SBORDR | IEVME_OBMEM | lcv;
		iev->pgmap[IEVME_MAPSZ - 1] = IEVME_SBORDR | IEVME_OBMEM | 0;
		(sc->memzero)(sc->sc_maddr, sc->sc_msize);

		/*
		 * set up pointers to data structures and buffer area.
		 * scp is in double mapped page... get offset into page
		 * and add to sc_maddr.
		 */
		sc->scp = (volatile struct ie_sys_conf_ptr *)
		    (sc->sc_maddr + (IE_SCP_ADDR & (IEVME_PAGESIZE - 1)));
		sc->iscp = (volatile struct ie_int_sys_conf_ptr *)
		    sc->sc_maddr;	/* iscp @ location zero */
		sc->scb = (volatile struct ie_sys_ctl_block *)
		    sc->sc_maddr + sizeof(struct ie_int_sys_conf_ptr);
		/* scb follows iscp */

		/*
		 * rest of first page is unused, rest of ram
		 * for buffers
		 */
		sc->buf_area = sc->sc_maddr + IEVME_PAGESIZE;
		sc->buf_area_sz = sc->sc_msize - IEVME_PAGESIZE;
		break;
	    }
	default:
		printf("unknown\n");
		return;
	}

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	if (ie_setupram(sc) == 0) {
		printf(": RAM CONFIG FAILED!\n");
		/* XXX should reclaim resources? */
		return;
	}
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = iestart;
	ifp->if_ioctl = ieioctl;
	ifp->if_watchdog = iewatchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	printf(" pri %d address %s, type %s\n", pri,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr),
	    ie_hardware_names[sc->hard_type]);

	switch (ca->ca_bustype) {
#if defined(SUN4)
	case BUS_OBIO:
		sc->sc_ih.ih_fun = ieintr;
		sc->sc_ih.ih_arg = sc;
		intr_establish(pri, &sc->sc_ih, IPL_NET);
		break;
	case BUS_VME16:
	case BUS_VME32:
		sc->sc_ih.ih_fun = ieintr;
		sc->sc_ih.ih_arg = sc;
		vmeintr_establish(ca->ca_ra.ra_intr[0].int_vec, pri,
		    &sc->sc_ih, IPL_NET);
		break;
#endif /* SUN4 */
	}

	bp = ca->ca_ra.ra_bp;
	if (bp != NULL && strcmp(bp->name, "ie") == 0 &&
	    sc->sc_dev.dv_unit == bp->val[1])
		bp->dev = &sc->sc_dev;
}



/*
 * Device timeout/watchdog routine.  Entered if the device neglects to generate
 * an interrupt after a transmit has been started on it.
 */
void
iewatchdog(ifp)
	struct ifnet *ifp;
{
	struct ie_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++sc->sc_arpcom.ac_if.if_oerrors;

	iereset(sc);
}

/*
 * What to do upon receipt of an interrupt.
 */
int
ieintr(v)
void *v;
{
	struct ie_softc *sc = v;
	u_short status;

	status = sc->scb->ie_status;

        /*
         * check for parity error
         */
        if (sc->hard_type == IE_VME) {
                volatile struct ievme *iev = (volatile struct ievme *)sc->sc_reg
;
                if (iev->status & IEVME_PERR) {
                        printf("%s: parity error (ctrl 0x%x @ 0x%02x%04x)\n",
                               sc->sc_dev.dv_xname, iev->pectrl,
			       iev->pectrl & IEVME_HADDR, iev->peaddr);
                        iev->pectrl = iev->pectrl | IEVME_PARACK;
                }
        }


loop:
	/* Ack interrupts FIRST in case we receive more during the ISR. */
	ie_ack(sc, IE_ST_WHENCE & status);

	if (status & (IE_ST_RECV | IE_ST_RNR)) {
#ifdef IEDEBUG
		in_ierint++;
		if (sc->sc_debug & IED_RINT)
			printf("%s: rint\n", sc->sc_dev.dv_xname);
#endif
		ierint(sc);
#ifdef IEDEBUG
		in_ierint--;
#endif
	}

	if (status & IE_ST_DONE) {
#ifdef IEDEBUG
		in_ietint++;
		if (sc->sc_debug & IED_TINT)
			printf("%s: tint\n", sc->sc_dev.dv_xname);
#endif
		ietint(sc);
#ifdef IEDEBUG
		in_ietint--;
#endif
	}

	if (status & IE_ST_RNR) {
		printf("%s: receiver not ready\n", sc->sc_dev.dv_xname);
		sc->sc_arpcom.ac_if.if_ierrors++;
		iereset(sc);
	}

#ifdef IEDEBUG
	if ((status & IE_ST_ALLDONE) && (sc->sc_debug & IED_CNA))
		printf("%s: cna\n", sc->sc_dev.dv_xname);
#endif

	if ((status = sc->scb->ie_status) & IE_ST_WHENCE)
		goto loop;

	return 1;
}

/*
 * Process a received-frame interrupt.
 */
void
ierint(sc)
	struct ie_softc *sc;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;
	int i, status;
	static int timesthru = 1024;

	i = sc->rfhead;
	for (;;) {
		status = sc->rframes[i]->ie_fd_status;

		if ((status & IE_FD_COMPLETE) && (status & IE_FD_OK)) {
			sc->sc_arpcom.ac_if.if_ipackets++;
			if (!--timesthru) {
				sc->sc_arpcom.ac_if.if_ierrors +=
				    SWAP(scb->ie_err_crc) +
				    SWAP(scb->ie_err_align) +
				    SWAP(scb->ie_err_resource) +
				    SWAP(scb->ie_err_overrun);
				scb->ie_err_crc = scb->ie_err_align =
				    scb->ie_err_resource = scb->ie_err_overrun =
				    0;
				timesthru = 1024;
			}
			ie_readframe(sc, i);
		} else {
			if ((status & IE_FD_RNR) != 0 &&
			    (scb->ie_status & IE_RU_READY) == 0) {
				sc->rframes[0]->ie_fd_buf_desc =
					MK_16(sc->sc_maddr, sc->rbuffs[0]);
				scb->ie_recv_list =
				  MK_16(sc->sc_maddr, sc->rframes[0]);
				command_and_wait(sc, IE_RU_START, 0, 0);
			}
			break;
		}
		i = (i + 1) % sc->nframes;
	}
}

/*
 * Process a command-complete interrupt.  These are only generated by the
 * transmission of frames.  This routine is deceptively simple, since most of
 * the real work is done by iestart().
 */
void
ietint(sc)
	struct ie_softc *sc;
{
	int status;

	sc->sc_arpcom.ac_if.if_timer = 0;
	sc->sc_arpcom.ac_if.if_flags &= ~IFF_OACTIVE;

	status = sc->xmit_cmds[sc->xctail]->ie_xmit_status;

	if (!(status & IE_STAT_COMPL) || (status & IE_STAT_BUSY))
		printf("ietint: command still busy!\n");

	if (status & IE_STAT_OK) {
		sc->sc_arpcom.ac_if.if_opackets++;
		sc->sc_arpcom.ac_if.if_collisions +=
		  SWAP(status & IE_XS_MAXCOLL);
	} else if (status & IE_STAT_ABORT) {
		printf("%s: send aborted\n", sc->sc_dev.dv_xname);
		sc->sc_arpcom.ac_if.if_oerrors++;
	} else if (status & IE_XS_NOCARRIER) {
		printf("%s: no carrier\n", sc->sc_dev.dv_xname);
		sc->sc_arpcom.ac_if.if_oerrors++;
	} else if (status & IE_XS_LOSTCTS) {
		printf("%s: lost CTS\n", sc->sc_dev.dv_xname);
		sc->sc_arpcom.ac_if.if_oerrors++;
	} else if (status & IE_XS_UNDERRUN) {
		printf("%s: DMA underrun\n", sc->sc_dev.dv_xname);
		sc->sc_arpcom.ac_if.if_oerrors++;
	} else if (status & IE_XS_EXCMAX) {
		printf("%s: too many collisions\n", sc->sc_dev.dv_xname);
		sc->sc_arpcom.ac_if.if_collisions += 16;
		sc->sc_arpcom.ac_if.if_oerrors++;
	}

	/*
	 * If multicast addresses were added or deleted while transmitting,
	 * mc_reset() set the want_mcsetup flag indicating that we should do
	 * it.
	 */
	if (sc->want_mcsetup) {
		mc_setup(sc, (caddr_t)sc->xmit_cbuffs[sc->xctail]);
		sc->want_mcsetup = 0;
	}

	/* Done with the buffer. */
	sc->xmit_free++;
	sc->xmit_busy = 0;
	sc->xctail = (sc->xctail + 1) % NTXBUF;

	iestart(&sc->sc_arpcom.ac_if);
}

/*
 * Compare two Ether/802 addresses for equality, inlined and unrolled for
 * speed.  I'd love to have an inline assembler version of this...
 */
static __inline int
ether_equal(one, two)
	u_char *one, *two;
{

	if (one[0] != two[0] || one[1] != two[1] || one[2] != two[2] ||
	    one[3] != two[3] || one[4] != two[4] || one[5] != two[5])
		return 0;
	return 1;
}

/*
 * Check for a valid address.  to_bpf is filled in with one of the following:
 *   0 -> BPF doesn't get this packet
 *   1 -> BPF does get this packet
 *   2 -> BPF does get this packet, but we don't
 * Return value is true if the packet is for us, and false otherwise.
 *
 * This routine is a mess, but it's also critical that it be as fast
 * as possible.  It could be made cleaner if we can assume that the
 * only client which will fiddle with IFF_PROMISC is BPF.  This is
 * probably a good assumption, but we do not make it here.  (Yet.)
 */
static __inline int
check_eh(sc, eh, to_bpf)
	struct ie_softc *sc;
	struct ether_header *eh;
	int *to_bpf;
{
	int i;

	switch(sc->promisc) {
	case IFF_ALLMULTI:
		/*
		 * Receiving all multicasts, but no unicasts except those
		 * destined for us.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.if_bpf != 0); /* BPF gets this packet if anybody cares */
#endif
		if (eh->ether_dhost[0] & 1)
			return 1;
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr)) return 1;
		return 0;

	case IFF_PROMISC:
		/*
		 * Receiving all packets.  These need to be passed on to BPF.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.if_bpf != 0) ||
		    (sc->sc_arpcom.ac_if.if_bridge != NULL);
#else
		*to_bpf = (sc->sc_arpcom.ac_if.if_bridge != NULL);
#endif
		/* If for us, accept and hand up to BPF */
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr)) return 1;

#if NBPFILTER > 0
		if (*to_bpf && sc->sc_arpcom.ac_if.if_bridge == NULL)
			*to_bpf = 2; /* we don't need to see it */
#endif

		/*
		 * Not a multicast, so BPF wants to see it but we don't.
		 */
		if (!(eh->ether_dhost[0] & 1))
			return 1;

		/*
		 * If it's one of our multicast groups, accept it and pass it
		 * up.
		 */
		for (i = 0; i < sc->mcast_count; i++) {
			if (ether_equal(eh->ether_dhost, (u_char *)&sc->mcast_addrs[i])) {
#if NBPFILTER > 0
				if (*to_bpf)
					*to_bpf = 1;
#endif
				return 1;
			}
		}
		return 1;

	case IFF_ALLMULTI | IFF_PROMISC:
		/*
		 * Acting as a multicast router, and BPF running at the same
		 * time.  Whew!  (Hope this is a fast machine...)
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.if_bpf != 0) ||
		    (sc->sc_arpcom.ac_if.if_bridge != NULL);
#else
		*to_bpf = (sc->sc_arpcom.ac_if.if_bridge != 0);
#endif
		/* We want to see multicasts. */
		if (eh->ether_dhost[0] & 1)
			return 1;

		/* We want to see our own packets */
		if (ether_equal(eh->ether_dhost, sc->sc_arpcom.ac_enaddr))
			return 1;

		/* Anything else goes to BPF but nothing else. */
#if NBPFILTER > 0
		if (*to_bpf && sc->sc_arpcom.ac_if.if_bridge == NULL)
			*to_bpf = 2;
#endif
		return 1;

	default:
		/*
		 * Only accept unicast packets destined for us, or multicasts
		 * for groups that we belong to.  For now, we assume that the
		 * '586 will only return packets that we asked it for.  This
		 * isn't strictly true (it uses hashing for the multicast
		 * filter), but it will do in this case, and we want to get out
		 * of here as quickly as possible.
		 */
#if NBPFILTER > 0
		*to_bpf = (sc->sc_arpcom.ac_if.if_bpf != 0);
#endif
		return 1;
	}
	return 0;
}

/*
 * We want to isolate the bits that have meaning...  This assumes that
 * IE_RBUF_SIZE is an even power of two.  If somehow the act_len exceeds
 * the size of the buffer, then we are screwed anyway.
 */
static __inline int
ie_buflen(sc, head)
	struct ie_softc *sc;
	int head;
{

	return (SWAP(sc->rbuffs[head]->ie_rbd_actual)
	    & (IE_RBUF_SIZE | (IE_RBUF_SIZE - 1)));
}

static __inline int
ie_packet_len(sc)
	struct ie_softc *sc;
{
	int i;
	int head = sc->rbhead;
	int acc = 0;

	do {
		if (!(sc->rbuffs[sc->rbhead]->ie_rbd_actual & IE_RBD_USED)) {
#ifdef IEDEBUG
			print_rbd(sc->rbuffs[sc->rbhead]);
#endif
			log(LOG_ERR, "%s: receive descriptors out of sync at %d\n",
			    sc->sc_dev.dv_xname, sc->rbhead);
			iereset(sc);
			return -1;
		}

		i = sc->rbuffs[head]->ie_rbd_actual & IE_RBD_LAST;

		acc += ie_buflen(sc, head);
		head = (head + 1) % sc->nrxbuf;
	} while (!i);

	return acc;
}

/*
 * Setup all necessary artifacts for an XMIT command, and then pass the XMIT
 * command to the chip to be executed.  On the way, if we have a BPF listener
 * also give him a copy.
 */
static __inline void
iexmit(sc)
	struct ie_softc *sc;
{

#if NBPFILTER > 0
	/*
	 * If BPF is listening on this interface, let it see the packet before
	 * we push it on the wire.
	 */
	if (sc->sc_arpcom.ac_if.if_bpf)
		bpf_tap(sc->sc_arpcom.ac_if.if_bpf,
		    sc->xmit_cbuffs[sc->xctail],
		    SWAP(sc->xmit_buffs[sc->xctail]->ie_xmit_flags));
#endif

	sc->xmit_buffs[sc->xctail]->ie_xmit_flags |= IE_XMIT_LAST;
	sc->xmit_buffs[sc->xctail]->ie_xmit_next = SWAP(0xffff);
	ST_24(sc->xmit_buffs[sc->xctail]->ie_xmit_buf,
	    MK_24(sc->sc_iobase, sc->xmit_cbuffs[sc->xctail]));

	sc->xmit_cmds[sc->xctail]->com.ie_cmd_link = SWAP(0xffff);
	sc->xmit_cmds[sc->xctail]->com.ie_cmd_cmd =
	  IE_CMD_XMIT | IE_CMD_INTR | IE_CMD_LAST;

	sc->xmit_cmds[sc->xctail]->ie_xmit_status = SWAP(0);
	sc->xmit_cmds[sc->xctail]->ie_xmit_desc =
	    MK_16(sc->sc_maddr, sc->xmit_buffs[sc->xctail]);

	sc->scb->ie_command_list =
	  MK_16(sc->sc_maddr, sc->xmit_cmds[sc->xctail]);
	command_and_wait(sc, IE_CU_START, 0, 0);

	sc->xmit_busy = 1;
	sc->sc_arpcom.ac_if.if_timer = 5;
}

/*
 * Read data off the interface, and turn it into an mbuf chain.
 *
 * This code is DRAMATICALLY different from the previous version; this
 * version tries to allocate the entire mbuf chain up front, given the
 * length of the data available.  This enables us to allocate mbuf
 * clusters in many situations where before we would have had a long
 * chain of partially-full mbufs.  This should help to speed up the
 * operation considerably.  (Provided that it works, of course.)
 */
static inline int
ieget(sc, mp, ehp, to_bpf)
	struct ie_softc *sc;
	struct mbuf **mp;
	struct ether_header *ehp;
	int *to_bpf;
{
	struct mbuf *m, *top, **mymp;
	int i;
	int offset;
	int totlen, resid;
	int thismboff;
	int head;

	totlen = ie_packet_len(sc);
	if (totlen <= 0)
		return -1;

	i = sc->rbhead;

	/*
	 * Snarf the Ethernet header.
	 */
	(sc->memcopy)((caddr_t)sc->cbuffs[i], (caddr_t)ehp, sizeof *ehp);

	/*
	 * As quickly as possible, check if this packet is for us.
	 * If not, don't waste a single cycle copying the rest of the
	 * packet in.
	 * This is only a consideration when FILTER is defined; i.e., when
	 * we are either running BPF or doing multicasting.
	 */
	if (!check_eh(sc, ehp, to_bpf)) {
		ie_drop_packet_buffer(sc);
		sc->sc_arpcom.ac_if.if_ierrors--; /* just this case, it's not an error */
		return -1;
	}
	totlen -= (offset = sizeof *ehp);

	MGETHDR(*mp, M_DONTWAIT, MT_DATA);
	if (!*mp) {
		ie_drop_packet_buffer(sc);
		return -1;
	}

	m = *mp;
	m->m_pkthdr.rcvif = &sc->sc_arpcom.ac_if;
	m->m_len = MHLEN;
	resid = m->m_pkthdr.len = totlen;
	top = 0;
	mymp = &top;

	/*
	 * This loop goes through and allocates mbufs for all the data we will
	 * be copying in.  It does not actually do the copying yet.
	 */
	do {				/* while (resid > 0) */
		/*
		 * Try to allocate an mbuf to hold the data that we have.  If
		 * we already allocated one, just get another one and stick it
		 * on the end (eventually).  If we don't already have one, try
		 * to allocate an mbuf cluster big enough to hold the whole
		 * packet, if we think it's reasonable, or a single mbuf which
		 * may or may not be big enough.
		 * Got that?
		 */
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (!m) {
				m_freem(top);
				ie_drop_packet_buffer(sc);
				return -1;
			}
			m->m_len = MLEN;
		}

		if (resid >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = min(resid, MCLBYTES);
		} else {
			if (resid < m->m_len) {
				if (!top && resid + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = resid;
			}
		}
		resid -= m->m_len;
		*mymp = m;
		mymp = &m->m_next;
	} while (resid > 0);

	resid = totlen;
	m = top;
	thismboff = 0;
	head = sc->rbhead;

	/*
	 * Now we take the mbuf chain (hopefully only one mbuf most of the
	 * time) and stuff the data into it.  There are no possible failures at
	 * or after this point.
	 */
	while (resid > 0) {		/* while there's stuff left */
		int thislen = ie_buflen(sc, head) - offset;

		/*
		 * If too much data for the current mbuf, then fill the current
		 * one up, go to the next one, and try again.
		 */
		if (thislen > m->m_len - thismboff) {
			int newlen = m->m_len - thismboff;
			(sc->memcopy)((caddr_t)(sc->cbuffs[head] + offset),
			    mtod(m, caddr_t) + thismboff, (u_int)newlen);
			m = m->m_next;
			thismboff = 0;		/* new mbuf, so no offset */
			offset += newlen;	/* we are now this far
							into the packet */
			resid -= newlen;	/* so there is this much
							left to get */
			continue;
		}

		/*
		 * If there is more than enough space in the mbuf to hold the
		 * contents of this buffer, copy everything in, advance
		 * pointers and so on.
		 */
		if (thislen < m->m_len - thismboff) {
			(sc->memcopy)((caddr_t)(sc->cbuffs[head] + offset),
			    mtod(m, caddr_t) + thismboff, (u_int)thislen);
			thismboff += thislen;	/* we are this far into the mbuf */
			resid -= thislen;	/* and this much is left */
			goto nextbuf;
		}

		/*
		 * Otherwise, there is exactly enough space to put this
		 * buffer's contents into the current mbuf.  Do the combination
		 * of the above actions.
		 */
		(sc->memcopy)((caddr_t)(sc->cbuffs[head] + offset),
		    mtod(m, caddr_t) + thismboff, (u_int)thislen);
		m = m->m_next;
		thismboff = 0;		/* new mbuf, start at the beginning */
		resid -= thislen;	/* and we are this far through */

		/*
		 * Advance all the pointers.  We can get here from either of
		 * the last two cases, but never the first.
		 */
	nextbuf:
		offset = 0;
		sc->rbuffs[head]->ie_rbd_actual = SWAP(0);
		sc->rbuffs[head]->ie_rbd_length |= IE_RBD_LAST;
		sc->rbhead = head = (head + 1) % sc->nrxbuf;
		sc->rbuffs[sc->rbtail]->ie_rbd_length &= ~IE_RBD_LAST;
		sc->rbtail = (sc->rbtail + 1) % sc->nrxbuf;
	}

	/*
	 * Unless something changed strangely while we were doing the copy, we
	 * have now copied everything in from the shared memory.
	 * This means that we are done.
	 */
	return 0;
}

/*
 * Read frame NUM from unit UNIT (pre-cached as IE).
 *
 * This routine reads the RFD at NUM, and copies in the buffers from the list
 * of RBD, then rotates the RBD and RFD lists so that the receiver doesn't
 * start complaining.  Trailers are DROPPED---there's no point in wasting time
 * on confusing code to deal with them.  Hopefully, this machine will never ARP
 * for trailers anyway.
 */
static void
ie_readframe(sc, num)
	struct ie_softc *sc;
	int num;			/* frame number to read */
{
	int status;
	struct mbuf *m = 0;
	struct ether_header eh;
#if NBPFILTER > 0
	int bpf_gets_it = 0;
#endif

	status = sc->rframes[num]->ie_fd_status;

	/* Immediately advance the RFD list, since we have copied ours now. */
	sc->rframes[num]->ie_fd_status = SWAP(0);
	sc->rframes[num]->ie_fd_last |= IE_FD_LAST;
	sc->rframes[sc->rftail]->ie_fd_last &= ~IE_FD_LAST;
	sc->rftail = (sc->rftail + 1) % sc->nframes;
	sc->rfhead = (sc->rfhead + 1) % sc->nframes;

	if (status & IE_FD_OK) {
#if NBPFILTER > 0
		if (ieget(sc, &m, &eh, &bpf_gets_it)) {
#else
		if (ieget(sc, &m, &eh, 0)) {
#endif
			sc->sc_arpcom.ac_if.if_ierrors++;
			return;
		}
	}

#ifdef IEDEBUG
	if (sc->sc_debug & IED_READFRAME)
		printf("%s: frame from ether %s type 0x%x\n",
		    sc->sc_dev.dv_xname,
		    ether_sprintf(eh.ether_shost), (u_int)eh.ether_type);
#endif

	if (!m)
		return;

	if (last_not_for_us) {
		m_freem(last_not_for_us);
		last_not_for_us = 0;
	}

#if NBPFILTER > 0
	/*
	 * Check for a BPF filter; if so, hand it up.
	 * Note that we have to stick an extra mbuf up front, because bpf_mtap
	 * expects to have the ether header at the front.
	 * It doesn't matter that this results in an ill-formatted mbuf chain,
	 * since BPF just looks at the data.  (It doesn't try to free the mbuf,
	 * tho' it will make a copy for tcpdump.)
	 */
	if (bpf_gets_it) {
		struct mbuf m0;
		m0.m_len = sizeof eh;
		m0.m_data = (caddr_t)&eh;
		m0.m_next = m;

		/* Pass it up. */
		bpf_mtap(sc->sc_arpcom.ac_if.if_bpf, &m0);
	}
	/*
	 * A signal passed up from the filtering code indicating that the
	 * packet is intended for BPF but not for the protocol machinery.
	 * We can save a few cycles by not handing it off to them.
	 */
	if (bpf_gets_it == 2) {
		last_not_for_us = m;
		return;
	}
#endif /* NBPFILTER > 0 */

	/*
	 * In here there used to be code to check destination addresses upon
	 * receipt of a packet.  We have deleted that code, and replaced it
	 * with code to check the address much earlier in the cycle, before
	 * copying the data in; this saves us valuable cycles when operating
	 * as a multicast router or when using BPF.
	 */

	/*
	 * Finally pass this packet up to higher layers.
	 */
	ether_input(&sc->sc_arpcom.ac_if, &eh, m);
}

static void
ie_drop_packet_buffer(sc)
	struct ie_softc *sc;
{
	int i;

	do {
		/*
		 * This means we are somehow out of sync.  So, we reset the
		 * adapter.
		 */
		if (!(sc->rbuffs[sc->rbhead]->ie_rbd_actual & IE_RBD_USED)) {
#ifdef IEDEBUG
			print_rbd(sc->rbuffs[sc->rbhead]);
#endif
			log(LOG_ERR, "%s: receive descriptors out of sync at %d\n",
			    sc->sc_dev.dv_xname, sc->rbhead);
			iereset(sc);
			return;
		}

		i = sc->rbuffs[sc->rbhead]->ie_rbd_actual & IE_RBD_LAST;

		sc->rbuffs[sc->rbhead]->ie_rbd_length |= IE_RBD_LAST;
		sc->rbuffs[sc->rbhead]->ie_rbd_actual = SWAP(0);
		sc->rbhead = (sc->rbhead + 1) % sc->nrxbuf;
		sc->rbuffs[sc->rbtail]->ie_rbd_length &= ~IE_RBD_LAST;
		sc->rbtail = (sc->rbtail + 1) % sc->nrxbuf;
	} while (!i);
}


/*
 * Start transmission on an interface.
 */
void
iestart(ifp)
	struct ifnet *ifp;
{
	struct ie_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	u_char *buffer;
	u_short len;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	if (sc->xmit_free == 0) {
		ifp->if_flags |= IFF_OACTIVE;
		if (!sc->xmit_busy)
			iexmit(sc);
		return;
	}

	do {
		IF_DEQUEUE(&sc->sc_arpcom.ac_if.if_snd, m);
		if (!m)
			break;

		len = 0;
		buffer = sc->xmit_cbuffs[sc->xchead];

		for (m0 = m; m && (len +m->m_len) < IE_TBUF_SIZE;
		                                           m = m->m_next) {
			bcopy(mtod(m, caddr_t), buffer, m->m_len);
			buffer += m->m_len;
			len += m->m_len;
		}
		if (m)
		  printf("%s: tbuf overflow\n", sc->sc_dev.dv_xname);

		m_freem(m0);
		len = max(len, ETHER_MIN_LEN);
		sc->xmit_buffs[sc->xchead]->ie_xmit_flags = SWAP(len);

		sc->xmit_free--;
		sc->xchead = (sc->xchead + 1) % NTXBUF;
	} while (sc->xmit_free > 0);

	/* If we stuffed any packets into the card's memory, send now. */
	if ((sc->xmit_free < NTXBUF) && (!sc->xmit_busy))
		iexmit(sc);

	return;
}

/*
 * set up IE's ram space
 */
int
ie_setupram(sc)
	struct ie_softc *sc;
{
	volatile struct ie_sys_conf_ptr *scp;
	volatile struct ie_int_sys_conf_ptr *iscp;
	volatile struct ie_sys_ctl_block *scb;
	int     s;

	s = splnet();

	scp = sc->scp;
	(sc->memzero)((char *) scp, sizeof *scp);

	iscp = sc->iscp;
	(sc->memzero)((char *) iscp, sizeof *iscp);

	scb = sc->scb;
	(sc->memzero)((char *) scb, sizeof *scb);

	scp->ie_bus_use = 0;	/* 16-bit */
	ST_24(scp->ie_iscp_ptr, MK_24(sc->sc_iobase, iscp));

	iscp->ie_busy = 1;	/* ie_busy == char */
	iscp->ie_scb_offset = MK_16(sc->sc_maddr, scb);
	ST_24(iscp->ie_base, MK_24(sc->sc_iobase, sc->sc_maddr));

	(sc->reset_586) (sc);
	(sc->chan_attn) (sc);

	delay(100);		/* wait a while... */

	if (iscp->ie_busy) {
		splx(s);
		return 0;
	}
	/*
	 * Acknowledge any interrupts we may have caused...
	 */
	ie_ack(sc, IE_ST_WHENCE);
	splx(s);

	return 1;
}

void
iereset(sc)
	struct ie_softc *sc;
{
	int s = splnet();

	printf("%s: reset\n", sc->sc_dev.dv_xname);

	/* Clear OACTIVE in case we're called from watchdog (frozen xmit). */
	sc->sc_arpcom.ac_if.if_flags &= ~(IFF_UP | IFF_OACTIVE);
	ieioctl(&sc->sc_arpcom.ac_if, SIOCSIFFLAGS, 0);

	/*
	 * Stop i82586 dead in its tracks.
	 */
	if (command_and_wait(sc, IE_RU_ABORT | IE_CU_ABORT, 0, 0))
		printf("%s: abort commands timed out\n", sc->sc_dev.dv_xname);

	if (command_and_wait(sc, IE_RU_DISABLE | IE_CU_STOP, 0, 0))
		printf("%s: disable commands timed out\n", sc->sc_dev.dv_xname);

#ifdef notdef
	if (!check_ie_present(sc, sc->sc_maddr, sc->sc_msize))
		panic("ie disappeared!");
#endif

	sc->sc_arpcom.ac_if.if_flags |= IFF_UP;
	ieioctl(&sc->sc_arpcom.ac_if, SIOCSIFFLAGS, 0);

	splx(s);
}

/*
 * This is called if we time out.
 */
static void
chan_attn_timeout(rock)
	void *rock;
{

	*(int *)rock = 1;
}

/*
 * Send a command to the controller and wait for it to either complete
 * or be accepted, depending on the command.  If the command pointer
 * is null, then pretend that the command is not an action command.
 * If the command pointer is not null, and the command is an action
 * command, wait for
 * ((volatile struct ie_cmd_common *)pcmd)->ie_cmd_status & MASK
 * to become true.
 */
static int
command_and_wait(sc, cmd, pcmd, mask)
	struct ie_softc *sc;
	int cmd;
	volatile void *pcmd;
	int mask;
{
	volatile struct ie_cmd_common *cc = pcmd;
	volatile struct ie_sys_ctl_block *scb = sc->scb;
	volatile int timedout = 0;
	struct timeout chan_tmo;
	extern int hz;

	scb->ie_command = (u_short)cmd;

	if (IE_ACTION_COMMAND(cmd) && pcmd) {
		(sc->chan_attn)(sc);

                /*
                 * XXX
                 * I don't think this timeout works on suns.
                 * we are at splnet() in the loop, and the timeout
                 * stuff runs at software spl (so it is masked off?).
                 */

		/*
		 * According to the packet driver, the minimum timeout should
		 * be .369 seconds, which we round up to .4.
		 */
		timeout_set(&chan_tmo, chan_attn_timeout, (caddr_t)&timedout);
		timeout_add(&chan_tmo, 2 * hz / 5);

		/*
		 * Now spin-lock waiting for status.  This is not a very nice
		 * thing to do, but I haven't figured out how, or indeed if, we
		 * can put the process waiting for action to sleep.  (We may
		 * be getting called through some other timeout running in the
		 * kernel.)
		 */
		for (;;)
			if ((cc->ie_cmd_status & mask) || timedout)
				break;

		timeout_del(&chan_tmo);

		return timedout;
	} else {
		/*
		 * Otherwise, just wait for the command to be accepted.
		 */
		(sc->chan_attn)(sc);

		while (scb->ie_command)
			;				/* XXX spin lock */

		return 0;
	}
}

/*
 * Run the time-domain reflectometer.
 */
static void
run_tdr(sc, cmd)
	struct ie_softc *sc;
	struct ie_tdr_cmd *cmd;
{
	int result;

	cmd->com.ie_cmd_status = SWAP(0);
	cmd->com.ie_cmd_cmd = IE_CMD_TDR | IE_CMD_LAST;
	cmd->com.ie_cmd_link = SWAP(0xffff);

	sc->scb->ie_command_list = MK_16(sc->sc_maddr, cmd);
	cmd->ie_tdr_time = SWAP(0);

	if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
	    !(cmd->com.ie_cmd_status & IE_STAT_OK))
		result = 0x10000; /* XXX */
	else
		result = cmd->ie_tdr_time;

	ie_ack(sc, IE_ST_WHENCE);

	if (result & IE_TDR_SUCCESS)
		return;

	if (result & 0x10000)
		printf("%s: TDR command failed\n", sc->sc_dev.dv_xname);
	else if (result & IE_TDR_XCVR)
		printf("%s: transceiver problem\n", sc->sc_dev.dv_xname);
	else if (result & IE_TDR_OPEN)
		printf("%s: TDR detected an open %d clocks away\n",
		    sc->sc_dev.dv_xname, result & IE_TDR_TIME);
	else if (result & IE_TDR_SHORT)
		printf("%s: TDR detected a short %d clocks away\n",
		    sc->sc_dev.dv_xname, result & IE_TDR_TIME);
	else
		printf("%s: TDR returned unknown status 0x%x\n",
		    sc->sc_dev.dv_xname, result);
}

#ifdef notdef
/* ALIGN works on 8 byte boundaries.... but 4 byte boundaries are ok for sun */
#define	_ALLOC(p, n)	(bzero(p, n), p += n, p - n)
#define	ALLOC(p, n)	_ALLOC(p, ALIGN(n)) /* XXX convert to this? */
#endif

static __inline caddr_t
Align(ptr)
        caddr_t ptr;
{
        u_long  l = (u_long)ptr;

        l = (l + 3) & ~3L;
        return (caddr_t)l;
}

/*
 * setup_bufs: set up the buffers
 *
 * we have a block of KVA at sc->buf_area which is of size sc->buf_area_sz.
 * this is to be used for the buffers.  the chip indexs its control data
 * structures with 16 bit offsets, and it indexes actual buffers with
 * 24 bit addresses.   so we should allocate control buffers first so that
 * we don't overflow the 16 bit offset field.   The number of transmit
 * buffers is fixed at compile time.
 *
 * note: this function was written to be easy to understand, rather than
 *       highly efficient (it isn't in the critical path).
 */
static void
setup_bufs(sc)
	struct ie_softc *sc;
{
	caddr_t ptr = sc->buf_area;	/* memory pool */
	int     n, r;

	/*
	 * step 0: zero memory and figure out how many recv buffers and
	 * frames we can have.   XXX CURRENTLY HARDWIRED AT MAX
	 */
	(sc->memzero)(ptr, sc->buf_area_sz);
	ptr = Align(ptr);	/* set alignment and stick with it */

	n = (int)Align((caddr_t) sizeof(struct ie_xmit_cmd)) +
	    (int)Align((caddr_t) sizeof(struct ie_xmit_buf)) + IE_TBUF_SIZE;
	n *= NTXBUF;		/* n = total size of xmit area */

	n = sc->buf_area_sz - n;/* n = free space for recv stuff */

	r = (int)Align((caddr_t) sizeof(struct ie_recv_frame_desc)) +
	    (((int)Align((caddr_t) sizeof(struct ie_recv_buf_desc)) +
		IE_RBUF_SIZE) * B_PER_F);

	/* r = size of one R frame */

	sc->nframes = n / r;
	if (sc->nframes <= 0)
		panic("ie: bogus buffer calc");
	if (sc->nframes > MXFRAMES)
		sc->nframes = MXFRAMES;

	sc->nrxbuf = sc->nframes * B_PER_F;

#ifdef IEDEBUG
	printf("IEDEBUG: %d frames %d bufs\n", sc->nframes, sc->nrxbuf);
#endif

	/*
	 *  step 1a: lay out and zero frame data structures for transmit and recv
	 */
	for (n = 0; n < NTXBUF; n++) {
		sc->xmit_cmds[n] = (volatile struct ie_xmit_cmd *) ptr;
		ptr = Align(ptr + sizeof(struct ie_xmit_cmd));
	}

	for (n = 0; n < sc->nframes; n++) {
		sc->rframes[n] = (volatile struct ie_recv_frame_desc *) ptr;
		ptr = Align(ptr + sizeof(struct ie_recv_frame_desc));
	}

	/*
	 * step 1b: link together the recv frames and set EOL on last one
	 */
	for (n = 0; n < sc->nframes; n++) {
		sc->rframes[n]->ie_fd_next =
		    MK_16(sc->sc_maddr, sc->rframes[(n + 1) % sc->nframes]);
	}
	sc->rframes[sc->nframes - 1]->ie_fd_last |= IE_FD_LAST;

	/*
	 * step 2a: lay out and zero frame buffer structures for xmit and recv
	 */
	for (n = 0; n < NTXBUF; n++) {
		sc->xmit_buffs[n] = (volatile struct ie_xmit_buf *) ptr;
		ptr = Align(ptr + sizeof(struct ie_xmit_buf));
	}

	for (n = 0; n < sc->nrxbuf; n++) {
		sc->rbuffs[n] = (volatile struct ie_recv_buf_desc *) ptr;
		ptr = Align(ptr + sizeof(struct ie_recv_buf_desc));
	}

	/*
	 * step 2b: link together recv bufs and set EOL on last one
	 */
	for (n = 0; n < sc->nrxbuf; n++) {
		sc->rbuffs[n]->ie_rbd_next =
		    MK_16(sc->sc_maddr, sc->rbuffs[(n + 1) % sc->nrxbuf]);
	}
	sc->rbuffs[sc->nrxbuf - 1]->ie_rbd_length |= IE_RBD_LAST;

	/*
	 * step 3: allocate the actual data buffers for xmit and recv
	 * recv buffer gets linked into recv_buf_desc list here
	 */
	for (n = 0; n < NTXBUF; n++) {
		sc->xmit_cbuffs[n] = (u_char *) ptr;
		ptr = Align(ptr + IE_TBUF_SIZE);
	}

	/* Pointers to last packet sent and next available transmit buffer. */
	sc->xchead = sc->xctail = 0;

	/* Clear transmit-busy flag and set number of free transmit buffers. */
	sc->xmit_busy = 0;
	sc->xmit_free = NTXBUF;

	for (n = 0; n < sc->nrxbuf; n++) {
		sc->cbuffs[n] = (char *) ptr;	/* XXX why char vs uchar? */
		sc->rbuffs[n]->ie_rbd_length = SWAP(IE_RBUF_SIZE);
		ST_24(sc->rbuffs[n]->ie_rbd_buffer, MK_24(sc->sc_iobase, ptr));
		ptr = Align(ptr + IE_RBUF_SIZE);
	}

	/*
	 * step 4: set the head and tail pointers on receive to keep track of
	 * the order in which RFDs and RBDs are used.   link in recv frames
	 * and buffer into the scb.
	 */

	sc->rfhead = 0;
	sc->rftail = sc->nframes - 1;
	sc->rbhead = 0;
	sc->rbtail = sc->nrxbuf - 1;

	sc->scb->ie_recv_list = MK_16(sc->sc_maddr, sc->rframes[0]);
	sc->rframes[0]->ie_fd_buf_desc = MK_16(sc->sc_maddr, sc->rbuffs[0]);

#ifdef IEDEBUG
	printf("IE_DEBUG: reserved %d bytes\n", ptr - sc->buf_area);
#endif
}

/*
 * Run the multicast setup command.
 * Called at splnet().
 */
static int
mc_setup(sc, ptr)
	struct ie_softc *sc;
	void *ptr;
{
	volatile struct ie_mcast_cmd *cmd = ptr;

	cmd->com.ie_cmd_status = SWAP(0);
	cmd->com.ie_cmd_cmd = IE_CMD_MCAST | IE_CMD_LAST;
	cmd->com.ie_cmd_link = SWAP(0xffff);

	(sc->memcopy)((caddr_t)sc->mcast_addrs, (caddr_t)cmd->ie_mcast_addrs,
	    sc->mcast_count * sizeof *sc->mcast_addrs);

	cmd->ie_mcast_bytes =
	  SWAP(sc->mcast_count * ETHER_ADDR_LEN); /* grrr... */

	sc->scb->ie_command_list = MK_16(sc->sc_maddr, cmd);
	if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
	    !(cmd->com.ie_cmd_status & IE_STAT_OK)) {
		printf("%s: multicast address setup command failed\n",
		    sc->sc_dev.dv_xname);
		return 0;
	}
	return 1;
}

/*
 * This routine takes the environment generated by check_ie_present() and adds
 * to it all the other structures we need to operate the adapter.  This
 * includes executing the CONFIGURE, IA-SETUP, and MC-SETUP commands, starting
 * the receiver unit, and clearing interrupts.
 *
 * THIS ROUTINE MUST BE CALLED AT splnet() OR HIGHER.
 */
int
ieinit(sc)
	struct ie_softc *sc;
{
	volatile struct ie_sys_ctl_block *scb = sc->scb;
	void *ptr;

	ptr = sc->buf_area;

	/*
	 * Send the configure command first.
	 */
	{
		volatile struct ie_config_cmd *cmd = ptr;

		scb->ie_command_list = MK_16(sc->sc_maddr, cmd);
		cmd->com.ie_cmd_status = SWAP(0);
		cmd->com.ie_cmd_cmd = IE_CMD_CONFIG | IE_CMD_LAST;
		cmd->com.ie_cmd_link = SWAP(0xffff);

		ie_setup_config(cmd, sc->promisc, 0);

		if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
		    !(cmd->com.ie_cmd_status & IE_STAT_OK)) {
			printf("%s: configure command failed\n",
			    sc->sc_dev.dv_xname);
			return 0;
		}
	}

	/*
	 * Now send the Individual Address Setup command.
	 */
	{
		volatile struct ie_iasetup_cmd *cmd = ptr;

		scb->ie_command_list = MK_16(sc->sc_maddr, cmd);
		cmd->com.ie_cmd_status = SWAP(0);
		cmd->com.ie_cmd_cmd = IE_CMD_IASETUP | IE_CMD_LAST;
		cmd->com.ie_cmd_link = SWAP(0xffff);

		(sc->memcopy)(sc->sc_arpcom.ac_enaddr,
		      (caddr_t)&cmd->ie_address, sizeof cmd->ie_address);

		if (command_and_wait(sc, IE_CU_START, cmd, IE_STAT_COMPL) ||
		    !(cmd->com.ie_cmd_status & IE_STAT_OK)) {
			printf("%s: individual address setup command failed\n",
			    sc->sc_dev.dv_xname);
			return 0;
		}
	}

	/*
	 * Now run the time-domain reflectometer.
	 */
	run_tdr(sc, ptr);

	/*
	 * Acknowledge any interrupts we have generated thus far.
	 */
	ie_ack(sc, IE_ST_WHENCE);

	/*
	 * Set up the transmit and recv buffers.
	 */
	setup_bufs(sc);

	sc->sc_arpcom.ac_if.if_flags |= IFF_RUNNING; /* tell higher levels that we are here */

	sc->scb->ie_recv_list = MK_16(sc->sc_maddr, sc->rframes[0]);
	command_and_wait(sc, IE_RU_START, 0, 0);

	ie_ack(sc, IE_ST_WHENCE);

	if (sc->run_586)
	  (sc->run_586)(sc);

	return 0;
}

static void
iestop(sc)
	struct ie_softc *sc;
{

	command_and_wait(sc, IE_RU_DISABLE, 0, 0);
}

int
ieioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ie_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch(cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			ieinit(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
#ifdef NS
		/* XXX - This code is probably wrong. */
		case AF_NS:
		    {
			struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)(sc->sc_arpcom.ac_enaddr);
			else
				bcopy(ina->x_host.c_host,
				    sc->sc_arpcom.ac_enaddr,
				    sizeof(sc->sc_arpcom.ac_enaddr));
			/* Set new address. */
			ieinit(sc);
			break;
		    }
#endif /* NS */
		default:
			ieinit(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		sc->promisc = ifp->if_flags & (IFF_PROMISC | IFF_ALLMULTI);
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			iestop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			ieinit(sc);
		} else {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			iestop(sc);
			ieinit(sc);
		}
#ifdef IEDEBUG
		if (ifp->if_flags & IFF_DEBUG)
			sc->sc_debug = IED_ALL;
		else
			sc->sc_debug = 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom):
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			mc_reset(sc);
			error = 0;
		}
		break;

	default:
		error = EINVAL;
	}
	splx(s);
	return error;
}

static void
mc_reset(sc)
	struct ie_softc *sc;
{
	struct ether_multi *enm;
	struct ether_multistep step;

	/*
	 * Step through the list of addresses.
	 */
	sc->mcast_count = 0;
	ETHER_FIRST_MULTI(step, &sc->sc_arpcom, enm);
	while (enm) {
		if (sc->mcast_count >= MAXMCAST ||
		    bcmp(enm->enm_addrlo, enm->enm_addrhi, 6) != 0) {
			sc->sc_arpcom.ac_if.if_flags |= IFF_ALLMULTI;
			ieioctl(&sc->sc_arpcom.ac_if, SIOCSIFFLAGS, (void *)0);
			goto setflag;
		}

		bcopy(enm->enm_addrlo, &sc->mcast_addrs[sc->mcast_count], 6);
		sc->mcast_count++;
		ETHER_NEXT_MULTI(step, enm);
	}
setflag:
	sc->want_mcsetup = 1;
}

#ifdef IEDEBUG
void
print_rbd(rbd)
	volatile struct ie_recv_buf_desc *rbd;
{

	printf("RBD at %08lx:\nactual %04x, next %04x, buffer %08x\n"
	    "length %04x, mbz %04x\n", (u_long)rbd, rbd->ie_rbd_actual,
	    rbd->ie_rbd_next, rbd->ie_rbd_buffer, rbd->ie_rbd_length,
	    rbd->mbz);
}
#endif
