/*	$OpenBSD: fdformat.c,v 1.8 1998/08/13 05:36:56 deraadt Exp $	*/

/*
 * Copyright (C) 1992-1994 by Joerg Wunsch, Dresden
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * FreeBSD:
 * format a floppy disk
 * 
 * Added FD_GTYPE ioctl, verifying, proportional indicators.
 * Serge Vakulenko, vak@zebub.msk.su
 * Sat Dec 18 17:45:47 MSK 1993
 *
 * Final adaptation, change format/verify logic, add separate
 * format gap/interleave values
 * Andrew A. Chernov, ache@astral.msk.su
 * Thu Jan 27 00:47:24 MSK 1994
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <util.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <machine/ioctl_fd.h>

extern const char *__progname;

static void
format_track(fd, cyl, secs, head, rate, gaplen, secsize, fill, interleave)
	int fd, cyl, secs, head, rate, gaplen, secsize;
	int fill, interleave;
{
	struct fd_formb f;
	register int i,j;
	int il[FD_MAX_NSEC + 1];

	memset(il,0,sizeof il);
	for(j = 0, i = 1; i <= secs; i++) {
		while(il[(j%secs)+1])
			j++;
		il[(j%secs)+1] = i;
		j += interleave;
        }

	f.format_version = FD_FORMAT_VERSION;
	f.head = head;
	f.cyl = cyl;
	f.transfer_rate = rate;

	f.fd_formb_secshift = secsize;
	f.fd_formb_nsecs = secs;
	f.fd_formb_gaplen = gaplen;
	f.fd_formb_fillbyte = fill;
	for(i = 0; i < secs; i++) {
		f.fd_formb_cylno(i) = cyl;
		f.fd_formb_headno(i) = head;
		f.fd_formb_secno(i) = il[i+1];
		f.fd_formb_secsize(i) = secsize;
	}
	if (ioctl(fd, FD_FORM, (caddr_t)&f) < 0)
		err(1, "FD_FORM");
}

static int
verify_track(fd, track, tracksize)
	int fd, track, tracksize;
{
	static char *buf = 0;
	static int bufsz = 0;
	int fdopts = -1, ofdopts, rv = 0;

	if (ioctl(fd, FD_GOPTS, &fdopts) < 0)
		warn("FD_GOPTS");
	else {
		ofdopts = fdopts;
		fdopts |= FDOPT_NORETRY;
		(void)ioctl(fd, FD_SOPTS, &fdopts);
	}
	
	if (bufsz < tracksize) {
		if (buf)
			free (buf);
		bufsz = tracksize;
		buf = 0;
	}
	if (! buf)
		buf = malloc (bufsz);
	if (! buf) {
		fprintf (stderr, "\nfdformat: out of memory\n");
		exit (2);
	}
	if (lseek (fd, (off_t) track*tracksize, 0) < 0)
		rv = -1;
	/* try twice reading it, without using the normal retrier */
	else if (read (fd, buf, tracksize) != tracksize
		 && read (fd, buf, tracksize) != tracksize)
		rv = -1;
	if (fdopts != -1)
		(void)ioctl(fd, FD_SOPTS, &ofdopts);
	return (rv);
}

static void
usage ()
{
	printf("Usage:\n\t%s [-q] [-n | -v] [-c #] [-s #] [-h #]\n",
		__progname);
	printf("\t\t [-r #] [-g #] [-i #] [-S #] [-F #] [-t #] devname\n");
	printf("Options:\n");
	printf("\t-q\tsupress any normal output, don't ask for confirmation\n");
	printf("\t-n\tdon't verify floppy after formatting\n");
	printf("\t-v\tdon't format, verify only\n");
	printf("\t\tvalid choices are 360, 720, 800, 820, 1200, 1440, 1480, 1720\n");
	printf("\tdevname\tthe full name of floppy device or in short form fd0, fd1\n");
	printf("Obscure options:\n");
	printf("\t-c #\tspecify number of cylinders, 40 or 80\n");
	printf("\t-s #\tspecify number of sectors per track, 9, 10, 15 or 18\n");
	printf("\t-h #\tspecify number of floppy heads, 1 or 2\n");
	printf("\t-r #\tspecify data rate, 250, 300 or 500 kbps\n");
	printf("\t-g #\tspecify gap length\n");
	printf("\t-i #\tspecify interleave factor\n");
	printf("\t-S #\tspecify sector size, 0=128, 1=256, 2=512 bytes\n");
	printf("\t-F #\tspecify fill byte\n");
	printf("\t-t #\tnumber of steps per track\n");
	exit(2);
}

static int
yes ()
{
	char reply [256], *p;

	reply[sizeof(reply)-1] = 0;
	for (;;) {
		fflush(stdout);
		if (! fgets (reply, sizeof(reply)-1, stdin))
			return (0);
		for (p=reply; *p==' ' || *p=='\t'; ++p)
			continue;
		if (*p=='y' || *p=='Y')
			return (1);
		if (*p=='n' || *p=='N' || *p=='\n' || *p=='\r')
			return (0);
		printf("Answer `yes' or `no': ");
	}
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int cyls = -1, secs = -1, heads = -1, intleave = -1;
	int rate = -1, gaplen = -1, secsize = -1, steps = -1;
	int fill = 0xf6, quiet = 0, verify = 1, verify_only = 0;
	int fd, c, track, error, tracks_per_dot, bytes_per_track, errs;
	char *devname;
	struct fd_type fdt;

	while((c = getopt(argc, argv, "c:s:h:r:g:S:F:t:i:qvn")) != -1)
		switch (c) {
		case 'c':       /* # of cyls */
			cyls = atoi(optarg);
			break;

		case 's':       /* # of secs per track */
			secs = atoi(optarg);
			break;

		case 'h':       /* # of heads */
			heads = atoi(optarg);
			break;

		case 'r':       /* transfer rate, kilobyte/sec */
			rate = atoi(optarg);
			break;

		case 'g':       /* length of GAP3 to format with */
			gaplen = atoi(optarg);
			break;

		case 'S':       /* sector size shift factor (1 << S)*128 */
			secsize = atoi(optarg);
			break;

		case 'F':       /* fill byte, C-like notation allowed */
			fill = (int)strtol(optarg, (char **)0, 0);
			break;

		case 't':       /* steps per track */
			steps = atoi(optarg);
			break;

		case 'i':       /* interleave factor */
			intleave = atoi(optarg);
			break;

		case 'q':
			quiet = 1;
			break;

		case 'n':
			verify = 0;
			break;

		case 'v':
			verify = 1;
			verify_only = 1;
			break;

		case '?': default:
			usage();
		}

	if (optind != argc - 1)
		usage();

	if ((fd = opendev(argv[optind], O_RDWR, OPENDEV_PART, &devname)) < 0)
		err(1, devname);

	if (ioctl(fd, FD_GTYPE, &fdt) < 0)
		errx(1, "not a floppy disk: %s", devname);

	switch (rate) {
	case -1:
		break;
	case 250:
		fdt.rate = FDC_250KBPS;
		break;
	case 300:
		fdt.rate = FDC_300KBPS;
		break;
	case 500:
		fdt.rate = FDC_500KBPS;
		break;
	default:
		errx(1, "invalid transfer rate: %d", rate);
	}

	if (cyls >= 0)
		fdt.tracks = cyls;
	if (secs >= 0)
		fdt.sectrac = secs;
	if (fdt.sectrac > FD_MAX_NSEC)
		errx(1, "too many sectors per track, max value is %d",
			FD_MAX_NSEC);
	if (heads >= 0)
		fdt.heads = heads;
	if (gaplen >= 0)
		fdt.gap2 = gaplen;
	if (secsize >= 0)
		fdt.secsize = secsize;
	if (steps >= 0)
		fdt.step = steps;

	bytes_per_track = fdt.sectrac * (1<<fdt.secsize) * 128;
	tracks_per_dot = fdt.tracks * fdt.heads / 40;

	if (verify_only) {
		if (!quiet)
			printf("Verify %dK floppy `%s'.\n",
				fdt.tracks * fdt.heads * bytes_per_track / 1024,
				devname);
	}
	else if (!quiet) {
		printf("Format %dK floppy `%s'? (y/n): ",
			fdt.tracks * fdt.heads * bytes_per_track / 1024,
			devname);
		if (!yes()) {
			printf("Not confirmed.\n");
			exit(0);
		}
	}

	/*
	 * Formatting.
	 */
	if (!quiet) {
		printf("Processing ");
		for (track = 0; track < fdt.tracks * fdt.heads; track++) {
			if (!((track + 1) % tracks_per_dot))
				putchar('-');
		}
		putchar('\r');
		printf("Processing ");
		fflush(stdout);
	}

	error = errs = 0;

	for (track = 0; track < fdt.tracks * fdt.heads; track++) {
		if (!verify_only) {
			format_track(fd, track / fdt.heads, fdt.sectrac,
				track % fdt.heads, fdt.rate, fdt.gap2,
				     fdt.secsize, fill,
				     intleave >= 0 ? intleave : 1);
			if (!quiet && !((track + 1) % tracks_per_dot)) {
				putchar('F');
				fflush(stdout);
			}
		}
		if (verify) {
			if (verify_track(fd, track, bytes_per_track) < 0)
				error = errs = 1;
			if (!quiet && !((track + 1) % tracks_per_dot)) {
				if (!verify_only)
					putchar('\b');
				if (error) {
					putchar('E');
					error = 0;
				}
				else
					putchar('V');
				fflush(stdout);
			}
		}
	}
	if (!quiet)
		printf(" done.\n");

	exit(errs);
}
