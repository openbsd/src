/* $OpenBSD: wsconsio.h,v 1.59 2010/03/04 14:31:12 jsing Exp $ */
/* $NetBSD: wsconsio.h,v 1.74 2005/04/28 07:15:44 martin Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#ifndef _DEV_WSCONS_WSCONSIO_H_
#define	_DEV_WSCONS_WSCONSIO_H_

/*
 * WSCONS (wsdisplay, wskbd, wsmouse) exported interfaces.
 *
 * Ioctls are all in group 'W'.  Ioctl number space is partitioned like:
 *	0-31	keyboard ioctls (WSKBDIO)
 *	32-63	mouse ioctls (WSMOUSEIO)
 *	64-95	display ioctls (WSDISPLAYIO)
 *	96-127	mux ioctls (WSMUXIO)
 *	128-159 driver private ioctls
 *	160-255 reserved for future use
 */

#include <sys/types.h>
#include <sys/ioccom.h>
#include <dev/wscons/wsksymvar.h>

#include <sys/pciio.h>

#define	WSSCREEN_NAME_SIZE	16
#define	WSEMUL_NAME_SIZE	16
#define	WSFONT_NAME_SIZE	16

/*
 * Common event structure (used by keyboard and mouse)
 */
struct wscons_event {
	u_int		type;
	int		value;
	struct timespec	time;
};

/* Event type definitions.  Comment for each is information in value. */
#define	WSCONS_EVENT_KEY_UP		1	/* key code */
#define	WSCONS_EVENT_KEY_DOWN		2	/* key code */
#define	WSCONS_EVENT_ALL_KEYS_UP	3	/* void */
#define	WSCONS_EVENT_MOUSE_UP		4	/* button # (leftmost = 0) */
#define	WSCONS_EVENT_MOUSE_DOWN		5	/* button # (leftmost = 0)  */
#define	WSCONS_EVENT_MOUSE_DELTA_X	6	/* X delta amount */
#define	WSCONS_EVENT_MOUSE_DELTA_Y	7	/* Y delta amount */
#define	WSCONS_EVENT_MOUSE_ABSOLUTE_X	8	/* X location */
#define	WSCONS_EVENT_MOUSE_ABSOLUTE_Y	9	/* Y location */
#define	WSCONS_EVENT_MOUSE_DELTA_Z	10	/* Z delta amount */
#define	WSCONS_EVENT_MOUSE_ABSOLUTE_Z	11	/* Z location */
				     /*	12-15, see below */
#define	WSCONS_EVENT_MOUSE_DELTA_W	16	/* W delta amount */
#define	WSCONS_EVENT_MOUSE_ABSOLUTE_W	17	/* W location */
/* 
 * Following events are not real wscons_event but are used as parameters of the
 * WSDISPLAYIO_WSMOUSED ioctl 
 */
#define WSCONS_EVENT_WSMOUSED_ON	12	/* wsmoused(8) active */
#define WSCONS_EVENT_WSMOUSED_OFF	13	/* wsmoused(8) inactive */
#define WSCONS_EVENT_WSMOUSED_SLEEP	14	/* wsmoused(8) sleeping */
#define WSCONS_EVENT_WSMOUSED_CLOSE	15	/* notify wsmoused(8) to close 
						   mouse device */

#define IS_MOTION_EVENT(type) (((type) == WSCONS_EVENT_MOUSE_DELTA_X) || \
			       ((type) == WSCONS_EVENT_MOUSE_DELTA_Y) || \
			       ((type) == WSCONS_EVENT_MOUSE_DELTA_Z) || \
			       ((type) == WSCONS_EVENT_MOUSE_DELTA_W))
#define IS_BUTTON_EVENT(type) (((type) == WSCONS_EVENT_MOUSE_UP) || \
			       ((type) == WSCONS_EVENT_MOUSE_DOWN))
#define IS_CTRL_EVENT(type) ((type == WSCONS_EVENT_WSMOUSED_ON) || \
			     (type == WSCONS_EVENT_WSMOUSED_OFF)|| \
			     (type == WSCONS_EVENT_WSMOUSED_SLEEP))

/*
 * Keyboard ioctls (0 - 31)
 */

/* Get keyboard type. */
#define	WSKBDIO_GTYPE		_IOR('W', 0, u_int)
#define		WSKBD_TYPE_LK201	1	/* lk-201 */
#define		WSKBD_TYPE_LK401	2	/* lk-401 */
#define		WSKBD_TYPE_PC_XT	3	/* PC-ish, XT scancode */
#define		WSKBD_TYPE_PC_AT	4	/* PC-ish, AT scancode */
#define		WSKBD_TYPE_USB		5	/* USB, XT scancode */
#define		WSKBD_TYPE_NEXT		6	/* NeXT keyboard */
#define		WSKBD_TYPE_HPC_KBD	7	/* HPC builtin keyboard */
#define		WSKBD_TYPE_HPC_BTN	8	/* HPC/PsPC buttons */
#define		WSKBD_TYPE_ARCHIMEDES	9	/* Archimedes keyboard */
#define		WSKBD_TYPE_ADB		10	/* Apple ADB keyboard */
#define		WSKBD_TYPE_SUN		11	/* Sun Type3/4 */
#define		WSKBD_TYPE_SUN5		12	/* Sun Type5 */
#define		WSKBD_TYPE_HIL		13	/* HP HIL */
#define		WSKBD_TYPE_GSC		14	/* HP PS/2 */
#define		WSKBD_TYPE_LUNA		15	/* OMRON Luna */
#define		WSKBD_TYPE_ZAURUS	16	/* Sharp Zaurus */
#define		WSKBD_TYPE_DOMAIN	17	/* Apollo Domain */
#define		WSKBD_TYPE_BLUETOOTH	18	/* Bluetooth keyboard */
#define		WSKBD_TYPE_KPC		19	/* Palm keypad */

/* Manipulate the keyboard bell. */
struct wskbd_bell_data {
	u_int	which;				/* values to get/set */
	u_int	pitch;				/* pitch, in Hz */
	u_int	period;				/* period, in milliseconds */
	u_int	volume;				/* percentage of max volume */
};
#define		WSKBD_BELL_DOPITCH	0x1		/* get/set pitch */
#define		WSKBD_BELL_DOPERIOD	0x2		/* get/set period */
#define		WSKBD_BELL_DOVOLUME	0x4		/* get/set volume */
#define		WSKBD_BELL_DOALL	0x7		/* all of the above */

#define	WSKBDIO_BELL		_IO('W', 1)
#define	WSKBDIO_COMPLEXBELL	_IOW('W', 2, struct wskbd_bell_data)
#define	WSKBDIO_SETBELL		_IOW('W', 3, struct wskbd_bell_data)
#define	WSKBDIO_GETBELL		_IOR('W', 4, struct wskbd_bell_data)
#define	WSKBDIO_SETDEFAULTBELL	_IOW('W', 5, struct wskbd_bell_data)
#define	WSKBDIO_GETDEFAULTBELL	_IOR('W', 6, struct wskbd_bell_data)

/* Manipulate the emulation key repeat settings. */
struct wskbd_keyrepeat_data {
	u_int	which;				/* values to get/set */
	u_int	del1;				/* delay before first, ms */
	u_int	delN;				/* delay before rest, ms */
};
#define		WSKBD_KEYREPEAT_DODEL1	0x1		/* get/set del1 */
#define		WSKBD_KEYREPEAT_DODELN	0x2		/* get/set delN */
#define		WSKBD_KEYREPEAT_DOALL	0x3		/* all of the above */

#define	WSKBDIO_SETKEYREPEAT	_IOW('W', 7, struct wskbd_keyrepeat_data)
#define	WSKBDIO_GETKEYREPEAT	_IOR('W', 8, struct wskbd_keyrepeat_data)
#define	WSKBDIO_SETDEFAULTKEYREPEAT \
	    _IOW('W', 9, struct wskbd_keyrepeat_data)
#define	WSKBDIO_GETDEFAULTKEYREPEAT \
	    _IOR('W', 10, struct wskbd_keyrepeat_data)

/* Get/set keyboard leds */
#define		WSKBD_LED_CAPS		0x01
#define		WSKBD_LED_NUM		0x02
#define		WSKBD_LED_SCROLL	0x04
#define		WSKBD_LED_COMPOSE	0x08

#define	WSKBDIO_SETLEDS		_IOW('W', 11, int)
#define	WSKBDIO_GETLEDS		_IOR('W', 12, int)

/* Manipulate keysym groups. */
struct wskbd_map_data {
	u_int	maplen;				/* number of entries in map */
#define WSKBDIO_MAXMAPLEN	65536
	struct wscons_keymap *map;		/* map to get or set */
};
#define WSKBDIO_GETMAP		_IOWR('W', 13, struct wskbd_map_data)
#define WSKBDIO_SETMAP		_IOW('W', 14, struct wskbd_map_data)
#define WSKBDIO_GETENCODING	_IOR('W', 15, kbd_t)
#define WSKBDIO_SETENCODING	_IOW('W', 16, kbd_t)

/* internal use only */
#define WSKBDIO_SETMODE		_IOW('W', 19, int)
#define WSKBDIO_GETMODE		_IOR('W', 20, int)
#define		WSKBD_TRANSLATED	0
#define		WSKBD_RAW		1

/*
 * Mouse ioctls (32 - 63)
 */

/* Get mouse type */
#define	WSMOUSEIO_GTYPE		_IOR('W', 32, u_int)
#define		WSMOUSE_TYPE_VSXXX	1	/* DEC serial */
#define		WSMOUSE_TYPE_PS2	2	/* PS/2-compatible */
#define		WSMOUSE_TYPE_USB	3	/* USB mouse */
#define		WSMOUSE_TYPE_LMS	4	/* Logitech busmouse */
#define		WSMOUSE_TYPE_MMS	5	/* Microsoft InPort mouse */
#define		WSMOUSE_TYPE_TPANEL	6	/* Generic Touch Panel */
#define		WSMOUSE_TYPE_NEXT	7	/* NeXT mouse */
#define		WSMOUSE_TYPE_ARCHIMEDES	8	/* Archimedes mouse */
#define		WSMOUSE_TYPE_ADB	9	/* ADB */
#define		WSMOUSE_TYPE_HIL	10	/* HP HIL */
#define		WSMOUSE_TYPE_LUNA	11	/* OMRON Luna */
#define		WSMOUSE_TYPE_DOMAIN	12	/* Apollo Domain */
#define		WSMOUSE_TYPE_BLUETOOTH	13	/* Bluetooth mouse */
#define		WSMOUSE_TYPE_SUN	14	/* SUN serial mouse */

/* Set resolution.  Not applicable to all mouse types. */
#define	WSMOUSEIO_SRES		_IOW('W', 33, u_int)
#define		WSMOUSE_RES_MIN		0
#define		WSMOUSE_RES_DEFAULT	75
#define		WSMOUSE_RES_MAX		100

/* Set scale factor (num / den).  Not applicable to all mouse types. */
#define	WSMOUSEIO_SSCALE	_IOW('W', 34, u_int[2])

/* Set sample rate.  Not applicable to all mouse types. */
#define	WSMOUSEIO_SRATE		_IOW('W', 35, u_int)
#define		WSMOUSE_RATE_MIN	0
#define		WSMOUSE_RATE_DEFAULT	50
#define		WSMOUSE_RATE_MAX	100

/* Set/get sample coordinates for calibration */
#define	WSMOUSE_CALIBCOORDS_MAX		16
#define	WSMOUSE_CALIBCOORDS_RESET	-1
struct wsmouse_calibcoords {
	int minx, miny;		/* minimum value of X/Y */
	int maxx, maxy;		/* maximum value of X/Y */
	int swapxy;		/* swap X/Y axis */
	int resx, resy;		/* X/Y resolution */
	int samplelen;		/* number of samples available or
				   WSMOUSE_CALIBCOORDS_RESET for raw mode */
	struct wsmouse_calibcoord {
		int rawx, rawy;	/* raw coordinate */
		int x, y;	/* translated coordinate */
	} samples[WSMOUSE_CALIBCOORDS_MAX];	/* sample coordinates */
};
#define	WSMOUSEIO_SCALIBCOORDS	_IOW('W', 36, struct wsmouse_calibcoords)
#define	WSMOUSEIO_GCALIBCOORDS	_IOR('W', 37, struct wsmouse_calibcoords)

/*
 * Display ioctls (64 - 95)
 */

/* Get display type */
#define	WSDISPLAYIO_GTYPE	_IOR('W', 64, u_int)
#define		WSDISPLAY_TYPE_UNKNOWN	0	/* unknown */
#define		WSDISPLAY_TYPE_PM_MONO	1	/* DEC [23]100 mono */
#define		WSDISPLAY_TYPE_PM_COLOR	2	/* DEC [23]100 color */
#define		WSDISPLAY_TYPE_CFB	3	/* DEC TC CFB (CX) */
#define		WSDISPLAY_TYPE_XCFB	4	/* DEC `maxine' onboard fb */
#define		WSDISPLAY_TYPE_MFB	5	/* DEC TC MFB (MX) */
#define		WSDISPLAY_TYPE_SFB	6	/* DEC TC SFB (HX) */
#define		WSDISPLAY_TYPE_ISAVGA	7	/* (generic) ISA VGA */
#define		WSDISPLAY_TYPE_PCIVGA	8	/* (generic) PCI VGA */
#define		WSDISPLAY_TYPE_TGA	9	/* DEC PCI TGA */
#define		WSDISPLAY_TYPE_SFBP	10	/* DEC TC SFB+ (HX+) */
#define		WSDISPLAY_TYPE_PCIMISC	11	/* (generic) PCI misc. disp. */
#define		WSDISPLAY_TYPE_NEXTMONO	12	/* NeXT mono display */
#define		WSDISPLAY_TYPE_PX	13	/* DEC TC PX */
#define		WSDISPLAY_TYPE_PXG	14	/* DEC TC PXG */
#define		WSDISPLAY_TYPE_TX	15	/* DEC TC TX */
#define		WSDISPLAY_TYPE_HPCFB	16	/* Handheld/PalmSize PC */
#define		WSDISPLAY_TYPE_VIDC	17	/* Acorn/ARM VIDC */
#define		WSDISPLAY_TYPE_SPX	18	/* DEC SPX (VS3100/VS4000) */
#define		WSDISPLAY_TYPE_GPX	19	/* DEC GPX (uVAX/VS2K/VS3100) */
#define		WSDISPLAY_TYPE_LCG	20	/* DEC LCG (VS4000) */
#define		WSDISPLAY_TYPE_VAX_MONO	21	/* DEC VS2K/VS3100 mono */
#define		WSDISPLAY_TYPE_SB_P9100	22	/* Tadpole SPARCbook P9100 */
#define		WSDISPLAY_TYPE_EGA	23	/* (generic) EGA */
#define		WSDISPLAY_TYPE_DCPVR	24	/* Dreamcast PowerVR */
#define		WSDISPLAY_TYPE_SUN24	25	/* Sun 24 bit framebuffers */
#define		WSDISPLAY_TYPE_SUNBW	26	/* Sun black and white fb */
#define		WSDISPLAY_TYPE_STI	27	/* HP STI framebuffers */
#define		WSDISPLAY_TYPE_SUNCG3	28	/* Sun cgthree */
#define		WSDISPLAY_TYPE_SUNCG6	29	/* Sun cgsix */
#define		WSDISPLAY_TYPE_SUNFFB	30	/* Sun creator FFB */
#define		WSDISPLAY_TYPE_SUNCG14	31	/* Sun cgfourteen */
#define		WSDISPLAY_TYPE_SUNCG2	32	/* Sun cgtwo */
#define		WSDISPLAY_TYPE_SUNCG4	33	/* Sun cgfour */
#define		WSDISPLAY_TYPE_SUNCG8	34	/* Sun cgeight */
#define		WSDISPLAY_TYPE_SUNTCX	35	/* Sun TCX */
#define		WSDISPLAY_TYPE_AGTEN	36	/* AG10E */
#define		WSDISPLAY_TYPE_XVIDEO	37	/* Xvideo */
#define		WSDISPLAY_TYPE_SUNCG12	38	/* Sun cgtwelve */
#define		WSDISPLAY_TYPE_MGX	39	/* SMS MGX */
#define		WSDISPLAY_TYPE_SB_P9000	40	/* Tadpole SPARCbook P9000 */
#define		WSDISPLAY_TYPE_RFLEX	41	/* RasterFlex series */
#define		WSDISPLAY_TYPE_LUNA	42	/* OMRON Luna */
#define		WSDISPLAY_TYPE_DVBOX	43	/* HP DaVinci */
#define		WSDISPLAY_TYPE_GBOX	44	/* HP Gatorbox */
#define		WSDISPLAY_TYPE_RBOX	45	/* HP Renaissance */
#define		WSDISPLAY_TYPE_HYPERION	46	/* HP Hyperion */
#define		WSDISPLAY_TYPE_TOPCAT	47	/* HP Topcat */
#define		WSDISPLAY_TYPE_PXALCD	48	/* PXALCD (Zaurus) */
#define		WSDISPLAY_TYPE_MAC68K	49	/* Generic mac68k framebuffer */
#define		WSDISPLAY_TYPE_SUNLEO	50	/* Sun ZX/Leo */
#define		WSDISPLAY_TYPE_TVRX	51	/* HP TurboVRX */
#define		WSDISPLAY_TYPE_CFXGA	52	/* CF VoyagerVGA */
#define		WSDISPLAY_TYPE_LCSPX	53	/* DEC LCSPX (VS4000) */
#define		WSDISPLAY_TYPE_GBE	54	/* SGI GBE frame buffer */
#define		WSDISPLAY_TYPE_LEGSS	55	/* DEC LEGSS (VS35x0) */
#define		WSDISPLAY_TYPE_IFB	56	/* Sun Expert3D{,-Lite} */
#define		WSDISPLAY_TYPE_RAPTOR	57	/* Tech Source Raptor */
#define		WSDISPLAY_TYPE_DL	58	/* DisplayLink DL-120/DL-160 */
#define		WSDISPLAY_TYPE_MACHFB	59	/* Sun PGX/PGX64 */
#define		WSDISPLAY_TYPE_GFXP	60	/* Sun PGX32 */
#define		WSDISPLAY_TYPE_RADEONFB	61	/* Sun XVR-100 */
#define		WSDISPLAY_TYPE_SMFB	62	/* SiliconMotion SM712 */
#define		WSDISPLAY_TYPE_SISFB	63	/* SiS 315 Pro */
#define		WSDISPLAY_TYPE_ODYSSEY	64	/* SGI Odyssey */

/* Basic display information.  Not applicable to all display types. */
struct wsdisplay_fbinfo {
	u_int	height;				/* height in pixels */
	u_int	width;				/* width in pixels */
	u_int	depth;				/* bits per pixel */
	u_int	cmsize;				/* color map size (entries) */
};
#define	WSDISPLAYIO_GINFO	_IOR('W', 65, struct wsdisplay_fbinfo)

/* Colormap operations.  Not applicable to all display types. */
struct wsdisplay_cmap {
	u_int	index;				/* first element (0 origin) */
	u_int	count;				/* number of elements */
	u_char	*red;				/* red color map elements */
	u_char	*green;				/* green color map elements */
	u_char	*blue;				/* blue color map elements */
};      
#define WSDISPLAYIO_GETCMAP	_IOW('W', 66, struct wsdisplay_cmap)
#define WSDISPLAYIO_PUTCMAP	_IOW('W', 67, struct wsdisplay_cmap)

/* Video control.  Not applicable to all display types. */
#define	WSDISPLAYIO_GVIDEO	_IOR('W', 68, u_int)
#define	WSDISPLAYIO_SVIDEO	_IOW('W', 69, u_int)
#define		WSDISPLAYIO_VIDEO_OFF	0	/* video off */
#define		WSDISPLAYIO_VIDEO_ON	1	/* video on */

/* Cursor control.  Not applicable to all display types. */
struct wsdisplay_curpos {			/* cursor "position" */
	u_int x, y;
};

struct wsdisplay_cursor {
	u_int	which;				/* values to get/set */
	u_int	enable;				/* enable/disable */
	struct wsdisplay_curpos pos;		/* position */
	struct wsdisplay_curpos hot;		/* hot spot */
	struct wsdisplay_cmap cmap;		/* color map info */
	struct wsdisplay_curpos size;		/* bit map size */
	u_char *image;				/* image data */
	u_char *mask;				/* mask data */
};
#define		WSDISPLAY_CURSOR_DOCUR		0x01	/* get/set enable */
#define		WSDISPLAY_CURSOR_DOPOS		0x02	/* get/set pos */
#define		WSDISPLAY_CURSOR_DOHOT		0x04	/* get/set hot spot */
#define		WSDISPLAY_CURSOR_DOCMAP		0x08	/* get/set cmap */
#define		WSDISPLAY_CURSOR_DOSHAPE	0x10	/* get/set img/mask */
#define		WSDISPLAY_CURSOR_DOALL		0x1f	/* all of the above */

/* Cursor control: get and set position */
#define	WSDISPLAYIO_GCURPOS	_IOR('W', 70, struct wsdisplay_curpos)
#define	WSDISPLAYIO_SCURPOS	_IOW('W', 71, struct wsdisplay_curpos)

/* Cursor control: get maximum size */
#define	WSDISPLAYIO_GCURMAX	_IOR('W', 72, struct wsdisplay_curpos)

/* Cursor control: get/set cursor attributes/shape */
#define	WSDISPLAYIO_GCURSOR	_IOWR('W', 73, struct wsdisplay_cursor)
#define	WSDISPLAYIO_SCURSOR	_IOW('W', 74, struct wsdisplay_cursor)

/* Display mode: Emulation (text) vs. Mapped (graphics) mode */
#define	WSDISPLAYIO_GMODE	_IOR('W', 75, u_int)
#define	WSDISPLAYIO_SMODE	_IOW('W', 76, u_int)
#define		WSDISPLAYIO_MODE_EMUL	0	/* emulation (text) mode */
#define		WSDISPLAYIO_MODE_MAPPED	1	/* mapped (graphics) mode */
#define		WSDISPLAYIO_MODE_DUMBFB	2	/* mapped (graphics) fb mode */

struct wsdisplay_font {
	char name[WSFONT_NAME_SIZE];
	int index;
	int firstchar, numchars;
	int encoding;
#define WSDISPLAY_FONTENC_ISO 0
#define WSDISPLAY_FONTENC_IBM 1
#define WSDISPLAY_FONTENC_PCVT 2
#define WSDISPLAY_FONTENC_ISO7 3 /* greek */
	u_int fontwidth, fontheight, stride;
#define WSDISPLAY_MAXFONTSZ	(512*1024)
	int bitorder, byteorder;
#define	WSDISPLAY_FONTORDER_KNOWN	0	/* i.e, no need to convert */
#define	WSDISPLAY_FONTORDER_L2R		1
#define	WSDISPLAY_FONTORDER_R2L		2
	void *cookie;
	void *data;
};
#define WSDISPLAYIO_LDFONT	_IOW ('W', 77, struct wsdisplay_font)
#define	WSDISPLAYIO_LSFONT	_IOWR('W', 78, struct wsdisplay_font)
#define	WSDISPLAYIO_DELFONT	_IOW ('W', 79, struct wsdisplay_font)
#define WSDISPLAYIO_USEFONT	_IOW ('W', 80, struct wsdisplay_font)

struct wsdisplay_burner {
	u_int	off;
	u_int	on;
	u_int	flags;
#define	WSDISPLAY_BURN_VBLANK	0x0001
#define	WSDISPLAY_BURN_KBD	0x0002
#define	WSDISPLAY_BURN_MOUSE	0x0004
#define	WSDISPLAY_BURN_OUTPUT	0x0008
};
#define	WSDISPLAYIO_SBURNER	_IOW('W', 81, struct wsdisplay_burner)
#define	WSDISPLAYIO_GBURNER	_IOR('W', 82, struct wsdisplay_burner) 

/*
 * XXX WARNING
 * XXX The following definitions are very preliminary and are likely
 * XXX to be changed without care about backwards compatibility!
 */
struct wsdisplay_addscreendata {
	int idx; /* screen index */
	char screentype[WSSCREEN_NAME_SIZE];
	char emul[WSEMUL_NAME_SIZE];
};
#define WSDISPLAYIO_ADDSCREEN	_IOW('W', 83, struct wsdisplay_addscreendata)

struct wsdisplay_delscreendata {
	int idx; /* screen index */
	int flags;
#define	WSDISPLAY_DELSCR_FORCE	0x01
#define	WSDISPLAY_DELSCR_QUIET	0x02
};
#define WSDISPLAYIO_DELSCREEN	_IOW('W', 84, struct wsdisplay_delscreendata)

#define WSDISPLAYIO_GETSCREEN	_IOWR('W', 85, struct wsdisplay_addscreendata)
#define	WSDISPLAYIO_SETSCREEN	_IOW('W', 86, u_int)

/* Display information: number of bytes per row, may be same as pixels */
#define	WSDISPLAYIO_LINEBYTES	_IOR('W', 95, u_int)

/* Mouse console support */
#define WSDISPLAYIO_WSMOUSED	_IOW('W', 88, struct wscons_event)

/* Misc control.  Not applicable to all display types. */
struct wsdisplay_param {
        int param;
#define	WSDISPLAYIO_PARAM_BACKLIGHT	1
#define	WSDISPLAYIO_PARAM_BRIGHTNESS	2
#define	WSDISPLAYIO_PARAM_CONTRAST	3
        int min, max, curval;
        int reserved[4];
};
#define	WSDISPLAYIO_GETPARAM	_IOWR('W', 89, struct wsdisplay_param)
#define	WSDISPLAYIO_SETPARAM	_IOWR('W', 90, struct wsdisplay_param)

#define WSDISPLAYIO_GPCIID	_IOR('W', 91, struct pcisel)

/* graphical mode control */

#define WSDISPLAYIO_DEPTH_1		0x1
#define WSDISPLAYIO_DEPTH_4		0x2
#define WSDISPLAYIO_DEPTH_8		0x4
#define WSDISPLAYIO_DEPTH_15		0x8
#define WSDISPLAYIO_DEPTH_16		0x10
#define WSDISPLAYIO_DEPTH_24_24		0x20
#define WSDISPLAYIO_DEPTH_24_32		0x40
#define WSDISPLAYIO_DEPTH_24 (WSDISPLAYIO_DEPTH_24_24|WSDISPLAYIO_DEPTH_24_32)

#define WSDISPLAYIO_GETSUPPORTEDDEPTH	_IOR('W', 92, unsigned int)

struct wsdisplay_gfx_mode {
	int width;
	int height;
	int depth;
};

#define WSDISPLAYIO_SETGFXMODE	_IOW('W', 92, struct wsdisplay_gfx_mode)

/* XXX NOT YET DEFINED */
/* Mapping information retrieval. */

/*
 * Mux ioctls (96 - 127)
 */

#define WSMUXIO_INJECTEVENT	_IOW('W', 96, struct wscons_event)
#define	WSMUX_INJECTEVENT	WSMUXIO_INJECTEVENT	/* XXX compat */

struct wsmux_device {
	int type;
#define WSMUX_MOUSE	1
#define WSMUX_KBD	2
#define WSMUX_MUX	3
	int idx;
};
#define WSMUXIO_ADD_DEVICE	_IOW('W', 97, struct wsmux_device)
#define	WSMUX_ADD_DEVICE	WSMUXIO_ADD_DEVICE	/* XXX compat */
#define WSMUXIO_REMOVE_DEVICE	_IOW('W', 98, struct wsmux_device)
#define	WSMUX_REMOVE_DEVICE	WSMUXIO_REMOVE_DEVICE	/* XXX compat */

#define WSMUX_MAXDEV 32
struct wsmux_device_list {
	int ndevices;
	struct wsmux_device devices[WSMUX_MAXDEV];
};
#define WSMUXIO_LIST_DEVICES	_IOWR('W', 99, struct wsmux_device_list)
#define	WSMUX_LIST_DEVICES	WSMUXIO_LIST_DEVICES	/* XXX compat */

#endif /* _DEV_WSCONS_WSCONSIO_H_ */
