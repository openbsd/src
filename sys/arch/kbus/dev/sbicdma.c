/*	$OpenBSD: sbicdma.c,v 1.1.1.1 1997/10/14 07:25:30 gingold Exp $ */

/*
 * Copyright (c) 1996 Steve Woodford
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *  This product includes software developed by the University of
 *  California, Berkeley and its contributors.
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
 *  @(#)wdsc.c
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <dev/dmavar.h>
#include <dev/sbicreg.h>
#include <dev/sbicvar.h>
#include <dev/sbicdma.h>
#include <machine/ioasic.h>
#include <machine/autoconf.h>
#include <vm/vm.h>

void    wdscattach      __P((struct device *, struct device *, void *));
int     wdscmatch       __P((struct device *, struct cfdata *, void *));

void    wdsc_enintr     __P((struct sbic_softc *));
int     wdsc_dmago      __P((struct sbic_softc *, char *, int, int));
int	wdsc_dmanext    __P((struct sbic_softc *));
void    wdsc_dmastop    __P((struct sbic_softc *));
int     wdsc_dmaintr    __P((struct sbic_softc *));
int     wdsc_scsiintr   __P((void *));

struct scsi_adapter wdsc_scsiswitch = {
    sbic_scsicmd,
    sbic_minphys,
    0,          /* no lun support */
    0,          /* no lun support */
};

struct scsi_device wdsc_scsidev = {
    NULL,       /* use default error handler */
    NULL,       /* do not have a start functio */
    NULL,       /* have no async handler */
    NULL,       /* Use default done routine */
};

struct cfattach si_ca = {
	sizeof(struct sbic_softc), (cfmatch_t)wdscmatch, wdscattach
};

struct cfdriver si_cd = {
    NULL, "si", DV_DULL, NULL, 0 
};

/*
 * Define 'scsi_nosync = 0x00' to enable sync SCSI mode.
 * This is untested as yet, use at your own risk...
 */
u_long      scsi_nosync  = 0xff;
int         shift_nosync = 0;

/*
 * Match for SCSI devices on the onboard WD33C93 chip
 */
int
wdscmatch(pdp, cdp, auxp)
    struct device *pdp;
    struct cfdata *cdp;
    void *auxp;
{
    /*
     * Match everything
     */
    return(1);
}


/*
 * Attach the wdsc driver
 */
void
wdscattach(pdp, dp, auxp)
    struct device *pdp, *dp;
    void *auxp;
{
    struct sbic_softc   *sc = (struct sbic_softc *)dp;
/*    struct confargs *ca = auxp; */

    sc->sc_enintr  = wdsc_enintr;
    sc->sc_dmago   = wdsc_dmago;
    sc->sc_dmanext = wdsc_dmanext;
    sc->sc_dmastop = wdsc_dmastop;
    sc->sc_dmacmd  = 0;

    sc->sc_link.adapter_softc  = sc;
    sc->sc_link.adapter_target = 7;
    sc->sc_link.adapter        = &wdsc_scsiswitch;
    sc->sc_link.device         = &wdsc_scsidev;
    sc->sc_link.openings       = 2;

    printf(": target %d\n", sc->sc_link.adapter_target);


    sc->sc_sbicp = (sbic_regmap_p)((void *)ioasic + WD33C93A_OFFSET);

    /*
     * Eveything is a valid dma address.
     * 
     */
    sc->sc_dmamask = 0;

    /*
     * The onboard WD33C93 is usually clocked at 20MHz...
     * (We use 10 times this for accuracy in later calculations)
     */
    sc->sc_clkfreq = 200;

    /*
     * Initialise the hardware
     */
    sbicinit(sc);

    /*
     * Fix up the interrupts
     */
    sc->sc_sbicih.ih_fun = wdsc_scsiintr;
    sc->sc_sbicih.ih_arg = sc;
    intr_establish (INTR_IOASIC /* ca->ca_intpri */, 0, &sc->sc_sbicih);

    /*
     * Attach all scsi units on us, watching for boot device
     * (see dk_establish).
     */
    config_found(dp, &sc->sc_link, scsiprint);
}

/*
 * Enable DMA interrupts
 */
void
wdsc_enintr(dev)
    struct sbic_softc *dev;
{
    dev->sc_flags |= SBICF_INTR;
}

/*
 * Prime the hardware for a DMA transfer
 */
int
wdsc_dmago (sc, pa, count, flags)
    struct sbic_softc *sc;
    char *pa;
    int count, flags;
{
  printf ("dmago: ir=%d, count=%d, pa=%p\n",
	  ioasic->ioasic_ir, count, pa);
    /*
     * Set up the command word based on flags
     */
    if ( (flags & DMAGO_READ) == 0 )
        sc->sc_dmacmd = IOASIC_SI_CTL_WRITE;
    else
        sc->sc_dmacmd = IOASIC_SI_CTL_READ;

    sc->sc_flags |= SBICF_INTR;

    /*
     * Prime the hardware.
     * Note, it's probably not necessary to do this here, since dmanext
     * is called just prior to the actual transfer.
     */

    ioasic->ioasic_si_sar = ((u_long)pa) >> IOASIC_SI_SAR_SHIFT;
    ioasic->ioasic_si_ctl =
      ((((u_long)pa) << IOASIC_SI_CTL_SHIFT) & IOASIC_SI_CTL_AMASK)
	| sc->sc_dmacmd;
      
    return(sc->sc_tcnt);
}

/*
 * Prime the hardware for the next DMA transfer
 */
int
wdsc_dmanext(dev)
    struct sbic_softc *dev;
{
  printf ("sbic dmanext called");
  return 0;
}

/*
 * Stop DMA, and disable interrupts
 */
void
wdsc_dmastop(dev)
    struct sbic_softc *dev;
{
}

/*
 * Come here for SCSI interrupts
 */
int
wdsc_scsiintr(arg)
    void *arg;
{
    struct sbic_softc *dev = (struct sbic_softc *)arg;
    int                 found;

    if (ioasic->ioasic_ir & IOASIC_IR_SI_PO)
      {
	printf ("IOASIC page overflow, sar = %p, ctl = %p, pre = %p\n",
		ioasic->ioasic_si_sar << IOASIC_SI_SAR_SHIFT,
		ioasic->ioasic_si_ctl,
		ioasic->ioasic_si_sar_pre);
	return 1;
      }
		
    /*
     * Go handle it
     */
    found = sbicintr(dev);

    return(found);
}
