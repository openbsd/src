/*	$OpenBSD: midicat.c,v 1.7 2022/12/02 22:36:34 cheloha Exp $	*/
/*
 * Copyright (c) 2015 Alexandre Ratchov <alex@caoua.org>
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
#include <err.h>
#include <fcntl.h>
#include <sndio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void __dead usage(void);

int
main(int argc, char **argv)
{
#define MIDI_BUFSZ	1024
	unsigned char buf[MIDI_BUFSZ];
	struct mio_hdl *ih, *oh;
	char *port0, *port1, *ifile, *ofile;
	int ifd, ofd;
	int dump, c, i, len, n, sep, mode;

	dump = 0;
	port0 = port1 = ifile = ofile = NULL;
	ih = oh = NULL;
	ifd = ofd = -1;

	while ((c = getopt(argc, argv, "di:o:q:")) != -1) {
		switch (c) {
		case 'd':
			dump = 1;
			break;
		case 'q':
			if (port0 == NULL)
				port0 = optarg;
			else if (port1 == NULL)
				port1 = optarg;
			else
				errx(1, "too many -q options");
			break;
		case 'i':
			ifile = optarg;
			break;
		case 'o':
			ofile = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	/* we don't support more than one data flow */
	if (ifile != NULL && ofile != NULL)
		errx(1, "-i and -o are exclusive");

	/* second port makes sense only for port-to-port transfers */
	if (port1 != NULL && !(ifile == NULL && ofile == NULL))
		errx(1, "too many -q options");

	/* if there're neither files nor ports, then we've nothing to do */
	if (port0 == NULL && ifile == NULL && ofile == NULL)
		usage();

	/* if no port specified, use default one */
	if (port0 == NULL)
		port0 = MIO_PORTANY;

	/* open input or output file (if any) */
	if (ifile) {
		if (strcmp(ifile, "-") == 0) {
			ifile = "stdin";
			ifd = STDIN_FILENO;
		} else {
			ifd = open(ifile, O_RDONLY);
			if (ifd == -1)
				err(1, "%s", ifile);
		}
	} else if (ofile) {
		if (strcmp(ofile, "-") == 0) {
			ofile = "stdout";
			ofd = STDOUT_FILENO;
		} else {
			ofd = open(ofile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (ofd == -1)
				err(1, "%s", ofile);
		}
	}

	/* open first port for input and output (if output needed) */
	if (ofile)
		mode = MIO_IN;
	else if (ifile)
		mode = MIO_OUT;
	else if (port1 == NULL)
		mode = MIO_IN | MIO_OUT;
	else
		mode = MIO_IN;
	ih = mio_open(port0, mode, 0);
	if (ih == NULL)
		errx(1, "%s: couldn't open port", port0);

	/* open second port, output only */
	if (port1 == NULL)
		oh = ih;
	else {
		oh = mio_open(port1, MIO_OUT, 0);
		if (oh == NULL)
			errx(1, "%s: couldn't open port", port1);
	}

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	/* transfer until end-of-file or error */
	for (;;) {
		if (ifile != NULL) {
			len = read(ifd, buf, sizeof(buf));
			if (len == 0)
				break;
			if (len == -1) {
				warn("%s", ifile);
				break;
			}
		} else {
			len = mio_read(ih, buf, sizeof(buf));
			if (len == 0) {
				warnx("%s: disconnected", port0);
				break;
			}
		}
		if (ofile != NULL) {
			n = write(ofd, buf, len);
			if (n != len) {
				warn("%s: short write", ofile);
				break;
			}
		} else {
			n = mio_write(oh, buf, len);
			if (n != len) {
				warnx("%s: disconnected", port1);
				break;
			}
		}
		if (dump) {
			for (i = 0; i < len; i++) {
				sep = (i % 16 == 15 || i == len - 1) ?
				    '\n' : ' ';
				fprintf(stderr, "%02x%c", buf[i], sep);
			}
		}
	}

	/* clean-up */
	if (port0)
		mio_close(ih);
	if (port1)
		mio_close(oh);
	if (ifile)
		close(ifd);
	if (ofile)
		close(ofd);
	return 0;
}

void __dead
usage(void)
{
	fprintf(stderr, "usage: midicat [-d] [-i in-file] [-o out-file] "
	    "[-q in-port] [-q out-port]\n");
	exit(1);
}
