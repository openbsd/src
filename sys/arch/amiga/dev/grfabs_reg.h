/*	$OpenBSD: grfabs_reg.h,v 1.2 1996/05/02 06:43:58 niklas Exp $	*/
/*	$NetBSD: grfabs_reg.h,v 1.5 1996/04/21 21:11:31 veego Exp $	*/

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

#if ! defined (_GRFABS_REG_H)
#define _GRFABS_REG_H

struct point {
    long x;
    long y;
};
typedef struct point point_t;

struct dimension {
    u_long width;
    u_long height;
};
typedef struct dimension dimen_t;

struct box {
    long x;
    long y;
    u_long width;
    u_long height;
};
typedef struct box box_t;

struct rectangle {
    long left;
    long top;
    long right;
    long bottom;
};

typedef struct rectangle rect_t;

typedef struct bitmap bmap_t;
typedef struct colormap colormap_t;
typedef struct view view_t;
typedef struct display_mode dmode_t;
typedef struct monitor monitor_t;

LIST_HEAD(monitor_list, monitor);
extern struct monitor_list *monitors;

/*
 * Bitmap stuff.
 */

/*
 * Note structure is 5 long words big.  This may come in handy for
 * contiguous allocations 
 * 
 * Please do fill in everything correctly this is the main input for
 * all other programs.  In other words all problems with RTG start here.
 * If you do not mimic everyone else exactly problems will appear.
 * If you need a template look at alloc_bitmap() in grf_cc.c.
 *
 * WARNING: the plane array is only for convience, all data for bitplanes 
 *	MUST be contiguous.  This is for mapping purposes.  The reason
 *	for the plane pointers and row_mod is to support interleaving
 *	on monitors that wish to support this. 
 *	
 * 2nd Warning: Also don't get funky with these pointers you are expected
 *	to place the start of mappable plane data in ``hardware_address'',
 *	``hardware_address'' is the only thing that /dev/view checks and it
 *	expects the planes to follow with no padding in between.  If you have
 *	special alignment requirements make use of the given fields
 *	so that the entire contiguous plane data is exactly:
 *	bytes_per_row*height*depth long starting at the physical address
 *	contained within hardware_address.
 *	
 * Final Warning: Plane data must begin on a PAGE address and the allocation
 *	must be ``n'' PAGES big do to mapping requirements (otherwise the
 *	user could write over non-allocated memory.
 *	
 */
struct bitmap {
    u_short   bytes_per_row;	  /* number of bytes per display row. */
    u_short   row_mod;		  /* number of bytes to reach next row. */
    u_short   rows;		  /* number of display rows. */
    u_short   depth;		  /* depth of bitmap. */
    u_short   flags;		  /* flags. */
    u_short   pad;
    u_char  *blit_temp;		  /* private monitor buffer. */
    u_char **plane;		  /* plane data for bitmap. */
    u_char  *hardware_address;	  /* mappable bitplane pointer. */
};

enum bitmap_flag_bits {
    BMB_CLEAR,			  /* init only. */
    BMB_INTERLEAVED,		  /* init/read. */
    BMB_ALIGN64,		  /* init/read. */
};

enum bitmap_flags {
    BMF_CLEAR = 1 << BMB_CLEAR,			  /* init only. */
    BMF_INTERLEAVED = 1 << BMB_INTERLEAVED,	  /* init/read. */
    BMF_ALIGN64 = 1 << BMB_ALIGN64		  /* init/read. */
};

/* Use these macros to find misc. sizes of actual bitmap */
#define BM_WIDTH(b)	((b)->bytes_per_row << 3)
#define BM_HEIGHT(b)	((b)->rows)
#define BM_ROW(b,p,l) \
    ((b)->plane[p] + (((b)->bytes_per_row + (b)->row_mod) * l))

/*
 * Colormap stuff.
 */

/*
 * valid masks are a bitfield of zeros followed by ones that indicate 
 * which mask are valid for each component.  The ones and zeros will 
 * be contiguous so adding one to this value yields the number of
 * levels for that component. 
 * -ch 
 */

struct colormap {
    u_char type;	/* what type of entries these are. */
    union {
        /* CM_GREYSCALE */
        u_char grey;
#define grey_mask  valid_mask.grey
        /* CM_COLOR */
        struct {
            u_char red;
#define red_mask   valid_mask.rgb_mask.red
            u_char green;
#define green_mask valid_mask.rgb_mask.green
            u_char blue;
#define blue_mask  valid_mask.rgb_mask.blue
        } rgb_mask;
    } valid_mask;
    u_short first;	/* what color register does entry[0] refer to. */
    u_short size;	/* number of entries */
    u_long *entry;	/* the table of actual color values. */
};

enum colormap_type {
    CM_MONO,		/* only on or off allowed */
    CM_GREYSCALE,	/* grey vals. */
    CM_COLOR		/* RGB vals. */
};

#define CM_FIXVAL(x) (0xff & (x))

/* these macros are for creating entries */
#define MAKE_COLOR_ENTRY(r,g,b) \
    (CM_FIXVAL(r) << 16 | CM_FIXVAL(g) << 8 | CM_FIXVAL(b))
#define MAKE_MONO_ENTRY(x)	((x) ? 1 : 0)
#define MAKE_GREY_ENTRY(l)	CM_FIXVAL(l)

#define CM_LTOW(v) \
    (((0x000F0000 & (v)) >> 8) | ((0x00000F00 & (v)) >> 4) | (0xF & (v)))
#define CM_WTOL(v) \
    (((0xF00 & (v)) << 8) | ((0x0F0 & (v)) << 4) | (0xF & (v)))

/*
 * View stuff.
 */
typedef void remove_view_func (view_t *v);                               
typedef void free_view_func (view_t *v);                               
typedef void display_view_func (view_t *v);             
typedef dmode_t *get_mode_func (view_t *v);    
typedef int get_colormap_func (view_t *v, colormap_t *);
typedef int use_colormap_func (view_t *v, colormap_t *);

struct view {
    bmap_t  *bitmap;			/* bitmap. */
    box_t    display;			/* viewable area. */
    void    *data;			/* view specific data. */

    /* functions */
    display_view_func *display_view;	/* make this view active */
    remove_view_func  *remove_view;	/* remove this view if active */
    free_view_func    *free_view;	/* free this view */
    get_mode_func     *get_display_mode;/* get the mode this view belongs to */
    get_colormap_func *get_colormap;	/* get a color map for registers */
    use_colormap_func *use_colormap;	/* use color map to load registers */
};

#define VDISPLAY_LINE(v, p, l) ((v)->bitmap->plane[(p)] +\
	(((v)->bitmap->bytes_per_row + (v)->bitmap->row_mod) * l))

/*
 * Mode stuff
 */

typedef view_t *alloc_view_func (dmode_t *mode, dimen_t *dim, u_char depth);
typedef view_t *get_current_view_func (dmode_t *);
typedef monitor_t  *get_monitor_func (dmode_t *);
    
struct display_mode {
    LIST_ENTRY(display_mode) link;
    u_char    *name;			/* logical name for mode. */
    dimen_t    nominal_size;		/* best fit. */
    void      *data;			/* mode specific flags. */
    alloc_view_func       *alloc_view;	/* allocate a view for this mode. */
    get_current_view_func *get_current_view;	/* get active view. */
    get_monitor_func      *get_monitor;	/* get monitor that mode belongs to */
};

/*
 * Monitor stuff.
 */
typedef void     vbl_handler_func (void *);
typedef dmode_t *get_next_mode_func (dmode_t *);
typedef dmode_t *get_current_mode_func (void);
typedef dmode_t *get_best_mode_func (dimen_t *size, u_char depth);
typedef bmap_t *alloc_bitmap_func (u_short w, u_short h, u_short d, u_short f);
typedef void    free_bitmap_func (bmap_t *bm);

struct monitor {
    LIST_ENTRY(monitor) link; /* a link into the database. */
    u_char     *name;	/* a logical name for this monitor. */
    void       *data;	/* monitor specific data. */
    get_current_mode_func *get_current_mode;
    vbl_handler_func	*vbl_handler;	/* called on every vbl if not NULL */
    get_next_mode_func	*get_next_mode;	/* return next mode in list */
    get_best_mode_func	*get_best_mode; /* return mode that best fits */
    
    alloc_bitmap_func	*alloc_bitmap;
    free_bitmap_func	*free_bitmap;
};

/*
 * Misc draw related macros.
 */

#define BOX_2_RECT(b,r) do { \
    (r)->left = (b)->x; (r)->top = (b)->y; \
    (r)->right = (b)->x + (b)->width -1; \
    (r)->bottom = (b)->y + (b)->height -1; \
    } while (0)

#define RECT_2_BOX(r,b) do { \
    (b)->x = (r)->left; \
    (b)->y = (r)->top; \
    (b)->width = (r)->right - (r)->left +1; \
    (b)->height = (r)->bottom - (r)->top +1; \
    } while(0)

#define INIT_BOX(b,xx,yy,ww,hh) do{(b)->x = xx; (b)->y = yy; (b)->width = ww; (b)->height = hh;}while (0)
#define INIT_RECT(rc,l,t,r,b) do{(rc)->left = l; (rc)->right = r; (rc)->top = t; (rc)->bottom = b;}while (0)
#define INIT_POINT(p,xx,yy) do {(p)->x = xx; (p)->y = yy;} while (0)
#define INIT_DIM(d,w,h) do {(d)->width = w; (d)->height = h;} while (0)


/*
 * Prototypes
 */

/* views */
view_t * grf_alloc_view __P((dmode_t *d, dimen_t *dim, u_char depth));
void grf_display_view __P((view_t *v));
void grf_remove_view __P((view_t *v));
void grf_free_view __P((view_t *v));
dmode_t *grf_get_display_mode __P((view_t *v));
int grf_get_colormap __P((view_t *v, colormap_t *cm));
int grf_use_colormap __P((view_t *v, colormap_t *cm));

/* modes */
view_t *grf_get_current_view __P((dmode_t *d));
monitor_t *grf_get_monitor __P((dmode_t *d));

/* monitors */
dmode_t * grf_get_next_mode __P((monitor_t *m, dmode_t *d));
dmode_t * grf_get_current_mode __P((monitor_t *));
dmode_t * grf_get_best_mode __P((monitor_t *m, dimen_t *size, u_char depth));
bmap_t  * grf_alloc_bitmap __P((monitor_t *m, u_short w, u_short h,
				u_short d, u_short f));
void grf_free_bitmap __P((monitor_t *m, bmap_t *bm));

int grfcc_probe __P((void));

#endif /* _GRFABS_REG_H */
