/*	$OpenBSD: mixerctl.c,v 1.9 2002/12/03 22:27:42 pvalchev Exp $	*/
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>

char *catstr(char *p, char *q);
struct field *findfield(char *name);
void prfield(struct field *p, char *sep, int prvalset);
int rdfield(struct field *p, char *q);
int main(int argc, char **argv);
void usage(void);

FILE *out = stdout;

struct field {
	char *name;
	mixer_ctrl_t *valp;
	mixer_devinfo_t *infp;
	char changed;
} *fields, *rfields;

mixer_ctrl_t *values;
mixer_devinfo_t *infos;

char *
catstr(char *p, char *q)
{
	int len;
	char *r;

	len = strlen(p) + 1 + strlen(q) + 1;
	if ((r = malloc(len)) == NULL)
		err(1, "malloc()");
	strlcpy(r, p, len);
	strlcat(r, ".", len);
	strlcat(r, q, len);
	return (r);
}

struct field *
findfield(char *name)
{
	int i;
	for(i = 0; fields[i].name; i++)
		if (strcmp(fields[i].name, name) == 0)
			return &fields[i];
	return (0);
}

void
prfield(struct field *p, char *sep, int prvalset)
{
	mixer_ctrl_t *m;
	int i, n;

	if (sep)
		fprintf(out, "%s%s", p->name, sep);
	m = p->valp;
	switch(m->type) {
	case AUDIO_MIXER_ENUM:
		for(i = 0; i < p->infp->un.e.num_mem; i++)
			if (p->infp->un.e.member[i].ord == m->un.ord)
				fprintf(out, "%s",
					p->infp->un.e.member[i].label.name);
		if (prvalset) {
			fprintf(out, "  [ ");
			for(i = 0; i < p->infp->un.e.num_mem; i++)
				fprintf(out, "%s ", p->infp->un.e.member[i].label.name);
			fprintf(out, "]");
		}
		break;
	case AUDIO_MIXER_SET:
		for(n = i = 0; i < p->infp->un.s.num_mem; i++)
			if (m->un.mask & p->infp->un.s.member[i].mask)
				fprintf(out, "%s%s", n++ ? "," : "",
					p->infp->un.s.member[i].label.name);
		if (prvalset) {
			fprintf(out, "  { ");
			for(i = 0; i < p->infp->un.s.num_mem; i++)
				fprintf(out, "%s ", p->infp->un.s.member[i].label.name);
			fprintf(out, "}");
		}
		break;
	case AUDIO_MIXER_VALUE:
		if (m->un.value.num_channels == 1)
			fprintf(out, "%d", m->un.value.level[0]);
		else
			fprintf(out, "%d,%d", m->un.value.level[0],
			       m->un.value.level[1]);
		if (prvalset)
			fprintf(out, " %s", p->infp->un.v.units.name);
		break;
	default:
		errx(1, "Invalid format.");
	}
}

int
rdfield(struct field *p, char *q)
{
	mixer_ctrl_t *m;
	int v, v0, v1, mask;
	int i;
	char *s;

	m = p->valp;
	switch(m->type) {
	case AUDIO_MIXER_ENUM:
		for(i = 0; i < p->infp->un.e.num_mem; i++)
			if (strcmp(p->infp->un.e.member[i].label.name, q) == 0)
				break;
		if (i < p->infp->un.e.num_mem)
			m->un.ord = p->infp->un.e.member[i].ord;
		else
		        errx(1, "Bad enum value %s", q);
		break;
	case AUDIO_MIXER_SET:
		mask = 0;
		for(v = 0; q && *q; q = s) {
			if (s = strchr(q, ','))
				*s++ = 0;
			for (i = 0; i < p->infp->un.s.num_mem; i++)
				if (strcmp(p->infp->un.s.member[i].label.name, q) == 0)
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
			if (sscanf(q, "%d", &v) == 1) {
				switch (*q) {
				case '+':
				case '-':
					m->un.value.level[0] += v;
					break;
				default:
					m->un.value.level[0] = v;
					break;
				}
			} else
				errx(1, "Bad number %s", q);
		} else {
			if (sscanf(q, "%d,%d", &v0, &v1) == 2) {
				switch (*q) {
				case '+':
				case '-':
					m->un.value.level[0] += v0;
					break;
				default:
					m->un.value.level[0] = v0;
					break;
				}
				s = strchr(q, ',') + 1;
				switch (*s) {
				case '+':
				case '-':
					m->un.value.level[1] += v1;
					break;
				default:
					m->un.value.level[1] = v1;
					break;
				}
			} else if (sscanf(q, "%d", &v) == 1) {
				switch (*q) {
				case '+':
				case '-':
					m->un.value.level[0] += v;
					m->un.value.level[1] += v;
					break;
				default:
					m->un.value.level[0] = v;
					m->un.value.level[1] = v;
					break;
				}
			} else
				errx(1, "Bad numbers %s", q);
		}
		break;
	default:
		errx(1, "Invalid format.");
	}
	p->changed = 1;
	return (1);
}

int
main(int argc, char **argv)
{
	int fd, i, j, ch, pos;
	int aflag = 0, wflag = 0, vflag = 0;
	char *file;
	char *sep = "=";
	mixer_devinfo_t dinfo;
	mixer_ctrl_t val;
	int ndev = 0;

	if ((file = getenv("MIXERDEVICE")) == 0 || *file == '\0')
	        file = "/dev/mixer";

	while ((ch = getopt(argc, argv, "af:nvw")) != -1) {
		switch(ch) {
		case 'a':
			aflag++;
			break;
		case 'w':
			wflag++;
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
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((fd = open(file, wflag ? O_RDWR : O_RDONLY)) < 0)
		err(1, "%s", file);

	for(;;) {
	       dinfo.index = ndev++;
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

	for(i = 0; i < ndev; i++) {
		infos[i].index = i;
		ioctl(fd, AUDIO_MIXER_DEVINFO, &infos[i]);
	}

	for(i = 0; i < ndev; i++) {
		rfields[i].name = infos[i].label.name;
		rfields[i].valp = &values[i];
		rfields[i].infp = &infos[i];
	}

	for(i = 0; i < ndev; i++) {
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

	for(j = i = 0; i < ndev; i++) {
		if (infos[i].type != AUDIO_MIXER_CLASS &&
		    infos[i].type != -1) {
			fields[j++] = rfields[i];
			for(pos = infos[i].next; pos != AUDIO_MIXER_LAST;
			    pos = infos[pos].next) {
				fields[j] = rfields[pos];
				fields[j].name = catstr(rfields[i].name,
							infos[pos].label.name);
				infos[pos].type = -1;
				j++;
			}
		}
	}

	for(i = 0; i < j; i++) {
		int cls = fields[i].infp->mixer_class;
		if (cls >= 0 && cls < ndev)
			fields[i].name = catstr(infos[cls].label.name,
						fields[i].name);
	}

	if (!argc && aflag && !wflag) {
		for(i = 0; fields[i].name; i++) {
			prfield(&fields[i], sep, vflag);
			fprintf(out, "\n");
		}
	} else if (argc > 0 && !aflag) {
		struct field *p;
		if (wflag) {
			while(argc--) {
				char *q;

				if (q = strchr(*argv, '=')) {
					*q++ = 0;
					p = findfield(*argv);
					if (p == 0)
						warnx("field %s does not exist", *argv);
					else {
						val = *p->valp;
						if (rdfield(p, q)) {
							if (ioctl(fd, AUDIO_MIXER_WRITE, p->valp) < 0)
								warn("AUDIO_MIXER_WRITE");
							else if (sep) {
								*p->valp = val;
								prfield(p, ": ", 0);
								ioctl(fd, AUDIO_MIXER_READ, p->valp);
								printf(" -> ");
								prfield(p, 0, 0);
								printf("\n");
							}
						}
					}
				} else
					warnx("No `=' in %s", *argv);
				argv++;
			}
		} else {
			while(argc--) {
				p = findfield(*argv);
				if (p == 0)
					warnx("field %s does not exist", *argv);
				else
					prfield(p, sep, vflag), fprintf(out, "\n");
				argv++;
			}
		}
	} else
		usage();
	exit(0);
}

void
usage(void)
{
	extern char *__progname;	/* from crt0.o */

	fprintf(stderr,
	    "usage: %s [-f file] [-n] [-v] name ...\n"
	    "       %s [-f file] [-n] [-v] -w name=value ...\n"
	    "       %s [-f file] [-n] [-v] -a\n", __progname,
		__progname, __progname);

	exit(1);
}
