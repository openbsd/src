/*	$OpenBSD: key.h,v 1.6 2019/06/22 15:38:15 lum Exp $	*/

/* This file is in the public domain. */

/* key.h: Insert file for mg 2 functions that need to reference key pressed */

#define MAXKEY	8			/* maximum number of prefix chars */

struct key {				/* the character sequence in a key */
	int	k_count;		/* number of chars */
	KCHAR	k_chars[MAXKEY];	/* chars */
};

extern struct key key;
