/*
 * Copyright (c) 2000 Jean-Baptiste Marchand, Julien Montagne and Jerome Verdon
 * 
 * Copyright (c) 1998 by Kazutaka Yokota
 *
 * Copyright (c) 1995 Michael Smith
 * 
 * Copyright (c) 1993 by David Dawes <dawes@xfree86.org>
 *
 * Copyright (c) 1990,91 by Thomas Roell, Dinkelscherben, Germany.
 *
 * All rights reserved.
 *
 * Most of this code was taken from the FreeBSD moused daemon, written by
 * Michael Smith. The FreeBSD moused daemon already contained code from the 
 * Xfree Project, written by David Dawes and Thomas Roell and Kazutaka Yokota.
 *
 * Adaptation to OpenBSD was done by Jean-Baptiste Marchand, Julien Montagne
 * and Jerome Verdon.
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
 *	This product includes software developed by
 *      David Dawes, Jean-Baptiste Marchand, Julien Montagne, Thomas Roell,
 *      Michael Smith, Jerome Verdon and Kazutaka Yokota.
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/tty.h>
#include <machine/pcvt_ioctl.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <setjmp.h>
#include <signal.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <varargs.h>

#include "moused.h"

extern char *optarg;
extern int optind;
extern char *__progname;

int debug = 0;
int nodaemon = FALSE;
int background = FALSE;
int identify = ID_NONE;
char *pidfile = "/var/run/moused.pid";
static sigjmp_buf env;
static jmp_buf restart_env;

/*
 * Most of the structures are from the Xfree Project
 */

/* Buttons status (for multiple click detection) */

static struct {
    int count;		/* 0: up, 1: single click, 2: double click,... */
    struct timeval tv;	/* timestamp on the last `up' event */
} buttonstate[MOUSE_MAXBUTTON];

/* Mouse physical interfaces */

static symtab_t mouse_ifs[] = {
    { "serial",		MOUSE_IF_SERIAL },
    { "bus",		MOUSE_IF_BUS },
    { "inport",		MOUSE_IF_INPORT },
    { "ps/2",		MOUSE_IF_PS2 },
    { "usb",		MOUSE_IF_USB },
    { NULL,		MOUSE_IF_UNKNOWN }
};

/* Mouse model names */

static symtab_t	mouse_models[] = {
    { "NetScroll",	MOUSE_MODEL_NETSCROLL },
    { "NetMouse",	MOUSE_MODEL_NET },
    { "GlidePoint",	MOUSE_MODEL_GLIDEPOINT },
    { "ThinkingMouse",	MOUSE_MODEL_THINK },
    { "IntelliMouse",	MOUSE_MODEL_INTELLI },
    { "EasyScroll",	MOUSE_MODEL_EASYSCROLL },
    { "MouseMan+",	MOUSE_MODEL_MOUSEMANPLUS },
    { "Kidspad",	MOUSE_MODEL_KIDSPAD },
    { "VersaPad",	MOUSE_MODEL_VERSAPAD },
    { "generic",	MOUSE_MODEL_GENERIC },
    { NULL, 		MOUSE_MODEL_UNKNOWN },
};

/* 
 *  Cflags of each mouse protocol, ordered by P_XXX
 */

static unsigned short mousecflags[] =
{
    (CS7	           | CREAD | CLOCAL | HUPCL ),	/* MicroSoft */
    (CS8 | CSTOPB	   | CREAD | CLOCAL | HUPCL ),	/* MouseSystems */
    (CS8 | CSTOPB	   | CREAD | CLOCAL | HUPCL ),	/* Logitech */
    (CS8 | PARENB | PARODD | CREAD | CLOCAL | HUPCL ),	/* MMSeries */
    (CS7		   | CREAD | CLOCAL | HUPCL ),	/* MouseMan */
    0,							/* Bus */
    0,							/* InPort */
    0,							/* PS/2 */
    (CS8		   | CREAD | CLOCAL | HUPCL ),	/* MM HitTablet */
    (CS7	           | CREAD | CLOCAL | HUPCL ),	/* GlidePoint */
    (CS7                   | CREAD | CLOCAL | HUPCL ),	/* IntelliMouse */
    (CS7                   | CREAD | CLOCAL | HUPCL ),	/* Thinking Mouse */
};

/* Array ordered by P_XXX giving protocol properties */

static unsigned char proto[][7] = {
    /*  hd_mask hd_id   dp_mask dp_id   bytes b4_mask b4_id */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x23,  0x00 }, /* MicroSoft */
    {	0xf8,	0x80,	0x00,	0x00,	5,    0x00,  0xff }, /* MouseSystems */
    {	0xe0,	0x80,	0x80,	0x00,	3,    0x00,  0xff }, /* Logitech */
    {	0xe0,	0x80,	0x80,	0x00,	3,    0x00,  0xff }, /* MMSeries */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x33,  0x00 }, /* MouseMan */
    {	0xf8,	0x80,	0x00,	0x00,	5,    0x00,  0xff }, /* Bus */
    {	0xf8,	0x80,	0x00,	0x00,	5,    0x00,  0xff }, /* InPort */
    {	0xc0,	0x00,	0x00,	0x00,	3,    0x00,  0xff }, /* PS/2 mouse */
    {	0xe0,	0x80,	0x80,	0x00,	3,    0x00,  0xff }, /* MM HitTablet */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x33,  0x00 }, /* GlidePoint */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x3f,  0x00 }, /* IntelliMouse */
    { 	0x40,	0x40,	0x40,	0x00,	3,   ~0x33,  0x00 }, /* ThinkingMouse */
};

/* 
 * array ordered by P_XXX (mouse protocols) giving the protocol corresponding
 * to the name of a mouse 
 */

static char *mouse_names[] = {
    "microsoft",
    "mousesystems",
    "logitech",
    "mmseries",
    "mouseman",
    "busmouse",
    "inportmouse",
    "ps/2",
    "mmhitab",
    "glidepoint",
    "intellimouse",
    "thinkingmouse",
    NULL
};

/* protocol currently used */

static unsigned char cur_proto[7];

mouse_t mouse = {
    flags : 0, 
    portname : NULL,
    proto : P_UNKNOWN,
    baudrate : 1200, 
    old_baudrate : 1200,
    rate : MOUSE_RATE_UNKNOWN,
    resolution : MOUSE_RES_UNKNOWN, 
    zmap: 0,
    wmode: 0,
    mfd : -1,
    clickthreshold : 500,	/* 0.5 sec */
};

/* PnP EISA/product IDs */
static symtab_t pnpprod[] = {
    { "KML0001",P_THINKING,MOUSE_MODEL_THINK},	/* Kensignton ThinkingMouse */
    { "MSH0001",P_IMSERIAL,MOUSE_MODEL_INTELLI},/* MS IntelliMouse */
    { "MSH0004",P_IMSERIAL,MOUSE_MODEL_INTELLI},/* MS IntelliMouse TrackBall */
    { "KYEEZ00",P_MS,MOUSE_MODEL_GENERIC}, /* Genius EZScroll */
    { "KYE0001",P_MS,MOUSE_MODEL_GENERIC}, /* Genius PnP Mouse */
    { "KYE0003",P_IMSERIAL,MOUSE_MODEL_NET},/* Genius NetMouse */
    { "LGI800C",P_IMSERIAL,MOUSE_MODEL_MOUSEMANPLUS}, /* Logitech MouseMan (4 button model) */
    { "LGI8050",P_IMSERIAL,MOUSE_MODEL_MOUSEMANPLUS}, /* Logitech MouseMan+ */
    { "LGI8051",P_IMSERIAL,MOUSE_MODEL_MOUSEMANPLUS}, /* Logitech FirstMouse+ */
    { "LGI8001",P_LOGIMAN,MOUSE_MODEL_GENERIC},	/* Logitech serial */

    { "PNP0F00",P_BM,MOUSE_MODEL_GENERIC },	/* MS bus */
    { "PNP0F01",P_MS,MOUSE_MODEL_GENERIC },	/* MS serial */
    { "PNP0F02",P_BM,MOUSE_MODEL_GENERIC },	/* MS InPort */
    { "PNP0F03",P_PS2,MOUSE_MODEL_GENERIC },	/* MS PS/2 */
    /*
     * EzScroll returns PNP0F04 in the compatible device field; but it
     * doesn't look compatible... XXX
     */
    { "PNP0F04",P_MSC,MOUSE_MODEL_GENERIC },	/* MouseSystems */ 
    { "PNP0F05",P_MSC,MOUSE_MODEL_GENERIC },	/* MouseSystems */ 
    { "PNP0F08",P_LOGIMAN,MOUSE_MODEL_GENERIC },/* Logitech serial */
    { "PNP0F09",P_MS ,MOUSE_MODEL_GENERIC },	/* MS BallPoint serial */
    { "PNP0F0A",P_MS ,MOUSE_MODEL_GENERIC },	/* MS PnP serial */
    { "PNP0F0B",P_MS ,MOUSE_MODEL_GENERIC },	/* MS PnP BallPoint serial */
    { "PNP0F0C",P_MS ,MOUSE_MODEL_GENERIC },	/* MS serial comatible */
    { "PNP0F0D",P_BM,MOUSE_MODEL_GENERIC  },	/* MS InPort comatible */
    { "PNP0F0E",P_PS2,MOUSE_MODEL_GENERIC  },	/* MS PS/2 comatible */
    { "PNP0F0F",P_MS,MOUSE_MODEL_GENERIC  },	/* MS BallPoint comatible */
    { "PNP0F11",P_BM,MOUSE_MODEL_GENERIC },	/* MS bus comatible */
    { "PNP0F12",P_PS2,MOUSE_MODEL_GENERIC  },	/* Logitech PS/2 */
    { "PNP0F13",P_PS2,MOUSE_MODEL_GENERIC  },	/* PS/2 */
    { "PNP0F15",P_BM,MOUSE_MODEL_GENERIC  },	/* Logitech bus */ 
    { "PNP0F17",P_LOGIMAN,MOUSE_MODEL_GENERIC  },/* Logitech serial compat */
    { "PNP0F18",P_BM,MOUSE_MODEL_GENERIC  },	/* Logitech bus compatible */
    { "PNP0F19",P_PS2,MOUSE_MODEL_GENERIC  },	/* Logitech PS/2 compatible */
    { NULL,		-1 },
};

/*
 * XXX Functions
 */

static char *
skipspace(char *s)
{
    while(isspace(*s))
	++s;
    return s;
}

static char *
gettokenname(symtab_t *tab, int val)
{
    int i;

    for (i = 0; tab[i].name != NULL; ++i) {
	if (tab[i].val == val)
	    return tab[i].name;
    }
    return NULL;
}

static symtab_t *
gettoken(symtab_t *tab, char *s, int len)
{
    int i;

    for (i = 0; tab[i].name != NULL; ++i) {
	if (strncmp(tab[i].name, s, len) == 0)
	    break;
    }
    return &tab[i];
}

static char *
mouse_name(int type)
{
    return ((type == P_UNKNOWN) 
	|| (type > sizeof(mouse_names)/sizeof(mouse_names[0]) - 1))
	? "unknown" : mouse_names[type];
}

static char *
mouse_model(int model)
{
    char *s;

    s = gettokenname(mouse_models, model);
    return (s == NULL) ? "unknown" : s;
}

/* Fills the hardware info in the main structure */

static void
mouse_fill_hwinfo(void)
{
	/* default settings */

	mouse.hw.iftype = MOUSE_IF_UNKNOWN;
	mouse.hw.type = MOUSE_TYPE_MOUSE;	
	if (mouse.hw.model == 0) { /* If no type has been given */
		mouse.hw.model = MOUSE_MODEL_GENERIC;
	}

	if (strcmp(mouse.portname,PMS_DEV) == 0) {
		mouse.hw.iftype = MOUSE_IF_PS2;
	}

	if (strcmp(mouse.portname,LMS_DEV) == 0) {
		mouse.hw.iftype = MOUSE_IF_BUS;
	}

	if (strcmp(mouse.portname,MMS_DEV) == 0) {
		mouse.hw.iftype = MOUSE_IF_INPORT;
	}

	/* serial device begins with /dev/cua0 (9 caracters) */
	if (strncmp(mouse.portname,SERIAL_DEV,9) == 0) {
		mouse.hw.iftype = MOUSE_IF_SERIAL;
	}

}

/* Fills the mode structure in the main structure */

static void 
mouse_fill_mousemode(void)
{
	/* default settings */
	
	mouse.mode.protocol = P_UNKNOWN; 
	mouse.mode.accelfactor = 0; /* no accel */

	if (strcmp(mouse.portname,PMS_DEV) == 0) {
		mouse.mode.protocol = P_PS2;
		if (mouse.hw.model == MOUSE_MODEL_INTELLI)
			mouse.mode.packetsize = MOUSE_INTELLI_PACKETSIZE;
		else
			mouse.mode.packetsize = MOUSE_PS2_PACKETSIZE;
		mouse.mode.syncmask[0] = MOUSE_PS2_SYNCMASK;
		mouse.mode.syncmask[1] = MOUSE_PS2_SYNC;
	}
			
	if (strcmp(mouse.portname,LMS_DEV) == 0) {
		mouse.mode.protocol = P_BM;
		mouse.mode.packetsize = MOUSE_MSC_PACKETSIZE;
		mouse.mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
		mouse.mode.syncmask[1] = MOUSE_MSC_SYNC;
	}

	if (strcmp(mouse.portname,MMS_DEV) == 0) {
		mouse.hw.iftype = P_INPORT;
		mouse.mode.packetsize = MOUSE_MSC_PACKETSIZE;
		mouse.mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
		mouse.mode.syncmask[1] = MOUSE_MSC_SYNC;
	}
	if (mouse.proto != -1)
		mouse.mode.protocol = mouse.proto;
	
	/* resolution */
	
	switch (mouse.resolution) {
	case MOUSE_RES_HIGH:
	case MOUSE_RES_MEDIUMHIGH:
	case MOUSE_RES_MEDIUMLOW:
	case MOUSE_RES_LOW:
		mouse.mode.resolution = mouse.resolution;
		break;
	case MOUSE_RES_DEFAULT:
	case MOUSE_RES_UNKNOWN:
		/* default to low resolution */
		mouse.mode.resolution = MOUSE_RES_LOW;
		break;
	default:
		if (mouse.resolution >= 200)
			mouse.mode.resolution = MOUSE_RES_HIGH;
		else if (mouse.resolution >= 100)
			mouse.mode.resolution = MOUSE_RES_MEDIUMHIGH;
		else if (mouse.resolution >= 50)
			mouse.mode.resolution = MOUSE_RES_MEDIUMLOW;
		else mouse.mode.resolution = MOUSE_RES_LOW;
	}
		
	/* sample rate */

	if (mouse.rate != MOUSE_RATE_UNKNOWN) {
		if (mouse.rate >= 200)
			mouse.mode.rate = MOUSE_RATE_VERY_HIGH;
		else if (mouse.rate >= 100)
			mouse.mode.rate = MOUSE_RATE_HIGH;
		else if (mouse.rate >= 80)
			mouse.mode.rate = MOUSE_RATE_MEDIUM_HIGH;
		else if (mouse.rate >= 60)
			mouse.mode.rate = MOUSE_RATE_MEDIUM_LOW;
		else if (mouse.rate >= 40)
			mouse.mode.rate = MOUSE_RATE_LOW;
		else 
			mouse.mode.rate = MOUSE_RATE_VERY_LOW;
	}
}


static void
freedev(int sig)
{ 
  int save_errno = errno;

  /* 
   *  close the device, when a USR1 signal is received.
   *  Tipically used by an X server so it can open the device for it's 
   *  own purpose. 
   */
  close(mouse.mfd);
  mouse.mfd = -1;
  sigpause(0);
  errno = save_errno;
}

static void mouse_init(void);

static void
opendev(int sig)
{
	/* re-open the mouse device */
	if ((mouse.mfd = open(mouse.portname, O_RDWR | O_NONBLOCK, 0)) == -1) {
		logerr(1, "unable to open %s", mouse.portname);	/* XXX race */
		_exit(1);
	}
	/* re-init the mouse */
	mouse_init();
	longjmp(restart_env, 1);	/* XXX signal/longjmp re-entrancy */
}

static void 
cleanup(int sig)
{
    char moused_flag = MOUSED_OFF;
  
    ioctl(mouse.cfd, PCVT_MOUSED, &moused_flag);
    _exit(0);
}

/*
 * Begin of functions from the Xfree Project
 */

/* 
 * Functions below come from the Xfree Project and are derived from a two files
 * of Xfree86 3.3.6 with the following CVS tags :
 $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86_Mouse.c,v 3.21.2.24 
 1999/12/11 19:00:42 hohndel Exp $ 
 and
 $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86_PnPMouse.c,v 1.1.2.6 
 1999/07/29 09:22:51 hohndel Exp $ 
 */

void SetMouseSpeed(int old, int new, unsigned int cflag)
{
	struct termios tty;
	char *c;

	if (mouse.hw.iftype != MOUSE_IF_SERIAL)
		return;

	if (tcgetattr(mouse.mfd, &tty) < 0)
	{
		ErrorF("Warning: %s unable to get status of mouse fd (%s)\n",
		       mouse.portname, strerror(errno));
		return;
	}

	/* this will query the initial baudrate only once */
	if (mouse.old_baudrate < 0) { 
	   switch (cfgetispeed(&tty)) 
	      {
	      case B9600: 
		 mouse.old_baudrate = 9600;
		 break;
	      case B4800: 
		 mouse.old_baudrate = 4800;
		 break;
	      case B2400: 
		 mouse.old_baudrate = 2400;
		 break;
	      case B1200: 
	      default:
		 mouse.old_baudrate = 1200;
		 break;
	      }
	}

	tty.c_iflag = IGNBRK | IGNPAR;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tty.c_cflag = (tcflag_t)cflag;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;

	switch (old)
	{
	case 9600:
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

	if (tcsetattr(mouse.mfd, TCSADRAIN, &tty) < 0) {
		printf("Unable to get mouse status. Exiting...\n");
		exit(1);
	}
	
	switch (new)
	{
	case 9600:
		c = "*q";
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		c = "*p";
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		c = "*o";
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		c = "*n";
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

	if (mouse.proto == P_LOGIMAN || mouse.proto == P_LOGI)
	{
		if (write(mouse.mfd, c, 2) != 2) {
			printf("Unable to write to mouse. Exiting...\n");
			exit(1);
		}
	}
	usleep(100000);

	if (tcsetattr(mouse.mfd, TCSADRAIN, &tty) < 0) {
		printf("Unable to get mouse status. Exiting...\n");
		exit(1);
	}
}

int
FlushInput(int fd)
{
	struct pollfd pfd[1];
	char c[4];

	if (tcflush(fd, TCIFLUSH) == 0)
		return 0;

	pfd[0].fd = fd;
	pfd[0].events = POLLIN;

	while (poll(pfd, 1, 0) > 0)
		read(fd, &c, sizeof(c));
	return 0;
}

/*
 * Try to elicit a PnP ID as described in 
 * Microsoft, Hayes: "Plug and Play External COM Device Specification, 
 * rev 1.00", 1995.
 *
 * The routine does not fully implement the COM Enumerator as par Section
 * 2.1 of the document.  In particular, we don't have idle state in which
 * the driver software monitors the com port for dynamic connection or 
 * removal of a device at the port, because `moused' simply quits if no 
 * device is found.
 *
 * In addition, as PnP COM device enumeration procedure slightly has 
 * changed since its first publication, devices which follow earlier
 * revisions of the above spec. may fail to respond if the rev 1.0 
 * procedure is used. XXX
 */

static int
pnpgets(int mouse_fd, char *buf)
{
    struct timeval timeout;
    struct pollfd pfd[1];
    int i;
    char c;

    pfd[0].fd = mouse_fd;
    pfd[0].events = POLLIN;

#if 0
    /* 
     * This is the procedure described in rev 1.0 of PnP COM device spec.
     * Unfortunately, some devices which comform to earlier revisions of
     * the spec gets confused and do not return the ID string...
     */

    /* port initialization (2.1.2) */
    ioctl(mouse_fd, TIOCMGET, &i);
    i |= TIOCM_DTR;		/* DTR = 1 */
    i &= ~TIOCM_RTS;		/* RTS = 0 */
    ioctl(mouse_fd, TIOCMSET, &i);
    usleep(200000);
    if ((ioctl(mouse_fd, TIOCMGET, &i) == -1) || ((i & TIOCM_DSR) == 0))
	goto disconnect_idle;

    /* port setup, 1st phase (2.1.3) */
    SetMouseSpeed(1200, 1200, (CS7 | CREAD | CLOCAL | HUPCL));
    i = TIOCM_DTR | TIOCM_RTS;	/* DTR = 0, RTS = 0 */
    ioctl(mouse_fd, TIOCMBIC, &i);
    usleep(200000);
    i = TIOCM_DTR;		/* DTR = 1, RTS = 0 */
    ioctl(mouse_fd, TIOCMBIS, &i);
    usleep(200000);

    /* wait for response, 1st phase (2.1.4) */
    FlushInput(mouse_fd);
    i = TIOCM_RTS;		/* DTR = 1, RTS = 1 */
    ioctl(mouse_fd, TIOCMBIS, &i);

    /* try to read something */
    if (poll(pfd, 1, 200000/1000) <= 0) {

	/* port setup, 2nd phase (2.1.5) */
        i = TIOCM_DTR | TIOCM_RTS;	/* DTR = 0, RTS = 0 */
        ioctl(mouse_fd, TIOCMBIC, &i);
        usleep(200000);

	/* wait for respose, 2nd phase (2.1.6) */
	FlushInput(mouse_fd);
        i = TIOCM_DTR | TIOCM_RTS;	/* DTR = 1, RTS = 1 */
        ioctl(mouse_fd, TIOCMBIS, &i);

        /* try to read something */
        if (poll(pfd, 1, 200000/1000) <= 0)
	    goto connect_idle;
    }
#else
    
    /*
     * This is a simplified procedure; it simply toggles RTS.
     */
    SetMouseSpeed(1200, 1200, (CS7 | CREAD | CLOCAL | HUPCL));

    ioctl(mouse_fd, TIOCMGET, &i);
    i |= TIOCM_DTR;		/* DTR = 1 */
    i &= ~TIOCM_RTS;		/* RTS = 0 */
    ioctl(mouse_fd, TIOCMSET, &i);
    usleep(200000);

    /* wait for response */
    FlushInput(mouse_fd);
    i = TIOCM_DTR | TIOCM_RTS;	/* DTR = 1, RTS = 1 */
    ioctl(mouse_fd, TIOCMBIS, &i);

    /* try to read something */
    if (poll(pfd, 1, 200000/1000) <= 0)
        goto connect_idle;
#endif

    /* collect PnP COM device ID (2.1.7) */
    i = 0;
    usleep(200000);	/* the mouse must send `Begin ID' within 200msec */
    while (read(mouse_fd, &c, 1) == 1) {
	/* we may see "M", or "M3..." before `Begin ID' */
        if ((c == 0x08) || (c == 0x28)) {	/* Begin ID */
	    buf[i++] = c;
	    break;
        }
    }
    if (i <= 0) {
	/* we haven't seen `Begin ID' in time... */
	goto connect_idle;
    }

    ++c;			/* make it `End ID' */
    for (;;) {
        if (poll(pfd, 1, 200000/1000) <= 0)
	    break;

	read(mouse_fd, &buf[i], 1);
        if (buf[i++] == c)	/* End ID */
	    break;
	if (i >= 256)
	    break;
    }
    if (buf[i - 1] != c)
	goto connect_idle;
    return i;
    
#if 0
    /*
     * According to PnP spec, we should set DTR = 1 and RTS = 0 while 
     * in idle state.  But, `moused' shall set DTR = RTS = 1 and proceed, 
     * assuming there is something at the port even if it didn't 
     * respond to the PnP enumeration procedure.
     */
disconnect_idle:
    i = TIOCM_DTR | TIOCM_RTS;		/* DTR = 1, RTS = 1 */
    ioctl(mouse_fd, TIOCMBIS, &i);
#endif

connect_idle:
    return 0;
}

/*
 * pnpparse : parse a PnP string ID
 */

static int
pnpparse(pnpid_t *id, char *buf, int len)
{
    char s[3];
    int offset;
    int sum = 0;
    int i, j;

    id->revision = 0;
    id->eisaid = NULL;
    id->serial = NULL;
    id->class = NULL;
    id->compat = NULL;
    id->description = NULL;
    id->neisaid = 0;
    id->nserial = 0;
    id->nclass = 0;
    id->ncompat = 0;
    id->ndescription = 0;

    offset = 0x28 - buf[0];

    /* calculate checksum */
    for (i = 0; i < len - 3; ++i) {
	sum += buf[i];
	buf[i] += offset;
    }
    sum += buf[len - 1];
    for (; i < len; ++i)
	buf[i] += offset;
    ErrorF("Mouse: PnP ID string: '%*.*s'\n", len, len, buf);

    /* revision */
    buf[1] -= offset;
    buf[2] -= offset;
    id->revision = ((buf[1] & 0x3f) << 6) | (buf[2] & 0x3f);
    ErrorF("Mouse: PnP rev %d.%02d\n", id->revision / 100, id->revision % 100);

    /* EISA vender and product ID */
    id->eisaid = &buf[3];
    id->neisaid = 7;

    /* option strings */
    i = 10;
    if (buf[i] == '\\') {
        /* device serial # */
        for (j = ++i; i < len; ++i) {
            if (buf[i] == '\\')
		break;
        }
	if (i >= len)
	    i -= 3;
	if (i - j == 8) {
            id->serial = &buf[j];
            id->nserial = 8;
	}
    }
    if (buf[i] == '\\') {
        /* PnP class */
        for (j = ++i; i < len; ++i) {
            if (buf[i] == '\\')
		break;
        }
	if (i >= len)
	    i -= 3;
	if (i > j + 1) {
            id->class = &buf[j];
            id->nclass = i - j;
        }
    }
    if (buf[i] == '\\') {
	/* compatible driver */
        for (j = ++i; i < len; ++i) {
            if (buf[i] == '\\')
		break;
        }
	/*
	 * PnP COM spec prior to v0.96 allowed '*' in this field, 
	 * it's not allowed now; just ignore it.
	 */
	if (buf[j] == '*')
	    ++j;
	if (i >= len)
	    i -= 3;
	if (i > j + 1) {
            id->compat = &buf[j];
            id->ncompat = i - j;
        }
    }
    if (buf[i] == '\\') {
	/* product description */
        for (j = ++i; i < len; ++i) {
            if (buf[i] == ';')
		break;
        }
	if (i >= len)
	    i -= 3;
	if (i > j + 1) {
            id->description = &buf[j];
            id->ndescription = i - j;
        }
    }

    /* checksum exists if there are any optional fields */
    if ((id->nserial > 0) || (id->nclass > 0)
	|| (id->ncompat > 0) || (id->ndescription > 0)) {
#if 0
        ErrorF("Mouse: PnP checksum: 0x%02X\n", sum); 
#endif
        sprintf(s, "%02X", sum & 0x0ff);
        if (strncmp(s, &buf[len - 3], 2) != 0) {
#if 0
            /* 
	     * Checksum error!!
	     * I found some mice do not comply with the PnP COM device 
	     * spec regarding checksum... XXX
	     */
	    return FALSE;
#endif
        }
    }

    return 1;
}

/*
 * pnpproto : return the prototype used, based on the PnP ID string
 */

static symtab_t *
pnpproto(pnpid_t *id)
{
    symtab_t *t;
    int i, j;

    if (id->nclass > 0)
	if (strncmp(id->class, "MOUSE", id->nclass) != 0)
	    /* this is not a mouse! */
	    return NULL;

    if (id->neisaid > 0) {
        t = gettoken(pnpprod, id->eisaid, id->neisaid);
	if (t->val != -1)
            return t;
    }

    /*
     * The 'Compatible drivers' field may contain more than one
     * ID separated by ','.
     */
    if (id->ncompat <= 0)
	return NULL;
    for (i = 0; i < id->ncompat; ++i) {
        for (j = i; id->compat[i] != ','; ++i)
            if (i >= id->ncompat)
		break;
        if (i > j) {
            t = gettoken(pnpprod, id->compat + j, i - j);
	    if (t->val != -1)
                return t;
	}
    }

    return NULL;
}

/* mouse_if : returns a string giving the name of the physical interface */

static char *
mouse_if(int iftype)
{
    char *s;

    s = gettokenname(mouse_ifs, iftype);
    return (s == NULL) ? "unknown" : s;
}

/* mouse_init : inits the mouse by writing appropriate sequences */

static void
mouse_init(void)
{
    struct pollfd pfd[1];
    char *s;
    char c;
    int i;

    pfd[0].fd = mouse.mfd;
    pfd[0].events = POLLIN;

    /**
     ** This comment is a little out of context here, but it contains 
     ** some useful information...
     ********************************************************************
     **
     ** The following lines take care of the Logitech MouseMan protocols.
     **
     ** NOTE: There are diffrent versions of both MouseMan and TrackMan!
     **       Hence I add another protocol P_LOGIMAN, which the user can
     **       specify as MouseMan in his XF86Config file. This entry was
     **       formerly handled as a special case of P_MS. However, people
     **       who don't have the middle button problem, can still specify
     **       Microsoft and use P_MS.
     **
     ** By default, these mice should use a 3 byte Microsoft protocol
     ** plus a 4th byte for the middle button. However, the mouse might
     ** have switched to a different protocol before we use it, so I send
     ** the proper sequence just in case.
     **
     ** NOTE: - all commands to (at least the European) MouseMan have to
     **         be sent at 1200 Baud.
     **       - each command starts with a '*'.
     **       - whenever the MouseMan receives a '*', it will switch back
     **	 to 1200 Baud. Hence I have to select the desired protocol
     **	 first, then select the baud rate.
     **
     ** The protocols supported by the (European) MouseMan are:
     **   -  5 byte packed binary protocol, as with the Mouse Systems
     **      mouse. Selected by sequence "*U".
     **   -  2 button 3 byte MicroSoft compatible protocol. Selected
     **      by sequence "*V".
     **   -  3 button 3+1 byte MicroSoft compatible protocol (default).
     **      Selected by sequence "*X".
     **
     ** The following baud rates are supported:
     **   -  1200 Baud (default). Selected by sequence "*n".
     **   -  9600 Baud. Selected by sequence "*q".
     **
     ** Selecting a sample rate is no longer supported with the MouseMan!
     ** Some additional lines in xf86Config.c take care of ill configured
     ** baud rates and sample rates. (The user will get an error.)
     */

    switch (mouse.proto) {

    case P_LOGI:
	/* 
	 * The baud rate selection command must be sent at the current
	 * baud rate; try all likely settings 
	 */
	SetMouseSpeed(9600, mouse.baudrate, mousecflags[mouse.proto]);
	SetMouseSpeed(4800, mouse.baudrate, mousecflags[mouse.proto]);
	SetMouseSpeed(2400, mouse.baudrate, mousecflags[mouse.proto]);
	// SetMouseSpeed(1200, mouse.baudrate, mousecflags[mouse.proto]);
	/* select MM series data format */
	write(mouse.mfd, "S", 1);
	SetMouseSpeed(mouse.baudrate, mouse.baudrate,
		      mousecflags[P_MM]);
	/* select report rate/frequency */
	if      (mouse.rate <= 0)   write(mouse.mfd, "O", 1);
	else if (mouse.rate <= 15)  write(mouse.mfd, "J", 1);
	else if (mouse.rate <= 27)  write(mouse.mfd, "K", 1);
	else if (mouse.rate <= 42)  write(mouse.mfd, "L", 1);
	else if (mouse.rate <= 60)  write(mouse.mfd, "R", 1);
	else if (mouse.rate <= 85)  write(mouse.mfd, "M", 1);
	else if (mouse.rate <= 125) write(mouse.mfd, "Q", 1);
	else			     write(mouse.mfd, "N", 1);
	break;

    case P_LOGIMAN:
	/* The command must always be sent at 1200 baud */
	SetMouseSpeed(1200, 1200, mousecflags[mouse.proto]);
	write(mouse.mfd, "*X", 2);
	SetMouseSpeed(1200, mouse.baudrate, mousecflags[mouse.proto]);
	break;

    case P_MMHIT:
	SetMouseSpeed(1200, mouse.baudrate, mousecflags[mouse.proto]);

	/*
	 * Initialize Hitachi PUMA Plus - Model 1212E to desired settings.
	 * The tablet must be configured to be in MM mode, NO parity,
	 * Binary Format.  xf86Info.sampleRate controls the sensativity
	 * of the tablet.  We only use this tablet for it's 4-button puck
	 * so we don't run in "Absolute Mode"
	 */
	write(mouse.mfd, "z8", 2);	/* Set Parity = "NONE" */
	usleep(50000);
	write(mouse.mfd, "zb", 2);	/* Set Format = "Binary" */
	usleep(50000);
	write(mouse.mfd, "@", 1);	/* Set Report Mode = "Stream" */
	usleep(50000);
	write(mouse.mfd, "R", 1);	/* Set Output Rate = "45 rps" */
	usleep(50000);
	write(mouse.mfd, "I\x20", 2);	/* Set Incrememtal Mode "20" */
	usleep(50000);
	write(mouse.mfd, "E", 1);	/* Set Data Type = "Relative */
	usleep(50000);

	/* Resolution is in 'lines per inch' on the Hitachi tablet */
	if      (mouse.resolution == MOUSE_RES_LOW) 		c = 'g';
	else if (mouse.resolution == MOUSE_RES_MEDIUMLOW)	c = 'e';
	else if (mouse.resolution == MOUSE_RES_MEDIUMHIGH)	c = 'h';
	else if (mouse.resolution == MOUSE_RES_HIGH)		c = 'd';
	else if (mouse.resolution <=   40) 			c = 'g';
	else if (mouse.resolution <=  100) 			c = 'd';
	else if (mouse.resolution <=  200) 			c = 'e';
	else if (mouse.resolution <=  500) 			c = 'h';
	else if (mouse.resolution <= 1000) 			c = 'j';
	else                                			c = 'd';
	write(mouse.mfd, &c, 1);
	usleep(50000);

	write(mouse.mfd, "\021", 1);	/* Resume DATA output */
	break;

    case P_THINKING:
	SetMouseSpeed(1200, mouse.baudrate, mousecflags[mouse.proto]);
	/* the PnP ID string may be sent again, discard it */
	usleep(200000);
	i = FREAD;
	ioctl(mouse.mfd, TIOCFLUSH, &i);
	/* send the command to initialize the beast */
	for (s = "E5E5"; *s; ++s) {
	    write(mouse.mfd, s, 1);

	    if (poll(pfd, 1, -1) <= 0)
		break;
	    read(mouse.mfd, &c, 1);
	    debug("%c", c);
	    if (c != *s)
	        break;
	}
	break;

    case P_MSC:
	SetMouseSpeed(1200, mouse.baudrate, mousecflags[mouse.proto]);
	if (mouse.flags & ClearDTR) {
	   i = TIOCM_DTR;
	   ioctl(mouse.mfd, TIOCMBIC, &i);
        }
        if (mouse.flags & ClearRTS) {
	   i = TIOCM_RTS;
	   ioctl(mouse.mfd, TIOCMBIC, &i);
        }
	break;

    case P_BM: 
    	break;
    
    case P_PS2:
	
  /* now sets the resolution and rate for PS/2 mice */
	
	/* always sets resolution, to a default value if no value is given */
	
	c = PS2_SET_RES;
	write(mouse.mfd, &c, 1);
	c = mouse.mode.resolution;
	write(mouse.mfd, &c, 1);
	
	if (mouse.rate != MOUSE_RATE_UNKNOWN) {
		c = PS2_SET_RATE;
		write(mouse.mfd, &c, 1);
		c = mouse.mode.rate;
		write(mouse.mfd, &c, 1);
	}

	break;

    default:
	SetMouseSpeed(1200, mouse.baudrate, mousecflags[mouse.proto]);
	break;
    }
}

		


/* mouse_identify : identify the protocol used by the mouse */

static int
mouse_identify(void)
{
    char pnpbuf[256];	/* PnP identifier string may be up to 256 bytes long */
    pnpid_t pnpid;
    symtab_t *t;
    int len;
    
    /*
     * Note : We don't have the ioctl that exist in FreeBSD so we simulate
     * them with the functions mouse_fill_hwinfo() and mouse_fill_mousemode()
     */
	mouse_fill_hwinfo();
	mouse_fill_mousemode();

	if (mouse.proto != P_UNKNOWN)
		bcopy(proto[mouse.proto], cur_proto, sizeof(cur_proto));
	
	/* INPORT and BUS are the same... */
	if (mouse.mode.protocol == P_INPORT)
		mouse.mode.protocol = P_BM;
	if (mouse.mode.protocol != mouse.proto) {
		/* Hmm, the driver doesn't agree with the user... */
		if (mouse.proto != P_UNKNOWN)
			logwarn("mouse type mismatch (%s != %s), %s is assumed",
					mouse_name(mouse.mode.protocol), 
					mouse_name(mouse.proto),
					mouse_name(mouse.mode.protocol));
		mouse.proto = mouse.mode.protocol;
		bcopy(proto[mouse.proto], cur_proto, sizeof(cur_proto));
	}
	
	cur_proto[4] = mouse.mode.packetsize;
	cur_proto[0] = mouse.mode.syncmask[0];	/* header byte bit mask */
	cur_proto[1] = mouse.mode.syncmask[1];	/* header bit pattern */

    /* maybe this is an PnP mouse... */
    if (mouse.mode.protocol == P_UNKNOWN) {

        if (mouse.flags & NoPnP)
            return mouse.proto;
	if (((len = pnpgets(mouse.mfd,pnpbuf)) <= 0) 
		|| !pnpparse(&pnpid, pnpbuf, len))
            return mouse.proto;

        debug("PnP serial mouse: '%*.*s' '%*.*s' '%*.*s'",
	    pnpid.neisaid, pnpid.neisaid, pnpid.eisaid, 
	    pnpid.ncompat, pnpid.ncompat, pnpid.compat, 
	    pnpid.ndescription, pnpid.ndescription, pnpid.description);

	/* we have a valid PnP serial device ID */
        mouse.hw.iftype = MOUSE_IF_SERIAL;
	t = pnpproto(&pnpid);
	if (t != NULL) {
            mouse.mode.protocol = t->val;
            mouse.hw.model = t->val2;
	} else {
            mouse.mode.protocol = P_UNKNOWN;
	}
	if (mouse.mode.protocol == P_INPORT)
	    mouse.mode.protocol = P_BM;

        /* make final adjustment */
	if (mouse.mode.protocol != P_UNKNOWN) {
	    if (mouse.mode.protocol != mouse.proto) {
		/* Hmm, the device doesn't agree with the user... */
                if (mouse.proto != P_UNKNOWN)
	            logwarn("mouse type mismatch (%s != %s), %s is assumed",
		        mouse_name(mouse.mode.protocol), 
			mouse_name(mouse.proto),
		        mouse_name(mouse.mode.protocol));
	        mouse.proto = mouse.mode.protocol;
                bcopy(proto[mouse.proto], cur_proto, sizeof(cur_proto));
	    }
	}
    }

    debug("proto params: %02x %02x %02x %02x %d %02x %02x",
	cur_proto[0], cur_proto[1], cur_proto[2], cur_proto[3], 
	cur_proto[4], cur_proto[5], cur_proto[6]);

    return mouse.proto;
}

/* mouse_protocol : decode bytes with the current mouse protocol */

static int
mouse_protocol(u_char rBuf, mousestatus_t *act)
{
    /* MOUSE_MSS_BUTTON?DOWN -> MOUSE_BUTTON?DOWN */
    static int butmapmss[4] = {	/* Microsoft, MouseMan, GlidePoint, 
				   IntelliMouse, Thinking Mouse */
	0, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
    };
    static int butmapmss2[4] = { /* Microsoft, MouseMan, GlidePoint, 
				    Thinking Mouse */
	0, 
	MOUSE_BUTTON4DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON4DOWN, 
    };
    /* MOUSE_INTELLI_BUTTON?DOWN -> MOUSE_BUTTON?DOWN */
    static int butmapintelli[4] = { /* IntelliMouse, NetMouse, Mie Mouse,
				       MouseMan+ */
	0, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON4DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON4DOWN, 
    };
    /* MOUSE_MSC_BUTTON?UP -> MOUSE_BUTTON?DOWN */
    static int butmapmsc[8] = {	/* MouseSystems, MMSeries, Logitech, 
				   Bus, sysmouse */
	0, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN,
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
    };
    /* MOUSE_PS2_BUTTON?DOWN -> MOUSE_BUTTON?DOWN */
    static int butmapps2[8] = {	/* PS/2 */
	0, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN,
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
    };
    /* for Hitachi tablet */
    static int butmaphit[8] = {	/* MM HitTablet */
	0, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON2DOWN, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON4DOWN, 
	MOUSE_BUTTON5DOWN, 
	MOUSE_BUTTON6DOWN, 
	MOUSE_BUTTON7DOWN, 
    };
    /* for PS/2 VersaPad */
    static int butmapversaps2[8] = { /* VersaPad */
	0, 
	MOUSE_BUTTON3DOWN, 
	0, 
	MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
	MOUSE_BUTTON1DOWN, 
	MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
    };
    static int           pBufP = 0;
    static unsigned char pBuf[8];
    static int		 prev_x, prev_y;
    static int		 on = FALSE;
    int			 x, y;

    debug("received char 0x%x",(int)rBuf);

    /*
     * Hack for resyncing: We check here for a package that is:
     *  a) illegal (detected by wrong data-package header)
     *  b) invalid (0x80 == -128 and that might be wrong for MouseSystems)
     *  c) bad header-package
     *
     * NOTE: b) is a voilation of the MouseSystems-Protocol, since values of
     *       -128 are allowed, but since they are very seldom we can easily
     *       use them as package-header with no button pressed.
     * NOTE/2: On a PS/2 mouse any byte is valid as a data byte. Furthermore,
     *         0x80 is not valid as a header byte. For a PS/2 mouse we skip
     *         checking data bytes.
     *         For resyncing a PS/2 mouse we require the two most significant
     *         bits in the header byte to be 0. These are the overflow bits,
     *         and in case of an overflow we actually lose sync. Overflows
     *         are very rare, however, and we quickly gain sync again after
     *         an overflow condition. This is the best we can do. (Actually,
     *         we could use bit 0x08 in the header byte for resyncing, since
     *         that bit is supposed to be always on, but nobody told
     *         Microsoft...)
     */

    if (pBufP != 0 && mouse.proto != P_PS2 &&
	((rBuf & cur_proto[2]) != cur_proto[3] || rBuf == 0x80))
    {
	pBufP = 0;		/* skip package */
    }
    
    if (pBufP == 0 && (rBuf & cur_proto[0]) != cur_proto[1])
	return 0;

    /* is there an extra data byte? */
    if (pBufP >= cur_proto[4] && (rBuf & cur_proto[0]) != cur_proto[1])
    {
	/*
	 * Hack for Logitech MouseMan Mouse - Middle button
	 *
	 * Unfortunately this mouse has variable length packets: the standard
	 * Microsoft 3 byte packet plus an optional 4th byte whenever the
	 * middle button status changes.
	 *
	 * We have already processed the standard packet with the movement
	 * and button info.  Now post an event message with the old status
	 * of the left and right buttons and the updated middle button.
	 */

	/*
	 * Even worse, different MouseMen and TrackMen differ in the 4th
	 * byte: some will send 0x00/0x20, others 0x01/0x21, or even
	 * 0x02/0x22, so I have to strip off the lower bits.
         *
         * [JCH-96/01/21]
         * HACK for ALPS "fourth button". (It's bit 0x10 of the "fourth byte"
         * and it is activated by tapping the glidepad with the finger! 8^)
         * We map it to bit bit3, and the reverse map in xf86Events just has
         * to be extended so that it is identified as Button 4. The lower
         * half of the reverse-map may remain unchanged.
	 */

        /*
	 * [KY-97/08/03]
	 * Receive the fourth byte only when preceeding three bytes have
	 * been detected (pBufP >= cur_proto[4]).  In the previous
	 * versions, the test was pBufP == 0; thus, we may have mistakingly
	 * received a byte even if we didn't see anything preceeding 
	 * the byte.
	 */

	if ((rBuf & cur_proto[5]) != cur_proto[6]) {
            pBufP = 0;
	    return 0;
	}

	switch (mouse.proto) {

	/*
	 * IntelliMouse, NetMouse (including NetMouse Pro) and Mie Mouse
	 * always send the fourth byte, whereas the fourth byte is
	 * optional for GlidePoint and ThinkingMouse. The fourth byte 
	 * is also optional for MouseMan+ and FirstMouse+ in their 
	 * native mode. It is always sent if they are in the IntelliMouse 
	 * compatible mode.
	 */ 
	case P_IMSERIAL:	/* IntelliMouse, NetMouse, Mie Mouse,
					   MouseMan+ */
	    act->dx = act->dy = 0;
	    act->dz = (rBuf & 0x08) ? (rBuf & 0x0f) - 16 : (rBuf & 0x0f);
	    act->obutton = act->button;
	    act->button = butmapintelli[(rBuf & MOUSE_MSS_BUTTONS) >> 4]
		| (act->obutton & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN));
	    break;

	default:
	    act->dx = act->dy = act->dz = 0;
	    act->obutton = act->button;
	    act->button = butmapmss2[(rBuf & MOUSE_MSS_BUTTONS) >> 4]
		| (act->obutton & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN));
	    break;
	}

	act->flags = ((act->dx || act->dy || act->dz) ? MOUSE_POSCHANGED : 0)
	    | (act->obutton ^ act->button);
        pBufP = 0;
	return act->flags;
    }
        
    if (pBufP >= cur_proto[4])
	pBufP = 0;
    pBuf[pBufP++] = rBuf;
    if (pBufP != cur_proto[4])
	return 0;
    
    /*
     * assembly full package
     */

    debug("assembled full packet (len %d) %x,%x,%x,%x,%x,%x,%x,%x",
	cur_proto[4], 
	pBuf[0], pBuf[1], pBuf[2], pBuf[3], 
	pBuf[4], pBuf[5], pBuf[6], pBuf[7]);

    act->dz = 0;
    act->obutton = act->button;
    switch (mouse.proto) 
    {
    case P_MS:		/* Microsoft */
    case P_LOGIMAN:	/* MouseMan/TrackMan */
    case P_GLIDEPOINT:	/* GlidePoint */
    case P_THINKING:		/* ThinkingMouse */
    case P_IMSERIAL:		/* IntelliMouse, NetMouse, Mie Mouse,
					   MouseMan+ */
	act->button = (act->obutton & (MOUSE_BUTTON2DOWN | MOUSE_BUTTON4DOWN))
            | butmapmss[(pBuf[0] & MOUSE_MSS_BUTTONS) >> 4];
	act->dx = (char)(((pBuf[0] & 0x03) << 6) | (pBuf[1] & 0x3F));
	act->dy = (char)(((pBuf[0] & 0x0C) << 4) | (pBuf[2] & 0x3F));
	break;
      
    case P_MSC:		/* MouseSystems Corp */
	act->button = butmapmsc[(~pBuf[0]) & MOUSE_MSC_BUTTONS];
	act->dx =    (char)(pBuf[1]) + (char)(pBuf[3]);
	act->dy = - ((char)(pBuf[2]) + (char)(pBuf[4]));
	break;
      
    case P_MMHIT:		/* MM HitTablet */
	act->button = butmaphit[pBuf[0] & 0x07];
	act->dx = (pBuf[0] & MOUSE_MM_XPOSITIVE) ?   pBuf[1] : - pBuf[1];
	act->dy = (pBuf[0] & MOUSE_MM_YPOSITIVE) ? - pBuf[2] :   pBuf[2];
	break;

    case P_MM:		/* MM Series */
    case P_LOGI:		/* Logitech Mice */
	act->button = butmapmsc[pBuf[0] & MOUSE_MSC_BUTTONS];
	act->dx = (pBuf[0] & MOUSE_MM_XPOSITIVE) ?   pBuf[1] : - pBuf[1];
	act->dy = (pBuf[0] & MOUSE_MM_YPOSITIVE) ? - pBuf[2] :   pBuf[2];
	break;
      
    case P_BM:		/* Bus */
    case P_INPORT:		/* InPort */
	act->button = butmapmsc[(~pBuf[0]) & MOUSE_MSC_BUTTONS];
	act->dx =   (char)pBuf[1];
	act->dy = - (char)pBuf[2];
	break;

    case P_PS2:		/* PS/2 */
	act->button = butmapps2[pBuf[0] & MOUSE_PS2_BUTTONS];
	act->dx = (pBuf[0] & MOUSE_PS2_XNEG) ?    pBuf[1] - 256  :  pBuf[1];
	act->dy = (pBuf[0] & MOUSE_PS2_YNEG) ?  -(pBuf[2] - 256) : -pBuf[2];
	/*
	 * Moused usually operates the psm driver at the operation level 1
	 * which sends mouse data in P_SYSMOUSE protocol.
	 * The following code takes effect only when the user explicitly 
	 * requets the level 2 at which wheel movement and additional button 
	 * actions are encoded in model-dependent formats. At the level 0
	 * the following code is no-op because the psm driver says the model
	 * is MOUSE_MODEL_GENERIC.
	 */
	switch (mouse.hw.model) {
	case MOUSE_MODEL_INTELLI:
	case MOUSE_MODEL_NET:
	    /* wheel data is in the fourth byte */
	    act->dz = (char)pBuf[3];
	    break;
	case MOUSE_MODEL_MOUSEMANPLUS:
	    if (((pBuf[0] & MOUSE_PS2PLUS_SYNCMASK) == MOUSE_PS2PLUS_SYNC)
		    && (abs(act->dx) > 191)
		    && MOUSE_PS2PLUS_CHECKBITS(pBuf)) {
		/* the extended data packet encodes button and wheel events */
		switch (MOUSE_PS2PLUS_PACKET_TYPE(pBuf)) {
		case 1:
		    /* wheel data packet */
		    act->dx = act->dy = 0;
		    if (pBuf[2] & 0x80) {
			/* horizontal roller count - ignore it XXX*/
		    } else {
			/* vertical roller count */
			act->dz = (pBuf[2] & MOUSE_PS2PLUS_ZNEG)
			    ? (pBuf[2] & 0x0f) - 16 : (pBuf[2] & 0x0f);
		    }
		    act->button |= (pBuf[2] & MOUSE_PS2PLUS_BUTTON4DOWN)
			? MOUSE_BUTTON4DOWN : 0;
		    act->button |= (pBuf[2] & MOUSE_PS2PLUS_BUTTON5DOWN)
			? MOUSE_BUTTON5DOWN : 0;
		    break;
		case 2:
		    /* this packet type is reserved, and currently ignored */
		    /* FALL THROUGH */
		case 0:
		    /* device type packet - shouldn't happen */
		    /* FALL THROUGH */
		default:
		    act->dx = act->dy = 0;
		    act->button = act->obutton;
            	    debug("unknown PS2++ packet type %d: 0x%02x 0x%02x 0x%02x\n",
			  MOUSE_PS2PLUS_PACKET_TYPE(pBuf),
			  pBuf[0], pBuf[1], pBuf[2]);
		    break;
		}
	    } else {
		/* preserve button states */
		act->button |= act->obutton & MOUSE_EXTBUTTONS;
	    }
	    break;
	case MOUSE_MODEL_GLIDEPOINT:
	    /* `tapping' action */
	    act->button |= ((pBuf[0] & MOUSE_PS2_TAP)) ? 0 : MOUSE_BUTTON4DOWN;
	    break;
	case MOUSE_MODEL_NETSCROLL:
	    /* three addtional bytes encode button and wheel events */
	    act->button |= (pBuf[3] & MOUSE_PS2_BUTTON3DOWN) 
		? MOUSE_BUTTON4DOWN : 0;
	    act->dz = (pBuf[3] & MOUSE_PS2_XNEG) ? pBuf[4] - 256 : pBuf[4];
	    break;
	case MOUSE_MODEL_THINK:
	    /* the fourth button state in the first byte */
	    act->button |= (pBuf[0] & MOUSE_PS2_TAP) ? MOUSE_BUTTON4DOWN : 0;
	    break;
	case MOUSE_MODEL_VERSAPAD:
	    act->button = butmapversaps2[pBuf[0] & MOUSE_PS2VERSA_BUTTONS];
	    act->button |=
		(pBuf[0] & MOUSE_PS2VERSA_TAP) ? MOUSE_BUTTON4DOWN : 0;
	    act->dx = act->dy = 0;
	    if (!(pBuf[0] & MOUSE_PS2VERSA_IN_USE)) {
		on = FALSE;
		break;
	    }
	    x = ((pBuf[4] << 8) & 0xf00) | pBuf[1];
	    if (x & 0x800)
		x -= 0x1000;
	    y = ((pBuf[4] << 4) & 0xf00) | pBuf[2];
	    if (y & 0x800)
		y -= 0x1000;
	    if (on) {
		act->dx = prev_x - x;
		act->dy = prev_y - y;
	    } else {
		on = TRUE;
	    }
	    prev_x = x;
	    prev_y = y;
	    break;
	case MOUSE_MODEL_GENERIC:
	default:
	    break;
	}
	break;

    default:
	return 0;
    }
    /* 
     * We don't reset pBufP here yet, as there may be an additional data
     * byte in some protocols. See above.
     */

    /* has something changed? */
    act->flags = ((act->dx || act->dy || act->dz) ? MOUSE_POSCHANGED : 0)
	| (act->obutton ^ act->button);

    if (mouse.flags & Emulate3Button) {
	if (((act->flags & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))
	        == (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))
	    && ((act->button & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))
	        == (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))) {
	    act->button &= ~(MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN);
	    act->button |= MOUSE_BUTTON2DOWN;
	} else if ((act->obutton & MOUSE_BUTTON2DOWN)
	    && ((act->button & (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))
	        != (MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN))) {
	    act->button &= ~(MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN 
			       | MOUSE_BUTTON3DOWN);
	}
	act->flags &= MOUSE_POSCHANGED;
	act->flags |= act->obutton ^ act->button;
    }

    return act->flags;
}

/*
 * Buttons remapping
 */

/* physical to logical button mapping */
static int p2l[MOUSE_MAXBUTTON] = {
    MOUSE_BUTTON1DOWN, MOUSE_BUTTON2DOWN, MOUSE_BUTTON3DOWN, MOUSE_BUTTON4DOWN, 
    MOUSE_BUTTON5DOWN, MOUSE_BUTTON6DOWN, MOUSE_BUTTON7DOWN, MOUSE_BUTTON8DOWN, 
    0x00000100,        0x00000200,        0x00000400,        0x00000800,
    0x00001000,        0x00002000,        0x00004000,        0x00008000,
    0x00010000,        0x00020000,        0x00040000,        0x00080000,
    0x00100000,        0x00200000,        0x00400000,        0x00800000,
    0x01000000,        0x02000000,        0x04000000,        0x08000000,
    0x10000000,        0x20000000,        0x40000000,
};

/*
 * End of functions from the Xfree Project
 */

/* mouse_installmap : install a map between physical and logical buttons */

static int
mouse_installmap(char *arg)
{
    int pbutton;
    int lbutton;
    char *s;

    while (*arg) {
	arg = skipspace(arg);
	s = arg;
	while (isdigit(*arg))
	    ++arg;
	arg = skipspace(arg);
	if ((arg <= s) || (*arg != '='))
	    return FALSE;
	lbutton = atoi(s);

	arg = skipspace(++arg);
	s = arg;
	while (isdigit(*arg))
	    ++arg;
	if ((arg <= s) || (!isspace(*arg) && (*arg != '\0')))
	    return FALSE;
	pbutton = atoi(s);

	if ((lbutton <= 0) || (lbutton > MOUSE_MAXBUTTON))
	    return FALSE;
	if ((pbutton <= 0) || (pbutton > MOUSE_MAXBUTTON))
	    return FALSE;
	p2l[pbutton - 1] = 1 << (lbutton - 1);
    }

    return TRUE;
}

/* mouse_map : convert physical buttons to logical buttons */

static void
mouse_map(mousestatus_t *act1, mousestatus_t *act2)
{
    int pb;
    int pbuttons;
    int lbuttons;

    pbuttons = act1->button;
    lbuttons = 0;

    act2->obutton = act2->button;
    if (pbuttons & mouse.wmode) {
	pbuttons &= ~mouse.wmode;
	act1->dz = act1->dy;
	act1->dx = 0;
	act1->dy = 0;
    }
    act2->dx = act1->dx;
    act2->dy = act1->dy;
    act2->dz = act1->dz;

    switch (mouse.zmap) {
    case 0:	/* do nothing */
	break;
    case MOUSE_XAXIS:
	if (act1->dz != 0) {
	    act2->dx = act1->dz;
	    act2->dz = 0;
	}
	break;
    case MOUSE_YAXIS:
	if (act1->dz != 0) {
	    act2->dy = act1->dz;
	    act2->dz = 0;
	}
	break;
    default:	/* buttons */
	pbuttons &= ~(mouse.zmap | (mouse.zmap << 1));
	if (act1->dz < 0)
	    pbuttons |= mouse.zmap;
	else if (act1->dz > 0)
	    pbuttons |= (mouse.zmap << 1);
	act2->dz = 0;
	break;
    }

    for (pb = 0; (pb < MOUSE_MAXBUTTON) && (pbuttons != 0); ++pb) {
	lbuttons |= (pbuttons & 1) ? p2l[pb] : 0;
	pbuttons >>= 1;
    }
    act2->button = lbuttons;

    act2->flags = ((act2->dx || act2->dy || act2->dz) ? MOUSE_POSCHANGED : 0)
	| (act2->obutton ^ act2->button);
}

/* 
 * Function to handle button click 
 * Note that an ioctl is sent for each button 
 */

static void
mouse_click(mousestatus_t *act)
{
    mouse_info_t mouse_infos;
    struct timeval tv;
    struct timeval tv1;
    struct timeval tv2;
    struct timezone tz;
    int button;
    int mask;
    int i;

    mask = act->flags & MOUSE_BUTTONS;
    if (mask == 0)
	return;

    gettimeofday(&tv1, &tz);
    tv2.tv_sec = mouse.clickthreshold/1000;
    tv2.tv_usec = (mouse.clickthreshold%1000)*1000;
    timersub(&tv1, &tv2, &tv); 
    debug("tv:  %ld %ld", tv.tv_sec, tv.tv_usec);
    button = MOUSE_BUTTON1DOWN;
    for (i = 0; (i < MOUSE_MAXBUTTON) && (mask != 0); ++i) {
        if (mask & 1) {
            if (act->button & button) {
                /* the button is down */
    		debug("  :  %ld %ld", 
		    buttonstate[i].tv.tv_sec, buttonstate[i].tv.tv_usec);
		if (timercmp(&tv, &buttonstate[i].tv, >)) {
                    buttonstate[i].tv.tv_sec = 0;
                    buttonstate[i].tv.tv_usec = 0;
                    buttonstate[i].count = 1;
                } else {
                    ++buttonstate[i].count;
                }
	        mouse_infos.u.event.value = buttonstate[i].count;
            } else {
                /* the button is up */
                buttonstate[i].tv = tv1;
	        mouse_infos.u.event.value = 0;
            }
	    mouse_infos.operation = MOUSE_BUTTON_EVENT;
	    mouse_infos.u.event.id = button;
	    ioctl(mouse.cfd, PCVT_MOUSECTL, &mouse_infos);
	    debug("button %d  count %d\n", i + 1, mouse_infos.u.event.value);
	}
	button <<= 1;
	mask >>= 1;
    }
}

/* 
 * moused : main function 
 *        - wait for mouse events
 *        - translate according to the current protocol
 *        - execute the actions associated to events
 */

static void
moused(void)
{
    mouse_info_t mouse_infos;
    mousestatus_t action;		/* original mouse action */
    mousestatus_t action2;		/* mapped action */
    struct pollfd pfd[1];
    u_char b;
    FILE *fp;
    char moused_flag;

    if ((mouse.cfd = open("/dev/pcvtctl", O_RDWR, 0)) == -1)
	logerr(1, "cannot open /dev/pcvtctl");

    if (!nodaemon && !background) {
	if (daemon(0, 0)) {
	    logerr(1, "failed to become a daemon");
	} else {
	  background = TRUE;
	    fp = fopen(pidfile, "w");
	    if (fp != NULL) {
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	    }
	}
    }

    /* restart point when coming from X */
    setjmp(restart_env);
    
    /* display initial cursor */
    mouse_infos.operation = MOUSE_INIT;

    moused_flag = MOUSED_ON;
    ioctl(mouse.cfd, PCVT_MOUSED, &moused_flag);
    ioctl(mouse.cfd, PCVT_MOUSECTL, &mouse_infos);
    
    /* clear mouse data */
    bzero(&action, sizeof(action));
    bzero(&action2, sizeof(action2));
    bzero(&buttonstate, sizeof(buttonstate));
    bzero(&mouse_infos, sizeof(mouse));

    pfd[0].fd = mouse.mfd;
    pfd[0].events = POLLIN;

    /* process mouse data */
    for (;;) {

	if (poll(pfd, 1, -1) <= 0)
	    logwarn("failed to read from mouse");

	/*  mouse event  */
	read(mouse.mfd, &b, 1);
	if (mouse_protocol(b, &action)) {	/* handler detected action */
	    mouse_map(&action, &action2);
#if 0
	    fprintf("activity : buttons 0x%08x  dx %d  dy %d  dz %d\n",
		action2.button, action2.dx, action2.dy, action2.dz);
#endif
	    mouse_click(&action2);
	    if (action2.flags & MOUSE_POSCHANGED) {
		    mouse_infos.operation = MOUSE_MOTION_EVENT;
		    mouse_infos.u.data.buttons = action2.button;
		    mouse_infos.u.data.x = action2.dx;
	            mouse_infos.u.data.y = action2.dy;
	            mouse_infos.u.data.z = action2.dz;
		    ioctl(mouse.cfd, PCVT_MOUSECTL, &mouse_infos);
	    }
	    /*
	     * If the Z axis movement is mapped to a imaginary physical 
	     * button, we need to cook up a corresponding button `up' event
	     * after sending a button `down' event.
	     */
            if ((mouse.zmap > 0) && (action.dz != 0)) {
		action.obutton = action.button;
		action.dx = action.dy = action.dz = 0;
	        mouse_map(&action, &action2);
	        debug("activity : buttons 0x%08x  dx %d  dy %d  dz %d",
		    action2.button, action2.dx, action2.dy, action2.dz);
		
		mouse_click(&action2);
	    } 
	    /* NOT REACHED */
	}
    }
}    

static void 
usage(void)
{
	printf("usage : %s [-3DPRcdfs] [-I file] [-F rate] [-r resolution ] [-S baudrate]",__progname);
	printf("                  [-C threshold] [-t protocol] [-m type] [-M N=M] [-b buttons]");
	printf("                    [-w N] [-z target] -p port \n");
	printf("        %s -i info -p port \n",__progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int opt;
	int i;

	while ((opt = (getopt(argc,argv,"3DPRb:cdfhi:m:st:I:F:r:M:S:C:p:w:z:"))) != -1) {
		switch (opt) {
			case '3':
				mouse.flags |= Emulate3Button;
				break;
			case 'D':
				mouse.flags |= ClearDTR;
				break;
			case 'P':
				mouse.flags |= NoPnP;
				break;
			case 'R':
				mouse.flags |= ClearRTS;
				break;
			case 'b':
				mouse.hw.buttons = atoi(optarg);
				break;
			case 'c':
				mouse.flags |= ChordMiddle;			
				break;
			case 'd':
				++debug;
				break;
			case 'f':
				nodaemon = TRUE;
				break;
			case 'M':
				if (!mouse_installmap(optarg)) {
					warnx("invalid mapping `%s'\n", optarg);
					usage();
				}
				break;
			case 's':
				mouse.baudrate = 9600;
				break;	
			case 't':
				if (strcmp(optarg, "auto") == 0) {
					mouse.proto = P_UNKNOWN;
					mouse.flags &= ~NoPnP;
					break;
				}
				for (i = 0; mouse_names[i]; i++)
					if (strcmp(optarg,mouse_names[i]) == 0){
						mouse.proto = i;
						mouse.flags |= NoPnP;
						break;
					}
				if (mouse_names[i])
					break;
				printf("no such mouse protocol `%s'\n", optarg);
				usage();
				break;
			case 'm':
				for (i = 0 ; mouse_models[i].name; i++)
					if (strcmp(optarg,mouse_models[i].name) == 0){
						mouse.hw.model =
							mouse_models[i].val;
						break;
					}
				if (mouse_models[i].name)
					break;
				printf("no such mouse type `%s'\n", optarg);
				usage();
				break;
			case 'i':
				if (strcmp(optarg, "all") == 0)
					identify = ID_ALL;
				else if (strcmp(optarg, "port") == 0)
					identify = ID_PORT;
				else if (strcmp(optarg, "if") == 0)
					identify = ID_IF;
				else if (strcmp(optarg, "type") == 0)
					identify = ID_TYPE;
				else if (strcmp(optarg, "model") == 0)
					identify = ID_MODEL;
				else {
					printf("invalid argument `%s'", optarg);
					usage();
				}
				break;
			case 'I':
				pidfile = optarg;
				break;
			case 'F':
				mouse.rate = atoi(optarg);
				if (mouse.rate <= 0) {
					printf("invalid rate `%s'\n", optarg);
					usage();
				}
				break;
			case 'r':
				if (strcmp(optarg, "high") == 0)
					mouse.resolution = MOUSE_RES_HIGH;
				else if (strcmp(optarg, "medium-high") == 0)
					mouse.resolution = MOUSE_RES_MEDIUMHIGH;
				else if (strcmp(optarg, "medium-low") == 0)
					mouse.resolution = MOUSE_RES_MEDIUMLOW;
				else if (strcmp(optarg, "low") == 0)
					mouse.resolution = MOUSE_RES_LOW;
				else if (strcmp(optarg, "default") == 0)
					mouse.resolution = MOUSE_RES_DEFAULT;
				else {
					mouse.resolution = atoi(optarg);
					if (mouse.resolution <= 0) {
						printf("invalid resolution `%s'\n", optarg);
						usage();
					}
				}
				break;
			case 'S':
				mouse.baudrate = atoi(optarg);
				if (mouse.baudrate <= 0) {
					printf("invalid baudrate `%s'\n", optarg);
					usage();
				}
				break;
			case 'C':
#define MAX_CLICKTHRESHOLD 2000 /* max delay for double click */
				
				mouse.clickthreshold = atoi(optarg);
				if ((mouse.clickthreshold < 0) || 
				(mouse.clickthreshold > MAX_CLICKTHRESHOLD)) {
					printf("invalid threshold `%s': max value is %d\n"
						, optarg,MAX_CLICKTHRESHOLD);
					usage();
				}
				break;
			case 'p':
				mouse.portname = strdup(optarg);
				break;
			case 'h':
				usage();
				break;
			case 'w':
				i = atoi(optarg);
				if ((i <= 0) || (i > MOUSE_MAXBUTTON)) {
					warnx("invalid argument `%s'", optarg);
					usage();
				}
				mouse.wmode = 1 << (i - 1);
				break;
			case 'z':
				if (strcmp(optarg, "x") == 0)
					mouse.zmap = MOUSE_XAXIS;
				else if (strcmp(optarg, "y") == 0)
					mouse.zmap = MOUSE_YAXIS;
				else {
					i = atoi(optarg);
				/* 
				 * Use button i for negative Z axis movement 
				 * and button (i + 1) for positive Z axis 
				 * movement.
			         */
					if ((i <= 0) || 
					(i > MOUSE_MAXBUTTON - 1)) {
						warnx("invalid argument `%s'", 
								optarg);
						usage();
					}
					mouse.zmap = 1 << (i - 1);
				}
				break;
			default:
				usage();
		}
	}
	/* 
	 * mouse device name : 
	 * Logitech bus mouse : /dev/lms0
	 * Microsoft bus mouse (also refered as inport) : /dev/mms0 
	 * PS/2 mouse : /dev/pms0 
	 *
	 * Note that Logitech and Microsoft bus mouse speak the same
	 * protocol (BusMouse).
	 */
	switch(mouse.proto) {
		case P_INPORT:
			/* Inport and Bus use the same protocol (BusMouse) */
			mouse.proto = P_BM;
			mouse.portname = MMS_DEV;
			break;
		case P_BM:
			if (!mouse.portname)
				mouse.portname = LMS_DEV;
			break;
		case P_PS2:
			if (!mouse.portname)
				mouse.portname = PMS_DEV;
			break;
		default:
			if (mouse.portname) /* serial mouse */
				break;
			printf("no port name specified\n");
			usage();
	}
	
	for (;;) {
		if (sigsetjmp(env, 1) == 0) {
			signal(SIGUSR1, freedev);
			signal(SIGUSR2, opendev);
			signal(SIGINT , cleanup);
			signal(SIGQUIT, cleanup);
			signal(SIGTERM, cleanup);
			if ((mouse.mfd = open(mouse.portname, 
			    O_RDWR | O_NONBLOCK, 0)) == -1) 
				logerr(1, "unable to open %s", mouse.portname);
			if (mouse_identify() == P_UNKNOWN) {
				logwarn("cannot determine mouse type on %s",
				    mouse.portname);
				close(mouse.mfd);
				mouse.mfd = -1;
			}

			/* print some information */
			if (identify != ID_NONE) {
				if (identify == ID_ALL)
					printf("%s %s %s %s\n", 
					    mouse.portname,
					    mouse_if(mouse.hw.iftype),
					    mouse_name(mouse.proto), 
					    mouse_model(mouse.hw.model));
				else if (identify & ID_PORT)
					printf("%s\n", mouse.portname);
				else if (identify & ID_IF)
					printf("%s\n", mouse_if(mouse.hw.iftype));
				else if (identify & ID_TYPE)
					printf("%s\n", mouse_name(mouse.proto));
				else if (identify & ID_MODEL)
					printf("%s\n", mouse_model(mouse.hw.model));
				exit(0);
			} else {
				debug("port: %s  interface: %s  type: %s  model: %s", 
				mouse.portname, mouse_if(mouse.hw.iftype),
				mouse_name(mouse.proto), mouse_model(mouse.hw.model));
			}

			if (mouse.mfd == -1) {
			/*
		         * We cannot continue because of error.  Exit if the 
		         * program has not become a daemon.  Otherwise, block 
		         * until the the user corrects the problem and issues 
			 * SIGHUP. 
			 */
				if (!background)
					exit(1);
				sigpause(0);
			}

			mouse_init(); /* init mouse */
			moused();
	}

		if (mouse.mfd != -1)
			close(mouse.mfd);
	}
	/* NOT REACHED */

	exit(0);
	return (0);
}

