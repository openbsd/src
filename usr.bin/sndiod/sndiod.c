/*	$OpenBSD: sndiod.c,v 1.34 2018/08/08 22:31:43 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/socket.h>

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

#include "amsg.h"
#include "defs.h"
#include "dev.h"
#include "fdpass.h"
#include "file.h"
#include "listen.h"
#include "midi.h"
#include "opt.h"
#include "sock.h"
#include "utils.h"

/*
 * unprivileged user name
 */
#ifndef SNDIO_USER
#define SNDIO_USER	"_sndio"
#endif

/*
 * privileged user name
 */
#ifndef SNDIO_PRIV_USER
#define SNDIO_PRIV_USER	"_sndiop"
#endif

/*
 * priority when run as root
 */
#ifndef SNDIO_PRIO
#define SNDIO_PRIO	(-20)
#endif

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
#define DEFAULT_BUFSZ	7680
#endif

/*
 * default device in server mode
 */
#ifndef DEFAULT_DEV
#define DEFAULT_DEV "rsnd/0"
#endif

void sigint(int);
void opt_ch(int *, int *);
void opt_enc(struct aparams *);
int opt_mmc(void);
int opt_onoff(void);
int getword(char *, char **);
unsigned int opt_mode(void);
void getbasepath(char *);
void setsig(void);
void unsetsig(void);
struct dev *mkdev(char *, struct aparams *,
    int, int, int, int, int, int);
struct port *mkport(char *, int);
struct opt *mkopt(char *, struct dev *,
    int, int, int, int, int, int, int, int);

unsigned int log_level = 0;
volatile sig_atomic_t quit_flag = 0;

char usagestr[] = "usage: sndiod [-d] [-a flag] [-b nframes] "
    "[-C min:max] [-c min:max] [-e enc]\n\t"
    "[-f device] [-j flag] [-L addr] [-m mode] [-q port] [-r rate]\n\t"
    "[-s name] [-t mode] [-U unit] [-v volume] [-w flag] [-z nframes]\n";

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

void
opt_ch(int *rcmin, int *rcmax)
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
	if (cmin < 0 || cmax < cmin || cmax >= NCHAN_MAX)
		goto failed;
	*rcmin = cmin;
	*rcmax = cmax;
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
opt_mmc(void)
{
	if (strcmp("off", optarg) == 0)
		return 0;
	if (strcmp("slave", optarg) == 0)
		return 1;
	errx(1, "%s: off/slave expected", optarg);
}

int
opt_onoff(void)
{
	if (strcmp("off", optarg) == 0)
		return 0;
	if (strcmp("on", optarg) == 0)
		return 1;
	errx(1, "%s: on/off expected", optarg);
}

int
getword(char *word, char **str)
{
	char *p = *str;

	for (;;) {
		if (*word == '\0')
			break;
		if (*word++ != *p++)
			return 0;
	}
	if (*p == ',' || *p == '\0') {
		*str = p;
		return 1;
	}
	return 0;
}

unsigned int
opt_mode(void)
{
	unsigned int mode = 0;
	char *p = optarg;

	for (;;) {
		if (getword("play", &p)) {
			mode |= MODE_PLAY;
		} else if (getword("rec", &p)) {
			mode |= MODE_REC;
		} else if (getword("mon", &p)) {
			mode |= MODE_MON;
		} else if (getword("midi", &p)) {
			mode |= MODE_MIDIMASK;
		} else
			errx(1, "%s: bad mode", optarg);
		if (*p == '\0')
			break;
		p++;
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
}

void
unsetsig(void)
{
	struct sigaction sa;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
		err(1, "unsetsig(hup): sigaction failed");
	if (sigaction(SIGTERM, &sa, NULL) < 0)
		err(1, "unsetsig(term): sigaction failed");
	if (sigaction(SIGINT, &sa, NULL) < 0)
		err(1, "unsetsig(int): sigaction failed");
}

void
getbasepath(char *base)
{
	uid_t uid;
	struct stat sb;
	mode_t mask, omask;

	uid = geteuid();
	if (uid == 0) {
		mask = 022;
		snprintf(base, SOCKPATH_MAX, SOCKPATH_DIR);
	} else {
		mask = 077;
		snprintf(base, SOCKPATH_MAX, SOCKPATH_DIR "-%u", uid);
	}
	omask = umask(mask);
	if (mkdir(base, 0777) < 0) {
		if (errno != EEXIST)
			err(1, "mkdir(\"%s\")", base);
	}
	umask(omask);
	if (stat(base, &sb) < 0)
		err(1, "stat(\"%s\")", base);
	if (!S_ISDIR(sb.st_mode))
		errx(1, "%s is not a directory", base);
	if (sb.st_uid != uid || (sb.st_mode & mask) != 0)
		errx(1, "%s has wrong permissions", base);
}

struct dev *
mkdev(char *path, struct aparams *par,
    int mode, int bufsz, int round, int rate, int hold, int autovol)
{
	struct dev *d;

	for (d = dev_list; d != NULL; d = d->next) {
		if (strcmp(d->path, path) == 0)
			return d;
	}
	if (!bufsz && !round) {
		round = DEFAULT_ROUND;
		bufsz = DEFAULT_BUFSZ;
	} else if (!bufsz) {
		bufsz = round * 2;
	} else if (!round)
		round = bufsz / 2;
	d = dev_new(path, par, mode, bufsz, round, rate, hold, autovol);
	if (d == NULL)
		exit(1);
	return d;
}

struct port *
mkport(char *path, int hold)
{
	struct port *c;

	for (c = port_list; c != NULL; c = c->next) {
		if (strcmp(c->path, path) == 0)
			return c;
	}
	c = port_new(path, MODE_MIDIMASK, hold);
	if (c == NULL)
		exit(1);
	return c;
}

struct opt *
mkopt(char *path, struct dev *d,
    int pmin, int pmax, int rmin, int rmax,
    int mode, int vol, int mmc, int dup)
{
	struct opt *o;

	o = opt_new(d, path, pmin, pmax, rmin, rmax,
	    MIDI_TO_ADATA(vol), mmc, dup, mode);
	if (o == NULL)
		return NULL;
	dev_adjpar(d, o->mode, o->pmax, o->rmax);
	return o;
}

static void
dounveil(char *name, char *prefix, char *path_prefix)
{
	size_t prefix_len;
	char path[PATH_MAX];

	prefix_len = strlen(prefix);

	if (strncmp(name, prefix, prefix_len) != 0)
		errx(1, "%s: unsupported device or port format", name);
	snprintf(path, sizeof(path), "%s%s", path_prefix, name + prefix_len);
	if (unveil(path, "rw") < 0)
		err(1, "unveil");
}

static int
start_helper(int background)
{
	struct dev *d;
	struct port *p;
	struct passwd *pw;
	int s[2];
	pid_t pid;

	if (geteuid() == 0) {
		if ((pw = getpwnam(SNDIO_PRIV_USER)) == NULL)
			errx(1, "unknown user %s", SNDIO_PRIV_USER);
	} else
		pw = NULL;
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, s) < 0) {
		perror("socketpair");
		return 0;
	}
	pid = fork();
	if (pid	== -1) {
		log_puts("can't fork\n");
		return 0;
	}
	if (pid == 0) {
		setproctitle("helper");
		close(s[0]);
		if (fdpass_new(s[1], &helper_fileops) == NULL)
			return 0;
		if (background) {
			log_flush();
			log_level = 0;
			if (daemon(0, 0) < 0)
				err(1, "daemon");
		}
		if (pw != NULL) {
			if (setgroups(1, &pw->pw_gid) ||
			    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
			    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
				err(1, "cannot drop privileges");
		}
		for (d = dev_list; d != NULL; d = d->next)
			dounveil(d->path, "rsnd/", "/dev/audio");
		for (p = port_list; p != NULL; p = p->next)
			dounveil(p->path, "rmidi/", "/dev/rmidi");
		if (pledge("stdio sendfd rpath wpath", NULL) < 0)
			err(1, "pledge");
		while (file_poll())
			; /* nothing */
		exit(0);
	} else {
		close(s[1]);
		if (fdpass_new(s[0], &worker_fileops) == NULL)
			return 0;
	}
	return 1;
}

static void
stop_helper(void)
{
	if (fdpass_peer)
		fdpass_close(fdpass_peer);
}

int
main(int argc, char **argv)
{
	int c, background, unit;
	int pmin, pmax, rmin, rmax;
	char base[SOCKPATH_MAX], path[SOCKPATH_MAX];
	unsigned int mode, dup, mmc, vol;
	unsigned int hold, autovol, bufsz, round, rate;
	const char *str;
	struct aparams par;
	struct dev *d;
	struct port *p;
	struct listen *l;
	struct passwd *pw;
	struct tcpaddr {
		char *host;
		struct tcpaddr *next;
	} *tcpaddr_list, *ta;

	atexit(log_flush);

	/*
	 * global options defaults
	 */
	vol = 118;
	dup = 1;
	mmc = 0;
	hold = 0;
	autovol = 1;
	bufsz = 0;
	round = 0;
	rate = DEFAULT_RATE;
	unit = 0;
	background = 1;
	pmin = 0;
	pmax = 1;
	rmin = 0;
	rmax = 1;
	aparams_init(&par);
	mode = MODE_PLAY | MODE_REC;
	tcpaddr_list = NULL;

	while ((c = getopt(argc, argv, "a:b:c:C:de:f:j:L:m:q:r:s:t:U:v:w:x:z:")) != -1) {
		switch (c) {
		case 'd':
			log_level++;
			background = 0;
			break;
		case 'U':
			unit = strtonum(optarg, 0, 15, &str);
			if (str)
				errx(1, "%s: unit number is %s", optarg, str);
			break;
		case 'L':
			ta = xmalloc(sizeof(struct tcpaddr));
			ta->host = optarg;
			ta->next = tcpaddr_list;
			tcpaddr_list = ta;
			break;
		case 'm':
			mode = opt_mode();
			break;
		case 'j':
			dup = opt_onoff();
			break;
		case 't':
			mmc = opt_mmc();
			break;
		case 'c':
			opt_ch(&pmin, &pmax);
			break;
		case 'C':
			opt_ch(&rmin, &rmax);
			break;
		case 'e':
			opt_enc(&par);
			break;
		case 'r':
			rate = strtonum(optarg, RATE_MIN, RATE_MAX, &str);
			if (str)
				errx(1, "%s: rate is %s", optarg, str);
			break;
		case 'v':
			vol = strtonum(optarg, 0, MIDI_MAXCTL, &str);
			if (str)
				errx(1, "%s: volume is %s", optarg, str);
			break;
		case 's':
			if ((d = dev_list) == NULL) {
				d = mkdev(DEFAULT_DEV, &par, 0, bufsz, round,
				    rate, hold, autovol);
			}
			if (mkopt(optarg, d, pmin, pmax, rmin, rmax,
				mode, vol, mmc, dup) == NULL)
				return 1;
			break;
		case 'q':
			mkport(optarg, hold);
			break;
		case 'a':
			hold = opt_onoff();
			break;
		case 'w':
			autovol = opt_onoff();
			break;
		case 'b':
			bufsz = strtonum(optarg, 1, RATE_MAX, &str);
			if (str)
				errx(1, "%s: buffer size is %s", optarg, str);
			break;
		case 'z':
			round = strtonum(optarg, 1, SHRT_MAX, &str);
			if (str)
				errx(1, "%s: block size is %s", optarg, str);
			break;
		case 'f':
			mkdev(optarg, &par, 0, bufsz, round,
			    rate, hold, autovol);
			break;
		default:
			fputs(usagestr, stderr);
			return 1;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 0) {
		fputs(usagestr, stderr);
		return 1;
	}
	if (dev_list == NULL)
		mkdev(DEFAULT_DEV, &par, 0, bufsz, round, rate, hold, autovol);
	for (d = dev_list; d != NULL; d = d->next) {
		if (opt_byname(d, "default"))
			continue;
		if (mkopt("default", d, pmin, pmax, rmin, rmax,
			mode, vol, mmc, dup) == NULL)
			return 1;
	}

	setsig();
	filelist_init();

	if (!start_helper(background))
		return 1;

	if (geteuid() == 0) {
		if ((pw = getpwnam(SNDIO_USER)) == NULL)
			errx(1, "unknown user %s", SNDIO_USER);
	} else
		pw = NULL;
	getbasepath(base);
	snprintf(path, SOCKPATH_MAX, "%s/" SOCKPATH_FILE "%u", base, unit);
	if (!listen_new_un(path))
		return 1;
	for (ta = tcpaddr_list; ta != NULL; ta = ta->next) {
		if (!listen_new_tcp(ta->host, AUCAT_PORT + unit))
			return 1;
	}
	for (l = listen_list; l != NULL; l = l->next) {
		if (!listen_init(l))
			return 1;
	}
	midi_init();
	for (p = port_list; p != NULL; p = p->next) {
		if (!port_init(p))
			return 1;
	}
	for (d = dev_list; d != NULL; d = d->next) {
		if (!dev_init(d))
			return 1;
	}
	if (background) {
		log_flush();
		log_level = 0;
		if (daemon(0, 0) < 0)
			err(1, "daemon");
	}
	if (pw != NULL) {
		if (setpriority(PRIO_PROCESS, 0, SNDIO_PRIO) < 0)
			err(1, "setpriority");
		if (chroot(pw->pw_dir) != 0 || chdir("/") != 0)
			err(1, "cannot chroot to %s", pw->pw_dir);
		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			err(1, "cannot drop privileges");
	}
	if (tcpaddr_list) {
		if (pledge("stdio audio recvfd unix inet", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio audio recvfd unix", NULL) == -1)
			err(1, "pledge");
	}
	for (;;) {
		if (quit_flag)
			break;
		if (!fdpass_peer)
			break;
		if (!file_poll())
			break;
	}
	stop_helper();
	while (listen_list != NULL)
		listen_close(listen_list);
	while (sock_list != NULL)
		sock_close(sock_list);
	for (d = dev_list; d != NULL; d = d->next)
		dev_done(d);
	for (p = port_list; p != NULL; p = p->next)
		port_done(p);
	while (file_poll())
		; /* nothing */
	midi_done();

	while (dev_list)
		dev_del(dev_list);
	while (port_list)
		port_del(port_list);
	while (tcpaddr_list) {
		ta = tcpaddr_list;
		tcpaddr_list = ta->next;
		xfree(ta);
	}
	filelist_done();
	unsetsig();
	return 0;
}
