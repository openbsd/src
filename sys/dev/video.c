/*	$OpenBSD: video.c,v 1.1 2008/04/09 19:49:55 robert Exp $	*/
/*
 * Copyright (c) 2008 Robert Nagy <robert@openbsd.org>
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
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/videoio.h>

#include <dev/video_if.h>
#include <dev/videovar.h>

#define	DPRINTF(x)	do { printf x; } while (0)

int	videoprobe(struct device *, void *, void *);
void	videoattach(struct device *, struct device *, void *);
int	videodetach(struct device *, int);
int	videoactivate(struct device *, enum devact);
int	videoprint(void *, const char *);

struct cfattach video_ca = {
	sizeof(struct video_softc), videoprobe, videoattach,
	videodetach, videoactivate
};

struct cfdriver video_cd = {
	NULL, "video", DV_DULL
};

int
videoprobe(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
videoattach(struct device *parent, struct device *self, void *aux)
{
	struct video_softc *sc = (void *) self;
	struct video_attach_args *sa = aux;

	printf("\n");
	sc->hw_if = sa->hwif;
	sc->hw_hdl = sa->hdl;
	sc->sc_dev = parent;
}

int
videoopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	int	unit;
	struct video_softc *sc;

	unit = VIDEOUNIT(dev);
	if (unit >= video_cd.cd_ndevs ||
	    (sc = video_cd.cd_devs[unit]) == NULL ||
	     sc->hw_if == NULL)
		return (ENXIO);

	if (sc->hw_if->open != NULL)
		return (sc->hw_if->open(sc->hw_hdl, flags));
	else
		return (0);
}

int
videoclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct video_softc *sc;

	sc = video_cd.cd_devs[VIDEOUNIT(dev)];

	if (sc->hw_if->close != NULL)
		return (sc->hw_if->close(sc->hw_hdl));
	else
		return (0);
}

int
videoioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	struct video_softc *sc;
	int unit, error;

	unit = VIDEOUNIT(dev);
	if (unit >= video_cd.cd_ndevs ||
	    (sc = video_cd.cd_devs[unit]) == NULL || sc->hw_if == NULL)
		return (ENXIO);

	DPRINTF(("video_ioctl(%d, '%c', %d)\n",
	    IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd & 0xff));

	error = EOPNOTSUPP;
	switch (cmd) {
	case VIDIOC_QUERYCAP:
		if (sc->hw_if->querycap)
			error = (sc->hw_if->querycap)(sc->hw_hdl,
				(struct v4l2_capability *)data);
		break;
	case VIDIOC_S_FMT:
		if (!(flags & FWRITE))
			return (EACCES);
		if (sc->hw_if->s_fmt)
			error = (sc->hw_if->s_fmt)(sc->hw_hdl,
				(struct v4l2_format *)data);
		break;
	case VIDIOC_G_FMT:
		if (sc->hw_if->g_fmt)
			error = (sc->hw_if->g_fmt)(sc->hw_hdl,
				(struct v4l2_format *)data);
		break;
	case VIDIOC_REQBUFS:
		if (sc->hw_if->reqbufs)
			error = (sc->hw_if->reqbufs)(sc->hw_hdl,
				(struct v4l2_requestbuffers *)data);
		break;
	case VIDIOC_QUERYBUF:
	case VIDIOC_QBUF:
	default:
		error = (ENOTTY);
	}

	return (error);
}

/*
 * Called from hardware driver. This is where the MI video driver gets
 * probed/attached to the hardware driver
 */
struct device *
video_attach_mi(struct video_hw_if *rhwp, void *hdlp, struct device *dev)
{
	struct video_attach_args arg;

	arg.hwif = rhwp;
	arg.hdl = hdlp;
	return (config_found(dev, &arg, videoprint));
}

int
videoprint(void *aux, const char *pnp)
{
	if (pnp != NULL)
		printf("video at %s", pnp);
	return (UNCONF);
}

int
videodetach(struct device *self, int flags)
{
	/*struct video_softc *sc = (struct video_softc *)self;*/
	int maj, mn;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == videoopen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

int
videoactivate(struct device *self, enum devact act)
{
	struct video_softc *sc = (struct video_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}
