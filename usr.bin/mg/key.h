/* key.h: Insert file for mg 2 functions that need to reference key pressed */

#ifndef EXTERN
#define EXTERN	extern
#endif

#define MAXKEY	8			/* maximum number of prefix chars */

EXTERN	struct {			/* the chacter sequence in a key */
	int	k_count;		/* number of chars		*/
	KCHAR	k_chars[MAXKEY];	/* chars			*/
}	key;
#undef	EXTERN
