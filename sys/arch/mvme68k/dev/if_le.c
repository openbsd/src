/*	$OpenBSD: if_le.c,v 1.13 2000/01/24 05:20:53 smurph Exp $ */

/*-
 * Copyright (c) 1982, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)if_le.c	8.2 (Berkeley) 10/30/93
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#define LEDEBUG 1
#include <dev/ic/am7990reg.h>
#include <dev/ic/am7990var.h>

#include <mvme68k/dev/if_lereg.h>
#include <mvme68k/dev/if_levar.h>
#include <mvme68k/dev/pccreg.h>
#include <mvme68k/dev/vme.h>

/* autoconfiguration driver */
void	leattach(struct device *, struct device *, void *);
int	lematch(struct device *, void *, void *);

struct cfattach le_ca = {
	sizeof(struct le_softc), lematch, leattach
};

static int lebustype;

hide void lewrcsr __P((struct am7990_softc *, u_int16_t, u_int16_t));
hide u_int16_t lerdcsr __P((struct am7990_softc *, u_int16_t));
hide void vlewrcsr __P((struct am7990_softc *, u_int16_t, u_int16_t));
hide u_int16_t vlerdcsr __P((struct am7990_softc *, u_int16_t));

/* send command to the nvram controller */
nvram_cmd(sc, cmd, addr )
struct am7990_softc *sc;
u_char cmd;
u_short addr;
{
   int i;
   u_char rcmd = 0;
   u_char rcmd2= 0;
	struct vlereg1 *reg1 = (struct vlereg1 *)((struct le_softc *)sc)->sc_r1;
   
   rcmd = addr;
   rcmd = rcmd << 3;
   rcmd |= cmd;
   for(i=0;i<8;i++){ 
      reg1->ler1_ear=((cmd|(addr<<1))>>i); 
      CDELAY; 
   } 
}

/* read nvram one bit at a time */
u_int16_t
nvram_read(sc, nvram_addr)
struct am7990_softc *sc;
u_char nvram_addr;
{
   u_short val = 0, mask = 0x04000;
   u_int16_t wbit;
   /* these used by macros DO NOT CHANGE!*/
   int i;
	struct vlereg1 *reg1 = (struct vlereg1 *)((struct le_softc *)sc)->sc_r1;
   ((struct le_softc *)sc)->csr = 0x4f;
   ENABLE_NVRAM;
   nvram_cmd(sc, NVRAM_RCL, 0);
   DISABLE_NVRAM;
   CDELAY;
   ENABLE_NVRAM;
   nvram_cmd(sc, NVRAM_READ, nvram_addr);
   for (wbit=0; wbit<15; wbit++) {
      (reg1->ler1_ear & 0x01) ? (val = (val | mask)) : (val = (val & (~mask)));
      mask = mask>>1;
      CDELAY;
   }
   (reg1->ler1_ear & 0x01) ? (val = (val | 0x8000)) : (val = (val & 0x7FFF));
   CDELAY;
   DISABLE_NVRAM;
   return (val);
}

void
vleetheraddr(sc)
struct am7990_softc *sc;
{
   u_char * cp = sc->sc_arpcom.ac_enaddr;
   u_int16_t ival[3];
   u_char i;
   
   for (i=0; i<3; i++) {
      ival[i] = nvram_read(sc, i);
   }
   memcpy(cp, &ival[0], 6);
}

hide void
lewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	register struct lereg1 *ler1 = (struct lereg1 *)((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

hide void
vlewrcsr(sc, port, val)
	struct am7990_softc *sc;
	u_int16_t port, val;
{
	register struct vlereg1 *ler1 = (struct vlereg1 *)((struct le_softc *)sc)->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

hide u_int16_t
lerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	register struct lereg1 *ler1 = (struct lereg1 *)((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

hide u_int16_t
vlerdcsr(sc, port)
	struct am7990_softc *sc;
	u_int16_t port;
{
	register struct vlereg1 *ler1 = (struct vlereg1 *)((struct le_softc *)sc)->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

/* init MVME376, set ipl and vec */
void
vleinit(sc)
	struct am7990_softc *sc;
{
	register struct vlereg1 *reg1 = (struct vlereg1 *)((struct le_softc *)sc)->sc_r1;
   u_char vec = ((struct le_softc *)sc)->sc_vec;
   u_char ipl = ((struct le_softc *)sc)->sc_ipl;
   ((struct le_softc *)sc)->csr = 0x4f;
   WRITE_CSR_AND( ~ipl );
   SET_VEC(vec);
	return;
}

/* MVME376 hardware reset */
void
vlereset(sc)
	struct am7990_softc *sc;
{
	register struct vlereg1 *reg1 = (struct vlereg1 *)((struct le_softc *)sc)->sc_r1;
	RESET_HW;
	if (sc->sc_debug) {
		printf("\nle: hardware reset\n");
	}
	SYSFAIL_CL;
	return;
}

int
vle_intr(sc)
	register void *sc;
{
	register struct vlereg1 *reg1 = (struct vlereg1 *)((struct le_softc *)sc)->sc_r1;
   int rc;
   rc = am7990_intr(sc);
   ENABLE_INTR;
   return (rc);
}

void
vle_copytobuf_contig(sc, from, boff, len)
	struct am7990_softc *sc;
	void *from;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;

   /* 
    * Do the cache stuff 
    */
   dma_cachectl(buf + boff, len);
	/*
	 * Just call bcopy() to do the work.
	 */
	bcopy(from, buf + boff, len);
}

void
vle_zerobuf_contig(sc, boff, len)
	struct am7990_softc *sc;
	int boff, len;
{
	volatile caddr_t buf = sc->sc_mem;
   /* 
    * Do the cache stuff 
    */
   dma_cachectl(buf + boff, len);
	/*
	 * Just let bzero() do the work
	 */
	bzero(buf + boff, len);
}

int
lematch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;

	return (!badvaddr(ca->ca_vaddr, 2));
}

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
leattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct le_softc *lesc = (struct le_softc *)self;
	struct am7990_softc *sc = &lesc->sc_am7990;
	struct confargs *ca = aux;
	register int a;
	int pri = ca->ca_ipl;
	extern void *etherbuf;
	caddr_t addr;

	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr(u_char *);
   
	lebustype = ca->ca_bustype;

	/* Are we the boot device? */
	if (ca->ca_paddr == bootaddr)
		bootdv = self;

	switch (lebustype) {
   case BUS_VMES:
      /* 
		 * get the first avaliable etherbuf.  MVME376 uses its own dual-ported 
		 * RAM for etherbuf.  It is set by dip switches on board.  We support 
		 * the four Motorola address locations, however, the board can be set up 
		 * at any other address. We must map this space into the extio map. 
		 * XXX-smurph.
		 */
      switch ((int)ca->ca_paddr) {
      case 0xFFFF1200:
         addr = (caddr_t)0xFD6C0000;
         break;
      case 0xFFFF1400:
         addr = (caddr_t)0xFD700000;
         break;
      case 0xFFFF1600:
         addr = (caddr_t)0xFD740000;
         break;
		case 0xFFFFD200:
         addr = (caddr_t)0xFD780000;
         break;
      default:
         panic("le: invalid address");
      }
      sc->sc_mem = (void *)mapiodev(addr, VLEMEMSIZE);
      if (sc->sc_mem == NULL) 
			panic("\nle: no more memory in external I/O map\n");
   	lesc->sc_r1 = (void *)ca->ca_vaddr;
      lesc->sc_ipl = ca->ca_ipl;
      lesc->sc_vec = ca->ca_vec;
   	sc->sc_memsize = VLEMEMSIZE;
   	sc->sc_conf3 = LE_C3_BSWP;
   	sc->sc_addr = kvtop(sc->sc_mem);
      sc->sc_hwreset = vlereset;
      sc->sc_rdcsr = vlerdcsr;
      sc->sc_wrcsr = vlewrcsr;
      sc->sc_hwinit = vleinit;
		sc->sc_copytodesc = vle_copytobuf_contig;
		sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
		sc->sc_copytobuf = vle_copytobuf_contig;
		sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
		sc->sc_zerobuf = am7990_zerobuf_contig;
		/* get ether address */
   	vleetheraddr(sc);
		break;		
   case BUS_PCC:
      sc->sc_mem = etherbuf;
   	lesc->sc_r1 = (void *)ca->ca_vaddr;
   	sc->sc_conf3 = LE_C3_BSWP /*| LE_C3_ACON | LE_C3_BCON*/;
   	sc->sc_addr = kvtop(sc->sc_mem);
   	sc->sc_memsize = LEMEMSIZE;
      sc->sc_rdcsr = lerdcsr;
      sc->sc_wrcsr = lewrcsr;
      sc->sc_hwreset = NULL;
      sc->sc_hwinit = NULL;
		sc->sc_copytodesc = am7990_copytobuf_contig;
		sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
		sc->sc_copytobuf = am7990_copytobuf_contig;
		sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
		sc->sc_zerobuf = am7990_zerobuf_contig;
      /* get ether address */
      myetheraddr(sc->sc_arpcom.ac_enaddr);
		break;
	default:
		panic("\nle: unknown bus type.\n");
	}
	evcnt_attach(&sc->sc_dev, "intr", &lesc->sc_intrcnt);
	evcnt_attach(&sc->sc_dev, "errs", &lesc->sc_errcnt);

	/*
	if (lebustype == BUS_VMES) 
		vleinit(sc);
	*/

	am7990_config(sc);

	/* connect the interrupt */
	switch (lebustype) {
   case BUS_VMES:
		lesc->sc_ih.ih_fn = vle_intr;
		lesc->sc_ih.ih_arg = sc;
		lesc->sc_ih.ih_ipl = pri;
		vmeintr_establish(ca->ca_vec + 0, &lesc->sc_ih);
		break;
	case BUS_PCC:
		lesc->sc_ih.ih_fn = am7990_intr;
		lesc->sc_ih.ih_arg = sc;
		lesc->sc_ih.ih_ipl = pri;
		pccintr_establish(PCCV_LE, &lesc->sc_ih);
		((struct pccreg *)ca->ca_master)->pcc_leirq = pri | PCC_IRQ_IEN;
		break;
	}
}

   

