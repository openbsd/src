/*	$OpenBSD: grfioctl.h,v 1.5 2001/09/13 13:20:22 jj Exp $	*/
/*	$NetBSD: grfioctl.h,v 1.13 1997/07/29 17:54:11 veego Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: grfioctl.h 1.1 90/07/09$
 *
 *	@(#)grfioctl.h	7.2 (Berkeley) 11/4/90
 */


/* these are changeable values, encapsulated in their own structure, so
   no the whole thing has to be copied when setting parameters. */
struct grfdyninfo {
	int	gdi_fbx;		/* frame buffer x offset */
	int	gdi_fby;		/* frame buffer y offset */
	int	gdi_dwidth;		/* displayed part width */
	int	gdi_dheight;		/* displayed part height */
	int	gdi_dx;			/* displayed part x offset */
	int	gdi_dy;			/* displayed part y offset */ 
};

struct	grfinfo {
	caddr_t	gd_regaddr;		/* control registers physaddr */
	int	gd_regsize;		/* control registers size */
	caddr_t	gd_fbaddr;		/* frame buffer physaddr */
	int	gd_fbsize;		/* frame buffer size */
	short	gd_colors;		/* number of colors */
	short	gd_planes;		/* number of planes */

	int	gd_fbwidth;		/* frame buffer width */
	int	gd_fbheight;		/* frame buffer height */

	struct grfdyninfo gd_dyn;	/* everything changable by GRFIOCSINFO */
/* compatibility... */
#define gd_fbx		gd_dyn.gdi_fbx
#define gd_fby		gd_dyn.gdi_fby
#define gd_dwidth 	gd_dyn.gdi_dwidth
#define gd_dheight	gd_dyn.gdi_dheight
#define gd_dx		gd_dyn.gdi_dx
#define gd_dy		gd_dyn.gdi_dy

	/* new for banked pager support */
	int	gd_bank_size;		/* size of a bank (or 0) */
};


/* video mode, should be display-independant, but it might need 
   modifications in the future to really become hardware-independant. */

struct grfvideo_mode {
	u_char	mode_num;		/* index in mode table */
	char	mode_descr[80];		/* description of mode */
	u_long	pixel_clock;		/* in Hz. */
	u_short	disp_width;		/* width of displayed video (incl overscan) */
	u_short	disp_height;		/* height "" */
	u_short	depth;			/* number of bitplanes resp. bits per pixel */
	u_short	hblank_start;
	u_short	hsync_start;		/* video-parameters, take care not to   */
	u_short	hsync_stop;		/* use parameters that violete specs of */
	u_short	htotal;			/* your monitor !                       */
	u_short	vblank_start;
	u_short	vsync_start;
	u_short	vsync_stop;
	u_short	vtotal;
	u_short	disp_flags;		/* GRF_FLAGS_xxxx */
};

/* values for grfvideo_mode->disp_flags */
#define	GRF_FLAGS_DEFAULT	0x00	/* default */
#define	GRF_FLAGS_DBLSCAN	0x01	/* doublescan */
#define	GRF_FLAGS_LACE		0x02	/* interlace */
#define	GRF_FLAGS_PHSYNC	0x04	/* +hsync */
#define	GRF_FLAGS_NHSYNC	0x08	/* -hsync */
#define	GRF_FLAGS_PVSYNC	0x10	/* +vsync */
#define	GRF_FLAGS_NVSYNC	0x20	/* -vsync */
#define	GRF_FLAGS_SYNC_ON_GREEN	0x40	/* sync-on-green */

/*
 * BSD ioctls
 */
#define OGRFIOCGINFO	0x40344700		/* to keep compat for a while... */
#define	GRFIOCGINFO	_IOR('G', 0, struct grfinfo) /* get info on device */
#define	GRFIOCON	_IO('G', 1)		/* turn graphics on */
#define	GRFIOCOFF	_IO('G', 2)		/* turn graphics off */
#define GRFIOCMAP	_IOWR('G', 5, int)	/* map in regs+framebuffer */
#define GRFIOCUNMAP	_IOW('G', 6, int)	/* unmap regs+framebuffer */

/* amiga addons */
	/* set info on device */
#define	GRFIOCSINFO	_IOW('G', 40, struct grfdyninfo)
	/* get video_mode. mode_num==0 gets current mode. */
#define GRFGETVMODE	_IOWR('G', 41, struct grfvideo_mode)
	/* set video_mode. */
#define GRFSETVMODE	_IOW('G', 42, int)
	/* get number of configured video modes */
#define GRFGETNUMVM	_IOR('G', 43, int)
	/* toggle between Cirrus and ECS, ill */
#define GRFTOGGLE	_IO('G', 44)
	/* load in a display mode */
#define GRFIOCSETMON	_IOW('G', 45, struct grfvideo_mode)

/*
 * generic framebuffer-related ioctls. These are somewhat
 * similar to SunOS fb-ioctls since I liked them reading
 * thru the X11-server code. 
 */

/*
 * colormap related information. Every grf has an associated
 * colormap. Depending on the capabilities of the hardware, more
 * or less of the information provided may be used.
 * Maxium value of "index" can be deduced from grfinfo->gd_colors.
 */
struct grf_colormap {
	u_int	index;		/* start at red[index],green[index],blue[index] */
	u_int	count;		/* till < red[index+count],... */
	u_char	*red;
	u_char	*green;
	u_char	*blue;
};

/* write the selected slots into the active colormap */
#define GRFIOCPUTCMAP	_IOW('G', 50, struct grf_colormap)

/* retrieve the selected slots from the active colormap */
#define GRFIOCGETCMAP	_IOWR('G', 51, struct grf_colormap)


/*
 * support a possibly available hardware sprite. calls just fail
 * if a given grf doesn't implement hardware sprites.
 */
struct grf_position {
	u_short x, y;		/* 0,0 is upper left corner */
};
#define GRFIOCSSPRITEPOS _IOW('G', 52, struct grf_position)
#define GRFIOCGSPRITEPOS _IOR('G', 53, struct grf_position)

struct grf_spriteinfo {
  u_short  set;
#define GRFSPRSET_ENABLE  (1<<0)
#define GRFSPRSET_POS	  (1<<1)
#define GRFSPRSET_HOT	  (1<<2)
#define GRFSPRSET_CMAP	  (1<<3)
#define GRFSPRSET_SHAPE	  (1<<4)
#define GRFSPRSET_ALL	  0x1f
	u_short  enable;	    /* sprite is displayed if == 1 */
	struct grf_position pos;  /* sprite location */
	struct grf_position hot;  /* sprite hot spot */
	struct grf_colormap cmap; /* colormap for the sprite. */
	struct grf_position size; /* x == width, y == height */
	u_char *image, *mask;	    /* sprite bitmap and mask */
};

#define GRFIOCSSPRITEINF _IOW('G', 54, struct grf_spriteinfo)
#define GRFIOCGSPRITEINF _IOR('G', 55, struct grf_spriteinfo)

/* get maximum sprite size hardware can display */
#define GRFIOCGSPRITEMAX _IOR('G', 56, struct grf_position)


/* support for a BitBlt operation. The op-codes are identical
   to X11 GCs */
#define	GRFBBOPclear		0x0	/* 0 */
#define GRFBBOPand		0x1	/* src AND dst */
#define GRFBBOPandReverse	0x2	/* src AND NOT dst */
#define GRFBBOPcopy		0x3	/* src */
#define GRFBBOPandInverted	0x4	/* NOT src AND dst */
#define	GRFBBOPnoop		0x5	/* dst */
#define GRFBBOPxor		0x6	/* src XOR dst */
#define GRFBBOPor		0x7	/* src OR dst */
#define GRFBBOPnor		0x8	/* NOT src AND NOT dst */
#define GRFBBOPequiv		0x9	/* NOT src XOR dst */
#define GRFBBOPinvert		0xa	/* NOT dst */
#define GRFBBOPorReverse	0xb	/* src OR NOT dst */
#define GRFBBOPcopyInverted	0xc	/* NOT src */
#define GRFBBOPorInverted	0xd	/* NOT src OR dst */
#define GRFBBOPnand		0xe	/* NOT src OR NOT dst */
#define GRFBBOPset		0xf	/* 1 */

struct grf_bitblt {
	u_short op;		/* see above */
	u_short src_x, src_y;	/* upper left corner of source-region */
	u_short dst_x, dst_y;	/* upper left corner of dest-region */
	u_short w, h;		/* width, height of region */
	u_short mask;		/* bitmask to apply */
};

#define GRFIOCBITBLT	_IOW('G', 57, struct grf_bitblt)

/* can't use IOCON/OFF because that would turn ite on */
#define GRFIOCBLANK	_IOW('G', 58, int)

#define GRFIOCBLANK_LIVE	1
#define GRFIOCBLANK_DARK	0
