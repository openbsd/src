/*	$NetBSD: grfabs_reg.h,v 1.5 1995/09/04 19:41:41 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman
 * Copyright (c) 1994 Christian E. Hopps
 * All rights reserved.
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
 *      This product includes software developed by Christian E. Hopps.
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

#ifndef _GRFABS_REG_H
#define _GRFABS_REG_H

struct point {
    long	x;
    long	y;
};
typedef struct point point_t;

struct dimension {
    u_long	width;
    u_long	height;
};
typedef struct dimension dimen_t;

struct box {
    long	x;
    long	y;
    u_long	width;
    u_long	height;
};
typedef struct box box_t;

struct rectangle {
    long	left;
    long	top;
    long	right;
    long	bottom;
};
typedef struct rectangle rect_t;

typedef struct bitmap bmap_t;
typedef struct colormap colormap_t;
typedef struct display_mode dmode_t;

/*
 * View stuff.
 */

struct view {
    bmap_t	*bitmap;	/* bitmap.				*/
    box_t	display;	/* viewable area.			*/
    dmode_t	*mode;		/* the mode for this view		*/
    colormap_t	*colormap;	/* the colormap for this view		*/
    int		flags;
};
typedef struct view view_t;

/* View-flags: */
#define	VF_DISPLAY	1	/* view is on the air now		*/

/*
 * Bitmap stuff.
 */
struct bitmap {
    u_short	bytes_per_row;	/* number of bytes per display row.	*/
    u_short	rows;		/* number of display rows.		*/
    u_short	depth;		/* depth of bitmap.			*/
    u_char	*plane;		/* plane data for bitmap.		*/
    u_char	*hw_address;	/* mappable bitplane pointer.		*/
};

/*
 * Colormap stuff.
 */
struct colormap {
    u_char	type;		/* what type of entries these are.	*/
    union {
        u_char	grey;		/* CM_GREYSCALE				*/
        struct {		/* CM_COLOR				*/
            u_char	red;
            u_char	green;
            u_char	blue;
        } rgb_mask;
    } valid_mask;
    u_short	first;		/* color register entry[0] refers to	*/
    u_short	size;		/* number of entries			*/
    u_long	*entry;		/* the table of actual color values	*/
};

/*
 * Mask short-hands
 */
#define grey_mask  valid_mask.grey
#define red_mask   valid_mask.rgb_mask.red
#define green_mask valid_mask.rgb_mask.green
#define blue_mask  valid_mask.rgb_mask.blue

enum colormap_type {
	CM_MONO,		/* only on or off allowed		*/
	CM_GREYSCALE,		/* grey values				*/
	CM_COLOR		/* RGB values				*/
};

#define	MAX_CENTRIES		256		/* that all there is	*/
/*
 * Create a colormap entry
 */
#define	MAKE_COLOR_ENTRY(r,g,b)	(((r & 0xff)<<16)|((g & 0xff)<<8)|(b & 0xff))
#define MAKE_MONO_ENTRY(x)	((x) ? 1 : 0)
#define MAKE_GREY_ENTRY(l)	(l & 0xff)

#define CM_L2TT(v) \
    (((0x000f0000 & (v)) >> 8) | ((0x00000f00 & (v)) >> 4) |\
      (0x0000000f & (v)))
#define CM_TT2L(v) \
    ((((0x00000f00 & (v)) * 0xff / 0xf) << 8) |\
     (((0x000000f0 & (v)) * 0xff / 0xf) << 4) |\
       (0x0000000f & (v)) * 0xff / 0xf)
#define CM_L2FAL(v) \
    (((0x003f0000 & (v)) << 10) | ((0x00003f00 & (v)) << 10) |\
      (0x0000003f & (v)) << 2)
#define CM_FAL2L(v) \
    (((((0xfc000000 & (v)) >> 10) * 0xff / 0x3f) & 0x00ff0000) |\
     ((((0x00fc0000 & (v)) >> 10) * 0xff / 0x3f) & 0x0000ff00) |\
       ((0x000000fc & (v)) >>  2) * 0xff / 0x3f)
#define CM_L2ST(v) \
    (((0x000e0000 & (v)) >> 9) | ((0x00000e00 & (v)) >> 5) |\
      (0x0000000e & (v)) >> 1)
#define CM_ST2L(v) \
    (((((0x00000700 & (v)) * 0xff / 0x7) << 8) & 0x00ff0000) |\
     ((((0x00000070 & (v)) * 0xff / 0x7) << 4) & 0x0000ff00) |\
        (0x00000007 & (v)) * 0xff / 0x7)

struct grfabs_sw {
	void	 (*display_view) __P((view_t*));
	view_t	* (*alloc_view) __P((dmode_t *, dimen_t *, u_char));
	void	 (*free_view) __P((view_t *));
	void	 (*remove_view) __P((view_t *));
	int 	 (*use_colormap) __P((view_t *, colormap_t *));
};
	
/* display mode */
struct display_mode {
    LIST_ENTRY(display_mode)	link;
    u_char			*name;		/* logical name for mode. */
    dimen_t			size;		/* screen size		  */
    u_char			depth;		/* screen depth		  */
    u_short			vm_reg;		/* video mode register	  */
    struct grfabs_sw		*grfabs_funcs;	/* hardware switch table  */
    view_t			*current_view;	/* view displaying me	  */
};

/*
 * Definition of available graphic mode list.
 */
typedef LIST_HEAD(modelist, display_mode) MODES;

/*
 * Misc draw related macros.
 */
#define INIT_BOX(b,xx,yy,ww,hh)						\
	do {								\
		(b)->x = xx;						\
		(b)->y = yy;						\
		(b)->width = ww;					\
		(b)->height = hh;					\
	} while(0)


/*
 * Common variables
 */
extern view_t		gra_con_view;
extern colormap_t	gra_con_cmap;
extern long		gra_con_colors[MAX_CENTRIES];
extern u_long		gra_def_color16[16];

/*
 * Prototypes:
 */
#ifdef FALCON_VIDEO
void	falcon_probe_video __P((MODES *));
#endif /* FALCON_VIDEO */
#ifdef TT_VIDEO
void	tt_probe_video __P((MODES *));
#endif /* TT_VIDEO */

view_t	*grf_alloc_view __P((dmode_t *d, dimen_t *dim, u_char depth));
dmode_t	*grf_get_best_mode __P((dimen_t *dim, u_char depth));
void	grf_display_view __P((view_t *v));
void	grf_remove_view __P((view_t *v));
void	grf_free_view __P((view_t *v));
int	grf_get_colormap __P((view_t *, colormap_t *));
int	grf_use_colormap __P((view_t *, colormap_t *));

#endif /* _GRFABS_REG_H */
