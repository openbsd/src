/*	$OpenBSD: audioctl.c,v 1.36 2018/09/17 14:14:39 mestre Exp $	*/
/*
 * Copyright (c) 2016 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

/*
 * Default bytes per sample for the given bits per sample.
 */
#define BPS(bits) (((bits) <= 8) ? 1 : (((bits) <= 16) ? 2 : 4))

struct audio_device rname;
struct audio_status rstatus;
struct audio_swpar rpar, wpar;
struct audio_pos rpos;

struct field {
	char *name;
	void *raddr, *waddr;
#define MODE	0
#define NUM	1
#define STR	2
#define ENC	3
	int type;
	int set;
} fields[] = {
	{"name",		&rname.name,		NULL,		STR},
	{"mode",		&rstatus.mode,		NULL,		MODE},
	{"pause",		&rstatus.pause,		NULL,		NUM},
	{"active",		&rstatus.active,	NULL,		NUM},
	{"nblks",		&rpar.nblks,		&wpar.nblks,	NUM},
	{"blksz",		&rpar.round,		&wpar.round,	NUM},
	{"rate",		&rpar.rate,		&wpar.rate,	NUM},
	{"encoding",		&rpar,			&wpar,		ENC},
	{"play.channels",	&rpar.pchan,		&wpar.pchan,	NUM},
	{"play.bytes",		&rpos.play_pos,		NULL,		NUM},
	{"play.errors",		&rpos.play_xrun,	NULL,		NUM},
	{"record.channels",	&rpar.rchan,		&wpar.rchan, 	NUM},
	{"record.bytes",	&rpos.rec_pos,		NULL,		NUM},
	{"record.errors",	&rpos.rec_xrun,		NULL,		NUM},
	{NULL,			NULL,			0}
};

const char usagestr[] =
	"usage: audioctl [-f file]\n"
	"       audioctl [-n] [-f file] name ...\n"
	"       audioctl [-nq] [-f file] name=value ...\n";

/*
 * parse encoding string (examples: s8, u8, s16, s16le, s24be ...)
 * and fill enconding fields of audio_swpar structure
 */
int
strtoenc(struct audio_swpar *ap, char *p)
{
	/* expect "s" or "u" (signedness) */
	if (*p == 's')
		ap->sig = 1;
	else if (*p == 'u')
		ap->sig = 0;
	else
		return 0;
	p++;

	/* expect 1-2 decimal digits (bits per sample) */
	ap->bits = 0;
	while (*p >= '0' && *p <= '9') {
		ap->bits = (ap->bits * 10) + *p++ - '0';
		if (ap->bits > 32)
			return 0;
	}
	if (ap->bits < 8)
		return 0;

	/* set defaults as next tokens are optional */
	ap->bps = BPS(ap->bits);
	ap->le = (BYTE_ORDER == LITTLE_ENDIAN);
	ap->msb = 1;
	if (*p == '\0')
		return 1;

	/* expect "le" or "be" (endianness) */
	if (p[0] == 'l' && p[1] == 'e')
		ap->le = 1;
	else if (p[0] == 'b' && p[1] == 'e')
		ap->le = 0;
	else
		return 0;
	p += 2;
	if (*p == '\0')
		return 1;

	/* expect 1 decimal digit (number of bytes) */
	if (*p < '0' || *p > '9')
		return 0;
	ap->bps = *p - '0';
	if (ap->bps < ((ap->bits + 7) >> 3) || ap->bps > 4)
		return 0;
	if (*++p == '\0')
		return 1;

	/* expect "msb" or "lsb" (alignment) */
	if (p[0] == 'm' && p[1] == 's' && p[2] == 'b')
		ap->msb = 1;
	else if (p[0] == 'l' && p[1] == 's' && p[2] == 'b')
		ap->msb = 0;
	else if (*p == '\0')
		return 1;
	p += 3;
	if (*p == '\0')
		return 1;

	/* must be no additional junk */
	return 0;
}

void
print_val(struct field *p, void *addr)
{
	int mode;
	struct audio_swpar *ap;

	switch (p->type) {
	case NUM:
		printf("%u", *(unsigned int *)addr);
		break;
	case STR:
		printf("%s", (char *)addr);
		break;
	case MODE:
		mode = *(unsigned int *)addr;
		if (mode & AUMODE_PLAY)
			printf("play");
		if (mode & AUMODE_RECORD) {
			if (mode & AUMODE_PLAY)
				printf(",");
			printf("record");
		}
		break;
	case ENC:
		ap = addr;
		printf("%s%u", ap->sig ? "s" : "u", ap->bits);
		if (ap->bps == 1)
			break;
		printf("%s", ap->le ? "le" : "be");
		if (ap->bps != BPS(ap->bits) || ap->bits < ap->bps * 8) {
			printf("%u", ap->bps);
			if (ap->bits < ap->bps * 8)
				printf("%s", ap->msb ? "msb" : "lsb");
		}
	}
}

void
parse_val(struct field *f, void *addr, char *p)
{
	const char *strerr;

	switch (f->type) {
	case NUM:
		*(unsigned int *)addr = strtonum(p, 0, UINT_MAX, &strerr);
		if (strerr)
			errx(1, "%s: %s", p, strerr);
		break;
	case ENC:
		if (!strtoenc((struct audio_swpar *)addr, p))
			errx(1, "%s: bad encoding", p);
	}
}

int
main(int argc, char **argv)
{
	struct field *f;
	char *lhs, *rhs, *path = "/dev/audioctl0";
	int fd, c, set = 0, print_names = 1, quiet = 0;

	while ((c = getopt(argc, argv, "anf:q")) != -1) {
		switch (c) {
		case 'a':	/* ignored, compat */
			break;
		case 'n':
			print_names = 0;
			break;
		case 'f':
			path = optarg;
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			fputs(usagestr, stderr);
			return 1;
		}
	}
	argc -= optind;
	argv += optind;

	if (unveil(path, "rw") == -1)
		err(1, "unveil");
	if (unveil(NULL, NULL) == -1)
		err(1, "unveil");

	fd = open(path, O_RDWR);
	if (fd < 0)
		err(1, "%s", path);
	if (ioctl(fd, AUDIO_GETSTATUS, &rstatus) < 0)
		err(1, "AUDIO_GETSTATUS");
	if (ioctl(fd, AUDIO_GETDEV, &rname) < 0)
		err(1, "AUDIO_GETDEV");
	if (ioctl(fd, AUDIO_GETPAR, &rpar) < 0)
		err(1, "AUDIO_GETPAR");
	if (ioctl(fd, AUDIO_GETPOS, &rpos) < 0)
		err(1, "AUDIO_GETPOS");
	if (argc == 0) {
		for (f = fields; f->name != NULL; f++) {
			printf("%s=", f->name);
			print_val(f, f->raddr);
			printf("\n");
		}
	}
	AUDIO_INITPAR(&wpar);
	for (; argc > 0; argc--, argv++) {
		lhs = *argv;
		rhs = strchr(*argv, '=');
		if (rhs)
			*rhs++ = '\0';
		for (f = fields;; f++) {
			if (f->name == NULL)
				errx(1, "%s: unknown parameter", lhs);
			if (strcmp(f->name, lhs) == 0)
				break;
		}
		if (rhs) {
			if (f->waddr == NULL)
				errx(1, "%s: is read only", f->name);
			parse_val(f, f->waddr, rhs);
			f->set = 1;
			set = 1;
		} else {
			if (print_names)
				printf("%s=", f->name);
			print_val(f, f->raddr);
			printf("\n");
		}
	}
	if (!set) {
		close(fd);
		return 0;
	}
	if (ioctl(fd, AUDIO_SETPAR, &wpar) < 0)
		err(1, "AUDIO_SETPAR");
	if (ioctl(fd, AUDIO_GETPAR, &wpar) < 0)
		err(1, "AUDIO_GETPAR");
	for (f = fields; f->name != NULL; f++) {
		if (!f->set || quiet)
			continue;
		if (print_names) {
			printf("%s: ", f->name);
			print_val(f, f->raddr);
			printf(" -> ");
		}
		print_val(f, f->waddr);
		printf("\n");
	}
	close(fd);
	return 0;	
}
