/*	$OpenBSD: audio.c,v 1.112 2011/06/06 06:13:45 deraadt Exp $	*/
/*	$NetBSD: audio.c,v 1.119 1999/11/09 16:50:47 augustss Exp $	*/

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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/selinfo.h>
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
#include <sys/workq.h>

#include <dev/audio_if.h>
#include <dev/audiovar.h>

#include <dev/rndvar.h>

#include <machine/endian.h>

#include "wskbd.h"	/* NWSKBD (mixer tuning using keyboard) */

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (audiodebug) printf x
#define DPRINTFN(n,x)	if (audiodebug>(n)) printf x
int	audiodebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define ROUNDSIZE(x) x &= -16	/* round to nice boundary */

int	audio_blk_ms = AUDIO_BLK_MS;

int	audiosetinfo(struct audio_softc *, struct audio_info *);
int	audiogetinfo(struct audio_softc *, struct audio_info *);
int	audiogetbufinfo(struct audio_softc *, struct audio_bufinfo *, int);
int	audio_open(dev_t, struct audio_softc *, int, int, struct proc *);
int	audio_close(dev_t, int, int, struct proc *);
int	audio_read(dev_t, struct uio *, int);
int	audio_write(dev_t, struct uio *, int);
int	audio_ioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	audio_poll(dev_t, int, struct proc *);
paddr_t	audio_mmap(dev_t, off_t, int);

int	mixer_open(dev_t, struct audio_softc *, int, int, struct proc *);
int	mixer_close(dev_t, int, int, struct proc *);
int	mixer_ioctl(dev_t, u_long, caddr_t, int, struct proc *);
static	void mixer_remove(struct audio_softc *, struct proc *p);
static	void mixer_signal(struct audio_softc *);

void	audio_init_record(struct audio_softc *);
void	audio_init_play(struct audio_softc *);
int	audiostartr(struct audio_softc *);
int	audiostartp(struct audio_softc *);
void	audio_rint(void *);
void	audio_pint(void *);
int	audio_check_params(struct audio_params *);

void	audio_set_blksize(struct audio_softc *, int, int);
void	audio_calc_blksize(struct audio_softc *, int);
void	audio_fill_silence(struct audio_params *, u_char *, u_char *, int);
int	audio_silence_copyout(struct audio_softc *, int, struct uio *);

void	audio_init_ringbuffer(struct audio_ringbuffer *);
int	audio_initbufs(struct audio_softc *);
void	audio_calcwater(struct audio_softc *);
static __inline int audio_sleep_timo(int *, char *, int);
static __inline int audio_sleep(int *, char *);
static __inline void audio_wakeup(int *);
void	audio_selwakeup(struct audio_softc *sc, int play);
int	audio_drain(struct audio_softc *);
void	audio_clear(struct audio_softc *);
static __inline void audio_pint_silence(struct audio_softc *, struct audio_ringbuffer *, u_char *, int);

int	audio_quiesce(struct audio_softc *);
void	audio_resume(struct audio_softc *);
void	audio_resume_to(void *);
void	audio_resume_task(void *, void *);

int	audio_alloc_ring(struct audio_softc *, struct audio_ringbuffer *, int, int);
void	audio_free_ring(struct audio_softc *, struct audio_ringbuffer *);

int	audioprint(void *, const char *);

int	audioprobe(struct device *, void *, void *);
void	audioattach(struct device *, struct device *, void *);
int	audiodetach(struct device *, int);
int	audioactivate(struct device *, int);

struct portname {
	char	*name;
	int	mask;
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
struct gainpref {
	char *class, *device;
};
static struct gainpref ipreftab[] = {
	{ AudioCinputs, AudioNvolume },
	{ AudioCinputs, AudioNinput  },
	{ AudioCinputs, AudioNrecord },
	{ AudioCrecord, AudioNvolume },
	{ AudioCrecord, AudioNrecord },
	{ NULL, NULL}
};
static struct gainpref opreftab[] = {
	{ AudioCoutputs, AudioNoutput },
	{ AudioCoutputs, AudioNdac },
	{ AudioCinputs,  AudioNdac },
	{ AudioCoutputs, AudioNmaster },
	{ NULL, NULL}
};
static struct gainpref mpreftab[] = {
	{ AudioCoutputs, AudioNmonitor },
	{ AudioCmonitor, AudioNmonitor },
	{ NULL, NULL}
};

void	au_gain_match(struct audio_softc *, struct gainpref *, 
		mixer_devinfo_t *, mixer_devinfo_t *, int *, int *);			    
void	au_check_ports(struct audio_softc *, struct au_mixer_ports *,
		            mixer_devinfo_t *, mixer_devinfo_t *, 
		            char *, char *, struct portname *);
int	au_set_gain(struct audio_softc *, struct au_mixer_ports *,
			 int, int);
void	au_get_gain(struct audio_softc *, struct au_mixer_ports *,
			 u_int *, u_char *);
int	au_set_port(struct audio_softc *, struct au_mixer_ports *,
			 u_int);
int	au_get_port(struct audio_softc *, struct au_mixer_ports *);
int	au_set_mute(struct audio_softc *, struct au_mixer_ports *, u_char);
int	au_get_mute(struct audio_softc *, struct au_mixer_ports *, u_char *);
int	au_get_lr_value(struct audio_softc *, mixer_ctrl_t *,
			     int *, int *r);
int	au_set_lr_value(struct audio_softc *, mixer_ctrl_t *,
			     int, int);
int	au_portof(struct audio_softc *, char *);


/* The default audio mode: 8 kHz mono ulaw */
struct audio_params audio_default =
	{ 8000, AUDIO_ENCODING_ULAW, 8, 1, 1, 1, 0, 1 };

struct cfattach audio_ca = {
	sizeof(struct audio_softc), audioprobe, audioattach,
	audiodetach, audioactivate
};

struct cfdriver audio_cd = {
	NULL, "audio", DV_DULL
};

void filt_audiowdetach(struct knote *);
int filt_audiowrite(struct knote *, long);

struct filterops audiowrite_filtops =
	{ 1, NULL, filt_audiowdetach, filt_audiowrite};

void filt_audiordetach(struct knote *);
int filt_audioread(struct knote *, long);

struct filterops audioread_filtops =
	{ 1, NULL, filt_audiordetach, filt_audioread};

#if NWSKBD > 0
/* Mixer manipulation using keyboard */
int wskbd_set_mixervolume(long, int);
#endif

int
audioprobe(struct device *parent, void *match, void *aux)
{
	struct audio_attach_args *sa = aux;

	DPRINTF(("audioprobe: type=%d sa=%p hw=%p\n",
		   sa->type, sa, sa->hwif));
	return (sa->type == AUDIODEV_TYPE_AUDIO) ? 1 : 0;
}

void
audioattach(struct device *parent, struct device *self, void *aux)
{
	struct audio_softc *sc = (void *)self;
	struct audio_attach_args *sa = aux;
	struct audio_hw_if *hwp = sa->hwif;
	void *hdlp = sa->hdl;
	int error;
	mixer_devinfo_t mi, cl;
	int ipref, opref, mpref;

	printf("\n");

#ifdef DIAGNOSTIC
	if (hwp == 0 ||
	    hwp->open == 0 ||
	    hwp->close == 0 ||
	    hwp->query_encoding == 0 ||
	    hwp->set_params == 0 ||
	    (hwp->start_output == 0 && hwp->trigger_output == 0) ||
	    (hwp->start_input == 0 && hwp->trigger_input == 0) ||
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
	sc->sc_async_mixer = NULL;

	error = audio_alloc_ring(sc, &sc->sc_pr, AUMODE_PLAY, AU_RING_SIZE);
	if (error) {
		sc->hw_if = 0;
		printf("audio: could not allocate play buffer\n");
		return;
	}
	error = audio_alloc_ring(sc, &sc->sc_rr, AUMODE_RECORD, AU_RING_SIZE);
	if (error) {
		audio_free_ring(sc, &sc->sc_pr);
		sc->hw_if = 0;
		printf("audio: could not allocate record buffer\n");
		return;
	}

	/*
	 * Set default softc params
	 */

	if (hwp->get_default_params) {
		hwp->get_default_params(hdlp, AUMODE_PLAY, &sc->sc_pparams);
		hwp->get_default_params(hdlp, AUMODE_RECORD, &sc->sc_rparams);
	} else {
		sc->sc_pparams = audio_default;
		sc->sc_rparams = audio_default;
	}

	/* Set up some default values */
	sc->sc_rr.blkset = sc->sc_pr.blkset = 0;
	audio_calc_blksize(sc, AUMODE_RECORD);
	audio_calc_blksize(sc, AUMODE_PLAY);
	audio_init_ringbuffer(&sc->sc_rr);
	audio_init_ringbuffer(&sc->sc_pr);
	audio_calcwater(sc);

	ipref = opref = mpref = -1;
	sc->sc_inports.index = -1;
	sc->sc_inports.nports = 0;
	sc->sc_inports.isenum = 0;
	sc->sc_inports.allports = 0;
	sc->sc_inports.master = -1;
	sc->sc_outports.index = -1;
	sc->sc_outports.nports = 0;
	sc->sc_outports.isenum = 0;
	sc->sc_outports.allports = 0;
	sc->sc_outports.master = -1;
	sc->sc_monitor_port = -1;
	for(mi.index = 0; ; mi.index++) {
		if (hwp->query_devinfo(hdlp, &mi) != 0)
			break;
		if (mi.type == AUDIO_MIXER_CLASS)
			continue;
		cl.index = mi.mixer_class;
		if (hwp->query_devinfo(hdlp, &cl) != 0)
			continue;

		au_gain_match(sc, ipreftab, &cl, &mi, &sc->sc_inports.master, &ipref);
		au_gain_match(sc, opreftab, &cl, &mi, &sc->sc_outports.master, &opref);
		au_gain_match(sc, mpreftab, &cl, &mi, &sc->sc_monitor_port, &mpref);

		au_check_ports(sc, &sc->sc_inports,  &cl, &mi,
		    AudioCrecord, AudioNsource, itable);
		au_check_ports(sc, &sc->sc_outports, &cl, &mi,
		    AudioCoutputs, AudioNselect, otable);
	}
	DPRINTF(("audio_attach: inputs ports=0x%x, output ports=0x%x\n",
		 sc->sc_inports.allports, sc->sc_outports.allports));

	timeout_set(&sc->sc_resume_to, audio_resume_to, sc);
}

int
audioactivate(struct device *self, int act)
{
	struct audio_softc *sc = (struct audio_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		break;
	case DVACT_QUIESCE:
		audio_quiesce(sc);
		break;
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		audio_resume(sc);
		break;
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}

int
audiodetach(struct device *self, int flags)
{
	struct audio_softc *sc = (struct audio_softc *)self;
	int maj, mn;
	int s;

	DPRINTF(("audio_detach: sc=%p flags=%d\n", sc, flags));

	sc->sc_dying = 1;

	timeout_del(&sc->sc_resume_to);
	wakeup(&sc->sc_quiesce);
	wakeup(&sc->sc_wchan);
	wakeup(&sc->sc_rchan);
	s = splaudio();
	if (--sc->sc_refcnt >= 0) {
		if (tsleep(&sc->sc_refcnt, PZERO, "auddet", hz * 120))
			printf("audiodetach: %s didn't detach\n",
			    sc->dev.dv_xname);
	}
	splx(s);

	/* free resources */
	audio_free_ring(sc, &sc->sc_pr);
	audio_free_ring(sc, &sc->sc_rr);

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == audioopen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn | SOUND_DEVICE,    mn | SOUND_DEVICE, VCHR);
	vdevgone(maj, mn | AUDIO_DEVICE,    mn | AUDIO_DEVICE, VCHR);
	vdevgone(maj, mn | AUDIOCTL_DEVICE, mn | AUDIOCTL_DEVICE, VCHR);
	vdevgone(maj, mn | MIXER_DEVICE,    mn | MIXER_DEVICE, VCHR);

	return (0);
}

int
au_portof(struct audio_softc *sc, char *name)
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
au_check_ports(struct audio_softc *sc, struct au_mixer_ports *ports,
    mixer_devinfo_t *cl, mixer_devinfo_t *mi, char *cname, char *mname, 
    struct portname *tbl)
{
	int i, j;

	if (strcmp(cl->label.name, cname) != 0 ||
	    strcmp(mi->label.name, mname) != 0)
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
 * check if the given (class, device) is better
 * than the current setting (*index), if so, set the
 * current setting.
 */
void
au_gain_match(struct audio_softc *sc, struct gainpref *tbl, 
    mixer_devinfo_t *cls, mixer_devinfo_t *dev, int *index, int *pref) 
{
	int i;

	for (i = *pref + 1; tbl[i].class != NULL; i++) {
		if (strcmp(tbl[i].class, cls->label.name) == 0 &&
		    strcmp(tbl[i].device, dev->label.name) == 0) {
			if (*pref < i) {
				DPRINTF(("au_gain_match: found %s.%s\n", 
				    cls->label.name, dev->label.name));
				*index = dev->index;
				*pref = i;
			}
			break;
		}
	}
}

/*
 * Called from hardware driver.  This is where the MI audio driver gets
 * probed/attached to the hardware driver.
 */
struct device *
audio_attach_mi(struct audio_hw_if *ahwp, void *hdlp, struct device *dev)
{
	struct audio_attach_args arg;

#ifdef DIAGNOSTIC
	if (ahwp == NULL) {
		printf ("audio_attach_mi: NULL\n");
		return 0;
	}
#endif

	arg.type = AUDIODEV_TYPE_AUDIO;
	arg.hwif = ahwp;
	arg.hdl = hdlp;
	return config_found(dev, &arg, audioprint);
}

int
audioprint(void *aux, const char *pnp)
{
	struct audio_attach_args *arg = aux;
	const char *type;

	if (pnp != NULL) {
		switch (arg->type) {
		case AUDIODEV_TYPE_AUDIO:
			type = "audio";
			break;
		case AUDIODEV_TYPE_OPL:
			type = "opl";
			break;
		case AUDIODEV_TYPE_MPU:
			type = "mpu";
			break;
		default:
			panic("audioprint: unknown type %d", arg->type);
		}
		printf("%s at %s", type, pnp);
	}
	return (UNCONF);
}

#ifdef AUDIO_DEBUG
void	audio_printsc(struct audio_softc *);
void	audio_print_params(char *, struct audio_params *);

void
audio_printsc(struct audio_softc *sc)
{
	printf("hwhandle %p hw_if %p ", sc->hw_hdl, sc->hw_if);
	printf("open 0x%x mode 0x%x\n", sc->sc_open, sc->sc_mode);
	printf("rchan 0x%x wchan 0x%x ", sc->sc_rchan, sc->sc_wchan);
	printf("rring used 0x%x pring used=%d\n", sc->sc_rr.used, sc->sc_pr.used);
	printf("rbus 0x%x pbus 0x%x ", sc->sc_rbus, sc->sc_pbus);
	printf("pblksz %d, rblksz %d", sc->sc_pr.blksize, sc->sc_rr.blksize);
	printf("hiwat %d lowat %d\n", sc->sc_pr.usedhigh, sc->sc_pr.usedlow);
}

void
audio_print_params(char *s, struct audio_params *p)
{
	printf("audio: %s sr=%ld, enc=%d, chan=%d, prec=%d bps=%d\n", s,
	    p->sample_rate, p->encoding, p->channels, p->precision, p->bps);
}
#endif

int
audio_alloc_ring(struct audio_softc *sc, struct audio_ringbuffer *r,
    int direction, int bufsize)
{
	struct audio_hw_if *hw = sc->hw_if;
	void *hdl = sc->hw_hdl;
	/*
	 * Alloc DMA play and record buffers
	 */
	if (bufsize < AUMINBUF)
		bufsize = AUMINBUF;
	ROUNDSIZE(bufsize);
	if (hw->round_buffersize)
		bufsize = hw->round_buffersize(hdl, direction, bufsize);
	r->bufsize = bufsize;
	if (hw->allocm)
		r->start = hw->allocm(hdl, direction, r->bufsize, M_DEVBUF,
		    M_WAITOK);
	else
		r->start = malloc(bufsize, M_DEVBUF, M_WAITOK);
	if (r->start == 0)
		return ENOMEM;
	return 0;
}

void
audio_free_ring(struct audio_softc *sc, struct audio_ringbuffer *r)
{
	if (sc->hw_if->freem) {
		sc->hw_if->freem(sc->hw_hdl, r->start, M_DEVBUF);
	} else {
		free(r->start, M_DEVBUF);
	}
}

int
audioopen(dev_t dev, int flags, int ifmt, struct proc *p)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	int error;

	if (unit >= audio_cd.cd_ndevs ||
	    (sc = audio_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	if (sc->sc_dying)
		return (EIO);

	if (!sc->hw_if)
		return (ENXIO);

	sc->sc_refcnt ++;
	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	case AUDIOCTL_DEVICE:
		error = audio_open(dev, sc, flags, ifmt, p);
		break;
	case MIXER_DEVICE:
		error = mixer_open(dev, sc, flags, ifmt, p);
		break;
	default:
		error = ENXIO;
		break;
	}

	if (--sc->sc_refcnt < 0)
		wakeup(&sc->sc_refcnt);

	return (error);
}

int
audioclose(dev_t dev, int flags, int ifmt, struct proc *p)
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
audioread(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	int error;

	if (unit >= audio_cd.cd_ndevs ||
	    (sc = audio_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt ++;
	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		error = audio_read(dev, uio, ioflag);
		break;
	case AUDIOCTL_DEVICE:
	case MIXER_DEVICE:
		error = ENODEV;
		break;
	default:
		error = ENXIO;
		break;
	}

	if (--sc->sc_refcnt < 0)
		wakeup(&sc->sc_refcnt);
	return (error);
}

int
audiowrite(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	int error;

	if (unit >= audio_cd.cd_ndevs ||
	    (sc = audio_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt ++;
	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		error = audio_write(dev, uio, ioflag);
		break;
	case AUDIOCTL_DEVICE:
	case MIXER_DEVICE:
		error = ENODEV;
		break;
	default:
		error = ENXIO;
		break;
	}

	if (--sc->sc_refcnt < 0)
		wakeup(&sc->sc_refcnt);
	return (error);
}

int
audioioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	int error;

	if (unit >= audio_cd.cd_ndevs ||
	    (sc = audio_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	if (sc->sc_dying)
		return (EIO);

	sc->sc_refcnt ++;
	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
	case AUDIOCTL_DEVICE:
		error = audio_ioctl(dev, cmd, addr, flag, p);
		break;
	case MIXER_DEVICE:
		error = mixer_ioctl(dev, cmd, addr, flag, p);
		break;
	default:
		error = ENXIO;
		break;
	}

	if (--sc->sc_refcnt < 0)
		wakeup(&sc->sc_refcnt);
	return (error);
}

int
audiopoll(dev_t dev, int events, struct proc *p)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	int error;

	if (unit >= audio_cd.cd_ndevs ||
	    (sc = audio_cd.cd_devs[unit]) == NULL)
		return POLLERR;

	if (sc->sc_dying)
		return POLLERR;

	sc->sc_refcnt ++;
	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		error = audio_poll(dev, events, p);
		break;
	case AUDIOCTL_DEVICE:
	case MIXER_DEVICE:
		error = 0;
		break;
	default:
		error = 0;
		break;
	}

	if (--sc->sc_refcnt < 0)
		wakeup(&sc->sc_refcnt);
	return (error);
}

paddr_t
audiommap(dev_t dev, off_t off, int prot)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc;
	int ret;

	if (unit >= audio_cd.cd_ndevs ||
	    (sc = audio_cd.cd_devs[unit]) == NULL)
		return (-1);

	if (sc->sc_dying)
		return (-1);

	sc->sc_refcnt ++;
	switch (AUDIODEV(dev)) {
	case SOUND_DEVICE:
	case AUDIO_DEVICE:
		ret = audio_mmap(dev, off, prot);
		break;
	case AUDIOCTL_DEVICE:
	case MIXER_DEVICE:
		ret = -1;
		break;
	default:
		ret = -1;
		break;
	}

	if (--sc->sc_refcnt < 0)
		wakeup(&sc->sc_refcnt);
	return (ret);
}

/*
 * Audio driver
 */
void
audio_init_ringbuffer(struct audio_ringbuffer *rp)
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
	rp->stamp_last = 0;
	rp->drops = 0;
	rp->pdrops = 0;
	rp->mmapped = 0;
}

int
audio_initbufs(struct audio_softc *sc)
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
	    (u_long)sc->sc_pr.blksize * 100000 /
	    (u_long)(sc->sc_pparams.bps *
		sc->sc_pparams.channels *
		sc->sc_pparams.sample_rate)) * 10;
	DPRINTF(("audio: play blktime = %lu for %d\n",
		 sc->sc_pblktime, sc->sc_pr.blksize));
	sc->sc_rnintr = 0;
	sc->sc_rblktime = (u_long)(
	    (u_long)sc->sc_rr.blksize * 100000 /
	    (u_long)(sc->sc_rparams.bps *
		sc->sc_rparams.channels *
		sc->sc_rparams.sample_rate)) * 10;
	DPRINTF(("audio: record blktime = %lu for %d\n",
		 sc->sc_rblktime, sc->sc_rr.blksize));
#endif

	return 0;
}

void
audio_calcwater(struct audio_softc *sc)
{
	int hiwat, lowat;

	hiwat = (sc->sc_pr.end - sc->sc_pr.start) / sc->sc_pr.blksize;
	lowat = hiwat * 3 / 4;
	if (lowat == hiwat)
		lowat = hiwat - 1;
	sc->sc_pr.usedhigh = hiwat * sc->sc_pr.blksize;
	sc->sc_pr.usedlow = lowat * sc->sc_pr.blksize;
	sc->sc_rr.usedhigh = sc->sc_rr.end - sc->sc_rr.start;
	sc->sc_rr.usedlow = 0;
}

static __inline int
audio_sleep_timo(int *chan, char *label, int timo)
{
	int st;

	if (!label)
		label = "audio";

	DPRINTFN(3, ("audio_sleep_timo: chan=%p, label=%s, timo=%d\n",
	    chan, label, timo));
	*chan = 1;
	st = tsleep(chan, PWAIT | PCATCH, label, timo);
	*chan = 0;
#ifdef AUDIO_DEBUG
	if (st != 0)
	    printf("audio_sleep: woke up st=%d\n", st);
#endif
	return (st);
}

static __inline int
audio_sleep(int *chan, char *label)
{
	return audio_sleep_timo(chan, label, 0);
}

/* call at splaudio() */
static __inline void
audio_wakeup(int *chan)
{
	DPRINTFN(3, ("audio_wakeup: chan=%p, *chan=%d\n", chan, *chan));
	if (*chan) {
		wakeup(chan);
		*chan = 0;
	}
}

int
audio_open(dev_t dev, struct audio_softc *sc, int flags, int ifmt,
    struct proc *p)
{
	int error;
	int mode;
	struct audio_info ai;

	DPRINTF(("audio_open: dev=0x%x flags=0x%x sc=%p hdl=%p\n", dev, flags, sc, sc->hw_hdl));

	if (ISDEVAUDIOCTL(dev))
		return 0;

	if ((sc->sc_open & (AUOPEN_READ|AUOPEN_WRITE)) != 0)
		return (EBUSY);

	error = sc->hw_if->open(sc->hw_hdl, flags);
	if (error)
		return (error);

	sc->sc_async_audio = 0;
	sc->sc_rchan = 0;
	sc->sc_wchan = 0;
	sc->sc_sil_count = 0;
	sc->sc_rbus = 0;
	sc->sc_pbus = 0;
	sc->sc_eof = 0;
	sc->sc_playdrop = 0;

	sc->sc_full_duplex = 0;
/* doesn't always work right on SB.
		(flags & (FWRITE|FREAD)) == (FWRITE|FREAD) &&
		(sc->hw_if->get_props(sc->hw_hdl) & AUDIO_PROP_FULLDUPLEX);
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
	 * Multiplex device: /dev/audio (default) and /dev/sound (last)
	 * The /dev/audio is always (re)set to the default parameters.
	 * For the other devices, you get what they were last set to.
	 */
	if (ISDEVAUDIO(dev)) {
		/* /dev/audio */
		if (sc->hw_if->get_default_params) {
			sc->hw_if->get_default_params(sc->hw_hdl, AUMODE_PLAY,
			    &sc->sc_pparams);
			sc->hw_if->get_default_params(sc->hw_hdl, AUMODE_RECORD,
			    &sc->sc_rparams);
		} else {
			sc->sc_rparams = audio_default;
			sc->sc_pparams = audio_default;
		}
	}
#ifdef DIAGNOSTIC
	/*
	 * Sample rate and precision are supposed to be set to proper
	 * default values by the hardware driver, so that it may give
	 * us these values.
	 */
	if (sc->sc_rparams.precision == 0 || sc->sc_pparams.precision == 0) {
		printf("audio_open: 0 precision\n");
		error = EINVAL;
		goto bad;
	}
#endif

	AUDIO_INITINFO(&ai);
	ai.record.sample_rate = sc->sc_rparams.sample_rate;
	ai.record.encoding    = sc->sc_rparams.encoding;
	ai.record.channels    = sc->sc_rparams.channels;
	ai.record.precision   = sc->sc_rparams.precision;
	ai.record.bps         = sc->sc_rparams.bps;
	ai.record.msb         = sc->sc_rparams.msb;
	ai.record.pause	      = 0;
	ai.play.sample_rate   = sc->sc_pparams.sample_rate;
	ai.play.encoding      = sc->sc_pparams.encoding;
	ai.play.channels      = sc->sc_pparams.channels;
	ai.play.precision     = sc->sc_pparams.precision;
	ai.play.bps           = sc->sc_pparams.bps;
	ai.play.msb           = sc->sc_pparams.msb;
	ai.play.pause	      = 0;
	ai.mode		      = mode;
	sc->sc_rr.blkset = sc->sc_pr.blkset = 0; /* Block sizes not set yet */
	sc->sc_pr.blksize = sc->sc_rr.blksize = 0; /* force recalculation */
	error = audiosetinfo(sc, &ai);
	if (error)
		goto bad;

	DPRINTF(("audio_open: done sc_mode = 0x%x\n", sc->sc_mode));

	return 0;

bad:
	sc->hw_if->close(sc->hw_hdl);
	sc->sc_open = 0;
	sc->sc_mode = 0;
	sc->sc_full_duplex = 0;
	return error;
}

/*
 * Must be called from task context.
 */
void
audio_init_record(struct audio_softc *sc)
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
audio_init_play(struct audio_softc *sc)
{
	int s = splaudio();

	sc->sc_wstamp = sc->sc_pr.stamp;
	if (sc->hw_if->speaker_ctl)
		sc->hw_if->speaker_ctl(sc->hw_hdl, SPKR_ON);
	splx(s);
}

int
audio_drain(struct audio_softc *sc)
{
	int error, drops;
	struct audio_ringbuffer *cb = &sc->sc_pr;
	int s;

	DPRINTF(("audio_drain: enter busy=%d used=%d\n",
	    sc->sc_pbus, sc->sc_pr.used));
	if (sc->sc_pr.mmapped || sc->sc_pr.used <= 0)
		return 0;
	if (!sc->sc_pbus) {
		/* We've never started playing, probably because the
		 * block was too short.  Pad it and start now.
		 */
		int cc;
		u_char *inp = cb->inp;

		cc = cb->blksize - (inp - cb->start) % cb->blksize;
		if (sc->sc_pparams.sw_code) {
			int ncc = cc / sc->sc_pparams.factor;
			audio_fill_silence(&sc->sc_pparams, cb->start, inp, ncc);
			sc->sc_pparams.sw_code(sc->hw_hdl, inp, ncc);
		} else
			audio_fill_silence(&sc->sc_pparams, cb->start, inp, cc);
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
		if (sc->sc_dying)
			error = EIO;
	}
	splx(s);
	return error;
}

int
audio_quiesce(struct audio_softc *sc)
{
	sc->sc_quiesce = AUDIO_QUIESCE_START;

	while (sc->sc_pbus && !sc->sc_pqui)
		audio_sleep(&sc->sc_wchan, "audpqui");
	while (sc->sc_rbus && !sc->sc_rqui)
		audio_sleep(&sc->sc_rchan, "audrqui");

	sc->sc_quiesce = AUDIO_QUIESCE_SILENT;

	au_get_mute(sc, &sc->sc_outports, &sc->sc_mute);
	au_set_mute(sc, &sc->sc_outports, 1);

	if (sc->sc_pbus)
		sc->hw_if->halt_output(sc->hw_hdl);
	if (sc->sc_rbus)
		sc->hw_if->halt_input(sc->hw_hdl);

	return 0;
}

void
audio_resume(struct audio_softc *sc)
{
	timeout_add_msec(&sc->sc_resume_to, 1500);
}

void
audio_resume_to(void *v)
{
	struct audio_softc *sc = v;
	workq_queue_task(NULL, &sc->sc_resume_task, 0,
	    audio_resume_task, sc, 0);
}

void
audio_resume_task(void *arg1, void *arg2)
{
	struct audio_softc *sc = arg1;
	int setmode = 0;

	sc->sc_pqui = sc->sc_rqui = 0;

	au_set_mute(sc, &sc->sc_outports, sc->sc_mute);

	if (sc->sc_pbus)
		setmode |= AUMODE_PLAY;
	if (sc->sc_rbus)
		setmode |= AUMODE_RECORD;

	if (setmode) {
		sc->hw_if->set_params(sc->hw_hdl, setmode,
		    sc->sc_mode & (AUMODE_PLAY | AUMODE_RECORD),
		    &sc->sc_pparams, &sc->sc_rparams);
	}

	if (sc->sc_pbus) {
		if (sc->hw_if->trigger_output)
			sc->hw_if->trigger_output(sc->hw_hdl, sc->sc_pr.start,
			    sc->sc_pr.end, sc->sc_pr.blksize,
			    audio_pint, (void *)sc, &sc->sc_pparams);
		else
			sc->hw_if->start_output(sc->hw_hdl, sc->sc_pr.outp,
			    sc->sc_pr.blksize, audio_pint, (void *)sc);
	}
	if (sc->sc_rbus) {
		if (sc->hw_if->trigger_input)
			sc->hw_if->trigger_input(sc->hw_hdl, sc->sc_rr.start,
			    sc->sc_rr.end, sc->sc_rr.blksize,
			    audio_rint, (void *)sc, &sc->sc_rparams);
		else
			sc->hw_if->start_input(sc->hw_hdl, sc->sc_rr.inp,
			    sc->sc_rr.blksize, audio_rint, (void *)sc);
	}

	sc->sc_quiesce = 0;
	wakeup(&sc->sc_quiesce);
}

/*
 * Close an audio chip.
 */
/* ARGSUSED */
int
audio_close(dev_t dev, int flags, int ifmt, struct proc *p)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_hw_if *hw = sc->hw_if;
	int s;

	DPRINTF(("audio_close: unit=%d flags=0x%x\n", unit, flags));

	s = splaudio();
	/* Stop recording. */
	if ((flags & FREAD) && sc->sc_rbus) {
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
	/*
	 * If there is pending output, let it drain (unless
	 * the output is paused).
	 */
	if ((flags & FWRITE) && sc->sc_pbus) {
		if (!sc->sc_pr.pause && !audio_drain(sc) && hw->drain)
			(void)hw->drain(sc->hw_hdl);
		sc->hw_if->halt_output(sc->hw_hdl);
		sc->sc_pbus = 0;
	}

	hw->close(sc->hw_hdl);

	/*
	 * If flags has neither read nor write then reset both
	 * directions. Encountered when someone runs revoke(2).
	 */

	if ((flags & FREAD) || ((flags & (FREAD|FWRITE)) == 0)) {
		sc->sc_open &= ~AUOPEN_READ;
		sc->sc_mode &= ~AUMODE_RECORD;
	}
	if ((flags & FWRITE) || ((flags & (FREAD|FWRITE)) == 0)) {
		sc->sc_open &= ~AUOPEN_WRITE;
		sc->sc_mode &= ~(AUMODE_PLAY|AUMODE_PLAY_ALL);
	}

	sc->sc_async_audio = 0;
	sc->sc_full_duplex = 0;
	splx(s);
	DPRINTF(("audio_close: done\n"));

	return (0);
}

int
audio_read(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_ringbuffer *cb = &sc->sc_rr;
	u_char *outp;
	int error, s, cc, n, resid;

	if (cb->mmapped)
		return EINVAL;

	DPRINTFN(1,("audio_read: cc=%d mode=%d\n",
	    uio->uio_resid, sc->sc_mode));

	/*
	 * Block if fully quiesced.  Don't block when quiesce
	 * has started, as the buffer position may still need
	 * to advance.
	 */
	while (sc->sc_quiesce == AUDIO_QUIESCE_SILENT)
		tsleep(&sc->sc_quiesce, 0, "aud_qrd", 0);

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
				if (sc->sc_dying)
					error = EIO;
				if (error) {
					splx(s);
					return error;
				}
			}
			splx(s);

			if (uio->uio_resid < cc / sc->sc_rparams.factor)
				cc = uio->uio_resid * sc->sc_rparams.factor;
			DPRINTFN(1, ("audio_read: reading in write mode, cc=%d\n", cc));
			error = audio_silence_copyout(sc,
			    cc / sc->sc_rparams.factor, uio);
			sc->sc_wstamp += cc;
		}
		return (error);
	}
	while (uio->uio_resid > 0) {
		s = splaudio();
		while (cb->used <= 0) {
			if (!sc->sc_rbus && !sc->sc_rr.pause) {
				error = audiostartr(sc);
				if (error) {
					splx(s);
					return error;
				}
			}
			if (ioflag & IO_NDELAY) {
				splx(s);
				return (EWOULDBLOCK);
			}
			DPRINTFN(2, ("audio_read: sleep used=%d\n", cb->used));
			error = audio_sleep(&sc->sc_rchan, "aud_rd");
			if (sc->sc_dying)
				error = EIO;
			if (error) {
				splx(s);
				return error;
			}
		}
		resid = uio->uio_resid * sc->sc_rparams.factor;
		outp = cb->outp;
		cc = cb->used - cb->usedlow; /* maximum to read */
		n = cb->end - outp;
		if (cc > n)
			cc = n;		/* don't read beyond end of buffer */
		
		if (cc > resid)
			cc = resid;	/* and no more than we want */
		cb->used -= cc;
		cb->outp += cc;
		if (cb->outp >= cb->end)
			cb->outp = cb->start;
		splx(s);
		DPRINTFN(1,("audio_read: outp=%p, cc=%d\n", outp, cc));
		if (sc->sc_rparams.sw_code)
			sc->sc_rparams.sw_code(sc->hw_hdl, outp, cc);
		error = uiomove(outp, cc / sc->sc_rparams.factor, uio);
		if (error)
			return error;
	}
	return 0;
}

void
audio_clear(struct audio_softc *sc)
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
audio_set_blksize(struct audio_softc *sc, int mode, int fpb) {
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_params *parm;
	struct audio_ringbuffer *rb;
	int bs, fs, maxbs;

	if (mode == AUMODE_PLAY) {
		parm = &sc->sc_pparams;
		rb = &sc->sc_pr;
	} else {
		parm = &sc->sc_rparams;
		rb = &sc->sc_rr;
	}

	fs = parm->channels * parm->bps;
	bs = fpb * fs;
	maxbs = rb->bufsize / 2;
	if (bs > maxbs)
		bs = (maxbs / fs) * fs;

	ROUNDSIZE(bs);
	if (hw->round_blocksize)
		bs = hw->round_blocksize(sc->hw_hdl, bs);
	rb->blksize = bs;

	DPRINTF(("audio_set_blksize: %s blksize=%d\n",
		 mode == AUMODE_PLAY ? "play" : "record", bs));
}

void
audio_calc_blksize(struct audio_softc *sc, int mode)
{
	struct audio_params *param;

	if (mode == AUMODE_PLAY) {
		if (sc->sc_pr.blkset)
			return;
		param = &sc->sc_pparams;
	} else {
		if (sc->sc_rr.blkset)
			return;
		param = &sc->sc_rparams;
	}
	audio_set_blksize(sc, mode, param->sample_rate * audio_blk_ms / 1000);
}

void
audio_fill_silence(struct audio_params *params, u_char *start, u_char *p, int n)
{
	size_t rounderr;
	int i, nsamples;
	u_char auzero[4] = {0, 0, 0, 0};

	/*
	 * p may point the middle of a sample; round it to the
	 * beginning of the sample, so we overwrite partially written
	 * ones.
	 */
	rounderr = (p - start) % params->bps;
	p -= rounderr;
	n += rounderr;
	nsamples = n / params->bps;

	switch (params->encoding) {
	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_SLINEAR_BE:
		break;
	case AUDIO_ENCODING_ULAW:
		auzero[0] = 0x7f;
		break;
	case AUDIO_ENCODING_ALAW:
		auzero[0] = 0x55;
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		if (params->msb == 1)
			auzero[params->bps - 1] = 0x80;
		else
			auzero[params->bps - 1] = 1 << ((params->precision + 7) % NBBY);
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		if (params->msb == 1)
			auzero[0] = 0x80;
		else
			auzero[0] = 1 << ((params->precision + 7) % NBBY);
		break;
	case AUDIO_ENCODING_MPEG_L1_STREAM:
	case AUDIO_ENCODING_MPEG_L1_PACKETS:
	case AUDIO_ENCODING_MPEG_L1_SYSTEM:
	case AUDIO_ENCODING_MPEG_L2_STREAM:
	case AUDIO_ENCODING_MPEG_L2_PACKETS:
	case AUDIO_ENCODING_MPEG_L2_SYSTEM:
	case AUDIO_ENCODING_ADPCM: /* is this right XXX */
		break;
	default:
		DPRINTF(("audio: bad encoding %d\n", params->encoding));
		break;
	}
	while (--nsamples >= 0) {
		for (i = 0; i < params->bps; i++) 
			*p++ = auzero[i];
	}
}

int
audio_silence_copyout(struct audio_softc *sc, int n, struct uio *uio)
{
	int error;
	int k;
	u_char zerobuf[128];

	audio_fill_silence(&sc->sc_rparams, zerobuf, zerobuf, sizeof zerobuf);

	error = 0;
	while (n > 0 && uio->uio_resid > 0 && !error) {
		k = min(n, min(uio->uio_resid, sizeof zerobuf));
		error = uiomove(zerobuf, k, uio);
		n -= k;
	}
	return (error);
}

int
audio_write(dev_t dev, struct uio *uio, int ioflag)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_ringbuffer *cb = &sc->sc_pr;
	u_char *inp;
	int error, s, n, cc, resid, avail;

	DPRINTFN(2, ("audio_write: sc=%p(unit=%d) count=%d used=%d(hi=%d)\n", sc, unit,
		 uio->uio_resid, sc->sc_pr.used, sc->sc_pr.usedhigh));

	if (cb->mmapped)
		return EINVAL;

	/*
	 * Block if fully quiesced.  Don't block when quiesce
	 * has started, as the buffer position may still need
	 * to advance.
	 */
	while (sc->sc_quiesce == AUDIO_QUIESCE_SILENT)
		tsleep(&sc->sc_quiesce, 0, "aud_qwr", 0);

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
		n = min(sc->sc_playdrop, uio->uio_resid * sc->sc_pparams.factor);
		DPRINTF(("audio_write: playdrop %d\n", n));
		uio->uio_offset += n / sc->sc_pparams.factor;
		uio->uio_resid -= n / sc->sc_pparams.factor;
		sc->sc_playdrop -= n;
		if (uio->uio_resid == 0)
			return 0;
	}

	DPRINTFN(1, ("audio_write: sr=%ld, enc=%d, prec=%d, chan=%d, sw=%p, fact=%d\n",
	    sc->sc_pparams.sample_rate, sc->sc_pparams.encoding,
	    sc->sc_pparams.precision, sc->sc_pparams.channels,
	    sc->sc_pparams.sw_code, sc->sc_pparams.factor));

	while (uio->uio_resid > 0) {
		s = splaudio();
		while (cb->used >= cb->usedhigh) {
			DPRINTFN(2, ("audio_write: sleep used=%d lowat=%d hiwat=%d\n",
				 cb->used, cb->usedlow, cb->usedhigh));
			if (ioflag & IO_NDELAY) {
				splx(s);
				return (EWOULDBLOCK);
			}
			error = audio_sleep(&sc->sc_wchan, "aud_wr");
			if (sc->sc_dying)
				error = EIO;
			if (error) {
				splx(s);
				return error;
			}
		}
		resid = uio->uio_resid * sc->sc_pparams.factor;
		avail = cb->end - cb->inp;
		inp = cb->inp;
		cc = cb->usedhigh - cb->used;
		if (cc > resid)
			cc = resid;
		if (cc > avail)
			cc = avail;
		cb->inp += cc;
		if (cb->inp >= cb->end)
			cb->inp = cb->start;
		cb->used += cc;
		/*
		 * This is a very suboptimal way of keeping track of
		 * silence in the buffer, but it is simple.
		 */
		sc->sc_sil_count = 0;
		if (!sc->sc_pbus && !cb->pause && cb->used >= cb->blksize) {
			error = audiostartp(sc);
			if (error) {
				splx(s);
				return error;
			}
		}
		splx(s);
		cc /= sc->sc_pparams.factor;
		DPRINTFN(1, ("audio_write: uiomove cc=%d inp=%p, left=%d\n",
		    cc, inp, uio->uio_resid));
		error = uiomove(inp, cc, uio);
		if (error)
			return 0;
		if (sc->sc_pparams.sw_code) {
			sc->sc_pparams.sw_code(sc->hw_hdl, inp, cc);
			DPRINTFN(1, ("audio_write: expanded cc=%d\n", cc));
		}
	}
	return 0;
}

int
audio_ioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_offset *ao;
	struct audio_info ai;
	int error = 0, s, offs, fd;
	int rbus, pbus;

	/*
	 * Block if fully quiesced.  Don't block when quiesce
	 * has started, as the buffer position may still need
	 * to advance.  An ioctl may be used to determine how
	 * much to read or write.
	 */
	while (sc->sc_quiesce == AUDIO_QUIESCE_SILENT)
		tsleep(&sc->sc_quiesce, 0, "aud_qio", 0);

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
		sc->sc_rr.pause = 0;
		sc->sc_pr.pause = 0;
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
	 * 
	 * The audio_ringbuffer->drops count is the number of buffer
	 * sample size bytes.  Convert it to userland sample size bytes,
	 * then convert to samples.  There is no easy way to get the
	 * buffer sample size, but the userland sample size can be
	 * calculated with userland channels and userland precision.
	 *
	 * original formula:
	 *  sc->sc_rr.drops /
	 *  sc->sc_rparams.factor /
	 *  (sc->sc_rparams.channels * sc->sc_rparams.bps)
	 */
	case AUDIO_RERROR:
		*(int *)addr = sc->sc_rr.drops /
		    (sc->sc_rparams.factor * sc->sc_rparams.channels *
		    sc->sc_rparams.bps);
		break;

	case AUDIO_PERROR:
		*(int *)addr = sc->sc_pr.drops /
		    (sc->sc_pparams.factor * sc->sc_pparams.channels *
		    sc->sc_pparams.bps);
		break;

	/*
	 * Offsets into buffer.
	 */
	case AUDIO_GETIOFFS:
		s = splaudio();
		/* figure out where next DMA will start */
		ao = (struct audio_offset *)addr;
		ao->samples = sc->sc_rr.stamp / sc->sc_rparams.factor;
		ao->deltablks = (sc->sc_rr.stamp - sc->sc_rr.stamp_last) / sc->sc_rr.blksize;
		sc->sc_rr.stamp_last = sc->sc_rr.stamp;
		ao->offset = (sc->sc_rr.inp - sc->sc_rr.start) / sc->sc_rparams.factor;
		splx(s);
		break;

	case AUDIO_GETOOFFS:
		s = splaudio();
		/* figure out where next DMA will start */
		ao = (struct audio_offset *)addr;
		offs = sc->sc_pr.outp - sc->sc_pr.start + sc->sc_pr.blksize;
		if (sc->sc_pr.start + offs >= sc->sc_pr.end)
			offs = 0;
		ao->samples = sc->sc_pr.stamp / sc->sc_pparams.factor;
		ao->deltablks = (sc->sc_pr.stamp - sc->sc_pr.stamp_last) / sc->sc_pr.blksize;
		sc->sc_pr.stamp_last = sc->sc_pr.stamp;
		ao->offset = offs / sc->sc_pparams.factor;
		splx(s);
		break;

	/*
	 * How many bytes will elapse until mike hears the first
	 * sample of what we write next?
	 */
	case AUDIO_WSEEK:
		*(u_long *)addr = sc->sc_pr.used / sc->sc_pparams.factor;
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
		/* Pass read/write info down to query_encoding */
		((struct audio_encoding *)addr)->flags = sc->sc_open;
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
			if (!error) {
				sc->sc_full_duplex = fd;
				if (fd) {
					AUDIO_INITINFO(&ai);
					ai.mode = sc->sc_mode |
					    (AUMODE_PLAY | AUMODE_RECORD);
					error = audiosetinfo(sc, &ai);
				}
			}
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

	case AUDIO_GETPRINFO:
		DPRINTF(("AUDIO_GETPRINFO\n"));
		error = audiogetbufinfo(sc, (struct audio_bufinfo *)addr,
		    AUMODE_PLAY);
		break;

	case AUDIO_GETRRINFO:
		DPRINTF(("AUDIO_GETRRINFO\n"));
		error = audiogetbufinfo(sc, (struct audio_bufinfo *)addr,
		    AUMODE_RECORD);
		break;

	default:
		DPRINTF(("audio_ioctl: unknown ioctl\n"));
		error = ENOTTY;
		break;
	}
	DPRINTF(("audio_ioctl(%d,'%c',%d) result %d\n",
	    IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff, error));
	return (error);
}

void
audio_selwakeup(struct audio_softc *sc, int play)
{
	struct selinfo *si;

	si = play? &sc->sc_wsel : &sc->sc_rsel;

	audio_wakeup(play? &sc->sc_wchan : &sc->sc_rchan);
	selwakeup(si);
	if (sc->sc_async_audio)
		psignal(sc->sc_async_audio, SIGIO);
}

#define	AUDIO_FILTREAD(sc) ( \
    (!sc->sc_full_duplex && (sc->sc_mode & AUMODE_PLAY)) ? \
    sc->sc_pr.stamp > sc->sc_wstamp : sc->sc_rr.used > sc->sc_rr.usedlow)
    
#define	AUDIO_FILTWRITE(sc) ( \
    (!sc->sc_full_duplex && (sc->sc_mode & AUMODE_RECORD)) ||		\
    (!(sc->sc_mode & AUMODE_PLAY_ALL) && sc->sc_playdrop > 0) || 	\
    (sc->sc_pr.used < (sc->sc_pr.usedlow + sc->sc_pr.blksize)))

int
audio_poll(dev_t dev, int events, struct proc *p)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	int revents = 0, s = splaudio();

	DPRINTF(("audio_poll: events=0x%x mode=%d\n", events, sc->sc_mode));

	if (events & (POLLIN | POLLRDNORM)) {
		if (AUDIO_FILTREAD(sc))
			revents |= events & (POLLIN | POLLRDNORM);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		if (AUDIO_FILTWRITE(sc))
			revents |= events & (POLLOUT | POLLWRNORM);
	}
	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &sc->sc_rsel);
		if (events & (POLLOUT | POLLWRNORM))
			selrecord(p, &sc->sc_wsel);
	}
	splx(s);
	return (revents);
}

paddr_t
audio_mmap(dev_t dev, off_t off, int prot)
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

	if ((u_int)off >= cb->bufsize)
		return -1;
	if (!cb->mmapped) {
		cb->mmapped = 1;
		if (cb == &sc->sc_pr) {
			audio_fill_silence(&sc->sc_pparams, cb->start, cb->start, cb->bufsize);
			s = splaudio();
			if (!sc->sc_pbus && !sc->sc_pr.pause)
				(void)audiostartp(sc);
			splx(s);
		} else {
			s = splaudio();
			if (!sc->sc_rbus && !sc->sc_rr.pause)
				(void)audiostartr(sc);
			splx(s);
		}
	}

	return hw->mappage(sc->hw_hdl, cb->start, off, prot);
}

int
audiostartr(struct audio_softc *sc)
{
	int error;

	DPRINTF(("audiostartr: start=%p used=%d(hi=%d) mmapped=%d\n",
		 sc->sc_rr.start, sc->sc_rr.used, sc->sc_rr.usedhigh,
		 sc->sc_rr.mmapped));

	if (sc->hw_if->trigger_input)
		error = sc->hw_if->trigger_input(sc->hw_hdl, sc->sc_rr.start,
		    sc->sc_rr.end, sc->sc_rr.blksize,
		    audio_rint, (void *)sc, &sc->sc_rparams);
	else
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
audiostartp(struct audio_softc *sc)
{
	int error;

	DPRINTF(("audiostartp: start=%p used=%d(hi=%d) mmapped=%d\n",
		 sc->sc_pr.start, sc->sc_pr.used, sc->sc_pr.usedhigh,
		 sc->sc_pr.mmapped));

	if (!sc->sc_pr.mmapped && sc->sc_pr.used < sc->sc_pr.blksize)
		return 0;

	if (sc->hw_if->trigger_output)
		error = sc->hw_if->trigger_output(sc->hw_hdl, sc->sc_pr.start,
		    sc->sc_pr.end, sc->sc_pr.blksize,
		    audio_pint, (void *)sc, &sc->sc_pparams);
	else
		error = sc->hw_if->start_output(sc->hw_hdl, sc->sc_pr.outp,
		    sc->sc_pr.blksize, audio_pint, (void *)sc);
	if (error) {
		DPRINTF(("audiostartp failed: %d\n", error));
		return error;
	}
	sc->sc_pbus = 1;
	return 0;
}

/*
 * When the play interrupt routine finds that the write isn't keeping
 * the buffer filled it will insert silence in the buffer to make up
 * for this.  The part of the buffer that is filled with silence
 * is kept track of in a very approximate way: it starts at sc_sil_start
 * and extends sc_sil_count bytes.  If there is already silence in
 * the requested area nothing is done; so when the whole buffer is
 * silent nothing happens.  When the writer starts again sc_sil_count
 * is set to 0.
 */
/* XXX
 * Putting silence into the output buffer should not really be done
 * at splaudio, but there is no softaudio level to do it at yet.
 */
static __inline void
audio_pint_silence(struct audio_softc *sc, struct audio_ringbuffer *cb,
    u_char *inp, int cc)
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
				sc->sc_sil_count = max(sc->sc_sil_count, q-s);
			DPRINTFN(5, ("audio_pint_silence: fill cc=%d inp=%p, count=%d size=%d\n",
			    cc, inp, sc->sc_sil_count, (int)(cb->end - cb->start)));

			if (sc->sc_pparams.sw_code) {
				int ncc = cc / sc->sc_pparams.factor;
				audio_fill_silence(&sc->sc_pparams, cb->start, inp, ncc);
				sc->sc_pparams.sw_code(sc->hw_hdl, inp, ncc);
			} else
				audio_fill_silence(&sc->sc_pparams, cb->start, inp, cc);

		} else {
			DPRINTFN(5, ("audio_pint_silence: already silent cc=%d inp=%p\n", cc, inp));

		}
	} else {
		sc->sc_sil_start = inp;
		sc->sc_sil_count = cc;
		DPRINTFN(5, ("audio_pint_silence: start fill %p %d\n",
		    inp, cc));

		if (sc->sc_pparams.sw_code) {
			int ncc = cc / sc->sc_pparams.factor;
			audio_fill_silence(&sc->sc_pparams, cb->start, inp, ncc);
			sc->sc_pparams.sw_code(sc->hw_hdl, inp, ncc);
		} else
			audio_fill_silence(&sc->sc_pparams, cb->start, inp, cc);

	}
}

/*
 * Called from HW driver module on completion of dma output.
 * Start output of new block, wrap in ring buffer if needed.
 * If no more buffers to play, output zero instead.
 * Do a wakeup if necessary.
 */
void
audio_pint(void *v)
{
	struct audio_softc *sc = v;
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_ringbuffer *cb = &sc->sc_pr;
	u_char *inp;
	int cc;
	int blksize;
	int error;

	if (!sc->sc_open)
		return;		/* ignore interrupt if not open */

	if (sc->sc_pqui)
		return;

	blksize = cb->blksize;

	add_audio_randomness((long)cb);

	cb->outp += blksize;
	if (cb->outp >= cb->end)
		cb->outp = cb->start;
	cb->stamp += blksize;
	if (cb->mmapped) {
		DPRINTFN(5, ("audio_pint: mmapped outp=%p cc=%d inp=%p\n",
		    cb->outp, blksize, cb->inp));
		if (!hw->trigger_output)
			(void)hw->start_output(sc->hw_hdl, cb->outp,
			    blksize, audio_pint, (void *)sc);
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
			if (lastdelta > sc->sc_pblktime / 3) {
				printf("audio: play interrupt(%d) off relative by %ld us (%lu)\n",
				    sc->sc_pnintr, lastdelta, sc->sc_pblktime);
			}
			totdelta = t - sc->sc_pfirstintr - sc->sc_pblktime * sc->sc_pnintr;
			if (totdelta > sc->sc_pblktime) {
				printf("audio: play interrupt(%d) off absolute by %ld us (%lu) (LOST)\n",
				    sc->sc_pnintr, totdelta, sc->sc_pblktime);
				sc->sc_pnintr++; /* avoid repeated messages */
			}
		} else
			sc->sc_pfirstintr = t;
		sc->sc_plastintr = t;
		sc->sc_pnintr++;
	}
#endif

	cb->used -= blksize;
	if (cb->used < blksize) {
		/* we don't have a full block to use */
		inp = cb->inp;
		cc = blksize - (inp - cb->start) % blksize;
		if (cb->pause)
			cb->pdrops += cc;
		else {
			cb->drops += cc;
			sc->sc_playdrop += cc;
		}
		audio_pint_silence(sc, cb, inp, cc);
		inp += cc;
		if (inp >= cb->end)
			inp = cb->start;
		cb->inp = inp;
		cb->used += cc;

		/* Clear next block so we keep ahead of the DMA. */
		if (cb->used + cc < cb->usedhigh)
			audio_pint_silence(sc, cb, inp, blksize);
	}

	DPRINTFN(5, ("audio_pint: outp=%p cc=%d\n", cb->outp, blksize));
	if (!hw->trigger_output) {
		error = hw->start_output(sc->hw_hdl, cb->outp, blksize,
		    audio_pint, (void *)sc);
		if (error) {
			/* XXX does this really help? */
			DPRINTF(("audio_pint restart failed: %d\n", error));
			audio_clear(sc);
		}
	}

	DPRINTFN(2, ("audio_pint: mode=%d pause=%d used=%d lowat=%d\n",
	    sc->sc_mode, cb->pause, cb->used, cb->usedlow));
	if ((sc->sc_mode & AUMODE_PLAY) && !cb->pause &&
	    cb->used <= cb->usedlow)
		audio_selwakeup(sc, 1);

	/* Possible to return one or more "phantom blocks" now. */
	if (!sc->sc_full_duplex && sc->sc_rchan)
		audio_selwakeup(sc, 0);

	/*
	 * If quiesce requested, halt output when the ring buffer position
	 * is at the beginning, because when the hardware is resumed, it's
	 * buffer position is reset to the beginning.  This will put
	 * hardware and software positions in sync across a suspend cycle.
	 */
	if (sc->sc_quiesce == AUDIO_QUIESCE_START && cb->outp == cb->start) {
		sc->sc_pqui = 1;
		audio_wakeup(&sc->sc_wchan);
	}
}

/*
 * Called from HW driver module on completion of dma input.
 * Mark it as input in the ring buffer (fiddle pointers).
 * Do a wakeup if necessary.
 */
void
audio_rint(void *v)
{
	struct audio_softc *sc = v;
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_ringbuffer *cb = &sc->sc_rr;
	int blksize;
	int error;

	if (!sc->sc_open)
		return;		/* ignore interrupt if not open */

	if (sc->sc_rqui)
		return;

	add_audio_randomness((long)cb);

	blksize = cb->blksize;

	cb->inp += blksize;
	if (cb->inp >= cb->end)
		cb->inp = cb->start;
	cb->stamp += blksize;
	if (cb->mmapped) {
		DPRINTFN(2, ("audio_rint: mmapped inp=%p cc=%d\n",
		    cb->inp, blksize));
		if (!hw->trigger_input)
			(void)hw->start_input(sc->hw_hdl, cb->inp, blksize,
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

	cb->used += blksize;
	if (cb->pause) {
		DPRINTFN(1, ("audio_rint: pdrops %lu\n", cb->pdrops));
		cb->pdrops += blksize;
		cb->outp += blksize;
		if (cb->outp >= cb->end)
			cb->outp = cb->start;
		cb->used -= blksize;
	} else if (cb->used >= cb->usedhigh) {
		DPRINTFN(1, ("audio_rint: drops %lu\n", cb->drops));
		cb->drops += blksize;
		cb->outp += blksize;
		if (cb->outp >= cb->end)
			cb->outp = cb->start;
		cb->used -= blksize;
	}

	DPRINTFN(2, ("audio_rint: inp=%p cc=%d used=%d\n",
	    cb->inp, blksize, cb->used));
	if (!hw->trigger_input) {
		error = hw->start_input(sc->hw_hdl, cb->inp, blksize,
		    audio_rint, (void *)sc);
		if (error) {
			/* XXX does this really help? */
			DPRINTF(("audio_rint: restart failed: %d\n", error));
			audio_clear(sc);
		}
	}

	audio_selwakeup(sc, 0);

	/*
	 * If quiesce requested, halt input when the ring buffer position
	 * is at the beginning, because when the hardware is resumed, it's
	 * buffer position is reset to the beginning.  This will put
	 * hardware and software positions in sync across a suspend cycle.
	 */
	if (sc->sc_quiesce == AUDIO_QUIESCE_START && cb->inp == cb->start) {
		sc->sc_rqui = 1;
		audio_wakeup(&sc->sc_rchan);
	}
}

int
audio_check_params(struct audio_params *p)
{
	if (p->channels < 1 || p->channels > 12)
		return (EINVAL);

	if (p->precision < 8 || p->precision > 32)
		return (EINVAL);

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
			p->precision = 8;
		break;
	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_SLINEAR_BE:
	case AUDIO_ENCODING_ULINEAR_LE:
	case AUDIO_ENCODING_ULINEAR_BE:
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

	return (0);
}

int
au_set_lr_value(struct audio_softc *sc, mixer_ctrl_t *ct, int l, int r)
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
au_get_mute(struct audio_softc *sc, struct au_mixer_ports *ports, u_char *mute)
{
	mixer_devinfo_t mi;
	mixer_ctrl_t ct;
	int error;

	*mute = 0;

	 /* if no master, silently ignore request */
	if (ports->master == -1)
		return 0;

	mi.index = ports->master;
	error = sc->hw_if->query_devinfo(sc->hw_hdl, &mi);
	if (error != 0)
		return error;

	/* master mute control should be the next device, if it exists */
	if (mi.next < 0)
		return 0;

	ct.dev = mi.next;
	ct.type = AUDIO_MIXER_ENUM;
	error = sc->hw_if->get_port(sc->hw_hdl, &ct);
	if (error != 0)
		return error;

	*mute = ct.un.ord;

	return error;
}

int
au_set_mute(struct audio_softc *sc, struct au_mixer_ports *ports, u_char mute)
{
	mixer_devinfo_t mi;
	mixer_ctrl_t ct;
	int error;

	 /* if no master, silently ignore request */
	if (ports->master == -1)
		return 0;

	mi.index = ports->master;
	error = sc->hw_if->query_devinfo(sc->hw_hdl, &mi);
	if (error != 0)
		return error;

	/* master mute control should be the next device, if it exists */
	if (mi.next < 0)
		return 0;

	ct.dev = mi.next;
	ct.type = AUDIO_MIXER_ENUM;
	error = sc->hw_if->get_port(sc->hw_hdl, &ct);
	if (error != 0)
		return error;

	DPRINTF(("au_set_mute: mute (old): %d, mute (new): %d\n",
	    ct.un.ord, mute));

	ct.un.ord = (mute != 0 ? 1 : 0);
	error = sc->hw_if->set_port(sc->hw_hdl, &ct);

	if (!error)
		mixer_signal(sc);
	return error;
}

int
au_set_gain(struct audio_softc *sc, struct au_mixer_ports *ports, int gain,
    int balance)
{
	mixer_ctrl_t ct;
	int i, error;
	int l, r;
	u_int mask;
	int nset;

	/* XXX silently adjust to within limits or return EINVAL ? */
	if (gain > AUDIO_MAX_GAIN)
		gain = AUDIO_MAX_GAIN;
	else if (gain < AUDIO_MIN_GAIN)
		gain = AUDIO_MIN_GAIN;

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
au_get_lr_value(struct audio_softc *sc, mixer_ctrl_t *ct, int *l, int *r)
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
au_get_gain(struct audio_softc *sc, struct au_mixer_ports *ports, u_int *pgain,
    u_char *pbalance)
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
au_set_port(struct audio_softc *sc, struct au_mixer_ports *ports, u_int port)
{
	mixer_ctrl_t ct;
	int i, error;

	if (port == 0)	/* allow this special case */
		return 0;

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
au_get_port(struct audio_softc *sc, struct au_mixer_ports *ports)
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
audiosetinfo(struct audio_softc *sc, struct audio_info *ai)
{
	struct audio_prinfo *r = &ai->record, *p = &ai->play;
	int cleared;
	int s, setmode, modechange = 0;
	int error;
	struct audio_hw_if *hw = sc->hw_if;
	struct audio_params pp, rp;
	int np, nr;
	unsigned int blks;
	int oldpblksize, oldrblksize;
	int rbus, pbus;
	int fpb;
	int fs;
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
	if (p->bps != ~0) {
		pp.bps = p->bps;
		np++;
	}
	if (r->bps != ~0) {
		rp.bps = r->bps;
		nr++;
	}
	if (p->msb != ~0) {
		pp.msb = p->msb;
		np++;
	}
	if (r->msb != ~0) {
		rp.msb = r->msb;
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
		modechange = cleared = 1;
		rp.sw_code = 0;
		rp.factor = 1;
		setmode |= AUMODE_RECORD;
	}
	if (np) {
		if (!cleared)
			audio_clear(sc);
		modechange = cleared = 1;
		pp.sw_code = 0;
		pp.factor = 1;
		setmode |= AUMODE_PLAY;
	}

	if (ai->mode != ~0) {
		if (!cleared)
			audio_clear(sc);
		modechange = cleared = 1;
		sc->sc_mode = ai->mode;
		if (sc->sc_mode & AUMODE_PLAY_ALL)
			sc->sc_mode |= AUMODE_PLAY;
		if ((sc->sc_mode & AUMODE_PLAY) && !sc->sc_full_duplex)
			/* Play takes precedence */
			sc->sc_mode &= ~AUMODE_RECORD;
	}

	if (modechange) {
		int indep = hw->get_props(sc->hw_hdl) & AUDIO_PROP_INDEPENDENT;
		if (!indep) {
			if (setmode == AUMODE_RECORD)
				pp = rp;
			else
				rp = pp;
		}
		error = hw->set_params(sc->hw_hdl, setmode,
		    sc->sc_mode & (AUMODE_PLAY | AUMODE_RECORD), &pp, &rp);
		if (error)
			return (error);
		if (!indep) {
			if (setmode == AUMODE_RECORD) {
				pp.sample_rate = rp.sample_rate;
				pp.encoding    = rp.encoding;
				pp.channels    = rp.channels;
				pp.precision   = rp.precision;
				pp.bps         = rp.bps;
				pp.msb         = rp.msb;
			} else if (setmode == AUMODE_PLAY) {
				rp.sample_rate = pp.sample_rate;
				rp.encoding    = pp.encoding;
				rp.channels    = pp.channels;
				rp.precision   = pp.precision;
				rp.bps         = pp.bps;
				rp.msb         = pp.msb;
			}
		}
		sc->sc_rparams = rp;
		sc->sc_pparams = pp;
	}

	oldpblksize = sc->sc_pr.blksize;
	oldrblksize = sc->sc_rr.blksize;

	/*
	 * allow old-style blocksize changes, for compatibility;
	 * individual play/record block sizes have precedence
	 */
	if (ai->blocksize != ~0) {
		if (r->block_size == ~0)
			r->block_size = ai->blocksize;
		if (p->block_size == ~0)
			p->block_size = ai->blocksize;
	}
	if (r->block_size != ~0) {
		sc->sc_rr.blkset = 0;
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		nr++;
	}
	if (p->block_size != ~0) {
		sc->sc_pr.blkset = 0;
		if (!cleared)
			audio_clear(sc);
		cleared = 1;
		np++;
	}
	if (nr) {
		if (r->block_size == ~0 || r->block_size == 0) {
			fpb = rp.sample_rate * audio_blk_ms / 1000;
		} else {
			fs = rp.channels * rp.bps; 
			fpb = (r->block_size * rp.factor) / fs;
		}
		if (sc->sc_rr.blkset == 0)
			audio_set_blksize(sc, AUMODE_RECORD, fpb);
	}
	if (np) {
		if (p->block_size == ~0 || p->block_size == 0) {
			fpb = pp.sample_rate * audio_blk_ms / 1000;
		} else {
			fs = pp.channels * pp.bps;
			fpb = (p->block_size * pp.factor) / fs;
		}
		if (sc->sc_pr.blkset == 0)
			audio_set_blksize(sc, AUMODE_PLAY, fpb);
	}
	if (r->block_size != ~0 && r->block_size != 0)
		sc->sc_rr.blkset = 1;
	if (p->block_size != ~0 && p->block_size != 0)
		sc->sc_pr.blkset = 1;

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
	if ((r->gain != ~0) && (r->port != 0)) {
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
	if ((r->balance != (u_char)~0) && (r->port != 0)) {
		au_get_gain(sc, &sc->sc_inports, &gain, &balance);
		error = au_set_gain(sc, &sc->sc_inports, gain, r->balance);
		if (error)
			return(error);
	}

	if (ai->output_muted != (u_char)~0) {
		error = au_set_mute(sc, &sc->sc_outports, ai->output_muted);
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
		error = sc->hw_if->set_port(sc->hw_hdl, &ct);
		if (error)
			return(error);
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
		    pbus && !sc->sc_pbus && !sc->sc_pr.pause)
			error = audiostartp(sc);
		if (!error &&
		    (sc->sc_mode & AUMODE_RECORD) &&
		    rbus && !sc->sc_rbus && !sc->sc_rr.pause)
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

	return (0);
}

int
audiogetinfo(struct audio_softc *sc, struct audio_info *ai)
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
	p->bps = sc->sc_pparams.bps;
	r->bps = sc->sc_rparams.bps;
	p->msb = sc->sc_pparams.msb;
	r->msb = sc->sc_rparams.msb;
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

	au_get_mute(sc, &sc->sc_outports, &ai->output_muted);

	p->seek = sc->sc_pr.used / sc->sc_pparams.factor;
	r->seek = sc->sc_rr.used / sc->sc_rparams.factor;

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

	p->buffer_size = sc->sc_pr.bufsize / sc->sc_pparams.factor;
	r->buffer_size = sc->sc_rr.bufsize / sc->sc_rparams.factor;

	r->block_size = sc->sc_rr.blksize / sc->sc_rparams.factor;
	p->block_size = sc->sc_pr.blksize / sc->sc_pparams.factor;
	if (p->block_size != 0) {
		ai->hiwat = sc->sc_pr.usedhigh / sc->sc_pr.blksize;
		ai->lowat = sc->sc_pr.usedlow / sc->sc_pr.blksize;
	} else {
		ai->hiwat = ai->lowat = 0;
	}
	ai->blocksize = p->block_size;	/* for compatibility, remove this */
	ai->mode = sc->sc_mode;

	return (0);
}

int
audiogetbufinfo(struct audio_softc *sc, struct audio_bufinfo *info, int mode)
{
	struct audio_ringbuffer *buf;
	int factor;

	factor = 1;
	if (mode == AUMODE_PLAY) {
		buf = &sc->sc_pr;
		factor = sc->sc_pparams.factor;
	} else {
		buf = &sc->sc_rr;
		factor = sc->sc_rparams.factor;
	}

	info->seek = buf->used / factor;
	info->blksize = buf->blksize / factor;
	if (buf->blksize != 0) {
		info->hiwat = buf->usedhigh / buf->blksize;
		info->lowat = buf->usedlow / buf->blksize;
	} else {
		info->hiwat = 0;
		info->lowat = 0;
	}

	return (0);
}


/*
 * Mixer driver
 */
int
mixer_open(dev_t dev, struct audio_softc *sc, int flags, int ifmt,
    struct proc *p)
{
	DPRINTF(("mixer_open: dev=0x%x flags=0x%x sc=%p\n", dev, flags, sc));

	return (0);
}

/*
 * Remove a process from those to be signalled on mixer activity.
 */
static void
mixer_remove(struct audio_softc *sc, struct proc *p)
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
mixer_signal(struct audio_softc *sc)
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
mixer_close(dev_t dev, int flags, int ifmt, struct proc *p)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];

	DPRINTF(("mixer_close: unit %d\n", AUDIOUNIT(dev)));

	mixer_remove(sc, p);

	return (0);
}

int
mixer_ioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct audio_hw_if *hw = sc->hw_if;
	int error = EINVAL;

	DPRINTF(("mixer_ioctl(%d,'%c',%d)\n",
		 IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff));

	/* Block when fully quiesced.  No need to block earlier. */
	while (sc->sc_quiesce == AUDIO_QUIESCE_SILENT)
		tsleep(&sc->sc_quiesce, 0, "aud_qmi", 0);

	switch (cmd) {
	case FIOASYNC:
		mixer_remove(sc, p); /* remove old entry */
		if (*(int *)addr) {
			struct mixer_asyncs *ma;
			ma = malloc(sizeof (struct mixer_asyncs),
			    M_DEVBUF, M_WAITOK);
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
		((mixer_devinfo_t *)addr)->un.v.delta = 0; /* default */
		error = hw->query_devinfo(sc->hw_hdl, (mixer_devinfo_t *)addr);
		break;

	case AUDIO_MIXER_READ:
		DPRINTF(("AUDIO_MIXER_READ\n"));
		error = hw->get_port(sc->hw_hdl, (mixer_ctrl_t *)addr);
		break;

	case AUDIO_MIXER_WRITE:
		if (!(flag & FWRITE))
			return (EACCES);
		DPRINTF(("AUDIO_MIXER_WRITE\n"));
		error = hw->set_port(sc->hw_hdl, (mixer_ctrl_t *)addr);
		if (!error && hw->commit_settings)
			error = hw->commit_settings(sc->hw_hdl);
		if (!error)
			mixer_signal(sc);
		break;

	default:
		error = ENOTTY;
		break;
	}
	DPRINTF(("mixer_ioctl(%d,'%c',%d) result %d\n",
		 IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd&0xff, error));
	return (error);
}

int
audiokqfilter(dev_t dev, struct knote *kn)
{
	int unit = AUDIOUNIT(dev);
	struct audio_softc *sc = audio_cd.cd_devs[unit];
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->sc_rsel.si_note;
		kn->kn_fop = &audioread_filtops;
		break;
	case EVFILT_WRITE:
		klist = &sc->sc_wsel.si_note;
		kn->kn_fop = &audiowrite_filtops;
		break;
	default:
		return (1);
	}
	kn->kn_hook = (void *)sc;

	s = splaudio();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);

	return (0);
}

void
filt_audiordetach(struct knote *kn)
{
	struct audio_softc *sc = (struct audio_softc *)kn->kn_hook;
	int s = splaudio();

	SLIST_REMOVE(&sc->sc_rsel.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_audioread(struct knote *kn, long hint)
{
	struct audio_softc *sc = (struct audio_softc *)kn->kn_hook;

	return AUDIO_FILTREAD(sc);
}

void
filt_audiowdetach(struct knote *kn)
{
	struct audio_softc *sc = (struct audio_softc *)kn->kn_hook;
	int s = splaudio();

	SLIST_REMOVE(&sc->sc_wsel.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_audiowrite(struct knote *kn, long hint)
{
	struct audio_softc *sc = (struct audio_softc *)kn->kn_hook;

	return AUDIO_FILTWRITE(sc);
}

#if NWSKBD > 0
int
wskbd_set_mixervolume(long dir, int out)
{
	struct audio_softc *sc;
	mixer_devinfo_t mi;
	int error;
	u_int gain;
	u_char balance, mute;
	struct au_mixer_ports *ports;

	if (audio_cd.cd_ndevs == 0 || (sc = audio_cd.cd_devs[0]) == NULL) {
		DPRINTF(("wskbd_set_mixervolume: audio_cd\n"));
		return (ENXIO);
	}

	ports = out ? &sc->sc_outports : &sc->sc_inports;

	if (ports->master == -1) {
		DPRINTF(("wskbd_set_mixervolume: master == -1\n"));
		return (ENXIO);
	}

	if (dir == 0) {
		/* Mute */

		error = au_get_mute(sc, ports, &mute);
		if (error != 0) {
			DPRINTF(("wskbd_set_mixervolume:"
			    " au_get_mute: %d\n", error));
			return (error);
		}

		mute = !mute;

		error = au_set_mute(sc, ports, mute);
		if (error != 0) {
			DPRINTF(("wskbd_set_mixervolume:"
			    " au_set_mute: %d\n", error));
			return (error);
		}
	} else {
		/* Raise or lower volume */

		mi.index = ports->master;
		error = sc->hw_if->query_devinfo(sc->hw_hdl, &mi);
		if (error != 0) {
			DPRINTF(("wskbd_set_mixervolume:"
			    " query_devinfo: %d\n", error));
			return (error);
		}

		au_get_gain(sc, ports, &gain, &balance);

		if (dir > 0)
			gain += mi.un.v.delta;
		else
			gain -= mi.un.v.delta;

		error = au_set_gain(sc, ports, gain, balance);
		if (error != 0) {
			DPRINTF(("wskbd_set_mixervolume:"
			    " au_set_gain: %d\n", error));
			return (error);
		}
	}

	return (0);
}
#endif /* NWSKBD > 0 */
