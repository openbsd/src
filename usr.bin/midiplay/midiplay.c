/*	$OpenBSD: midiplay.c,v 1.17 2015/02/08 23:40:34 deraadt Exp $	*/
/*	$NetBSD: midiplay.c,v 1.8 1998/11/25 22:17:07 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@netbsd.org).
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

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <sndio.h>

struct track {
	u_char *start, *end;
	u_long curtime;
	u_char status;
};

#define MIDI_META 0xff

#define META_SEQNO	0x00
#define META_TEXT	0x01
#define META_COPYRIGHT	0x02
#define META_TRACK	0x03
#define META_INSTRUMENT	0x04
#define META_LYRIC	0x05
#define META_MARKER	0x06
#define META_CUE	0x07
#define META_CHPREFIX	0x20
#define META_EOT	0x2f
#define META_SET_TEMPO	0x51
#define META_KEY	0x59
#define META_SMPTE	0x54
#define META_TIMESIGN	0x58

char *metanames[] = { 
	"", "Text", "Copyright", "Track", "Instrument", 
	"Lyric", "Marker", "Cue",
};

static int midi_lengths[] = { 2,2,2,2,1,1,2,0 };
/* Number of bytes in a MIDI command */
#define MIDI_LENGTH(d) (midi_lengths[((d) >> 4) & 7])

#define MIDI_IS_STATUS(d)	((d) & 0x80)
#define MIDI_IS_COMMON(d)	((d) < 0xf0)
#define MIDI_SYSEX_START	0xf0
#define MIDI_SYSEX_STOP		0xf7

void usage(void);
void send_event(u_char, u_char *, u_int);
void dometa(u_int, u_char *, u_int);
void midireset(void);
u_long getvar(struct track *);
void playfile(FILE *, char *);
void playdata(u_char *, u_int, char *);
void sigalrm(int);
int main(int argc, char **argv);

extern char *__progname;

#define P(c) 1,0x90,c,0x7f,4,0x80,c,0
#define PL(c) 1,0x90,c,0x7f,8,0x80,c,0
#define C 0x3c
#define D 0x3e
#define E 0x40
#define F 0x41

u_char sample[] = { 
	'M','T','h','d',  0,0,0,6,  0,1,  0,1,  0,8,
	'M','T','r','k',  0,0,0,4+13*8,
	P(C), P(C), P(C), P(E), P(D), P(D), P(D), 
	P(F), P(E), P(E), P(D), P(D), PL(C),
	0, 0xff, 0x2f, 0
};
#undef P
#undef PL
#undef C
#undef D
#undef E
#undef F

#define MARK_HEADER "MThd"
#define MARK_TRACK "MTrk"
#define MARK_LEN 4

#define SIZE_LEN 4
#define HEADER_LEN 6

#define GET8(p) ((p)[0])
#define GET16(p) (((p)[0] << 8) | (p)[1])
#define GET24(p) (((p)[0] << 16) | ((p)[1] << 8) | (p)[2])
#define GET32(p) (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3])

void
usage(void)
{
	printf("usage: "
	       "%s [-gmqvx] [-f device] [-t tempo] [file ...]\n",
	       __progname);
	exit(1);
}

int showmeta = 0;
int verbose = 0;
u_int tempo = 60 * 1000000 / 100;	/* default tempo is 100bpm */
int play = 1;
struct mio_hdl *hdl;
struct timespec ts, ts_last;

void
send_event(u_char status, u_char *data, u_int len)
{
	u_int i;

	if (verbose > 1) {
		printf("MIDI %02x", status);
		for (i = 0; i < len; i++)
			printf(" %02x", data[i]);
		printf("\n");
	}
	if (play) {
		mio_write(hdl, &status, 1);
		mio_write(hdl, data, len);
	}
}

u_long
getvar(struct track *tp)
{
	u_long r, c;

	r = 0;
	do {
		c = *tp->start++;
		r = (r << 7) | (c & 0x7f);
	} while ((c & 0x80) && tp->start < tp->end);
	return (r);
}

void
dometa(u_int meta, u_char *p, u_int len)
{
	switch (meta) {
	case META_TEXT:
	case META_COPYRIGHT:
	case META_TRACK:
	case META_INSTRUMENT:
	case META_LYRIC:
	case META_MARKER:
	case META_CUE:
		if (showmeta) {
			printf("%s: ", metanames[meta]);
			fwrite(p, len, 1, stdout);
			printf("\n");
		}
		break;
	case META_SET_TEMPO:
		tempo = GET24(p);
		if (showmeta)
			printf("Tempo: %d us / quarter note\n", tempo);
		break;
	case META_TIMESIGN:
		if (showmeta) {
			int n = p[1];
			int d = 1;
			while (n-- > 0)
				d *= 2;
			printf("Time signature: %d/%d %d,%d\n",
			       p[0], d, p[2], p[3]);
		}
		break;
	case META_KEY:
		if (showmeta)
			printf("Key: %d %s\n", (char)p[0],
			       p[1] ? "minor" : "major");
		break;
	default:
		break;
	}
}

void
midireset(void)
{
	/* General MIDI reset sequence */
	static u_char gm_reset[] = { 0x7e, 0x7f, 0x09, 0x01, 0xf7 };

	send_event(MIDI_SYSEX_START, gm_reset, sizeof gm_reset);
}

void
playfile(FILE *f, char *name)
{
	u_char *buf, *newbuf;
	u_int tot, n, size, newsize, nread;

	/* 
	 * We need to read the whole file into memory for easy processing.
	 * Using mmap() would be nice, but some file systems do not support
	 * it, nor does reading from e.g. a pipe.  The latter also precludes
	 * finding out the file size without reading it.
	 */
	size = 1000;
	buf = malloc(size);
	if (buf == NULL)
		err(1, "malloc() failed");
	nread = size;
	tot = 0;
	for (;;) {
		n = fread(buf + tot, 1, nread, f);
		tot += n;
		if (n < nread)
			break;
		/* There must be more to read. */
		nread = size;
		newsize = size * 2;
		newbuf = realloc(buf, newsize);
		if (newbuf == NULL)
			err(1, "realloc() failed");
		buf = newbuf;
		size = newsize;
	}
	playdata(buf, tot, name);
	free(buf);
}

void
sigalrm(int i)
{
}

void
playdata(u_char *buf, u_int tot, char *name)
{
	long long delta_nsec = 0;
	u_int delta_ticks;
	int format, ntrks, divfmt, ticks, t, besttrk = 0;
	u_int len, mlen;
	u_char *p, *end, byte, meta;
	struct track *tracks;
	u_long bestcur, now;
	struct track *tp;

	end = buf + tot;
	if (verbose)
		printf("Playing %s (%d bytes) ... \n", name, tot);

	if (memcmp(buf, MARK_HEADER, MARK_LEN) != 0) {
		warnx("Not a MIDI file, missing header");
		return;
	}
	if (GET32(buf + MARK_LEN) != HEADER_LEN) {
		warnx("Not a MIDI file, bad header");
		return;
	}
	format = GET16(buf + MARK_LEN + SIZE_LEN);
	ntrks = GET16(buf + MARK_LEN + SIZE_LEN + 2);
	divfmt = GET8(buf + MARK_LEN + SIZE_LEN + 4);
	ticks = GET8(buf + MARK_LEN + SIZE_LEN + 5);
	p = buf + MARK_LEN + SIZE_LEN + HEADER_LEN;
	if ((divfmt & 0x80) == 0)
		ticks |= divfmt << 8;
	else
		errx(1, "Absolute time codes not implemented yet");
	if (verbose > 1)
		printf("format=%d ntrks=%d divfmt=%x ticks=%d\n",
		       format, ntrks, divfmt, ticks);
	if (format != 0 && format != 1) {
		warnx("Cannnot play MIDI file of type %d", format);
		return;
	}
	if (ntrks == 0)
		return;
	tracks = calloc(ntrks, sizeof(struct track));
	if (tracks == NULL)
		err(1, "malloc() tracks failed");
	for (t = 0; t < ntrks; ) {
		if (p >= end - MARK_LEN - SIZE_LEN) {
			warnx("Cannot find track %d", t);
			goto ret;
		}
		len = GET32(p + MARK_LEN);
		if (len > 1000000) { /* a safe guard */
			warnx("Crazy track length");
			goto ret;
		}
		if (memcmp(p, MARK_TRACK, MARK_LEN) == 0) {
			tracks[t].start = p + MARK_LEN + SIZE_LEN;
			tracks[t].end = tracks[t].start + len;
			tracks[t].curtime = getvar(&tracks[t]);
			t++;
		}
		p += MARK_LEN + SIZE_LEN + len;
	}

	/* 
	 * Play MIDI events by selecting the track with the lowest
	 * curtime.  Execute the event, update the curtime and repeat.
	 */

	now = 0;
	delta_nsec = 0;
	if (clock_gettime(CLOCK_MONOTONIC, &ts_last) < 0)
		err(1, "clock_gettime");
	for (;;) {
		/* Locate lowest curtime */
		bestcur = ULONG_MAX;
		for (t = 0; t < ntrks; t++) {
			if (tracks[t].curtime < bestcur) {
				bestcur = tracks[t].curtime;
				besttrk = t;
			}
		}
		if (bestcur == ULONG_MAX)
			break;
		if (verbose > 1) {
			printf("DELAY %4ld TRACK %2d ", bestcur-now, besttrk);
			fflush(stdout);
		}
		while (now < bestcur) {
			pause();
			if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
				err(1, "clock_gettime");
			delta_nsec += 1000000000L * (ts.tv_sec - ts_last.tv_sec);
			delta_nsec += ts.tv_nsec - ts_last.tv_nsec;
			ts_last = ts;
			if (delta_nsec <= 0)
				continue;
			delta_ticks = delta_nsec * ticks / (1000LL * tempo);
			delta_nsec -= 1000LL * delta_ticks * tempo / ticks;
			now += delta_ticks;
		}
		tp = &tracks[besttrk];
		byte = *tp->start++;
		if (byte == MIDI_META) {
			meta = *tp->start++;
			mlen = getvar(tp);
			if (verbose > 1)
				printf("META %02x (%d)\n", meta, mlen);
			dometa(meta, tp->start, mlen);
			tp->start += mlen;
		} else {
			if (MIDI_IS_STATUS(byte))
				tp->status = byte;
			else
				tp->start--;
			if (MIDI_IS_COMMON(tp->status)) {
				mlen = MIDI_LENGTH(tp->status);
				send_event(tp->status, tp->start, mlen);
			} else if (tp->status == MIDI_SYSEX_START) {
				mlen = getvar(tp);
				send_event(MIDI_SYSEX_START, tp->start, mlen);
			} else if (tp->status == MIDI_SYSEX_STOP) {
				mlen = getvar(tp);
				/* Sorry, can't do this yet */;
			} else {
				if (verbose)
					printf("MIDI event 0x%02x ignored\n",
					       tp->status);
			}
			tp->start += mlen;
		}
		if (tp->start >= tp->end)
			tp->curtime = ULONG_MAX;
		else
			tp->curtime += getvar(tp);
	}
 ret:
	free(tracks);
}

int
main(int argc, char **argv)
{
	int ch;
	int example = 0;
	int gmreset = 0;
	char *file = NULL;
	FILE *f;
	const char *errstr;
	struct sigaction sa;
	struct itimerval it;

	while ((ch = getopt(argc, argv, "?d:f:glmqt:vx")) != -1) {
		switch (ch) {
		case 'f':
			file = optarg;
			break;
		case 'g':
			gmreset = 1;
			break;
		case 'm':
			showmeta = 1;
			break;
		case 'q':
			play = 0;
			break;
		case 't':
			tempo = 60 * 1000000 / 
			    strtonum(optarg, 40, 240, &errstr);
			if (errstr)
				errx(1, "tempo is %s: %s", errstr, optarg);
			break;
		case 'v':
			verbose++;
			break;
		case 'x':
			example = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
    
	hdl = mio_open(file, MIO_OUT, 0);
	if (hdl == NULL)
		errx(1, "failed to open MIDI output");
	if (gmreset)
		midireset();

	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sigalrm;
	sigfillset(&sa.sa_mask);
	if (sigaction(SIGALRM, &sa, NULL) < 0)
		err(1, "sigaction");
	it.it_interval.tv_sec = it.it_value.tv_sec = 0;
	it.it_interval.tv_usec = it.it_value.tv_usec = 1000;
	if (setitimer(ITIMER_REAL, &it, NULL) < 0)
		err(1, "setitimer");

	if (example)
		playdata(sample, sizeof sample, "<Gubben Noa>");
	else if (argc == 0)
		playfile(stdin, "<stdin>");
	else
		while (argc--) {
			f = fopen(*argv, "r");
			if (f == NULL)
				err(1, "%s", *argv);
			else {
				playfile(f, *argv);
				fclose(f);
			}
			argv++;
		}

	exit(0);
}
