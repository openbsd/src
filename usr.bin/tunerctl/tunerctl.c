/*
 * Copyright (c) 2005 Jacob Meuser <jakemsr@jakemsr.com>
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
 *  $OpenBSD: tunerctl.c,v 1.1 2005/07/04 21:10:26 jakemsr Exp $
 */


#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <dev/ic/bt8xx.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_TUNER_DEVICE "/dev/tuner0"

struct fields {
	char	*name;
	int	 type;
#define INT	1
#define ASRC	2
#define CSET	3
#define MUTE	4
#define OFFON	5
#define FREQ	6
#define VSAT	7
#define USAT	8
#define HUE	9
#define BRIGHT	10
#define CONTR	11
	int	 val;
	long	 io_set;
	long	 io_get;
	int	 valmin;
	int	 valmax;
} fields[] = {
{ "chanset",	CSET,	0, TVTUNER_SETTYPE, TVTUNER_GETTYPE,	0, 0 },
{ "channel",	INT,	0, TVTUNER_SETCHNL, TVTUNER_GETCHNL,	0, 150 },
{ "freq",	FREQ,	0, TVTUNER_SETFREQ, TVTUNER_GETFREQ,	608, 14240 },
{ "afc",	OFFON,	0, TVTUNER_SETAFC,  TVTUNER_GETAFC,	0, 1 },
{ "audio",	ASRC,	0, BT848_SAUDIO,    BT848_GAUDIO,	0, 0 },
{ "mute",	MUTE,	0, BT848_SAUDIO,    BT848_GAUDIO,	0, 1 },
{ "bright", 	BRIGHT,	0, BT848_SBRIG,	    BT848_GBRIG,
    BT848_BRIGHTMIN, BT848_BRIGHTMAX },
{ "contrast", 	CONTR,	0, BT848_SCONT,	    BT848_GCONT,
    BT848_CONTRASTMIN, BT848_CONTRASTMAX },
{ "hue", 	HUE,	0, BT848_SHUE,	    BT848_GHUE,		BT848_HUEMIN,
    BT848_HUEMAX },
{ "usat", 	USAT,	0, BT848_SUSAT,	    BT848_GUSAT,	BT848_SATUMIN,
    BT848_SATUMAX },
{ "vsat", 	VSAT,	0, BT848_SVSAT,	    BT848_GVSAT,	BT848_SATVMIN,
    BT848_SATVMAX },
{ 0, 0, 0, 0, 0, 0, 0}
};

struct chansets {
	int value;
	char *name;
} chansets[] = {
{ CHNLSET_NABCST,	"nabcst",	},
{ CHNLSET_CABLEIRC,	"cableirc",	},
{ CHNLSET_CABLEHRC,	"cablehrc",	},
{ CHNLSET_WEUROPE,	"weurope",	},
{ CHNLSET_JPNBCST,	"jpnbcst",	},
{ CHNLSET_JPNCABLE,	"jpncable",	},
{ CHNLSET_XUSSR,	"xussr",	},
{ CHNLSET_AUSTRALIA,	"australia",	},
{ CHNLSET_FRANCE,	"france",	},
{ 0, 0 }
};

struct audiosources {
	int value;
	char *name;
} audiosources[] = {
{ AUDIO_TUNER,		"tuner",	},
{ AUDIO_EXTERN,		"extern",	},
{ AUDIO_INTERN,		"intern",	},
{ 0, 0 }
};

int	 tuner_fd;
int	 print_choices;
int	 print_name;
int	 print_value;

__dead void usage(void);
int	 run(int, char *);
int	 findfield(char *);
int	 prfield(int);
int	 do_ioctls(int, char *);
#define OFF	0
#define	ON	1
int	 isoffon(const char *);


/* getopt externs */
extern char *optarg;
extern int opterr;
extern int optind;
extern int optopt;
extern int optreset;


__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
		"usage: %s [-nv] [-f file] -a\n"
		"       %s [-nv] [-f file] name [...]\n"
		"       %s [-q]  [-f file] name=value [...]\n",
			__progname, __progname, __progname);

	exit (1);
}

int
isoffon(const char *offon)
{
	if (strncmp(offon, "off", 3) == 0)
		return (OFF);
	else if (strncmp(offon, "on", 2) == 0)
		return (ON);

	return (-1);
}

int
findfield(char *name)
{
	int i, found = 0;

	for (i = 0; fields[i].name; i++) {
		if (strncmp(fields[i].name, name, strlen(fields[i].name)) ==0) {
			found = 1;
			break;
		}
	}
	if (found == 1)
		return (i);
	else
		return (-1);
}

int
prfield(int index)
{
	int	switchval;
	int	i;

	if (print_name == 1)
		printf("%s=", fields[index].name);

	if (ioctl(tuner_fd, fields[index].io_get, &fields[index].val) < 0) {
		warn("%s", fields[index].name);
		return (1);
	}

	switchval = fields[index].type;
	switch (switchval) {
	case ASRC:
		for (i = 0; audiosources[i].name; i++)
			if (audiosources[i].value ==
			    (fields[index].val & ~AUDIO_MUTE))
				break;
		printf("%s", audiosources[i].name);
		if (print_choices == 1) {
			printf("  [ ");
			for (i = 0; audiosources[i].name; i++)
				printf("%s ", audiosources[i].name);
			printf("]");
		}
		break;
	case CSET:
		for (i = 0; chansets[i].name; i++)
			if (chansets[i].value == fields[index].val)
				break;
		printf("%s", chansets[i].name);
		if (print_choices == 1) {
			printf("  [ ");
			for (i = 0; chansets[i].name; i++)
				printf("%s ", chansets[i].name);
			printf("]");
		}
		break;
	case FREQ:
		printf("%0.2f", (double)fields[index].val / 16);
		if (print_choices == 1)
			printf("  ( %0.2f - %0.2f )",
			    (double)fields[index].valmin / 16,
			    (double)fields[index].valmax / 16);
		break;
	case INT:
	case BRIGHT:
	case CONTR:
	case HUE:
	case VSAT:
	case USAT:
		i = fields[index].val;
		if (switchval == BRIGHT) {
			i = (i - BT848_BRIGHTREGMIN) *
			    BT848_BRIGHTRANGE / BT848_BRIGHTSTEPS +
			    BT848_BRIGHTMIN + (i < 0 ? -0.5 : 0.5);
		} else if (switchval == CONTR) {
			i = (i - BT848_CONTRASTREGMIN) *
			    BT848_CONTRASTRANGE / BT848_CONTRASTSTEPS +
			    BT848_CONTRASTMIN + (i < 0 ? -0.5 : 0.5);
		} else if (switchval == HUE) {
			i = (i - BT848_HUEREGMIN) *
			    BT848_HUERANGE / BT848_HUESTEPS +
			    BT848_HUEMIN + (i < 0 ? -0.5 : 0.5);
		} else if (switchval == USAT) {
			i = (i - BT848_SATUREGMIN) *
			    BT848_SATURANGE / BT848_SATUSTEPS +
			    BT848_SATUMIN + (i < 0 ? -0.5 : 0.5);
		} else if (switchval == VSAT) {
			i = (i - BT848_SATVREGMIN) *
			    BT848_SATVRANGE / BT848_SATVSTEPS +
			    BT848_SATVMIN + (i < 0 ? -0.5 : 0.5);
		}
		printf("%d", i);
		if (print_choices == 1)
			printf("  ( %d - %d )", fields[index].valmin,
			    fields[index].valmax);
		break;		
	case MUTE:
	case OFFON:
		if (((switchval == MUTE) && (fields[index].val & AUDIO_MUTE)) ||
		    ((switchval != MUTE) && (fields[index].val == 1)))
			printf("on");
		else
			printf("off");
		if (print_choices == 1)
			printf("  [ off on ]");
		break;
	default:
		warnx("internal error: prfield");
		break;
	}
	printf("\n");

	return (0);
}

int
do_ioctls(int index, char *arg)
{
	const char *errstr;
	int	 i;
	int	 switchval;

	switchval = fields[index].type;

	if (arg != NULL) {
		switch(switchval) {
		case ASRC:
			for (i = 0; audiosources[i].name; i++)
				if (strncmp(audiosources[i].name, arg,
				    strlen(audiosources[i].name)) == 0)
					break;
			if (audiosources[i].name[0] != '\0')
				fields[index].val = audiosources[i].value;
			else {
				warnx("%s is invalid: %s", fields[index].name,
				    arg);
				return (1);
			}
			break;
		case CSET:
			for (i = 0; chansets[i].name; i++)
				if (strncmp(chansets[i].name, arg,
				    strlen(chansets[i].name)) == 0)
					break;
			if (chansets[i].name[0] != '\0')
				fields[index].val = chansets[i].value;
			else {
				warnx("%s is invalid: %s", fields[index].name,
				    arg);
				return (1);
			}
			break;
		case FREQ:
			fields[index].val = strtod(arg, (char **)NULL) * 16;
			if ((fields[index].val < fields[index].valmin) ||
			    (fields[index].val > fields[index].valmax)) {
				warnx("%s is invalid: %s", fields[index].name,
				    arg);
				return (1);
			}
			break;
		case INT:
		case BRIGHT:
		case CONTR:
		case HUE:
		case USAT:
		case VSAT:
			i = strtonum(arg, fields[index].valmin,
			    fields[index].valmax, &errstr);
			if (errstr != NULL) {
				warnx("%s is %s: %s", fields[index].name,
				    errstr, arg);
				return (1);
			}
			if (switchval == BRIGHT) {
				i = (i - BT848_BRIGHTMIN) *
				    BT848_BRIGHTSTEPS / BT848_BRIGHTRANGE +
				    BT848_BRIGHTREGMIN + (i < 0 ? -0.5 : 0.5);
				if (i > BT848_BRIGHTREGMAX)
					i = BT848_BRIGHTREGMAX;
			} else if (switchval == CONTR) {
				i = (i - BT848_CONTRASTMIN) *
				    BT848_CONTRASTSTEPS / BT848_CONTRASTRANGE +
				    BT848_CONTRASTREGMIN + (i < 0 ? -0.5 : 0.5);
				if (i > BT848_CONTRASTREGMAX)
					i = BT848_CONTRASTREGMAX;
			} else if (switchval == HUE) {
				i = (i - BT848_HUEMIN) *
				    BT848_HUESTEPS / BT848_HUERANGE +
				    BT848_HUEREGMIN + (i < 0 ? -0.5 : 0.5);
				if (i > BT848_HUEREGMAX)
					i = BT848_HUEREGMAX;
			} else if (switchval == USAT) {
				i = (i - BT848_SATUMIN) *
				    BT848_SATUSTEPS / BT848_SATURANGE +
				    BT848_SATUREGMIN + (i < 0 ? -0.5 : 0.5);
				if (i > BT848_SATUREGMAX)
					i = BT848_SATUREGMAX;
			} else if (switchval == VSAT) {
				i = (i - BT848_SATVMIN) *
				    BT848_SATVSTEPS / BT848_SATVRANGE +
				    BT848_SATVREGMIN + (i < 0 ? -0.5 : 0.5);
				if (i > BT848_SATVREGMAX)
					i = BT848_SATVREGMAX;
			}
			fields[index].val = i;
			break;
		case MUTE:
		case OFFON:
			fields[index].val = isoffon(arg);
			if (fields[index].val < 0) {
				warnx("%s is invalid: %s", fields[index].name,
				    optarg);
				return (1);
			}
			if (switchval == MUTE) {
				if (fields[index].val == 1)
					fields[index].val = AUDIO_MUTE;
				else
					fields[index].val = AUDIO_UNMUTE;
			}
			break;
		default:
			warnx("internal error: do_ioctls: set");
			break;
		}
		if (ioctl(tuner_fd, fields[index].io_set,&fields[index].val)<0){
			warn("%s", fields[index].name);
			return (1);
		}
	} else {
		/* nothing is being set, so the -q option is meaningless */
		print_value = 1;
	}

	if (print_value == 1)
		if (prfield(index) > 0)
			return (1);

	return (0);
}


int
main(int argc, char *argv[])
{
	char 	*device = DEFAULT_TUNER_DEVICE;
	int	 aflag = 0;
	int	 err = 0;
	int	 ch, i;

	print_choices = 0;
	print_name = 1;
	print_value = 1;

	while ((ch = getopt(argc, argv, "af:nqv")) != -1) {
		switch (ch) {
		case 'a':
			aflag++;
			break;
		case 'f':
			device = optarg;
			break;
		case 'n':
			print_name = 0;
			break;
		case 'q':
			print_value = 0;
			break;
		case 'v':
			print_choices = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((argc == 0) && (aflag == 0))
		usage();

	if ((tuner_fd = open(device, O_RDONLY)) < 0) {
		warn("%s", device);
		close(tuner_fd);
		exit (1);
	}

	if (aflag > 0) {
		for (i = 0; fields[i].name; i++) {
			if (do_ioctls(i, NULL) > 0) {
				err++;
				break;
			}
		}
	} else {
		for (; argc--; argv++) {
			char *q;

			q = strchr(*argv, '=');
			i = findfield(*argv);
			if (i < 0) {
				warnx("field '%s' does not exist", *argv);
				err++;
				break;
			} else {
				if (q != NULL)
					*q++ = 0;
				if (do_ioctls(i, q) > 0) {
					err++;
					break;
				}
			}
		}
	}
	close(tuner_fd);
	exit (err);
}
