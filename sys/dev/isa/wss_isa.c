/*	$OpenBSD: wss_isa.c,v 1.3 2000/03/28 14:07:42 espie Exp $	*/
/*	$NetBSD: wss_isa.c,v 1.1 1998/01/19 22:18:24 augustss Exp $	*/

/*
 * Copyright (c) 1994 John Brezak
 * Copyright (c) 1991-1993 Regents of the University of California.
 * All rights reserved.
 *
 * MAD support:
 * Copyright (c) 1996 Lennart Augustsson
 * Based on code which is
 * Copyright (c) 1994 Hannu Savolainen
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/ad1848reg.h>
#include <dev/isa/ad1848var.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/wssvar.h>
#include <dev/isa/madreg.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (wssdebug) printf x
extern int	wssdebug;
#else
#define DPRINTF(x)
#endif

static int	wssfind __P((struct device *, struct wss_softc *, struct isa_attach_args *));

static void	madprobe __P((struct wss_softc *, int));
static void	madunmap __P((struct wss_softc *));
static int	detect_mad16 __P((struct wss_softc *, int));

int		wss_isa_probe __P((struct device *, void *, void *));
void		wss_isa_attach __P((struct device *, struct device *, void *));

struct cfattach wss_isa_ca = {
	sizeof(struct wss_softc), wss_isa_probe, wss_isa_attach
};

struct cfdriver wss_cd = {
	NULL, "wss", DV_DULL
};

/*
 * Probe for the Microsoft Sound System hardware.
 */
int
wss_isa_probe(parent, match, aux)
    struct device *parent;
#define __BROKEN_INDIRECT_CONFIG
#ifdef __BROKEN_INDIRECT_CONFIG
    void *match;
#else
    struct cfdata *match;
#endif
    void *aux;
{
    struct wss_softc probesc, *sc = &probesc;

    bzero(sc, sizeof *sc);
#ifdef __BROKEN_INDIRECT_CONFIG
    sc->sc_dev.dv_cfdata = ((struct device *)match)->dv_cfdata;
#else
    sc->sc_dev.dv_cfdata = match;
#endif
    if (wssfind(parent, sc, aux)) {
        bus_space_unmap(sc->sc_iot, sc->sc_ioh, WSS_CODEC);
        ad1848_unmap(&sc->sc_ad1848);
        madunmap(sc);
        return 1;
    } else
        /* Everything is already unmapped */
        return 0;
}

static int
wssfind(parent, sc, ia)
    struct device *parent;
    struct wss_softc *sc;
    struct isa_attach_args *ia;
{
    static u_char interrupt_bits[12] = {
	-1, -1, -1, -1, -1, 0x0, -1, 0x08, -1, 0x10, 0x18, 0x20
    };
    static u_char dma_bits[4] = {1, 2, 0, 3};
    
    sc->sc_iot = ia->ia_iot;
    if (sc->sc_dev.dv_cfdata->cf_flags & 1)
	madprobe(sc, ia->ia_iobase);
    else
	sc->mad_chip_type = MAD_NONE;

    if (!WSS_BASE_VALID(ia->ia_iobase)) {
	DPRINTF(("wss: configured iobase %x invalid\n", ia->ia_iobase));
	goto bad1;
    }

    /* Map the ports upto the AD1848 port */
    if (bus_space_map(sc->sc_iot, ia->ia_iobase, WSS_CODEC, 0, &sc->sc_ioh))
	goto bad1;

    sc->sc_ad1848.sc_iot = sc->sc_iot;

    /* Is there an ad1848 chip at (WSS iobase + WSS_CODEC)? */
    if (ad1848_mapprobe(&sc->sc_ad1848, ia->ia_iobase + WSS_CODEC) == 0)
	goto bad;
	
    ia->ia_iosize = WSS_NPORT;

    /* Setup WSS interrupt and DMA */
    if (!WSS_DRQ_VALID(ia->ia_drq)) {
	DPRINTF(("wss: configured dma chan %d invalid\n", ia->ia_drq));
	goto bad;
    }
    sc->wss_drq = ia->ia_drq;

    if (sc->wss_drq != DRQUNK && !isa_drq_isfree(parent, sc->wss_drq))
	    goto bad;

    if (!WSS_IRQ_VALID(ia->ia_irq)) {
	DPRINTF(("wss: configured interrupt %d invalid\n", ia->ia_irq));
	goto bad;
    }

    sc->wss_irq = ia->ia_irq;

    if (sc->sc_ad1848.mode <= 1)
	ia->ia_drq2 = DRQUNK;
    sc->wss_recdrq = 
	sc->sc_ad1848.mode > 1 && ia->ia_drq2 != DRQUNK ? 
	ia->ia_drq2 : ia->ia_drq;
    if (sc->wss_recdrq != sc->wss_drq && !isa_drq_isfree(parent, sc->wss_recdrq))
	goto bad;

    /* XXX recdrq */
    bus_space_write_1(sc->sc_iot, sc->sc_ioh, WSS_CONFIG,
		      (interrupt_bits[ia->ia_irq] | dma_bits[ia->ia_drq]));

    return 1;

bad:
    bus_space_unmap(sc->sc_iot, sc->sc_ioh, WSS_CODEC);
bad1:
    madunmap(sc);
    return 0;
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver .
 */
void
wss_isa_attach(parent, self, aux)
    struct device *parent, *self;
    void *aux;
{
    struct wss_softc *sc = (struct wss_softc *)self;
    struct isa_attach_args *ia = (struct isa_attach_args *)aux;
    
    if (!wssfind(parent, sc, ia)) {
        printf("%s: wssfind failed\n", sc->sc_dev.dv_xname);
        return;
    }

    sc->sc_ic = ia->ia_ic;
    sc->sc_ad1848.sc_isa = parent;

    wssattach(sc);
}

/*
 * Copyright by Hannu Savolainen 1994
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Initialization code for OPTi MAD16 compatible audio chips. Including
 *
 *      OPTi 82C928     MAD16           (replaced by C929)
 *      OAK OTI-601D    Mozart
 *      OPTi 82C929     MAD16 Pro
 *	OPTi 82C931
 */

static int
detect_mad16(sc, chip_type)
    struct wss_softc *sc;
    int chip_type;
{
    unsigned char tmp, tmp2;

    sc->mad_chip_type = chip_type;
    /*
     * Check that reading a register doesn't return bus float (0xff)
     * when the card is accessed using password. This may fail in case
     * the card is in low power mode. Normally at least the power saving mode
     * bit should be 0.
     */
    if ((tmp = mad_read(sc, MC1_PORT)) == 0xff) {
	DPRINTF(("MC1_PORT returned 0xff\n"));
	return 0;
    }

    /*
     * Now check that the gate is closed on first I/O after writing
     * the password. (This is how a MAD16 compatible card works).
     */
    if ((tmp2 = bus_space_read_1(sc->sc_iot, sc->mad_ioh, MC1_PORT)) == tmp)	{
	DPRINTF(("MC1_PORT didn't close after read (0x%02x)\n", tmp2));
	return 0;
    }

    mad_write(sc, MC1_PORT, tmp ^ 0x80);	/* Toggle a bit */

    /* Compare the bit */
    if ((tmp2 = mad_read(sc, MC1_PORT)) != (tmp ^ 0x80)) {
	mad_write(sc, MC1_PORT, tmp);	/* Restore */
	DPRINTF(("Bit revert test failed (0x%02x, 0x%02x)\n", tmp, tmp2));
	return 0;
    }

    mad_write(sc, MC1_PORT, tmp);	/* Restore */
    return 1;
}

static void
madprobe(sc, iobase)
    struct wss_softc *sc;
    int iobase;
{
    static int valid_ports[M_WSS_NPORTS] = 
        { M_WSS_PORT0, M_WSS_PORT1, M_WSS_PORT2, M_WSS_PORT3 };
    int i;

    /* Allocate bus space that the MAD chip wants */
    if (bus_space_map(sc->sc_iot, MAD_BASE, MAD_NPORT, 0, &sc->mad_ioh))
	goto bad0;
    if (bus_space_map(sc->sc_iot, MAD_REG1, MAD_LEN1, 0, &sc->mad_ioh1))
        goto bad1;
    if (bus_space_map(sc->sc_iot, MAD_REG2, MAD_LEN2, 0, &sc->mad_ioh2))
        goto bad2;
    if (bus_space_map(sc->sc_iot, MAD_REG3, MAD_LEN3, 0, &sc->mad_ioh3))
        goto bad3;

    DPRINTF(("mad: Detect using password = 0xE2\n"));
    if (!detect_mad16(sc, MAD_82C928)) {
	/* No luck. Try different model */
	DPRINTF(("mad: Detect using password = 0xE3\n"));
	if (!detect_mad16(sc, MAD_82C929))
	    goto bad;
	sc->mad_chip_type = MAD_82C929;
	DPRINTF(("mad: 82C929 detected\n"));
    } else {
	sc->mad_chip_type = MAD_82C928;
	if ((mad_read(sc, MC3_PORT) & 0x03) == 0x03) {
	    DPRINTF(("mad: Mozart detected\n"));
	    sc->mad_chip_type = MAD_OTI601D;
	} else {
	    DPRINTF(("mad: 82C928 detected?\n"));
	    sc->mad_chip_type = MAD_82C928;
	}
    }

#ifdef AUDIO_DEBUG
    if (wssdebug)
	for (i = MC1_PORT; i <= MC7_PORT; i++)
	    printf("mad: port %03x = %02x\n", i, mad_read(sc, i));
#endif

    /* Set the WSS address. */
    for (i = 0; i < M_WSS_NPORTS; i++)
	if (valid_ports[i] == iobase)
	    break;
    if (i >= M_WSS_NPORTS) {		/* Not a valid port */
	printf("mad: Bad WSS base address 0x%x\n", iobase);
	goto bad;
    }
    sc->mad_ioindex = i;
    /* enable WSS emulation at the I/O port, no joystick */
    mad_write(sc, MC1_PORT, M_WSS_PORT_SELECT(i) | MC1_JOYDISABLE);
    mad_write(sc, MC2_PORT, 0x03); /* ? */
    mad_write(sc, MC3_PORT, 0xf0); /* Disable SB */
    return;

bad:
    bus_space_unmap(sc->sc_iot, sc->mad_ioh3, MAD_LEN3);
bad3:
    bus_space_unmap(sc->sc_iot, sc->mad_ioh2, MAD_LEN2);
bad2:
    bus_space_unmap(sc->sc_iot, sc->mad_ioh1, MAD_LEN1);
bad1:
    bus_space_unmap(sc->sc_iot, sc->mad_ioh, MAD_NPORT);
bad0:
    sc->mad_chip_type = MAD_NONE;
}

static void
madunmap(sc)
    struct wss_softc *sc;
{
    if (sc->mad_chip_type == MAD_NONE)
        return;
    bus_space_unmap(sc->sc_iot, sc->mad_ioh, MAD_NPORT);
    bus_space_unmap(sc->sc_iot, sc->mad_ioh1, MAD_LEN1);
    bus_space_unmap(sc->sc_iot, sc->mad_ioh2, MAD_LEN2);
    bus_space_unmap(sc->sc_iot, sc->mad_ioh3, MAD_LEN3);
}
