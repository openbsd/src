/*	$OpenBSD: key.h,v 1.4 2004/02/01 22:26:41 vincent Exp $	*/

/* key.h: Insert file for mg 2 functions that need to reference key pressed */

#define MAXKEY	8			/* maximum number of prefix chars */

struct key {				/* the chacter sequence in a key */
	int	k_count;		/* number of chars */
	KCHAR	k_chars[MAXKEY];	/* chars */
};

extern struct key key;
