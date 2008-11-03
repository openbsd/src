/*	$OpenBSD: aucat.c,v 1.31 2008/11/03 22:25:13 ratchov Exp $	*/
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
/*
 * TODO:
 *
 *	(hard) use parsable encoding names instead of the lookup
 *	table. For instance, [s|u]bits[le|be][/bytes{msb|lsb}], example
 *	s8, s16le, s24le/3msb. This would give names that correspond to
 *	what use most linux-centric apps, but for which we have an
 *	algorithm to convert the name to a aparams structure.
 *
 *	(easy) uses {chmin-chmax} instead of chmin:chmax notation for
 *	channels specification to match the notation used in rmix.
 *
 *	(easy) use comma-separated parameters syntax, example:
 *	s24le/3msb,{3-6},48000 so we don't have to use three -e, -r, -c
 *	flags, but only one -p flag that specify one or more parameters.
 *
 *	(hard) if all inputs are over, the mixer terminates and closes
 *	the write end of the device. It should continue writing zeros
 *	until the recording is over (or be able to stop write end of
 *	the device)
 *
 *	(hard) implement -n flag (no device) to connect all inputs to
 *	the outputs.
 *
 *	(hard) ignore input files that are not audible (because channels
 *	they provide are not used on the output). Similarly ignore
 *	outputs that are zero filled (because channels they consume are
 *	not provided).
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>

#include <signal.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <varargs.h>

#include "conf.h"
#include "aparams.h"
#include "aproc.h"
#include "abuf.h"
#include "wav.h"
#include "listen.h"
#include "dev.h"

int debug_level = 0;
volatile int quit_flag = 0;

/*
 * SIGINT handler, it raises the quit flag. If the flag is already set,
 * that means that the last SIGINT was not handled, because the process
 * is blocked somewhere, so exit
 */
void
sigint(int s)
{
	if (quit_flag)
		_exit(1);
	quit_flag = 1;
}

/*
 * increase debug level on SIGUSR1
 */
void
sigusr1(int s)
{
	if (debug_level < 4)
		debug_level++;
}

/*
 * decrease debug level on SIGUSR2
 */
void
sigusr2(int s)
{
	if (debug_level > 0)
		debug_level--;
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-lu] [-b nsamples] [-C min:max] [-c min:max] [-e enc] "
	    "[-f device]\n"
	    "\t[-h fmt] [-i file] [-o file] [-r rate] [-x policy]\n",
	    __progname);
}

void
opt_ch(struct aparams *par)
{
	if (sscanf(optarg, "%u:%u", &par->cmin, &par->cmax) != 2 ||
	    par->cmax < par->cmin || par->cmax > NCHAN_MAX - 1)
		err(1, "%s: bad channel range", optarg);
}

void
opt_rate(struct aparams *par)
{
	if (sscanf(optarg, "%u", &par->rate) != 1 ||
	    par->rate < RATE_MIN || par->rate > RATE_MAX)
		err(1, "%s: bad sample rate", optarg);
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
	err(1, "%s: bad header specification", optarg);
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
	errx(1, "%s: onderrun/overrun policy", optarg);
}

/*
 * Arguments of -i and -o opations are stored in a list.
 */
struct farg {
	SLIST_ENTRY(farg) entry;
	struct aparams par;	/* last requested format */
	unsigned vol;		/* last requested volume */
	char *name;		/* optarg pointer (no need to copy it */
	int hdr;		/* header format */
	int xrun;		/* overrun/underrun policy */
};

SLIST_HEAD(farglist, farg);

/*
 * Add a farg entry to the given list, corresponding
 * to the given file name.
 */
void
opt_file(struct farglist *list, 
    struct aparams *par, unsigned vol, int hdr, int xrun, char *optarg)
{
	struct farg *fa;
	size_t namelen;
	
	fa = malloc(sizeof(struct farg));
	if (fa == NULL)
		err(1, "%s", optarg);

	if (hdr == HDR_AUTO) {
		namelen = strlen(optarg);
		if (namelen >= 4 && 
		    strcasecmp(optarg + namelen - 4, ".wav") == 0) {
			fa->hdr = HDR_WAV;
			DPRINTF("%s: assuming wav file format\n", optarg);
		} else {
			fa->hdr = HDR_RAW;
			DPRINTF("%s: assuming headerless file\n", optarg);
		}
	} else 
		fa->hdr = hdr;
	fa->xrun = xrun;
	fa->par = *par;
	fa->vol = vol;
	fa->name = optarg;
	SLIST_INSERT_HEAD(list, fa, entry);
}

/*
 * Open an input file and setup converter if necessary.
 */
void
newinput(struct farg *fa)
{
	int fd;
	struct wav *f;
	struct aproc *proc;
	struct abuf *buf;
	unsigned nfr;

	if (strcmp(fa->name, "-") == 0) {
		fd = STDIN_FILENO;
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
			warn("stdin");
		fa->name = "stdin";
	} else {
		fd = open(fa->name, O_RDONLY | O_NONBLOCK, 0666);
		if (fd < 0)
			err(1, "%s", fa->name);
	}
	/*
	 * XXX : we should round rate, right ?
	 */
	f = wav_new_in(&wav_ops, fd, fa->name, &fa->par, fa->hdr);
	nfr = dev_bufsz * fa->par.rate / dev_rate;
	buf = abuf_new(nfr, &fa->par);
	proc = rpipe_new((struct file *)f);
	aproc_setout(proc, buf);
	abuf_fill(buf); /* XXX: move this in dev_attach() ? */
	dev_attach(fa->name, buf, &fa->par, fa->xrun, NULL, NULL, 0);
}

/*
 * Open an output file and setup converter if necessary.
 */
void
newoutput(struct farg *fa)
{
	int fd;
	struct wav *f;
	struct aproc *proc;
	struct abuf *buf;
	unsigned nfr;

	if (strcmp(fa->name, "-") == 0) {
		fd = STDOUT_FILENO;
		if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
			warn("stdout");
		fa->name = "stdout";
	} else {
		fd = open(fa->name,
		    O_WRONLY | O_TRUNC | O_CREAT | O_NONBLOCK, 0666);
		if (fd < 0)
			err(1, "%s", fa->name);
	}
	/*
	 * XXX : we should round rate, right ?
	 */
	f = wav_new_out(&wav_ops, fd, fa->name, &fa->par, fa->hdr);
	nfr = dev_bufsz * fa->par.rate / dev_rate;
	proc = wpipe_new((struct file *)f);
	buf = abuf_new(nfr, &fa->par);
	aproc_setin(proc, buf);
	dev_attach(fa->name, NULL, NULL, 0, buf, &fa->par, fa->xrun);
}

int
main(int argc, char **argv)
{
	int c, u_flag, l_flag, hdr, xrun;
	struct farg *fa;
	struct farglist  ifiles, ofiles;
	struct aparams ipar, opar, dipar, dopar;
	struct sigaction sa;
	unsigned ivol, ovol, bufsz = 0;
	char *devpath, *dbgenv, *listenpath;
	const char *errstr;
	extern char *malloc_options;

	malloc_options = "FGJ";

	dbgenv = getenv("AUCAT_DEBUG");
	if (dbgenv) {
		debug_level = strtonum(dbgenv, 0, 4, &errstr);
		if (errstr)
			errx(1, "AUCAT_DEBUG is %s: %s", errstr, dbgenv);
	}

	aparams_init(&ipar, 0, 1, 44100);
	aparams_init(&opar, 0, 1, 44100);

	u_flag = 0;
	l_flag = 0;
	devpath = NULL;
	SLIST_INIT(&ifiles);
	SLIST_INIT(&ofiles);
	hdr = HDR_AUTO;
	xrun = XRUN_IGNORE;
	ivol = ovol = MIDI_TO_ADATA(127);

	while ((c = getopt(argc, argv, "b:c:C:e:r:h:x:i:o:f:lu"))
	    != -1) {
		switch (c) {
		case 'h':
			hdr = opt_hdr();
			break;
		case 'x':
			xrun = opt_xrun();
			break;
		case 'c':
			opt_ch(&ipar);
			break;
		case 'C':
			opt_ch(&opar);
			break;
		case 'e':
			opt_enc(&ipar);
			aparams_copyenc(&opar, &ipar);
			break;
		case 'r':
			opt_rate(&ipar);
			opar.rate = ipar.rate;
			break;
		case 'i':
			opt_file(&ifiles, &ipar, 127, hdr, xrun, optarg);
			break;
		case 'o':
			opt_file(&ofiles, &opar, 127, hdr, xrun, optarg);
			break;
		case 'f':
			if (devpath)
				err(1, "only one -f allowed");
			devpath = optarg;
			dipar = opar;
			dopar = ipar;
			break;
		case 'l':
			l_flag = 1;
			break;
		case 'u':
			u_flag = 1;
			break;
		case 'b':
			if (sscanf(optarg, "%u", &bufsz) != 1) {
				fprintf(stderr, "%s: bad buf size\n", optarg);
				exit(1);
			}
			break;
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (!devpath) {
		dipar = ipar;
		dopar = opar;
	}

	if (!l_flag && SLIST_EMPTY(&ifiles) &&
	    SLIST_EMPTY(&ofiles) && argc > 0) {
		/*
		 * Legacy mode: if no -i or -o options are provided, and
		 * there are arguments then assume the arguments are files
		 * to play.
		 */
		for (c = 0; c < argc; c++)
			if (legacy_play(devpath, argv[c]) != 0) {
				errx(1, "%s: could not play\n", argv[c]);
			}
		exit(0);
	} else if (argc > 0) {
		usage();
		exit(1);
	}

	if (l_flag && (!SLIST_EMPTY(&ofiles) || !SLIST_EMPTY(&ifiles)))
		errx(1, "can't use -l with -o or -i");

	if (!u_flag && !l_flag) {
		/*
		 * Calculate "best" device parameters. Iterate over all
		 * inputs and outputs and find the maximum sample rate
		 * and channel number.
		 */
		aparams_init(&dipar, NCHAN_MAX - 1, 0, RATE_MAX);
		aparams_init(&dopar, NCHAN_MAX - 1, 0, RATE_MIN);
		SLIST_FOREACH(fa, &ifiles, entry) {
			if (dopar.cmin > fa->par.cmin)
				dopar.cmin = fa->par.cmin;
			if (dopar.cmax < fa->par.cmax)
				dopar.cmax = fa->par.cmax;
			if (dopar.rate < fa->par.rate)
				dopar.rate = fa->par.rate;
		}
		SLIST_FOREACH(fa, &ofiles, entry) {
			if (dipar.cmin > fa->par.cmin)
				dipar.cmin = fa->par.cmin;
			if (dipar.cmax < fa->par.cmax)
				dipar.cmax = fa->par.cmax;
			if (dipar.rate > fa->par.rate)
				dipar.rate = fa->par.rate;
		}
	}

	quit_flag = 0;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigint;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		DPRINTF("sigaction(int) failed\n");
#ifdef DEBUG
	sa.sa_handler = sigusr1;
	if (sigaction(SIGUSR1, &sa, NULL) < 0)
		DPRINTF("sigaction(usr1) failed\n");
	sa.sa_handler = sigusr2;
	if (sigaction(SIGUSR2, &sa, NULL) < 0)
		DPRINTF("sigaction(usr2) failed1n");
#endif
	filelist_init();

	/*
	 * Open the device.
	 */
	dev_init(devpath, 
	    (l_flag || !SLIST_EMPTY(&ofiles)) ? &dipar : NULL,
	    (l_flag || !SLIST_EMPTY(&ifiles)) ? &dopar : NULL,
	    bufsz);

	if (l_flag) {
		listenpath = getenv("AUCAT_SOCKET");
		if (!listenpath)
			listenpath = DEFAULT_SOCKET;
		(void)listen_new(&listen_ops, listenpath);
	}

	/*
	 * Create buffers for all input and output pipes.
	 */
	while (!SLIST_EMPTY(&ifiles)) {
		fa = SLIST_FIRST(&ifiles);
		SLIST_REMOVE_HEAD(&ifiles, entry);
		newinput(fa);
		free(fa);
	}
	while (!SLIST_EMPTY(&ofiles)) {
		fa = SLIST_FIRST(&ofiles);
		SLIST_REMOVE_HEAD(&ofiles, entry);
		newoutput(fa);
		free(fa);
	}

	/*
	 * automatically terminate when there no are streams
	 */
	if (!l_flag) {
		if (dev_mix)
			dev_mix->u.mix.flags |= MIX_AUTOQUIT;
		if (dev_sub)
			dev_sub->u.sub.flags |= SUB_AUTOQUIT;
	}

	/*
	 * loop, start audio
	 */
	for (;;) {
		if (quit_flag) {
			if (l_flag)
				filelist_unlisten();
			break;
		}
		if (!file_poll())
			break;
	}

	dev_done();
	filelist_done();

       	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		DPRINTF("dev_done: sigaction failed\n");
	return 0;
}
