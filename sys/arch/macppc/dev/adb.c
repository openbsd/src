/*	$OpenBSD: adb.c,v 1.7 2002/09/15 09:01:58 deraadt Exp $	*/
/*	$NetBSD: adb.c,v 1.6 1999/08/16 06:28:09 tsubai Exp $	*/

/*-
 * Copyright (C) 1994	Bradley A. Grantham
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
 *	This product includes software developed by Bradley A. Grantham.
 * 4. The name of the author may not be used to endorse or promote products
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
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>

#include <machine/autoconf.h>

#include <macppc/dev/adbvar.h>
#include <macppc/dev/adb_direct.h>
#include <macppc/dev/akbdvar.h>
#include <macppc/dev/viareg.h>

#include "aed.h"
#include "apm.h"

/*
 * Function declarations.
 */
int	adbmatch(struct device *, void *, void *);
void	adbattach(struct device *, struct device *, void *);
int	adbprint(void *, const char *);

/*
 * Global variables.
 */
int     adb_polling;		/* Are we polling?  (Debugger mode) */
#ifdef ADB_DEBUG
int	adb_debug;		/* Output debugging messages */
#endif /* ADB_DEBUG */

/*
 * Driver definition.
 */
struct cfattach adb_ca = {
	sizeof(struct adb_softc), adbmatch, adbattach
};
struct cfdriver adb_cd = {
	NULL, "adb", DV_DULL
};

int
adbmatch(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	struct confargs *ca = aux;

	if (ca->ca_nreg < 8)
		return 0;

	if (ca->ca_nintr < 4)
		return 0;

	if (strcmp(ca->ca_name, "via-cuda") == 0)
		return 1;

	if (strcmp(ca->ca_name, "via-pmu") == 0)
		return 1;

	return 0;
}

/* HACK ALERT */
typedef int (clock_read_t)(int *sec, int *min, int *hour, int *day,
         int *mon, int *yr);
typedef int (time_read_t)(u_long *sec);
typedef int (time_write_t)(u_long sec);
extern time_read_t  *time_read;
extern time_write_t  *time_write;
extern clock_read_t  *clock_read;


void
adbattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct adb_softc *sc = (struct adb_softc *)self;
	struct confargs *ca = aux;

	ADBDataBlock adbdata;
	struct adb_attach_args aa_args;
	int totaladbs;
	int adbindex, adbaddr;

	ca->ca_reg[0] += ca->ca_baseaddr;

	sc->sc_regbase = mapiodev(ca->ca_reg[0], ca->ca_reg[1]);
	Via1Base = sc->sc_regbase;

	if (strcmp(ca->ca_name, "via-cuda") == 0)
		adbHardware = ADB_HW_CUDA;
	else if (strcmp(ca->ca_name, "via-pmu") == 0)
		adbHardware = ADB_HW_PB;

	adb_polling = 1;
	ADBReInit();

	mac_intr_establish(parent, ca->ca_intr[0], IST_LEVEL, IPL_HIGH,
		adb_intr, sc, "adb");

	/* init powerpc globals which control RTC functionality */
	clock_read = NULL;
	time_read = adb_read_date_time;
	time_write = adb_set_date_time;

#ifdef ADB_DEBUG
	if (adb_debug)
		printf("adb: done with ADBReInit\n");
#endif
	totaladbs = CountADBs();

	printf(" irq %d", ca->ca_intr[0]);

	switch (adbHardware) {
		case ADB_HW_CUDA:
			printf(": via-cuda ");
			break;
		case ADB_HW_PB:
			printf(": via-pmu ");
			break;
	}
 
	printf("%d targets\n", totaladbs);
	

#if NAED > 0
	/* ADB event device for compatibility */
	aa_args.origaddr = 0;
	aa_args.adbaddr = 0;
	aa_args.handler_id = 0;
	(void)config_found(self, &aa_args, adbprint);
#endif

	/* for each ADB device */
	for (adbindex = 1; adbindex <= totaladbs; adbindex++) {
		/* Get the ADB information */
		adbaddr = GetIndADB(&adbdata, adbindex);

		aa_args.origaddr = adbdata.origADBAddr;
		aa_args.adbaddr = adbaddr;
		aa_args.handler_id = adbdata.devType;

		(void)config_found(self, &aa_args, adbprint);
	}

#if NAPM > 0
	/* Magic for signalling the apm driver to match. */
	aa_args.origaddr = ADBADDR_APM;
	aa_args.adbaddr = ADBADDR_APM;
	aa_args.handler_id = ADBADDR_APM;

	(void)config_found(self, &aa_args, NULL);
#endif

	if (adbHardware == ADB_HW_CUDA)
		adb_cuda_autopoll();
	adb_polling = 0;
}

int
adbprint(args, name)
	void *args;
	const char *name;
{
	struct adb_attach_args *aa_args = (struct adb_attach_args *)args;
	int rv = UNCONF;

	if (name) {	/* no configured device matched */
		rv = UNSUPP; /* most ADB device types are unsupported */

		/* print out what kind of ADB device we have found */
		printf("%s addr %d: ", name, aa_args->adbaddr);
		switch(aa_args->origaddr) {
#ifdef DIAGNOSTIC
#if NAED > 0
		case 0:
			printf("ADB event device");
			rv = UNCONF;
			break;
#endif
		case ADBADDR_SECURE:
			printf("security dongle (%d)", aa_args->handler_id);
			break;
#endif
		case ADBADDR_MAP:
			printf("mapped device (%d)", aa_args->handler_id);
			rv = UNCONF;
			break;
		case ADBADDR_REL:
			printf("relative positioning device (%d)",
			    aa_args->handler_id);
			rv = UNCONF;
			break;
#ifdef DIAGNOSTIC
		case ADBADDR_ABS:
			switch (aa_args->handler_id) {
			case ADB_ARTPAD:
				printf("WACOM ArtPad II");
				break;
			default:
				printf("absolute positioning device (%d)",
				    aa_args->handler_id);
				break;
			}
			break;
		case ADBADDR_DATATX:
			printf("data transfer device (modem?) (%d)",
			    aa_args->handler_id);
			break;
		case ADBADDR_MISC:
			switch (aa_args->handler_id) {
			case ADB_POWERKEY:
				printf("Sophisticated Circuits PowerKey");
				break;
			default:
				printf("misc. device (remote control?) (%d)",
				    aa_args->handler_id);
				break;
			}
			break;
		default:
			printf("unknown type device, (handler %d)",
			    aa_args->handler_id);
			break;
#endif /* DIAGNOSTIC */
		}
	} else		/* a device matched and was configured */
                printf(" addr %d: ", aa_args->adbaddr);

	return rv;
}
