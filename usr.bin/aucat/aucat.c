/*	$OpenBSD: aucat.c,v 1.26 2008/08/14 09:58:55 ratchov Exp $	*/
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
#include "file.h"
#include "dev.h"

int debug_level = 0, quiet_flag = 0;
volatile int quit_flag = 0, pause_flag = 0;

void suspend(struct file *);
void fill(struct file *);
void flush(struct file *);

/*
 * List of allowed encodings and their names.
 */
struct enc {
	char *name;
	struct aparams par;
} enc_list[] = {
	/* name		bps,	bits,	le,	sign,	msb,	unused  */
	{ "s8",		{ 1, 	8,	1,	1,	1,	0, 0, 0 } },
	{ "u8",		{ 1,	8,	1,	0,	1,	0, 0, 0 } },
	{ "s16le",	{ 2,	16,	1,	1,	1,	0, 0, 0 } },
	{ "u16le",	{ 2,	16,	1,	0,	1,	0, 0, 0 } },
	{ "s16be",	{ 2,	16,	0,	1,	1,	0, 0, 0 } },
	{ "u16be",	{ 2,	16,	0,	0,	1,	0, 0, 0 } },
	{ "s24le",	{ 4,	24,	1,	1,	1,	0, 0, 0 } },
	{ "u24le",	{ 4,	24,	1,	0,	1,	0, 0, 0 } },
	{ "s24be",	{ 4,	24,	0,	1,	1,	0, 0, 0 } },
	{ "u24be",	{ 4,	24,	0,	0,	1,	0, 0, 0 } },
	{ "s32le",	{ 4,	32,	1,	1,	1,	0, 0, 0 } },
	{ "u32le",	{ 4,	32,	1,	0,	1,	0, 0, 0 } },
	{ "s32be",	{ 4,	32,	0,	1,	1,	0, 0, 0 } },
	{ "u32be",	{ 4,	32,	0,	0,	1,	0, 0, 0 } },
	{ "s24le3",	{ 3,	24,	1,	1,	1,	0, 0, 0 } },
	{ "u24le3",	{ 3,	24,	1,	0,	1,	0, 0, 0 } },
	{ "s24be3",	{ 3,	24,	0,	1,	1,	0, 0, 0 } },
	{ "u24be3",	{ 3,	24,	0,	0,	1,	0, 0, 0 } },
	{ "s20le3",	{ 3,	20,	1,	1,	1,	0, 0, 0 } },
	{ "u20le3",	{ 3,	20,	1,	0,	1,	0, 0, 0 } },
	{ "s20be3",	{ 3,	20,	0,	1,	1,	0, 0, 0 } },
	{ "u20be3",	{ 3,	20,	0,	0,	1,	0, 0, 0 } },
	{ "s18le3",	{ 3,	18,	1,	1,	1,	0, 0, 0 } },
	{ "u18le3",	{ 3,	18,	1,	0,	1,	0, 0, 0 } },
	{ "s18be3",	{ 3,	18,	0,	1,	1,	0, 0, 0 } },
	{ "u18be3",	{ 3,	18,	0,	0,	1,	0, 0, 0 } },
	{ NULL,		{ 0,	0,	0,	0,	0,	0, 0, 0 } }
};

/*
 * Search an encoding in the above table. On success fill encoding
 * part of "par" and return 1, otherwise return 0.
 */
unsigned
enc_lookup(char *name, struct aparams *par)
{
	struct enc *e;

	for (e = enc_list; e->name != NULL; e++) {
		if (strcmp(e->name, name) == 0) {
			par->bps = e->par.bps;
			par->bits = e->par.bits;
			par->sig = e->par.sig;
			par->le = e->par.le;
			par->msb = e->par.msb;
			return 1;
		}
	}
	return 0;
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-qu] [-C min:max] [-c min:max] [-d level] "
	    "[-E enc] [-e enc]\n"
	    "\t[-f device] [-H fmt] [-h fmt] [-i file] [-o file] [-R rate]\n"
	    "\t[-r rate] [-X policy] [-x policy]\n",
	    __progname);
}

void
opt_ch(struct aparams *par)
{
	if (sscanf(optarg, "%u:%u", &par->cmin, &par->cmax) != 2 ||
	    par->cmin > CHAN_MAX || par->cmax > CHAN_MAX ||
	    par->cmin > par->cmax)
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
	if (!enc_lookup(optarg, par))
		err(1, "%s: bad encoding", optarg);
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
	struct file *f;
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
	f = file_new(fd, fa->name);
	f->hdr = 0;
	f->hpar = fa->par;
	if (fa->hdr == HDR_WAV) {
		if (!wav_readhdr(fd, &f->hpar, &f->rbytes))
			exit(1);
	}
	nfr = dev_onfr * f->hpar.rate / dev_opar.rate;
	buf = abuf_new(nfr, aparams_bpf(&f->hpar));
	proc = rpipe_new(f);
	aproc_setout(proc, buf);
	dev_attach(fa->name, buf, &f->hpar, fa->xrun, NULL, NULL, 0);
}

/*
 * Open an output file and setup converter if necessary.
 */
void
newoutput(struct farg *fa)
{
	int fd;
	struct file *f;
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
	f = file_new(fd, fa->name);
	f->hdr = fa->hdr;
	f->hpar = fa->par;
	if (f->hdr == HDR_WAV) {
		f->wbytes = WAV_DATAMAX;
		if (!wav_writehdr(fd, &f->hpar))
			exit(1);
	}
	nfr = dev_infr * f->hpar.rate / dev_ipar.rate;
	proc = wpipe_new(f);
	buf = abuf_new(nfr, aparams_bpf(&f->hpar));
	aproc_setin(proc, buf);
	dev_attach(fa->name, NULL, NULL, 0, buf, &f->hpar, fa->xrun);
}

int
main(int argc, char **argv)
{
	int c, u_flag, ohdr, ihdr, ixrun, oxrun;
	struct farg *fa;
	struct farglist  ifiles, ofiles;
	struct aparams ipar, opar, dipar, dopar;
	unsigned ivol, ovol;
	char *devpath, *dbgenv;
	const char *errstr;

	dbgenv = getenv("AUCAT_DEBUG");
	if (dbgenv) {
		debug_level = strtonum(dbgenv, 0, 4, &errstr);
		if (errstr)
			errx(1, "AUCAT_DEBUG is %s: %s", errstr, dbgenv);
	}

	aparams_init(&ipar, 0, 1, 44100);
	aparams_init(&opar, 0, 1, 44100);

	u_flag = 0;
	devpath = NULL;
	SLIST_INIT(&ifiles);
	SLIST_INIT(&ofiles);
	ihdr = ohdr = HDR_AUTO;
	ixrun = oxrun = XRUN_IGNORE;
	ivol = ovol = MIDI_TO_ADATA(127);

	while ((c = getopt(argc, argv, "c:C:e:E:r:R:h:H:x:X:i:o:f:qu"))
	    != -1) {
		switch (c) {
		case 'h':
			ihdr = opt_hdr();
			break;
		case 'H':
			ohdr = opt_hdr();
			break;
		case 'x':
			ixrun = opt_xrun();
			break;
		case 'X':
			oxrun = opt_xrun();
			break;
		case 'c':
			opt_ch(&ipar);
			break;
		case 'C':
			opt_ch(&opar);
			break;
		case 'e':
			opt_enc(&ipar);
			break;
		case 'E':
			opt_enc(&opar);
			break;
		case 'r':
			opt_rate(&ipar);
			break;
		case 'R':
			opt_rate(&opar);
			break;
		case 'i':
			opt_file(&ifiles, &ipar, 127, ihdr, ixrun, optarg);
			break;
		case 'o':
			opt_file(&ofiles, &opar, 127, ohdr, oxrun, optarg);
			break;
		case 'f':
			if (devpath)
				err(1, "only one -f allowed");
			devpath = optarg;
			dipar = ipar;
			dopar = opar;
			break;
		case 'q':
			quiet_flag = 1;
			break;
		case 'u':
			u_flag = 1;
			break;
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (!devpath) {
		devpath = getenv("AUDIODEVICE");
		if (devpath == NULL)
			devpath = DEFAULT_DEVICE;
		dipar = ipar;
		dopar = opar;
	}

	if (SLIST_EMPTY(&ifiles) && SLIST_EMPTY(&ofiles) && argc > 0) {
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


	if (!u_flag) {
		/*
		 * Calculate "best" device parameters. Iterate over all
		 * inputs and outputs and find the maximum sample rate
		 * and channel number.
		 */
		aparams_init(&dipar, CHAN_MAX, 0, RATE_MAX);
		aparams_init(&dopar, CHAN_MAX, 0, RATE_MIN);
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
	file_start();

	/*
	 * Open the device, dev_init() will return new parameters
	 * that must be used by all inputs and outputs.
	 */
	dev_init(devpath, 
	    (!SLIST_EMPTY(&ofiles)) ? &dipar : NULL,
	    (!SLIST_EMPTY(&ifiles)) ? &dopar : NULL);

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
	 * Normalize input levels
	 */
	if (dev_mix)
		mix_setmaster(dev_mix);

	/*
	 * start audio
	 */
	if (!quiet_flag)
		fprintf(stderr, "starting device...\n");
	dev_start();
	if (!quiet_flag)
		fprintf(stderr, "process started...\n");
	dev_run(1);
	if (!quiet_flag)
		fprintf(stderr, "stopping device...\n");
	dev_done();

	file_stop();
	return 0;
}
