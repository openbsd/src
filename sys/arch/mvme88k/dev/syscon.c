
/*	$OpenBSD: syscon.c,v 1.2 2000/03/26 23:32:00 deraadt Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 *      This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * VME188 SYSCON
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/frame.h>
#include <machine/board.h>
#include <dev/cons.h>

#include <mvme88k/dev/sysconreg.h>

struct sysconreg syscon_reg = {
   (volatile unsigned int*)IEN0_REG,(volatile unsigned int*)IEN1_REG,
   (volatile unsigned int*)IEN2_REG,(volatile unsigned int*)IEN3_REG,
   (volatile unsigned int*)IENALL_REG,(volatile unsigned int*)IST_REG,
   (volatile unsigned int*)SETSWI_REG,(volatile unsigned int*)CLRSWI_REG,
   (volatile unsigned int*)ISTATE_REG,(volatile unsigned int*)CLRINT_REG,
   (volatile unsigned char*)GLB0,(volatile unsigned char*)GLB1,
   (volatile unsigned char*)GLB2,(volatile unsigned char*)GLB3,
   (volatile unsigned int*)UCSR_REG,(volatile unsigned int*)GLBRES_REG,
   (volatile unsigned int*)CCSR_REG,(volatile unsigned int*)ERROR_REG,
   (volatile unsigned int*)PCNFA_REG,(volatile unsigned int*)PCNFB_REG,
   (volatile unsigned int*)EXTAD_REG,(volatile unsigned int*)EXTAM_REG,
   (volatile unsigned int*)WHOAMI_REG,(volatile unsigned int*)WMAD_REG,
   (volatile unsigned int*)RMAD_REG,(volatile unsigned int*)WVAD_REG,
   (volatile unsigned int*)RVAD_REG,(volatile unsigned int*)CIO_PORTC,
   (volatile unsigned int*)CIO_PORTB,(volatile unsigned int*)CIO_PORTA,
   (volatile unsigned int*)CIO_CTRL 
   };
  
struct sysconsoftc {
	struct device	sc_dev;
	void		*sc_vaddr;	/* Utility I/O space */
	void		*sc_paddr;
	struct sysconreg *sc_syscon;	/* the actual registers */
   struct intrhand sc_abih;       /* `abort' switch */
   struct intrhand sc_acih;       /* `ac fial' */
   struct intrhand sc_sfih;       /* `sys fial' */
};

void sysconattach __P((struct device *, struct device *, void *));
int  sysconmatch __P((struct device *, void *, void *));
void setupiackvectors __P((void));
int  sysconabort __P((struct frame *frame));
int  sysconacfail __P((struct frame *frame));
int  sysconsysfail __P((struct frame *frame));

struct cfattach syscon_ca = {
	sizeof(struct sysconsoftc), sysconmatch, sysconattach
};

struct cfdriver syscon_cd = {
	NULL, "syscon", DV_DULL, 0
};

struct sysconreg *sys_syscon = NULL;

int
sysconmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
	struct sysconreg *syscon;

	/* Don't match if wrong cpu */
	if (cputyp != CPU_188) return (0);
   /* Uh, MVME188 better have on of these, so always match if it 
    * is a MVME188... */	
   syscon = (struct sysconreg *)(IIOV(ca->ca_paddr));
	return (1);
}

int
syscon_print(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	if (ca->ca_offset != -1)
		printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
syscon_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct sysconsoftc *sc = (struct sysconsoftc *)parent;
	struct confargs *ca = args;
	struct confargs oca;

	if (parent->dv_cfdata->cf_driver->cd_indirect) {
      printf(" indirect devices not supported\n");
      return 0;
   }

	bzero(&oca, sizeof oca);
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if ((oca.ca_offset != (void*)-1) && ISIIOVA(sc->sc_vaddr + oca.ca_offset)) {
		oca.ca_vaddr = sc->sc_vaddr + oca.ca_offset;
		oca.ca_paddr = sc->sc_paddr + oca.ca_offset;
	} else {
		oca.ca_vaddr = (void *)-1;
		oca.ca_paddr = (void *)-1;
	}
	oca.ca_bustype = BUS_SYSCON;
	oca.ca_master = (void *)sc->sc_syscon;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, syscon_print);
	return (1);
}

void
sysconattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct sysconsoftc *sc = (struct sysconsoftc *)self;
	int i;

	if (sys_syscon)
		panic("syscon already attached!");

	/*
	 * since we know ourself to land in intiobase land,
	 * we must adjust our address
	 */
	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = (void *)IIOV(sc->sc_paddr);
	sc->sc_syscon = &syscon_reg;
	sys_syscon = sc->sc_syscon;

	printf(": rev %d\n", 1);

   /* 
    * pseudo driver, abort interrupt handler
    */
   sc->sc_abih.ih_fn = sysconabort;
   sc->sc_abih.ih_arg = 0;
   sc->sc_abih.ih_ipl = IPL_ABORT;
   sc->sc_abih.ih_wantframe = 1;
   sc->sc_acih.ih_fn = sysconacfail;
   sc->sc_acih.ih_arg = 0;
   sc->sc_acih.ih_ipl = IPL_ABORT;
   sc->sc_acih.ih_wantframe = 1;
   sc->sc_sfih.ih_fn = sysconsysfail;
   sc->sc_sfih.ih_arg = 0;
   sc->sc_sfih.ih_ipl = IPL_ABORT;
   sc->sc_sfih.ih_wantframe = 1;
   
   intr_establish(SYSCV_ABRT, &sc->sc_abih);
   intr_establish(SYSCV_ACF, &sc->sc_acih);
   intr_establish(SYSCV_SYSF, &sc->sc_sfih);

	config_search(syscon_scan, self, args);
}

int
sysconintr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
	return (intr_establish(vec, ih));
}

int
sysconabort(frame)
	struct frame *frame;
{
   ISR_RESET_NMI;
   nmihand(frame);
	return (1);
}

int
sysconsysfail(frame)
	struct frame *frame;
{
   ISR_RESET_SYSFAIL;
   nmihand(frame);
	return (1);
}

int
sysconacfail(frame)
	struct frame *frame;
{
   ISR_RESET_ACFAIL;
   nmihand(frame);
	return (1);
}

