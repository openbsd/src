/*	$OpenBSD: aucat.c,v 1.109 2011/04/16 11:51:48 ratchov Exp $	*/
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

#ifdef DEBUG
int debug_level = 0;
#endif
volatile int quit_flag = 0;

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
		} else if (strncmp("duplex", p, len) == 0) {
			/* XXX: backward compat, remove this */
			mode |= MODE_REC | MODE_PLAY;
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

/*
 * stream configuration
 */
struct cfstr {
	SLIST_ENTRY(cfstr) entry;
	unsigned mode;			/* bitmap of MODE_XXX */
	struct aparams ipar;		/* input (read) parameters */
	struct aparams opar;		/* output (write) parameters */
	unsigned vol;			/* last requested volume */
	int hdr;			/* header format */
	int xrun;			/* overrun/underrun policy */
	int mmc;			/* MMC mode */
	int join;			/* join/expand enabled */
	char *path;			/* static path (no need to copy it) */
};

SLIST_HEAD(cfstrlist, cfstr);

/*
 * midi device (control stream)
 */
struct cfmid {
	SLIST_ENTRY(cfmid) entry;
	char *path;			/* static path (no need to copy it) */
};

SLIST_HEAD(cfmidlist, cfmid);

/*
 * audio device configuration
 */
struct cfdev {
	SLIST_ENTRY(cfdev) entry;
	struct cfstrlist ins;		/* files to play */
	struct cfstrlist outs;		/* files to record */
	struct cfstrlist opts;		/* subdevices to expose */
	struct cfmidlist mids;		/* midi ports to subscribe */
	struct aparams ipar;		/* input (read) parameters */
	struct aparams opar;		/* output (write) parameters */
	unsigned hold;			/* open immediately */
	unsigned bufsz;			/* par.bufsz for sio device */
	unsigned round;			/* par.round for sio device */
	unsigned mode;			/* bitmap of MODE_XXX */
	char *path;			/* static path (no need to copy it) */
};

SLIST_HEAD(cfdevlist, cfdev);

void
cfdev_add(struct cfdevlist *list, struct cfdev *templ, char *path)
{
	struct cfdev *cd;

	cd = malloc(sizeof(struct cfdev));
	if (cd == NULL) {
		perror("malloc");
		abort();
	}
	*cd = *templ;
	cd->path = path;
	SLIST_INSERT_HEAD(list, cd, entry);
	SLIST_INIT(&templ->ins);
	SLIST_INIT(&templ->outs);
	SLIST_INIT(&templ->opts);
	SLIST_INIT(&templ->mids);
}

void
cfstr_add(struct cfstrlist *list, struct cfstr *templ, char *path)
{
	size_t len;
	struct cfstr *cs;
	unsigned hdr;

	if (templ->hdr == HDR_AUTO) {
		len = strlen(path);
		if (len >= 4 && strcasecmp(path + len - 4, ".wav") == 0)
			hdr = HDR_WAV;
		else
			hdr = HDR_RAW;
	} else
		hdr = templ->hdr;
	cs = malloc(sizeof(struct cfstr));
	if (cs == NULL) {
		perror("malloc");
		abort();
	}
	*cs = *templ;
	cs->path = path;
	cs->hdr = hdr;
	SLIST_INSERT_HEAD(list, cs, entry);
}

void
cfmid_add(struct cfmidlist *list, char *path)
{
	struct cfmid *cm;
	
	cm = malloc(sizeof(struct cfmid));
	if (cm == NULL) {
		perror("malloc");
		abort();
	}
	cm->path = path;
	SLIST_INSERT_HEAD(list, cm, entry);
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

void
aucat_usage(void)
{
	(void)fputs("usage: " PROG_AUCAT " [-dlnu] [-a flag] [-b nframes] "
	    "[-C min:max] [-c min:max] [-e enc]\n\t"
	    "[-f device] [-h fmt] [-i file] [-j flag] [-m mode]"
	    "[-o file] [-q device]\n\t"
	    "[-r rate] [-s name] [-t mode] [-U unit] "
	    "[-v volume] [-x policy]\n\t"
	    "[-z nframes]\n",
	    stderr);
}

int
aucat_main(int argc, char **argv)
{
	struct cfdevlist cfdevs;
	struct cfmid *cm;
	struct cfstr *cs;
	struct cfdev *cd;
	struct listen *listen = NULL;
	int c, u_flag, d_flag, l_flag, n_flag, unit;
	char base[PATH_MAX], path[PATH_MAX];
	unsigned mode, rate;
	const char *str;
	int autostart;
	struct dev *d, *dnext;
	unsigned active;
	unsigned nsock, nfile;

	/*
	 * global options defaults
	 */
	unit = -1;
	u_flag = 0;
	d_flag = 0;
	l_flag = 0;
	n_flag = 0;
	SLIST_INIT(&cfdevs);
	nfile = nsock = 0;

	/*
	 * default stream params
	 */
	cs = malloc(sizeof(struct cfstr));
	if (cs == NULL) {
		perror("malloc");
		exit(1);
	}
	aparams_init(&cs->ipar, 0, 1, DEFAULT_RATE);
	aparams_init(&cs->opar, 0, 1, DEFAULT_RATE);
	cs->mmc = 0;
	cs->hdr = HDR_AUTO;
	cs->xrun = XRUN_IGNORE;
	cs->vol = MIDI_MAXCTL;
	cs->mode = MODE_PLAY | MODE_REC;
	cs->join = 1;

	/*
	 * default device
	 */
	cd = malloc(sizeof(struct cfdev));
	if (cd == NULL) {
		perror("malloc");
		exit(1);
	}
	aparams_init(&cd->ipar, 0, 1, DEFAULT_RATE);
	aparams_init(&cd->opar, 0, 1, DEFAULT_RATE);
	SLIST_INIT(&cd->ins);
	SLIST_INIT(&cd->outs);
	SLIST_INIT(&cd->opts);
	SLIST_INIT(&cd->mids);
	cd->path = NULL;
	cd->bufsz = 0;
	cd->round = 0;
	cd->hold = 1;

	while ((c = getopt(argc, argv, "a:dnb:c:C:e:r:h:x:v:i:o:f:m:luq:s:U:t:j:z:")) != -1) {
		switch (c) {
		case 'd':
#ifdef DEBUG
			if (d_flag)
				debug_level++;
#endif
			d_flag = 1;
			break;
		case 'n':
			n_flag = 1;
			break;
		case 'u':
			u_flag = 1;
			break;
		case 'U':
			unit = strtonum(optarg, 0, MIDI_MAXCTL, &str);
			if (str)
				errx(1, "%s: unit number is %s", optarg, str);
			break;
		case 'm':
			cs->mode = opt_mode();
			cd->mode = cs->mode;
			break;
		case 'h':
			cs->hdr = opt_hdr();
			break;
		case 'x':
			cs->xrun = opt_xrun();
			break;
		case 'j':
			cs->join = opt_onoff();
			break;
		case 't':
			cs->mmc = opt_mmc();
			break;
		case 'c':
			opt_ch(&cs->ipar);
			cd->opar.cmin = cs->ipar.cmin;
			cd->opar.cmax = cs->ipar.cmax;
			break;
		case 'C':
			opt_ch(&cs->opar);
			cd->ipar.cmin = cs->opar.cmin;
			cd->ipar.cmax = cs->opar.cmax;
			break;
		case 'e':
			opt_enc(&cs->ipar);
			aparams_copyenc(&cs->opar, &cs->ipar);
			break;
		case 'r':
			rate = strtonum(optarg, RATE_MIN, RATE_MAX, &str);
			if (str)
				errx(1, "%s: rate is %s", optarg, str);
			cs->opar.rate = cs->ipar.rate = rate;
			cd->ipar.rate = cd->opar.rate = rate;
			break;
		case 'v':
			cs->vol = strtonum(optarg, 0, MIDI_MAXCTL, &str);
			if (str)
				errx(1, "%s: volume is %s", optarg, str);
			break;
		case 'i':
			cfstr_add(&cd->ins, cs, optarg);
			nfile++;
			break;
		case 'o':
			cfstr_add(&cd->outs, cs, optarg);
			nfile++;
			break;
		case 's':
			cfstr_add(&cd->opts, cs, optarg);
			nsock++;
			break;
		case 'a':
			cd->hold = opt_onoff();
			break;
		case 'q':
			cfmid_add(&cd->mids, optarg);
			break;
		case 'b':
			cd->bufsz = strtonum(optarg, 1, RATE_MAX * 5, &str);
			if (str)
				errx(1, "%s: buffer size is %s", optarg, str);
			break;
		case 'z':
			cd->round = strtonum(optarg, 1, SHRT_MAX, &str);
			if (str)
				errx(1, "%s: block size is %s", optarg, str);
			break;
		case 'f':
			if (SLIST_EMPTY(&cd->opts) &&
			    SLIST_EMPTY(&cd->ins) &&
			    SLIST_EMPTY(&cd->outs)) {
				cfstr_add(&cd->opts, cs, DEFAULT_OPT);
				nsock++;
			}
			cfdev_add(&cfdevs, cd, optarg);
			break;
		case 'l':
			l_flag = 1;
			autostart = 0;
			break;
		default:
			aucat_usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

#ifdef DEBUG
	if (debug_level == 0)
		debug_level = 1;
#endif
	if (argc > 0) {
		aucat_usage();
		exit(1);
	}

	/*
	 * Check constraints specific to -n option
	 */
	if (n_flag) {
		if (!SLIST_EMPTY(&cfdevs) ||
		    !SLIST_EMPTY(&cd->mids) ||
		    !SLIST_EMPTY(&cd->opts))
			errx(1, "-f, -s, and -q not allowed in loopback mode");
		if (SLIST_EMPTY(&cd->ins) || SLIST_EMPTY(&cd->outs))
			errx(1, "-i and -o are required in loopback mode");
	}

	/*
	 * If there's no device specified, do as if the default
	 * device is specified as last argument.
	 */
	if (SLIST_EMPTY(&cfdevs)) {
		if (SLIST_EMPTY(&cd->opts) &&
		    SLIST_EMPTY(&cd->ins) &&
		    SLIST_EMPTY(&cd->outs)) {
			cfstr_add(&cd->opts, cs, DEFAULT_OPT);
			nsock++;
		}
		if (!cd->hold)
			errx(1, "-a off not compatible with default device");
		cfdev_add(&cfdevs, cd, "default");
	}
	if ((cs = SLIST_FIRST(&cd->opts)) ||
	    (cs = SLIST_FIRST(&cd->ins)) ||
	    (cs = SLIST_FIRST(&cd->outs)))
		errx(1, "%s: no device to attach the stream to", cs->path);

	/*
	 * Check modes and calculate "best" device parameters. Iterate over all
	 * inputs and outputs and find the maximum sample rate and channel
	 * number.
	 */
	SLIST_FOREACH(cd, &cfdevs, entry) {
		mode = 0;
		SLIST_FOREACH(cs, &cd->ins, entry) {
			if (cs->mode == 0)
				errx(1, "%s: not in play mode", cs->path);
			mode |= (cs->mode & MODE_PLAY);
			if (!u_flag)
				aparams_grow(&cd->opar, &cs->ipar);
		}
		SLIST_FOREACH(cs, &cd->outs, entry) {
			if (cs->mode == 0)
				errx(1, "%s: not in rec/mon mode", cs->path);
			if ((cs->mode & MODE_REC) && (cs->mode & MODE_MON))
				errx(1, "%s: can't rec and mon", cs->path);
			mode |= (cs->mode & MODE_RECMASK);
			if (!u_flag)
				aparams_grow(&cd->ipar, &cs->opar);
		}
		SLIST_FOREACH(cs, &cd->opts, entry) {
			if ((cs->mode & MODE_REC) && (cs->mode & MODE_MON))
				errx(1, "%s: can't rec and mon", cs->path);
			mode |= (cs->mode & (MODE_RECMASK | MODE_PLAY));
			if (!u_flag) {
				aparams_grow(&cd->opar, &cs->ipar);
				aparams_grow(&cd->ipar, &cs->opar);
			}
		}
		if ((mode & MODE_MON) && !(mode & MODE_PLAY))
			errx(1, "no playback stream to monitor");
		if (n_flag && (mode & MODE_MON))
			errx(1, "-m mon not allowed in loopback mode");	
		rate = (mode & MODE_REC) ? cd->ipar.rate : cd->opar.rate;
		if (!cd->round)
			cd->round = rate / 15;
		if (!cd->bufsz)
			cd->bufsz = rate / 15 * 4;
		cd->mode = mode;
	}
	if (nsock > 0) {
		getbasepath(base, sizeof(base));
		if (unit < 0)
			unit = 0;
	}
	setsig();
	filelist_init();

	/*
	 * Open devices
	 */
	while (!SLIST_EMPTY(&cfdevs)) {
		cd = SLIST_FIRST(&cfdevs);
		SLIST_REMOVE_HEAD(&cfdevs, entry);

		if (n_flag) {
			d = dev_new_loop(&cd->ipar, &cd->opar, cd->bufsz);
		} else {
			d = dev_new_sio(cd->path, cd->mode | MODE_MIDIMASK,
			    &cd->ipar, &cd->opar, cd->bufsz, cd->round,
			    cd->hold);
		}
		if (d == NULL)
			errx(1, "%s: can't open device", cd->path);

		/*
		 * register midi devices
		 */
		while (!SLIST_EMPTY(&cd->mids)) {
			cm = SLIST_FIRST(&cd->mids);
			SLIST_REMOVE_HEAD(&cd->mids, entry);
			if (!dev_thruadd(d, cm->path, 1, 1))
				errx(1, "%s: can't open device", cm->path);
			free(cm);
		}

		/*
		 * register files
		 */
		autostart = 0;
		while (!SLIST_EMPTY(&cd->ins)) {
			cs = SLIST_FIRST(&cd->ins);
			SLIST_REMOVE_HEAD(&cd->ins, entry);
			if (!cs->mmc)
				autostart = 1;
			if (strcmp(cs->path, "-") == 0)
				cs->path = NULL;
			if (!wav_new_in(&wav_ops, d, cs->mode & MODE_PLAY,
				cs->path, cs->hdr, &cs->ipar, cs->xrun,
				cs->vol, cs->mmc, cs->join))
				exit(1);
			free(cs);
		}
		while (!SLIST_EMPTY(&cd->outs)) {
			cs = SLIST_FIRST(&cd->outs);
			SLIST_REMOVE_HEAD(&cd->outs, entry);
			if (!cs->mmc)
				autostart = 1;
			if (strcmp(cs->path, "-") == 0)
				cs->path = NULL;
			if (!wav_new_out(&wav_ops, d, cs->mode & MODE_RECMASK,
				cs->path, cs->hdr, &cs->opar, cs->xrun,
				cs->mmc, cs->join))
				exit(1);
			free(cs);
		}
		while (!SLIST_EMPTY(&cd->opts)) {
			cs = SLIST_FIRST(&cd->opts);
			SLIST_REMOVE_HEAD(&cd->opts, entry);
			opt_new(cs->path, d, &cs->opar, &cs->ipar,
			    MIDI_TO_ADATA(cs->vol), cs->mmc,
			    cs->join, cs->mode | MODE_MIDIMASK);
			free(cs);
		}
		free(cd);
		if (autostart) {
			/*
			 * inject artificial mmc start
			 */
			ctl_start(d->midi);
		}
	}
	if (nsock > 0) {
		snprintf(path, sizeof(path), "%s/%s%u", base,
		    AUCAT_PATH, unit);
		listen = listen_new(&listen_ops, path);
		if (listen == NULL)
			exit(1);
	}
	if (geteuid() == 0)
		privdrop();
	if (l_flag) {
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
			if (d->pstate != DEV_CLOSED && !ctl_idle(d->midi))
				active = 1;
		}
		if (dev_list == NULL)
			break;
		if (nsock == 0 && !active)
			break;
		if (!file_poll())
			break;
	}
  fatal:
	if (nsock > 0)
		file_close(&listen->file);
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
	if (nsock > 0) {
		if (rmdir(base) < 0 && errno != ENOTEMPTY && errno != EPERM)
			warn("rmdir(\"%s\")", base);
	}
	unsetsig();
	return 0;
}

void
midicat_usage(void)
{
	(void)fputs("usage: " PROG_MIDICAT " [-dl] "
	    "[-i file] [-o file] [-q port] [-s name] [-U unit]\n",
	    stderr);
}

int
midicat_main(int argc, char **argv)
{
	struct cfdevlist cfdevs;
	struct cfmid *cm;
	struct cfstr *cs;
	struct cfdev *cd;
	struct listen *listen = NULL;
	int c, d_flag, l_flag, unit, fd;
	char base[PATH_MAX], path[PATH_MAX];
	struct file *stdx;
	struct aproc *p;
	struct abuf *buf;
	const char *str;
	struct dev *d, *dnext;
	unsigned nsock;

	/*
	 * global options defaults
	 */
	unit = -1;
	d_flag = 0;
	l_flag = 0;
	SLIST_INIT(&cfdevs);
	nsock = 0;
	
	/*
	 * default stream params
	 */
	cs = malloc(sizeof(struct cfstr));
	if (cs == NULL) {
		perror("malloc");
		exit(1);
	}
	cs->hdr = HDR_RAW;

	/*
	 * default device
	 */
	cd = malloc(sizeof(struct cfdev));
	if (cd == NULL) {
		perror("malloc");
		exit(1);
	}
	SLIST_INIT(&cd->ins);
	SLIST_INIT(&cd->outs);
	SLIST_INIT(&cd->opts);
	SLIST_INIT(&cd->mids);
	cd->path = NULL;


	while ((c = getopt(argc, argv, "di:o:ls:q:U:")) != -1) {
		switch (c) {
		case 'd':
#ifdef DEBUG
			if (d_flag)
				debug_level++;
#endif
			d_flag = 1;
			break;
		case 'i':
			cfstr_add(&cd->ins, cs, optarg);
			break;
		case 'o':
			cfstr_add(&cd->outs, cs, optarg);
			break;
		case 'q':
			cfmid_add(&cd->mids, optarg);
			break;
		case 's':
			cfstr_add(&cd->opts, cs, optarg);
			cfdev_add(&cfdevs, cd, optarg);
			nsock++;
			break;
		case 'l':
			l_flag = 1;
			break;
		case 'U':
			unit = strtonum(optarg, 0, MIDI_MAXCTL, &str);
			if (str)
				errx(1, "%s: unit number is %s", optarg, str);
			break;
		default:
			midicat_usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

#ifdef DEBUG
	if (debug_level == 0)
		debug_level = 1;
#endif
	if (argc > 0) {
		midicat_usage();
		exit(1);
	}

	/*
	 * If there's no device specified (-s), then create one with
	 * reasonable defaults:
	 *
	 *  - if there are no streams (-ioq) defined, assume server mode
	 *    and expose the "defaut" option
	 *
	 *  - if there are files (-io) but no ports (-q) to send/receive
	 *    from, add the default sndio(7) MIDI port
	 */
	if (SLIST_EMPTY(&cfdevs)) {
		if (SLIST_EMPTY(&cd->mids)) {
			if (!SLIST_EMPTY(&cd->ins) || !SLIST_EMPTY(&cd->outs))
			    	cfmid_add(&cd->mids, "default");
			else {
				cfstr_add(&cd->opts, cs, DEFAULT_OPT);
				nsock++;
			}
		}
		cfdev_add(&cfdevs, cd, "default");
	}
	if (nsock > 0) {
		getbasepath(base, sizeof(path));
		if (unit < 0)
			unit = 0;
	}
	setsig();
	filelist_init();

	while (!SLIST_EMPTY(&cfdevs)) {
		cd = SLIST_FIRST(&cfdevs);
		SLIST_REMOVE_HEAD(&cfdevs, entry);

		d = dev_new_thru();
		if (d == NULL)
			errx(1, "%s: can't open device", cd->path);
		if (!dev_ref(d))
			errx(1, "couldn't open midi thru box");
		if (SLIST_EMPTY(&cd->opts) && APROC_OK(d->midi))
			d->midi->flags |= APROC_QUIT;

		/*
		 * register midi ports
		 */
		while (!SLIST_EMPTY(&cd->mids)) {
			cm = SLIST_FIRST(&cd->mids);
			SLIST_REMOVE_HEAD(&cd->mids, entry);
			if (!dev_thruadd(d, cm->path, 1, 1))
				errx(1, "%s: can't open device", cm->path);
			free(cm);
		}

		/*
		 * register files
		 */
		while (!SLIST_EMPTY(&cd->ins)) {
			cs = SLIST_FIRST(&cd->ins);
			SLIST_REMOVE_HEAD(&cd->ins, entry);
			if (strcmp(cs->path, "-") == 0) {
				fd = STDIN_FILENO;
				if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
					warn("stdin");
			} else {
				fd = open(cs->path, O_RDONLY | O_NONBLOCK, 0666);
				if (fd < 0)
					err(1, "%s", cs->path);
			}
			stdx = (struct file *)pipe_new(&pipe_ops, fd, cs->path);
			p = rfile_new(stdx);
			buf = abuf_new(MIDI_BUFSZ, &aparams_none);
			aproc_setout(p, buf);
			dev_midiattach(d, buf, NULL);
			free(cs);
		}
		while (!SLIST_EMPTY(&cd->outs)) {
			cs = SLIST_FIRST(&cd->outs);
			SLIST_REMOVE_HEAD(&cd->outs, entry);
			if (strcmp(cs->path, "-") == 0) {
				fd = STDOUT_FILENO;
				if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
					warn("stdout");
			} else {
				fd = open(cs->path,
				    O_WRONLY | O_TRUNC | O_CREAT | O_NONBLOCK, 0666);
				if (fd < 0)
					err(1, "%s", cs->path);
			}
			stdx = (struct file *)pipe_new(&pipe_ops, fd, cs->path);
			p = wfile_new(stdx);
			buf = abuf_new(MIDI_BUFSZ, &aparams_none);
			aproc_setin(p, buf);
			dev_midiattach(d, NULL, buf);
			free(cs);
		}
		while (!SLIST_EMPTY(&cd->opts)) {
			cs = SLIST_FIRST(&cd->opts);
			SLIST_REMOVE_HEAD(&cd->opts, entry);
			opt_new(cs->path, d, NULL, NULL, 0, 0, 0, MODE_MIDIMASK);
			free(cs);
		}
		free(cd);
	}
	if (nsock > 0) {
		snprintf(path, sizeof(path), "%s/%s%u", base,
		    MIDICAT_PATH, unit);
		listen = listen_new(&listen_ops, path);
		if (listen == NULL)
			exit(1);
	}
	if (geteuid() == 0)
		privdrop();
	if (l_flag) {
#ifdef DEBUG
		debug_level = 0;
		dbg_flush();
#endif
		if (daemon(0, 0) < 0)
			err(1, "daemon");
	}

	/*
	 * loop, start processing
	 */
	for (;;) {
		if (quit_flag)
			break;
		for (d = dev_list; d != NULL; d = dnext) {
			dnext = d->next;
			if (!dev_run(d))
				goto fatal;
		}
		if (!file_poll())
			break;
	}
  fatal:
	if (nsock > 0)
		file_close(&listen->file);
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
	if (nsock > 0) {
		if (rmdir(base) < 0 && errno != ENOTEMPTY && errno != EPERM)
			warn("rmdir(\"%s\")", base);
	}
	unsetsig();
	return 0;
}

int
main(int argc, char **argv)
{
	char *prog;

#ifdef DEBUG
	atexit(dbg_flush);
#endif
	prog = strrchr(argv[0], '/');
	if (prog == NULL)
		prog = argv[0];
	else
		prog++;
	if (strcmp(prog, PROG_AUCAT) == 0) {
		return aucat_main(argc, argv);
	} else if (strcmp(prog, PROG_MIDICAT) == 0) {
		return midicat_main(argc, argv);
	} else {
		fprintf(stderr, "%s: can't determine program to run\n", prog);
	}
	return 1;
}
