/* $OpenBSD: wsfontload.c,v 1.1 2000/07/02 01:29:44 mickey Exp $ */
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <err.h>

#include <dev/wscons/wsconsio.h>

#define DEFDEV		"/dev/ttyEcfg"
#define DEFWIDTH	8
#define DEFHEIGHT	16
#define DEFENC		WSDISPLAY_FONTENC_ISO
#define DEFBITORDER	WSDISPLAY_FONTORDER_L2R
#define DEFBYTEORDER	WSDISPLAY_FONTORDER_L2R

int main __P((int, char**));
static void usage __P((void));
static int getencoding __P((char *));

static void
usage()
{
	extern char *__progname;

	(void)fprintf(stderr,
		"Usage: %s [-f wsdev] [-w width] [-h height] [-e encoding]"
		" [-N name] [-b] [-B] [fontfile]\n",
		      __progname);
	exit(1);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *wsdev;
	struct wsdisplay_font f;
	int c, res, wsfd, ffd;
	size_t len;
	void *buf;

	wsdev = DEFDEV;
	f.fontwidth = DEFWIDTH;
	f.fontheight = DEFHEIGHT;
	f.firstchar = 0;
	f.numchars = 256;
	f.stride = 0;
	f.encoding = DEFENC;
	f.name = 0;
	f.bitorder = DEFBITORDER;
	f.byteorder = DEFBYTEORDER;

	while ((c = getopt(argc, argv, "f:w:h:e:N:bB")) != -1) {
		switch (c) {
		case 'f':
			wsdev = optarg;
			break;
		case 'w':
			if (sscanf(optarg, "%d", &f.fontwidth) != 1)
				errx(1, "invalid font width");
			break;
		case 'h':
			if (sscanf(optarg, "%d", &f.fontheight) != 1)
				errx(1, "invalid font height");
			break;
		case 'e':
			f.encoding = getencoding(optarg);
			break;
		case 'N':
			f.name = optarg;
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

	if (argc > 1)
		usage();

	wsfd = open(wsdev, O_RDWR, 0);
	if (wsfd < 0)
		err(2, "open ws");

	if (argc > 0) {
		ffd = open(argv[0], O_RDONLY, 0);
		if (ffd < 0)
			err(4, "open font");
		if (!f.name)
			f.name = argv[0];
	} else
		ffd = 0;

	if (!f.stride)
		f.stride = (f.fontwidth + 7) / 8;
	len = f.fontheight * f.numchars * f.stride;
	if (!len)
		errx(1, "invalid font size");

	buf = malloc(len);
	if (!buf)
		errx(1, "malloc");
	res = read(ffd, buf, len);
	if (res < 0)
		err(4, "read font");
	if (res != len)
		errx(4, "short read");

	f.data = buf;

	res = ioctl(wsfd, WSDISPLAYIO_LDFONT, &f);
	if (res < 0)
		err(3, "WSDISPLAYIO_LDFONT");

	return (0);
}

static struct {
	char *name;
	int val;
} encodings[] = {
	{"iso", WSDISPLAY_FONTENC_ISO},
	{"ibm", WSDISPLAY_FONTENC_IBM},
	{"pcvt", WSDISPLAY_FONTENC_PCVT},
};

static int
getencoding(name)
	char *name;
{
	int i;

	for (i = 0; i < sizeof(encodings) / sizeof(encodings[0]); i++)
		if (!strcmp(name, encodings[i].name))
			return (encodings[i].val);

	if (sscanf(name, "%d", &i) != 1)
		errx(1, "invalid encoding");
	return (i);
}
