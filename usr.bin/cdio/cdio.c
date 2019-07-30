/*	$OpenBSD: cdio.c,v 1.74 2015/01/16 06:40:06 deraadt Exp $	*/

/*  Copyright (c) 1995 Serge V. Vakulenko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Serge V. Vakulenko.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Compact Disc Control Utility by Serge V. Vakulenko <vak@cronyx.ru>.
 * Based on the non-X based CD player by Jean-Marc Zucconi and
 * Andrey A. Chernov.
 *
 * Fixed and further modified on 5-Sep-1995 by Jukka Ukkonen <jau@funet.fi>.
 *
 * 11-Sep-1995: Jukka A. Ukkonen <jau@funet.fi>
 *              A couple of further fixes to my own earlier "fixes".
 *
 * 18-Sep-1995: Jukka A. Ukkonen <jau@funet.fi>
 *              Added an ability to specify addresses relative to the
 *              beginning of a track. This is in fact a variation of
 *              doing the simple play_msf() call.
 *
 * 11-Oct-1995: Serge V.Vakulenko <vak@cronyx.ru>
 *              New eject algorithm.
 *              Some code style reformatting.
 *
 * $FreeBSD: cdcontrol.c,v 1.13 1996/06/25 21:01:27 ache Exp $
 */

#include <sys/param.h>	/* isset */
#include <sys/file.h>
#include <sys/cdio.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/scsiio.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <histedit.h>
#include <util.h>
#include <vis.h>

#include "extern.h"

#define ASTS_INVALID    0x00  /* Audio status byte not valid */
#define ASTS_PLAYING    0x11  /* Audio play operation in progress */
#define ASTS_PAUSED     0x12  /* Audio play operation paused */
#define ASTS_COMPLETED  0x13  /* Audio play operation successfully completed */
#define ASTS_ERROR      0x14  /* Audio play operation stopped due to error */
#define ASTS_VOID       0x15  /* No current audio status to return */

#ifndef DEFAULT_CD_DRIVE
#  define DEFAULT_CD_DRIVE  "cd0"
#endif

#define CMD_DEBUG       1
#define CMD_DEVICE      2
#define CMD_EJECT       3
#define CMD_HELP        4
#define CMD_INFO        5
#define CMD_PAUSE       6
#define CMD_PLAY        7
#define CMD_QUIT        8
#define CMD_RESUME      9
#define CMD_STOP        10
#define CMD_VOLUME      11
#define CMD_CLOSE       12
#define CMD_RESET       13
#define CMD_SET         14
#define CMD_STATUS      15
#define CMD_NEXT	16
#define CMD_PREV	17
#define CMD_REPLAY	18
#define CMD_CDDB	19
#define CMD_CDID	20
#define CMD_BLANK	21
#define CMD_CDRIP	22
#define CMD_CDPLAY	23

struct cmdtab {
	int command;
	char *name;
	int min;
	char *args;
} cmdtab[] = {
{ CMD_BLANK,	"blank",	1, "" },
{ CMD_CDDB,	"cddbinfo",     2, "[n]" },
{ CMD_CDID,	"cdid",		3, "" },
{ CMD_CDPLAY,	"cdplay",	3, "[track1-trackN ...]" },
{ CMD_CDRIP,	"cdrip",	3, "[track1-trackN ...]" },
{ CMD_CLOSE,    "close",        1, "" },
{ CMD_DEBUG,    "debug",        3, "on | off" },
{ CMD_DEVICE,   "device",       1, "devname" },
{ CMD_EJECT,    "eject",        1, "" },
{ CMD_QUIT,	"exit",		2, "" },
{ CMD_HELP,     "?",            1, 0 },
{ CMD_HELP,     "help",         1, "" },
{ CMD_INFO,     "info",         1, "" },
{ CMD_NEXT,	"next",		1, "" },
{ CMD_PAUSE,    "pause",        2, "" },
{ CMD_PLAY,     "play",         1, "[track1[.index1] [track2[.index2]]]" },
{ CMD_PLAY,     "play",         1, "[[tr1] m1:s1[.f1] [tr2] [m2:s2[.f2]]]" },
{ CMD_PLAY,     "play",         1, "[#block [len]]" },
{ CMD_PREV,	"previous",	2, "" },
{ CMD_QUIT,     "quit",         1, "" },
{ CMD_REPLAY,	"replay",	3, "" },
{ CMD_RESET,    "reset",        4, "" },
{ CMD_RESUME,   "resume",       1, "" },
{ CMD_SET,      "set",          2, "lba | msf" },
{ CMD_STATUS,   "status",       1, "" },
{ CMD_STOP,     "stop",         3, "" },
{ CMD_VOLUME,   "volume",       1, "left_channel right_channel" },
{ CMD_VOLUME,   "volume",       1, "left | right | mono | stereo | mute" },
{ 0, 0, 0, 0}
};

struct cd_toc_entry *toc_buffer;

char		*cdname;
int		fd = -1;
int		writeperm = 0;
u_int8_t	mediacap[MMC_FEATURE_MAX / NBBY];
int		verbose = 1;
int		msf = 1;
const char	*cddb_host;
char		**track_names;

EditLine	*el = NULL;	/* line-editing structure */
History		*hist = NULL;	/* line-editing history */
void		switch_el(void);

extern char	*__progname;

int		setvol(int, int);
int		read_toc_entrys(int);
int		play_msf(int, int, int, int, int, int);
int		play_track(int, int, int, int);
int		status(int *, int *, int *, int *);
int		is_wave(int);
__dead void	tao(int argc, char **argv);
int		play(char *arg);
int		info(char *arg);
int		cddbinfo(char *arg);
int		pstatus(char *arg);
int		play_next(char *arg);
int		play_prev(char *arg);
int		play_same(char *arg);
char		*input(int *);
char		*prompt(void);
void		prtrack(struct cd_toc_entry *e, int lastflag, char *name);
void		lba2msf(unsigned long lba, u_char *m, u_char *s, u_char *f);
unsigned int	msf2lba(u_char m, u_char s, u_char f);
int		play_blocks(int blk, int len);
int		run(int cmd, char *arg);
char		*parse(char *buf, int *cmd);
void		help(void);
void		usage(void);
char		*strstatus(int);
int		cdid(void);
void		addmsf(u_int *, u_int *, u_int *, u_char, u_char, u_char);
int		cmpmsf(u_char, u_char, u_char, u_char, u_char, u_char);
void		toc2msf(u_int, u_char *, u_char *, u_char *);

void
help(void)
{
	struct cmdtab *c;
	char *s, n;
	int i;

	for (c = cmdtab; c->name; ++c) {
		if (!c->args)
			continue;
		printf("\t");
		for (i = c->min, s = c->name; *s; s++, i--) {
			if (i > 0)
				n = toupper((unsigned char)*s);
			else
				n = *s;
			putchar(n);
		}
		if (*c->args)
			printf(" %s", c->args);
		printf("\n");
	}
	printf("\n\tThe word \"play\" is not required for the play commands.\n");
	printf("\tThe plain target address is taken as a synonym for play.\n");
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-sv] [-d host:port] [-f device] [command args ...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch, cmd;
	char *arg;

	cdname = getenv("DISC");
	if (!cdname)
		cdname = getenv("CDROM");

	cddb_host = getenv("CDDB");
	if (!cddb_host)
		cddb_host = "freedb.freedb.org";

	while ((ch = getopt(argc, argv, "svd:f:")) != -1)
		switch (ch) {
		case 's':
			verbose = 0;
			break;
		case 'v':
			verbose++;
			break;
		case 'f':
			cdname = optarg;
			break;
		case 'd':
			cddb_host = optarg;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc > 0 && ! strcasecmp(*argv, "help"))
		usage();

	if (!cdname) {
		cdname = DEFAULT_CD_DRIVE;
		if (verbose > 1)
			fprintf(stderr,
			    "No CD device name specified. Defaulting to %s.\n",
			    cdname);
	}

	if (argc > 0 && !strcasecmp(*argv, "tao")) {
		tao(argc, argv);
		/* NOTREACHED */
	}
	if (argc > 0) {
		char buf[80], *p;
		int len;

		for (p=buf; argc-->0; ++argv) {
			len = snprintf(p, buf + sizeof buf - p,
			   "%s%s", (p > buf) ? " " : "", *argv);

			if (len == -1 || len >= buf + sizeof buf - p)
				errx(1, "argument list too long.");

			p += len;
		}
		arg = parse(buf, &cmd);
		return (run(cmd, arg));
	}

	if (verbose == 1)
		verbose = isatty(0);

	if (verbose) {
		printf("Compact Disc Control utility, version %s\n", VERSION);
		printf("Type `?' for command list\n\n");
	}

	switch_el();

	for (;;) {
		arg = input(&cmd);
		if (run(cmd, arg) < 0) {
			if (verbose)
				warn(NULL);
			close(fd);
			fd = -1;
		}
		fflush(stdout);
	}
}

int
run(int cmd, char *arg)
{
	int l, r, rc;
	static char newcdname[PATH_MAX];

	switch (cmd) {

	case CMD_QUIT:
		switch_el();
		exit(0);

	case CMD_INFO:
		if (!open_cd(cdname, 0))
			return (0);

		return info(arg);

	case CMD_CDDB:
		if (!open_cd(cdname, 0))
			return (0);

		return cddbinfo(arg);

	case CMD_CDID:
		if (!open_cd(cdname, 0))
			return (0);
		return cdid();

	case CMD_STATUS:
		if (!open_cd(cdname, 0))
			return (0);

		return pstatus(arg);

	case CMD_PAUSE:
		if (!open_cd(cdname, 0))
			return (0);

		return ioctl(fd, CDIOCPAUSE);

	case CMD_RESUME:
		if (!open_cd(cdname, 0))
			return (0);

		return ioctl(fd, CDIOCRESUME);

	case CMD_STOP:
		if (!open_cd(cdname, 0))
			return (0);

		rc = ioctl(fd, CDIOCSTOP);

		(void) ioctl(fd, CDIOCALLOW);

		return (rc);

	case CMD_RESET:
		if (!open_cd(cdname, 0))
			return (0);

		rc = ioctl(fd, CDIOCRESET);
		if (rc < 0)
			return rc;
		close(fd);
		fd = -1;
		return (0);

	case CMD_DEBUG:
		if (!open_cd(cdname, 0))
			return (0);

		if (!strcasecmp(arg, "on"))
			return ioctl(fd, CDIOCSETDEBUG);

		if (!strcasecmp(arg, "off"))
			return ioctl(fd, CDIOCCLRDEBUG);

		printf("%s: Invalid command arguments\n", __progname);

		return (0);

	case CMD_DEVICE:
		/* close old device */
		if (fd > -1) {
			(void) ioctl(fd, CDIOCALLOW);
			close(fd);
			fd = -1;
		}

		if (strlen(arg) == 0) {
			printf("%s: Invalid parameter\n", __progname);
			return (0);
		}

		/* open new device */
		if (!open_cd(arg, 0))
			return (0);
		(void) strlcpy(newcdname, arg, sizeof(newcdname));
		cdname = newcdname;
		return (1);

	case CMD_EJECT:
		if (!open_cd(cdname, 0))
			return (0);

		(void) ioctl(fd, CDIOCALLOW);
		rc = ioctl(fd, CDIOCEJECT);
		if (rc < 0)
			return (rc);
#if defined(__OpenBSD__)
		close(fd);
		fd = -1;
#endif
		if (track_names)
			free_names(track_names);
		track_names = NULL;
		return (0);

	case CMD_CLOSE:
#if defined(CDIOCCLOSE)
		if (!open_cd(cdname, 0))
			return (0);

		(void) ioctl(fd, CDIOCALLOW);
		rc = ioctl(fd, CDIOCCLOSE);
		if (rc < 0)
			return (rc);
		close(fd);
		fd = -1;
		return (0);
#else
		printf("%s: Command not yet supported\n", __progname);
		return (0);
#endif

	case CMD_PLAY:
		if (!open_cd(cdname, 0))
			return (0);

		while (isspace((unsigned char)*arg))
			arg++;

		return play(arg);

	case CMD_SET:
		if (!strcasecmp(arg, "msf"))
			msf = 1;
		else if (!strcasecmp(arg, "lba"))
			msf = 0;
		else
			printf("%s: Invalid command arguments\n", __progname);
		return (0);

	case CMD_VOLUME:
		if (!open_cd(cdname, 0))
			return (0);

		if (!strncasecmp(arg, "left", strlen(arg)))
			return ioctl(fd, CDIOCSETLEFT);

		if (!strncasecmp(arg, "right", strlen(arg)))
			return ioctl(fd, CDIOCSETRIGHT);

		if (!strncasecmp(arg, "mono", strlen(arg)))
			return ioctl(fd, CDIOCSETMONO);

		if (!strncasecmp(arg, "stereo", strlen(arg)))
			return ioctl(fd, CDIOCSETSTEREO);

		if (!strncasecmp(arg, "mute", strlen(arg)))
			return ioctl(fd, CDIOCSETMUTE);

		if (2 != sscanf(arg, "%d%d", &l, &r)) {
			printf("%s: Invalid command arguments\n", __progname);
			return (0);
		}

		return setvol(l, r);

	case CMD_NEXT:
		if (!open_cd(cdname, 0))
			return (0);

		return play_next(arg);

	case CMD_PREV:
		if (!open_cd(cdname, 0))
			return (0);

		return play_prev(arg);

	case CMD_REPLAY:
		if (!open_cd(cdname, 0))
			return 0;

		return play_same(arg);
	case CMD_BLANK:
		if (!open_cd(cdname, 1))
			return 0;

		if (get_media_capabilities(mediacap, 1) == -1) {
			warnx("Can't determine media type");
			return (0);
		}
		if (isset(mediacap, MMC_FEATURE_CDRW_WRITE) == 0 &&
		    get_media_type() != MEDIATYPE_CDRW) {
			warnx("The media doesn't support blanking");
			return (0);
		}

		return blank();
	case CMD_CDRIP:
		if (!open_cd(cdname, 0))
			return (0);

		while (isspace((unsigned char)*arg))
			arg++;

		return cdrip(arg);
	case CMD_CDPLAY:
		if (!open_cd(cdname, 0))
			return (0);

		while (isspace((unsigned char)*arg))
			arg++;

		return cdplay(arg);
	default:
	case CMD_HELP:
		help();
		return (0);

	}
}

/*
 * Check if audio file has RIFF WAVE format. If not, we assume it's just PCM.
 */
int
is_wave(int fd)
{
	char buf[WAVHDRLEN];
	int rv;

	rv = 0;
	if (read(fd, buf, sizeof(buf)) == sizeof(buf)) {
		if (memcmp(buf, "RIFF", 4) == 0 &&
		    memcmp(buf + 8, "WAVE", 4) == 0)
			rv = 1;
	}

	return (rv);
}

__dead void
tao(int argc, char **argv)
{
	struct stat sb;
	struct track_info *cur_track;
	struct track_info *tr;
	off_t availblk, needblk = 0;
	u_int blklen;
	u_int ntracks = 0;
	char type;
	int ch, speed;
	const char *errstr;

	if (argc == 1)
		usage();

	SLIST_INIT(&tracks);
	type = 'd';
	speed = DRIVE_SPEED_OPTIMAL;
	blklen = 2048;
	while (argc > 1) {
		tr = malloc(sizeof(struct track_info));
		if (tr == NULL)
			err(1, "tao");

		optreset = 1;
		optind = 1;
		while ((ch = getopt(argc, argv, "ads:")) != -1) {
			switch (ch) {
			case 'a':
				type = 'a';
				blklen = 2352;
				break;
			case 'd':
				type = 'd';
				blklen = 2048;
				break;
			case 's':
				if (strcmp(optarg, "auto") == 0) {
					speed = DRIVE_SPEED_OPTIMAL;
				} else if (strcmp(optarg, "max") == 0) {
					speed = DRIVE_SPEED_MAX;
				} else {
					speed = (int)strtonum(optarg, 1,
					    CD_MAX_SPEED, &errstr);
					if (errstr != NULL) {
						errx(1,
						    "incorrect speed value");
					}
				}
				break;
			default:
				usage();
				/* NOTREACHED */
			}
		}

		if (speed != DRIVE_SPEED_OPTIMAL && speed != DRIVE_SPEED_MAX)
			tr->speed = CD_SPEED_TO_KBPS(speed, blklen);
		else
			tr->speed = speed;

		tr->type = type;
		tr->blklen = blklen;
		argc -= optind;
		argv += optind;
		if (argv[0] == NULL)
			usage();
		tr->file = argv[0];
		tr->fd = open(tr->file, O_RDONLY, 0640);
		if (tr->fd == -1)
			err(1, "cannot open file %s", tr->file);
		if (fstat(tr->fd, &sb) == -1)
			err(1, "cannot stat file %s", tr->file);
		tr->sz = sb.st_size;
		tr->off = 0;
		if (tr->type == 'a') {
			if (is_wave(tr->fd)) {
				tr->sz -= WAVHDRLEN;
				tr->off = WAVHDRLEN;
			}
		}
		if (SLIST_EMPTY(&tracks))
			SLIST_INSERT_HEAD(&tracks, tr, track_list);
		else
			SLIST_INSERT_AFTER(cur_track, tr, track_list);
		cur_track = tr;
	}

	if (!open_cd(cdname, 1))
		exit(1);
	if (get_media_capabilities(mediacap, 1) == -1)
		errx(1, "Can't determine media type");
	if (isset(mediacap, MMC_FEATURE_CD_TAO) == 0)
		errx(1, "The media can't be written in TAO mode");

	get_disc_size(&availblk);
	SLIST_FOREACH(tr, &tracks, track_list) {
		needblk += tr->sz/tr->blklen;
		ntracks++;
	}
	needblk += (ntracks - 1) * 150; /* transition area between tracks */
	if (needblk > availblk)
		errx(1, "Only %llu of the required %llu blocks available",
		    availblk, needblk);
	if (writetao(&tracks) != 0)
		exit(1);
	else
		exit(0);
}

int
play(char *arg)
{
	struct ioc_toc_header h;
	unsigned char tm, ts, tf;
	unsigned int tr1, tr2, m1, m2, s1, s2, f1, f2, i1, i2;
	unsigned int blk, len, n;
	char c;
	int rc;

	rc = ioctl(fd, CDIOREADTOCHEADER, &h);

	if (rc < 0)
		return (rc);

	if (h.starting_track > h.ending_track) {
		printf("TOC starting_track > TOC ending_track\n");
		return (0);
	}

	n = h.ending_track - h.starting_track + 1;
	rc = read_toc_entrys((n + 1) * sizeof (struct cd_toc_entry));

	if (rc < 0)
		return (rc);

	/*
	 * Truncate trailing white space. Then by adding %c to the end of the
	 * sscanf() formats we catch any errant trailing characters.
	 */
	rc = strlen(arg) - 1;
	while (rc >= 0 && isspace((unsigned char)arg[rc])) {
		arg[rc] = '\0';
		rc--;
	}

	if (!arg || ! *arg) {
		/* Play the whole disc */
		return (play_track(h.starting_track, 1, h.ending_track, 1));
	}

	if (strchr(arg, '#')) {
		/* Play block #blk [ len ] */
		if (2 != sscanf(arg, "#%u%u%c", &blk, &len, &c) &&
		    1 != sscanf(arg, "#%u%c", &blk, &c)) {
			printf("%s: Invalid command arguments\n", __progname);
			return (0);
		}

		if (len == 0) {
			if (msf)
				len = msf2lba(toc_buffer[n].addr.msf.minute,
				    toc_buffer[n].addr.msf.second,
				    toc_buffer[n].addr.msf.frame) - blk;
			else
				len = toc_buffer[n].addr.lba - blk;
		}
		return play_blocks(blk, len);
	}

	if (strchr(arg, ':') == NULL) {
		/*
		 * Play track tr1[.i1] [tr2[.i2]]
		 */
		if (4 == sscanf(arg, "%u.%u%u.%u%c", &tr1, &i1, &tr2, &i2, &c))
			goto play_track;

		i2 = 1;
		if (3 == sscanf(arg, "%u.%u%u%c", &tr1, &i1, &tr2, &c))
			goto play_track;

		i1 = 1;
		if (3 == sscanf(arg, "%u%u.%u%c", &tr1, &tr2, &i2, &c))
			goto play_track;

		tr2 = 0;
		i2 = 1;
		if (2 == sscanf(arg, "%u.%u%c", &tr1, &i1, &c))
			goto play_track;

		i1 = i2 = 1;
		if (2 == sscanf(arg, "%u%u%c", &tr1, &tr2, &c))
			goto play_track;

		i1 = i2 = 1;
		tr2 = 0;
		if (1 == sscanf(arg, "%u%c", &tr1, &c))
			goto play_track;

		printf("%s: Invalid command arguments\n", __progname);
		return (0);

play_track:
		if (tr1 > n || tr2 > n) {
			printf("Track number must be between 0 and %u\n", n);
			return (0);
		} else if (tr2 == 0)
			tr2 = h.ending_track;

		if (tr1 > tr2) {
			printf("starting_track > ending_track\n");
			return (0);
		}

		return (play_track(tr1, i1, tr2, i2));
	}

	/*
	 * Play MSF [tr1] m1:s1[.f1] [tr2] [m2:s2[.f2]]
	 *
	 * Start Time		End Time
	 * ----------		--------
	 * tr1 m1:s1.f1		tr2 m2:s2.f2
	 * tr1 m1:s1   		tr2 m2:s2.f2
	 * tr1 m1:s1.f1		tr2 m2:s2
	 * tr1 m1:s1   		tr2 m2:s2
	 *     m1:s1.f1		tr2 m2:s2.f2
	 *     m1:s1   		tr2 m2:s2.f2
	 *     m1:s1.f1		tr2 m2:s2
	 *     m1:s1   		tr2 m2:s2
	 * tr1 m1:s1.f1		    m2:s2.f2
	 * tr1 m1:s1   		    m2:s2.f2
	 * tr1 m1:s1.f1		    m2:s2
	 * tr1 m1:s1  		    m2:s2
	 *     m1:s1.f1		    m2:s2.f2
	 *     m1:s1       	    m2:s2.f2
	 *     m1:s1.f1  	    m2:s2
	 *     m1:s1     	    m2:s2
	 * tr1 m1:s1.f1		tr2
	 * tr1 m1:s1    	tr2
	 *     m1:s1.f1  	tr2
	 *     m1:s1      	tr2
	 * tr1 m1:s1.f1		<end of disc>
	 * tr1 m1:s1    	<end of disc>
	 *     m1:s1.f1  	<end of disc>
	 *     m1:s1      	<end of disc>
	 */

	/* tr1 m1:s1.f1		tr2 m2:s2.f2 */
	if (8 == sscanf(arg, "%u%u:%u.%u%u%u:%u.%u%c",
	    &tr1, &m1, &s1, &f1, &tr2, &m2, &s2, &f2, &c))
		goto play_msf;

	/* tr1 m1:s1   		tr2 m2:s2.f2 */
	f1 = 0;
	if (7 == sscanf(arg, "%u%u:%u%u%u:%u.%u%c",
	    &tr1, &m1, &s1, &tr2, &m2, &s2, &f2, &c))
		goto play_msf;

	/* tr1 m1:s1.f1		tr2 m2:s2 */
	f2 =0;
	if (7 == sscanf(arg, "%u%u:%u.%u%u%u:%u%c",
	    &tr1, &m1, &s1, &f1, &tr2, &m2, &s2, &c))
		goto play_msf;

	/*     m1:s1.f1		tr2 m2:s2.f2 */
	tr1 = 0;
	if (7 == sscanf(arg, "%u:%u.%u%u%u:%u.%u%c",
	    &m1, &s1, &f1, &tr2, &m2, &s2, &f2, &c))
		goto play_msf;

	/* tr1 m1:s1.f1		    m2:s2.f2 */
	tr2 = 0;
	if (7 == sscanf(arg, "%u%u:%u.%u%u:%u.%u%c",
	    &tr1, &m1, &s1, &f1, &m2, &s2, &f2, &c))
		goto play_msf;

	/*     m1:s1   		tr2 m2:s2.f2 */
	tr1 = f1 = 0;
	if (6 == sscanf(arg, "%u:%u%u%u:%u.%u%c",
	    &m1, &s1, &tr2, &m2, &s2, &f2, &c))
		goto play_msf;

	/*     m1:s1.f1		tr2 m2:s2 */
	tr1 = f2 = 0;
	if (6 == sscanf(arg, "%u:%u.%u%u%u:%u%c",
	    &m1, &s1, &f1, &tr2, &m2, &s2, &c))
		goto play_msf;

	/*     m1:s1.f1		    m2:s2.f2 */
	tr1 = tr2 = 0;
	if (6 == sscanf(arg, "%u:%u.%u%u:%u.%u%c",
	    &m1, &s1, &f1, &m2, &s2, &f2, &c))
		goto play_msf;

	/* tr1 m1:s1.f1		    m2:s2 */
	tr2 = f2 = 0;
	if (6 == sscanf(arg, "%u%u:%u.%u%u:%u%c",
	    &tr1, &m1, &s1, &f1, &m2, &s2, &c))
		goto play_msf;

	/* tr1 m1:s1   		    m2:s2.f2 */
	tr2 = f1 = 0;
	if (6 == sscanf(arg, "%u%u:%u%u:%u.%u%c",
	    &tr1, &m1, &s1, &m2, &s2, &f2, &c))
		goto play_msf;

	/* tr1 m1:s1   		tr2 m2:s2 */
	f1 = f2 = 0;
	if (6 == sscanf(arg, "%u%u:%u%u%u:%u%c",
	    &tr1, &m1, &s1, &tr2, &m2, &s2, &c))
		goto play_msf;

	/*     m1:s1   		tr2 m2:s2 */
	tr1 = f1 = f2 = 0;
	if (5 == sscanf(arg, "%u:%u%u%u:%u%c", &m1, &s1, &tr2, &m2, &s2, &c))
		goto play_msf;

	/* tr1 m1:s1  		    m2:s2 */
	f1 = tr2 = f2 = 0;
	if (5 == sscanf(arg, "%u%u:%u%u:%u%c", &tr1, &m1, &s1, &m2, &s2, &c))
		goto play_msf;

	/*     m1:s1       	    m2:s2.f2 */
	tr1 = f1 = tr2 = 0;
	if (5 == sscanf(arg, "%u:%u%u:%u.%u%c", &m1, &s1, &m2, &s2, &f2, &c))
		goto play_msf;

	/*     m1:s1.f1  	    m2:s2 */
	tr1 = tr2 = f2 = 0;
	if (5 == sscanf(arg, "%u:%u.%u%u:%u%c", &m1, &s1, &f1, &m2, &s2, &c))
		goto play_msf;

	/* tr1 m1:s1.f1		tr2 */
	m2 = s2 = f2 = 0;
	if (5 == sscanf(arg, "%u%u:%u.%u%u%c", &tr1, &m1, &s1, &f1, &tr2, &c))
		goto play_msf;

	/*     m1:s1     	    m2:s2 */
	tr1 = f1 = tr2 = f2 = 0;
	if (4 == sscanf(arg, "%u:%u%u:%u%c", &m1, &s1, &m2, &s2, &c))
		goto play_msf;

	/* tr1 m1:s1.f1		<end of disc> */
	tr2 = m2 = s2 = f2 = 0;
	if (4 == sscanf(arg, "%u%u:%u.%u%c", &tr1, &m1, &s1, &f1, &c))
		goto play_msf;

	/* tr1 m1:s1    	tr2 */
	f1 = m2 = s2 = f2 = 0;
	if (4 == sscanf(arg, "%u%u:%u%u%c", &tr1, &m1, &s1, &tr2, &c))
		goto play_msf;

	/*     m1:s1.f1  	tr2 */
	tr1 = m2 = s2 = f2 = 0;
	if (4 == sscanf(arg, "%u%u:%u%u%c", &m1, &s1, &f1, &tr2, &c))
		goto play_msf;

	/*     m1:s1.f1  	<end of disc> */
	tr1 = tr2 = m2 = s2 = f2 = 0;
	if (3 == sscanf(arg, "%u:%u.%u%c", &m1, &s1, &f1, &c))
		goto play_msf;

	/* tr1 m1:s1    	<end of disc> */
	f1 = tr2 = m2 = s2 = f2 = 0;
	if (3 == sscanf(arg, "%u%u:%u%c", &tr1, &m1, &s1, &c))
		goto play_msf;

	/*     m1:s1      	tr2 */
	tr1 = f1 = m2 = s2 = f2 = 0;
	if (3 == sscanf(arg, "%u:%u%u%c", &m1, &s1, &tr2, &c))
		goto play_msf;

	/*     m1:s1      	<end of disc> */
	tr1 = f1 = tr2 = m2 = s2 = f2 = 0;
	if (2 == sscanf(arg, "%u:%u%c", &m1, &s1, &c))
		goto play_msf;

	printf("%s: Invalid command arguments\n", __progname);
	return (0);

play_msf:
	if (tr1 > n || tr2 > n) {
		printf("Track number must be between 0 and %u\n", n);
		return (0);
	} else if (m1 > 99 || m2 > 99) {
		printf("Minutes must be between 0 and 99\n");
		return (0);
	} else if (s1 > 59 || s2 > 59) {
		printf("Seconds must be between 0 and 59\n");
		return (0);
	} if (f1 > 74 || f2 > 74) {
		printf("Frames number must be between 0 and 74\n");
		return (0);
	}

	if (tr1 > 0) {
		/*
		 * Start time is relative to tr1, Add start time of tr1
		 * to (m1,s1,f1) to yield absolute start time.
		 */
		toc2msf(tr1, &tm, &ts, &tf);
		addmsf(&m1, &s1, &f1, tm, ts, tf);

		/* Compare (m1,s1,f1) to start time of next track. */
		toc2msf(tr1+1, &tm, &ts, &tf);
		if (cmpmsf(m1, s1, f1, tm, ts, tf) == 1) {
			printf("Track %u is not that long.\n", tr1);
			return (0);
		}
	}

	toc2msf(n+1, &tm, &ts, &tf);
	if (cmpmsf(m1, s1, f1, tm, ts, tf) == 1) {
		printf("Start time is after end of disc.\n");
		return (0);
	}

	if (tr2 > 0) {
		/*
		 * End time is relative to tr2, Add start time of tr2
		 * to (m2,s2,f2) to yield absolute end time.
		 */
		toc2msf(tr2, &tm, &ts, &tf);
		addmsf(&m2, &s2, &f2, tm, ts, tf);

		/* Compare (m2,s2,f2) to start time of next track. */
		toc2msf(tr2+1, &tm, &ts, &tf);
		if (cmpmsf(m2, s2, f2, tm, ts, tf) == 1) {
			printf("Track %u is not that long.\n", tr2);
			return (0);
		}
	}

	toc2msf(n+1, &tm, &ts, &tf);

	if (!(tr2 || m2 || s2 || f2)) {
		/* Play to end of disc. */
		m2 = tm;
		s2 = ts;
		f2 = tf;
	} else if (cmpmsf(m2, s2, f2, tm, ts, tf) == 1) {
		printf("End time is after end of disc.\n");
		return (0);
	}

	if (cmpmsf(m1, s1, f1, m2, s2, f2) == 1) {
		printf("Start time is after end time.\n");
		return (0);
	}

	return play_msf(m1, s1, f1, m2, s2, f2);
}

/* ARGSUSED */
int
play_prev(char *arg)
{
	int trk, min, sec, frm, rc;
	struct ioc_toc_header h;

	if (status(&trk, &min, &sec, &frm) >= 0) {
		trk--;

		rc = ioctl(fd, CDIOREADTOCHEADER, &h);
		if (rc < 0) {
			warn("getting toc header");
			return (rc);
		}

		if (trk < h.starting_track)
			return play_track(h.starting_track, 1,
			    h.ending_track + 1, 1);
		return play_track(trk, 1, h.ending_track, 1);
	}

	return (0);
}

/* ARGSUSED */
int
play_same(char *arg)
{
	int trk, min, sec, frm, rc;
	struct ioc_toc_header h;

	if (status (&trk, &min, &sec, &frm) >= 0) {
		rc = ioctl(fd, CDIOREADTOCHEADER, &h);
		if (rc < 0) {
			warn("getting toc header");
			return (rc);
		}

		return play_track(trk, 1, h.ending_track, 1);
	}

	return (0);
}

/* ARGSUSED */
int
play_next(char *arg)
{
	int trk, min, sec, frm, rc;
	struct ioc_toc_header h;

	if (status(&trk, &min, &sec, &frm) >= 0) {
		trk++;
		rc = ioctl(fd, CDIOREADTOCHEADER, &h);
		if (rc < 0) {
			warn("getting toc header");
			return (rc);
		}

		if (trk > h.ending_track) {
			printf("%s: end of CD\n", __progname);

			rc = ioctl(fd, CDIOCSTOP);

			(void) ioctl(fd, CDIOCALLOW);

			return (rc);
		}

		return play_track(trk, 1, h.ending_track, 1);
	}

	return (0);
}

char *
strstatus(int sts)
{
	switch (sts) {
	case ASTS_INVALID:
		return ("invalid");
	case ASTS_PLAYING:
		return ("playing");
	case ASTS_PAUSED:
		return ("paused");
	case ASTS_COMPLETED:
		return ("completed");
	case ASTS_ERROR:
		return ("error");
	case ASTS_VOID:
		return ("void");
	default:
		return ("??");
	}
}

/* ARGSUSED */
int
pstatus(char *arg)
{
	struct ioc_vol v;
	struct ioc_read_subchannel ss;
	struct cd_sub_channel_info data;
	int rc, trk, m, s, f;
	char vis_catalog[1 + 4 * 15];

	rc = status(&trk, &m, &s, &f);
	if (rc >= 0) {
		if (verbose) {
			if (track_names)
				printf("Audio status = %d<%s>, "
				    "current track = %d (%s)\n"
				    "\tcurrent position = %d:%02d.%02d\n",
				    rc, strstatus(rc), trk,
				    trk ? track_names[trk-1] : "", m, s, f);
			else
				printf("Audio status = %d<%s>, "
				    "current track = %d, "
				    "current position = %d:%02d.%02d\n",
				    rc, strstatus(rc), trk, m, s, f);
		} else
			printf("%d %d %d:%02d.%02d\n", rc, trk, m, s, f);
	} else
		printf("No current status info available\n");

	bzero(&ss, sizeof (ss));
	ss.data = &data;
	ss.data_len = sizeof (data);
	ss.address_format = msf ? CD_MSF_FORMAT : CD_LBA_FORMAT;
	ss.data_format = CD_MEDIA_CATALOG;
	rc = ioctl(fd, CDIOCREADSUBCHANNEL, (char *) &ss);
	if (rc >= 0) {
		printf("Media catalog is %sactive",
		ss.data->what.media_catalog.mc_valid ? "": "in");
		if (ss.data->what.media_catalog.mc_valid &&
		    ss.data->what.media_catalog.mc_number[0]) {
			strvisx(vis_catalog,
			    (char *)ss.data->what.media_catalog.mc_number,
			    15, VIS_SAFE);
			printf(", number \"%.15s\"", vis_catalog);
		}
		putchar('\n');
	} else
		printf("No media catalog info available\n");

	rc = ioctl(fd, CDIOCGETVOL, &v);
	if (rc >= 0) {
		if (verbose)
			printf("Left volume = %d, right volume = %d\n",
			    v.vol[0], v.vol[1]);
		else
			printf("%d %d\n", v.vol[0], v.vol[1]);
	} else
		printf("No volume level info available\n");
	return(0);
}

int
cdid(void)
{
	unsigned long id;
	struct ioc_toc_header h;
	int rc, n;

	rc = ioctl(fd, CDIOREADTOCHEADER, &h);
	if (rc == -1) {
		warn("getting toc header");
		return (rc);
	}

	n = h.ending_track - h.starting_track + 1;
	rc = read_toc_entrys((n + 1) * sizeof (struct cd_toc_entry));
	if (rc < 0)
		return (rc);

	id = cddb_discid(n, toc_buffer);
	if (id) {
		if (verbose)
			printf("CDID=");
		printf("%08lx\n", id);
	}
	return id ? 0 : 1;
}

/* ARGSUSED */
int
info(char *arg)
{
	struct ioc_toc_header h;
	int rc, i, n;

	if (get_media_capabilities(mediacap, 1) == -1)
		errx(1, "Can't determine media type");

	rc = ioctl(fd, CDIOREADTOCHEADER, &h);
	if (rc >= 0) {
		if (verbose)
			printf("Starting track = %d, ending track = %d, TOC size = %d bytes\n",
			    h.starting_track, h.ending_track, h.len);
		else
			printf("%d %d %d\n", h.starting_track,
			    h.ending_track, h.len);
	} else {
		warn("getting toc header");
		return (rc);
	}

	n = h.ending_track - h.starting_track + 1;
	rc = read_toc_entrys((n + 1) * sizeof (struct cd_toc_entry));
	if (rc < 0)
		return (rc);

	if (verbose) {
		printf("track     start  duration   block  length   type\n");
		printf("-------------------------------------------------\n");
	}

	for (i = 0; i < n; i++) {
		printf("%5d  ", toc_buffer[i].track);
		prtrack(toc_buffer + i, 0, NULL);
	}
	printf("%5d  ", toc_buffer[n].track);
	prtrack(toc_buffer + n, 1, NULL);
	return (0);
}

int
cddbinfo(char *arg)
{
	struct ioc_toc_header h;
	int rc, i, n;

	rc = ioctl(fd, CDIOREADTOCHEADER, &h);
	if (rc == -1) {
		warn("getting toc header");
		return (rc);
	}

	n = h.ending_track - h.starting_track + 1;
	rc = read_toc_entrys((n + 1) * sizeof (struct cd_toc_entry));
	if (rc < 0)
		return (rc);

	if (track_names)
		free_names(track_names);
	track_names = NULL;

	track_names = cddb(cddb_host, n, toc_buffer, arg);
	if (!track_names)
		return(0);

	printf("-------------------------------------------------\n");

	for (i = 0; i < n; i++) {
		printf("%5d  ", toc_buffer[i].track);
		prtrack(toc_buffer + i, 0, track_names[i]);
	}
	printf("%5d  ", toc_buffer[n].track);
	prtrack(toc_buffer + n, 1, "");
	return (0);
}

void
lba2msf(unsigned long lba, u_char *m, u_char *s, u_char *f)
{
	lba += 150;		/* block start offset */
	lba &= 0xffffff;	/* negative lbas use only 24 bits */
	*m = lba / (60 * 75);
	lba %= (60 * 75);
	*s = lba / 75;
	*f = lba % 75;
}

unsigned int
msf2lba(u_char m, u_char s, u_char f)
{
	return (((m * 60) + s) * 75 + f) - 150;
}

unsigned long
entry2time(struct cd_toc_entry *e)
{
	int block;
	u_char m, s, f;

	if (msf) {
		return (e->addr.msf.minute * 60 + e->addr.msf.second);
	} else {
		block = e->addr.lba;
		lba2msf(block, &m, &s, &f);
		return (m*60+s);
	}
}

unsigned long
entry2frames(struct cd_toc_entry *e)
{
	int block;
	unsigned char m, s, f;

	if (msf) {
		return e->addr.msf.frame + e->addr.msf.second * 75 +
		    e->addr.msf.minute * 60 * 75;
	} else {
		block = e->addr.lba;
		lba2msf(block, &m, &s, &f);
		return f + s * 75 + m * 60 * 75;
	}
}

void
prtrack(struct cd_toc_entry *e, int lastflag, char *name)
{
	int block, next, len;
	u_char m, s, f;

	if (msf) {
		if (!name || lastflag)
			/* Print track start */
			printf("%2d:%02d.%02d  ", e->addr.msf.minute,
			    e->addr.msf.second, e->addr.msf.frame);

		block = msf2lba(e->addr.msf.minute, e->addr.msf.second,
			e->addr.msf.frame);
	} else {
		block = e->addr.lba;
		if (!name || lastflag) {
			lba2msf(block, &m, &s, &f);
			/* Print track start */
			printf("%2d:%02d.%02d  ", m, s, f);
		}
	}
	if (lastflag) {
		if (!name)
			/* Last track -- print block */
			printf("       -  %6d       -      -\n", block);
		else
			printf("\n");
		return;
	}

	if (msf)
		next = msf2lba(e[1].addr.msf.minute, e[1].addr.msf.second,
			e[1].addr.msf.frame);
	else
		next = e[1].addr.lba;
	len = next - block;
	lba2msf(len - 150, &m, &s, &f);

	if (name)
		printf("%2d:%02d.%02d  %s\n", m, s, f, name);
	/* Print duration, block, length, type */
	else
		printf("%2d:%02d.%02d  %6d  %6d  %5s\n", m, s, f, block, len,
		    (e->control & 4) ? "data" : "audio");
}

int
play_track(int tstart, int istart, int tend, int iend)
{
	struct ioc_play_track t;

	t.start_track = tstart;
	t.start_index = istart;
	t.end_track = tend;
	t.end_index = iend;

	return ioctl(fd, CDIOCPLAYTRACKS, &t);
}

int
play_blocks(int blk, int len)
{
	struct ioc_play_blocks  t;

	t.blk = blk;
	t.len = len;

	return ioctl(fd, CDIOCPLAYBLOCKS, &t);
}

int
setvol(int left, int right)
{
	struct ioc_vol  v;

	v.vol[0] = left;
	v.vol[1] = right;
	v.vol[2] = 0;
	v.vol[3] = 0;

	return ioctl(fd, CDIOCSETVOL, &v);
}

int
read_toc_entrys(int len)
{
	struct ioc_read_toc_entry t;

	if (toc_buffer) {
		free(toc_buffer);
		toc_buffer = 0;
	}

	toc_buffer = malloc(len);

	if (!toc_buffer) {
		errno = ENOMEM;
		return (-1);
	}

	t.address_format = msf ? CD_MSF_FORMAT : CD_LBA_FORMAT;
	t.starting_track = 0;
	t.data_len = len;
	t.data = toc_buffer;

	return (ioctl(fd, CDIOREADTOCENTRYS, (char *) &t));
}

int
play_msf(int start_m, int start_s, int start_f, int end_m, int end_s, int end_f)
{
	struct ioc_play_msf a;

	a.start_m = start_m;
	a.start_s = start_s;
	a.start_f = start_f;
	a.end_m = end_m;
	a.end_s = end_s;
	a.end_f = end_f;

	return ioctl(fd, CDIOCPLAYMSF, (char *) &a);
}

int
status(int *trk, int *min, int *sec, int *frame)
{
	struct ioc_read_subchannel s;
	struct cd_sub_channel_info data;
	u_char mm, ss, ff;

	bzero(&s, sizeof (s));
	s.data = &data;
	s.data_len = sizeof (data);
	s.address_format = msf ? CD_MSF_FORMAT : CD_LBA_FORMAT;
	s.data_format = CD_CURRENT_POSITION;

	if (ioctl(fd, CDIOCREADSUBCHANNEL, (char *) &s) < 0)
		return -1;

	*trk = s.data->what.position.track_number;
	if (msf) {
		*min = s.data->what.position.reladdr.msf.minute;
		*sec = s.data->what.position.reladdr.msf.second;
		*frame = s.data->what.position.reladdr.msf.frame;
	} else {
		/*
		 * NOTE: CDIOCREADSUBCHANNEL does not put the lba info into
		 * host order like CDIOREADTOCENTRYS does.
		 */
		lba2msf(betoh32(s.data->what.position.reladdr.lba), &mm, &ss,
		    &ff);
		*min = mm;
		*sec = ss;
		*frame = ff;
	}

	return s.data->header.audio_status;
}

char *
input(int *cmd)
{
	char *buf;
	int siz = 0;
	char *p;
	HistEvent hev;

	do {
		if ((buf = (char *) el_gets(el, &siz)) == NULL || !siz) {
			*cmd = CMD_QUIT;
			fprintf(stderr, "\r\n");
			return (0);
		}
		if (strlen(buf) > 1)
			history(hist, &hev, H_ENTER, buf);
		p = parse(buf, cmd);
	} while (!p);
	return (p);
}

char *
parse(char *buf, int *cmd)
{
	struct cmdtab *c;
	char *p;
	size_t len;

	for (p=buf; isspace((unsigned char)*p); p++)
		continue;

	if (isdigit((unsigned char)*p) ||
	    (p[0] == '#' && isdigit((unsigned char)p[1]))) {
		*cmd = CMD_PLAY;
		return (p);
	}

	for (buf = p; *p && ! isspace((unsigned char)*p); p++)
		continue;

	len = p - buf;
	if (!len)
		return (0);

	if (*p) {		/* It must be a spacing character! */
		char *q;

		*p++ = 0;
		for (q=p; *q && *q != '\n' && *q != '\r'; q++)
			continue;
		*q = 0;
	}

	*cmd = -1;
	for (c=cmdtab; c->name; ++c) {
		/* Is it an exact match? */
		if (!strcasecmp(buf, c->name)) {
			*cmd = c->command;
			break;
		}

		/* Try short hand forms then... */
		if (len >= c->min && ! strncasecmp(buf, c->name, len)) {
			if (*cmd != -1 && *cmd != c->command) {
				fprintf(stderr, "Ambiguous command\n");
				return (0);
			}
			*cmd = c->command;
		}
	}

	if (*cmd == -1) {
		fprintf(stderr, "%s: Invalid command, enter ``help'' for commands.\n",
		    __progname);
		return (0);
	}

	while (isspace((unsigned char)*p))
		p++;
	return p;
}

int
open_cd(char *dev, int needwrite)
{
	char *realdev;
	int tries;

	if (fd > -1) {
		if (needwrite && !writeperm) {
			close(fd);
			fd = -1;
		} else
			return (1);
	}

	for (tries = 0; fd < 0 && tries < 10; tries++) {
		if (needwrite)
			fd = opendev(dev, O_RDWR, OPENDEV_PART, &realdev);
		else
			fd = opendev(dev, O_RDONLY, OPENDEV_PART, &realdev);
		if (fd < 0) {
			if (errno == ENXIO) {
				/*  ENXIO has an overloaded meaning here.
				 *  The original "Device not configured" should
				 *  be interpreted as "No disc in drive %s". */
				warnx("No disc in drive %s.", realdev);
				return (0);
			} else if (errno != EIO) {
				/*  EIO may simply mean the device is not ready
				 *  yet which is common with CD changers. */
				warn("Can't open %s", realdev);
				return (0);
			}
		}
		sleep(1);
	}
	if (fd < 0) {
		warn("Can't open %s", realdev);
		return (0);
	}
	writeperm = needwrite;
	return (1);
}

char *
prompt(void)
{
	return (verbose ? "cdio> " : "");
}

void
switch_el(void)
{
	HistEvent hev;

	if (el == NULL && hist == NULL) {
		el = el_init(__progname, stdin, stdout, stderr);
		hist = history_init();
		history(hist, &hev, H_SETSIZE, 100);
		el_set(el, EL_HIST, history, hist);
		el_set(el, EL_EDITOR, "emacs");
		el_set(el, EL_PROMPT, prompt);
		el_set(el, EL_SIGNAL, 1);
		el_source(el, NULL);

	} else {
		if (hist != NULL) {
			history_end(hist);
			hist = NULL;
		}
		if (el != NULL) {
			el_end(el);
			el = NULL;
		}
	}
}

void
addmsf(u_int *m, u_int *s, u_int *f, u_char m_inc, u_char s_inc, u_char f_inc)
{
	*f += f_inc;
	if (*f > 75) {
		*s += *f / 75;
		*f %= 75;
	}

	*s += s_inc;
	if (*s > 60) {
		*m += *s / 60;
		*s %= 60;
	}

	*m += m_inc;
}

int
cmpmsf(u_char m1, u_char s1, u_char f1, u_char m2, u_char s2, u_char f2)
{
	if (m1 > m2)
		return (1);
	else if (m1 < m2)
		return (-1);

	if (s1 > s2)
		return (1);
	else if (s1 < s2)
		return (-1);

	if  (f1 > f2)
		return (1);
	else if (f1 < f2)
		return (-1);

	return (0);
}

void
toc2msf(u_int track, u_char *m, u_char *s, u_char *f)
{
	struct cd_toc_entry *ctep;

	ctep = &toc_buffer[track - 1];

	if (msf) {
		*m = ctep->addr.msf.minute;
		*s = ctep->addr.msf.second;
		*f = ctep->addr.msf.frame;
	} else
		lba2msf(ctep->addr.lba, m, s, f);
}
