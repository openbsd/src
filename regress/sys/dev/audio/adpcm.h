/*	$OpenBSD: adpcm.h,v 1.1.1.1 2003/02/01 17:58:18 jason Exp $	*/

/*
** adpcm.h - include file for adpcm coder.
**
** Version 1.0, 7-Jul-92.
*/

struct adpcm_state {
    int16_t	valprev;	/* Previous output value */
    char	index;		/* Index into stepsize table */
};

#ifdef __STDC__
#define ARGS(x) x
#else
#define ARGS(x) ()
#endif

void adpcm_coder ARGS((int16_t [], char [], int, struct adpcm_state *));
void adpcm_decoder ARGS((char [], int16_t [], int, struct adpcm_state *));
