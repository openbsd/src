/*	$OpenBSD: abtn.c,v 1.1 2001/09/01 15:50:00 drahn Exp $	*/
/*	$NetBSD: abtn.c,v 1.1 1999/07/12 17:48:26 tsubai Exp $	*/

/*-
 * Copyright (C) 1999 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <macppc/mac/adbvar.h>
#include <macppc/mac/pm_direct.h>

#define NVRAM_BRIGHTNESS 0x140e
#define ABTN_HANDLER_ID 31

struct abtn_softc {
	struct device sc_dev;

	int origaddr;		/* ADB device type */
	int adbaddr;		/* current ADB address */
	int handler_id;

	int brightness;		/* backlight brightness */
	int volume;		/* speaker volume (not yet) */
};

static int abtn_match __P((struct device *, void *, void *));
static void abtn_attach __P((struct device *, struct device *, void *));
static void abtn_adbcomplete __P((caddr_t, caddr_t, int));

struct cfattach abtn_ca = {
	sizeof(struct abtn_softc), abtn_match, abtn_attach
};
struct cfdriver abtn_cd = {
	NULL, "abtn", DV_DULL
};

int
abtn_match(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct adb_attach_args *aa = aux;

	if (aa->origaddr == ADBADDR_MISC &&
	    aa->handler_id == ABTN_HANDLER_ID)
		return 1;

	return 0;
}

void
abtn_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct abtn_softc *sc = (struct abtn_softc *)self;
	struct adb_attach_args *aa = aux;
	ADBSetInfoBlock adbinfo;
	int bright;

	printf("brightness/volume button\n");

	bright = pm_read_nvram(NVRAM_BRIGHTNESS);
	pm_set_brightness(bright);
	sc->brightness = bright;

	sc->origaddr = aa->origaddr;
	sc->adbaddr = aa->adbaddr;
	sc->handler_id = aa->handler_id;

	adbinfo.siServiceRtPtr = (Ptr)abtn_adbcomplete;
	adbinfo.siDataAreaAddr = (caddr_t)sc;

	SetADBInfo(&adbinfo, sc->adbaddr);
}

void 
abtn_adbcomplete(buffer, data, adb_command)
	caddr_t buffer, data;
	int adb_command;
{
	struct abtn_softc *sc = (struct abtn_softc *)data;
	u_int cmd;

	cmd = buffer[1];

	switch (cmd) {
	case 0x0a:
		sc->brightness -= 8;
		if (sc->brightness < 8)
			sc->brightness = 8;
		pm_set_brightness(sc->brightness);
		pm_write_nvram(NVRAM_BRIGHTNESS, sc->brightness);
		break;

	case 0x09:
		sc->brightness += 8;
		if (sc->brightness > 0x78)
			sc->brightness = 0x78;
		pm_set_brightness(sc->brightness);
		pm_write_nvram(NVRAM_BRIGHTNESS, sc->brightness);
		break;
	}
}
