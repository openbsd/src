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

/* Logging macros */

#define debug(fmt,args...) \
	if (debug&&nodaemon) printf(fmt, ##args)

#define logerr(e, fmt, args...) {				\
	if (background) {					\
	    syslog(LOG_DAEMON | LOG_ERR, fmt, ##args);		\
	    exit(e);						\
	} else							\
	    errx(e, fmt, ##args);				\
}

#define logwarn(fmt, args...) {				\
	if (background)						\
	    syslog(LOG_DAEMON | LOG_WARNING, fmt, ##args);	\
	else							\
	    warnx(fmt, ##args);					\
}

/* Logitech PS2++ protocol */
#define MOUSE_PS2PLUS_CHECKBITS(b)	\
			((((b[2] & 0x03) << 2) | 0x02) == (b[1] & 0x0f))
#define MOUSE_PS2PLUS_PACKET_TYPE(b)	\
			(((b[0] & 0x30) >> 2) | ((b[1] & 0x30) >> 4))

/* Daemon flags */

#define	ChordMiddle	0x0001 /* avoid bug reporting middle button as down 
				  when left and right are pressed */
#define Emulate3Button	0x0002 /* option to emulate a third button */
#define ClearDTR	0x0004 /* for mousesystems protocol (3 button mouse) */
#define ClearRTS	0x0008 /* idem as above */
#define NoPnP		0x0010 /* disable PnP for PnP mice */

/* Mouse info flags (when specifying the -i option) */

#define ID_NONE		0
#define ID_PORT		1
#define ID_IF		2
#define ID_TYPE 	4
#define ID_MODEL	8
#define ID_ALL		(ID_PORT | ID_IF | ID_TYPE | ID_MODEL)

#define FALSE 0
#define TRUE 1
#define ErrorF printf

/* Mouse Interface */

#define MOUSE_IF_UNKNOWN 	(-1)
#define MOUSE_IF_SERIAL 	0
#define MOUSE_IF_BUS 		1
#define MOUSE_IF_INPORT 	2
#define MOUSE_IF_PS2 		3
#define MOUSE_IF_USB 		4

/* Devices corresponding to physical interfaces */

#define LMS_DEV "/dev/lms0"
#define MMS_DEV "/dev/mms0"
#define PMS_DEV "/dev/psm0"
#define SERIAL_DEV "/dev/cua0" /* can be /dev/cua00, /dev/cua01, ... */

/* array giving the names of the physical interface (ordered by MOUSE_IF_XXX) */

/* symbol table entry */
typedef struct {
    char *name;
    int val;
    int val2;
} symtab_t;

/* Mouse type */

#define MOUSE_TYPE_UNKNOWN	(-1)
#define MOUSE_TYPE_MOUSE	0
#define MOUSE_TRACKBALL		1
#define MOUSE_STICK		2
#define MOUSE_PAD		3

/* Mouse models */

#define MOUSE_MODEL_UNKNOWN		(-1)
#define MOUSE_MODEL_GENERIC		0
#define MOUSE_MODEL_GLIDEPOINT		1
#define MOUSE_MODEL_NETSCROLL		2
#define MOUSE_MODEL_NET			3
#define MOUSE_MODEL_INTELLI		4
#define MOUSE_MODEL_THINK		5
#define MOUSE_MODEL_EASYSCROLL		6
#define MOUSE_MODEL_MOUSEMANPLUS	7
#define MOUSE_MODEL_KIDSPAD		8
#define MOUSE_MODEL_VERSAPAD		9

/* Mouse protocols */

#define P_UNKNOWN 	(-1)
#define P_MS 		0 /* Microsoft Serial, 3 bytes */
#define P_MSC 		1 /* Mouse Systems, 5 bytes */
#define P_LOGI 		2 /* Logitech, 3 bytes */
#define P_MM 		3 /* MM series, 3 bytes */
#define P_LOGIMAN	4 /* Logitech MouseMan 3/4 bytes */
#define P_BM 		5 /* MS/Logitech bus mouse */
#define P_INPORT	6 /* MS/ATI InPort mouse (same protocol as above) */ 
#define P_PS2 		7 /* PS/2 mouse, 3 bytes */
#define P_MMHIT		8 /* Hitachi Tablet 3 bytes */
#define P_GLIDEPOINT	9 /* ALPS GlidePoint, 3/4 bytes */
#define P_IMSERIAL	10 /* MS IntelliMouse, 4 bytes */
#define P_THINKING	11 /* Kensignton Thinking Mouse, 3/4 bytes */

/* X and Y axis */

#define MOUSE_XAXIS	(-1)
#define MOUSE_YAXIS	(-2)

/* flags */

#define MOUSE_STDBUTTONSCHANGED	MOUSE_STDBUTTONS
#define MOUSE_EXTBUTTONSCHANGED	MOUSE_EXTBUTTONS
#define MOUSE_BUTTONSCHANGED	MOUSE_BUTTONS
#define MOUSE_POSCHANGED	0x80000000

/* 
 * List of all the protocols parameters 
 * The parameters are :
 * - size of the packet
 * - synchronization mask 
 * - synchronization value (must be equal to data ANDed with SYNCMASK)
 * - mask of buttons 
 * - mask of each button separetely
 */

/* Microsoft Serial mouse data packet */
#define MOUSE_MSS_PACKETSIZE	3
#define MOUSE_MSS_SYNCMASK	0x40
#define MOUSE_MSS_SYNC		0x40
#define MOUSE_MSS_BUTTONS	0x30
#define MOUSE_MSS_BUTTON1DOWN	0x20	/* left */
#define MOUSE_MSS_BUTTON2DOWN	0x00	/* no middle button */
#define MOUSE_MSS_BUTTON3DOWN	0x10	/* right */

/* Logitech MouseMan data packet (M+ protocol) */
#define MOUSE_LMAN_BUTTON2DOWN	0x20	/* middle button, the 4th byte */

/* ALPS GlidePoint extention (variant of M+ protocol) */
#define MOUSE_ALPS_BUTTON2DOWN	0x20	/* middle button, the 4th byte */
#define MOUSE_ALPS_TAP		0x10	/* `tapping' action, the 4th byte */

/* Kinsington Thinking Mouse extention (variant of M+ protocol) */
#define MOUSE_THINK_BUTTON2DOWN 0x20	/* lower-left button, the 4th byte */
#define MOUSE_THINK_BUTTON4DOWN 0x10	/* lower-right button, the 4th byte */

/* MS IntelliMouse (variant of MS Serial) */
#define MOUSE_INTELLI_PACKETSIZE 4
#define MOUSE_INTELLI_BUTTON2DOWN 0x10	/* middle button the 4th byte */

/* Mouse Systems Corp. mouse data packet */
#define MOUSE_MSC_PACKETSIZE	5
#define MOUSE_MSC_SYNCMASK	0xf8
#define MOUSE_MSC_SYNC		0x80
#define MOUSE_MSC_BUTTONS	0x07
#define MOUSE_MSC_BUTTON1UP	0x04	/* left */
#define MOUSE_MSC_BUTTON2UP	0x02	/* middle */
#define MOUSE_MSC_BUTTON3UP	0x01	/* right */
#define MOUSE_MSC_MAXBUTTON	3

/* MM series mouse data packet */
#define MOUSE_MM_PACKETSIZE	3
#define MOUSE_MM_SYNCMASK	0xe0
#define MOUSE_MM_SYNC		0x80
#define MOUSE_MM_BUTTONS	0x07
#define MOUSE_MM_BUTTON1DOWN	0x04	/* left */
#define MOUSE_MM_BUTTON2DOWN	0x02	/* middle */
#define MOUSE_MM_BUTTON3DOWN	0x01	/* right */
#define MOUSE_MM_XPOSITIVE	0x10
#define MOUSE_MM_YPOSITIVE	0x08

/* PS/2 mouse data packet */
#define MOUSE_PS2_PACKETSIZE	3
#define MOUSE_PS2_SYNCMASK	0xc8
#define MOUSE_PS2_SYNC		0x08
#define MOUSE_PS2_BUTTONS	0x07	/* 0x03 for 2 button mouse */
#define MOUSE_PS2_BUTTON1DOWN	0x01	/* left */
#define MOUSE_PS2_BUTTON2DOWN	0x04	/* middle */
#define MOUSE_PS2_BUTTON3DOWN	0x02	/* right */
#define MOUSE_PS2_TAP		MOUSE_PS2_SYNC /* GlidePoint (PS/2) `tapping'
					        * Yes! this is the same bit 
						* as SYNC!
					 	*/
#define MOUSE_PS2PLUS_BUTTON4DOWN 0x10	/* 4th button on MouseMan+ */
#define MOUSE_PS2PLUS_BUTTON5DOWN 0x20

#define MOUSE_PS2_XNEG		0x10
#define MOUSE_PS2_YNEG		0x20
#define MOUSE_PS2_XOVERFLOW	0x40
#define MOUSE_PS2_YOVERFLOW	0x80
#define MOUSE_PS2PLUS_ZNEG	0x08	/* MouseMan+ negative wheel movement */
#define MOUSE_PS2PLUS_SYNCMASK	0x48
#define MOUSE_PS2PLUS_SYNC	0x48

/* Interlink VersaPad (serial I/F) data packet */
#define MOUSE_VERSA_PACKETSIZE	6
#define MOUSE_VERSA_IN_USE	0x04
#define MOUSE_VERSA_SYNCMASK	0xc3
#define MOUSE_VERSA_SYNC	0xc0
#define MOUSE_VERSA_BUTTONS	0x30
#define MOUSE_VERSA_BUTTON1DOWN	0x20	/* left */
#define MOUSE_VERSA_BUTTON2DOWN	0x00	/* middle */
#define MOUSE_VERSA_BUTTON3DOWN	0x10	/* right */
#define MOUSE_VERSA_TAP		0x08

/* Interlink VersaPad (PS/2 I/F) data packet */
#define MOUSE_PS2VERSA_PACKETSIZE	6
#define MOUSE_PS2VERSA_IN_USE		0x10
#define MOUSE_PS2VERSA_SYNCMASK		0xe8
#define MOUSE_PS2VERSA_SYNC		0xc8
#define MOUSE_PS2VERSA_BUTTONS		0x05
#define MOUSE_PS2VERSA_BUTTON1DOWN	0x04	/* left */
#define MOUSE_PS2VERSA_BUTTON2DOWN	0x00	/* middle */
#define MOUSE_PS2VERSA_BUTTON3DOWN	0x01	/* right */
#define MOUSE_PS2VERSA_TAP		0x02

/* Mouse resolutions */

#define MOUSE_RES_UNKNOWN	(-1)
#define MOUSE_RES_DEFAULT	0
#define MOUSE_RES_LOW		(-2)
#define MOUSE_RES_MEDIUMLOW	(-3)
#define MOUSE_RES_MEDIUMHIGH	(-4)
#define MOUSE_RES_HIGH		(-5)

/* serial PnP ID string */
typedef struct {
    int revision;	/* PnP revision, 100 for 1.00 */
    char *eisaid;	/* EISA ID including mfr ID and product ID */
    char *serial;	/* serial No, optional */
    char *class;	/* device class, optional */
    char *compat;	/* list of compatible drivers, optional */
    char *description;	/* product description, optional */
    int neisaid;	/* length of the above fields... */
    int nserial;
    int nclass;
    int ncompat;
    int ndescription;
} pnpid_t;

/* mousehw structure : hardware infos */

typedef struct mousehw {
	int buttons;		/* -1 if unknown */
	int iftype;		/* MOUSE_IF_XXX */
	int type;		/* MOUSE_TYPE_XXX */
	int model;		/* MOUSE_MODEL_XXX */
} mousehw_t;

/* mousemode structure : mouse settings */

typedef struct mousemode {
	int protocol;		/* MOUSE_PROTO_XXX */
	int rate;		/* report rate (per sec), -1 if unknown */
	int resolution;		/* MOUSE_RES_XXX, -1 if unknown */
	int accelfactor;	/* accelation factor (must be 1 or greater) */
	int packetsize;		/* the length of the data packet */
	unsigned char syncmask[2]; /* sync. data bits in the header byte */
} mousemode_t;

/* mouse structure : main structure */

typedef struct mouse_s {
    int flags;
    char *portname;		/* /dev/XXX */
    int proto;			/* MOUSE_PROTO_XXX */
    int baudrate;
    int old_baudrate;
    int rate;			/* report rate */
    int resolution;		/* MOUSE_RES_XXX or a positive number */
    int zmap;			/* MOUSE_{X|Y}AXIS or a button number */
    int wmode;			/* wheel mode button number */
    int mfd;			/* mouse file descriptor */
    int cfd;			/* console file descriptor */
    long clickthreshold;	/* double click speed in msec */
    mousehw_t hw;		/* mouse device hardware information */
    mousemode_t mode;		/* protocol information */
} mouse_t ;

/* Current status of the mouse */
typedef struct mousestatus {
    int     flags;		/* state change flags */
    int     button;		/* button status */
    int     obutton;		/* previous button status */
    int     dx;			/* x movement */
    int     dy;			/* y movement */
    int     dz;			/* z movement */
} mousestatus_t;

