/* $Id: scio.c,v 1.12 2003/04/02 22:57:51 deraadt Exp $ */

/*
copyright 1997
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works 
and redistribute this software and such derivative works 
for any purpose, so long as the name of the university of 
michigan is not used in any advertising or publicity 
pertaining to the use or distribution of this software 
without specific, written prior authorization.  if the 
above copyright notice or any other identification of the 
university of michigan is included in any copy of any 
portion of this software, then the disclaimer below must 
also be included.

this software is provided as is, without representation 
from the university of michigan as to its fitness for any 
purpose, and without warranty by the university of 
michigan of any kind, either express or implied, including 
without limitation the implied warranties of 
merchantability and fitness for a particular purpose. the 
regents of the university of michigan shall not be liable 
for any damages, including special, indirect, incidental, or 
consequential damages, with respect to any claim arising 
out of or in connection with the use of the software, even 
if it has been or is hereafter advised of the possibility of 
such damages.
*/

/*
 * OS dependent part, Unix version
 *
 * Jim Rees, University of Michigan, October 1997
 */

#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef _AIX
#include <sys/select.h>
#endif /* _AIX */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "sectok.h"
#include "sc7816.h"
#include "todos_scrw.h"

#ifndef howmany
#define howmany(x, y) (((x) + ((y) - 1)) / (y))
#endif

static int todos_scfdopen(int ttyn, int fd, int flags, int *ep);
static int todos_sccts(int ttyn);

#ifdef __linux
static char ttynametmpl[] = "/dev/ttyS%01d";
#elif _AIX
static char ttynametmpl[] = "/dev/tty%01d";
#elif __OpenBSD__
static char ttynametmpl[] = "/dev/cua%02d";
#elif __sun
/*static char ttynametmpl[] = "/dev/cua/%c";*/
static char ttynametmpl[] = "/dev/tty%c";
#else
static char ttynametmpl[] = "/dev/tty%02d";
#endif

static struct {
    int fd, flags;
    pid_t pid;
    struct termios tio0, tio1;
} sc[4];

/* global variable */
#ifdef BYTECOUNT
int num_getc, num_putc;
#endif /* BYTECOUNT */

int
todos_scopen(int ttyn, int flags, int *ep)
{
    char ttyname[32];
    int fd, i, oflags;
    pid_t pid;

#ifdef BYTECOUNT
    num_getc = 0;
    num_putc = 0;
#endif /* BYTECOUNT */

#ifdef __sun
    snprintf(ttyname, sizeof ttyname, ttynametmpl, 'a' + ttyn);
#else
    snprintf(ttyname, sizeof ttyname, ttynametmpl, ttyn);
#endif

    
#ifdef DEBUG
    printf ("ttyname=%s\n", ttyname);
#endif /* DEBUG */
    oflags = O_RDWR;
    if (!(flags & SCODCD))
	oflags |= O_NONBLOCK;
    if ((fd = open(ttyname, oflags, 0)) < 0) {
	if (ep)
	    *ep = SCENOTTY;
	return -1;
    }

    if ((ttyn = todos_scfdopen(ttyn, fd, flags, ep)) < 0) {
	close(fd);
	return -1;
    }

    /* Figure out which reader we have */
    if (ioctl(fd, TIOCMGET, &i) < 0) {
	close(fd);
	if (ep)
	    *ep = SCENOTTY;
	return -1;
    }
#ifndef __sun
    /* Todos has RTS wired to RI, so set RTS and see if RI goes high */
    i |= TIOCM_RTS;
    ioctl(fd, TIOCMSET, &i);
    ioctl(fd, TIOCMGET, &i);
#else
    /* Sun serial port is broken, has no RI line */
    i = TIOCM_RI;
#endif
    if (i & TIOCM_RI) {
	/* Todos reader */
	scsleep(20);
	sc[ttyn].flags |= (SCOXCTS | SCOXDTR);
    }

    if (flags & SCODSR) {
	/* Wait for card present */
	while (!todos_sccardpresent(ttyn)) {
	    errno = 0;
	    sleep(1);
	    if (errno == EINTR)
		return -1;
	}
    }

    if (flags & SCOHUP) {
	/* spawn a process to wait for card removal */
	pid = fork();
	if (pid == 0) {
	    /* See if the card is still there */
	    while (todos_sccardpresent(ttyn))
		sleep(1);
	    kill(getppid(), SIGHUP);
	    exit(0);
	}
	sc[ttyn].pid = pid;
    }

    return ttyn;
}

int
todos_scsetflags(int ttyn, int flags, int mask)
{
    int oflags = sc[ttyn].flags;

    sc[ttyn].flags &= ~mask;
    sc[ttyn].flags |= (flags & mask);

    if ((sc[ttyn].flags & SCOINVRT) != (oflags & SCOINVRT)) {
	if (sc[ttyn].flags & SCOINVRT)
	    sc[ttyn].tio1.c_cflag |= PARODD;
	else
	    sc[ttyn].tio1.c_cflag &= ~PARODD;
	tcsetattr(sc[ttyn].fd, TCSADRAIN, &sc[ttyn].tio1);
    }

    return oflags;
}

/* NI: for Linux */
#if (B9600 != 9600)
struct speed_trans {
    speed_t t_val;
    int val;
    char *s;
} speed_trans[] = {
    {0, 0, "a"},
    {B9600, 9600, "a"},
    {B19200, 19200, "a"},
    {B38400, 38400, "a"},
    {B57600, 55928, "a"},
    {B57600, 57600, "a"},
    {B115200, 115200, "a"},
    {-1, -1, NULL}};
#endif /* (B9600 != 9600) */

int
todos_scsetspeed(int ttyn, int speed)
{

#if (B9600 == 9600)
    /* On OpenBSD, B9600 == 9600, and we can use the input argument
       "speed" of this function as an argument to cfset[io]speed(). */ 
#else
    /* On Linux, B9600 != 9600, and we have to translate the input argument
       to speed_t. */
    int i;

    for (i = 0; speed_trans[i].s; i++) 
	if (speed_trans[i].val == speed) break;
    if (speed_trans[i].s == NULL) {
	fprintf (stderr, "scsetspeed() failed : speed %d not supported.  ignore ...\n",
		 speed);
	return 0;
    }
    speed = speed_trans[i].t_val;
#endif    
    cfsetispeed(&sc[ttyn].tio1, speed);
    cfsetospeed(&sc[ttyn].tio1, speed);
    
    return tcsetattr(sc[ttyn].fd, TCSADRAIN, &sc[ttyn].tio1);
}

static int
todos_scfdopen(int ttyn, int fd, int flags, int *ep)
{
    struct termios t;

    /* Get and save the tty state */

    if (tcgetattr(fd, &t) < 0) {
	if (ep)
	    *ep = SCENOTTY;
	return -1;
    }
    sc[ttyn].fd = fd;
    sc[ttyn].tio0 = t;
    sc[ttyn].flags = flags;

    /* Now put the tty in a happy ISO state */

    /* 9600 bps */
    cfsetispeed(&t, B9600);
    cfsetospeed(&t, B9600);

    /* raw 8/E/2 */
    t.c_iflag &= ~(ISTRIP|INLCR|IGNCR|ICRNL|IXON);
    t.c_iflag |= (IGNBRK|IGNPAR);
    t.c_oflag &= ~OPOST;
    t.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
#ifdef CHWFLOW
    t.c_cflag &= ~CHWFLOW;
#endif
#ifdef CRTSCTS
    t.c_cflag &= ~CRTSCTS;
#endif
#ifdef CRTSXOFF
    t.c_cflag &= ~CRTSXOFF;
#endif
    t.c_cflag |= CLOCAL;

    /* 8/E/2 */
    t.c_cflag &= ~(CSIZE | PARODD);
    t.c_cflag |= (CS8 | PARENB | CSTOPB);

    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &t) < 0) {
	if (ep)
	    *ep = SCENOTTY;
	sc[ttyn].fd = -1;
	return -1;
    }
    sc[ttyn].tio1 = t;

    /* The open may or may not have reset the card.  Wait a while then flush
       anything that came in on the port. */
    scsleep(250);
    tcflush(sc[ttyn].fd, TCIFLUSH);

    return ttyn;
}

/* find out if there is a card in the reader */

int
todos_sccardpresent(int ttyn)
{
    return (sc[ttyn].flags & SCOXCTS) ? !todos_sccts(ttyn) : todos_scdsr(ttyn);
}

/* query dsr on the port (usually indicates whether the card is present) */

int
todos_scdsr(int ttyn)
{
    int fd = sc[ttyn].fd;
    int i;

    if (fd < 0 || ioctl(fd, TIOCMGET, &i) < 0)
	return 0;

    return ((i & TIOCM_DSR) ? 1 : 0);
}

static int
todos_sccts(int ttyn)
{
    int fd = sc[ttyn].fd;
    int i;

    if (fd < 0 || ioctl(fd, TIOCMGET, &i) < 0)
	return 0;

    return ((i & TIOCM_CTS) ? 1 : 0);
}

/* raise or lower dtr */

int
todos_scdtr(int ttyn, int cmd)
{
    int fd = sc[ttyn].fd;
    int i;

    if (!todos_sccardpresent(ttyn))
	return -1;

    if (ioctl(fd, TIOCMGET, &i) < 0)
	return -1;

    if (cmd)
	i |= TIOCM_DTR;
    else
	i &= ~TIOCM_DTR;
    i |= TIOCM_RTS;

    return ioctl(fd, TIOCMSET, &i);
}

int
todos_scclose(int ttyn)
{
    int fd = sc[ttyn].fd;

    tcsetattr(fd, TCSANOW, &sc[ttyn].tio0);
    close(fd);
    sc[ttyn].fd = -1;
    if (sc[ttyn].pid) {
	kill(sc[ttyn].pid, SIGTERM);
	sc[ttyn].pid = 0;
    }

#ifdef BYTECOUNT
    printf("#getc=%d, #putc=%d\n", num_getc - num_putc, num_putc);
#endif /* BYTECOUNT */    
    return 0;
}

/*
 * get one byte from the card.
 * wait at most ms msec.  0 for poll, -1 for infinite.
 * return byte in *cp.
 * return 0 or error.
 */

int
scgetc(int ttyn, unsigned char *cp, int ms)
{
    int fd = sc[ttyn].fd;
    fd_set *fdset;
    struct timeval tv, *tvp;

#ifdef BYTECOUNT
    num_getc++;
#endif /* BYTECOUNT */

    fdset = (fd_set *)calloc(howmany(fd + 1, NFDBITS), sizeof(fd_mask));
    if (fdset == NULL)
	return SCENOMEM;
    FD_SET(fd, fdset);

    if (ms == -1)
	tvp = NULL;
    else {
	tv.tv_sec = (ms + 1) / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	tvp = &tv;
    }

    if (select(fd + 1, fdset, NULL, NULL, tvp) != 1) {
	free(fdset);
	return SCTIMEO;
    }

    if (read(fd, cp, 1) != 1) {
	free(fdset);
	return SCTIMEO;
    }

    if (sc[ttyn].flags & SCOINVRT)
	*cp = todos_scinvert[*cp];

    free(fdset);
    return SCEOK; /* 0 */
}

/* write one byte to the card */

int
scputc(int ttyn, int ic)
{
    int fd = sc[ttyn].fd;
    unsigned char c0, c1;
    int code;

#ifdef BYTECOUNT
    num_putc++;
#endif /* BYTECOUNT */

    c0 = (sc[ttyn].flags & SCOINVRT) ? todos_scinvert[ic] : ic;
    write(fd, &c0, 1);

    /* gobble up the echo */
    code = scgetc(ttyn, &c1, 200);
#ifdef GOBBLEDEBUG
    if (sc[ttyn].flags & SCOINVRT)
	c1 = todos_scinvert[c1];
    if (code)
	printf("failed to gobble\n");
    else if (c0 != c1)
	printf("misgobbled %x != %x\n", c0, c1);
    else
	printf("gobble gobble %x\n", c0);
#endif
    return code;
}

int
scputblk(int ttyn, unsigned char *bp, int n)
{
    int fd = sc[ttyn].fd;
    unsigned char c;

    write(fd, bp, n);
    while (n--)
	scgetc(ttyn, &c, 30);

    return SCEOK;
}

void
scsleep(int ms)
{
    struct timeval tv;

    if (!ms)
	return;
    tv.tv_sec = (ms + 1) / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    select(0, NULL, NULL, NULL, &tv);
}
