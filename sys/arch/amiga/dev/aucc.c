/*	$OpenBSD: aucc.c,v 1.4 1998/11/03 21:22:34 downsj Exp $	*/
/*	$NetBSD: aucc.c,v 1.22 1998/01/12 10:39:10 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Stephan Thesing
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
 *      This product includes software developed by Stephan Thesing.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include "aucc.h"
#if NAUCC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/audioio.h>

#include <dev/audio_if.h>

#include <machine/cpu.h>

#include <amiga/amiga/cc.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/device.h>

#include <amiga/dev/auccvar.h>

#ifdef LEV6_DEFER
#define AUCC_MAXINT 3
#define AUCC_ALLINTF (INTF_AUD0|INTF_AUD1|INTF_AUD2)
#else
#define AUCC_MAXINT 4
#define AUCC_ALLINTF (INTF_AUD0|INTF_AUD1|INTF_AUD2|INTF_AUD3)
#endif
/* this unconditionally; we may use AUD3 as slave channel with LEV6_DEFER */
#define AUCC_ALLDMAF (DMAF_AUD0|DMAF_AUD1|DMAF_AUD2|DMAF_AUD3)

#ifdef AUDIO_DEBUG
/*extern printf __P((const char *,...));*/
int     auccdebug = 1;
#define DPRINTF(x)      if (auccdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifdef splaudio
#undef splaudio
#endif

#define splaudio() spl4();

/* clock frequency.. */
extern int eclockfreq; 

/* hw audio ch */
extern struct audio_channel channel[4];

/*
 * Software state.
 */
struct aucc_softc {
	struct	device sc_dev;		/* base device */

	int	sc_open;		/* single use device */
	aucc_data_t sc_channel[4];	/* per channel freq, ... */
	u_int	sc_encoding;		/* encoding AUDIO_ENCODING_.*/
	int	sc_channels;		/* # of channels used */

	int	sc_intrcnt;		/* interrupt count */
	int	sc_channelmask;  	/* which channels are used ? */
};

/* interrupt interfaces */
void aucc_inthdl __P((int)); 

/* forward declarations */
int	init_aucc __P((struct aucc_softc *));
u_int	freqtoper  __P((u_int));
u_int	pertofreq  __P((u_int));

/* autoconfiguration driver */
void	auccattach __P((struct device *, struct device *, void *));
int	auccmatch __P((struct device *, void *, void *));

struct cfattach aucc_ca = {
	sizeof(struct aucc_softc),
	auccmatch,
	auccattach
};

struct	cfdriver aucc_cd = {
	NULL, "aucc", DV_DULL, NULL, 0
};

struct audio_device aucc_device = {
	"Amiga-audio",
	"x",
	"aucc"
};

struct aucc_softc *aucc = NULL;

unsigned char ulaw_to_lin[] = {
	0x82, 0x86, 0x8a, 0x8e, 0x92, 0x96, 0x9a, 0x9e,
	0xa2, 0xa6, 0xaa, 0xae, 0xb2, 0xb6, 0xba, 0xbe,
	0xc1, 0xc3, 0xc5, 0xc7, 0xc9, 0xcb, 0xcd, 0xcf,
	0xd1, 0xd3, 0xd5, 0xd7, 0xd9, 0xdb, 0xdd, 0xdf,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8,
	0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0,
	0xf0, 0xf1, 0xf1, 0xf2, 0xf2, 0xf3, 0xf3, 0xf4,
	0xf4, 0xf5, 0xf5, 0xf6, 0xf6, 0xf7, 0xf7, 0xf8,
	0xf8, 0xf8, 0xf9, 0xf9, 0xf9, 0xf9, 0xfa, 0xfa,
	0xfa, 0xfa, 0xfb, 0xfb, 0xfb, 0xfb, 0xfc, 0xfc,
	0xfc, 0xfc, 0xfc, 0xfc, 0xfd, 0xfd, 0xfd, 0xfd,
	0xfd, 0xfd, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe, 0xfe,
	0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
	0x7d, 0x79, 0x75, 0x71, 0x6d, 0x69, 0x65, 0x61,
	0x5d, 0x59, 0x55, 0x51, 0x4d, 0x49, 0x45, 0x41,
	0x3e, 0x3c, 0x3a, 0x38, 0x36, 0x34, 0x32, 0x30,
	0x2e, 0x2c, 0x2a, 0x28, 0x26, 0x24, 0x22, 0x20,
	0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x19, 0x18, 0x17,
	0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0f,
	0x0f, 0x0e, 0x0e, 0x0d, 0x0d, 0x0c, 0x0c, 0x0b,
	0x0b, 0x0a, 0x0a, 0x09, 0x09, 0x08, 0x08, 0x07,
	0x07, 0x07, 0x06, 0x06, 0x06, 0x06, 0x05, 0x05,
	0x05, 0x05, 0x04, 0x04, 0x04, 0x04, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02,
	0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * Define our interface to the higher level audio driver.
 */
int	aucc_open __P((void *, int));
void	aucc_close __P((void *));
int	aucc_set_out_sr __P((void *, u_long));
int	aucc_query_encoding __P((void *, struct audio_encoding *));
int	aucc_round_blocksize __P((void *, int));
int	aucc_commit_settings __P((void *));
int	aucc_start_output __P((void *, void *, int, void (*)(void *),
	    void *));
int	aucc_start_input __P((void *, void *, int, void (*)(void *),
	    void *));
int	aucc_halt_output __P((void *));
int	aucc_halt_input __P((void *));
int	aucc_getdev __P((void *, struct audio_device *));
int	aucc_set_port __P((void *, mixer_ctrl_t *));
int	aucc_get_port __P((void *, mixer_ctrl_t *));
int	aucc_query_devinfo __P((void *, mixer_devinfo_t *));
void	aucc_encode __P((int, int, int, u_char *, u_short **));
int	aucc_set_params __P((void *, int, int,
	    struct audio_params *, struct audio_params *));
int	aucc_get_props __P((void *));

struct audio_hw_if sa_hw_if = {
	aucc_open,
	aucc_close,
	NULL,
	aucc_query_encoding,
	aucc_set_params,
	aucc_round_blocksize,
	aucc_commit_settings,
	NULL,
	NULL,
	aucc_start_output,
	aucc_start_input,
	aucc_halt_output,
	aucc_halt_input,
	NULL,
	aucc_getdev,
	NULL,
	aucc_set_port,
	aucc_get_port,
	aucc_query_devinfo,
	NULL,
	NULL,
	NULL,
	NULL,
	aucc_get_props,
	NULL,
	NULL
};

/* autoconfig routines */

int
auccmatch(pdp, match, aux)
	struct device *pdp;
	void *match;
	void *aux;
{
	struct cfdata *cfp = match;

	if (matchname((char *)aux, "aucc") &&
#ifdef DRACO
	    !is_draco() &&
#endif
	    (cfp->cf_unit == 0))
		return (1);

	return (0);
}

/*
 * Audio chip found.
 */
void
auccattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct aucc_softc *sc = (struct aucc_softc *)self;
	int i;

	printf("\n");

	if((i = init_aucc(sc)) != 0) {
		printf("audio: no chipmem\n");
		return;
	}

	audio_attach_mi(&sa_hw_if, 0, sc, &sc->sc_dev);
}

int
init_aucc(sc)
	struct aucc_softc *sc;
{
	int i, err=0;

	/* init values per channel */
 	for (i = 0; i < 4; i++) {
		sc->sc_channel[i].nd_freq = 8000;
		sc->sc_channel[i].nd_per = freqtoper(8000);
		sc->sc_channel[i].nd_busy = 0;
		sc->sc_channel[i].nd_dma = alloc_chipmem(AUDIO_BUF_SIZE * 2);
		if (sc->sc_channel[i].nd_dma == NULL)
			err = 1;
	 	sc->sc_channel[i].nd_dmalength = 0;
		sc->sc_channel[i].nd_volume = 64; 
		sc->sc_channel[i].nd_intr = NULL;
		sc->sc_channel[i].nd_intrdata = NULL;
		sc->sc_channel[i].nd_doublebuf = 0;
		DPRINTF(("dma buffer for channel %d is %p\n", i,
		    sc->sc_channel[i].nd_dma));
		        
	}

	if (err) {
		for(i = 0; i < 4; i++)
			if (sc->sc_channel[i].nd_dma)
				free_chipmem(sc->sc_channel[i].nd_dma);
	}

	sc->sc_channels = 1;
	sc->sc_channelmask = 0xf;

	/* clear interrupts and dma: */
	custom.intena = AUCC_ALLINTF;
	custom.dmacon = AUCC_ALLDMAF;

	sc->sc_encoding = AUDIO_ENCODING_ULAW;	

	return (err);

}

int
aucc_open(addr, flags)
	void *addr;
	int flags;
{
	struct aucc_softc *sc = addr;
	int i;

	DPRINTF(("sa_open: unit %p\n",sc));

	if (sc->sc_open)
		return (EBUSY);
	sc->sc_open = 1;
	for (i = 0; i < AUCC_MAXINT; i++) {
		sc->sc_channel[i].nd_intr = NULL;
		sc->sc_channel[i].nd_intrdata = NULL;
	}
	aucc = sc;
	sc->sc_channelmask = 0xf;

	DPRINTF(("saopen: ok -> sc=0x%p\n",sc));

	return (0);
}

void
aucc_close(addr)
	void *addr;
{
	struct aucc_softc *sc = addr;

	DPRINTF(("sa_close: sc=0x%p\n", sc));
	/*
	 * halt i/o, clear open flag, and done.
	 */
	aucc_halt_output(sc);
	sc->sc_open = 0;

	DPRINTF(("sa_close: closed.\n"));
}

int
aucc_set_out_sr(addr, sr)
	void *addr;
	u_long sr;
{
	struct aucc_softc *sc = addr;
	u_long per;
	int i;

	per = freqtoper(sr);
	if (per > 0xffff)
		return (EINVAL);
	sr = pertofreq(per);

	for (i = 0; i < 4; i++) {
		sc->sc_channel[i].nd_freq = sr;
		sc->sc_channel[i].nd_per = per;
	}

	return (0);
}

int
aucc_query_encoding(addr, fp)
	void *addr;
	struct audio_encoding *fp;
{
	switch (fp->index) {	
	case 0:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
		
	case 2:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;

	default:
		return (EINVAL);
		/*NOTREACHED*/
	}
	return (0);
}

int
aucc_set_params(addr, setmode, usemode, p, r)
	void *addr;
	int setmode, usemode;
	struct audio_params *p, *r;
{
	struct aucc_softc *sc = addr;

#if 0
	if (setmode & AUMODE_RECORD)
		return (ENXIO);
#endif

#ifdef AUCCDEBUG
	printf("aucc_set_params(setmode 0x%x, usemode 0x%x, enc %d, bits %d,"
	    "chn %d, sr %ld)\n",
	    setmode, usemode, p->encoding, p->precision, p->channels,
	    p->sample_rate);
#endif

	switch (p->encoding) {
	case AUDIO_ENCODING_ULAW:
	case AUDIO_ENCODING_SLINEAR:
	case AUDIO_ENCODING_SLINEAR_BE:
	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_ULINEAR_BE:
	case AUDIO_ENCODING_ULINEAR_LE:
		break;		

	default:
		return EINVAL;
		/* NOTREADCHED */
	}

	if (p->precision != 8)
		return (EINVAL);

	if ((p->channels < 1) || (p->channels > 4))
		return (EINVAL);

	sc->sc_channels = p->channels;
	sc->sc_encoding = p->encoding;

	return (aucc_set_out_sr(addr, p->sample_rate));
}

int
aucc_round_blocksize(addr, blk)
	void *addr;
	int blk;
{
	/* round up to even size */
	return (blk > AUDIO_BUF_SIZE ? AUDIO_BUF_SIZE : blk);
}

int
aucc_commit_settings(addr)
	void *addr;
{
	struct aucc_softc *sc = addr;
	int i;

	DPRINTF(("sa_commit.\n"));

	for (i = 0; i < 4; i++) {
		custom.aud[i].vol = sc->sc_channel[i].nd_volume;
		custom.aud[i].per = sc->sc_channel[i].nd_per;
	}

	DPRINTF(("commit done\n"));

	return (0);
}

static int masks[4] = {1, 3, 7, 15}; /* masks for n first channels */
static int masks2[4] = {1, 2, 4, 8};

int
aucc_start_output(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr) __P((void *));
	void *arg;
{
	struct aucc_softc *sc;
	int mask;
	int i, j, k;
	u_short *dmap[4];
	u_char *pp;

	sc = addr;
	mask = sc->sc_channelmask;

	dmap[0] = dmap[1] = dmap[2] = dmap[3] = NULL;

	DPRINTF(("sa_start_output: cc=%d %p (%p)\n", cc, intr, arg));

	if (sc->sc_channels > 1)
		mask &= masks[sc->sc_channels - 1];
		/* we use first sc_channels channels */
	if (mask == 0) /* active and used channels are disjoint */
		return (EINVAL);

	for (i = 0; i < 4; i++) { /* channels available ? */
		if ((masks2[i] & mask) && (sc->sc_channel[i].nd_busy))
			return (EBUSY); /* channel is busy */
		if (channel[i].isaudio == -1)
			return (EBUSY); /* system uses them */
	}

	/* enable interrupt on 1st channel */
	for (i= j = 0; i < AUCC_MAXINT; i++) {
		if (masks2[i] & mask) {
			DPRINTF(("first channel is %d\n", i));
			j = i;
			sc->sc_channel[i].nd_intr = intr;
			sc->sc_channel[i].nd_intrdata = arg;
			break;
		}
	}

	DPRINTF(("dmap is %p %p %p %p, mask=0x%x\n", dmap[0], dmap[1],
	    dmap[2], dmap[3], mask));

	/*
	 * disable ints, dma for channels, until all parameters set
	 * XXX dont disable DMA! custom.dmacon = mask;
	 */
	custom.intreq = mask << INTB_AUD0;
	custom.intena = mask << INTB_AUD0;

	/* copy data to dma buffer */
		
 	pp = (u_char *)p;

	if (sc->sc_channels == 1) {
		dmap[0] = dmap[1] = dmap[2] = dmap[3] =
		    sc->sc_channel[j].nd_dma;
	} else {
		for (k = 0; k < 4; k++) {
			if (masks2[k+j] & mask)
				dmap[k] = sc->sc_channel[k+j].nd_dma;
		}
	}

	sc->sc_channel[j].nd_doublebuf ^= 1;
	if (sc->sc_channel[j].nd_doublebuf) {
		dmap[0] += AUDIO_BUF_SIZE / sizeof (u_short);
		dmap[1] += AUDIO_BUF_SIZE / sizeof (u_short);
		dmap[2] += AUDIO_BUF_SIZE / sizeof (u_short);
		dmap[3] += AUDIO_BUF_SIZE / sizeof (u_short);
	}

	aucc_encode(sc->sc_encoding, sc->sc_channels, cc, pp, dmap);

	/* dma buffers: we use same buffer 4 all channels */
	/* write dma location and length */
	for (i = k = 0; i < 4; i++) {
		if (masks2[i] & mask) {
			DPRINTF(("turning channel %d on\n", i));
			/*  sc->sc_channel[i].nd_busy = 1;*/
			channel[i].isaudio = 1;
			channel[i].play_count = 1;
			channel[i].handler = NULL;
			custom.aud[i].per = sc->sc_channel[i].nd_per;
			custom.aud[i].vol = sc->sc_channel[i].nd_volume;
			custom.aud[i].lc = PREP_DMA_MEM(dmap[k++]);
			custom.aud[i].len = cc / (sc->sc_channels * 2);
			sc->sc_channel[i].nd_mask = mask;
			DPRINTF(("per is %d, vol is %d, len is %d\n",\
			    sc->sc_channel[i].nd_per,
			    sc->sc_channel[i].nd_volume, cc >> 1));
			
		}
	}

	channel[j].handler = aucc_inthdl;

	/* enable ints */
	custom.intena = INTF_SETCLR|INTF_INTEN|(masks2[j] << INTB_AUD0);

	DPRINTF(("enabled ints: 0x%x\n", (masks2[j] << INTB_AUD0)));

	/* enable dma */
	custom.dmacon = DMAF_SETCLR|DMAF_MASTER|mask;

	DPRINTF(("enabled dma, mask=0x%x\n",mask));

	return (0);
}

/* ARGSUSED */
int
aucc_start_input(addr, p, cc, intr, arg)
	void *addr;
	void *p;
	int cc;
	void (*intr) __P((void *));
	void *arg;
{
	return (ENXIO); /* no input */
}

int
aucc_halt_output(addr)
	void *addr;
{
	struct aucc_softc *sc = addr;
	int i;

	/* XXX only halt, if input is also halted ?? */
	/* stop dma, etc */
	custom.intena = AUCC_ALLINTF;
	custom.dmacon = AUCC_ALLDMAF;
	/* mark every busy unit idle */
	for (i = 0; i < 4; i++) {
		sc->sc_channel[i].nd_busy = sc->sc_channel[i].nd_mask = 0;
		channel[i].isaudio = 0;
		channel[i].play_count = 0;
	}

	return (0);
}

int
aucc_halt_input(addr)
	void *addr;
{
	/* no input */

	return (ENXIO);
}

int
aucc_getdev(addr, retp)
        void *addr;
        struct audio_device *retp;
{
        *retp = aucc_device;
        return (0);
}

int
aucc_set_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct aucc_softc *sc = addr;
	int i,j;

	DPRINTF(("aucc_set_port: port=%d", cp->dev));

	switch (cp->type) {
	case AUDIO_MIXER_SET:
		if (cp->dev != AUCC_CHANNELS)
			return (EINVAL);
		i = cp->un.mask;
		if ((i < 1) || (i > 15))
			return (EINVAL);
		sc->sc_channelmask = i;
		break;

	case AUDIO_MIXER_VALUE:
		i = cp->un.value.num_channels;
		if ((i < 1) || (i > 4))
			return (EINVAL);

#ifdef __XXXwhatsthat
		if (cp->dev != AUCC_VOLUME)
			return (EINVAL);
#endif

		/* set volume for channel 0..i-1 */

		/* evil workaround for xanim bug, IMO */
		if ((sc->sc_channels == 1) && (i == 2)) {
			sc->sc_channel[0].nd_volume = 
			    sc->sc_channel[3].nd_volume = 
			    cp->un.value.level[0] >> 2;
			sc->sc_channel[1].nd_volume = 
			    sc->sc_channel[2].nd_volume = 
			    cp->un.value.level[1] >> 2;
		} else if (i > 1) {
			for (j = 0; j < i; j++)
	 			sc->sc_channel[j].nd_volume =
				    cp->un.value.level[j] >> 2;
		} else if (sc->sc_channels > 1)
			for (j = 0; j < sc->sc_channels; j++)
	 			sc->sc_channel[j].nd_volume =
				    cp->un.value.level[0] >> 2;
		else
			for (j = 0; j < 4; j++)
	 			sc->sc_channel[j].nd_volume =
				    cp->un.value.level[0] >> 2;
		break;

	default:
		return (EINVAL);
		break;
	}
	return (0);
}

int
aucc_get_port(addr, cp)
	void *addr;
	mixer_ctrl_t *cp;
{
	struct aucc_softc *sc = addr;
	int i, j;

	DPRINTF(("aucc_get_port: port=%d", cp->dev));

	switch (cp->type) {
	case AUDIO_MIXER_SET:
		if (cp->dev != AUCC_CHANNELS)
			return (EINVAL);
		cp->un.mask = sc->sc_channelmask;
		break;

	case AUDIO_MIXER_VALUE:
		i = cp->un.value.num_channels;
		if ((i < 1) || (i > 4))
			return (EINVAL);

		for (j = 0; j < i; j++)
			cp->un.value.level[j] =
			    (sc->sc_channel[j].nd_volume << 2) +
			    (sc->sc_channel[j].nd_volume >> 4);
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
aucc_get_props(addr)
	void *addr;
{
	return 0;
}

int
aucc_query_devinfo(addr, dip)
	void *addr;
	register mixer_devinfo_t *dip;
{
	int i;

	switch (dip->index) {
	case AUCC_CHANNELS:
		dip->type = AUDIO_MIXER_SET;
		dip->mixer_class = AUCC_OUTPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
                strcpy(dip->label.name, AudioNmaster);
		for (i = 0; i < 16; i++) {
			sprintf(dip->un.s.member[i].label.name,
			    "channelmask%d", i);
			dip->un.s.member[i].mask = i;
		}
		dip->un.s.num_mem = 16;
		break;

	case AUCC_VOLUME:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = AUCC_OUTPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNspeaker);
		dip->un.v.num_channels = 4;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case AUCC_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = AUCC_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		break;

	default:
		return (ENXIO);
	}

	DPRINTF(("AUDIO_MIXER_DEVINFO: name=%s\n", dip->label.name));

	return (0);
}


/* audio int handler */
void
aucc_inthdl(ch)
	int ch;
{
	int i;
	int mask = aucc->sc_channel[ch].nd_mask;

	/*
	 * For all channels in this maskgroup:
	 * disable dma, int 
	 * mark idle
	 */
	DPRINTF(("inthandler called, channel %d, mask 0x%x\n", ch, mask));

	custom.intreq = mask << INTB_AUD0; /* clear request */

	/*
	 * XXX: maybe we can leave ints and/or DMA on, if another sample has
	 * to be played?
	 */
	custom.intena = mask << INTB_AUD0;

	/*
	 * XXX custom.dmacon = mask; NO!!! 
	 */ 
	for (i = 0; i < 4; i++) {
		if (masks2[i] && mask) {
			DPRINTF(("marking channel %d idle\n", i));
			aucc->sc_channel[i].nd_busy = 0;
			aucc->sc_channel[i].nd_mask = 0;
			channel[i].isaudio = channel[i].play_count = 0;
		}
	}

	/* call handler */
	if (aucc->sc_channel[ch].nd_intr) {
		DPRINTF(("calling %p\n",aucc->sc_channel[ch].nd_intr));
		(*(aucc->sc_channel[ch].nd_intr))(
		    aucc->sc_channel[ch].nd_intrdata);
	} else
		DPRINTF(("zero int handler\n"));
	DPRINTF(("ints done\n"));
}

/* transform frequency to period, adjust bounds */
u_int
freqtoper(freq)
	u_int freq;
{
	u_int per = eclockfreq * 5 / freq;
	
	if (per < 124)
		per = 124; /* must have at least 124 ticks between samples */

	return per;
}

/* transform period to frequency */
u_int
pertofreq(per)
	u_int per;
{
	u_int freq = eclockfreq * 5 / per;

	return freq;
}

void
aucc_encode(enc, channels, i, p, dmap)
	int enc, channels, i;
	u_char *p;
	u_short **dmap;
{
	char *q, *r, *s, *t;
	int off;
	u_char *tab;
#ifdef AUCCDEBUG
	static int debctl = 6;
#endif

	off = 0;
	tab = NULL;

#ifdef AUCCDEBUG
	if (--debctl >= 0)
		printf("Enc: enc %d, chan %d, dmap %p %p %p %p\n",
		    enc, channels, dmap[0], dmap[1], dmap[2], dmap[3]);
#endif

	switch (enc) {
	case AUDIO_ENCODING_ULAW:
		tab = ulaw_to_lin;
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
	case AUDIO_ENCODING_ULINEAR_LE:
		off = -128;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
	case AUDIO_ENCODING_SLINEAR_LE:
		break;
	default:
		return;
	}

	q = (char *)dmap[0];
	r = (char *)dmap[1];
	s = (char *)dmap[2];
	t = (char *)dmap[3];

	if (tab)
		while (i) {
			switch (channels) {
			case 4: *t++ = tab[*p++];
			case 3: *s++ = tab[*p++];
			case 2: *r++ = tab[*p++];
			case 1: *q++ = tab[*p++];
			}
			i -= channels;
		}
	else
		while (i) {
			switch (channels) {
			case 4: *t++ = *p++ + off;
			case 3: *s++ = *p++ + off;
			case 2: *r++ = *p++ + off;
			case 1: *q++ = *p++ + off;
			}
			i -= channels;
		}
	
}

#endif /* NAUCC > 0 */
