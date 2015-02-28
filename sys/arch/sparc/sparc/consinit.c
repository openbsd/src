/*	$OpenBSD: consinit.c,v 1.1 2015/02/28 17:54:54 miod Exp $	*/
/*	$NetBSD: zs.c,v 1.50 1997/10/18 00:00:40 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Zilog Z8530 Dual UART driver (machine-dependent part)
 *
 * Runs two serial lines per chip using slave drivers.
 * Plain tty/async lines use the zstty slave.
 * Sun keyboard/mouse uses the zskbd/zsms slaves.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/bsd_openprom.h>
#include <machine/eeprom.h>
#if defined(SUN4)
#include <machine/oldmon.h>
#endif
#include <machine/z8530var.h>

#include <dev/cons.h>

#include <sparc/dev/cons.h>

#ifdef solbourne
#include <machine/prom.h>
#endif

#include "zskbd.h"
#include "zs.h"

/* Make life easier for the initialized arrays here. */
#if NZS < 3
#undef  NZS
#define NZS 3
#endif

/* Flags from cninit() */
extern int zs_hwflags[NZS][2];

/*****************************************************************/

#if !defined(solbourne)

cons_decl(prom);

/*
 * The console is set to this one initially,
 * which lets us use the PROM until consinit()
 * is called to select a real console.
 */
struct consdev consdev_prom = {
	promcnprobe,
	promcninit,
	promcngetc,
	promcnputc,
	nullcnpollc,
};

/*
 * The console table pointer is statically initialized
 * to point to the PROM (output only) table, so that
 * early calls to printf will work.
 */
struct consdev *cn_tab = &consdev_prom;

void
promcnprobe(cn)
	struct consdev *cn;
{
	cn->cn_dev = makedev(0, 0);
	cn->cn_pri = CN_MIDPRI;
}

void
promcninit(cn)
	struct consdev *cn;
{
}

/*
 * PROM console input putchar.
 */
int
promcngetc(dev)
	dev_t dev;
{
	int s, c;

	if (promvec->pv_romvec_vers > 2) {
		int n = 0;
		unsigned char c0;

		s = splhigh();
		while (n <= 0) {
			n = (*promvec->pv_v2devops.v2_read)
			        (*promvec->pv_v2bootargs.v2_fd0, &c0, 1);
		}
		splx(s);

		c = c0;
	} else {
#if defined(SUN4)
		/* SUN4 PROM: must turn off local echo */
		extern struct om_vector *oldpvec;
		int saveecho = 0;
#endif
		s = splhigh();
#if defined(SUN4)
		if (CPU_ISSUN4) {
			saveecho = *(oldpvec->echo);
			*(oldpvec->echo) = 0;
		}
#endif
		c = (*promvec->pv_getchar)();
#if defined(SUN4)
		if (CPU_ISSUN4)
			*(oldpvec->echo) = saveecho;
#endif
		splx(s);
	}

	if (c == '\r')
		c = '\n';

	return (c);
}

/*
 * PROM console output putchar.
 */
void
promcnputc(dev, c)
	dev_t dev;
	int c;
{
	int s;
	char c0 = (c & 0x7f);

	s = splhigh();
	if (promvec->pv_romvec_vers > 2)
		(*promvec->pv_v2devops.v2_write)
			(*promvec->pv_v2bootargs.v2_fd1, &c0, 1);
	else
		(*promvec->pv_putchar)(c);
	splx(s);
}

#endif	/* !solbourne */

/*****************************************************************/

char *prom_inSrc_name[] = {
	"keyboard/display",
	"ttya", "ttyb",
	"ttyc", "ttyd" };

extern struct consdev zscn;
int zstty_unit;

/*
 * This function replaces sys/dev/cninit.c
 * Determine which device is the console using
 * the PROM "input source" and "output sink".
 */
void
consinit()
{
	struct zschan *zc;
	struct consdev *console = cn_tab;
	int channel, zs_unit;
	int inSource, outSink;

#if !defined(solbourne)
	if (promvec->pv_romvec_vers > 2) {
		/* We need to probe the PROM device tree */
		int node,fd;
		char buffer[128];
		struct nodeops *no;
		struct v2devops *op;
		char *cp;
		extern int fbnode;

		inSource = outSink = -1;
		no = promvec->pv_nodeops;
		op = &promvec->pv_v2devops;

		node = findroot();
		if (no->no_proplen(node, "stdin-path") >= sizeof(buffer)) {
			printf("consinit: increase buffer size and recompile\n");
			goto setup_output;
		}
		/* XXX: fix above */

		no->no_getprop(node, "stdin-path",buffer);

		/*
		 * Open an "instance" of this device.
		 * You'd think it would be appropriate to call v2_close()
		 * on the handle when we're done with it. But that seems
		 * to cause the device to shut down somehow; for the moment,
		 * we simply leave it open...
		 */
		if ((fd = op->v2_open(buffer)) == 0 ||
		     (node = op->v2_fd_phandle(fd)) == 0) {
			printf("consinit: bogus stdin path %s.\n",buffer);
			goto setup_output;
		}
		if (no->no_proplen(node,"keyboard") >= 0) {
			inSource = PROMDEV_KBD;
			goto setup_output;
		}
		if (strcmp(getpropstring(node,"device_type"), "serial") != 0) {
			/* not a serial, not keyboard. what is it?!? */
			inSource = -1;
			goto setup_output;
		}
		/*
		 * At this point we assume the device path is in the form
		 *   ....device@x,y:a for ttya and ...device@x,y:b for ttyb.
		 * If it isn't, we defer to the ROM
		 */
		cp = buffer;
		while (*cp)
		    cp++;
		cp -= 2;
#ifdef DEBUG
		if (cp < buffer)
		    panic("consinit: bad stdin path %s",buffer);
#endif
		/* XXX: only allows tty's a->z, assumes PROMDEV_TTYx contig */
		if (cp[0]==':' && cp[1] >= 'a' && cp[1] <= 'z')
		    inSource = PROMDEV_TTYA + (cp[1] - 'a');
		/* else use rom */
setup_output:
		node = findroot();
		if (no->no_proplen(node, "stdout-path") >= sizeof(buffer)) {
			printf("consinit: increase buffer size and recompile\n");
			goto setup_console;
		}
		/* XXX: fix above */

		no->no_getprop(node, "stdout-path", buffer);

		if ((fd = op->v2_open(buffer)) == 0 ||
		     (node = op->v2_fd_phandle(fd)) == 0) {
			printf("consinit: bogus stdout path %s.\n",buffer);
			goto setup_output;
		}
		if (strcmp(getpropstring(node,"device_type"),"display") == 0) {
			/* frame buffer output */
			outSink = PROMDEV_SCREEN;
			fbnode = node;
		} else if (strcmp(getpropstring(node,"device_type"), "serial")
			   != 0) {
			/* not screen, not serial. Whatzit? */
			outSink = -1;
		} else { /* serial console. which? */
			/*
			 * At this point we assume the device path is in the
			 * form:
			 * ....device@x,y:a for ttya, etc.
			 * If it isn't, we defer to the ROM
			 */
			cp = buffer;
			while (*cp)
			    cp++;
			cp -= 2;
#ifdef DEBUG
			if (cp < buffer)
				panic("consinit: bad stdout path %s",buffer);
#endif
			/* XXX: only allows tty's a->z, assumes PROMDEV_TTYx contig */
			if (cp[0]==':' && cp[1] >= 'a' && cp[1] <= 'z')
			    outSink = PROMDEV_TTYA + (cp[1] - 'a');
			else outSink = -1;
		}
	} else {
		inSource = *promvec->pv_stdin;
		outSink  = *promvec->pv_stdout;
	}
setup_console:
#endif	/* !solbourne */
#ifdef solbourne
	if (CPU_ISKAP) {
		const char *dev;

		inSource = PROMDEV_TTYA;	/* default */
		dev = prom_getenv(ENV_INPUTDEVICE);
		if (dev != NULL) {
			if (strcmp(dev, "ttyb") == 0)
				inSource = PROMDEV_TTYB;
			if (strcmp(dev, "keyboard") == 0)
				inSource = PROMDEV_KBD;
		}

		outSink = PROMDEV_TTYA;	/* default */
		dev = prom_getenv(ENV_OUTPUTDEVICE);
		if (dev != NULL) {
			if (strcmp(dev, "ttyb") == 0)
				outSink = PROMDEV_TTYB;
			if (strcmp(dev, "screen") == 0)
				outSink = PROMDEV_SCREEN;
		}
	}
#endif

	if (inSource != outSink) {
		printf("cninit: mismatched PROM output selector\n");
		/*
		 * In case of mismatch, force the console to be on
		 * serial.
		 * There are three possible mismatches:
		 * - input and output on different serial lines:
		 *   use the output line.
		 * - input on keyboard, output on serial:
		 *   use the output line (this allows systems configured
		 *   for glass console, which frame buffers have been removed,
		 *   to still work if the keyboard is left plugged).
		 * - input on serial, output on video:
		 *   use the input line, since we don't know if a keyboard
		 *   is connected.
		 */
		if (outSink == PROMDEV_TTYA || outSink == PROMDEV_TTYB)
			inSource = outSink;
		else
			outSink = inSource;
	}

	switch (inSource) {
	default:
		printf("cninit: invalid inSource=%d\n", inSource);
		callrom();
		inSource = PROMDEV_KBD;
		/* FALLTHROUGH */

	case PROMDEV_KBD: /* keyboard/display */
#if NZSKBD > 0
		zs_unit = 1;
		channel = 0;
		break;
#else	/* NZSKBD */
		printf("cninit: kdb/display not configured\n");
		callrom();
		inSource = PROMDEV_TTYA;
		/* FALLTHROUGH */
#endif	/* NZSKBD */

	case PROMDEV_TTYA:
	case PROMDEV_TTYB:
		zstty_unit = inSource - PROMDEV_TTYA;
		zs_unit = 0;
		channel = zstty_unit & 1;
		console = &zscn;
		break;

	}
	/* Now that inSource has been validated, print it. */
	printf("console is %s\n", prom_inSrc_name[inSource]);

	zc = zs_get_chan_addr(zs_unit, channel);
	if (zc == NULL) {
		printf("cninit: zs not mapped.\n");
		return;
	}
	zs_conschan = zc;
	zs_hwflags[zs_unit][channel] = ZS_HWFLAG_CONSOLE;
	/* switch to selected console */
	cn_tab = console;
	(*cn_tab->cn_probe)(cn_tab);
	(*cn_tab->cn_init)(cn_tab);
#ifdef	KGDB
	zs_kgdb_init();
#endif
}
