/*	$OpenBSD: rip.c,v 1.12 2009/04/10 18:19:41 ratchov Exp $	*/

/*
 * Copyright (c) 2007 Alexey Vatchenko <av@bsdua.org>
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
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/device.h>

#include <sys/cdio.h>
#include <sys/ioctl.h>
#include <sys/scsiio.h>
#include <sys/stat.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>
#include <scsi/cd.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sndio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int fd;
extern int msf;
extern struct cd_toc_entry *toc_buffer;

extern u_int	msf2lba(u_char m, u_char s, u_char f);
extern int	read_toc_entrys(int size);

/*
 * Arguments parser
 */
TAILQ_HEAD(track_pair_head, track_pair);

static int	_parse_val(char *start, char *nxt, int *val);
static int	_parse_pair(char *start, char *nxt, int *val1, int *val2);
static int	_add_pair(struct track_pair_head *head, int val1, int val2,
		    int issorted);

struct track_pair {
	u_char start;
	u_char end;
	TAILQ_ENTRY(track_pair) list;
};

void	parse_tracks_init(struct track_pair_head *head);
void	parse_tracks_final(struct track_pair_head *head);
int	parse_tracks(struct track_pair_head *head, u_char first, u_char last,
	    const char *arg, int issorted);
int	parse_tracks_add(struct track_pair_head *head, u_char first,
	    u_char last, int issorted);

/*
 * Tracks ripping
 */
/* Header of the canonical WAVE file */
static u_char wavehdr[44] = {
	'R', 'I', 'F', 'F', 0x0, 0x0, 0x0, 0x0, 'W', 'A', 'V', 'E',
	'f', 'm', 't', ' ', 0x10, 0x0, 0x0, 0x0, 0x1, 0x0, 0x2, 0x0,
	0x44, 0xac, 0x0, 0x0, 0x10, 0xb1, 0x2, 0x0, 0x4, 0x0, 0x10, 0x0,
	'd', 'a', 't', 'a', 0x0, 0x0, 0x0, 0x0
};

static int	write_sector(int, u_char *, u_int32_t);

int		read_data_sector(u_int32_t, u_char *, u_int32_t);

struct track_info {
	int fd;		/* descriptor of output file */
	struct sio_hdl *hdl; /* sndio handle */
	struct sio_par par; /* sndio parameters */
	u_int track;	/* track number */
	char name[12];	/* output file name, i.e. trackXX.wav/trackXX.dat */
	u_char isaudio;	/* true if audio track, otherwise it's data track */
	u_int32_t start_lba;	/* starting address of this track */
	u_int32_t end_lba;	/* starting address of the next track */
};

int	read_track(struct track_info *);

int	rip_next_track(struct track_info *);
int	play_next_track(struct track_info *);

static int	rip_tracks_loop(struct track_pair *tp, u_int n_tracks,
		    int (*next_track)(struct track_info *));

int	rip_tracks(char *arg, int (*next_track)(struct track_info *),
	    int issorted);

/* Next-Track function exit codes */
#define NXTRACK_OK		0
#define NXTRACK_FAIL		1
#define NXTRACK_SKIP		2

static int
_parse_val(char *start, char *nxt, int *val)
{
	char *p;
	int i, base, n;

	n = nxt - start;

	if (n > 3 || n < 1)
		return (-1);
	for (p = start; p < nxt; p++) {
		if (!isdigit(*p))
			return (-1);
	}

	*val = 0;
	base = 1;
	for (i = 0; i < n; i++) {
		*val += base * (start[n - i - 1] - '0');
		base *= 10;
	}
	return (0);
}

static int
_parse_pair(char *start, char *nxt, int *val1, int *val2)
{
	char *p, *delim;
	int error;

	delim = NULL;
	p = start;
	while (p < nxt) {
		if (*p == '-')
			delim = p;
		p++;
	}

	if (delim != NULL) {
		error = 0;
		if (delim - start < 1)
			*val1 = -1;
		else
			error = _parse_val(start, delim, val1);

		if (error == 0) {
			if ((nxt - delim - 1) < 1)
				*val2 = -1;
			else
				error = _parse_val(delim + 1, nxt, val2);
		}
	} else {
		error = _parse_val(start, nxt, val1);
		*val2 = *val1;
	}

	if (error == 0) {
		if (*val1 > 99 || *val2 > 99)
			error = -1;
	}

	return (error);
}

static int
_add_pair(struct track_pair_head *head, int val1, int val2, int issorted)
{
	u_char v1, v2, v3;
	struct track_pair *tp, *entry;
	int fix;

	v1 = (u_char)val1;
	v2 = (u_char)val2;

	if (issorted) {
		/* 1. Fix order */
		if (v1 > v2) {
			v3 = v1;
			v1 = v2;
			v2 = v3;
		}

		/* 2. Find closest range and fix it */
		fix = 0;
		TAILQ_FOREACH(entry, head, list) {
			if (v1 + 1 == entry->start || v1 == entry->start)
				fix = 1;
			else if (v1 > entry->start && v1 <= entry->end + 1)
				fix = 1;
			else if (v2 + 1 == entry->start || v2 == entry->start)
				fix = 1;
			else if (v2 > entry->start && v2 <= entry->end + 1)
				fix = 1;
			if (fix)
				break;
		}

		if (fix) {
			if (v1 < entry->start)
				entry->start = v1;
			if (v2 > entry->end)
				entry->end = v2;

			return (0);
		}
	}

	tp = (struct track_pair *)malloc(sizeof(*tp));
	if (tp == NULL)
		return (-1);

	tp->start = v1;
	tp->end = v2;
	TAILQ_INSERT_TAIL(head, tp, list);

	return (0);
}

void
parse_tracks_init(struct track_pair_head *head)
{

	memset(head, 0, sizeof(*head));
	TAILQ_INIT(head);
}

void
parse_tracks_final(struct track_pair_head *head)
{
	struct track_pair *tp;

	while ((tp = TAILQ_FIRST(head)) != TAILQ_END(head)) {
		TAILQ_REMOVE(head, tp, list);
		free(tp);
	}
}

int
parse_tracks(struct track_pair_head *head, u_char first, u_char last,
    const char *arg, int issorted)
{
	char *p, *nxt;
	int error, val1, val2;

	p = (char *)arg;
	for (;;) {
		/* Skip trailing spaces */
		while (*p != '\0' && isspace(*p))
			++p;
		if (*p == '\0')
			break;

		/* Search for the next space symbol */
		nxt = p;
		while (*nxt != '\0' && !isspace(*nxt))
			++nxt;
		/* ``nxt'' can't be equal to ``p'' here */
		error = _parse_pair(p, nxt, &val1, &val2);
		if (error != 0)
			break;	/* parse error */

		if (val1 == -1)
			val1 = first;
		if (val2 == -1)
			val2 = last;

		error = _add_pair(head, val1, val2, issorted);
		if (error != 0)
			break;	/* allocation error */

		p = nxt;
	}

	return (0);
}

int
parse_tracks_add(struct track_pair_head *head, u_char first, u_char last,
    int issorted)
{

	return _add_pair(head, first, last, issorted);
}

static int
write_sector(int fd, u_char *sec, u_int32_t secsize)
{
	ssize_t res;

	while (secsize > 0) {
		res = write(fd, sec, secsize);
		if (res < 0)
			return (-1);

		sec += res;
		secsize -= res;
	}

	return (0);
}

/*
 * ERRORS
 *	The function can return
 *	[EBUSY]		Device is busy.
 *	[ETIMEDOUT]	Operation timeout.
 *	[EIO]		Any other errors.
 *	[EAGAIN]	The operation must be made again. XXX - not implemented
 */
int
read_data_sector(u_int32_t lba, u_char *sec, u_int32_t secsize)
{
	scsireq_t scr;
	u_char *cmd;
	int error;

	memset(&scr, 0, sizeof(scr));

	cmd = (u_char *)scr.cmd;
	cmd[0] = 0xbe;			/* READ CD */
	_lto4b(lba, cmd + 2);		/* Starting Logical Block Address */
	_lto3b(1, cmd + 6);		/* Transfer Length in Blocks */
	cmd[9] = 0x10;			/* User Data field */

	scr.flags = SCCMD_ESCAPE | SCCMD_READ;
	scr.databuf = sec;
	scr.datalen = secsize;
	scr.cmdlen = 12;
	scr.timeout = 120000;
	scr.senselen = SENSEBUFLEN;

	/* XXX - what's wrong with DVD? */

	error = ioctl(fd, SCIOCCOMMAND, &scr);
	if (error == -1)
		return (EIO);
	else if (scr.retsts == SCCMD_BUSY)
		return (EBUSY);
	else if (scr.retsts == SCCMD_TIMEOUT)
		return (ETIMEDOUT);
	else if (scr.retsts != SCCMD_OK)
		return (EIO);

	return (0);
}

int
read_track(struct track_info *ti)
{
	struct timeval tv, otv, atv;
	u_int32_t i, blksize, n_sec;
	u_char *sec;
	int error;

	n_sec = ti->end_lba - ti->start_lba;
	blksize = (ti->isaudio) ? 2352 : 2048;
	sec = (u_char *)malloc(blksize);
	if (sec == NULL)
		return (-1);

	timerclear(&otv);
	atv.tv_sec = 1;
	atv.tv_usec = 0;

	for (i = 0; i < n_sec; ) {
		gettimeofday(&tv, NULL);
		if (timercmp(&tv, &otv, >)) {
			fprintf(stderr, "\rtrack %u '%c' %08u/%08u %3u%%",
			    ti->track,
			    (ti->isaudio) ? 'a' : 'd', i, n_sec,
			    100 * i / n_sec);
			timeradd(&tv, &atv, &otv);
		}

		error = read_data_sector(i + ti->start_lba, sec, blksize);
		if (error == 0) {
			if (ti->fd >= 0 &&
			    (write_sector(ti->fd, sec, blksize) != 0)) {
				free(sec);
				warnx("\nerror while writing to the %s file",
				    ti->name);
				return (-1);
			}
			if (ti->hdl != NULL &&
			    (sio_write(ti->hdl, sec, blksize) == 0)) {
				sio_close(ti->hdl);
				ti->hdl = NULL;
				warnx("\nerror while writing to audio output");
				return (-1);
			}

			i++;
		} else if (error != EAGAIN) {
			free(sec);
			warnx("\nerror while reading from device");
			return (-1);
		}
	}

	free(sec);
	fprintf(stderr, "\rtrack %u '%c' %08u/%08u 100%%\n",
	    ti->track,
	    (ti->isaudio) ? 'a' : 'd', i, n_sec);
	return (0);
}

int
rip_next_track(struct track_info *info)
{
	int error;
	u_int32_t size;

	info->fd = open(info->name, O_CREAT | O_TRUNC | O_RDWR,
	    S_IRUSR | S_IWUSR);
	if (info->fd == -1) {
		warnx("can't open %s file", info->name);
		return (NXTRACK_FAIL);
	}

	if (info->isaudio) {
		/*
 		 * Prepend audio track with Wave header
 		 */
		size = 2352 * (info->end_lba - info->start_lba);
		*(u_int32_t *)(wavehdr + 4) = htole32(size + 36);
		*(u_int32_t *)(wavehdr + 40) = htole32(size);
		error = write_sector(info->fd, wavehdr, sizeof(wavehdr));
		if (error == -1) {
			warnx("can't write WAVE header for %s file",
			    info->name);
			return (NXTRACK_FAIL);
		}
	}

	return (NXTRACK_OK);
}

int
play_next_track(struct track_info *info)
{
	if (!info->isaudio)
		return (NXTRACK_SKIP);

	if (info->hdl != NULL)
		return (NXTRACK_OK);

	info->hdl = sio_open(NULL, SIO_PLAY, 0);
	if (info->hdl == NULL) {
		warnx("could not open audio backend");
		goto bad;
	}

	sio_initpar(&info->par);

	info->par.rate = 44100;
	info->par.pchan = 2;
	info->par.bits = 16;
	info->par.sig = 1;
	info->par.le = 1;
	info->par.appbufsz = info->par.rate * 3 / 4;

	if (sio_setpar(info->hdl, &info->par) == 0) {
		warnx("could not set audio parameters");
		goto bad;
	}

	if (sio_getpar(info->hdl, &info->par) == 0) {
		warnx("could not get audio parameters");
		goto bad;
	}

	if (info->par.le != 1 ||
	    info->par.sig != 1 ||
	    info->par.bits != 16 ||
	    info->par.pchan != 2 ||
	    (info->par.rate > 44100 * 1.05 || info->par.rate < 44100 * 0.95)) {
		warnx("could not configure audio parameters as desired");
		goto bad;
	}

	if (sio_start(info->hdl) == 0) {
		warnx("could not start audio output");
		goto bad;
	}

	return (NXTRACK_OK);

bad:
	if (info->hdl != NULL) {
		sio_close(info->hdl);
		info->hdl = NULL;
	}
	return (NXTRACK_FAIL);
}

static int
rip_tracks_loop(struct track_pair *tp, u_int n_tracks,
    int (*next_track)(struct track_info *))
{
	struct track_info info;
	u_char trk;
	u_int i;
	char order;
	int error;

	info.fd = -1;
	info.hdl = NULL;

	order = (tp->start > tp->end) ? -1 : 1;
	trk = tp->start;
	for (;;) {
		error = 0;
		for (i = 0; i < n_tracks; i++) {
			if (trk == toc_buffer[i].track)
				break;
		}

		if (i != n_tracks) {
			/* Track is found */
			info.track = toc_buffer[i].track;
			info.isaudio = (toc_buffer[i].control & 4) == 0;
			snprintf(info.name, sizeof(info.name), "track%02u.%s",
			    toc_buffer[i].track,
			    (info.isaudio) ? "wav" : "dat");

			if (msf) {
				info.start_lba = msf2lba(
				    toc_buffer[i].addr.msf.minute,
				    toc_buffer[i].addr.msf.second,
				    toc_buffer[i].addr.msf.frame);
				info.end_lba = msf2lba(
				    toc_buffer[i + 1].addr.msf.minute,
				    toc_buffer[i + 1].addr.msf.second,
				    toc_buffer[i + 1].addr.msf.frame);
			} else {
				info.start_lba = toc_buffer[i].addr.lba;
				info.end_lba = toc_buffer[i + 1].addr.lba;
			}

			error = next_track(&info);
			if (error == NXTRACK_FAIL) {
				error = -1;
				break;
			} else if (error != NXTRACK_SKIP) {
				error = read_track(&info);
				if (info.fd >= 0) {
					close(info.fd);
					info.fd = -1;
				}
				if (error != 0) {
					warnx("can't rip %u track",
					    toc_buffer[i].track);
					break;
				}
			}
		}

		if (trk == tp->end)
			break;
		trk += order;
	}

	if (info.hdl != NULL) {
		sio_close(info.hdl);
		info.hdl = NULL;
	}

	return (error);
}

int
rip_tracks(char *arg, int (*next_track)(struct track_info *), int issorted)
{
	struct track_pair_head list;
	struct track_pair *tp;
	struct ioc_toc_header h;
	u_int n;
	int rc;

	rc = ioctl(fd, CDIOREADTOCHEADER, &h);
	if (rc < 0)
		return (rc);

	if (h.starting_track > h.ending_track) {
		warnx("TOC starting_track > TOC ending_track");
		return (0);
	}

	n = h.ending_track - h.starting_track + 1;
	rc = read_toc_entrys((n + 1) * sizeof(struct cd_toc_entry));
	if (rc < 0)
		return (rc);

	parse_tracks_init(&list);
	/* We assume that all spaces are skipped in ``arg''. */
	if (arg == NULL || *arg == '\0') {
		rc = parse_tracks_add(&list, h.starting_track, h.ending_track,
		    0);
	} else {
		rc = parse_tracks(&list, h.starting_track, h.ending_track, arg,
		    issorted);
	}
	if (rc < 0) {
		warnx("can't create track list");
		parse_tracks_final(&list);
		return (rc);
	}

	TAILQ_FOREACH(tp, &list, list) {
		rc = rip_tracks_loop(tp, n, next_track);
		if (rc < 0)
			break;
	}

	parse_tracks_final(&list);
	return (0);
}

int
cdrip(char *arg)
{
	return rip_tracks(arg, rip_next_track, 1);
}

int
cdplay(char *arg)
{
	return rip_tracks(arg, play_next_track, 0);
}
