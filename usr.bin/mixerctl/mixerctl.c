/*	$OpenBSD: mixerctl.c,v 1.20 2005/02/07 14:29:10 millert Exp $	*/
/*	$NetBSD: mixerctl.c,v 1.11 1998/04/27 16:55:23 augustss Exp $	*/

/*
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson, with some code and ideas from Chuck Cranor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*
 * mixerctl(1) - a program to control audio mixing.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct field *findfield(char *);
void adjlevel(char **, u_char *, int);
void catstr(char *, char *, char *);
void prfield(struct field *, char *, int);
void rdfield(int, struct field *, char *);
__dead void usage(void);

#define FIELD_NAME_MAX	64

struct field {
	char name[FIELD_NAME_MAX];
	mixer_ctrl_t *valp;
	mixer_devinfo_t *infp;
} *fields, *rfields;

mixer_ctrl_t *values;
mixer_devinfo_t *infos;

void
catstr(char *p, char *q, char *out)
{
	char tmp[FIELD_NAME_MAX];

	snprintf(tmp, FIELD_NAME_MAX, "%s.%s", p, q);
	strlcpy(out, tmp, FIELD_NAME_MAX);
}

struct field *
findfield(char *name)
{
	int i;
	for (i = 0; fields[i].name[0] != '\0'; i++)
		if (strcmp(fields[i].name, name) == 0)
			return &fields[i];
	return (0);
}

#define e_member_name	un.e.member[i].label.name
#define s_member_name	un.s.member[i].label.name

void
prfield(struct field *p, char *sep, int prvalset)
{
	mixer_ctrl_t *m;
	int i, n;

	if (sep)
		printf("%s%s", p->name, sep);
	m = p->valp;
	switch (m->type) {
	case AUDIO_MIXER_ENUM:
		for (i = 0; i < p->infp->un.e.num_mem; i++)
			if (p->infp->un.e.member[i].ord == m->un.ord)
				printf("%s",
					p->infp->e_member_name);
		if (prvalset) {
			printf("  [ ");
			for (i = 0; i < p->infp->un.e.num_mem; i++)
				printf("%s ", p->infp->e_member_name);
			printf("]");
		}
		break;
	case AUDIO_MIXER_SET:
		for (n = i = 0; i < p->infp->un.s.num_mem; i++)
			if (m->un.mask & p->infp->un.s.member[i].mask)
				printf("%s%s", n++ ? "," : "",
						p->infp->s_member_name);
		if (prvalset) {
			printf("  { ");
			for (i = 0; i < p->infp->un.s.num_mem; i++)
				printf("%s ", p->infp->s_member_name);
			printf("}");
		}
		break;
	case AUDIO_MIXER_VALUE:
		if (m->un.value.num_channels == 1)
			printf("%d", m->un.value.level[0]);
		else
			printf("%d,%d", m->un.value.level[0],
			       m->un.value.level[1]);
		if (prvalset)
			printf(" %s", p->infp->un.v.units.name);
		break;
	default:
		errx(1, "Invalid format.");
	}
}

void
adjlevel(char **p, u_char *olevel, int more)
{
	char *ep, *cp = *p;
	long inc;
	u_char level;

	if (*cp != '+' && *cp != '-')
		*olevel = 0;		/* absolute setting */

	errno = 0;
	inc = strtol(cp, &ep, 10);
	if (*cp == '\0' || (*ep != '\0' && *ep != ',') ||
	    (errno == ERANGE && (inc == LONG_MAX || inc == LONG_MIN)))
		errx(1, "Bad number %s", cp);
	if (*ep == ',' && !more)
		errx(1, "Too many values");
	*p = ep;

	if (inc < AUDIO_MIN_GAIN - *olevel)
		level = AUDIO_MIN_GAIN;
	else if (inc > AUDIO_MAX_GAIN - *olevel)
		level = AUDIO_MAX_GAIN;
	else
		level = *olevel + inc;
	*olevel = level;
}

void
rdfield(int fd, struct field *p, char *q)
{
	mixer_ctrl_t *m, oldval;
	int i, mask;
	char *s;

	oldval = *p->valp;
	m = p->valp;

	switch (m->type) {
	case AUDIO_MIXER_ENUM:
		for (i = 0; i < p->infp->un.e.num_mem; i++)
			if (strcmp(p->infp->e_member_name, q) == 0)
				break;
		if (i < p->infp->un.e.num_mem)
			m->un.ord = p->infp->un.e.member[i].ord;
		else
		        errx(1, "Bad enum value %s", q);
		break;
	case AUDIO_MIXER_SET:
		mask = 0;
		for (; q && *q; q = s) {
			if ((s = strchr(q, ',')) != NULL)
				*s++ = 0;
			for (i = 0; i < p->infp->un.s.num_mem; i++)
				if (strcmp(p->infp->s_member_name, q) == 0)
					break;
			if (i < p->infp->un.s.num_mem)
				mask |= p->infp->un.s.member[i].mask;
			else
			        errx(1, "Bad set value %s", q);
		}
		m->un.mask = mask;
		break;
	case AUDIO_MIXER_VALUE:
		if (m->un.value.num_channels == 1) {
			adjlevel(&q, &m->un.value.level[0], 0);
		} else {
			adjlevel(&q, &m->un.value.level[0], 1);
			if (*q++ == ',')
				adjlevel(&q, &m->un.value.level[1], 0);
			else
				m->un.value.level[1] = m->un.value.level[0];
		}
		break;
	default:
		errx(1, "Invalid format.");
	}

	if (ioctl(fd, AUDIO_MIXER_WRITE, p->valp) < 0) {
		warn("AUDIO_MIXER_WRITE");
	} else {
		*p->valp = oldval;
		prfield(p, ": ", 0);
		if (ioctl(fd, AUDIO_MIXER_READ, p->valp) < 0) {
			warn("AUDIO_MIXER_READ");
		} else {
			printf(" -> ");
			prfield(p, NULL, 0);
			printf("\n");
		}
	}
}

int
main(int argc, char **argv)
{
	int fd, i, j, ch, pos;
	int aflag = 0, qflag = 0, vflag = 0;
	char *file;
	char *sep = "=";
	mixer_devinfo_t dinfo;
	int ndev;

	if ((file = getenv("MIXERDEVICE")) == 0 || *file == '\0')
	        file = "/dev/mixer";

	while ((ch = getopt(argc, argv, "af:nqvw")) != -1) {
		switch(ch) {
		case 'a':
			aflag++;
			break;
		case 'w':
			/* compat */
			break;
		case 'v':
			vflag++;
			break;
		case 'n':
			sep = 0;
			break;
		case 'f':
			file = optarg;
			break;
		case 'q':
			qflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((fd = open(file, O_RDWR)) == -1)
		if ((fd = open(file, O_RDONLY)) == -1)
			err(1, "%s", file);

	for (ndev = 0; ; ndev++) {
		dinfo.index = ndev;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &dinfo) < 0)
			break;
	}

	if (!ndev)
		errx(1, "no mixer devices configured");

	if ((rfields = calloc(ndev, sizeof *rfields)) == NULL ||
	    (fields = calloc(ndev, sizeof *fields)) == NULL ||
	    (infos = calloc(ndev, sizeof *infos)) == NULL ||
	    (values = calloc(ndev, sizeof *values)) == NULL)
		err(1, "calloc()");

	for (i = 0; i < ndev; i++) {
		infos[i].index = i;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &infos[i]) < 0) {
			ndev--;
			i--;
			continue;
		}
	}

	for (i = 0; i < ndev; i++) {
		strlcpy(rfields[i].name, infos[i].label.name, FIELD_NAME_MAX);
		rfields[i].valp = &values[i];
		rfields[i].infp = &infos[i];
	}

	for (i = 0; i < ndev; i++) {
		values[i].dev = i;
		values[i].type = infos[i].type;
		if (infos[i].type != AUDIO_MIXER_CLASS) {
			values[i].un.value.num_channels = 2;
			if (ioctl(fd, AUDIO_MIXER_READ, &values[i]) < 0) {
				values[i].un.value.num_channels = 1;
				if (ioctl(fd, AUDIO_MIXER_READ, &values[i]) < 0)
					err(1, "AUDIO_MIXER_READ");
			}
		}
	}

	for (j = i = 0; i < ndev; i++) {
		if (infos[i].type != AUDIO_MIXER_CLASS &&
		    infos[i].type != -1) {
			fields[j++] = rfields[i];
			for (pos = infos[i].next; pos != AUDIO_MIXER_LAST;
			    pos = infos[pos].next) {
				fields[j] = rfields[pos];
				catstr(rfields[i].name, infos[pos].label.name,
				    fields[j].name);
				infos[pos].type = -1;
				j++;
			}
		}
	}

	for (i = 0; i < j; i++) {
		int cls = fields[i].infp->mixer_class;
		if (cls >= 0 && cls < ndev)
			catstr(infos[cls].label.name, fields[i].name,
			    fields[i].name);
	}

	if (!argc && aflag) {
		for (i = 0; fields[i].name[0] != '\0'; i++) {
			prfield(&fields[i], sep, vflag);
			printf("\n");
		}
	} else if (argc > 0 && !aflag) {
		struct field *p;

		while (argc--) {
			char *q;

			ch = 0;
			if ((q = strchr(*argv, '=')) != NULL) {
				*q++ = '\0';
				ch = 1;
			}

			if ((p = findfield(*argv)) == NULL) {
				warnx("field %s does not exist", *argv);
			} else if (ch) {
				rdfield(fd, p, q);
			} else {
				prfield(p, sep, vflag);
				printf("\n");
			}

			argv++;
		}
	} else
		usage();
	exit(0);
}

__dead void
usage(void)
{
	extern char *__progname;	/* from crt0.o */

	fprintf(stderr,
	    "usage: %s [-nv] [-f file] -a\n"
	    "       %s [-nv] [-f file] name [...]\n"
	    "       %s [-q]  [-f file] name=value [...]\n",
	    __progname, __progname, __progname);

	exit(1);
}
