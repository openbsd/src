/* $OpenBSD: wsfontload.c,v 1.14 2015/02/09 23:00:15 deraadt Exp $ */
/* $NetBSD: wsfontload.c,v 1.2 2000/01/05 18:46:43 ad Exp $ */

/*
 * Copyright (c) 1999
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
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
 *
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <err.h>

#include <dev/wscons/wsconsio.h>

#define DEFDEV		"/dev/ttyCcfg"
#define DEFENC		WSDISPLAY_FONTENC_ISO
#define DEFBITORDER	WSDISPLAY_FONTORDER_L2R
#define DEFBYTEORDER	WSDISPLAY_FONTORDER_L2R

int main(int, char**);
static void usage(void);
static int getencoding(char *);

static void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-Bbl] [-e encoding] [-f file] [-h height] [-N name]\n"
	    "       %*s [-w width] [fontfile]\n",
	    __progname, (int)strlen(__progname), "");
	exit(1);
}

static const struct {
	const char *name;
	int val;
} encodings[] = {
	{"iso",  WSDISPLAY_FONTENC_ISO},
	{"ibm",  WSDISPLAY_FONTENC_IBM},
#if 0
	{"pcvt", WSDISPLAY_FONTENC_PCVT},
	{"iso7", WSDISPLAY_FONTENC_ISO7},
#endif
};

int
main(int argc, char *argv[])
{
	char *wsdev, *infile, *p;
	struct wsdisplay_font f;
	int c, res, wsfd, ffd, type, list, i;
	int defwidth, defheight;
	struct stat stat;
	size_t len;
	void *buf;

	wsdev = DEFDEV;
	memset(&f, 0, sizeof f);
	f.firstchar = f.numchars = -1;
	f.encoding = -1;

	list = 0;
	while ((c = getopt(argc, argv, "bB:e:f:h:lN:w:")) != -1) {
		switch (c) {
		case 'f':
			wsdev = optarg;
			break;
		case 'w':
			if (sscanf(optarg, "%d", &f.fontwidth) != 1)
				errx(1, "invalid font width of %d",
				    f.fontwidth);
			break;
		case 'h':
			if (sscanf(optarg, "%d", &f.fontheight) != 1)
				errx(1, "invalid font height of %d",
				    f.fontheight);
			break;
		case 'e':
			f.encoding = getencoding(optarg);
			break;
		case 'l':
			list = 1;
			break;
		case 'N':
			strlcpy(f.name, optarg, WSFONT_NAME_SIZE);
			break;
		case 'b':
			f.bitorder = WSDISPLAY_FONTORDER_R2L;
			break;
		case 'B':
			f.byteorder = WSDISPLAY_FONTORDER_R2L;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (list && argc)
		usage();

	if (argc > 1)
		usage();

	wsfd = open(wsdev, O_RDWR, 0);
	if (wsfd < 0)
		err(2, "open %s", wsdev);

	if (list) {
		i = 0;
		p = " # Name             Encoding  W  H";
		do {
			f.index = i;
			res = ioctl(wsfd, WSDISPLAYIO_LSFONT, &f);
			if (res == 0) {
				if (f.name[0]) {
					if (p) {
						puts(p);
						p = NULL;
					}
					printf("%2d %-16s %8s %2d %2d\n",
					    f.index, f.name,
					    encodings[f.encoding].name,
					    f.fontwidth, f.fontheight);
				}
			}
			i++;
		} while(res == 0);

		return (0);
	}

	if (argc > 0) {
		infile = argv[0];
		ffd = open(infile, O_RDONLY, 0);
		if (ffd < 0)
			err(4, "open %s", infile);
		if (!*f.name)
			strlcpy(f.name, infile, WSFONT_NAME_SIZE);
	} else {
		infile = "stdin";
		ffd = STDIN_FILENO;
	}

	res = ioctl(wsfd, WSDISPLAYIO_GTYPE, &type);
	if (res != 0)
		type = WSDISPLAY_TYPE_UNKNOWN;

	switch (type) {
	/* text-mode VGA */
	case WSDISPLAY_TYPE_ISAVGA:
	case WSDISPLAY_TYPE_PCIVGA:
		defwidth = 8;
		defheight = 16;
		break;
	/* raster frame buffers */
	default:
		/* XXX ought to be computed from the frame buffer resolution */
		defwidth = 12;
		defheight = 22;
		break;
	}

	f.index = -1;
	if (f.fontwidth == 0)
		f.fontwidth = defwidth;
	if (f.fontheight == 0)
		f.fontheight = defheight;
	if (f.stride == 0)
		f.stride = (f.fontwidth + 7) / 8;
	if (f.encoding < 0)
		f.encoding = DEFENC;
	if (f.bitorder == 0)
		f.bitorder = DEFBITORDER;
	if (f.byteorder == 0)
		f.byteorder = DEFBYTEORDER;

	if (f.firstchar < 0)
		f.firstchar = 0;

	if (f.numchars < 0) {
		f.numchars = 256;
		if (argc > 0) {
			if (fstat(ffd, &stat) == 0)
				f.numchars = stat.st_size /
				    f.stride / f.fontheight;
		}
	}

	len = f.fontheight * f.numchars * f.stride;
	if (!len)
		errx(1, "invalid font size");

	buf = malloc(len);
	if (!buf)
		errx(1, "malloc");
	res = read(ffd, buf, len);
	if (res < 0)
		err(4, "read %s", infile);
	if (res != len)
		errx(4, "short read on %s", infile);

	f.data = buf;

	res = ioctl(wsfd, WSDISPLAYIO_LDFONT, &f);
	if (res < 0)
		err(3, "WSDISPLAYIO_LDFONT");

	return (0);
}

static int
getencoding(char *name)
{
	int i;

	for (i = 0; i < sizeof(encodings) / sizeof(encodings[0]); i++)
		if (!strcmp(name, encodings[i].name))
			return (encodings[i].val);

	if (sscanf(name, "%d", &i) != 1)
		errx(1, "invalid encoding");
	return (i);
}
