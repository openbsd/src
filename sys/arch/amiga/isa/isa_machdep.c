/*	$NetBSD: isa_machdep.c,v 1.1 1995/08/02 14:10:17 niklas Exp $	*/

/*
 * Copyright (c) 1995 Niklas Hallqvist
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
 *      This product includes software developed by Niklas Hallqvist.
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
#include <sys/param.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/isa/isavar.h>

#include <machine/pio.h>

#include <amiga/amiga/device.h>
#include <amiga/isa/isa_intr.h>

void	isaattach __P((struct device *, struct device *, void *));
int	isamatch __P((struct device *, void *, void *));

/*
 * After careful thought about this issue I decided that allowing only
 * one isabus configured into a system would be sufficient.  I'm not
 * lazy, I did the original design with possibilities of multiple ISA
 * busses, but that made porting of existing drivers a bit harder and
 * error-prone, as well as I had to write obfuscated code.  This
 * solution is more in the spirit of KISS.  --niklas@appli.se
 */
struct isa_link *isa;
int isadebug = 0;

struct cfdriver isacd = {
	NULL, "isa", isamatch, isaattach,
	DV_DULL, sizeof(struct device), 1
};

int
isamatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata, *aux;
{
	struct cfdata *cf = cfdata;

#ifdef DEBUG
	if (isadebug)
		printf(" isamatch");
#endif

	/* See if the unit number is valid. */
	if (cf->cf_unit > 0)
		return (0);

	return (1);
}

void
isaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_softc *sc = (struct isa_softc *)self;

	isa = (struct isa_link *)aux;

	printf("\n");

	TAILQ_INIT (&sc->sc_subdevs);

	config_scan(isascan, self);
}

void *
isa_intr_establish(intr, type, level, ih_fun, ih_arg)
	int intr;
	int type;
	int level;
	int (*ih_fun)(void *);
	void *ih_arg;
{
	return (*isa_intr_fcns->isa_intr_establish)(intr, type, level,
	    ih_fun, ih_arg);
}

void
isa_intr_disestablish(handler)
	void *handler;
{
	(*isa_intr_fcns->isa_intr_disestablish)(handler);
}

void
isa_outsb(port, addr, cnt)
	int port;
	void *addr;
	int cnt;
{
	u_int8_t *p = addr;

	while (cnt--)
		outb(port, *p++);
}

void
isa_insb(port, addr, cnt)
	int port;
	void *addr;
	int cnt;
{
	u_int8_t *p = addr;

	while (cnt--)
		*p++ = inb(port);
}

void
isa_outsw(port, addr, cnt)
	int port;
	void *addr;
	int cnt;
{
	u_int16_t *p = addr;

	while (cnt--)
		outw(port, *p++);
}

void
isa_insw(port, addr, cnt)
	int port;
	void *addr;
	int cnt;
{
	u_int16_t *p = addr;

	while (cnt--)
		*p++ = inw(port);
}
