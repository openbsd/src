/*	$OpenBSD: aucat.c,v 1.120 2011/10/12 07:20:03 ratchov Exp $	*/
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
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
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
#include "listen.h"
#include "midi.h"
#include "opt.h"
#include "wav.h"
#ifdef DEBUG
#include "dbg.h"
#endif

/*
 * unprivileged user name
 */
#define SNDIO_USER	"_sndio"

/*
 * priority when run as root
 */
#define SNDIO_PRIO	(-20)

#define PROG_AUCAT	"aucat"
#define PROG_MIDICAT	"midicat"

/*
 * sample rate if no ``-r'' is used
 */
#ifndef DEFAULT_RATE
#define DEFAULT_RATE	44100
#endif

/*
 * block size if no ``-z'' is used
 */
#ifndef DEFAULT_ROUND
#define DEFAULT_ROUND	(44100 / 15)
#endif

#ifdef DEBUG
volatile sig_atomic_t debug_level = 1;
#endif
volatile sig_atomic_t quit_flag = 0;

char aucat_usage[] = "usage: " PROG_AUCAT " [-dlMn] [-a flag] [-b nframes] "
    "[-C min:max] [-c min:max] [-e enc]\n\t"
    "[-f device] [-h fmt] [-i file] [-j flag] [-L addr] [-m mode] "
    "[-o file]\n\t"
    "[-q port] [-r rate] [-s name] [-t mode] [-U unit] [-v volume]\n\t"
    "[-w flag] [-x policy] [-z nframes]\n";

char midicat_usage[] = "usage: " PROG_MIDICAT " [-dlM] [-a flag] "
    "[-i file] [-L addr] [-s name] [-o file]\n\t"
    "[-q port] [-U unit]\n";

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

unsigned
opt_mode(void)
{
	unsigned mode = 0;
	char *p = optarg;
	size_t len;

	for (p = optarg; *p != '\0'; p++) {
		len = strcspn(p, ",");
		if (strncmp("play", p, len) == 0) {
			mode |= MODE_PLAY;
		} else if (strncmp("rec", p, len) == 0) {
			mode |= MODE_REC;
		} else if (strncmp("mon", p, len) == 0) {
			mode |= MODE_MON;
		} else if (strncmp("midi", p, len) == 0) {
			mode |= MODE_MIDIMASK;
		} else 
			errx(1, "%s: bad mode", optarg);
		p += len;
		if (*p == '\0')
			break;
	}
	if (mode == 0)
		errx(1, "empty mode");
	return mode;
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

void
getbasepath(char *base, size_t size)
{
	uid_t uid;
	struct stat sb;
	mode_t mask;

	uid = geteuid();
	if (uid == 0) {
		mask = 022;
		snprintf(base, PATH_MAX, "/tmp/aucat");
	} else {
		mask = 077;
		snprintf(base, PATH_MAX, "/tmp/aucat-%u", uid);
	}
	if (mkdir(base, 0777 & ~mask) < 0) {
		if (errno != EEXIST)
			err(1, "mkdir(\"%s\")", base);
	}
	if (stat(base, &sb) < 0)
		err(1, "stat(\"%s\")", base);
	if (sb.st_uid != uid || (sb.st_mode & mask) != 0)
		errx(1, "%s has wrong permissions", base);
}

void
privdrop(void)
{
	struct passwd *pw;
	struct stat sb;

	if ((pw = getpwnam(SNDIO_USER)) == NULL)
		errx(1, "unknown user %s", SNDIO_USER);
	if (stat(pw->pw_dir, &sb) < 0)
		err(1, "stat(\"%s\")", pw->pw_dir);
	if (sb.st_uid != 0 || (sb.st_mode & 022) != 0)
		errx(1, "%s has wrong permissions", pw->pw_dir);
	if (setpriority(PRIO_PROCESS, 0, SNDIO_PRIO) < 0)
		err(1, "setpriority");
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		err(1, "cannot drop privileges");
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
		path = "default";
	}
	if (!bufsz) {
		if (!round)
			round = DEFAULT_ROUND;
		bufsz = round * 4;
	} else if (!round)
		round = bufsz / 4;
	return dev_new(path, mode, bufsz, round, hold, autovol);
}

struct opt *
mkopt(char *path, struct dev *d, struct aparams *rpar, struct aparams *ppar,
    int mode, int vol, int mmc, int join)
{
	struct opt *o;

	if (d->reqmode & MODE_LOOP)
		errx(1, "%s: can't attach to loopback", path);
	if (d->reqmode & MODE_THRU)
		mode = MODE_MIDIMASK;
	if (!rpar->rate)
		ppar->rate = rpar->rate = DEFAULT_RATE;
	o = opt_new(path, d, rpar, ppar, MIDI_TO_ADATA(vol), mmc, join, mode);
	if (o == NULL)
		errx(1, "%s: couldn't create subdev", path);
	dev_adjpar(d, o->mode, rpar, ppar);
	return o;
}

int
main(int argc, char **argv)
{
	char *prog, *un_path, *optstr, *usagestr;
	int c, background, unit, server, tcp_port, active;
	char base[PATH_MAX], path[PATH_MAX];
	unsigned mode, hdr, xrun, rate, join, mmc, vol;
	unsigned hold, autovol, bufsz, round;
	const char *str;
	struct aparams ppar, rpar;
	struct dev *d, *dnext;
	struct listen *l;
	struct wav *w;

	/*
	 * global options defaults
	 */
	hdr = HDR_AUTO;
	xrun = XRUN_IGNORE;
	vol = MIDI_MAXCTL;
	hold = join = autovol = 1;
	mmc = 0;
	bufsz = 0;
	round = 0;
	unit = 0;
	background = 0;
	aparams_init(&ppar, 0, 1, DEFAULT_RATE);
	aparams_init(&rpar, 0, 1, DEFAULT_RATE);
	server = 0;

#ifdef DEBUG
	atexit(dbg_flush);
#endif
	setsig();
	filelist_init();

	prog = strrchr(argv[0], '/');
	if (prog == NULL)
		prog = argv[0];
	else
		prog++;
	if (strcmp(prog, PROG_AUCAT) == 0) {
		mode = MODE_MIDIMASK | MODE_PLAY | MODE_REC;
		optstr = "a:b:c:C:de:f:h:i:j:lL:m:Mno:q:r:s:t:U:v:w:x:z:t:j:z:";
		usagestr = aucat_usage;
		un_path = AUCAT_PATH;
		tcp_port = AUCAT_PORT;
	} else if (strcmp(prog, PROG_MIDICAT) == 0) {
		mode = MODE_MIDIMASK | MODE_THRU;
		optstr = "a:di:lL:Mo:q:s:U:";
		usagestr = midicat_usage;
		un_path = MIDICAT_PATH;
		tcp_port = MIDICAT_PORT;
		mkdev("midithru", MODE_THRU, 0, 0, 1, 0);
	} else {
		fprintf(stderr, "%s: can't determine program to run\n", prog);
		return 1;
	}


	while ((c = getopt(argc, argv, optstr)) != -1) {
		switch (c) {
		case 'd':
#ifdef DEBUG
			if (debug_level < 4)
				debug_level++;
#endif
			break;
		case 'U':
			if (server)
				errx(1, "-U must come before server options");
			unit = strtonum(optarg, 0, MIDI_MAXCTL, &str);
			if (str)
				errx(1, "%s: unit number is %s", optarg, str);
			server = 1;
			break;
		case 'L':
			listen_new_tcp(optarg, tcp_port + unit);
			server = 1;
			break;
		case 'm':
			mode = opt_mode();
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
		case 's':
			d = mkdev(NULL, 0, bufsz, round, 1, autovol);
			mkopt(optarg, d, &rpar, &ppar,
			    mode, vol, mmc, join);
			/* XXX: set device rate, if never set */
			server = 1;
			break;
		case 'q':
			d = mkdev(NULL, mode, bufsz, round, 1, autovol);
			if (!devctl_add(d, optarg, MODE_MIDIMASK))
				errx(1, "%s: can't open port", optarg);
			d->reqmode |= MODE_MIDIMASK;
			break;
		case 'a':
			hold = opt_onoff();
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
			mkdev("midithru", MODE_THRU, 0, 0, 1, 0);
			break;
		case 'l':
			background = 1;
			break;
		default:
			fputs(usagestr, stderr);
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0) {
		fputs(usagestr, stderr);
		exit(1);
	}
	if (wav_list == NULL) {
		if (opt_list == NULL) {
			d = mkdev(NULL, 0, bufsz, round, 1, autovol);
			mkopt("default", d, &rpar, &ppar,
			    mode, vol, mmc, join);
			server = 1;
		}
	} else {
		d = mkdev(NULL, 0, bufsz, round, 1, autovol);
		if ((d->reqmode & MODE_THRU) && !d->ctl_list) {
			if (!devctl_add(d, "default", MODE_MIDIMASK))
				errx(1, "%s: can't open port", optarg);
			d->reqmode |= MODE_MIDIMASK;
		}
	}
	if (server) {
		getbasepath(base, sizeof(base));
		snprintf(path, PATH_MAX, "%s/%s%u", base, un_path, unit);
		listen_new_un(path);
		if (geteuid() == 0)
			privdrop();
	}
	for (w = wav_list; w != NULL; w = w->next) {
		if (!wav_init(w))
			exit(1);
	}
	for (d = dev_list; d != NULL; d = d->next) {
		if (!dev_init(d))
			exit(1);
		if (d->autostart && (d->mode & MODE_AUDIOMASK))
			ctl_start(d->midi);
	}
	for (l = listen_list; l != NULL; l = l->next) {
		if (!listen_init(l))
			exit(1);
	}
	if (background) {
#ifdef DEBUG
		debug_level = 0;
		dbg_flush();
#endif
		if (daemon(0, 0) < 0)
			err(1, "daemon");
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
			if ((d->mode & MODE_THRU) ||
			    (d->pstate != DEV_CLOSED && !ctl_idle(d->midi)))
				active = 1;
		}
		if (dev_list == NULL)
			break;
		if (!server && !active)
			break;
		if (!file_poll())
			break;
	}
  fatal:
	while (listen_list != NULL)
		file_close(&listen_list->file);

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
	if (server) {
		if (rmdir(base) < 0 && errno != ENOTEMPTY && errno != EPERM)
			warn("rmdir(\"%s\")", base);
	}
	unsetsig();
	return 0;
}
