/*	$NetBSD: grfabs_ccreg.h,v 1.6 1995/10/05 12:41:19 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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

#if ! defined (_GRFABS_CCREG_H)
#define _GRFABS_CCREG_H

typedef colormap_t *alloc_colormap_func (int);

typedef struct monitor_data {
    LIST_HEAD(modelist, display_mode) modes; /* a list of supported modes. */
    dmode_t *current_mode;
    u_long    flags;		  /* monitor flags. */
} mdata_t;

typedef struct display_mode_data {
    monitor_t  * monitor;	/* the monitor that owns this mode. */
    view_t * current_view;	/* current view to be displayed. */
    cop_t  **frames;
    u_short    hedley_current;	/* current hedley quadrent. */
    u_short    bplcon0;		/* bplcon0 data. */
    u_short    std_start_x;
    u_short    std_start_y;
#if defined (GRF_ECS) || defined (GRF_AGA)
    /* ECS registers. */
    u_short   beamcon0;
    u_short   hbstart;		/* no modes use the rest of these */
    u_short   hbstop;		/* ECS registers. */
    u_short   hsstart;
    u_short   hsstop;
    u_short   vbstart;
    u_short   vbstop;
    u_short   vsstart;
    u_short   vsstop;
#endif
    /* some limit stuff. */
    dimen_t   max_size;		/* largest fit. */
    dimen_t   min_size;		/* smallest fit. */
    u_short   min_depth;
    u_short   max_depth;
    u_long   flags;		/* mode specific flags. */
    use_colormap_func *use_colormap;
    get_colormap_func *get_colormap;
    alloc_colormap_func *alloc_colormap;
    display_view_func  *display_view;
    vbl_handler_func   *vbl_handler;	/* gets called every vertical blank. */
					/* when this is the current mode.*/
} dmdata_t;

enum dmode_flag_bits {
    DMB_INTERLACE,
    DMB_HEDLEY_EXP
};

enum dmode_flags {
    DMF_INTERLACE = 1 << DMB_INTERLACE,
    DMF_HEDLEY_EXP = 1 << DMB_HEDLEY_EXP
};

typedef struct view_data {
    dmode_t *mode;		/* the mode for this view */
    colormap_t *colormap;
    u_long    flags;		/* view specific flags. */
} vdata_t;

enum view_flag_bits {
    VB_DISPLAY,
};

enum view_flags {
    VF_DISPLAY = 1 << VB_DISPLAY,  /* set if view is being displayed */
};

/*
 *  This that are in grfabs_ccglb.c
 */
#if defined (GRF_A2024)
#  if defined (GRF_PAL)
extern cop_t std_pal_a2024_copper_list[];
extern int std_pal_a2024_copper_list_len;
extern int std_pal_a2024_copper_list_size;
#  endif
#  if defined (GRF_NTSC)
extern cop_t std_a2024_copper_list[];
extern int std_a2024_copper_list_len;
extern int std_a2024_copper_list_size;
#  endif
extern cop_t std_dlace_copper_list[];
extern int std_dlace_copper_list_len;
extern int std_dlace_copper_list_size;

extern u_short a2024_color_value_line0[4];
extern u_short a2024_color_value_line1[4];
#endif /* GRF_A2024 */

#if defined(GRF_AGA)
extern cop_t aga_copper_list[];
extern int aga_copper_list_len;
extern int aga_copper_list_size;
#endif

extern cop_t std_copper_list[];
extern int std_copper_list_len;
extern int std_copper_list_size;
extern monitor_t *cc_monitor;
extern u_short cc_default_colors[32];
extern u_short cc_a2024_default_colors[4];

/*
 * Misc macros, defines and enums.
 */

#define MDATA(m) ((mdata_t *)(m->data))
#define DMDATA(d) ((dmdata_t *)(d->data))
#define VDATA(v) ((vdata_t *)(v->data))
#define RWDATA(r) ((rwdata_t *)(r->data))

#if defined (GRF_ECS) || defined (GRF_AGA)
#define CALC_DIWHIGH(hs, vs, he, ve) \
        ((u_short)((he&0x100)<<5)|(ve&0x700)|((hs&0x100)>>3)|((vs&0x700)>>8))
#define USE_CON3 0x0001
#else
#define USE_CON3 0x0
#endif

enum lace_frame_numbers {
    F_LACE_LONG,
    F_LACE_SHORT,
    F_LACE_STORE_LONG,
    F_LACE_STORE_SHORT,
    F_LACE_TOTAL
};

enum frame_numbers {
    F_LONG,
    F_STORE_LONG,
    F_TOTAL
};

#if defined (GRF_A2024)

/*
 * Defines macros and enums for A2024 hedley expansion
 */

enum quad_frame_numbers {
    F_QD_QUAD0, F_QD_QUAD1, F_QD_QUAD2, F_QD_QUAD3,
    F_QD_STORE_QUAD0, F_QD_STORE_QUAD1, F_QD_STORE_QUAD2, F_QD_STORE_QUAD3,
    F_QD_TOTAL
};

/* -------
 * |0 |1 |
 * |------
 * |2 |3 |
 * -------
 */

#define QUAD0_ID 0x0001
#define QUAD1_ID 0x00f1
#define QUAD2_ID 0x0f01
#define QUAD3_ID 0x0ff1
	
#define HALF_2024_LINE (512>>3)
#define DIGITAL_RED 0x0800
#define DIGITAL_GREEN 0x0080
#define DIGITAL_BLUE 0x0008
#define DIGITAL_INTENSE 0x0001

#define A2024_L0_BLACK  (0)
#define A2024_L0_DGREY  (DIGITAL_BLUE)
#define A2024_L0_LGREY  (DIGITAL_RED)
#define A2024_L0_WHITE  (DIGITAL_RED|DIGITAL_BLUE)

#define A2024_L1_BLACK  (0)
#define A2024_L1_DGREY  (DIGITAL_INTENSE)
#define A2024_L1_LGREY  (DIGITAL_GREEN)
#define A2024_L1_WHITE  (DIGITAL_GREEN|DIGITAL_INTENSE)

#define A2024_L0_INDEX(color_reg) (((0x4&color_reg)>>1)|(0x1&color_reg))
#define A2024_L1_INDEX(color_reg) (((0x8&color_reg)>>2)|((0x2&color_reg)>>1))
#define A2024_CM_TO_CR(cm,rn) \
	(a2024_color_value_line0[0x3 & (cm)->entry[A2024_L0_INDEX(rn)]] |\
	 a2024_color_value_line1[0x3 & (cm)->entry[A2024_L1_INDEX(rn)]])
#endif /* GRF_A2024 */

/*
 *  Misc defined values for custom chips.
 */

/* ECS stuff */
#define VARVBLANK	0x1000	/* Variable vertical blank enable */
#define LOLDIS		0x0800	/* long line disable */
#define CSCBLANKEN	0x0400	/* redirect composite sync */
#define VARVSYNC	0x0200	/* Variable vertical sync enable */
#define VARHSYNC	0x0100	/* Variable horizontal sync enable */
#define VARBEAM	0x0080	/* variable beam counter enable */
#define DISPLAYDUAL	0x0040	/* use UHRES pointer and standard pointers */
#define DISPLAYPAL	0x0020	/* set decodes to generate PAL display */
#define VARCSYNC	0x0010	/* Variable composite sync enable */
#define CSBLANK	0x0008	/* Composite blank out to CSY* pin */
#define CSYNCTRUE	0x0004	/* composite sync true signal */
#define VSYNCTRUE	0x0002	/* vertical sync true */
#define HSYNCTRUE	0x0001	/* horizontal sync true */

/* new defines for bplcon0 */
#define USE_BPLCON3	1

/* new defines for bplcon2 */
#define BPLCON2_ZDCTEN		(1<<10) /* colormapped genlock bit */
#define BPLCON2_ZDBPEN		(1<<11) /* use bitplane as genlock bits */
#define BPLCON2_ZDBPSEL0	(1<<12) /* three bits to select one */
#define BPLCON2_ZDBPSEL1	(1<<13) /* of 8 bitplanes in */
#define BPLCON2_ZDBPSEL2	(1<<14) /* ZDBPEN genlock mode */

/* defines for bplcon3 register */
#define BPLCON3_EXTBLNKEN	(1<<0)	/* external blank enable */
#define BPLCON3_EXTBLKZD	(1<<1)	/* external blank ored into trnsprncy */
#define BPLCON3_ZDCLKEN	(1<<2)	/* zd pin outputs a 14mhz clock*/
#define BPLCON3_BRDNTRAN	(1<<4)	/* border is opaque */
#define BPLCON3_BRDNBLNK	(1<<5)	/* border is opaque */

/* mixture of stuff. */
#define	STANDARD_NTSC_ROWS	262
#define	STANDARD_PAL_ROWS	312
#define	STANDARD_COLORCLOCKS	226
#define	STANDARD_DENISE_MAX	455
#define	STANDARD_DENISE_MIN	93
#define	STANDARD_NTSC_BEAMCON	( 0x0000 )
#define	STANDARD_PAL_BEAMCON	( DISPLAYPAL )
#define	SPECIAL_BEAMCON	( VARVBLANK | LOLDIS | VARVSYNC | VARHSYNC | VARBEAM | CSBLANK | VSYNCTRUE)

#define	MIN_NTSC_ROW	21
#define	MIN_PAL_ROW	29
#define	STANDARD_VIEW_X	0x81
#define	STANDARD_VIEW_Y	0x2C
#define	STANDARD_HBSTRT	0x06
#define	STANDARD_HSSTRT	0x0B
#define	STANDARD_HSSTOP	0x1C
#define	STANDARD_HBSTOP	0x2C
#define	STANDARD_VBSTRT	0x0122
#define	STANDARD_VSSTRT	0x02A6
#define	STANDARD_VSSTOP	0x03AA
#define	STANDARD_VBSTOP	0x1066

/*
 * Prototypes
 */

#if defined (__STDC__)
/* monitor functions */
monitor_t *cc_init_monitor(void);
void monitor_vbl_handler(monitor_t * m);
dmode_t *get_current_mode(void);
dmode_t *get_next_mode(dmode_t * d);
dmode_t *get_best_mode(dimen_t * size, u_char depth);
bmap_t *alloc_bitmap(u_short width, u_short height, u_short depth, u_short flags);
void free_bitmap(bmap_t * bm);
void cc_load_mode(dmode_t * d);
int cc_init_modes(void);
/* mode functions */
monitor_t *cc_get_monitor(dmode_t * d);
view_t *cc_get_current_view(dmode_t * d);
view_t *cc_alloc_view(dmode_t * mode, dimen_t * dim, u_char depth);
colormap_t *cc_alloc_colormap(int depth);
int cc_colormap_checkvals(colormap_t * vcm, colormap_t * cm, int use);
int cc_get_colormap(view_t * v, colormap_t * cm);
int cc_use_colormap(view_t * v, colormap_t * cm);

#  if defined (GRF_A2024)
colormap_t *cc_a2024_alloc_colormap(int depth);
int cc_a2024_get_colormap(view_t * v, colormap_t * cm);
int cc_a2024_use_colormap(view_t * v, colormap_t * cm);
#  endif /* GRF_2024 */

void cc_mode_vbl_handler(dmode_t * d);
void cc_lace_mode_vbl_handler(dmode_t * d);
/* view functions */
void cc_init_view(view_t * v, bmap_t * bm, dmode_t * mode, box_t * dbox);
void cc_free_view(view_t * v);
void cc_remove_view(view_t * v);
dmode_t * cc_get_display_mode(view_t * v);

#  if defined (GRF_NTSC)
dmode_t *cc_init_ntsc_hires(void);
void display_hires_view(view_t * v);
dmode_t *cc_init_ntsc_hires_lace(void);
void display_hires_lace_view(view_t * v);

#    if defined (GRF_A2024)
dmode_t *cc_init_ntsc_hires_dlace(void);
void display_hires_dlace_view(view_t * v);
dmode_t *cc_init_ntsc_a2024(void);
void display_a2024_view(view_t * v);
void a2024_mode_vbl_handler(dmode_t * d);
#    endif /* GRF_A2024 */

#    if defined (GRF_AGA)
dmode_t *cc_init_ntsc_aga(void);
void display_aga_view(view_t * v);
#    endif /* GRF_AGA */
#  endif /* GRF_NTSC */

#  if defined (GRF_PAL)
dmode_t *cc_init_pal_hires(void);
void display_pal_hires_view(view_t * v);
dmode_t *cc_init_pal_hires_lace(void);
void display_pal_hires_lace_view(view_t * v);

#    if defined (GRF_A2024)
dmode_t *cc_init_pal_hires_dlace(void);
void display_pal_hires_dlace_view(view_t * v);

dmode_t *cc_init_pal_a2024(void);
void display_pal_a2024_view(view_t * v);
void pal_a2024_mode_vbl_handler(dmode_t * d);
#    endif /* GRF_A2024 */

#    if defined (GRF_AGA)
dmode_t *cc_init_pal_aga(void);
void display_pal_aga_view(view_t * v);
#    endif /* GRF_AGA */
#  endif /* GRF_PAL */
#endif /* __STDC__ */


#endif /* _GRFABS_CCABS_H */
