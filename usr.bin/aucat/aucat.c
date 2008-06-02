/*	$OpenBSD: aucat.c,v 1.24 2008/06/02 17:08:51 ratchov Exp $	*/
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
 *	(not yet)add a silent/quiet/verbose/whatever flag, but be sure
 *	that by default the user is notified when one of the following
 *	(cpu consuming) aproc is created: mix, sub, conv
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
 *	(hard) dont create mix (sub) if there's only one input (output)
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
 *
 *	(easy) do we need -d flag ?
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

/*
 * Format for file headers.
 */
#define HDR_AUTO	0	/* guess by looking at the file name */
#define HDR_RAW		1	/* no headers, ie openbsd native ;-) */
#define HDR_WAV		2	/* microsoft riff wave */

int debug_level = 0;
volatile int quit_flag = 0;

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
	    "\t[-r rate]\n",
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
	int fd;			/* file descriptor for I/O */
	struct aproc *proc;	/* rpipe_xxx our wpipe_xxx */
	struct abuf *buf;
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
	fa->proc = NULL;
	SLIST_INSERT_HEAD(list, fa, entry);
}

/*
 * Open an input file and setup converter if necessary.
 */
void
newinput(struct farg *fa, struct aparams *npar, unsigned nfr, int quiet_flag)
{
	int fd;
	struct file *f;
	struct aproc *p, *c;
	struct abuf *buf, *nbuf;

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
	if (fa->hdr == HDR_WAV) {
		if (!wav_readhdr(fd, &fa->par, &f->rbytes))
			exit(1);
	}
	buf = abuf_new(nfr, aparams_bpf(&fa->par));
	p = rpipe_new(f);
	aproc_setout(p, buf);
	if (!aparams_eq(&fa->par, npar)) {
		if (!quiet_flag) {
			fprintf(stderr, "%s: ", fa->name);
			aparams_print2(&fa->par, npar);
			fprintf(stderr, "\n");
		}
		nbuf = abuf_new(nfr, aparams_bpf(npar));
		c = conv_new(fa->name, &fa->par, npar);
		aproc_setin(c, buf);
		aproc_setout(c, nbuf);
		fa->buf = nbuf;
	} else
		fa->buf = buf;
	fa->proc = p;
	fa->fd = fd;
}

/*
 * Open an output file and setup converter if necessary.
 */
void
newoutput(struct farg *fa, struct aparams *npar, unsigned nfr, int quiet_flag)
{
	int fd;
	struct file *f;
	struct aproc *p, *c;
	struct abuf *buf, *nbuf;

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
	if (fa->hdr == HDR_WAV) {
		f->wbytes = WAV_DATAMAX;
		if (!wav_writehdr(fd, &fa->par))
			exit(1);
	}
	buf = abuf_new(nfr, aparams_bpf(&fa->par));
	p = wpipe_new(f);
	aproc_setin(p, buf);
	if (!aparams_eq(&fa->par, npar)) {
		if (!quiet_flag) {
			fprintf(stderr, "%s: ", fa->name);
			aparams_print2(npar, &fa->par);
			fprintf(stderr, "\n");
		}
		c = conv_new(fa->name, npar, &fa->par);
		nbuf = abuf_new(nfr, aparams_bpf(npar));
		aproc_setin(c, nbuf);
		aproc_setout(c, buf);
		fa->buf = nbuf;
	} else
		fa->buf = buf;
	fa->proc = p;
	fa->fd = fd;
}

void
sighdl(int s)
{
	if (quit_flag)
		_exit(1);
	quit_flag = 1;
}

int
main(int argc, char **argv)
{
	sigset_t sigset;
	struct sigaction sa;
	int c, u_flag, quiet_flag, ohdr, ihdr, ixrun, oxrun;
	struct farg *fa;
	struct farglist  ifiles, ofiles;
	struct aparams ipar, opar, dipar, dopar, cipar, copar;
	unsigned ivol, ovol;
	unsigned dinfr, donfr, cinfr, confr;
	char *devpath, *dbgenv;
	unsigned n;
	struct aproc *rec, *play, *mix, *sub, *conv;
	struct file *dev, *f;
	struct abuf *buf, *cbuf;
	const char *errstr;
	int fd;

	dbgenv = getenv("AUCAT_DEBUG");
	if (dbgenv) {
		debug_level = strtonum(dbgenv, 0, 4, &errstr);
		if (errstr)
			errx(1, "AUCAT_DEBUG is %s: %s", errstr, dbgenv);
	}

	aparams_init(&ipar, 0, 1, 44100);
	aparams_init(&opar, 0, 1, 44100);

	u_flag = quiet_flag = 0;
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

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sighdl;
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "sigaction");

	sigemptyset(&sigset);
	(void)sigaddset(&sigset, SIGTSTP);
	(void)sigaddset(&sigset, SIGCONT);
	if (sigprocmask(SIG_BLOCK, &sigset, NULL))
		err(1, "sigprocmask");
	
	file_start();
	play = rec = mix = sub = NULL;

	aparams_init(&cipar, CHAN_MAX, 0, RATE_MIN);
	aparams_init(&copar, CHAN_MAX, 0, RATE_MAX);

	/*
	 * Iterate over all inputs and outputs and find the maximum
	 * sample rate and channel number.
	 */
	SLIST_FOREACH(fa, &ifiles, entry) {
		if (cipar.cmin > fa->par.cmin)
			cipar.cmin = fa->par.cmin;
		if (cipar.cmax < fa->par.cmax)
			cipar.cmax = fa->par.cmax;
		if (cipar.rate < fa->par.rate)
			cipar.rate = fa->par.rate;
	}	
	SLIST_FOREACH(fa, &ofiles, entry) {
		if (copar.cmin > fa->par.cmin)
			copar.cmin = fa->par.cmin;
		if (copar.cmax < fa->par.cmax)
			copar.cmax = fa->par.cmax;
		if (copar.rate > fa->par.rate)
			copar.rate = fa->par.rate;
	}

	/*
	 * Open the device and increase the maximum sample rate.
	 * channel number to include those used by the device
	 */
	if (!u_flag) {
		dipar = copar;
		dopar = cipar;
	}
	fd = dev_init(devpath,
	    !SLIST_EMPTY(&ofiles) ? &dipar : NULL,
	    !SLIST_EMPTY(&ifiles) ? &dopar : NULL, &dinfr, &donfr);
	if (fd < 0)
		exit(1);
	if (!SLIST_EMPTY(&ofiles)) {
		if (!quiet_flag) {
			fprintf(stderr, "%s: recording ", devpath);
			aparams_print(&dipar);
			fprintf(stderr, "\n");
		}
		if (copar.cmin > dipar.cmin)
			copar.cmin = dipar.cmin;
		if (copar.cmax < dipar.cmax)
			copar.cmax = dipar.cmax;
		if (copar.rate > dipar.rate)
			copar.rate = dipar.rate;
		dinfr *= DEFAULT_NBLK;
		DPRINTF("%s: using %ums rec buffer\n", devpath,
		    1000 * dinfr / dipar.rate);
	}
	if (!SLIST_EMPTY(&ifiles)) {
		if (!quiet_flag) {
			fprintf(stderr, "%s: playing ", devpath);
			aparams_print(&dopar);
			fprintf(stderr, "\n");
		}
		if (cipar.cmin > dopar.cmin)
			cipar.cmin = dopar.cmin;
		if (cipar.cmax < dopar.cmax)
			cipar.cmax = dopar.cmax;
		if (cipar.rate < dopar.rate)
			cipar.rate = dopar.rate;
		donfr *= DEFAULT_NBLK;
		DPRINTF("%s: using %ums play buffer\n", devpath,
		    1000 * donfr / dopar.rate);
	}
	
	/*
	 * Create buffers for the device.
	 */
	dev = file_new(fd, devpath);
	if (!SLIST_EMPTY(&ofiles)) {
		rec = rpipe_new(dev);
		sub = sub_new();
	}
	if (!SLIST_EMPTY(&ifiles)) {
		play = wpipe_new(dev);
		mix = mix_new();
	}

	/*
	 * Calculate sizes of buffers using "common" parameters, to
	 * have roughly the same duration as device buffers.
	 */
	cinfr = donfr * cipar.rate / dopar.rate;
	confr = dinfr * copar.rate / dipar.rate;

	/*
	 * Create buffers for all input and output pipes.
	 */
	SLIST_FOREACH(fa, &ifiles, entry) {
		newinput(fa, &cipar, cinfr, quiet_flag);
		if (mix) {
			aproc_setin(mix, fa->buf);
			fa->buf->xrun = fa->xrun;
		}
		if (!quiet_flag) {
			fprintf(stderr, "%s: reading ", fa->name);
			aparams_print(&fa->par);
			fprintf(stderr, "\n");
		}
	}
	SLIST_FOREACH(fa, &ofiles, entry) {
		newoutput(fa, &copar, confr, quiet_flag);
		if (sub) {
			aproc_setout(sub, fa->buf);
			fa->buf->xrun = fa->xrun;
		}
		if (!quiet_flag) {
			fprintf(stderr, "%s: writing ", fa->name);
			aparams_print(&fa->par);
			fprintf(stderr, "\n");
		}
	}

	/*
	 * Connect the multiplexer to the device input.
	 */
	if (sub) {
		buf = abuf_new(dinfr, aparams_bpf(&dipar));
		aproc_setout(rec, buf);
		if (!aparams_eq(&copar, &dipar)) {
			if (!quiet_flag) {
				fprintf(stderr, "%s: ", devpath);
				aparams_print2(&dipar, &copar);
				fprintf(stderr, "\n");
			}
			conv = conv_new("subconv", &dipar, &copar);
			cbuf = abuf_new(confr, aparams_bpf(&copar));
			aproc_setin(conv, buf);
			aproc_setout(conv, cbuf);
			aproc_setin(sub, cbuf);
		} else
			aproc_setin(sub, buf);
	}

	/*
	 * Normalize input levels and connect the mixer to the device
	 * output.
	 */
	if (mix) {
		n = 0;
		SLIST_FOREACH(fa, &ifiles, entry)
			n++;
		SLIST_FOREACH(fa, &ifiles, entry)
			fa->buf->mixvol /= n;
		buf = abuf_new(donfr, aparams_bpf(&dopar));
		aproc_setin(play, buf);
		if (!aparams_eq(&cipar, &dopar)) {
			if (!quiet_flag) {
				fprintf(stderr, "%s: ", devpath);
				aparams_print2(&cipar, &dopar);
				fprintf(stderr, "\n");
			}
			conv = conv_new("mixconv", &cipar, &dopar);
			cbuf = abuf_new(cinfr, aparams_bpf(&cipar));
			aproc_setout(conv, buf);
			aproc_setin(conv, cbuf);
			aproc_setout(mix, cbuf);
		} else
			aproc_setout(mix, buf);
	}

	/*
	 * start audio
	 */
	if (play != NULL) {
		if (!quiet_flag)
			fprintf(stderr, "filling buffers...\n");
		buf = LIST_FIRST(&play->ibuflist);
		while (!quit_flag) {
			/* no more devices to poll */
			if (!file_poll())
				break;
			/* eof */
			if (dev->state & FILE_EOF)
				break;
			/* device is blocked and play buffer is full */
			if ((dev->events & POLLOUT) && !ABUF_WOK(buf))
				break;
		}
	}
	if (!quiet_flag)
		fprintf(stderr, "starting device...\n");
	dev_start(dev->fd);
	if (mix)
		mix->u.mix.flags |= MIX_DROP;
	if (sub)
		sub->u.sub.flags |= SUB_DROP;
	while (!quit_flag) {
		if (!file_poll())
			break;
	}

	if (!quiet_flag)
		fprintf(stderr, "draining buffers...\n");

	/*
	 * generate EOF on all files that do input, so
	 * once buffers are drained, everything will be cleaned
	 */
	LIST_FOREACH(f, &file_list, entry) {
		if ((f->events) & POLLIN || (f->state & FILE_ROK))
			file_eof(f);
	}
	for (;;) {
		if (!file_poll())
			break;
	}
	SLIST_FOREACH(fa, &ofiles, entry) {
		if (fa->hdr == HDR_WAV)
			wav_writehdr(fa->fd, &fa->par);
		close(fa->fd);
		DPRINTF("%s: closed\n", fa->name);
	}
	dev_stop(dev->fd);
	file_stop();
	return 0;
}
