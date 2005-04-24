/*	$OpenBSD: ztsscale.c,v 1.1 2005/04/24 18:46:47 uwe Exp $	*/

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
#include <sys/sysctl.h>
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

#define WIDTH 640
#define HEIGHT 480
#define BLACK 0x0
#define WHITE 0xffff

#define ADDR(x,y) (HEIGHT*(x)+(y))

unsigned short *mapaddr, *save;
int fd;
int xc[] = { 25, 25, 320, 615, 615 };
int yc[] = { 24, 455, 240, 25, 455 };

struct ctlname topname[] = CTL_NAMES;
struct ctlname machdepname[] = CTL_MACHDEP_NAMES;

void
cross(unsigned short *fb, int x, int y)
{
	int i;

	y = 480 - y;
	for (i = x - 20; i <= x + 20; i++) 
		fb[ADDR(i,y)] = BLACK;
	for (i = y - 20; i <= y + 20; i++) 
		fb[ADDR(x, i)] = BLACK;
}

void
wait_event(int mfd, int *x, int *y)
{
	int down;
	size_t len;
	struct wscons_event evbuf;

	down = 0;
	*x = *y = -1;
	while (!down || *x == -1 || *y == -1 ) {
		len = read(mfd, &evbuf, sizeof(evbuf));
		if (len != 16)
			break;
		switch (evbuf.type) {
		case WSCONS_EVENT_MOUSE_DOWN:
			down = 1;
			break;
		case WSCONS_EVENT_MOUSE_ABSOLUTE_X:
			*x = evbuf.value;
			break;
		case WSCONS_EVENT_MOUSE_ABSOLUTE_Y:
			*y = evbuf.value;
			break;
		}
	}
	while (down) {
		len = read(mfd, &evbuf, sizeof(evbuf));
		if (len != 16)
			break;
		switch (evbuf.type) {
		case WSCONS_EVENT_MOUSE_UP:
			down = 0;
			break;
		}
	}
}	

void
save_screen(void)
{
	int mode = WSDISPLAYIO_MODE_DUMBFB;

	if (ioctl(fd, WSDISPLAYIO_SMODE, &mode) == -1) {
		warn("ioctl SMODE\n");
	}
	mapaddr = (void *)mmap(0, WIDTH*HEIGHT*sizeof(short), 
		PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (mapaddr == (void *)-1) {
		err(2, "mmap");
	}
	save = (unsigned short *)malloc(WIDTH*HEIGHT*sizeof(unsigned short));
	if (save == NULL) 
		err(2, "malloc");
	memcpy(save, mapaddr, WIDTH*HEIGHT*sizeof(unsigned short));
}

void
restore_screen(void)
{
	int mode = WSDISPLAYIO_MODE_EMUL;

	memcpy(mapaddr, save, WIDTH*HEIGHT*sizeof(unsigned short));
	if (ioctl(fd, WSDISPLAYIO_SMODE, &mode) == -1) {
		warn("ioctl SMODE");
	}
}

void
sighandler(int sig)
{
	restore_screen();
	close(fd);
	_exit(2);
}

int
main(int argc, char *argv[])
{
	int mfd;
	int i, x[5], y[5];
	double a, a1, a2, b, b1, b2, errx, erry;
	int mib[2];
	int rawmode;
	int oldval;
	size_t oldsize;
	struct ztsscale {
		int ts_minx;
		int ts_maxx;
		int ts_miny;
		int ts_maxy;
	} ts;
	
	fd = open("/dev/ttyC0", O_RDWR);
	if (fd < 0) {
		err(2, "open /dev/ttyC0");
	}
	mfd = open("/dev/wsmouse", O_RDONLY);
	if (mfd < 0) 
		err(2, "open /dev/wsmouse");

	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_ZTSRAWMODE;
	rawmode = 1;
	oldsize = sizeof(oldval);
	if (sysctl(mib, 2, &oldval, &oldsize, &rawmode,
	    sizeof(rawmode)) == -1)
		err(1, "sysctl");
	
	save_screen();
	signal(SIGINT, sighandler);
	for (i = 0; i < 5; i++) {
		memset(mapaddr, WHITE, WIDTH*HEIGHT*sizeof(unsigned short));
		cross(mapaddr, xc[i], yc[i]);
		/* printf("waiting for event\n"); */
		wait_event(mfd, &x[i], &y[i]);
	}
	restore_screen();
	close(fd);

	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_ZTSRAWMODE;
	rawmode = oldval;
	oldsize = sizeof(oldval);
	if (sysctl(mib, 2, NULL, NULL, &rawmode, sizeof(rawmode)) == -1)
		err(1, "sysctl");

	bzero(&ts, sizeof(ts));

	a1 = (x[4] - x[0])/(xc[4] - xc[0]);
	b1 = x[0] - a1*xc[0];
	a2 = (x[3] - x[1])/(xc[3] - xc[1]);
	b2 = x[1] - a2*xc[1];
	a = (a1+a2)/2.0;
	b = (b1+b2)/2.0;
	errx =  a*WIDTH/2+b - x[2];
	if (fabs(errx) > (a*WIDTH+b)*.05) {
		fprintf(stderr, "X error (%.2f) too high, try again\n",
			fabs(errx)); 
		exit(2);
	}

	ts.ts_minx = (int)(b+0.5);
	ts.ts_maxx = (int)(a*WIDTH+b+0.5);

	a1 = (y[4] - y[0])/(yc[4] - yc[0]);
	b1 = y[0] - a1*yc[0];
	a2 = (y[3] - y[1])/(yc[3] - yc[1]);
	b2 = y[1] - a2*yc[1];
	a = (a1+a2)/2.0;
	b = (b1+b2)/2.0;
	erry = a*HEIGHT/2+b - y[2];
	if (fabs(erry) > (a*HEIGHT+b)*.05) {
		fprintf(stderr, "Y error (%.2f) too high, try again\n",
			fabs(erry));
		exit(2);
	}

	ts.ts_miny = (int)(b+0.5);
	ts.ts_maxy = (int)(a*HEIGHT+b+0.5);

	(void)printf("%s.%s=%d,%d,%d,%d\n", topname[CTL_MACHDEP].ctl_name,
	    machdepname[CPU_ZTSSCALE].ctl_name, ts.ts_minx, ts.ts_maxx,
	    ts.ts_miny, ts.ts_maxy);

	return 0;
}
