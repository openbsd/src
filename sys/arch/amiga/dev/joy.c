/*	$OpenBSD: joy.c,v 1.3 2002/06/11 03:25:42 miod Exp $ */

/*
 * Copyright (c) 2000 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <machine/joystick.h>
#include <machine/conf.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cia.h>



int joymatch(struct device *, void *, void *);
void joyattach(struct device *, struct device *, void *);

struct cfattach joy_ca = {
	sizeof(struct device), joymatch, joyattach
};

struct cfdriver joy_cd = {
	NULL, "joy", DV_DULL, NULL, 0 };


/*
 * We assume the joysticks are always there.
 * They share the mouse ports, and there is no way to
 * distinguish what is connected. So what ?
 */
int
joymatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{
	if (matchname("joy", auxp))
		return(1);
	return(0);
}

void
joyattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	printf(": 2 ports\n");
}

int
joyopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);
	if (unit == 0 || unit == 1)
	    	return(0);
	else
	    	return(ENXIO);
}

int
joyclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	return(0);
}

int
joyread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	unsigned short w, x;
	unsigned char r;

	r = JOYSTICK_NONE;

	if (unit == 0) {
		w = custom.joy0dat;
		if ((ciaa.pra & 64) == 0)
			r = JOYSTICK_FIRE;
	} else {
		w = custom.joy1dat;
		if ((ciaa.pra & 128) == 0)
			r = JOYSTICK_FIRE;
	}


	x = (w>>1) ^ w;

	if (w & 2)
		r |= JOYSTICK_RIGHT;
	if (x & 1)
		r |= JOYSTICK_DOWN;
	if (w & 512)
		r |= JOYSTICK_LEFT;
	if (x & 256)
		r |= JOYSTICK_UP;

	return uiomove((caddr_t)&r, 1, uio);
}
