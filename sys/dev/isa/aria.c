/*	$OpenBSD: aria.c,v 1.11 2004/06/13 21:49:24 niklas Exp $ */

/*
 * Copyright (c) 1995, 1996 Roland C. Dowdeswell.  All rights reserved.
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
 *      This product includes software developed by Roland C. Dowdeswell.
 * 4. The name of the authors may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * TODO:
 *  o   Test the driver on cards other than a single
 *      Prometheus Aria 16.
 *  o   Look into where aria_prometheus_kludge() belongs.
 *  o   Add some dma code.  It accomplishes its goal by
 *      direct IO at the moment.
 *  o   Look into return values on aria_set_sr(), if there is
 *      no matching rate.  (I think that this behaves in the
 *      same way as sbdsp.c)
 *  o   Different programs should be able to open the device
 *      with O_RDONLY and O_WRONLY at the same time.  But I
 *      do not see support for this in /sys/dev/audio.c, so
 *	I cannot effectively code it.
 *  o   Separate the debugging code, with a #define.
 *      Write more into aria_printsc().
 *  o   Rework the mixer interface.
 *       o   Deal with the lvls better.  We need to do better mapping
 *           between logarithmic scales and the one byte that
 *           we are passed.
 *       o   Deal better with cards that have no mixer.
 *
 * roland@imrryr.org
 * update from http://www.imrryr.org/NetBSD/hacks/aria/
 */

#include "aria.h"
#if NARIA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/mulaw.h>
#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/ariareg.h>

#define FREAD 1
#define FWRITE 2

#ifdef AUDIO_DEBUG
extern void Dprintf(const char *, ...);
#define DPRINTF(x)	if (ariadebug) Dprintf x
int	ariadebug = 0;
#else
#define DPRINTF(x)
#endif

struct aria_mixdev_info {
	u_char	num_channels;
	u_char	level[2];
	u_char	mute;
};

struct aria_mixmaster {
	u_char num_channels;
	u_char level[2];
	u_char treble[2];
	u_char bass[2];
};

struct aria_softc {
	struct	device sc_dev;		/* base device */
	struct	isadev sc_id;		/* ISA device */
	void	*sc_ih;			/* interrupt vectoring */

	u_short	sc_iobase;		/* I/O port base address */
	u_short sc_irq;			/* interrupt */
	u_short sc_drq;			/* dma chan */

	u_short	sc_open;		/* reference count of open calls */
	u_short sc_play;		/* non-paused play chans 2**chan */
	u_short sc_record;		/* non-paused record chans 2**chan */
	u_short sc_change;		/* to keep track of changes of a type */
	u_short gain[2];		/* left/right gain (play) */
	u_int	spkr_state;		/* non-null is on */

	u_long	sc_rate;		/* Sample rate for input and output */
	u_int	sc_encoding;		/* audio encoding -- ulaw/linear */
	int	sc_chans;		/* # of channels */
	int	sc_precision;		/* # bits per sample */

	u_long	sc_interrupts;		/* number of interrupts taken */
	void	(*sc_rintr)(void *);	/* record transfer completion intr handler */
	void	(*sc_pintr)(void *);	/* play transfer completion intr handler */
	void	*sc_rarg;		/* arg for sc_rintr() */
	void	*sc_parg;		/* arg for sc_pintr() */

	int	sc_blocksize;		/* literal dio block size */
	void	*sc_rdiobuffer;		/* record: where the next samples should be */
	void	*sc_pdiobuffer;		/* play:   where the next samples are */

	u_short sc_hardware;		/* bit field of hardware present */
#define ARIA_TELEPHONE	0x0001		/* has telephone input */
#define ARIA_MIXER	0x0002		/* has SC18075 digital mixer */
#define ARIA_MODEL	0x0004		/* is SC18025 (=0) or SC18026 (=1) */

	struct aria_mixdev_info aria_mix[6];
	struct aria_mixmaster ariamix_master;
	u_char	aria_mix_source;
};

struct {
	int sendcmd;
	int wmidi;
} ariaerr;



int	ariaprobe();
void	ariaattach(struct device *, struct device *, void *);
void	ariaclose(void *);
int	ariaopen(dev_t, int);
int	aria_getdev(void *, struct audio_device *);

void	aria_do_kludge(u_short, u_short, u_short, u_short, u_short);
void	aria_prometheus_kludge(struct isa_attach_args *);

int	aria_set_sr(void *, u_long);
u_long	aria_get_sr(void *);
int	aria_query_encoding(void *, struct audio_encoding *);
int	aria_set_format(void *, u_int, u_int);
int	aria_get_encoding(void *);
int	aria_get_precision(void *);
int	aria_set_channels(void *, int);
int	aria_get_channels(void *);
int	aria_round_blocksize(void *, int);
int	aria_set_out_port(void *, int);
int	aria_get_out_port(void *);
int	aria_set_in_port(void *, int);
int	aria_get_in_port(void *);
int	aria_speaker_ctl(void *, int);
int	aria_commit_settings(void *);

int	aria_start_output(void *, void *, int, void (*)(), void *);
int	aria_start_input(void *, void *, int, void (*)(), void *);

int	aria_halt_input(void *);
int	aria_halt_output(void *);
int	aria_cont(void *);

int	aria_sendcmd(u_short, u_short, int, int, int);

u_short	aria_getdspmem(u_short, u_short);
u_short	aria_putdspmem(u_short, u_short, u_short);

int	aria_intr(void *);
short	ariaversion(struct aria_softc *);

int	aria_setfd(void *, int);

void	aria_mix_write(struct aria_softc *, int, int);
int	aria_mix_read(struct aria_softc *, int);

int	aria_mixer_set_port(void *, mixer_ctrl_t *);
int	aria_mixer_get_port(void *, mixer_ctrl_t *);
int	aria_mixer_query_devinfo(void *, mixer_devinfo_t *);

/*
 * Mixer defines...
 */

struct cfattach aria_ca = {
	sizeof(struct aria_softc), ariaprobe, ariaattach
};

struct cfdriver aria_cd = {
	NULL, "aria", DV_DULL
};

struct audio_device aria_device = {
	"Aria 16(se)",
	"x",
	"aria"
};

/*
 * Define our interface to the higher level audio driver.
 */

struct audio_hw_if aria_hw_if = {
	ariaopen,
	ariaclose,
	NULL,
	aria_set_sr,
	aria_get_sr,
	aria_set_sr,
	aria_get_sr,
	aria_query_encoding,
	aria_set_format,
	aria_get_encoding,
	aria_get_precision,
	aria_set_channels,
	aria_get_channels,
	aria_round_blocksize,
	aria_set_out_port,
	aria_get_out_port,
	aria_set_in_port,
	aria_get_in_port,
	aria_commit_settings,
	mulaw_expand,
	mulaw_compress,
	aria_start_output,
	aria_start_input,
	aria_halt_input,
	aria_halt_output,
	aria_cont,
	aria_cont,
	aria_speaker_ctl,
	aria_getdev,
	aria_setfd,
	aria_mixer_set_port,
	aria_mixer_get_port,
	aria_mixer_query_devinfo,
	1,	/* full-duplex */
	0,
	NULL,
	NULL
};

/*
 * Probe / attach routines.
 */

/*
 * Probe for the aria hardware.
 */
int
ariaprobe(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct aria_softc *sc = (void *)self;
	register struct isa_attach_args *ia = aux;
        struct cfdata *cf = sc->sc_dev.dv_cfdata;
	register u_short iobase = ia->ia_iobase;
	static u_char irq_conf[11] = {
	    -1, -1, 0x01, -1, -1, 0x02, -1, 0x04, -1, 0x01, 0x08
	};
	int i,j;
	int flags = cf->cf_flags;

	if (!ARIA_BASE_VALID(ia->ia_iobase)) {
		printf("aria: configured iobase %d invalid\n", ia->ia_iobase);
		return 0;
	}
	sc->sc_iobase = iobase;
		
	if (!ARIA_IRQ_VALID(ia->ia_irq)) {
		printf("aria: configured irq %d invalid\n", ia->ia_irq);
		return 0;
	}

	sc->sc_irq = ia->ia_irq;

	if (flags & ARIAR_PROMETHEUS_KLUDGE)
		aria_prometheus_kludge(ia);

	if (aria_reset(sc) != 0) {
		DPRINTF(("aria: aria probe failed\n"));
		return 0;
	}

	ia->ia_iosize = ARIADSP_NPORT;
	return 1;
}



/*
 * I didn't call this a kludge for
 * nothing.  This is cribbed from
 * ariainit, the author of that
 * disassembled some code to discover
 * how to set up the initial values of
 * the card.  Without this, the card
 * is dead. (It will not respond to _any_
 * input at all.)
 *
 * ariainit can be found (ftp) at:
 * ftp://ftp.wi.leidenuniv.nl/pub/audio/aria/programming/contrib/ariainit.zip
 * currently.
 */

void
aria_prometheus_kludge(ia)
	register struct isa_attach_args *ia;
{
	int	i, j;
	u_short	end;
	u_short rba = ia->ia_iobase;

	DPRINTF(("aria_prometheus_kludge\n"));

/* Begin Config Sequence */

        outb(0x204, 0x4c);
        outb(0x205, 0x42);
        outb(0x206, 0x00);
        outw(0x200, 0x0f);
        outb(0x201, 0x00);
        outw(0x200, 0x02);
        outb(0x201, rba>>2);

/* These next three lines set up the iobase, and the irq; and disable the drq.  */

	aria_do_kludge(0x111, ((ia->ia_iobase-0x280)>>2)+0xA0, 0xbf, 0xa0, rba);
	aria_do_kludge(0x011, ia->ia_irq-6, 0xf8, 0x00, rba);
	aria_do_kludge(0x011, 0x00, 0xef, 0x00, rba);

/* The rest of these lines just disable everything else */

	aria_do_kludge(0x113, 0x00, 0x88, 0x00, rba);
	aria_do_kludge(0x013, 0x00, 0xf8, 0x00, rba);
	aria_do_kludge(0x013, 0x00, 0xef, 0x00, rba);
	aria_do_kludge(0x117, 0x00, 0x88, 0x00, rba);
	aria_do_kludge(0x017, 0x00, 0xff, 0x00, rba);

/* End Sequence */

	outb(0x200, 0x0f);
	end = inb(rba);
	outw(0x200, 0x0f);
	outb(0x201, end|0x80);
	inb(0x200);
/*
 * This delay is necessary for some reason,
 * at least it would crash, and sometimes not
 * probe properly if it did not exist.
 */
	delay(1000000);
}

void
aria_do_kludge(func, bits, and, or, rba)
	u_short func;
	u_short bits;
	u_short and;
	u_short or;
	u_short rba;
{
	u_int i;
	if (func & 0x100) {
		func &= ~0x100;
		if (bits) {
			outw(0x200, func-1);
			outb(0x201, bits);
		}
	} else
		or |= bits;

	outb(0x200, func);
	i = inb(rba);
	outw(0x200, func);
	outb(0x201, (i&and) | or);
}

/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
ariaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct aria_softc *sc = (struct aria_softc *)self;
	struct isa_attach_args *ia = (struct isa_attach_args *)aux;
	register u_short iobase = ia->ia_iobase;
	register u_short i;
	int err;
	
	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_AUDIO, aria_intr, sc, sc->sc_dev.dv_xname);

	i = aria_getdspmem(iobase, ARIAA_HARDWARE_A);

	sc->sc_hardware  = 0;
	sc->sc_hardware |= ((i>>13)&0x01==1)?ARIA_TELEPHONE:0;
	sc->sc_hardware |= (((i>>5)&0x07)==0x04)?ARIA_MIXER:0;
	sc->sc_hardware |= (aria_getdspmem(iobase, ARIAA_MODEL_A)==1)?ARIA_MODEL:0;

	sc->sc_open       = 0;
	sc->sc_play       = 0;
	sc->sc_record     = 0;
	sc->sc_rate       = 7875;
	sc->sc_chans      = 1;
	sc->sc_change     = 1;
	sc->sc_blocksize  = 1024;
	sc->sc_precision  = 8;
        sc->sc_rintr      = 0;
        sc->sc_rarg       = 0;
        sc->sc_pintr      = 0;
        sc->sc_parg       = 0;
	sc->gain[0]       = 127;
	sc->gain[1]       = 127;

	for (i=0; i<6; i++) {
		if (i == ARIAMIX_TEL_LVL)
			sc->aria_mix[i].num_channels = 1;
		else
			sc->aria_mix[i].num_channels = 2;
		sc->aria_mix[i].level[0] = 127;
		sc->aria_mix[i].level[1] = 127;
	}

	sc->ariamix_master.num_channels = 2;
	sc->ariamix_master.level[0] = 222;
	sc->ariamix_master.level[1] = 222;
	sc->ariamix_master.bass[0] = 127;
	sc->ariamix_master.bass[1] = 127;
	sc->ariamix_master.treble[0] = 127;
	sc->ariamix_master.treble[1] = 127;
	sc->aria_mix_source = 0;

	sc->sc_change = 1;
	aria_commit_settings(sc); /* so that my cdplayer is at the 'right' vol */

	printf(": dsp %s", (ARIA_MODEL&sc->sc_hardware)?"SC18026":"SC18025");
	if (ARIA_TELEPHONE&sc->sc_hardware)
		printf(", tel");
	if (ARIA_MIXER&sc->sc_hardware)
		printf(", SC18075 mixer");
	printf("\n");

	snprintf(aria_device.version, sizeof aria_device.version, "%s",
	    (ARIA_MODEL&sc->sc_hardware?"SC18026":"SC18025"));

	if ((err = audio_hardware_attach(&aria_hw_if, sc)) != 0)
		printf("aria: could not attach to audio pseudo-device driver (%d)\n", err);
}

/*
 * Various routines to interface to higher level audio driver
 */

int
ariaopen(dev, flags)
	dev_t dev;
	int flags;
{
	struct aria_softc *sc;
	register u_short iobase = sc->sc_iobase;
	int unit = AUDIOUNIT(dev);
	short err;

	DPRINTF(("ariaopen() called\n"));
    
	if (unit >= aria_cd.cd_ndevs)
		return ENODEV;
    
	sc = aria_cd.cd_devs[unit];

	if (!sc || sc->sc_open != 0)
		return ENXIO;
    
	sc->sc_open  = 0;
	if (flags&FREAD)
		sc->sc_open |= ARIAR_OPEN_RECORD;
	if (flags&FWRITE)
		sc->sc_open |= ARIAR_OPEN_PLAY;
	sc->sc_play  = 0;
	sc->sc_record= 0;
	sc->sc_rintr = 0;
	sc->sc_rarg  = 0;
	sc->sc_pintr = 0;
	sc->sc_parg  = 0;
	sc->sc_change= 1;

	return 0;
}

int
aria_getdev(addr, retp)
	void *addr;
	struct audio_device *retp;
{
	*retp = aria_device;
	return 0;
}

#ifdef AUDIO_DEBUG
void
aria_printsc(struct aria_softc *sc)
{
	printf("open %x dmachan %d irq %d iobase %x nintr %d\n", sc->sc_open, sc->sc_drq,
		sc->sc_irq, sc->sc_iobase, sc->sc_interrupts);
	printf("irate %d encoding %x chans %d\n", sc->sc_rate, sc->encoding,
		sc->sc_chans);
	printf("\n");
}
#endif


/*
 * Various routines to interface to higher level audio driver
 */

int
aria_set_sr(addr, sr)
	void *addr;
	u_long sr;
{
        struct aria_softc *sc = addr;

	if (sr<=9000)
		sr = 7875;
	else if (sr<=15000)
		sr = 11025;
	else if (sr<=20000)
		sr = 15750;
	else if (sr<=25000)
		sr = 22050;
	else if (sr<=40000)
		sr = 31500;
	else
		sr = 44100;

	sc->sc_rate = sr;
	return 0;
}

u_long
aria_get_sr(addr)
	void *addr;
{
        struct aria_softc *sc = addr;
	return sc->sc_rate;
}

int
aria_query_encoding(addr, fp)
    void *addr;
    struct audio_encoding *fp;
{
	register struct aria_softc *sc = addr;

	switch (fp->index) {
		case 0:
			strlcpy(fp->name, AudioEmulaw, sizeof fp->name);
			fp->format_id = AUDIO_ENCODING_ULAW;
			break;
		case 1:
			strlcpy(fp->name, AudioEpcm16, sizeof fp->name);
			fp->format_id = AUDIO_ENCODING_PCM16;
			break;
		default:
			return(EINVAL);
		/*NOTREACHED*/
	}

	return (0);
}

int
aria_set_format(addr, enc, precision)
	void *addr;
	u_int enc, prec;
{
        register struct aria_softc *sc = addr;

	DPRINTF(("aria_set_format\n"));

        switch(enc){
        case AUDIO_ENCODING_ULAW:
        case AUDIO_ENCODING_PCM16:
	case AUDIO_ENCODING_PCM8:
		break;
        default:
                return (EINVAL);
        }

	if (prec!=8 && prec!=16)
		return (EINVAL);

	if (sc->encoding!=AUDIO_ENCODING_PCM16 && prec==16)
		return (EINVAL);

	sc->sc_encoding = enc;
	sc->sc_precision = prec;
        return (0);
}

int
aria_get_encoding(addr)
	void *addr;
{
        register struct aria_softc *sc = addr;

	DPRINTF(("aria_get_encoding\n"));

        return(sc->encoding);
}

int
aria_get_precision(addr)
	void *addr;
{
        struct aria_softc *sc = addr;

	DPRINTF(("aria_get_precision\n"));

	return sc->sc_precision;
}

int
aria_set_channels(addr, chans)
	void *addr;
	int chans;
{
        struct aria_softc *sc = addr;

	DPRINTF(("aria_set_channels\n"));

	if (chans != 1 && chans != 2)
		return EINVAL;

	sc->sc_chans = chans;

	return(0);
}

int
aria_get_channels(addr)
	void *addr;
{
        struct aria_softc *sc = addr;

	DPRINTF(("aria_get_channels\n"));

	return sc->sc_chans;
}

/*
 * There is only one way to output on
 * this card.
 */
int
aria_set_out_port(addr, port)
	void *addr;
	int port;
{
	DPRINTF(("aria_set_out_port\n"));
	return(0);
}

int
aria_get_out_port(addr)
	void *addr;
{
	DPRINTF(("aria_get_out_port\n"));
	return(ARIAMIX_OUT_LVL);
}


int
aria_set_in_port(addr, port)
	void *addr;
	int port;
{
	register struct aria_softc *sc = addr;

	DPRINTF(("aria_set_in_port\n"));

	if (port<0 || port>6)
		return ENXIO;

	sc->aria_mix_source = port;
	return(0);
}

int
aria_get_in_port(addr)
	void *addr;
{
	register struct aria_softc *sc = addr;

	DPRINTF(("aria_get_in_port\n"));

	return(sc->aria_mix_source);
}

/*
 * XXX -- to be done
 *  I should probably just add a mixer thing, and
 *  access it through here.
 */
int
aria_speaker_ctl(addr, newstate)
	void *addr;
	int newstate;
{
	return(0);
}

/*
 * Store blocksize in words (what the chipset
 * understands), but report and take values
 * in bytes.
 */

int
aria_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	int i;
        struct aria_softc *sc = addr;
	for (i=64; i<1024; i*=2)
		if (blk <= i)
			break;
	sc->sc_blocksize = i;
	sc->sc_change = 1;
	return(i);
}

/*
 * This is where all of the twiddling goes on.
 */

int
aria_commit_settings(addr)
	void *addr;
{
        struct aria_softc *sc = addr;
	register u_short iobase = sc->sc_iobase;
	u_char tones[16] = { 7, 6, 5, 4, 3, 2, 1, 0, 8, 9, 10, 11, 12, 13, 14, 15 };
	u_short format;
	u_short left, right;
	u_short samp;
	u_char i;

	DPRINTF(("aria_commit_settings\n"));

	switch (sc->sc_rate) {
		case  7875: format = 0x00; samp = 0x60; break;
		case 11025: format = 0x00; samp = 0x40; break;
		case 15750: format = 0x10; samp = 0x60; break;
		case 22050: format = 0x10; samp = 0x40; break;
		case 31500: format = 0x10; samp = 0x20; break;
		case 44100: format = 0x20; samp = 0x00; break;
		default:    format = 0x00; samp = 0x40; break;
	}

	format |= (sc->sc_chans==2)?1:0;
	format |= (sc->sc_precision==16)?2:0;

	aria_sendcmd(iobase, ARIADSPC_FORMAT, format, -1, -1);
	outw(iobase+ARIADSP_CONTROL, (inw(iobase+ARIADSP_STATUS)&~0x60)|samp); /* Addition parm for sample rate */

	if (sc->sc_hardware&ARIA_MIXER) {
		for (i=0; i<6; i++) {
			u_char source;
			switch(i) {
			case ARIAMIX_MIC_LVL:     source = 0x0001; break;
			case ARIAMIX_CD_LVL:      source = 0x0002; break;
			case ARIAMIX_LINE_IN_LVL: source = 0x0008; break;
			case ARIAMIX_TEL_LVL:     source = 0x0020; break;
			case ARIAMIX_AUX_LVL:     source = 0x0010; break;
			case ARIAMIX_DAC_LVL:     source = 0x0004; break;
			default:               source = 0x0000; break;
			}
				
			if (source != 0x0000 && source != 0x0004) {
				if (sc->aria_mix[i].mute == 1)
					aria_sendcmd(iobase, ARIADSPC_INPMONMODE, source, 3, -1);
				else
					aria_sendcmd(iobase, ARIADSPC_INPMONMODE, source, (sc->aria_mix[i].num_channels==2)?0:1, -1); 

				aria_sendcmd(iobase, ARIADSPC_INPMONMODE, 0x8000|source, (sc->aria_mix[i].num_channels==2)?0:1, -1);
				aria_sendcmd(iobase, ARIADSPC_MIXERVOL, source, sc->aria_mix[i].level[0] << 7, sc->aria_mix[i].level[1] << 7);
			}

			if (sc->aria_mix_source == i) {
				aria_sendcmd(iobase, ARIADSPC_ADCSOURCE, source, -1, -1);

				if (sc->sc_open & ARIAR_OPEN_RECORD)
					aria_sendcmd(iobase, ARIADSPC_ADCCONTROL, 1, -1, -1);
				else 
					aria_sendcmd(iobase, ARIADSPC_ADCCONTROL, 0, -1, -1);
			}
		}

		if (sc->sc_chans==2) {
			aria_sendcmd(iobase, ARIADSPC_CHAN_VOL, (sc->gain[0]+sc->gain[1])/2, -1, -1);
			aria_sendcmd(iobase, ARIADSPC_CHAN_PAN, (sc->gain[0]-sc->gain[1])/4+0x40, -1, -1);
		} else {
			aria_sendcmd(iobase, ARIADSPC_CHAN_VOL, sc->gain[0], -1, -1);
			aria_sendcmd(iobase, ARIADSPC_CHAN_PAN, 0x40, -1, -1);
		}

		/* aria_sendcmd(iobase, ARIADSPC_MASMONMODE, (sc->ariamix_master.num_channels==2)?0:1 | (1<<8), -1, -1); */
		aria_sendcmd(iobase, ARIADSPC_MASMONMODE, (sc->ariamix_master.num_channels==2)?0:1, -1, -1);

		aria_sendcmd(iobase, ARIADSPC_MIXERVOL, 0x0004, sc->ariamix_master.level[0] << 7, sc->ariamix_master.level[1] << 7);

		/* Convert treb/bass from byte to soundcard style */

		left  = tones[(sc->ariamix_master.bass[0]>>4)&0x0f]<<8 | tones[(sc->ariamix_master.treble[0]>>4)&0x0f];
		right = tones[(sc->ariamix_master.bass[1]>>4)&0x0f]<<8 | tones[(sc->ariamix_master.treble[1]>>4)&0x0f];

		aria_sendcmd(iobase, ARIADSPC_TONE, left, right, -1);
	}

	if (sc->sc_change != 0)
		aria_sendcmd(iobase, ARIADSPC_BLOCKSIZE, sc->sc_blocksize/2, -1, -1);

/*
 * If we think that the card is recording or playing, start it up again here.
 * Some of the previous commands turn the channels off.
 */

	if (sc->sc_record&(1<<ARIAR_RECORD_CHAN)) {
		aria_sendcmd(iobase, ARIADSPC_START_REC, ARIAR_PLAY_CHAN, -1, -1);
		sc->sc_play |= (1<<ARIAR_RECORD_CHAN);
	}

	if (sc->sc_play&(1<<ARIAR_PLAY_CHAN)) {
		aria_sendcmd(iobase, ARIADSPC_START_PLAY, ARIAR_PLAY_CHAN, -1, -1);
		sc->sc_play |= (1<<ARIAR_PLAY_CHAN);
	}

	sc->sc_change = 0;

	return(0);
}

void
ariaclose(addr)
	void *addr;
{
        struct aria_softc *sc = addr;
	register u_int iobase = sc->sc_iobase;

	DPRINTF(("aria_close sc=0x%x\n", sc));

        sc->spkr_state = SPKR_OFF;
        sc->sc_rintr = 0;
        sc->sc_pintr = 0;
	sc->sc_rdiobuffer = 0;
	sc->sc_pdiobuffer = 0;

	if (sc->sc_play&(1<<ARIAR_PLAY_CHAN) && sc->sc_open & ARIAR_OPEN_PLAY) {
		aria_sendcmd(iobase, ARIADSPC_STOP_PLAY, ARIAR_PLAY_CHAN, -1, -1);
		sc->sc_play &= ~(1<<ARIAR_PLAY_CHAN);
	}

	if (sc->sc_record&(1<<ARIAR_RECORD_CHAN) && sc->sc_open & ARIAR_OPEN_RECORD) {
		aria_sendcmd(iobase, ARIADSPC_STOP_REC, ARIAR_RECORD_CHAN, -1, -1);
		sc->sc_record &= ~(1<<ARIAR_RECORD_CHAN);
	}

	sc->sc_open = 0;

	if (aria_reset(sc) != 0) {
		delay(500);
		aria_reset(sc);
	}
}

/*
 * Reset the hardware.
 */

int
aria_reset(sc)
	register struct aria_softc *sc;
{
	register u_short iobase = sc->sc_iobase;
	int fail=0;

	outw(iobase + ARIADSP_CONTROL, ARIAR_ARIA_SYNTH|ARIAR_SR22K|ARIAR_DSPINTWR);
	aria_putdspmem(iobase, 0x6102, 0);

	fail |= aria_sendcmd(iobase, ARIADSPC_SYSINIT, 0x0000, 0x0000, 0x0000);

	while (aria_getdspmem(iobase, ARIAA_TASK_A) != 1)
		;

	outw(iobase+ARIADSP_CONTROL, ARIAR_ARIA_SYNTH|ARIAR_SR22K|ARIAR_DSPINTWR|ARIAR_PCINTWR);
	fail |= aria_sendcmd(iobase, ARIADSPC_MODE, ARIAV_MODE_NO_SYNTH,-1,-1);

	return (fail);
}

/*
 * Lower-level routines
 */

u_short
aria_putdspmem(iobase, loc, val)
	register u_short iobase;
	register u_short loc;
	register u_short val;
{
	outw(iobase + ARIADSP_DMAADDRESS, loc);
	outw(iobase + ARIADSP_DMADATA, val);
}

u_short
aria_getdspmem(iobase, loc)
	register u_short iobase;
	register u_short loc;
{
	outw(iobase+ARIADSP_DMAADDRESS, loc);
	return inw(iobase+ARIADSP_DMADATA);
}

/*
 * aria_sendcmd()
 *  each full DSP command is unified into this
 *  function.
 */

int
aria_sendcmd(iobase, command, arg1, arg2, arg3)
	u_short iobase;
	u_short command;
	int arg1;
	int arg2;
	int arg3;
{
	int i, fail = 0;

	for (i = ARIAR_NPOLL; (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY) != 0 && i>0; i-- )
		;

	fail |= (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY)==0?0:1;
	outw(iobase + ARIADSP_WRITE, (u_short) command); 

	if (arg1 != -1) {
		for (i = ARIAR_NPOLL; (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY) != 0 && i>0; i-- )
			;

		fail |= (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY)==0?0:2;
		outw(iobase + ARIADSP_WRITE, (u_short) arg1); 
	}

	if (arg2 != -1) {
		for (i = ARIAR_NPOLL; (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY) != 0 && i>0; i-- )
			;

		fail |= (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY)==0?0:4;
		outw(iobase + ARIADSP_WRITE, (u_short) arg2); 
	}

	if (arg3 != -1) {
		for (i = ARIAR_NPOLL; (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY) != 0 && i>0; i-- )
			;

		fail |= (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY)==0?0:8;
		outw(iobase + ARIADSP_WRITE, (u_short) arg3); 
	}

	for (i = ARIAR_NPOLL; (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY) != 0 && i>0; i-- )
		;

        fail |= (inw(iobase + ARIADSP_STATUS) & ARIAR_BUSY)==0?0:16;
	outw(iobase + ARIADSP_WRITE, (u_short) ARIADSPC_TERM); 
	
#ifdef AUDIO_DEBUG
	if (fail) {
		++ariaerr.sendcmd;
		DPRINTF(("aria_sendcmd: failure=(%d) cmd=(0x%x) fail=(0x%x)\n", ariaerr.sendcmd, command, fail));
		return -1;
	}
#else
	if (fail != 0) {
		++ariaerr.sendcmd;
		return -1;
	}
#endif

	return 0;
}

int
aria_halt_input(addr)
	void *addr;
{
	register struct aria_softc *sc = addr;

	DPRINTF(("aria_halt_input\n"));

	if (sc->sc_record&(1<<0)) {
		aria_sendcmd(sc->sc_iobase, ARIADSPC_STOP_REC, 0, -1, -1);
		sc->sc_record &= ~(1<<0);
	}

	return(0);
}

int
aria_halt_output(addr)
	void *addr;
{
	register struct aria_softc *sc = addr;

	DPRINTF(("aria_halt_output\n"));

	if (sc->sc_play & (1<<1)) {
		aria_sendcmd(sc->sc_iobase, ARIADSPC_STOP_PLAY, 1, -1, -1);
		sc->sc_play &= ~(1<<1);
	}

	return(0);
}

/*
 * This is not called in dev/audio.c?
 */
int
aria_cont(addr)
	void *addr;
{
	register struct aria_softc *sc = addr;

	DPRINTF(("aria_cont\n"));

	if (!sc->sc_record&(1<<0) && (sc->sc_open&ARIAR_OPEN_RECORD)) {
		aria_sendcmd(sc->sc_iobase, ARIADSPC_START_REC,  ARIAR_RECORD_CHAN, -1, -1);
		sc->sc_record |= ~(1<<ARIAR_RECORD_CHAN);
	}

	if (!sc->sc_play&(1<<ARIAR_PLAY_CHAN) && (sc->sc_open&ARIAR_OPEN_PLAY)) {
		aria_sendcmd(sc->sc_iobase, ARIADSPC_START_PLAY, 1, -1, -1);
		sc->sc_play |= ~(1<<ARIAR_PLAY_CHAN);
	}

	return(0);
}

/*
 * Here we just set up the buffers.  If we receive
 * an interrupt without these set, it is ignored.
 */

int
aria_start_input(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr)();
	void *arg;
{
	register struct aria_softc *sc = addr;
	register int i;

	DPRINTF(("aria_start_input %d @ %x\n", cc, p));

	if (cc != sc->sc_blocksize) {
		DPRINTF(("aria_start_input reqsize %d not sc_blocksize %d\n",
			cc, sc->sc_blocksize));
		return EINVAL;
	}

	sc->sc_rarg = arg;
	sc->sc_rintr = intr;
	sc->sc_rdiobuffer = p;

	if (!(sc->sc_record&(1<<0))) {
		aria_sendcmd(sc->sc_iobase, ARIADSPC_START_REC,  0, -1, -1);
		sc->sc_record |= (1<<0);
	}

	return 0;
}

int
aria_start_output(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr)();
	void *arg;
{
	register struct aria_softc *sc = addr;
	register int i;

	DPRINTF(("aria_start_output %d @ %x\n", cc, p));

	if (cc != sc->sc_blocksize) {
		DPRINTF(("aria_start_output reqsize %d not sc_blocksize %d\n",
			cc, sc->sc_blocksize));
		return EINVAL;
	}

	sc->sc_parg = arg;
	sc->sc_pintr = intr;
	sc->sc_pdiobuffer = p;

	if (!(sc->sc_play&(1<<1))) {
		aria_sendcmd(sc->sc_iobase, ARIADSPC_START_PLAY,  1, -1, -1);
		sc->sc_play |= (1<<1);
	}

	return 0;
}

/*
 * Process an interrupt.  This should be a
 * request (from the card) to write or read
 * samples.
 */
int
aria_intr(arg)
	void *arg;
{
	register struct  aria_softc *sc = arg;
	register u_short iobase = sc->sc_iobase;
	register u_short *pdata = sc->sc_pdiobuffer;
	register u_short *rdata = sc->sc_rdiobuffer;
	u_short address;
	int i;

	if (inw(iobase) & 1 != 0x1) 
		return 0;  /* not for us */

	sc->sc_interrupts++;

	DPRINTF(("aria_intr\n"));

	if ((sc->sc_open & ARIAR_OPEN_PLAY) && (pdata!=NULL)) {
		DPRINTF(("aria_intr play=(%x)\n", pdata));
		address = 0x8000 - 2*(sc->sc_blocksize);
		address+= aria_getdspmem(iobase, ARIAA_PLAY_FIFO_A);
		outw(iobase+ARIADSP_DMAADDRESS, address);
		outsw(iobase + ARIADSP_DMADATA, pdata, sc->sc_blocksize/2);
		if (sc->sc_pintr != NULL)
			(*sc->sc_pintr)(sc->sc_parg);
	}

	if ((sc->sc_open & ARIAR_OPEN_RECORD) && (rdata!=NULL)) {
		DPRINTF(("aria_intr record=(%x)\n", rdata));
		address = 0x8000 - (sc->sc_blocksize);
		address+= aria_getdspmem(iobase, ARIAA_REC_FIFO_A);
		outw(iobase+ARIADSP_DMAADDRESS, address);
		insw(iobase + ARIADSP_DMADATA, rdata, sc->sc_blocksize/2);
		if (sc->sc_rintr != NULL)
			(*sc->sc_rintr)(sc->sc_rarg);
	}

	aria_sendcmd(iobase, ARIADSPC_TRANSCOMPLETE, -1, -1, -1);

	return 1;
}

int
aria_setfd(addr, flag)
	void *addr;
	int flag;
{
/*
 * okay return yes.  I'll assume that it will only
 * ask when the file open read/write...  Or before...
 */
	return(0);
}

int
aria_mixer_set_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
	register struct aria_softc *sc = addr;
	int error = EINVAL;

	DPRINTF(("aria_mixer_set_port\n"));

	if (!(ARIA_MIXER&sc->sc_hardware))  /* This could be done better, no mixer still has some controls. */
		return ENXIO;

	if (cp->type == AUDIO_MIXER_VALUE) {
		register mixer_level_t *mv = &cp->un.value;
		switch (cp->dev) {
		case ARIAMIX_MIC_LVL:
			if (mv->num_channels == 1 || mv->num_channels == 2) {
				sc->aria_mix[ARIAMIX_MIC_LVL].num_channels = mv->num_channels;
				sc->aria_mix[ARIAMIX_MIC_LVL].level[0] = mv->level[0];
				sc->aria_mix[ARIAMIX_MIC_LVL].level[1] = mv->level[1];
				error = 0;
			}
			break;
	
		case ARIAMIX_LINE_IN_LVL:
			if (mv->num_channels == 1 || mv->num_channels == 2) {
				sc->aria_mix[ARIAMIX_LINE_IN_LVL].num_channels = mv->num_channels;
				sc->aria_mix[ARIAMIX_LINE_IN_LVL].level[0] = mv->level[0];
				sc->aria_mix[ARIAMIX_LINE_IN_LVL].level[1] = mv->level[1];
				error = 0;
			}
			break;
	
		case ARIAMIX_CD_LVL:
			if (mv->num_channels == 1 || mv->num_channels == 2) {
				sc->aria_mix[ARIAMIX_CD_LVL].num_channels = mv->num_channels;
				sc->aria_mix[ARIAMIX_CD_LVL].level[0] = mv->level[0];
				sc->aria_mix[ARIAMIX_CD_LVL].level[1] = mv->level[1];
				error = 0;
			}
			break;
	
		case ARIAMIX_TEL_LVL:
			if (mv->num_channels == 1) {
				sc->aria_mix[ARIAMIX_TEL_LVL].num_channels = mv->num_channels;
				sc->aria_mix[ARIAMIX_TEL_LVL].level[0] = mv->level[0];
				error = 0;
			}
			break;
	
		case ARIAMIX_DAC_LVL:
			if (mv->num_channels == 1 || mv->num_channels == 2) {
				sc->aria_mix[ARIAMIX_DAC_LVL].num_channels = mv->num_channels;
				sc->aria_mix[ARIAMIX_DAC_LVL].level[0] = mv->level[0];
				sc->aria_mix[ARIAMIX_DAC_LVL].level[1] = mv->level[1];
				error = 0;
			}
			break;

		case ARIAMIX_AUX_LVL:
			if (mv->num_channels == 1 || mv->num_channels == 2) {
				sc->aria_mix[ARIAMIX_AUX_LVL].num_channels = mv->num_channels;
				sc->aria_mix[ARIAMIX_AUX_LVL].level[0] = mv->level[0];
				sc->aria_mix[ARIAMIX_AUX_LVL].level[1] = mv->level[1];
				error = 0;
			}
			break;
	
		case ARIAMIX_MASTER_LVL:
			if (mv->num_channels == 1 || mv->num_channels == 2) {
				sc->ariamix_master.num_channels = mv->num_channels;
				sc->ariamix_master.level[0] = mv->level[0];
				sc->ariamix_master.level[1] = mv->level[1];
				error = 0;
			}
			break;
	
		case ARIAMIX_MASTER_TREBLE:
			if (mv->num_channels == 2) {
				sc->ariamix_master.treble[0] = (mv->level[0]==0)?1:mv->level[0];
				sc->ariamix_master.treble[1] = (mv->level[1]==0)?1:mv->level[1];
				error = 0;
			}
			break;
		case ARIAMIX_MASTER_BASS:
			if (mv->num_channels == 2) {
				sc->ariamix_master.bass[0] = (mv->level[0]==0)?1:mv->level[0];
				sc->ariamix_master.bass[1] = (mv->level[1]==0)?1:mv->level[1];
				error = 0;
			}
			break;
		case ARIAMIX_OUT_LVL:
			if (mv->num_channels == 1 || mv->num_channels == 2) {
				sc->gain[0] = mv->level[0];
				sc->gain[1] = mv->level[1];
				error = 0;
			}
			break;
		default:
		}
	}

	if (cp->type == AUDIO_MIXER_ENUM)
		switch(cp->dev) {
		case ARIAMIX_RECORD_SOURCE:
			if (cp->un.ord>=0 && cp->un.ord<=6) {
				sc->aria_mix_source = cp->un.ord;
				error = 0;
			}
			break;

		case ARIAMIX_MIC_MUTE:
			if (cp->un.ord == 0 || cp->un.ord == 1) {
				sc->aria_mix[ARIAMIX_MIC_LVL].mute = cp->un.ord;
				error = 0;
			}
			break;

		case ARIAMIX_LINE_IN_MUTE:
			if (cp->un.ord == 0 || cp->un.ord == 1) {
				sc->aria_mix[ARIAMIX_LINE_IN_LVL].mute = cp->un.ord;
				error = 0;
			}
			break;

		case ARIAMIX_CD_MUTE:
			if (cp->un.ord == 0 || cp->un.ord == 1) {
				sc->aria_mix[ARIAMIX_CD_LVL].mute = cp->un.ord;
				error = 0;
			}
			break;

		case ARIAMIX_DAC_MUTE:
			if (cp->un.ord == 0 || cp->un.ord == 1) {
				sc->aria_mix[ARIAMIX_DAC_LVL].mute = cp->un.ord;
				error = 0;
			}
			break;

		case ARIAMIX_AUX_MUTE:
			if (cp->un.ord == 0 || cp->un.ord == 1) {
				sc->aria_mix[ARIAMIX_AUX_LVL].mute = cp->un.ord;
				error = 0;
			}
			break;

		case ARIAMIX_TEL_MUTE:
			if (cp->un.ord == 0 || cp->un.ord == 1) {
				sc->aria_mix[ARIAMIX_TEL_LVL].mute = cp->un.ord;
				error = 0;
			}
			break;

		default:
			return ENXIO;
			/* NOTREACHED */
		}

	return(error);
}

int
aria_mixer_get_port(addr, cp)
    void *addr;
    mixer_ctrl_t *cp;
{
	register struct aria_softc *sc = addr;
	int error = EINVAL;

	DPRINTF(("aria_mixer_get_port\n"));

	if (!(ARIA_MIXER&sc->sc_hardware))  /* This could be done better, no mixer still has some controls. */
		return ENXIO;

	switch (cp->dev) {
	case ARIAMIX_MIC_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = sc->aria_mix[ARIAMIX_MIC_LVL].num_channels;
			cp->un.value.level[0] = sc->aria_mix[ARIAMIX_MIC_LVL].level[0];
			cp->un.value.level[1] = sc->aria_mix[ARIAMIX_MIC_LVL].level[1];
			error = 0;
		}
		break;
			
	case ARIAMIX_LINE_IN_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = sc->aria_mix[ARIAMIX_LINE_IN_LVL].num_channels;
			cp->un.value.level[0] = sc->aria_mix[ARIAMIX_LINE_IN_LVL].level[0];
			cp->un.value.level[1] = sc->aria_mix[ARIAMIX_LINE_IN_LVL].level[1];
			error = 0;
		}
		break;

	case ARIAMIX_CD_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = sc->aria_mix[ARIAMIX_CD_LVL].num_channels;
			cp->un.value.level[0] = sc->aria_mix[ARIAMIX_CD_LVL].level[0];
			cp->un.value.level[1] = sc->aria_mix[ARIAMIX_CD_LVL].level[1];
			error = 0;
		}
		break;

	case ARIAMIX_TEL_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = sc->aria_mix[ARIAMIX_TEL_LVL].num_channels;
			cp->un.value.level[0] = sc->aria_mix[ARIAMIX_TEL_LVL].level[0];
			error = 0;
		}
		break;
	case ARIAMIX_DAC_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = sc->aria_mix[ARIAMIX_DAC_LVL].num_channels;
			cp->un.value.level[0] = sc->aria_mix[ARIAMIX_DAC_LVL].level[0];
			cp->un.value.level[1] = sc->aria_mix[ARIAMIX_DAC_LVL].level[1];
			error = 0;
		}
		break;

	case ARIAMIX_AUX_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = sc->aria_mix[ARIAMIX_AUX_LVL].num_channels;
			cp->un.value.level[0] = sc->aria_mix[ARIAMIX_AUX_LVL].level[0];
			cp->un.value.level[1] = sc->aria_mix[ARIAMIX_AUX_LVL].level[1];
			error = 0;
		}
		break;

	case ARIAMIX_MIC_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = sc->aria_mix[ARIAMIX_MIC_LVL].mute;
			error = 0;
		}
		break;

	case ARIAMIX_LINE_IN_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = sc->aria_mix[ARIAMIX_LINE_IN_LVL].mute;
			error = 0;
		}
		break;

	case ARIAMIX_CD_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = sc->aria_mix[ARIAMIX_CD_LVL].mute;
			error = 0;
		}
		break;

	case ARIAMIX_DAC_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = sc->aria_mix[ARIAMIX_DAC_LVL].mute;
			error = 0;
		}
		break;

	case ARIAMIX_AUX_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = sc->aria_mix[ARIAMIX_AUX_LVL].mute;
			error = 0;
		}
		break;

	case ARIAMIX_TEL_MUTE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = sc->aria_mix[ARIAMIX_TEL_LVL].mute;
			error = 0;
		}
		break;

	case ARIAMIX_MASTER_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = sc->ariamix_master.num_channels;
			cp->un.value.level[0] = sc->ariamix_master.level[0];
			cp->un.value.level[1] = sc->ariamix_master.level[1];
			error = 0;
		}
		break;

	case ARIAMIX_MASTER_TREBLE:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->ariamix_master.treble[0];
			cp->un.value.level[1] = sc->ariamix_master.treble[1];
			error = 0;
		}
		break;

	case ARIAMIX_MASTER_BASS:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->ariamix_master.bass[0];
			cp->un.value.level[1] = sc->ariamix_master.bass[1];
			error = 0;
		}
		break;

	case ARIAMIX_OUT_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = sc->sc_chans;
			cp->un.value.level[0] = sc->gain[0];
			cp->un.value.level[1] = sc->gain[1];
			error = 0;
		}
		break;
	case ARIAMIX_RECORD_SOURCE:
		if (cp->type == AUDIO_MIXER_ENUM) {
			cp->un.ord = sc->aria_mix_source;
			error = 0;
		}
		break;

	default:
		return ENXIO;
		/* NOT REACHED */
	}

	return(error);
}

int
aria_mixer_query_devinfo(addr, dip)
	   void *addr;
	   register mixer_devinfo_t *dip;
{

	register struct aria_softc *sc = addr;

	DPRINTF(("aria_mixer_query_devinfo\n"));

	if (!(ARIA_MIXER&sc->sc_hardware))  /* This could be done better, no mixer still has some controls. */
		return ENXIO;

	dip->prev = dip->next = AUDIO_MIXER_LAST;

	switch(dip->index) {
	case ARIAMIX_MIC_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_INPUT_CLASS;
		dip->next = ARIAMIX_MIC_MUTE;
		strlcpy(dip->label.name, AudioNmicrophone,
		    sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_LINE_IN_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_INPUT_CLASS;
		dip->next = ARIAMIX_LINE_IN_MUTE;
		strlcpy(dip->label.name, AudioNline, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_CD_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_INPUT_CLASS;
		dip->next = ARIAMIX_CD_MUTE;
		strlcpy(dip->label.name, AudioNcd, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_TEL_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_INPUT_CLASS;
		dip->next = ARIAMIX_TEL_MUTE;
		strlcpy(dip->label.name, "telephone", sizeof dip->label.name);
		dip->un.v.num_channels = 1;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_DAC_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_INPUT_CLASS;
		dip->next = ARIAMIX_DAC_MUTE;
		strlcpy(dip->label.name, AudioNdac, sizeof dip->label.name);
		dip->un.v.num_channels = 1;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_AUX_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_INPUT_CLASS;
		dip->next = ARIAMIX_AUX_MUTE;
		strlcpy(dip->label.name, AudioNoutput, sizeof dip->label.name);
		dip->un.v.num_channels = 1;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_MIC_MUTE:
		dip->prev = ARIAMIX_MIC_LVL;
		goto mode;

	case ARIAMIX_LINE_IN_MUTE:
		dip->prev = ARIAMIX_LINE_IN_LVL;
		goto mode;
	
	case ARIAMIX_CD_MUTE:
		dip->prev = ARIAMIX_CD_LVL;
		goto mode;
	
	case ARIAMIX_DAC_MUTE:
		dip->prev = ARIAMIX_DAC_LVL;
		goto mode;

	case ARIAMIX_AUX_MUTE:
		dip->prev = ARIAMIX_AUX_LVL;
		goto mode;

	case ARIAMIX_TEL_MUTE:
		dip->prev = ARIAMIX_TEL_LVL;
		goto mode;

mode:
		dip->mixer_class = ARIAMIX_INPUT_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		strlcpy(dip->label.name, AudioNmute, sizeof dip->label.name);
		dip->un.e.num_mem = 2;
		strlcpy(dip->un.e.member[0].label.name, AudioNoff,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = 0;
		strlcpy(dip->un.e.member[1].label.name, AudioNon,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[1].ord = 1;
		break;

	case ARIAMIX_MASTER_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_OUTPUT_CLASS;
		dip->next = ARIAMIX_MASTER_TREBLE;
		strlcpy(dip->label.name, AudioNvolume, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_MASTER_TREBLE:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_OUTPUT_CLASS;
		dip->prev = ARIAMIX_MASTER_LVL;
		dip->next = ARIAMIX_MASTER_BASS;
		strlcpy(dip->label.name, AudioNmaster, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNtreble,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_MASTER_BASS:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_OUTPUT_CLASS;
		dip->prev = ARIAMIX_MASTER_TREBLE;
		strlcpy(dip->label.name, AudioNmaster, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNbass,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_OUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = ARIAMIX_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioNoutput, sizeof dip->label.name);
		dip->un.v.num_channels = 2;
		strlcpy(dip->un.v.units.name, AudioNvolume,
		    sizeof dip->un.v.units.name);
		break;

	case ARIAMIX_RECORD_SOURCE:
		dip->mixer_class = ARIAMIX_RECORD_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		strlcpy(dip->label.name, AudioNsource, sizeof dip->label.name);
		dip->un.e.num_mem = 6;
		strlcpy(dip->un.e.member[0].label.name, AudioNoutput,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[0].ord = ARIAMIX_AUX_LVL;
		strlcpy(dip->un.e.member[1].label.name, AudioNmicrophone,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[1].ord = ARIAMIX_MIC_LVL;
		strlcpy(dip->un.e.member[2].label.name, AudioNdac,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[2].ord = ARIAMIX_DAC_LVL;
		strlcpy(dip->un.e.member[3].label.name, AudioNline,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[3].ord = ARIAMIX_LINE_IN_LVL;
		strlcpy(dip->un.e.member[3].label.name, AudioNcd,
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[4].ord = ARIAMIX_CD_LVL;
		strlcpy(dip->un.e.member[3].label.name, "telephone",
		    sizeof dip->un.e.member[0].label.name);
		dip->un.e.member[5].ord = ARIAMIX_TEL_LVL;
		break;

	case ARIAMIX_INPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = ARIAMIX_INPUT_CLASS;
		strlcpy(dip->label.name, AudioCInputs, sizeof dip->label.name);
		break;

	case ARIAMIX_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = ARIAMIX_OUTPUT_CLASS;
		strlcpy(dip->label.name, AudioCOutputs,
		    sizeof dip->label.name);
		break;

	case ARIAMIX_RECORD_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = ARIAMIX_RECORD_CLASS;
		strlcpy(dip->label.name, AudioCRecord, sizeof dip->label.name);
		break;

	case ARIAMIX_EQ_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = ARIAMIX_EQ_CLASS;
		strlcpy(dip->label.name, AudioCEqualization,
		    sizeof dip->label.name);
		break;

	default:
		return ENXIO;
		/*NOTREACHED*/
	}
	return 0;
}

#endif /* NARIA */
