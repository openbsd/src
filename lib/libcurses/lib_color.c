
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/* lib_color.c 
 *  
 * Handles color emulation of SYS V curses
 *
 */

#include "curses.priv.h"
#include <stdlib.h>
#include <string.h>
#include "term.h"

int COLOR_PAIRS;
int COLORS;
unsigned char *color_pairs;

typedef struct
{
    short red, green, blue;
}
color_t;
static color_t	*color_table;

static const color_t cga_palette[] =
{
    /*  R	G	B */
	{0,	0,	0},	/* COLOR_BLACK */
	{1000,	0,	0},	/* COLOR_RED */
	{0,	1000,	0},	/* COLOR_GREEN */
	{1000,	1000,	0},	/* COLOR_YELLOW */
	{0,	0,	1000},	/* COLOR_BLUE */
	{1000,	0,	1000},	/* COLOR_MAGENTA */
	{0,	1000,	1000},	/* COLOR_CYAN */
	{1000,	1000,	1000},	/* COLOR_WHITE */
};
static const color_t hls_palette[] =
{
    /*  H	L	S */
	{0,	0,	0},	/* COLOR_BLACK */
	{120,	50,	100},	/* COLOR_RED */
	{240,	50,	100},	/* COLOR_GREEN */
	{180,	50,	100},	/* COLOR_YELLOW */
	{330,	50,	100},	/* COLOR_BLUE */
	{60,	50,	100},	/* COLOR_MAGENTA */
	{300,	50,	100},	/* COLOR_CYAN */
	{0,	50,	100},	/* COLOR_WHITE */
};

int start_color(void)
{
	T(("start_color() called."));

#ifdef orig_pair
	if (orig_pair != NULL)
	{
		TPUTS_TRACE("orig_pair");
		putp(orig_pair);
	}
#endif /* orig_pair */
#ifdef orig_colors
	if (orig_colors != NULL)
	{
		TPUTS_TRACE("orig_colors");
		putp(orig_colors);
	}
#endif /* orig_colors */
#if defined(orig_pair) && defined(orig_colors)
	if (!orig_pair && !orig_colors)
		return ERR;
#endif /* defined(orig_pair) && defined(orig_colors) */
	if (max_pairs != -1)
		COLOR_PAIRS = max_pairs;
	else
		return ERR;
	color_pairs = calloc((unsigned int)max_pairs, sizeof(char));
	if (max_colors != -1)
		COLORS = max_colors;
	else
		return ERR;
	SP->_coloron = 1;

#ifdef hue_lightness_saturation
	color_table = malloc(sizeof(color_t) * COLORS);
	if (hue_lightness_saturation)
	    memcpy(color_table, hls_palette, sizeof(color_t) * COLORS);
	else
#endif /* hue_lightness_saturation */
	    memcpy(color_table, cga_palette, sizeof(color_t) * COLORS);

	if (orig_colors)
	{
	    TPUTS_TRACE("orig_colors");
	    putp(orig_colors);
	}

	T(("started color: COLORS = %d, COLOR_PAIRS = %d", COLORS, COLOR_PAIRS));

	return OK;
}

#ifdef hue_lightness_saturation
static void rgb2hls(short r, short g, short b, short *h, short *l, short *s)
/* convert RGB to HLS system */
{
    short min, max, t;

    if ((min = g < r ? g : r) > b) min = b;
    if ((max = g > r ? g : r) < b) max = b;

    /* calculate lightness */
    *l = (min + max) / 20;

    if (min == max)		/* black, white and all shades of gray */
    {
	*h = 0;
	*s = 0;
	return;
    }

    /* calculate saturation */
    if (*l < 50)
	*s = ((max - min) * 100) / (max + min);
    else *s = ((max - min) * 100) / (2000 - max - min);

    /* calculate hue */
    if (r == max)
	t = 120 + ((g - b) * 60) / (max - min);
    else
	if (g == max)
	    t = 240 + ((b - r) * 60) / (max - min);
	else
	    t = 360 + ((r - g) * 60) / (max - min);

    *h = t % 360;
}
#endif /* hue_lightness_saturation */

int init_pair(short pair, short f, short b)
{
	T(("init_pair( %d, %d, %d )", pair, f, b));

	if ((pair < 1) || (pair >= COLOR_PAIRS))
		return ERR;
	if ((f  < 0) || (f >= COLORS) || (b < 0) || (b >= COLORS))
		return ERR;

	/* 
	 * FIXME: when a pair's content is changed, replace its colors
	 * (if pair was initialized before a screen update is performed
	 * replacing original pair colors with the new ones)
	 */

	color_pairs[pair] = ( (f & 0x0f) | (b & 0x0f) << 4 );

	if (initialize_pair)
	{
	    const color_t	*tp = hue_lightness_saturation ? hls_palette : cga_palette;

	    T(("initializing pair: pair = %d, fg=(%d,%d,%d), bg=(%d,%d,%d)\n",
	       pair,
	       tp[f].red, tp[f].green, tp[f].blue,
	       tp[b].red, tp[b].green, tp[b].blue));

	    if (initialize_pair)
	    {
		TPUTS_TRACE("initialize_pair");
		putp(tparm(initialize_pair,
			    pair,
			    tp[f].red, tp[f].green, tp[f].blue,
			    tp[b].red, tp[b].green, tp[b].blue));
	    }		
	}

	return OK;
}

int init_color(short color, short r, short g, short b)
{
#ifdef initialize_color
	if (initialize_color == NULL)
		return ERR;
#endif /* initialize_color */

	if (color < 0 || color >= COLORS)
		return ERR;
#ifdef hue_lightness_saturation
	if (hue_lightness_saturation == TRUE)
		if (r < 0 || r > 360 || g < 0 || g > 100 || b < 0 || b > 100)
			return ERR;	
	if (hue_lightness_saturation == FALSE)
#endif /* hue_lightness_saturation */
		if (r < 0 || r > 1000 || g < 0 ||  g > 1000 || b < 0 || b > 1000)
			return ERR;
				
#ifdef hue_lightness_saturation
	if (hue_lightness_saturation)
	    rgb2hls(r, g, b,
		      &color_table[color].red,
		      &color_table[color].green,
		      &color_table[color].blue);
	else
#endif /* hue_lightness_saturation */
	{
		color_table[color].red = r;
		color_table[color].green = g;
		color_table[color].blue = b;
	}

#ifdef initialize_color
	if (initialize_color)
	{
		TPUTS_TRACE("initialize_color");
		putp(tparm(initialize_color, color, r, g, b));
	}
#endif /* initialize_color */
	return OK;
}

bool can_change_color(void)
{
	return (can_change != 0);
}

int has_colors(void)
{
	return ((orig_pair != NULL || orig_colors != NULL) 
		&& (max_colors != -1) && (max_pairs != -1)
		&& 
		(((set_foreground != NULL) && (set_background != NULL))
		|| ((set_a_foreground != NULL) && (set_a_background != NULL))
		|| set_color_pair)
		);
}

int color_content(short color, short *r, short *g, short *b)
{
    if (color < 0 || color > COLORS)
	return ERR;

    *r = color_table[color].red;
    *g = color_table[color].green;
    *b = color_table[color].blue;
    return OK;
}

int pair_content(short pair, short *f, short *b)
{

	if ((pair < 1) || (pair > COLOR_PAIRS))
		return ERR;
	*f = color_pairs[pair] & 0x0f;
	*b = color_pairs[pair] & 0xf0;
	*b >>= 4;
	return OK;
}


void _nc_do_color(int pair, int  (*outc)(int))
{
    short fg, bg;

    if (pair == 0) 
    {
	if (orig_pair)
	{
	    TPUTS_TRACE("orig_pair");
	    tputs(orig_pair, 1, outc);
	}
    }
    else
    {
	if (set_color_pair)
	{
	    TPUTS_TRACE("set_color_pair");
	    tputs(tparm(set_color_pair, pair), 1, outc);
	}
	else
	{
	    pair_content(pair, &fg, &bg);

	    T(("setting colors: pair = %d, fg = %d, bg = %d\n", pair, fg, bg));

	    if (set_a_foreground)
	    {
		TPUTS_TRACE("set_a_foreground");
		tputs(tparm(set_a_foreground, fg), 1, outc);
	    }
	    else
	    {
		TPUTS_TRACE("set_foreground");
		tputs(tparm(set_foreground, fg), 1, outc);
	    }
	    if (set_a_background)
	    {
		TPUTS_TRACE("set_a_background");
		tputs(tparm(set_a_background, bg), 1, outc);
	    }
	    else
	    {
		TPUTS_TRACE("set_background");
		tputs(tparm(set_background, bg), 1, outc);
	    }
	}
    }
}
