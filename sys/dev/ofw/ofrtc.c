/*	$OpenBSD: ofrtc.c,v 1.3 1998/06/28 06:37:15 rahnds Exp $	*/
/*	$NetBSD: ofrtc.c,v 1.3 1996/10/13 01:38:14 christos Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>

#include <dev/ofw/openfirm.h>

struct ofrtc_softc {
	struct device sc_dev;
	int sc_phandle;
	int sc_ihandle;
};

static int ofrtcprobe __P((struct device *, void *, void *));
static void ofrtcattach __P((struct device *, struct device *, void *));

struct cfattach ofrtc_ca = {
	sizeof(struct ofrtc_softc), ofrtcprobe, ofrtcattach
};

struct cfdriver ofrtc_cd = {
	NULL, "ofrtc", DV_DULL
};

static int
ofrtcprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct ofprobe *ofp = aux;
	char type[8];
	int l;
	
	if ((l = OF_getprop(ofp->phandle, "device_type", type, sizeof type - 1)) < 0)
		return 0;
	if (l >= sizeof type)
		return 0;
	
	return !strcmp(type, "rtc");
}

int OF_clock_read(int *sec, int *min, int *hour, int *day,
         int *mon, int *yr);
typedef int (clock_read_t)(int *sec, int *min, int *hour, int *day,
         int *mon, int *yr);
extern clock_read_t *clock_read;

static void
ofrtcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ofrtc_softc *of = (void *)self;
	struct ofprobe *ofp = aux;
	char name[32];
	int l;
	
	clock_read = &OF_clock_read;
	of->sc_phandle = ofp->phandle;
	of->sc_ihandle = 0;
	if ((l = OF_getprop(of->sc_phandle, "name", name, sizeof name - 1)) < 0)
		panic("Device without name?");
	if (l >= sizeof name)
		l = sizeof name - 1;
	name[l] = 0;
	printf(": %s\n", name);
}

int
ofrtcopen(dev, flags, fmt)
	dev_t dev;
	int flags;
	int fmt;
{
	struct ofrtc_softc *of;
	int unit = minor(dev);
	char path[256];
	int l;
	
	if (unit >= ofrtc_cd.cd_ndevs)
		return ENXIO;
	if (!(of = ofrtc_cd.cd_devs[unit]))
		return ENXIO;
	if (!of->sc_ihandle) {
		if ((l = OF_package_to_path(of->sc_phandle, path, sizeof path - 1)) < 0)
			return ENXIO;
		if (l >= sizeof path)
			return ENXIO;
		path[l] = 0;
		
		if (!(of->sc_ihandle = OF_open(path))) {
			if (of->sc_ihandle) {
				OF_close(of->sc_ihandle);
				of->sc_ihandle = 0;
			}
			return ENXIO;
		}

	}

	return 0;
}

int
ofrtcclose(dev, flags, fmt)
	dev_t dev;
	int flags;
	int fmt;
{
	return 0;
}

static void
twodigit(bp, i)
	char *bp;
	int i;
{
	*bp++ = i / 10 + '0';
	*bp = i % 10 + '0';
}

static int
twodigits(bp)
	char *bp;
{
	int i;
	
	i = *bp++ - '0';
	return i * 10 + *bp++ - '0';
}

int
ofrtcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct ofrtc_softc *of = ofrtc_cd.cd_devs[minor(dev)];
	int date[6];
	char buf[14];
	int xlen;
	
	if (uio->uio_offset >= sizeof buf)
		return 0;
	
	if (OF_call_method("get-time", of->sc_ihandle, 0, 6,
			   date, date + 1, date + 2,
			   date + 3, date + 4, date + 5))
		return EIO;

	twodigit(buf, date[5] % 100);
	twodigit(buf + 2, date[4]);
	twodigit(buf + 4, date[3]);
	twodigit(buf + 6, date[2]);
	twodigit(buf + 8, date[1]);
	buf[10] = '.';
	twodigit(buf + 11, date[0]);
	buf[13] = '\n';
	
	xlen = sizeof(buf) - uio->uio_offset;
	if (xlen > uio->uio_resid)
		xlen = uio->uio_resid;
	
	return uiomove((caddr_t)buf, xlen, uio);
}

int
ofrtcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct ofrtc_softc *of = ofrtc_cd.cd_devs[minor(dev)];
	char buf[14];
	int cnt, year, error;
	
	/*
	 * We require atomic updates!
	 */
	cnt = uio->uio_resid;
	if (uio->uio_offset || (cnt != sizeof buf && cnt != sizeof buf - 1))
		return EINVAL;
	
	if (error = uiomove((caddr_t)buf, sizeof buf, uio))
		return error;

	if (cnt == sizeof buf && buf[sizeof buf - 1] != '\n')
		return EINVAL;
	
	year = twodigits(buf) + 1900;
	if (year < 1970)
		year += 100;
	if (OF_call_method("set-time", of->sc_ihandle, 6, 0,
			   twodigits(buf + 11), twodigits(buf + 8), twodigits(buf + 6),
			   twodigits(buf + 4), twodigits(buf + 2), year))
		return EIO;
	return 0;
}



/* powerpc expects clock_read data in BCD, discuss this if it is improper */
#define TOBCD(x)        (((x) / 10 * 16) + ((x) % 10))
#define YEAR0		1900

int
OF_clock_read(int *sec, int *min, int *hour, int *day,
         int *mon, int *yr)
{
	struct ofrtc_softc *of;
        char path[256];
        int l;

	if (!(of = ofrtc_cd.cd_devs[0]))
		return 1;
	if ((l = OF_package_to_path(of->sc_phandle, path, sizeof path - 1)) < 0)
		return 1;
	if (l >= sizeof path)
		return 1;
	path[l] = 0;
	
	if (!(of->sc_ihandle = OF_open(path))) {
		if (of->sc_ihandle) {
			OF_close(of->sc_ihandle);
			of->sc_ihandle = 0;
		}
		return 1;
	}
	if (OF_call_method("get-time", of->sc_ihandle, 0, 6,
			sec, min, hour, day, mon, yr))
	{
	return 1;
	}
	printf("y:%d m:%d d:%d H:%d M:%d S:%d\n",
		*yr, *mon, *day, *hour, *min, *sec);
	*yr -= YEAR0; /* bsd expects 0 - 200, not something like 1998 */
	*yr = TOBCD(*yr);
	*mon = TOBCD(*mon);
	*day = TOBCD(*day);
	*hour = TOBCD(*hour);
	*min = TOBCD(*min);
	*sec = TOBCD(*sec);

	return 0;
}
