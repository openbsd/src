/*	$OpenBSD: audio.c,v 1.12 1998/04/26 21:03:06 provos Exp $	*/
/*	$NetBSD: audio.c,v 1.71 1997/09/06 01:14:48 augustss Exp $	*/

/*
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 */

/*
 * This is a (partially) SunOS-compatible /dev/audio driver for NetBSD.
 * 
 * This code tries to do something half-way sensible with
 * half-duplex hardware, such as with the SoundBlaster hardware.  With
 * half-duplex hardware allowing O_RDWR access doesn't really make
 * sense.  However, closing and opening the device to "turn around the
 * line" is relatively expensive and costs a card reset (which can
 * take some time, at least for the SoundBlaster hardware).  Instead
 * we allow O_RDWR access, and provide an ioctl to set the "mode",
 * i.e. playing or recording.
 *
 * If you write to a half-duplex device in record mode, the data is
 * tossed.  If you read from the device in play mode, you get silence
 * filled buffers at the rate at which samples are naturally
 * generated.
 *
 * If you try to set both play and record mode on a half-duplex
 * device, playing takes precedence.
 */

/*
 * Todo:
 * - Add softaudio() isr processing for wakeup, poll, signals, 
 *   and silence fill.
 */

#include "audio.h"
#if NAUDIO > 0

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/device.h>

#include <dev/audio_if.h>
#include <dev/audiovar.h>

#include <vm/vm.h>
#include <vm/vm_prot.h>

#include <machine/endian.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (audiodebug) printf x
int	audiodebug = 0;
#else
#define DPRINTF(x)
#endif

#define ROUNDSIZE(x) x &= -16	/* round to nice boundary */

int	audio_blk_ms = AUDIO_BLK_MS;

int	audiosetinfo __P((struct audio_softc *, struct audio_info *));
int	audiogetinfo __P((struct audio_softc *, struct audio_info *));

int	audio_open __P((dev_t, int, int, struct proc *));
int	audio_close __P((dev_t, int, int, struct proc *));
int	audio_read __P((dev_t, struct uio *, int));
int	audio_write __P((dev_t, struct uio *, int));
int	audio_ioctl __P((dev_t, int, caddr_t, int, struct proc *));
int	audio_select __P((dev_t, int, struct proc *));
int	audio_mmap __P((dev_t, int, int));

int	mixer_open __P((dev_t, int, int, struct proc *));
int	mixer_close __P((dev_t, int, int, struct proc *));
int	mixer_ioctl __P((dev_t, int, caddr_t, int, struct proc *));
static	void mixer_remove __P((struct audio_softc *, struct proc *p));
static	void mixer_signal __P((struct audio_softc *));
    
void	audio_init_record __P((struct audio_softc *));
void	audio_init_play __P((struct audio_softc *));
int	audiostartr __P((struct audio_softc *));
int	audiostartp __P((struct audio_softc *));
void	audio_rint __P((void *));
void	audio_pint __P((void *));
int	audio_check_params __P((struct audio_params *));

void	audio_calc_blksize __P((struct audio_softc *, int));
void	audio_fill_silence __P((struct audio_params *, u_char *, int));
int	audio_silence_copyout __P((struct audio_softc *, int, struct uio *));

void	audio_init_ringbuffer __P((struct audio_ringbuffer *));
int	audio_initbufs __P((struct audio_softc *));
void	audio_calcwater __P((struct audio_softc *));
static __inline int audio_sleep_timo __P((int *, char *, int));
static __inline int audio_sleep __P((int *, char *));
static __inline void audio_wakeup __P((int *));
int	audio_drain __P((struct audio_softc *));
void	audio_clear __P((struct audio_softc *));
static __inline void audio_pint_silence __P((struct audio_softc *, struct audio_ringbuffer *, u_char *, int));

int	audio_alloc_ring __P((struct audio_softc *, struct audio_ringbuffer *, int));
void	audio_free_ring __P((struct audio_softc *, struct audio_ringbuffer *));

int	audioprint __P((void *, const char *));

#define __BROKEN_INDIRECT_CONFIG /* XXX */
#ifdef __BROKEN_INDIRECT_CONFIG
int	audioprobe __P((struct device *, void *, void *));
#else
int	audioprobe __P((struct device *, struct cfdata *, void *));
#endif
void	audioattach __P((struct device *, struct device *, void *));

struct portname {
	char 	*name;
	int 	mask;
};
static struct portname itable[] = {
	{ AudioNmicrophone,	AUDIO_MICROPHONE },
	{ AudioNline,		AUDIO_LINE_IN },
	{ AudioNcd,		AUDIO_CD },
	{ 0 }
};
static struct portname otable[] = {
	{ AudioNspeaker,	AUDIO_SPEAKER },
	{ AudioNheadphone,	AUDIO_HEADPHONE },
	{ AudioNline,		AUDIO_LINE_OUT },
	{ 0 }
};
void	au_check_ports __P((struct audio_softc *, struct au_mixer_ports *, 
			    mixer_devinfo_t *, int, char *, char *,
			    struct portname *));
int	au_set_gain __P((struct audio_softc *, struct au_mixer_ports *, 
			 int, int));
void	au_get_gain __P((struct audio_softc *, struct au_mixer_ports *,
			 u_int *, u_char *));
int	au_set_port __P((struct audio_softc *, struct au_mixer_ports *,
			 u_int));
int	au_get_port __P((struct audio_softc *, struct au_mixer_ports *));
int	au_get_lr_value __P((struct audio_softc *, mixer_ctrl_t *,
			     int *, int *r));
int	au_set_lr_value __P((struct audio_softc *, mixer_ctrl_t *,
			     int, int));
int	au_portof __P((struct audio_softc *, char *));


/* The default audio mode: 8 kHz mono ulaw */
struct audio_params audio_default = 
	{ 8000, AUDIO_ENCODING_ULAW, 8, 1, 0, 1 };

struct cfattach audio_ca = {
	sizeof(struct audio_softc), audioprobe, audioattach
};

struct cfdriver audio_cd = {
	NULL, "audio", DV_DULL
};

int
audioprobe(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct audio_attach_args *sa = aux;

	DPRINTF(("audioprobe: type=%d sa=%p hw=%p\n", 
		   sa->type, sa, sa->hwif));
	return (sa->type == AUDIODEV_TYPE_AUDIO) ? 1 : 0;
}

void
audioattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct audio_softc *sc = (void *)self;
	struct audio_attach_args *sa = aux;
	struct audio_hw_if *hwp = sa->hwif;
	void *hdlp = sa->hdl;
	int error;
	mixer_devinfo_t mi;
	int iclass, oclass;

	printf("\n");

#ifdef DIAGNOSTIC
	if (hwp == 0 ||
	    hwp->open == 0 ||
	    hwp->close == 0 ||
	    hwp->query_encoding == 0 ||
	    hwp->set_params == 0 ||
	    hwp->start_output == 0 ||
	    hwp->start_input == 0 ||
	    hwp->halt_output == 0 ||
	    hwp->halt_input == 0 ||
	    hwp->getdev == 0 ||
	    hwp->set_port == 0 ||
	    hwp->get_port == 0 ||
	    hwp->query_devinfo == 0 ||
	    hwp->get_props == 0) {
		printf("audio: missing method\n");
		sc->hw_if = 0;
		return;
        }
#endif

	sc->hw_if = hwp;
	sc->hw_hdl = hdlp;
	sc->sc_dev = parent;

	error = audio_alloc_ring(sc, &sc->sc_pr, AU_RING_SIZE);
	if (error) {
		sc->hw_if = 0;
		return;
	}
	error = audio_alloc_ring(sc, &sc->sc_rr, AU_RING_SIZE);
	if (error) {
		audio_free_ring(sc, &sc->sc_pr);
		sc->hw_if = 0;
		return;
	}
	
	/*
	 * Set default softc params
	 */
	sc->sc_pparams = audio_default;
	sc->sc_rparams = audio_default;

	/* Set up some default values */
	sc->sc_blkset = 0;
	audio_calc_blksize(sc, AUMODE_RECORD);
	audio_calc_blksize(sc, AUMODE_PLAY);
	audio_init_ringbuffer(&sc->sc_rr);
	audio_init_ringbuffer(&sc->sc_pr);
	audio_calcwater(sc);

	iclass = oclass = -1;
	sc->sc_inports.index = -1;
	sc->sc_inports.nports = 0;
	sc->sc_inports.isenum = 0;
	sc->sc_inports.allports = 0;
	sc->sc_outports.index = -1;
	sc->sc_outports.nports = 0;
	sc->sc_outports.isenum = 0;
	sc->sc_outports.allports = 0;
	sc->sc_monitor_port = -1;
	for(mi.index = 0; ; mi.index++) {
		if (hwp->query_devinfo(hdlp, &mi) != 0)
			break;
		if (mi.type == AUDIO_MIXER_CLASS &&
		    strcmp(mi.label.name, AudioCrecord) == 0)
			iclass = mi.index;
		if (mi.type == AUDIO_MIXER_CLASS &&
		    strcmp(mi.label.name, AudioCmonitor) == 0)
			oclass = mi.index;
 	}
	for(mi.index = 0; ; mi.index++) {
		if (hwp->query_devinfo(hdlp, &mi) != 0)
			break;
		au_check_ports(sc, &sc->sc_inports,  &mi, iclass, 
			       AudioNsource, AudioNrecord, itable);
		au_check_ports(sc, &sc->sc_outports, &mi, oclass, 
			       AudioNoutput, AudioNmaster, otable);
		if (mi.mixer_class == oclass && 
		    strcmp(mi.label.name, AudioNmonitor))
			sc->sc_monitor_port = mi.index;
	}
	DPRINTF(("audio_attach: inputs ports=0x%x, output ports=0x%x\n",
		 sc->sc_inports.allports, sc->sc_outports.allports));
}

int
au_portof(sc, name)
	struct	audio_softc *sc;
	char	*name;
{
	mixer_devinfo_t mi;

	for(mi.index = 0; 
	    sc->hw_if->query_devinfo(sc->hw_hdl, &mi) == 0;
	    mi.index++)
		if (strcmp(mi.label.name, name) == 0)
			return mi.index;
	return -1;
}

void
au_check_ports(sc, ports, mi, cls, name, mname, tbl)
	struct	audio_softc *sc;
	struct	au_mixer_ports *ports;
	mixer_devinfo_t *mi;
	int	cls;
	char	*name;
	char	*mname;
	struct	portname *tbl;
{
	int i, j;

	if (mi->mixer_class != cls)
		return;
	if (strcmp(mi->label.name, mname) == 0) {
		ports->master = mi->index;
		return;
	}
	if (strcmp(mi->label.name, name) != 0)
		return;
	if (mi->type == AUDIO_MIXER_ENUM) {
	    ports->index = mi->index;
	    for(i = 0; tbl[i].name; i++) {
		for(j = 0; j < mi->un.e.num_mem; j++) {
		    if (strcmp(mi->un.e.member[j].label.name,
			       tbl[i].name) == 0) {
			ports->aumask[ports->nports] = tbl[i].mask;
			ports->misel [ports->nports] = mi->un.e.member[j].ord;
			ports->miport[ports->nports++] = 
				au_portof(sc, mi->un.e.member[j].label.name);
			ports->allports |= tbl[i].mask;
		    }
		}
	    }
	    ports->isenum = 1;
	} else if (mi->type == AUDIO_MIXER_SET) {
	    ports->index = mi->index;
	    for(i = 0; tbl[i].name; i++) {
		for(j = 0; j < mi->un.s.num_mem; j++) {
		    if (strcmp(mi->un.s.member[j].label.name,
			       tbl[i].name) == 0) {
			ports->aumask[ports->nports] = tbl[i].mask;
			ports->misel [ports->nports] = mi->un.s.member[j].mask;
			ports->miport[ports->nports++] = 
				au_portof(sc, mi->un.s.member[j].label.name);
			ports->allports |= tbl[i].mask;
		    }
		}
	    }
	}
}

/*
 * Called from hardware driver.  This is where the MI audio driver gets
 * probed/attached to the hardware driver.
 */
void
audio_attach_mi(ahwp, mhwp, hdlp, dev)
	struct audio_hw_if *ahwp;
	struct midi_hw_if *mhwp;
	void *hdlp;
	struct device *dev;
{
	struct audio_attach_args arg;

	if (ahwp != NULL) {
		arg.type = AUDIODEV_TYPE_AUDIO;
		arg.hwif = ahwp;
		arg.hdl = hdlp;
		(void)config_found(dev, &arg, audioprint);
	}
	if (mhwp != NULL) {
		arg.type = AUDIODEV_TYPE_MIDI;
		arg.hwif = mhwp;
		arg.hdl = hdlp;
		(void)config_found(dev, &arg, audioprint);
	}
}

int
audioprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct audio_attach_args *arg = aux;
	const char *type;
 
	if (pnp != NULL) {
		switch (arg->type) {
		case AUDIODEV_TYPE_AUDIO:
			type = "audio";
			break;
		case AUDIODEV_TYPE_MIDI:
			type = "midi";
			break;
		default:
			panic("audioprint: unknown type %d", arg->type);
		}
		printf("%s at %s", type, pnp);
	}
	return (UNCONF);
}

#ifdef AUDIO_DEBUG
void	audio_printsc __P((struct audio_softc *));
void	audio_print_params __P((char *, struct audio_params *));

void
audio_printsc(sc)
	struct audio_softc *sc;
{
	printf("hwhandle %p hw_if %p ", sc->hw_hdl, sc->hw_if);
	printf("open 0x%x mode 0x%x\n", sc->sc_open, sc->sc_mode);
	printf("rchan 0x%x wchan 0x%x ", sc->sc_rchan, sc->sc_wchan);
	printf("rring used 0x%x pring used=%d\n", sc->sc_rr.used, sc->sc_pr.used);
	printf("rbus 0x%x pbus 0x%x ", sc->sc_rbus, sc->sc_pbus);
	printf("blksize %d", sc->sc_pr.blksize);
	printf("hiwat %d lowat %d\n", sc->sc_pr.usedhigh, sc->sc_pr.usedlow);
}

void
audio_print_params(s, p)
	char *s;
	struct audio_params *p;
{
	printf("audio: %s sr=%ld, enc=%d, chan=%d, prec=%d\n", s,
	       p->sample_rate, p->encoding, p->channels, p->precision);
}
#endif

int
audio_alloc_ring(sc, r, bufsize)
	struct audio_softc *sc;
	struct audio_ringbuffer *r;
	int bufsize;
{
	struct audio_hw_if *hw = sc->hw_if;
	void *hdl = sc->hw_hdl;
	/*
	 * Alloc DMA play and record buffers
	 */
	ROUNDSIZE(bufsize);
	if (bufsize < AUMINBUF)
		bufsize = AUMINBUF;
	if (hw->round_buffersize)
		bufsize = hw->round_buffersize(hdl, bufsize);
	r->bufsize = bufsize;
	if (hw->alloc)
	    r->start = hw->alloc(hdl, r->bufsize, M_DEVBUF, M_WAITOK);
	else
	    r->start = malloc(bufsize, M_DEVBUF, M_WAITOK);
	if (r->start == 0)
		return ENOMEM;
	return 0;
}

void
audio_free_ring(sc, r)
	struct audio_softc *sc;
	struct audio_ringbuffer *r;
{
	if (sc->hw_if->free) {
	    sc->hw_if->free(sc->hw_hdl, r->start, M_DEVBUF);
	} else {
	    free(r->start, M_DEVBUF);
	}
}

int
audioopen(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{

	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	case AUDIOCTL_DEVICE:
		return (audio_open(dev, flags, ifmt, p));
	case MIXER_DEVICE:
		return (mixer_open(dev, flags, ifmt, p));
	default:
		return (ENXIO);
	}
}

int
audioclose(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{

	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		return (audio_close(dev, flags, ifmt, p));
	case MIXER_DEVICE:
		return (mixer_close(dev, flags, ifmt, p));
	case AUDIOCTL_DEVICE:
		return 0;
	default:
		return (ENXIO);
	}
}

int
audioread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		return (audio_read(dev, uio, ioflag));
	case AUDIOCTL_DEVICE:
	case MIXER_DEVICE:
		return (ENODEV);
	default:
		return (ENXIO);
	}
}

int
audiowrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		return (audio_write(dev, uio, ioflag));
	case AUDIOCTL_DEVICE:
	case MIXER_DEVICE:
		return (ENODEV);
	default:
		return (ENXIO);
	}
}

int
audioioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{

	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	case AUDIOCTL_DEVICE:
		return (audio_ioctl(dev, cmd, addr, flag, p));
	case MIXER_DEVICE:
		return (mixer_ioctl(dev, cmd, addr, flag, p));
	default:
		return (ENXIO);
	}
}

int
audioselect(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{

	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		return (audio_select(dev, events, p));
	case AUDIOCTL_DEVICE:
	case MIXER_DEVICE:
		return (0);
	default:
		return (0);
	}
}

int
audiommap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		return (audio_mmap(dev, off, prot));
	case AUDIOCTL_DEVICE:
	case MIXER_DEVICE:
		return -1;
	default:
		return -1;
	}
}

/*
 * Audio driver
 */
void
audio_init_ringbuffer(rp)
	struct audio_ringbuffer *rp;
{
	int nblks;
	int blksize = rp->blksize;

	if (blksize < AUMINBLK)
		blksize = AUMINBLK;
	nblks = rp->bufsize / blksize;
	if (nblks < AUMINNOBLK) {
		nblks = AUMINNOBLK;
		blksize = rp->bufsize / nblks;
		ROUNDSIZE(blksize);
	}
	DPRINTF(("audio_init_ringbuffer: blksize=%d\n", blksize));
	rp->blksize = blksize;
	rp->maxblks = nblks;
	rp->used = 0;
	rp->end = rp->start + nblks * blksize;
	rp->inp = rp->outp = rp->start;
	rp->stamp = 0;
	rp->drops = 0;
	rp->pause = 0;
	rp->copying = 0;
	rp->needfill = 0;
	rp->mmapped = 0;
}

int
audio_initbufs(sc)
	struct audio_softc *sc;
{
	struct audio_hw_if *hw = sc->hw_if;
	int error;

	DPRINTF(("audio_initbufs: mode=0x%x\n", sc->sc_mode));
	audio_init_ringbuffer(&sc->sc_rr);
	if (hw->init_input && (sc->sc_mode & AUMODE_RECORD)) {
		error = hw->init_input(sc->hw_hdl, sc->sc_rr.start,
				       sc->sc_rr.end - sc->sc_rr.start);
		if (error)
			return error;
	}

	audio_init_ringbuffer(&sc->sc_pr);
	sc->sc_sil_count = 0;
	if (hw->init_output && (sc->sc_mode & AUMODE_PLAY)) {
		error = hw->init_output(sc->hw_hdl, sc->sc_pr.start,
					sc->sc_pr.end - sc->sc_pr.start);
		if (error)
			return error;
	}

#ifdef AUDIO_INTR_TIME
	sc->sc_pnintr = 0;
	sc->sc_pblktime = (u_long)(
	    (double)sc->sc_pr.blksize * 1e6 / 
	    (double)(sc->sc_pparams.precision / NBBY * 
                     sc->sc_pparams.channels * 
		     sc->sc_pparams.sample_rate));
	DPRINTF(("audio: play blktime = %lu for %d\n", 
		 sc->sc_pblktime, sc->sc_pr.blksize));
	sc->sc_rnintr = 0;
	sc->sc_rblktime = (u_long)(
	    (double)sc->sc_rr.blksize * 1e6 / 
	    (double)(sc->sc_rparams.precision / NBBY * 
                     sc->sc_rparams.channels * 
		     sc->sc_rparams.sample_rate));
	DPRINTF(("audio: record blktime = %lu for %d\n", 
		 sc->sc_rblktime, sc->sc_rr.blksize));
#endif

	return 0;
}

void
audio_calcwater(sc)
	struct audio_softc *sc;
{
	sc->sc_pr.usedhigh = sc->sc_pr.end - sc->sc_pr.start;
	sc->sc_pr.usedlow = sc->sc_pr.usedhigh * 3 / 4;	/* set lowater at 75% */
	if (sc->sc_pr.usedlow == sc->sc_pr.usedhigh)
		sc->sc_pr.usedlow -= sc->sc_pr.blksize;
	sc->sc_rr.usedhigh = sc->sc_pr.end - sc->sc_pr.start - sc->sc_pr.blksize;
	sc->sc_rr.usedlow = 0;
}

static __inline int
audio_sleep_timo(chan, label, timo)
	int *chan;
	char *label;
	int timo;
{
	int st;

	if (!label)
		label = "audio";

	*chan = 1;
	st = tsleep(chan, PWAIT | PCATCH, label, timo);
	*chan = 0;
#ifdef AUDIO_DEBUG
	if (st != 0)
	    printf("audio_sleep: %d\n", st);
#endif
	return (st);
}

static __inline int
audio_sleep(chan, label)
	int *chan;
	char *label;
{
	return audio_sleep_timo(chan, label, 0);
}

/* call at splaudio() */
static __inline void
audio_wakeup(chan)
	int *chan;
{
	if (*chan) {
		wakeup(chan);
		*chan = 0;
	}
}

int
audio_open(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	int error;
	int mode;
	struct audio_hw_if *hw;
	struct audio_info ai;

	if (unit >= audio_cd.cd_ndevs ||
	    (sc = audio_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	hw = sc->hw_if;
	if (!hw)
		return ENXIO;

	DPRINTF(("audio_open: dev=0x%x flags=0x%x sc=%p hdl=%p\n", dev, flags, sc, sc->hw_hdl));

	if (ISDEVAUDIOCTL(dev))
		return 0;

	if ((sc->sc_open & (AUOPEN_READ|AUOPEN_WRITE)) != 0)
		return (EBUSY);

	error = hw->open(sc->hw_hdl, flags);
	if (error)
		return (error);

	sc->sc_async_audio = 0;
	sc->sc_rchan = 0;
	sc->sc_wchan = 0;
	sc->sc_blkset = 0; /* Block sizes not set yet */
	sc->sc_sil_count = 0;
	sc->sc_rbus = 0;
	sc->sc_pbus = 0;
	sc->sc_eof = 0;
	sc->sc_playdrop = 0;

	sc->sc_full_duplex = 0;
/* doesn't always work right on SB.
		(flags & (FWRITE|FREAD)) == (FWRITE|FREAD) &&
		(hw->get_props(sc->hw_hdl) & AUDIO_PROP_FULLDUPLEX);
*/

	mode = 0;
	if (flags & FREAD) {
		sc->sc_open |= AUOPEN_READ;
		mode |= AUMODE_RECORD;
	}
	if (flags & FWRITE) {
		sc->sc_open |= AUOPEN_WRITE;
		mode |= AUMODE_PLAY | AUMODE_PLAY_ALL;
	}

	/*
	 * Multiplex device: /dev/audio (MU-Law) and /dev/sound (linear)
	 * The /dev/audio is always (re)set to 8-bit MU-Law mono
	 * For the other devices, you get what they were last set to.
	 */
	if (ISDEVAUDIO(dev)) {
		/* /dev/audio */
		sc->sc_rparams = audio_default;
		sc->sc_pparams = audio_default;
	}
#ifdef DIAGNOSTIC
	/*
	 * Sample rate and precision are supposed to be set to proper
	 * default values by the hardware driver, so that it may give
	 * us these values.
	 */
	if (sc->sc_rparams.precision == 0 || sc->sc_pparams.precision == 0) {
		printf("audio_open: 0 precision\n");
		return EINVAL;
	}
#endif

	AUDIO_INITINFO(&ai);
	ai.record.sample_rate = sc->sc_rparams.sample_rate;
	ai.record.encoding    = sc->sc_rparams.encoding;
	ai.record.channels    = sc->sc_rparams.channels;
	ai.record.precision   = sc->sc_rparams.precision;
	ai.play.sample_rate   = sc->sc_pparams.sample_rate;
	ai.play.encoding      = sc->sc_pparams.encoding;
	ai.play.channels      = sc->sc_pparams.channels;
	ai.play.precision     = sc->sc_pparams.precision;
	ai.mode		      = mode;
	sc->sc_pr.blksize = sc->sc_rr.blksize = 0; /* force recalculation */
	error = audiosetinfo(sc, &ai);
	if (error)
		goto bad;

	DPRINTF(("audio_open: done sc_mode = 0x%x\n", sc->sc_mode));
	
	return 0;

bad:
	hw->close(sc->hw_hdl);
	sc->sc_open = 0;
	sc->sc_mode = 0;
	sc->sc_full_duplex = 0;
	return error;
}

/*
 * Must be called from task context.
 */
void
audio_init_record(sc)
	struct audio_softc *sc;
{
	int s = splaudio();

	if (sc->hw_if->speaker_ctl &&
	    (!sc->sc_full_duplex || (sc->sc_mode & AUMODE_PLAY) == 0))
		sc->hw_if->speaker_ctl(sc->hw_hdl, SPKR_OFF);
	splx(s);
}

/*
 * Must be called from task context.
 */
void
audio_init_play(sc)
	struct audio_softc *sc;
{
	int s = splaudio();

	sc->sc_wstamp = sc->sc_pr.stamp;
	if (sc->hw_if->speaker_ctl)
		sc->hw_if->speaker_ctl(sc->hw_hdl, SPKR_ON);
	splx(s);
}

int
audio_drain(sc)
	struct audio_softc *sc;
{
	int error, drops;
	struct audio_ringbuffer *cb = &sc->sc_pr;
	int s;

	if (sc->sc_pr.mmapped || sc->sc_pr.used <= 0)
		return 0;
	if (!sc->sc_pbus) {
		/* We've never started playing, probably because the
		 * block was too short.  Pad it and start now.
		 */
		int cc;
		u_char *inp = cb->inp;

		cc = cb->blksize - (inp - cb->start) % cb->blksize;
		audio_fill_silence(&sc->sc_pparams, inp, cc);
		inp += cc;
		if (inp >= cb->end)
			inp = cb->start;
		s = splaudio();
		cb->used += cc;
		cb->inp = inp;
		error = audiostartp(sc);
		splx(s);
		if (error)
			return error;
	}
	/* 
	 * Play until a silence block has been played, then we
	 * know all has been drained.
	 * XXX This should be done some other way to avoid
	 * playing silence.
	 */
#ifdef DIAGNOSTIC
	if (cb->copying) {
		printf("audio_drain: copying in progress!?!\n");
		cb->copying = 0;
	}
#endif
	drops = cb->drops;
	error = 0;
	s = splaudio();
	while (cb->drops == drops && !error) {
		DPRINTF(("audio_drain: used=%d, drops=%ld\n", sc->sc_pr.used, cb->drops));
		/*
		 * When the process is exiting, it ignores all signals and
		 * we can't interrupt this sleep, so we set a timeout just in case.
		 */
		error = audio_sleep_timo(&sc->sc_wchan, "aud_dr", 30*hz);
	}
	splx(s);
	return error;
}

/*
 * Close an audio chip.
 */
/* ARGSUSED */
int
audio_close(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_hw_if *hw = sc->hw_if;
	int s;

	DPRINTF(("audio_close: unit=%d\n", unit));

        /* Stop recording. */
	if (sc->sc_rbus) {
		/* 
		 * XXX Some drivers (e.g. SB) use the same routine
		 * to halt input and output so don't halt input if
		 * in full duplex mode.  These drivers should be fixed.
		 */
		if (!sc->sc_full_duplex || sc->hw_if->halt_input != sc->hw_if->halt_output)
			sc->hw_if->halt_input(sc->hw_hdl);
		sc->sc_rbus = 0;
	}
	/*
	 * Block until output drains, but allow ^C interrupt.
	 */
	sc->sc_pr.usedlow = sc->sc_pr.blksize;	/* avoid excessive wakeups */
	s = splaudio();
	/*
	 * If there is pending output, let it drain (unless
	 * the output is paused).
	 */
	if ((sc->sc_mode & AUMODE_PLAY) && !sc->sc_pr.pause) {
		if (!audio_drain(sc) && hw->drain)
			(void)hw->drain(sc->hw_hdl);
	}
	
	hw->close(sc->hw_hdl);
	
	if (flags & FREAD)
		sc->sc_open &= ~AUOPEN_READ;
	if (flags & FWRITE)
		sc->sc_open &= ~AUOPEN_WRITE;

	sc->sc_async_audio = 0;
	sc->sc_mode = 0;
	sc->sc_full_duplex = 0;
	splx(s);
	DPRINTF(("audio_close: done\n"));

	return (0);
}

int
audio_read(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_ringbuffer *cb = &sc->sc_rr;
	u_char *outp;
	int error, s, used, cc, n;

	if (cb->mmapped)
		return EINVAL;

#ifdef AUDIO_DEBUG
	if (audiodebug > 1)
		printf("audio_read: cc=%d mode=%d\n", uio->uio_resid, sc->sc_mode);
#endif

	error = 0;
	/*
	 * If hardware is half-duplex and currently playing, return
	 * silence blocks based on the number of blocks we have output.
	 */
	if (!sc->sc_full_duplex &&
	    (sc->sc_mode & AUMODE_PLAY)) {
		while (uio->uio_resid > 0 && !error) {
			s = splaudio();
			for(;;) {
				cc = sc->sc_pr.stamp - sc->sc_wstamp;
				if (cc > 0)
					break;
				DPRINTF(("audio_read: stamp=%lu, wstamp=%lu\n", 
					 sc->sc_pr.stamp, sc->sc_wstamp));
				if (ioflag & IO_NDELAY) {
					splx(s);
					return EWOULDBLOCK;
				}
				error = audio_sleep(&sc->sc_rchan, "aud_hr");
				if (error) {
					splx(s);
					return error;
				}
			}
			splx(s);

			if (uio->uio_resid < cc)
				cc = uio->uio_resid;
#ifdef AUDIO_DEBUG
			if (audiodebug > 1)
				printf("audio_read: reading in write mode, cc=%d\n", cc);
#endif	
			error = audio_silence_copyout(sc, cc, uio);
			sc->sc_wstamp += cc;
		} 
		return (error);
	}
	while (uio->uio_resid > 0 && !error) {
		s = splaudio();
		while (cb->used <= 0) {
			if (ioflag & IO_NDELAY) {
				splx(s);
				return EWOULDBLOCK;
			}
			if (!sc->sc_rbus) {
				error = audiostartr(sc);
				if (error) {
					splx(s);
					return error;
				}
			}
#ifdef AUDIO_DEBUG
			if (audiodebug > 2)
				printf("audio_read: sleep used=%d\n", cb->used);
#endif
			error = audio_sleep(&sc->sc_rchan, "aud_rd");
			if (error) {
				splx(s);
				return error;
			}
		}
		used = cb->used;
		outp = cb->outp;
		cb->copying = 1;
		splx(s);
		cc = used - cb->usedlow; /* maximum to read */
		n = cb->end - outp;
		if (n < cc)
			cc = n;	/* don't read beyond end of buffer */
		
		if (uio->uio_resid < cc)
			cc = uio->uio_resid; /* and no more than we want */

		if (sc->sc_rparams.sw_code)
			sc->sc_rparams.sw_code(sc->hw_hdl, outp, cc);
#ifdef AUDIO_DEBUG
		if (audiodebug > 1)
			printf("audio_read: outp=%p, cc=%d\n", outp, cc);
#endif	
		error = uiomove(outp, cc, uio);
		used -= cc;
		outp += cc;
		if (outp >= cb->end)
			outp = cb->start;
		s = splaudio();
		cb->outp = outp;
		cb->used = used;
		cb->copying = 0;
		splx(s);
	}
	return (error);
}

void
audio_clear(sc)
	struct audio_softc *sc;
{
	int s = splaudio();

	if (sc->sc_rbus) {
		audio_wakeup(&sc->sc_rchan);
		sc->hw_if->halt_input(sc->hw_hdl);
		sc->sc_rbus = 0;
	}
	if (sc->sc_pbus) {
		audio_wakeup(&sc->sc_wchan);
		sc->hw_if->halt_output(sc->hw_hdl);
		sc->sc_pbus = 0;
	}
	splx(s);
}

void
audio_calc_blksize(sc, mode)
	struct audio_softc *sc;
	int mode;
{
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_params *parm;
	struct audio_ringbuffer *rb;
    	int bs;

	if (sc->sc_blkset)
		return;

	if (mode == AUMODE_PLAY) {
		parm = &sc->sc_pparams;
		rb = &sc->sc_pr;
	} else {
		parm = &sc->sc_rparams;
		rb = &sc->sc_rr;
	}
	
	bs = parm->sample_rate * audio_blk_ms / 1000 *
	     parm->channels * parm->precision / NBBY *
	     parm->factor;
	ROUNDSIZE(bs);
	if (hw->round_blocksize)
		bs = hw->round_blocksize(sc->hw_hdl, bs);
	rb->blksize = bs;

	DPRINTF(("audio_calc_blksize: %s blksize=%d\n", 
		 mode == AUMODE_PLAY ? "play" : "record", bs));
}

void
audio_fill_silence(params, p, n)
	struct audio_params *params;
        u_char *p;
        int n;
{
	u_char auzero0, auzero1 = 0; /* initialize to please gcc */
	int nfill = 1;

	switch (params->encoding) {
	case AUDIO_ENCODING_ULAW:
	    	auzero0 = 0x7f; 
		break;
	case AUDIO_ENCODING_ALAW:
		auzero0 = 0x55;
		break;
	case AUDIO_ENCODING_MPEG_L1_STREAM:
	case AUDIO_ENCODING_MPEG_L1_PACKETS:
	case AUDIO_ENCODING_MPEG_L1_SYSTEM:
	case AUDIO_ENCODING_MPEG_L2_STREAM:
	case AUDIO_ENCODING_MPEG_L2_PACKETS:
	case AUDIO_ENCODING_MPEG_L2_SYSTEM:
	case AUDIO_ENCODING_ADPCM: /* is this right XXX */
	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_SLINEAR_BE:
		auzero0 = 0;	/* fortunately this works for both 8 and 16 bits */
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
	case AUDIO_ENCODING_ULINEAR_BE:
		if (params->precision == 16) {
			nfill = 2;
			if (params->encoding == AUDIO_ENCODING_ULINEAR_LE) {
				auzero0 = 0;
				auzero1 = 0x80;
			} else {
				auzero0 = 0x80;
				auzero1 = 0;
			}
		} else
			auzero0 = 0x80;
		break;
	default:
		DPRINTF(("audio: bad encoding %d\n", params->encoding));
		auzero0 = 0;
		break;
	}
	if (nfill == 1) {
		while (--n >= 0)
			*p++ = auzero0; /* XXX memset */
	} else /* nfill must be 2 */ {
		while (n > 1) {
			*p++ = auzero0;
			*p++ = auzero1;
			n -= 2;
		}
	}
}

int
audio_silence_copyout(sc, n, uio)
	struct audio_softc *sc;
	int n;
	struct uio *uio;
{
	int error;
	int k;
	u_char zerobuf[128];

	audio_fill_silence(&sc->sc_rparams, zerobuf, sizeof zerobuf);

	error = 0;
        while (n > 0 && uio->uio_resid > 0 && !error) {
		k = min(n, min(uio->uio_resid, sizeof zerobuf));
		error = uiomove(zerobuf, k, uio);
		n -= k;
	}
        return (error);
}

int
audio_write(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_ringbuffer *cb = &sc->sc_pr;
	u_char *inp, *einp;
	int error, s, n, cc, used;

	DPRINTF(("audio_write: sc=%p(unit=%d) count=%d used=%d(hi=%d)\n", sc, unit,
		 uio->uio_resid, sc->sc_pr.used, sc->sc_pr.usedhigh));

	if (cb->mmapped)
		return EINVAL;

	if (uio->uio_resid == 0) {
		sc->sc_eof++;
		return 0;
	}

	/*
	 * If half-duplex and currently recording, throw away data.
	 */
	if (!sc->sc_full_duplex &&
	    (sc->sc_mode & AUMODE_RECORD)) {
		uio->uio_offset += uio->uio_resid;
		uio->uio_resid = 0;
		DPRINTF(("audio_write: half-dpx read busy\n"));
		return (0);
	}

	if (!(sc->sc_mode & AUMODE_PLAY_ALL) && sc->sc_playdrop > 0) {
		n = min(sc->sc_playdrop, uio->uio_resid);
		DPRINTF(("audio_write: playdrop %d\n", n));
		uio->uio_offset += n;
		uio->uio_resid -= n;
		sc->sc_playdrop -= n;
		if (uio->uio_resid == 0)
			return 0;
	}

#ifdef AUDIO_DEBUG
	if (audiodebug > 1)
		printf("audio_write: sr=%ld, enc=%d, prec=%d, chan=%d, sw=%p, fact=%d\n",
		       sc->sc_pparams.sample_rate, sc->sc_pparams.encoding,
		       sc->sc_pparams.precision, sc->sc_pparams.channels,
		       sc->sc_pparams.sw_code, sc->sc_pparams.factor);
#endif

	error = 0;
	while (uio->uio_resid > 0 && !error) {
		s = splaudio();
		while (cb->used >= cb->usedhigh) {
			DPRINTF(("audio_write: sleep used=%d lowat=%d hiwat=%d\n", 
				 cb->used, cb->usedlow, cb->usedhigh));
			if (ioflag & IO_NDELAY) {
				splx(s);
				return (EWOULDBLOCK);
			}
			error = audio_sleep(&sc->sc_wchan, "aud_wr");
			if (error) {
				splx(s);
				return error;
			}
		}
		used = cb->used;
		inp = cb->inp;
		cb->copying = 1;
		splx(s);
		cc = cb->usedhigh - used; 	/* maximum to write */
		n = cb->end - inp;
		if (sc->sc_pparams.factor != 1) {
			/* Compensate for software coding expansion factor. */
			n /= sc->sc_pparams.factor;
			cc /= sc->sc_pparams.factor;
		}
		if (n < cc)
			cc = n;			/* don't write beyond end of buffer */
		if (uio->uio_resid < cc)
			cc = uio->uio_resid; 	/* and no more than we have */

#ifdef DIAGNOSTIC
		/* 
		 * This should never happen since the block size and and
		 * block pointers are always nicely aligned. 
		 */
		if (cc == 0) {
			printf("audio_write: cc == 0, swcode=%p, factor=%d\n",
			       sc->sc_pparams.sw_code, sc->sc_pparams.factor);
			cb->copying = 0;
			return EINVAL;
		}
#endif
#ifdef AUDIO_DEBUG
		if (audiodebug > 1)
		    printf("audio_write: uiomove cc=%d inp=%p, left=%d\n", cc, inp, uio->uio_resid);
#endif
		n = uio->uio_resid;
		error = uiomove(inp, cc, uio);
		cc = n - uio->uio_resid; /* number of bytes actually moved */
#ifdef AUDIO_DEBUG
		if (error)
		        printf("audio_write:(1) uiomove failed %d; cc=%d inp=%p\n",
			       error, cc, inp);
#endif
		/* 
		 * Continue even if uiomove() failed because we may have
		 * gotten a partial block.
		 */

		if (sc->sc_pparams.sw_code) {
			sc->sc_pparams.sw_code(sc->hw_hdl, inp, cc);
			/* Adjust count after the expansion. */
			cc *= sc->sc_pparams.factor;
#ifdef AUDIO_DEBUG
			if (audiodebug > 1)
				printf("audio_write: expanded cc=%d\n", cc);
#endif
		}

		einp = cb->inp + cc;
		if (einp >= cb->end)
			einp = cb->start;

		s = splaudio();
		/*
		 * This is a very suboptimal way of keeping track of
		 * silence in the buffer, but it is simple.
		 */
		sc->sc_sil_count = 0;

		cb->inp = einp;
		cb->used += cc;
		/* If the interrupt routine wants the last block filled AND
		 * the copy did not fill the last block completely it needs to
		 * be padded.
		 */
		if (cb->needfill &&
		    (inp  - cb->start) / cb->blksize == 
		    (einp - cb->start) / cb->blksize) {
			/* Figure out how many bytes there is to a block boundary. */
			cc = cb->blksize - (einp - cb->start) % cb->blksize;
			DPRINTF(("audio_write: partial fill %d\n", cc));
		} else
			cc = 0;
		cb->needfill = 0;
		cb->copying = 0;
		if (!sc->sc_pbus && !cb->pause)
			error = audiostartp(sc); /* XXX should not clobber error */
		splx(s);
		if (cc) {
#ifdef AUDIO_DEBUG
			if (audiodebug > 1)
				printf("audio_write: fill %d\n", cc);
#endif
			audio_fill_silence(&sc->sc_pparams, einp, cc);
		}
	}
	return (error);
}

int
audio_ioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_offset *ao;
	int error = 0, s, offs, fd;
	int rbus, pbus;

	DPRINTF(("audio_ioctl(%d,'%c',%d)\n",
	          IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff));
	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;

	case FIOASYNC:
		if (*(int *)addr) {
			if (sc->sc_async_audio)
				return (EBUSY);
			sc->sc_async_audio = p;
			DPRINTF(("audio_ioctl: FIOASYNC %p\n", p));
		} else
			sc->sc_async_audio = 0;
		break;

	case AUDIO_FLUSH:
		DPRINTF(("AUDIO_FLUSH\n"));
		rbus = sc->sc_rbus;
		pbus = sc->sc_pbus;
		audio_clear(sc);
		s = splaudio();
		error = audio_initbufs(sc);
		if (error) {
			splx(s);
			return error;
		}
		if ((sc->sc_mode & AUMODE_PLAY) && !sc->sc_pbus && pbus)
			error = audiostartp(sc);
		if (!error &&
		    (sc->sc_mode & AUMODE_RECORD) && !sc->sc_rbus && rbus)
			error = audiostartr(sc);
		splx(s);
		break;

	/*
	 * Number of read (write) samples dropped.  We don't know where or
	 * when they were dropped.
	 */
	case AUDIO_RERROR:
		*(int *)addr = sc->sc_rr.drops;
		break;

	case AUDIO_PERROR:
		*(int *)addr = sc->sc_pr.drops;
		break;

	/*
	 * Offsets into buffer.
	 */
	case AUDIO_GETIOFFS:
		s = splaudio();
		/* figure out where next DMA will start */
		ao = (struct audio_offset *)addr;
		ao->samples = sc->sc_rr.stamp;
		ao->deltablks = (sc->sc_rr.stamp - sc->sc_rr.stamp_last) / sc->sc_rr.blksize;
		sc->sc_rr.stamp_last = sc->sc_rr.stamp;
		ao->offset = sc->sc_rr.inp - sc->sc_rr.start;
		splx(s);
		break;

	case AUDIO_GETOOFFS:
		s = splaudio();
		/* figure out where next DMA will start */
		ao = (struct audio_offset *)addr;
		offs = sc->sc_pr.outp - sc->sc_pr.start + sc->sc_pr.blksize;
		if (sc->sc_pr.start + offs >= sc->sc_pr.end)
			offs = 0;
		ao->samples = sc->sc_pr.stamp;
		ao->deltablks = (sc->sc_pr.stamp - sc->sc_pr.stamp_last) / sc->sc_pr.blksize;
		sc->sc_pr.stamp_last = sc->sc_pr.stamp;
		ao->offset = offs;
		splx(s);
		break;

	/*
	 * How many bytes will elapse until mike hears the first
	 * sample of what we write next?
	 */
	case AUDIO_WSEEK:
		*(u_long *)addr = sc->sc_rr.used;
		break;

	case AUDIO_SETINFO:
		DPRINTF(("AUDIO_SETINFO mode=0x%x\n", sc->sc_mode));
		error = audiosetinfo(sc, (struct audio_info *)addr);
		break;

	case AUDIO_GETINFO:
		DPRINTF(("AUDIO_GETINFO\n"));
		error = audiogetinfo(sc, (struct audio_info *)addr);
		break;

	case AUDIO_DRAIN:
		DPRINTF(("AUDIO_DRAIN\n"));
		error = audio_drain(sc);
		if (!error && hw->drain)
		    error = hw->drain(sc->hw_hdl);
		break;

	case AUDIO_GETDEV:
		DPRINTF(("AUDIO_GETDEV\n"));
		error = hw->getdev(sc->hw_hdl, (audio_device_t *)addr);
		break;
		
	case AUDIO_GETENC:
		DPRINTF(("AUDIO_GETENC\n"));
		error = hw->query_encoding(sc->hw_hdl, (struct audio_encoding *)addr);
		break;

	case AUDIO_GETFD:
		DPRINTF(("AUDIO_GETFD\n"));
		*(int *)addr = sc->sc_full_duplex;
		break;

	case AUDIO_SETFD:
		DPRINTF(("AUDIO_SETFD\n"));
		fd = *(int *)addr;
		if (hw->get_props(sc->hw_hdl) & AUDIO_PROP_FULLDUPLEX) {
			if (hw->setfd)
				error = hw->setfd(sc->hw_hdl, fd);
			else
				error = 0;
			if (!error)
				sc->sc_full_duplex = fd;
		} else {
			if (fd)
				error = ENOTTY;
			else
				error = 0;
		}
		break;

	case AUDIO_GETPROPS:
		DPRINTF(("AUDIO_GETPROPS\n"));
		*(int *)addr = hw->get_props(sc->hw_hdl);
		break;

	default:
		DPRINTF(("audio_ioctl: unknown ioctl\n"));
		error = EINVAL;
		break;
	}
	DPRINTF(("audio_ioctl(%d,'%c',%d) result %d\n",
	          IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff, error));
	return (error);
}

int
audio_select(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	int s = splaudio();

	DPRINTF(("audio_select: rw=0x%x mode=%d\n", rw, sc->sc_mode));

	switch (rw) {

	case FREAD:
		if ((sc->sc_mode & AUMODE_PLAY) ?
		    sc->sc_pr.stamp > sc->sc_wstamp : 
		    sc->sc_rr.used > sc->sc_rr.usedlow) {
			splx(s);
			return (1);
		}
		selrecord(p, &sc->sc_rsel);
		break;

	case FWRITE:
		if (sc->sc_mode & AUMODE_RECORD ||
		    sc->sc_pr.used <= sc->sc_pr.usedlow) {
			splx(s);
			return (1);
		}
		selrecord(p, &sc->sc_wsel);
		break;
	}
	splx(s);
	return (0);
}

int
audio_mmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	int s;
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_ringbuffer *cb;

	DPRINTF(("audio_mmap: off=%d, prot=%d\n", off, prot));

	if (!(hw->get_props(sc->hw_hdl) & AUDIO_PROP_MMAP) || !hw->mappage)
		return -1;
#if 0
/* XXX
 * The idea here was to use the protection to determine if
 * we are mapping the read or write buffer, but it fails.
 * The VM system is broken in (at least) two ways.
 * 1) If you map memory VM_PROT_WRITE you SIGSEGV
 *    when writing to it, so VM_PROT_READ|VM_PROT_WRITE
 *    has to be used for mmapping the play buffer.
 * 2) Even if calling mmap() with VM_PROT_READ|VM_PROT_WRITE
 *    audio_mmap will get called at some point with VM_PROT_READ
 *    only.
 * So, alas, we always map the play buffer for now.
 */
	if (prot == (VM_PROT_READ|VM_PROT_WRITE) ||
	    prot == VM_PROT_WRITE)
		cb = &sc->sc_pr;
	else if (prot == VM_PROT_READ)
		cb = &sc->sc_rr;
	else
		return -1;
#else
	cb = &sc->sc_pr;
#endif

	if (off >= cb->bufsize)
		return -1;
	if (!cb->mmapped) {
		cb->mmapped = 1;
		if (cb == &sc->sc_pr) {
			audio_fill_silence(&sc->sc_pparams, cb->start, cb->bufsize);
			s = splaudio();
			if (!sc->sc_pbus)
				(void)audiostartp(sc);
			splx(s);
		} else {
			s = splaudio();
			if (!sc->sc_rbus)
				(void)audiostartr(sc);
			splx(s);
		}
	}

	return hw->mappage(sc->hw_hdl, cb->start, off, prot);
}

int
audiostartr(sc)
	struct audio_softc *sc;
{
	int error;
    
    	DPRINTF(("audiostartr: start=%p used=%d(hi=%d) mmapped=%d\n", 
		 sc->sc_rr.start, sc->sc_rr.used, sc->sc_rr.usedhigh, 
		 sc->sc_rr.mmapped));

	error = sc->hw_if->start_input(sc->hw_hdl, sc->sc_rr.start, 
				       sc->sc_rr.blksize, audio_rint, (void *)sc);
	if (error) {
		DPRINTF(("audiostartr failed: %d\n", error));
		return error;
	}
	sc->sc_rbus = 1;
	return 0;
}

int
audiostartp(sc)
	struct audio_softc *sc;
{
	int error;
    
    	DPRINTF(("audiostartp: start=%p used=%d(hi=%d) mmapped=%d\n", 
		 sc->sc_pr.start, sc->sc_pr.used, sc->sc_pr.usedhigh,
		 sc->sc_pr.mmapped));
    
	if (sc->sc_pr.used >= sc->sc_pr.blksize || sc->sc_pr.mmapped) {
		error = sc->hw_if->start_output(sc->hw_hdl, sc->sc_pr.outp,
					sc->sc_pr.blksize, audio_pint, (void *)sc);
		if (error) {
			DPRINTF(("audiostartp failed: %d\n", error));
		    	return error;
		}
		sc->sc_pbus = 1;
	}
	return 0;
}

/*
 * When the play interrupt routine finds that the write isn't keeping
 * the buffer filled it will insert silence in the buffer to make up
 * for this.  The part of the buffer that is filled with silence
 * is kept track of in a very approcimate way: it starts at sc_sil_start
 * and extends sc_sil_count bytes.  If the writer doesn't write sc_sil_count
 * get to encompass the whole buffer after which no more filling needs
 * to be done.  When the writer starts again sc_sil_count is set to 0.
 */
/* XXX
 * Putting silence into the output buffer should not really be done
 * at splaudio, but there is no softaudio level to do it at yet.
 */
static __inline void
audio_pint_silence(sc, cb, inp, cc)
	struct audio_softc *sc;
	struct audio_ringbuffer *cb;
	u_char *inp;
	int cc;
{
	u_char *s, *e, *p, *q;

	if (sc->sc_sil_count > 0) {
		s = sc->sc_sil_start; /* start of silence */
		e = s + sc->sc_sil_count; /* end of silence, may be beyond end */
		p = inp;	/* adjusted pointer to area to fill */
		if (p < s)
			p += cb->end - cb->start;
		q = p+cc;
		/* Check if there is already silence. */
		if (!(s <= p && p <  e &&
		      s <= q && q <= e)) {
			if (s <= p)
				sc->sc_sil_count = max(sc->sc_sil_count, q - s);
#ifdef AUDIO_DEBUG
			if (audiodebug > 2)
				printf("audio_pint_silence: fill cc=%d inp=%p, count=%d size=%d\n", 
				       cc, inp, sc->sc_sil_count, (int)(cb->end - cb->start));
#endif
			audio_fill_silence(&sc->sc_pparams, inp, cc);
		} else {
#ifdef AUDIO_DEBUG
			if (audiodebug > 2)
				printf("audio_pint_silence: already silent cc=%d inp=%p\n", cc, inp);
#endif
			
		}
	} else {
		sc->sc_sil_start = inp;
		sc->sc_sil_count = cc;
#ifdef AUDIO_DEBUG
		if (audiodebug > 2)
			printf("audio_pint_silence: start fill %p %d\n", inp, cc);
#endif
		audio_fill_silence(&sc->sc_pparams, inp, cc);
	}
}

/*
 * Called from HW driver module on completion of dma output.
 * Start output of new block, wrap in ring buffer if needed.
 * If no more buffers to play, output zero instead.
 * Do a wakeup if necessary.
 */
void
audio_pint(v)
	void *v;
{
	struct audio_softc *sc = v;
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_ringbuffer *cb = &sc->sc_pr;
	u_char *inp;
	int cc, ccr;
	int error;

        if (!sc->sc_open)
        	return;         /* ignore interrupt if not open */

	cb->outp += cb->blksize;
	if (cb->outp >= cb->end)
		cb->outp = cb->start;
	cb->stamp += cb->blksize / sc->sc_pparams.factor;
	if (cb->mmapped) {
#ifdef AUDIO_DEBUG
		if (audiodebug > 2)
			printf("audio_pint: mmapped outp=%p cc=%d inp=%p\n", 
			       cb->outp, cb->blksize, cb->inp);
#endif
		(void)hw->start_output(sc->hw_hdl, cb->outp, cb->blksize,
				       audio_pint, (void *)sc);
		return;
	}
		
#ifdef AUDIO_INTR_TIME
	{
		struct timeval tv;
		u_long t;
		microtime(&tv);
		t = tv.tv_usec + 1000000 * tv.tv_sec;
		if (sc->sc_pnintr) {
			long lastdelta, totdelta;
			lastdelta = t - sc->sc_plastintr - sc->sc_pblktime;
			if (lastdelta > sc->sc_pblktime / 5) {
				printf("audio: play interrupt(%d) off relative by %ld us (%lu)\n", 
				       sc->sc_pnintr, lastdelta, sc->sc_pblktime);
			}
			totdelta = t - sc->sc_pfirstintr - sc->sc_pblktime * sc->sc_pnintr;
			if (totdelta > sc->sc_pblktime / 2) {
				sc->sc_pnintr++;
				printf("audio: play interrupt(%d) off absolute by %ld us (%lu)\n", 
				       sc->sc_pnintr, totdelta, sc->sc_pblktime);
				sc->sc_pnintr++; /* avoid repeated messages */
			}
		} else
			sc->sc_pfirstintr = t;
		sc->sc_plastintr = t;
		sc->sc_pnintr++;
	}
#endif

	cb->used -= cb->blksize;
	if (cb->used < cb->blksize) {
		/* we don't have a full block to use */
		if (cb->copying) {
			/* writer is in progress, don't disturb */
			cb->needfill = 1;
#ifdef AUDIO_DEBUG
			if (audiodebug > 1)
			    printf("audio_pint: copying in progress\n");
#endif
		} else {
			inp = cb->inp;
			cc = cb->blksize - (inp - cb->start) % cb->blksize;
			ccr = cc / sc->sc_pparams.factor;
			if (cb->pause)
				cb->pdrops += ccr;
			else {
				cb->drops += ccr;
				sc->sc_playdrop += ccr;
			}
			audio_pint_silence(sc, cb, inp, cc);
			inp += cc;
			if (inp >= cb->end)
				inp = cb->start;
			cb->inp = inp;
			cb->used += cc;

			/* Clear next block so we keep ahead of the DMA. */
			if (cb->used + cc < cb->usedhigh)
				audio_pint_silence(sc, cb, inp, cb->blksize);
		}
	}

#ifdef AUDIO_DEBUG
	if (audiodebug > 3)
		printf("audio_pint: outp=%p cc=%d\n", cb->outp, cb->blksize);
#endif
	error = hw->start_output(sc->hw_hdl, cb->outp, cb->blksize,
				 audio_pint, (void *)sc);
	if (error) {
		/* XXX does this really help? */
		DPRINTF(("audio_pint restart failed: %d\n", error));
		audio_clear(sc);
	}

#ifdef AUDIO_DEBUG
	if (audiodebug > 3)
		printf("audio_pint: mode=%d pause=%d used=%d lowat=%d\n",
			 sc->sc_mode, cb->pause, cb->used, cb->usedlow);
#endif
	if ((sc->sc_mode & AUMODE_PLAY) && !cb->pause) {
		if (cb->used <= cb->usedlow) {
			audio_wakeup(&sc->sc_wchan);
			selwakeup(&sc->sc_wsel);
			if (sc->sc_async_audio) {
#ifdef AUDIO_DEBUG
				if (audiodebug > 3)
					printf("audio_pint: sending SIGIO %p\n", 
					       sc->sc_async_audio);
#endif
				psignal(sc->sc_async_audio, SIGIO);
			}
		}
	}

	/* Possible to return one or more "phantom blocks" now. */
	if (!sc->sc_full_duplex && sc->sc_rchan) {
		audio_wakeup(&sc->sc_rchan);
		selwakeup(&sc->sc_rsel);
		if (sc->sc_async_audio)
			psignal(sc->sc_async_audio, SIGIO);
	}
}

/*
 * Called from HW driver module on completion of dma input.
 * Mark it as input in the ring buffer (fiddle pointers).
 * Do a wakeup if necessary.
 */
void
audio_rint(v)
	void *v;
{
	struct audio_softc *sc = v;
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_ringbuffer *cb = &sc->sc_rr;
	int error;

        if (!sc->sc_open)
        	return;         /* ignore interrupt if not open */

	cb->inp += cb->blksize;
	if (cb->inp >= cb->end)
		cb->inp = cb->start;
	cb->stamp += cb->blksize;
	if (cb->mmapped) {
#ifdef AUDIO_DEBUG
		if (audiodebug > 2)
			printf("audio_rint: mmapped inp=%p cc=%d\n", 
			       cb->inp, cb->blksize);
#endif
		(void)hw->start_input(sc->hw_hdl, cb->inp, cb->blksize,
				      audio_rint, (void *)sc);
		return;
	}

#ifdef AUDIO_INTR_TIME
	{
		struct timeval tv;
		u_long t;
		microtime(&tv);
		t = tv.tv_usec + 1000000 * tv.tv_sec;
		if (sc->sc_rnintr) {
			long lastdelta, totdelta;
			lastdelta = t - sc->sc_rlastintr - sc->sc_rblktime;
			if (lastdelta > sc->sc_rblktime / 5) {
				printf("audio: record interrupt(%d) off relative by %ld us (%lu)\n", 
				       sc->sc_rnintr, lastdelta, sc->sc_rblktime);
			}
			totdelta = t - sc->sc_rfirstintr - sc->sc_rblktime * sc->sc_rnintr;
			if (totdelta > sc->sc_rblktime / 2) {
				sc->sc_rnintr++;
				printf("audio: record interrupt(%d) off absolute by %ld us (%lu)\n", 
				       sc->sc_rnintr, totdelta, sc->sc_rblktime);
				sc->sc_rnintr++; /* avoid repeated messages */
			}
		} else
			sc->sc_rfirstintr = t;
		sc->sc_rlastintr = t;
		sc->sc_rnintr++;
	}
#endif

	cb->used += cb->blksize;
	if (cb->pause) {
#ifdef AUDIO_DEBUG
		if (audiodebug > 1)
			printf("audio_rint: pdrops %lu\n", cb->pdrops);
#endif
		cb->pdrops += cb->blksize;
		cb->outp += cb->blksize;
		cb->used -= cb->blksize;
	} else if (cb->used + cb->blksize >= cb->usedhigh && !cb->copying) {
#ifdef AUDIO_DEBUG
		if (audiodebug > 1)
			printf("audio_rint: drops %lu\n", cb->drops);
#endif
		cb->drops += cb->blksize;
		cb->outp += cb->blksize;
		cb->used -= cb->blksize;
	}

#ifdef AUDIO_DEBUG
	if (audiodebug > 2)
		printf("audio_rint: inp=%p cc=%d used=%d\n", 
		       cb->inp, cb->blksize, cb->used);
#endif
	error = hw->start_input(sc->hw_hdl, cb->inp, cb->blksize,
				audio_rint, (void *)sc);
	if (error) {
		/* XXX does this really help? */
		DPRINTF(("audio_rint: restart failed: %d\n", error));
		audio_clear(sc);
	}

	audio_wakeup(&sc->sc_rchan);
	selwakeup(&sc->sc_rsel);
	if (sc->sc_async_audio)
		psignal(sc->sc_async_audio, SIGIO);
}

int
audio_check_params(p)
	struct audio_params *p;
{
#if defined(COMPAT_12)
	if (p->encoding == AUDIO_ENCODING_PCM16) {
		if (p->precision == 8)
			p->encoding = AUDIO_ENCODING_ULINEAR;
		else
			p->encoding = AUDIO_ENCODING_SLINEAR;
	} else if (p->encoding == AUDIO_ENCODING_PCM8) {
		if (p->precision == 8)
			p->encoding = AUDIO_ENCODING_ULINEAR;
		else
			return EINVAL;
	}
#endif

	if (p->encoding == AUDIO_ENCODING_SLINEAR)
#if BYTE_ORDER == LITTLE_ENDIAN
		p->encoding = AUDIO_ENCODING_SLINEAR_LE;
#else
		p->encoding = AUDIO_ENCODING_SLINEAR_BE;
#endif
	if (p->encoding == AUDIO_ENCODING_ULINEAR)
#if BYTE_ORDER == LITTLE_ENDIAN
		p->encoding = AUDIO_ENCODING_ULINEAR_LE;
#else
		p->encoding = AUDIO_ENCODING_ULINEAR_BE;
#endif

	switch (p->encoding) {
	case AUDIO_ENCODING_ULAW:
	case AUDIO_ENCODING_ALAW:
	case AUDIO_ENCODING_ADPCM:
		if (p->precision != 8)
			return (EINVAL);
		break;
	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_SLINEAR_BE:
	case AUDIO_ENCODING_ULINEAR_LE:
	case AUDIO_ENCODING_ULINEAR_BE:
		if (p->precision != 8 && p->precision != 16)
			return (EINVAL);
		break;
	case AUDIO_ENCODING_MPEG_L1_STREAM:
	case AUDIO_ENCODING_MPEG_L1_PACKETS:
	case AUDIO_ENCODING_MPEG_L1_SYSTEM:
	case AUDIO_ENCODING_MPEG_L2_STREAM:
	case AUDIO_ENCODING_MPEG_L2_PACKETS:
	case AUDIO_ENCODING_MPEG_L2_SYSTEM:
		break;
	default:
		return (EINVAL);
	}

	if (p->channels < 1 || p->channels > 8)	/* sanity check # of channels */
		return (EINVAL);

	return (0);
}

int
au_set_lr_value(sc, ct, l, r)
	struct	audio_softc *sc;
	mixer_ctrl_t *ct;
	int l, r;
{
	ct->type = AUDIO_MIXER_VALUE;
	ct->un.value.num_channels = 2;
	ct->un.value.level[AUDIO_MIXER_LEVEL_LEFT] = l;
	ct->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
	if (sc->hw_if->set_port(sc->hw_hdl, ct) == 0)
		return 0;
	ct->un.value.num_channels = 1;
	ct->un.value.level[AUDIO_MIXER_LEVEL_MONO] = (l+r)/2;
	return sc->hw_if->set_port(sc->hw_hdl, ct);
}

int
au_set_gain(sc, ports, gain, balance)
	struct	audio_softc *sc;
	struct	au_mixer_ports *ports;
	int	gain;
	int	balance;
{
	mixer_ctrl_t ct;
	int i, error;
	int l, r;
	u_int mask;
	int nset;

	if (balance == AUDIO_MID_BALANCE) {
		l = r = gain;
	} else if (balance < AUDIO_MID_BALANCE) {
		r = gain;
		l = (balance * gain) / AUDIO_MID_BALANCE;
	} else {
		l = gain;
		r = ((AUDIO_RIGHT_BALANCE - balance) * gain)
		    / AUDIO_MID_BALANCE;
	}
	DPRINTF(("au_set_gain: gain=%d balance=%d, l=%d r=%d\n",
		 gain, balance, l, r));

	if (ports->index == -1) {
	usemaster:
		if (ports->master == -1)
			return 0; /* just ignore it silently */
		ct.dev = ports->master;
		error = au_set_lr_value(sc, &ct, l, r);
	} else {
		ct.dev = ports->index;
		if (ports->isenum) {
			ct.type = AUDIO_MIXER_ENUM;
			error = sc->hw_if->get_port(sc->hw_hdl, &ct);
			if (error)
				return error;
			for(i = 0; i < ports->nports; i++) {
				if (ports->misel[i] == ct.un.ord) {
					ct.dev = ports->miport[i];
					if (ct.dev == -1 ||
					    au_set_lr_value(sc, &ct, l, r))
						goto usemaster;
					else
						break;
				}
			}
		} else {
			ct.type = AUDIO_MIXER_SET;
			error = sc->hw_if->get_port(sc->hw_hdl, &ct);
			if (error)
				return error;
			mask = ct.un.mask;
			nset = 0;
			for(i = 0; i < ports->nports; i++) {
				if (ports->misel[i] & mask) {
				    ct.dev = ports->miport[i];
				    if (ct.dev != -1 &&
					au_set_lr_value(sc, &ct, l, r) == 0)
					    nset++;
				}
			}
			if (nset == 0)
				goto usemaster;
		}
	}
	if (!error)
		mixer_signal(sc);
	return error;
}

int
au_get_lr_value(sc, ct, l, r)
	struct	audio_softc *sc;
	mixer_ctrl_t *ct;
	int *l, *r;
{
	int error;

	ct->un.value.num_channels = 2;
	if (sc->hw_if->get_port(sc->hw_hdl, ct) == 0) {
		*l = ct->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
		*r = ct->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
	} else {
		ct->un.value.num_channels = 1;
		error = sc->hw_if->get_port(sc->hw_hdl, ct);
		if (error)
			return error;
		*r = *l = ct->un.value.level[AUDIO_MIXER_LEVEL_MONO];
	}
	return 0;
}

void
au_get_gain(sc, ports, pgain, pbalance)
	struct	audio_softc *sc;
	struct	au_mixer_ports *ports;
	u_int	*pgain;
	u_char	*pbalance;
{
	mixer_ctrl_t ct;
	int i, l, r, n;
	int lgain = AUDIO_MAX_GAIN/2, rgain = AUDIO_MAX_GAIN/2;

	if (ports->index == -1) {
	usemaster:
		if (ports->master == -1)
			goto bad;
		ct.dev = ports->master;
		ct.type = AUDIO_MIXER_VALUE;
		if (au_get_lr_value(sc, &ct, &lgain, &rgain))
			goto bad;
	} else {
		ct.dev = ports->index;
		if (ports->isenum) {
			ct.type = AUDIO_MIXER_ENUM;
			if (sc->hw_if->get_port(sc->hw_hdl, &ct))
				goto bad;
			ct.type = AUDIO_MIXER_VALUE;
			for(i = 0; i < ports->nports; i++) {
				if (ports->misel[i] == ct.un.ord) {
					ct.dev = ports->miport[i];
					if (ct.dev == -1 ||
					    au_get_lr_value(sc, &ct, 
							    &lgain, &rgain))
						goto usemaster;
					else
						break;
				}
			}
		} else {
			ct.type = AUDIO_MIXER_SET;
			if (sc->hw_if->get_port(sc->hw_hdl, &ct))
				goto bad;
			ct.type = AUDIO_MIXER_VALUE;
			lgain = rgain = n = 0;
			for(i = 0; i < ports->nports; i++) {
				if (ports->misel[i] & ct.un.mask) {
					ct.dev = ports->miport[i];
					if (ct.dev == -1 ||
					    au_get_lr_value(sc, &ct, &l, &r))
						goto usemaster;
					else {
						lgain += l;
						rgain += r;
						n++;
					}
				}
			}
			if (n != 0) {
				lgain /= n;
				rgain /= n;
			}
		}
	}
bad:
	if (lgain == rgain) {	/* handles lgain==rgain==0 */
		*pgain = lgain;
		*pbalance = AUDIO_MID_BALANCE;
	} else if (lgain < rgain) {
		*pgain = rgain;
		*pbalance = (AUDIO_MID_BALANCE * lgain) / rgain;
	} else /* lgain > rgain */ {
		*pgain = lgain;
		*pbalance = AUDIO_RIGHT_BALANCE -
			    (AUDIO_MID_BALANCE * rgain) / lgain;
	}
}

int
au_set_port(sc, ports, port)
	struct	audio_softc *sc;
	struct	au_mixer_ports *ports;
	u_int	port;
{
	mixer_ctrl_t ct;
	int i, error;

	if (port == 0 && ports->allports == 0)
		return 0;	/* allow this special case */

	if (ports->index == -1)
		return EINVAL;
	ct.dev = ports->index;
	if (ports->isenum) {
		if (port & (port-1))
			return EINVAL; /* Only one port allowed */
		ct.type = AUDIO_MIXER_ENUM;
		error = EINVAL;
		for(i = 0; i < ports->nports; i++)
			if (ports->aumask[i] == port) {
				ct.un.ord = ports->misel[i];
				error = sc->hw_if->set_port(sc->hw_hdl, &ct);
				break;
			}
	} else {
		ct.type = AUDIO_MIXER_SET;
		ct.un.mask = 0;
		for(i = 0; i < ports->nports; i++)
			if (ports->aumask[i] & port) 
				ct.un.mask |= ports->misel[i];
		if (port != 0 && ct.un.mask == 0)
			error = EINVAL;
		else
			error = sc->hw_if->set_port(sc->hw_hdl, &ct);
	}
	if (!error)
		mixer_signal(sc);
	return error;
}

int
au_get_port(sc, ports)
	struct	audio_softc *sc;
	struct	au_mixer_ports *ports;
{
	mixer_ctrl_t ct;
	int i, aumask;

	if (ports->index == -1)
		return 0;
	ct.dev = ports->index;
	ct.type = ports->isenum ? AUDIO_MIXER_ENUM : AUDIO_MIXER_SET;
	if (sc->hw_if->get_port(sc->hw_hdl, &ct))
		return 0;
	aumask = 0;
	if (ports->isenum) {
		for(i = 0; i < ports->nports; i++)
			if (ct.un.ord == ports->misel[i])
				aumask = ports->aumask[i];
	} else {
		for(i = 0; i < ports->nports; i++)
			if (ct.un.mask & ports->misel[i])
				aumask |= ports->aumask[i];
	}
	return aumask;
}

int
audiosetinfo(sc, ai)
	struct audio_softc *sc;
	struct audio_info *ai;
{
	struct audio_prinfo *r = &ai->record, *p = &ai->play;
	int cleared;
	int s, setmode;
	int error;
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_params pp, rp;
	int np, nr;
	unsigned int blks;
	int oldpblksize, oldrblksize;
	int rbus, pbus;
	u_int gain;
	u_char balance;
	
	if (hw == 0)		/* HW has not attached */
		return(ENXIO);

	rbus = sc->sc_rbus;
	pbus = sc->sc_pbus;
	error = 0;
	cleared = 0;

	pp = sc->sc_pparams;	/* Temporary encoding storage in */
	rp = sc->sc_rparams;	/* case setting the modes fails. */
	nr = np = 0;

	if (p->sample_rate != ~0) {
		pp.sample_rate = p->sample_rate;
		np++;
	}
	if (r->sample_rate != ~0) {
		rp.sample_rate = r->sample_rate;
		nr++;
	}
	if (p->encoding != ~0) {
		pp.encoding = p->encoding;
		np++;
	}	
	if (r->encoding != ~0) {
		rp.encoding = r->encoding;
		nr++;
	}
	if (p->precision != ~0) {
		pp.precision = p->precision;
		np++;
	}
	if (r->precision != ~0) {
		rp.precision = r->precision;
		nr++;
	}
	if (p->channels != ~0) {
		pp.channels = p->channels;
		np++;
	}
	if (r->channels != ~0) {
		rp.channels = r->channels;
		nr++;
	}
#ifdef AUDIO_DEBUG
	if (audiodebug && nr)
	    audio_print_params("Setting record params", &rp);
	if (audiodebug && np)
	    audio_print_params("Setting play params", &pp);
#endif
	if (nr && (error = audio_check_params(&rp)))
		return error;
	if (np && (error = audio_check_params(&pp)))
		return error;
	setmode = 0;
	if (nr) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		rp.sw_code = 0;
		rp.factor = 1;
		setmode |= AUMODE_RECORD;
	}
	if (np) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		pp.sw_code = 0;
		pp.factor = 1;
		setmode |= AUMODE_PLAY;
	}

	if (ai->mode != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		sc->sc_mode = ai->mode;
		if (sc->sc_mode & AUMODE_PLAY_ALL)
			sc->sc_mode |= AUMODE_PLAY;
		if ((sc->sc_mode & AUMODE_PLAY) && !sc->sc_full_duplex)
			/* Play takes precedence */
			sc->sc_mode &= ~AUMODE_RECORD;
	}

	if (setmode) {
		int indep = hw->get_props(sc->hw_hdl) & AUDIO_PROP_INDEPENDENT;
		if (!indep) {
			if (setmode == AUMODE_RECORD)
				pp = rp;
			else if (setmode == AUMODE_PLAY)
				rp = pp;
		}
		error = hw->set_params(sc->hw_hdl, setmode, sc->sc_mode, &pp, &rp);
		if (error)
			return (error);
		if (!indep) {
			if (setmode == AUMODE_RECORD) {
				pp.sample_rate = rp.sample_rate;
				pp.encoding    = rp.encoding;
				pp.channels    = rp.channels;
				pp.precision   = rp.precision;
			} else if (setmode == AUMODE_PLAY) {
				rp.sample_rate = pp.sample_rate;
				rp.encoding    = pp.encoding;
				rp.channels    = pp.channels;
				rp.precision   = pp.precision;
			}
		}
		if (setmode & AUMODE_RECORD)
			sc->sc_rparams = rp;
		if (setmode & AUMODE_PLAY)
			sc->sc_pparams = pp;
	}

	oldpblksize = sc->sc_pr.blksize;
	oldrblksize = sc->sc_rr.blksize;
	/* Play params can affect the record params, so recalculate blksize. */
	if (nr || np) {
		audio_calc_blksize(sc, AUMODE_RECORD);
		audio_calc_blksize(sc, AUMODE_PLAY);
	}
#ifdef AUDIO_DEBUG
	if (audiodebug > 1 && nr)
	    audio_print_params("After setting record params", &sc->sc_rparams);
	if (audiodebug > 1 && np)
	    audio_print_params("After setting play params", &sc->sc_pparams);
#endif

	if (p->port != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;

		error = au_set_port(sc, &sc->sc_outports, p->port);
		if (error)
			return(error);
	}
	if (r->port != ~0) {
		if (!cleared)
			audio_clear(sc);
		cleared = 1;

		error = au_set_port(sc, &sc->sc_inports, r->port);
		if (error)
			return(error);
	}
	if (p->gain != ~0) {
		au_get_gain(sc, &sc->sc_outports, &gain, &balance);
		error = au_set_gain(sc, &sc->sc_outports, p->gain, balance);
		if (error)
			return(error);
	}
	if (r->gain != ~0) {
		au_get_gain(sc, &sc->sc_inports, &gain, &balance);
		error = au_set_gain(sc, &sc->sc_inports, r->gain, balance);
		if (error)
			return(error);
	}
	
	if (p->balance != (u_char)~0) {
		au_get_gain(sc, &sc->sc_outports, &gain, &balance);
		error = au_set_gain(sc, &sc->sc_outports, gain, p->balance);
		if (error)
			return(error);
	}
	if (r->balance != (u_char)~0) {
		au_get_gain(sc, &sc->sc_inports, &gain, &balance);
		error = au_set_gain(sc, &sc->sc_inports, gain, r->balance);
		if (error)
			return(error);
	}

	if (ai->monitor_gain != ~0 &&
	    sc->sc_monitor_port != -1) {
		mixer_ctrl_t ct;
		
		ct.dev = sc->sc_monitor_port;
		ct.type = AUDIO_MIXER_VALUE;
		ct.un.value.num_channels = 1;
		ct.un.value.level[AUDIO_MIXER_LEVEL_MONO] = ai->monitor_gain;
		error = sc->hw_if->get_port(sc->hw_hdl, &ct);
		if (error)
			return(error);
	}
	
	if (p->pause != (u_char)~0) {
		sc->sc_pr.pause = p->pause;
		if (!p->pause && !sc->sc_pbus && (sc->sc_mode & AUMODE_PLAY)) {
			s = splaudio();
			error = audiostartp(sc);
			splx(s);
			if (error)
				return error;
		}
	}
	if (r->pause != (u_char)~0) {
		sc->sc_rr.pause = r->pause;
		if (!r->pause && !sc->sc_rbus && (sc->sc_mode & AUMODE_RECORD)) {
			s = splaudio();
			error = audiostartr(sc);
			splx(s);
			if (error)
				return error;
		}
	}

	if (ai->blocksize != ~0) {
		/* Block size specified explicitly. */
		if (!cleared)
			audio_clear(sc);
		cleared = 1;

		if (ai->blocksize == 0) {
			audio_calc_blksize(sc, AUMODE_RECORD);
			audio_calc_blksize(sc, AUMODE_PLAY);
			sc->sc_blkset = 0;
		} else {
			int bs = ai->blocksize;
			if (hw->round_blocksize)
				bs = hw->round_blocksize(sc->hw_hdl, bs);
			sc->sc_pr.blksize = sc->sc_rr.blksize = bs;
			sc->sc_blkset = 1;
		}
	}

	if (ai->mode != ~0) {
		if (sc->sc_mode & AUMODE_PLAY)
			audio_init_play(sc);
		if (sc->sc_mode & AUMODE_RECORD)
			audio_init_record(sc);
	}

	if (hw->commit_settings) {
		error = hw->commit_settings(sc->hw_hdl);
		if (error)
			return (error);
	}

	if (cleared) {
		s = splaudio();
		error = audio_initbufs(sc);
		if (error) goto err;
		if (sc->sc_pr.blksize != oldpblksize ||
		    sc->sc_rr.blksize != oldrblksize)
			audio_calcwater(sc);
		if ((sc->sc_mode & AUMODE_PLAY) &&
		    pbus && !sc->sc_pbus)
			error = audiostartp(sc);
		if (!error && 
		    (sc->sc_mode & AUMODE_RECORD) &&
		    rbus && !sc->sc_rbus)
			error = audiostartr(sc);
	err:
		splx(s);
		if (error)
			return error;
	}
	
	/* Change water marks after initializing the buffers. */
	if (ai->hiwat != ~0) {
		blks = ai->hiwat;
		if (blks > sc->sc_pr.maxblks)
			blks = sc->sc_pr.maxblks;
		if (blks < 2)
			blks = 2;
		sc->sc_pr.usedhigh = blks * sc->sc_pr.blksize;
	}
	if (ai->lowat != ~0) {
		blks = ai->lowat;
		if (blks > sc->sc_pr.maxblks - 1)
			blks = sc->sc_pr.maxblks - 1;
		sc->sc_pr.usedlow = blks * sc->sc_pr.blksize;
	}
	if (ai->hiwat != ~0 || ai->lowat != ~0) {
		if (sc->sc_pr.usedlow > sc->sc_pr.usedhigh - sc->sc_pr.blksize)
			sc->sc_pr.usedlow = sc->sc_pr.usedhigh - sc->sc_pr.blksize;
	}

	return (0);
}

int
audiogetinfo(sc, ai)
	struct audio_softc *sc;
	struct audio_info *ai;
{
	struct audio_prinfo *r = &ai->record, *p = &ai->play;
	struct audio_hw_if *hw = sc->hw_if;
	
	if (hw == 0)		/* HW has not attached */
		return(ENXIO);
	
	p->sample_rate = sc->sc_pparams.sample_rate;
	r->sample_rate = sc->sc_rparams.sample_rate;
	p->channels = sc->sc_pparams.channels;
	r->channels = sc->sc_rparams.channels;
	p->precision = sc->sc_pparams.precision;
	r->precision = sc->sc_rparams.precision;
	p->encoding = sc->sc_pparams.encoding;
	r->encoding = sc->sc_rparams.encoding;

	r->port = au_get_port(sc, &sc->sc_inports);
	p->port = au_get_port(sc, &sc->sc_outports);

	r->avail_ports = sc->sc_inports.allports;
	p->avail_ports = sc->sc_outports.allports;

	au_get_gain(sc, &sc->sc_inports,  &r->gain, &r->balance);
	au_get_gain(sc, &sc->sc_outports, &p->gain, &p->balance);

	if (sc->sc_monitor_port != -1) {
		mixer_ctrl_t ct;
		
		ct.dev = sc->sc_monitor_port;
		ct.type = AUDIO_MIXER_VALUE;
		ct.un.value.num_channels = 1;
		if (sc->hw_if->get_port(sc->hw_hdl, &ct))
			ai->monitor_gain = 0;
		else
			ai->monitor_gain = 
				ct.un.value.level[AUDIO_MIXER_LEVEL_MONO];
	} else
		ai->monitor_gain = 0;

	p->seek = sc->sc_pr.used;
	r->seek = sc->sc_rr.used;

	p->samples = sc->sc_pr.stamp - sc->sc_pr.drops;
	r->samples = sc->sc_rr.stamp - sc->sc_rr.drops;

	p->eof = sc->sc_eof;
	r->eof = 0;

	p->pause = sc->sc_pr.pause;
	r->pause = sc->sc_rr.pause;

	p->error = sc->sc_pr.drops != 0;
	r->error = sc->sc_rr.drops != 0;

	p->waiting = r->waiting = 0;		/* open never hangs */

	p->open = (sc->sc_open & AUOPEN_WRITE) != 0;
	r->open = (sc->sc_open & AUOPEN_READ) != 0;

	p->active = sc->sc_pbus;
	r->active = sc->sc_rbus;

	p->buffer_size = sc->sc_pr.bufsize;
	r->buffer_size = sc->sc_rr.bufsize;

	ai->blocksize = sc->sc_pr.blksize;
	ai->hiwat = sc->sc_pr.usedhigh / sc->sc_pr.blksize;
	ai->lowat = sc->sc_pr.usedlow / sc->sc_pr.blksize;
	ai->mode = sc->sc_mode;

	return (0);
}

/*
 * Mixer driver
 */
int
mixer_open(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;

	if (unit >= audio_cd.cd_ndevs ||
	    (sc = audio_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	if (!sc->hw_if)
		return (ENXIO);

	DPRINTF(("mixer_open: dev=0x%x flags=0x%x sc=%p\n", dev, flags, sc));

	return (0);
}

/*
 * Remove a process from those to be signalled on mixer activity.
 */
static void
mixer_remove(sc, p)
	struct audio_softc *sc;
	struct proc *p;
{
	struct mixer_asyncs **pm, *m;

	for(pm = &sc->sc_async_mixer; *pm; pm = &(*pm)->next) {
		if ((*pm)->proc == p) {
			m = *pm;
			*pm = m->next;
			free(m, M_DEVBUF);
			return;
		}
	}
}

/*
 * Signal all processes waiting for the mixer.
 */
static void
mixer_signal(sc)
	struct audio_softc *sc;
{
	struct mixer_asyncs *m;

	for(m = sc->sc_async_mixer; m; m = m->next)
		psignal(m->proc, SIGIO);
}

/*
 * Close a mixer device
 */
/* ARGSUSED */
int
mixer_close(dev, flags, ifmt, p)
	dev_t dev;
	int flags, ifmt;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];

	DPRINTF(("mixer_close: unit %d\n", AUDIOUNIT(dev)));
	
	mixer_remove(sc, p);

	return (0);
}

int
mixer_ioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_hw_if *hw = sc->hw_if;
	int error = EINVAL;

	DPRINTF(("mixer_ioctl(%d,'%c',%d)\n",
		 IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff));

	switch (cmd) {
	case FIOASYNC:
		mixer_remove(sc, p); /* remove old entry */
		if (*(int *)addr) {
			struct mixer_asyncs *ma;
			ma = malloc(sizeof (struct mixer_asyncs), M_DEVBUF, M_WAITOK);
			ma->next = sc->sc_async_mixer;
			ma->proc = p;
			sc->sc_async_mixer = ma;
		}
		error = 0;
		break;

	case AUDIO_GETDEV:
		DPRINTF(("AUDIO_GETDEV\n"));
		error = hw->getdev(sc->hw_hdl, (audio_device_t *)addr);
		break;
		
	case AUDIO_MIXER_DEVINFO:
		DPRINTF(("AUDIO_MIXER_DEVINFO\n"));
		error = hw->query_devinfo(sc->hw_hdl, (mixer_devinfo_t *)addr);
		break;

	case AUDIO_MIXER_READ:
		DPRINTF(("AUDIO_MIXER_READ\n"));
		error = hw->get_port(sc->hw_hdl, (mixer_ctrl_t *)addr);
		break;

	case AUDIO_MIXER_WRITE:
		DPRINTF(("AUDIO_MIXER_WRITE\n"));
		error = hw->set_port(sc->hw_hdl, (mixer_ctrl_t *)addr);
		if (!error && hw->commit_settings)
			error = hw->commit_settings(sc->hw_hdl);
		if (!error)
			mixer_signal(sc);
		break;

	default:
		error = EINVAL;
		break;
	}
	DPRINTF(("mixer_ioctl(%d,'%c',%d) result %d\n",
		 IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff, error));
	return (error);
}
#endif
