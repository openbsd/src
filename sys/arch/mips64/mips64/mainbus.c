/*	$OpenBSD: mainbus.c,v 1.7 2009/04/19 12:52:33 miod Exp $ */

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <mips64/archtype.h>
#include <machine/autoconf.h>

/* Definition of the mainbus driver. */
int	mbmatch(struct device *, void *, void *);
void	mbattach(struct device *, struct device *, void *);
int	mbprint(void *, const char *);

const struct cfattach mainbus_ca = {
	sizeof(struct device), mbmatch, mbattach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mbmatch(struct device *parent, void *cfdata, void *aux)
{
	struct cfdata *cf = cfdata;

	if (cf->cf_unit > 0)
		return (0);
	return (1);
}

void
mbattach(struct device *parent, struct device *self, void *aux)
{
	struct confargs nca;

	printf("\n");

	/*
	 * Try to find and attach all of the CPUs in the machine.
	 * ( Right now only one CPU so code is simple )
	 */

	bzero(&nca, sizeof nca);
	nca.ca_name = "cpu";
	config_found(self, &nca, mbprint);
	nca.ca_name = "clock";
	config_found(self, &nca, mbprint);

	switch (sys_config.system_type) {
#ifdef TGT_O2
	case SGI_O2:
		nca.ca_name = "macebus";
		config_found(self, &nca, mbprint);
		nca.ca_name = "gbe";
		config_found(self, &nca, mbprint);
		break;
#endif
#if defined(TGT_ORIGIN200) || defined(TGT_ORIGIN2000) || defined(TGT_OCTANE)
	case SGI_O200:
	case SGI_O300:
	case SGI_OCTANE:
		nca.ca_name = "xbow";
		config_found(self, &nca, mbprint);
		break;
#endif
	}
}

int
mbprint(void *aux, const char *pnp)
{
	if (pnp)
		return (QUIET);
	return (UNCONF);
}
