/*	$OpenBSD: ztsscale.c,v 1.14 2007/06/17 10:07:30 robert Exp $	*/

/*
 * Copyright (c) 2005 Matthieu Herrb
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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <machine/cpu.h>
#include <dev/wscons/wsconsio.h>

#include <err.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "message.xbm"

#define TITLE_Y		64
#define MESSAGE_Y	128

#define WIDTH	640
#define HEIGHT	480
#define BLACK	0x0
#define RED	0xf000
#define WHITE	0xffff

#define ADDR(x,y) (HEIGHT*(x)+(y))

u_short		*mapaddr, *save;
int		 orawmode = -1;
int		 fd, mfd;
int		 xc[] = { 25, 25, 320, 615, 615 };
int		 yc[] = { 25, 455, 240, 25, 455 };

struct wsmouse_calibcoords wmcoords;

void		bitmap(u_short *, u_short, u_char[], int, int, int);
void		cross(u_short *, int, int);
void		wait_event(int, int *, int *);
void		save_screen(void);
void		restore_screen(void);
void		cleanup(void);
void		sighandler(int);
int		main(int, char *[]);
__dead void	usage(void);

void
bitmap(u_short *fb, u_short pixel, u_char bits[], int width, int height,
    int y)
{
	int i, j;
	int x;

#define BITADDR(x, y)	((width + 7)/8*(y) + (x)/8)
#define BITMASK(x)	(1 << ((x) % 8))

	for (i = 0; i < height; i++) {
		x = (WIDTH - width)/2;
		for (j = 0; j < width; j++)
			if (bits[BITADDR(j, i)] & BITMASK(j))
				fb[ADDR(x + j, HEIGHT - y - i)] = pixel;
	}
}

void
cross(u_short *fb, int x, int y)
{
	int i;

	y = HEIGHT - y;
	for (i = x - 20; i <= x + 20; i++)
		fb[ADDR(i, y)] = BLACK;
	for (i = y - 20; i <= y + 20; i++)
		fb[ADDR(x, i)] = BLACK;
}

void
wait_event(int mfd, int *x, int *y)
{
	int down;
	ssize_t len;
	struct wscons_event evbuf;

	down = 0;
	*x = *y = -1;
	while (down || *x == -1 || *y == -1) {
		len = read(mfd, &evbuf, sizeof(evbuf));
		if (len != 16)
			break;
		switch (evbuf.type) {
		case WSCONS_EVENT_MOUSE_DOWN:
			down = 1;
			break;
		case WSCONS_EVENT_MOUSE_UP:
			down = 0;
			break;
		case WSCONS_EVENT_MOUSE_ABSOLUTE_X:
			if (down)
				*x = evbuf.value;
			break;
		case WSCONS_EVENT_MOUSE_ABSOLUTE_Y:
			if (down)
				*y = evbuf.value;
			break;
		}
	}
}

void
save_screen(void)
{
	int mode = WSDISPLAYIO_MODE_DUMBFB;

	if (ioctl(fd, WSDISPLAYIO_SMODE, &mode) == -1)
		warn("ioctl SMODE");
	mapaddr = (void *)mmap(0, WIDTH*HEIGHT*sizeof(short),
	    PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)0);
	if (mapaddr == (void *)-1)
		err(2, "mmap");
	save = (u_short *)malloc(WIDTH*HEIGHT*sizeof(u_short));
	if (save == NULL)
		err(2, "malloc");
	memcpy(save, mapaddr, WIDTH*HEIGHT*sizeof(u_short));
}

void
restore_screen(void)
{
	int mode = WSDISPLAYIO_MODE_EMUL;

	memcpy(mapaddr, save, WIDTH*HEIGHT*sizeof(u_short));
	if (ioctl(fd, WSDISPLAYIO_SMODE, &mode) == -1)
		warn("ioctl SMODE");
}

void
cleanup(void)
{

	restore_screen();

	wmcoords.samplelen = orawmode;
	if (wmcoords.samplelen != -1 && ioctl(mfd, WSMOUSEIO_SCALIBCOORDS, &wmcoords) < 0)
		err(1, "WSMOUSEIO_SCALIBCOORDS");

	close(mfd);
}

/* ARGSUSED */
void
sighandler(int sig)
{

	cleanup();
	_exit(2);
}

int
main(int argc, char *argv[])
{
	int i, x[5], y[5];
	double a, a1, a2, b, b1, b2, xerr, yerr;
	size_t oldsize;
	struct ztsscale {
		int ts_minx;
		int ts_maxx;
		int ts_miny;
		int ts_maxy;
		int ts_swapxy;
	} ts;

	if (argc != 1)
		usage();

	fd = open("/dev/ttyC0", O_RDWR);
	if (fd < 0)
		err(2, "open /dev/ttyC0");
	save_screen();

	mfd = open("/dev/wsmouse", O_RDWR);
	if (mfd < 0) {
		restore_screen();
		err(2, "open /dev/wsmouse");
	}

	if (ioctl(mfd, WSMOUSEIO_GCALIBCOORDS, &wmcoords) < 0) {
		restore_screen();
                err(1, "WSMOUSEIO_GCALIBCOORDS");
	}

	/* Save the old rawmode value then switch rawmode on */ 
	orawmode = wmcoords.samplelen;
	wmcoords.samplelen = 1;

	if (ioctl(mfd, WSMOUSEIO_SCALIBCOORDS, &wmcoords) < 0) {
		restore_screen();
		err(1, "WSMOUSEIO_SCALIBCOORDS");
	}

again:
	signal(SIGINT, sighandler);
	for (i = 0; i < 5; i++) {
		memset(mapaddr, WHITE, WIDTH*HEIGHT*sizeof(u_short));
		bitmap(mapaddr, BLACK, title_bits, title_width,
		    title_height, TITLE_Y);
		bitmap(mapaddr, BLACK, message_bits, message_width,
		    message_height, MESSAGE_Y);
		cross(mapaddr, xc[i], yc[i]);
		/* printf("waiting for event\n"); */
		wait_event(mfd, &x[i], &y[i]);
	}

	bzero(&ts, sizeof(ts));

	/* get touch pad resolution to screen resolution ratio */
	a1 = (double)(x[4] - x[0])/(double)(xc[4] - xc[0]);
	a2 = (double)(x[3] - x[1])/(double)(xc[3] - xc[1]);
	/* get the minimum pad position on the X-axis */
	b1 = x[0] - a1*xc[0];
	b2 = x[1] - a2*xc[1];
	/* use the average ratio and average minimum position */
	a = (a1+a2)/2.0;
	b = (b1+b2)/2.0;
	xerr = a*WIDTH/2+b - x[2];
	if (fabs(xerr) > (a*WIDTH+b)*.01) {
#ifdef DEBUG
		fprintf(stderr, "X error (%.2f) too high, try again\n",
		    fabs(xerr));
#endif
		goto err;
	}

	ts.ts_minx = (int)(b+0.5);
	ts.ts_maxx = (int)(a*WIDTH+b+0.5);

	/* get touch pad resolution to screen resolution ratio */
	a1 = (double)(y[4] - y[0])/(double)(yc[4] - yc[0]);
	a2 = (double)(y[3] - y[1])/(double)(yc[3] - yc[1]);
	/* get the minimum pad position on the Y-axis */
	b1 = y[0] - a1*yc[0];
	b2 = y[1] - a2*yc[1];
	/* use the average ratio and average minimum position */
	a = (a1+a2)/2.0;
	b = (b1+b2)/2.0;
	yerr = a*HEIGHT/2+b - y[2];
	if (fabs(yerr) > (a*HEIGHT+b)*.01) {
#ifdef DEBUG
		fprintf(stderr, "Y error (%.2f) too high, try again\n",
		    fabs(yerr));
#endif
		goto err;
	}

	ts.ts_miny = (int)(b+0.5);
	ts.ts_maxy = (int)(a*HEIGHT+b+0.5);

	cleanup();

	(void)printf("mouse.scale=%d,%d,%d,%d,%d,%d,%d\n", ts.ts_minx, ts.ts_maxx,
	    ts.ts_miny, ts.ts_maxy, ts.ts_swapxy, WIDTH, HEIGHT);
	return 0;

err:
	memset(mapaddr, WHITE, WIDTH*HEIGHT*sizeof(u_short));
	bitmap(mapaddr, BLACK, title_bits, title_width, title_height,
	    TITLE_Y);
	bitmap(mapaddr, RED, error_bits, error_width, error_height,
	    MESSAGE_Y);
	sleep(2);
	goto again;
}

__dead void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s\n", __progname);
	exit(2);
}
