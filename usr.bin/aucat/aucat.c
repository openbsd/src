/*	$OpenBSD: aucat.c,v 1.145 2015/01/16 06:40:05 deraadt Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <sndio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "abuf.h"
#include "amsg.h"
#include "aparams.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "midi.h"
#include "wav.h"
#ifdef DEBUG
#include "dbg.h"
#endif

#define PROG_AUCAT	"aucat"

/*
 * sample rate if no ``-r'' is used
 */
#ifndef DEFAULT_RATE
#define DEFAULT_RATE	48000
#endif

/*
 * block size if neither ``-z'' nor ``-b'' is used
 */
#ifndef DEFAULT_ROUND
#define DEFAULT_ROUND	960
#endif

/*
 * buffer size if neither ``-z'' nor ``-b'' is used
 */
#ifndef DEFAULT_BUFSZ
#define DEFAULT_BUFSZ	7860
#endif

void sigint(int);
void sigusr1(int);
void sigusr2(int);
void opt_ch(struct aparams *);
void opt_enc(struct aparams *);
int opt_hdr(void);
int opt_mmc(void);
int opt_onoff(void);
int opt_xrun(void);
void setsig(void);
void unsetsig(void);
struct dev *mkdev(char *, int, int, int, int, int);

#ifdef DEBUG
volatile sig_atomic_t debug_level = 1;
#endif
volatile sig_atomic_t quit_flag = 0;

char aucat_usage[] = "usage: " PROG_AUCAT " [-dMn]\n\t"
    "[-C min:max] [-c min:max] [-e enc] [-f device]\n\t"
    "[-h fmt] [-i file] [-j flag] [-o file] [-q port]\n\t"
    "[-r rate] [-t mode] [-v volume] [-w flag] [-x policy]\n";

/*
 * SIGINT handler, it raises the quit flag. If the flag is already set,
 * that means that the last SIGINT was not handled, because the process
 * is blocked somewhere, so exit.
 */
void
sigint(int s)
{
	if (quit_flag)
		_exit(1);
	quit_flag = 1;
}

#ifdef DEBUG
/*
 * Increase debug level on SIGUSR1.
 */
void
sigusr1(int s)
{
	if (debug_level < 4)
		debug_level++;
}

/*
 * Decrease debug level on SIGUSR2.
 */
void
sigusr2(int s)
{
	if (debug_level > 0)
		debug_level--;
}
#endif

void
opt_ch(struct aparams *par)
{
	char *next, *end;
	long cmin, cmax;

	errno = 0;
	cmin = strtol(optarg, &next, 10);
	if (next == optarg || *next != ':')
		goto failed;
	cmax = strtol(++next, &end, 10);
	if (end == next || *end != '\0')
		goto failed;
	if (cmin < 0 || cmax < cmin || cmax > NCHAN_MAX)
		goto failed;
	par->cmin = cmin;
	par->cmax = cmax;
	return;
failed:
	errx(1, "%s: bad channel range", optarg);
}

void
opt_enc(struct aparams *par)
{
	int len;

	len = aparams_strtoenc(par, optarg);
	if (len == 0 || optarg[len] != '\0')
		errx(1, "%s: bad encoding", optarg);
}

int
opt_hdr(void)
{
	if (strcmp("auto", optarg) == 0)
		return HDR_AUTO;
	if (strcmp("raw", optarg) == 0)
		return HDR_RAW;
	if (strcmp("wav", optarg) == 0)
		return HDR_WAV;
	errx(1, "%s: bad header specification", optarg);
}

int
opt_mmc(void)
{
	if (strcmp("off", optarg) == 0)
		return 0;
	if (strcmp("slave", optarg) == 0)
		return 1;
	errx(1, "%s: bad MMC mode", optarg);
}

int
opt_onoff(void)
{
	if (strcmp("off", optarg) == 0)
		return 0;
	if (strcmp("on", optarg) == 0)
		return 1;
	errx(1, "%s: bad join/expand setting", optarg);
}

int
opt_xrun(void)
{
	if (strcmp("ignore", optarg) == 0)
		return XRUN_IGNORE;
	if (strcmp("sync", optarg) == 0)
		return XRUN_SYNC;
	if (strcmp("error", optarg) == 0)
		return XRUN_ERROR;
	errx(1, "%s: bad underrun/overrun policy", optarg);
}

void
setsig(void)
{
	struct sigaction sa;

	quit_flag = 0;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigint;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "sigaction(int) failed");
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		err(1, "sigaction(term) failed");
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err(1, "sigaction(hup) failed");
#ifdef DEBUG
	sa.sa_handler = sigusr1;
	if (sigaction(SIGUSR1, &sa, NULL) < 0)
		err(1, "sigaction(usr1) failed");
	sa.sa_handler = sigusr2;
	if (sigaction(SIGUSR2, &sa, NULL) < 0)
		err(1, "sigaction(usr2) failed1n");
#endif
}

void
unsetsig(void)
{
	struct sigaction sa;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
#ifdef DEBUG
	if (sigaction(SIGUSR2, &sa, NULL) < 0)
		err(1, "unsetsig(usr2): sigaction failed");
	if (sigaction(SIGUSR1, &sa, NULL) < 0)
		err(1, "unsetsig(usr1): sigaction failed");
#endif
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err(1, "unsetsig(hup): sigaction failed\n");
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		err(1, "unsetsig(term): sigaction failed\n");
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "unsetsig(int): sigaction failed\n");
}

struct dev *
mkdev(char *path, int mode, int bufsz, int round, int hold, int autovol)
{
	struct dev *d;

	if (path) {
		for (d = dev_list; d != NULL; d = d->next) {
			if (d->reqmode & (MODE_LOOP | MODE_THRU))
				continue;
			if (strcmp(d->path, path) == 0)
				return d;
		}
	} else {
		if (dev_list)
			return dev_list;
		path = SIO_DEVANY;
	}
	if (!bufsz && !round) {
		round = DEFAULT_ROUND;
		bufsz = DEFAULT_BUFSZ;
	} else if (!bufsz) {
		bufsz = round * 2;
	} else if (!round)
		round = bufsz / 2;
	d = dev_new(path, mode, bufsz, round, hold, autovol);
	if (d == NULL)
		exit(1);
	return d;
}

int
main(int argc, char **argv)
{
	int c, active;
	unsigned int mode, hdr, xrun, rate, join, mmc, vol;
	unsigned int hold, autovol, bufsz, round;
	const char *str;
	struct aparams ppar, rpar;
	struct dev *d, *dnext;
	struct wav *w;

	/*
	 * global options defaults
	 */
	hdr = HDR_AUTO;
	xrun = XRUN_IGNORE;
	vol = MIDI_MAXCTL;
	join = 1;
	mmc = 0;
	hold = 0;
	autovol = 1;
	bufsz = 0;
	round = 0;
	aparams_init(&ppar, 0, 1, DEFAULT_RATE);
	aparams_init(&rpar, 0, 1, DEFAULT_RATE);
	mode = MODE_MIDIMASK | MODE_PLAY | MODE_REC;

#ifdef DEBUG
	atexit(dbg_flush);
#endif
	setsig();
	filelist_init();

	while ((c = getopt(argc, argv,
		    "a:b:c:C:de:f:h:i:j:Mno:q:r:t:v:w:x:z:")) != -1) {
		switch (c) {
		case 'd':
#ifdef DEBUG
			if (debug_level < 4)
				debug_level++;
#endif
			break;
		case 'h':
			hdr = opt_hdr();
			break;
		case 'x':
			xrun = opt_xrun();
			break;
		case 'j':
			join = opt_onoff();
			break;
		case 't':
			mmc = opt_mmc();
			break;
		case 'c':
			opt_ch(&ppar);
			break;
		case 'C':
			opt_ch(&rpar);
			break;
		case 'e':
			opt_enc(&ppar);
			aparams_copyenc(&rpar, &ppar);
			break;
		case 'r':
			rate = strtonum(optarg, RATE_MIN, RATE_MAX, &str);
			if (str)
				errx(1, "%s: rate is %s", optarg, str);
			ppar.rate = rpar.rate = rate;
			break;
		case 'v':
			vol = strtonum(optarg, 0, MIDI_MAXCTL, &str);
			if (str)
				errx(1, "%s: volume is %s", optarg, str);
			break;
		case 'i':
			d = mkdev(NULL, 0, bufsz, round, 1, autovol);
			w = wav_new_in(&wav_ops, d,
			    mode & (MODE_PLAY | MODE_MIDIOUT), optarg,
			    hdr, &ppar, xrun, vol, mmc, join);
			if (w == NULL)
				errx(1, "%s: couldn't create stream", optarg);
			dev_adjpar(d, w->mode, NULL, &w->hpar);
			break;
		case 'o':	
			d = mkdev(NULL, 0, bufsz, round, 1, autovol);
			w = wav_new_out(&wav_ops, d,
			    mode & (MODE_RECMASK | MODE_MIDIIN), optarg,
			    hdr, &rpar, xrun, mmc, join);
			if (w == NULL)
				errx(1, "%s: couldn't create stream", optarg);
			dev_adjpar(d, w->mode, &w->hpar, NULL);
			break;
		case 'q':
			d = mkdev(NULL, mode, bufsz, round, 1, autovol);
			if (!devctl_add(d, optarg, MODE_MIDIMASK))
				errx(1, "%s: can't open port", optarg);
			d->reqmode |= MODE_MIDIMASK;
			break;
		case 'w':
			autovol = opt_onoff();
			break;
		case 'b':
			bufsz = strtonum(optarg, 1, RATE_MAX * 5, &str);
			if (str)
				errx(1, "%s: buffer size is %s", optarg, str);
			break;
		case 'z':
			round = strtonum(optarg, 1, SHRT_MAX, &str);
			if (str)
				errx(1, "%s: block size is %s", optarg, str);
			break;
		case 'f':
			mkdev(optarg, 0, bufsz, round, hold, autovol);
			break;
		case 'n':
			mkdev("loopback", MODE_LOOP, bufsz, round, 1, autovol);
			break;
		case 'M':
			mkdev("midithru", MODE_THRU, 0, 0, hold, 0);
			break;
		default:
			fputs(aucat_usage, stderr);
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0) {
		fputs(aucat_usage, stderr);
		exit(1);
	}
	if (wav_list) {
		if ((d = dev_list) && d->next)
			errx(1, "only one device allowed");
		if ((d->reqmode & MODE_THRU) && d->ctl_list == NULL) {
			if (!devctl_add(d, "default", MODE_MIDIMASK))
				errx(1, "%s: can't open port", optarg);
			d->reqmode |= MODE_MIDIMASK;
		}
	} else {
		fputs(aucat_usage, stderr);
		exit(1);
	}
	for (w = wav_list; w != NULL; w = w->next) {
		if (!wav_init(w))
			exit(1);
	}
	for (d = dev_list; d != NULL; d = d->next) {
		if (!dev_init(d))
			exit(1);
		if (d->autostart && (d->mode & MODE_AUDIOMASK))
			dev_mmcstart(d);
	}

	/*
	 * Loop, start audio.
	 */
	for (;;) {
		if (quit_flag)
			break;
		active = 0;
		for (d = dev_list; d != NULL; d = dnext) {
			dnext = d->next;
			if (!dev_run(d))
				goto fatal;
			if (d->refcnt > 0)
				active = 1;
		}
		if (dev_list == NULL)
			break;
		if (!active)
			break;
		if (!file_poll())
			break;
	}
  fatal:

	/*
	 * give a chance to drain
	 */
	for (d = dev_list; d != NULL; d = d->next)
		dev_drain(d);
	while (file_poll())
		; /* nothing */

	while (dev_list)
		dev_del(dev_list);
	filelist_done();
	unsetsig();
	return 0;
}
