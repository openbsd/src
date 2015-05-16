/*	$OpenBSD: midi.c,v 1.39 2015/05/16 09:56:10 ratchov Exp $	*/

/*
 * Copyright (c) 2003, 2004 Alexandre Ratchov
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/poll.h>
#include <sys/kernel.h>
#include <sys/timeout.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <dev/midi_if.h>
#include <dev/audio_if.h>
#include <dev/midivar.h>


int	midiopen(dev_t, int, int, struct proc *);
int	midiclose(dev_t, int, int, struct proc *);
int	midiread(dev_t, struct uio *, int);
int	midiwrite(dev_t, struct uio *, int);
int	midipoll(dev_t, int, struct proc *);
int	midikqfilter(dev_t, struct knote *);
int	midiioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	midiprobe(struct device *, void *, void *);
void	midiattach(struct device *, struct device *, void *);
int	mididetach(struct device *, int);
int	midiprint(void *, const char *);

void	midi_iintr(void *, int);
void 	midi_ointr(void *);
void	midi_timeout(void *);
void	midi_out_start(struct midi_softc *);
void	midi_out_stop(struct midi_softc *);
void	midi_out_do(struct midi_softc *);
void	midi_attach(struct midi_softc *, struct device *);


struct cfattach midi_ca = {
	sizeof(struct midi_softc), midiprobe, midiattach, mididetach
};

struct cfdriver midi_cd = {
	NULL, "midi", DV_DULL
};


void filt_midiwdetach(struct knote *);
int filt_midiwrite(struct knote *, long);

struct filterops midiwrite_filtops = {
	1, NULL, filt_midiwdetach, filt_midiwrite
};

void filt_midirdetach(struct knote *);
int filt_midiread(struct knote *, long);

struct filterops midiread_filtops = {
	1, NULL, filt_midirdetach, filt_midiread
};

void
midi_iintr(void *addr, int data)
{
	struct midi_softc  *sc = (struct midi_softc *)addr;
	struct midi_buffer *mb = &sc->inbuf;

	MUTEX_ASSERT_LOCKED(&audio_lock);
	if (!(sc->dev.dv_flags & DVF_ACTIVE) || !(sc->flags & FREAD))
		return;

	if (MIDIBUF_ISFULL(mb))
		return; /* discard data */

	MIDIBUF_WRITE(mb, data);
	if (mb->used == 1) {
		if (sc->rchan) {
			sc->rchan = 0;
			wakeup(&sc->rchan);
		}
		selwakeup(&sc->rsel);
		if (sc->async)
			psignal(sc->async, SIGIO);
	}
}

int
midiread(dev_t dev, struct uio *uio, int ioflag)
{
	struct midi_softc *sc;
	struct midi_buffer *mb = &sc->inbuf;
	size_t count;
	int error;

	sc = (struct midi_softc *)device_lookup(&midi_cd, minor(dev));
	if (sc == NULL)
		return ENXIO;
	if (!(sc->flags & FREAD)) {
		error = ENXIO;
		goto done;
	}

	/* if there is no data then sleep (unless IO_NDELAY flag is set) */
	error = 0;
	mtx_enter(&audio_lock);
	while (MIDIBUF_ISEMPTY(mb)) {
		if (ioflag & IO_NDELAY) {
			mtx_leave(&audio_lock);
			error = EWOULDBLOCK;
			goto done;
		}
		sc->rchan = 1;
		error = msleep(&sc->rchan, &audio_lock, PWAIT | PCATCH, "mid_rd", 0);
		if (!(sc->dev.dv_flags & DVF_ACTIVE))
			error = EIO;
		if (error) {
			mtx_leave(&audio_lock);
			goto done;
		}
	}

	/* at this stage, there is at least 1 byte */

	while (uio->uio_resid > 0 && mb->used > 0) {
		count = MIDIBUF_SIZE - mb->start;
		if (count > mb->used)
			count = mb->used;
		if (count > uio->uio_resid)
			count = uio->uio_resid;
		mtx_leave(&audio_lock);
		error = uiomove(mb->data + mb->start, count, uio);
		if (error)
			goto done;
		mtx_enter(&audio_lock);
		MIDIBUF_REMOVE(mb, count);
	}
	mtx_leave(&audio_lock);
done:
	device_unref(&sc->dev);
	return error;
}

void
midi_ointr(void *addr)
{
	struct midi_softc *sc = (struct midi_softc *)addr;
	struct midi_buffer *mb;

	MUTEX_ASSERT_LOCKED(&audio_lock);
	if (!(sc->dev.dv_flags & DVF_ACTIVE) || !(sc->flags & FWRITE))
		return;
	
	mb = &sc->outbuf;
	if (mb->used > 0) {
#ifdef MIDI_DEBUG
		if (!sc->isbusy) {
			printf("midi_ointr: output must be busy\n");
		}
#endif
		midi_out_do(sc);
	} else if (sc->isbusy)
		midi_out_stop(sc);
}

void
midi_timeout(void *addr)
{
	mtx_enter(&audio_lock);
	midi_ointr(addr);
	mtx_leave(&audio_lock);
}

void
midi_out_start(struct midi_softc *sc)
{
	if (!sc->isbusy) {
		sc->isbusy = 1;
		midi_out_do(sc);
	}
}

void
midi_out_stop(struct midi_softc *sc)
{
	sc->isbusy = 0;
	if (sc->wchan) {
		sc->wchan = 0;
		wakeup(&sc->wchan);
	}
	selwakeup(&sc->wsel);
	if (sc->async)
		psignal(sc->async, SIGIO);
}

void
midi_out_do(struct midi_softc *sc)
{
	struct midi_buffer *mb = &sc->outbuf;

	while (mb->used > 0) {
		if (!sc->hw_if->output(sc->hw_hdl, mb->data[mb->start]))
			break;
		MIDIBUF_REMOVE(mb, 1);
		if (MIDIBUF_ISEMPTY(mb)) {
			if (sc->hw_if->flush != NULL)
				sc->hw_if->flush(sc->hw_hdl);
			midi_out_stop(sc);
			return;
		}
	}

	if (!(sc->props & MIDI_PROP_OUT_INTR)) {
		if (MIDIBUF_ISEMPTY(mb))
			midi_out_stop(sc);
		else
			timeout_add(&sc->timeo, 1);
	}
}

int
midiwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct midi_softc *sc;
	struct midi_buffer *mb = &sc->outbuf;
	size_t count;
	int error;

	sc = (struct midi_softc *)device_lookup(&midi_cd, minor(dev));
	if (sc == NULL)
		return ENXIO;
	if (!(sc->flags & FWRITE)) {
		error = ENXIO;
		goto done;
	}

	/*
	 * If IO_NDELAY flag is set then check if there is enough room
	 * in the buffer to store at least one byte. If not then dont
	 * start the write process.
	 */
	error = 0;
	mtx_enter(&audio_lock);
	if ((ioflag & IO_NDELAY) && MIDIBUF_ISFULL(mb) && (uio->uio_resid > 0)) {
		mtx_leave(&audio_lock);
		error = EWOULDBLOCK;
		goto done;
	}

	while (uio->uio_resid > 0) {
		while (MIDIBUF_ISFULL(mb)) {
			if (ioflag & IO_NDELAY) {
				/*
				 * At this stage at least one byte is already
				 * moved so we do not return EWOULDBLOCK
				 */
				mtx_leave(&audio_lock);
				goto done;
			}
			sc->wchan = 1;
			error = msleep(&sc->wchan, &audio_lock,
			    PWAIT | PCATCH, "mid_wr", 0);
			if (!(sc->dev.dv_flags & DVF_ACTIVE))
				error = EIO;
			if (error) {
				mtx_leave(&audio_lock);
				goto done;
			}
		}

		count = MIDIBUF_SIZE - MIDIBUF_END(mb);
		if (count > MIDIBUF_AVAIL(mb))
			count = MIDIBUF_AVAIL(mb);
		if (count > uio->uio_resid)
			count = uio->uio_resid;
		mtx_leave(&audio_lock);
		error = uiomove(mb->data + MIDIBUF_END(mb), count, uio);
		if (error)
			goto done;
		mtx_enter(&audio_lock);
		mb->used += count;
		midi_out_start(sc);
	}
	mtx_leave(&audio_lock);
done:
	device_unref(&sc->dev);
	return error;
}

int
midipoll(dev_t dev, int events, struct proc *p)
{
	struct midi_softc *sc;
	int revents;

	sc = (struct midi_softc *)device_lookup(&midi_cd, minor(dev));
	if (sc == NULL)
		return POLLERR;
	revents = 0;
	mtx_enter(&audio_lock);
	if (events & (POLLIN | POLLRDNORM)) {
		if (!MIDIBUF_ISEMPTY(&sc->inbuf))
			revents |= events & (POLLIN | POLLRDNORM);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		if (!MIDIBUF_ISFULL(&sc->outbuf))
			revents |= events & (POLLOUT | POLLWRNORM);
	}
	if (revents == 0) {
		if (events & (POLLIN | POLLRDNORM))
			selrecord(p, &sc->rsel);
		if (events & (POLLOUT | POLLWRNORM))
			selrecord(p, &sc->wsel);
	}
	mtx_leave(&audio_lock);
	device_unref(&sc->dev);
	return (revents);
}

int
midikqfilter(dev_t dev, struct knote *kn)
{
	struct midi_softc *sc;
	struct klist 	  *klist;
	int error;

	sc = (struct midi_softc *)device_lookup(&midi_cd, minor(dev));
	if (sc == NULL)
		return ENXIO;
	error = 0;
	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->rsel.si_note;
		kn->kn_fop = &midiread_filtops;
		break;
	case EVFILT_WRITE:
		klist = &sc->wsel.si_note;
		kn->kn_fop = &midiwrite_filtops;
		break;
	default:
		error = EINVAL;
		goto done;
	}
	kn->kn_hook = (void *)sc;

	mtx_enter(&audio_lock);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mtx_leave(&audio_lock);
done:
	device_unref(&sc->dev);
	return error;
}

void
filt_midirdetach(struct knote *kn)
{
	struct midi_softc *sc = (struct midi_softc *)kn->kn_hook;

	mtx_enter(&audio_lock);
	SLIST_REMOVE(&sc->rsel.si_note, kn, knote, kn_selnext);
	mtx_leave(&audio_lock);
}

int
filt_midiread(struct knote *kn, long hint)
{
	struct midi_softc *sc = (struct midi_softc *)kn->kn_hook;
	int retval;

	mtx_enter(&audio_lock);
	retval = !MIDIBUF_ISEMPTY(&sc->inbuf);
	mtx_leave(&audio_lock);

	return (retval);
}

void
filt_midiwdetach(struct knote *kn)
{
	struct midi_softc *sc = (struct midi_softc *)kn->kn_hook;

	mtx_enter(&audio_lock);
	SLIST_REMOVE(&sc->wsel.si_note, kn, knote, kn_selnext);
	mtx_leave(&audio_lock);
}

int
filt_midiwrite(struct knote *kn, long hint)
{
	struct midi_softc *sc = (struct midi_softc *)kn->kn_hook;
	int		   retval;

	mtx_enter(&audio_lock);
	retval = !MIDIBUF_ISFULL(&sc->outbuf);
	mtx_leave(&audio_lock);

	return (retval);
}

int
midiioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct midi_softc *sc;
	int error;

	sc = (struct midi_softc *)device_lookup(&midi_cd, minor(dev));
	if (sc == NULL)
		return ENXIO;
	error = 0;
	switch(cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer */
		break;
	case FIOASYNC:
		if (*(int *)addr) {
			if (sc->async) {
				error = EBUSY;
				goto done;
			}
			sc->async = p;
		} else
			sc->async = 0;
		break;
	default:
		error = ENOTTY;
	}
done:
	device_unref(&sc->dev);
	return error;
}

int
midiopen(dev_t dev, int flags, int mode, struct proc *p)
{
	struct midi_softc *sc;
	int error;

	sc = (struct midi_softc *)device_lookup(&midi_cd, minor(dev));
	if (sc == NULL)
		return ENXIO;
	error = 0;
	if (sc->flags) {
		error = EBUSY;
		goto done;
	}
	MIDIBUF_INIT(&sc->inbuf);
	MIDIBUF_INIT(&sc->outbuf);
	sc->isbusy = 0;
	sc->rchan = sc->wchan = 0;
	sc->async = 0;
	sc->flags = flags;
	error = sc->hw_if->open(sc->hw_hdl, flags, midi_iintr, midi_ointr, sc);
	if (error)
		sc->flags = 0;
done:
	device_unref(&sc->dev);
	return error;
}

int
midiclose(dev_t dev, int fflag, int devtype, struct proc *p)
{
	struct midi_softc *sc;
	struct midi_buffer *mb;
	int error;

	sc = (struct midi_softc *)device_lookup(&midi_cd, minor(dev));
	if (sc == NULL)
		return ENXIO;

	/* start draining output buffer */
	error = 0;
	mb = &sc->outbuf;
	mtx_enter(&audio_lock);
	if (!MIDIBUF_ISEMPTY(mb))
		midi_out_start(sc);
	while (sc->isbusy) {
		sc->wchan = 1;
		error = msleep(&sc->wchan, &audio_lock,
		    PWAIT, "mid_dr", 5 * hz);
		if (!(sc->dev.dv_flags & DVF_ACTIVE))
			error = EIO;
		if (error)
			break;
	}
	mtx_leave(&audio_lock);

	/*
	 * some hw_if->close() reset immediately the midi uart
	 * which flushes the internal buffer of the uart device,
	 * so we may lose some (important) data. To avoid this,
	 * sleep 20ms (around 64 bytes) to give the time to the
	 * uart to drain its internal buffers.
	 */
	tsleep(&sc->wchan, PWAIT, "mid_cl", hz * MIDI_MAXWRITE / MIDI_RATE);
	sc->hw_if->close(sc->hw_hdl);
	sc->flags = 0;
	device_unref(&sc->dev);
	return 0;
}

int
midiprobe(struct device *parent, void *match, void *aux)
{
	struct audio_attach_args *sa = aux;

	return (sa != NULL && (sa->type == AUDIODEV_TYPE_MIDI) ? 1 : 0);
}

void
midiattach(struct device *parent, struct device *self, void *aux)
{
	struct midi_info	  mi;
	struct midi_softc        *sc = (struct midi_softc *)self;
	struct audio_attach_args *sa = (struct audio_attach_args *)aux;
	struct midi_hw_if        *hwif = sa->hwif;
	void  			 *hdl = sa->hdl;

#ifdef DIAGNOSTIC
	if (hwif == 0 ||
	    hwif->open == 0 ||
	    hwif->close == 0 ||
	    hwif->output == 0 ||
	    hwif->getinfo == 0) {
		printf("midi: missing method\n");
		return;
	}
#endif
	sc->hw_if = hwif;
	sc->hw_hdl = hdl;
	sc->hw_if->getinfo(sc->hw_hdl, &mi);
	sc->props = mi.props;
	sc->flags = 0;
	timeout_set(&sc->timeo, midi_timeout, sc);
	printf(": <%s>\n", mi.name);
}

int
mididetach(struct device *self, int flags)
{
	struct midi_softc *sc = (struct midi_softc *)self;
	int maj, mn;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == midiopen) {
			/* Nuke the vnodes for any open instances (calls close). */
			mn = self->dv_unit;
			vdevgone(maj, mn, mn, VCHR);
		}
	}

	/*
	 * The close() method did nothing (device_lookup() returns
	 * NULL), so quickly halt transfers (normally parent is already
	 * gone, and code below is no-op), and wake-up user-land blocked
	 * in read/write/ioctl, which return EIO.
	 */
	if (sc->flags) {
		if (sc->flags & FREAD) {
			sc->rchan = 0;
			wakeup(&sc->rchan);
			selwakeup(&sc->rsel);
		}
		if (sc->flags & FWRITE) {
			sc->wchan = 0;
			wakeup(&sc->wchan);
			selwakeup(&sc->wsel);
		}
		sc->hw_if->close(sc->hw_hdl);
		sc->flags = 0;
	}
	return 0;
}

int
midiprint(void *aux, const char *pnp)
{
	if (pnp)
		printf("midi at %s", pnp);
	return (UNCONF);
}

struct device *
midi_attach_mi(struct midi_hw_if *hwif, void *hdl, struct device *dev)
{
	struct audio_attach_args arg;

	arg.type = AUDIODEV_TYPE_MIDI;
	arg.hwif = hwif;
	arg.hdl = hdl;
	return config_found(dev, &arg, midiprint);
}
