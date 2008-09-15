/*	$OpenBSD: word.c,v 1.15 2008/09/15 16:11:35 kjell Exp $	*/

/* This file is in the public domain. */

/*
 *		Word mode commands.
 * The routines in this file implement commands that work word at a time.
 * There are all sorts of word mode commands.
 */

#include "def.h"

RSIZE	countfword(void);

/*
 * Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "backchar" and "forwchar" routines.
 */
/* ARGSUSED */
int
backword(int f, int n)
{
	if (n < 0)
		return (forwword(f | FFRAND, -n));
	if (backchar(FFRAND, 1) == FALSE)
		return (FALSE);
	while (n--) {
		while (inword() == FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		while (inword() != FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
	}
	return (forwchar(FFRAND, 1));
}

/*
 * Move the cursor forward by the specified number of words.  All of the
 * motion is done by "forwchar".
 */
/* ARGSUSED */
int
forwword(int f, int n)
{
	if (n < 0)
		return (backword(f | FFRAND, -n));
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		while (inword() != FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
	}
	return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words.  As you move,
 * convert any characters to upper case.
 */
/* ARGSUSED */
int
upperword(int f, int n)
{
	int	c, s;
	RSIZE	size;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		ewprintf("Buffer is read-only");
		return (FALSE);
	}

	if (n < 0)
		return (FALSE);
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		size = countfword();
		undo_add_change(curwp->w_dotp, curwp->w_doto, size);

		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISLOWER(c) != FALSE) {
				c = TOUPPER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFFULL);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
	}
	return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words.  As you move
 * convert characters to lower case.
 */
/* ARGSUSED */
int
lowerword(int f, int n)
{
	int	c, s;
	RSIZE	size;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		ewprintf("Buffer is read-only");
		return (FALSE);
	}
	if (n < 0)
		return (FALSE);
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		size = countfword();
		undo_add_change(curwp->w_dotp, curwp->w_doto, size);

		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISUPPER(c) != FALSE) {
				c = TOLOWER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFFULL);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
	}
	return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words.  As you move
 * convert the first character of the word to upper case, and subsequent
 * characters to lower case.  Error if you try to move past the end of the
 * buffer.
 */
/* ARGSUSED */
int
capword(int f, int n)
{
	int	c, s;
	RSIZE	size;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		ewprintf("Buffer is read-only");
		return (FALSE);
	}

	if (n < 0)
		return (FALSE);
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
		}
		size = countfword();
		undo_add_change(curwp->w_dotp, curwp->w_doto, size);

		if (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISLOWER(c) != FALSE) {
				c = TOUPPER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFFULL);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return (TRUE);
			while (inword() != FALSE) {
				c = lgetc(curwp->w_dotp, curwp->w_doto);
				if (ISUPPER(c) != FALSE) {
					c = TOLOWER(c);
					lputc(curwp->w_dotp, curwp->w_doto, c);
					lchange(WFFULL);
				}
				if (forwchar(FFRAND, 1) == FALSE)
					return (TRUE);
			}
		}
	}
	return (TRUE);
}

/*
 * Count characters in word, from current position
 */
RSIZE
countfword()
{
	RSIZE		 size;
	struct line	*dotp;
	int		 doto;

	dotp = curwp->w_dotp;
	doto = curwp->w_doto;
	size = 0;

	while (inword() != FALSE) {
		if (forwchar(FFRAND, 1) == FALSE)
			/* hit the end of the buffer */
			goto out;
		++size;
	}
out:
	curwp->w_dotp = dotp;
	curwp->w_doto = doto;
	return (size);
}


/*
 * Kill forward by "n" words.
 */
/* ARGSUSED */
int
delfword(int f, int n)
{
	RSIZE		 size;
	struct line	*dotp;
	int		 doto;
	int s;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		ewprintf("Buffer is read-only");
		return (FALSE);
	}
	if (n < 0)
		return (FALSE);

	/* purge kill buffer */
	if ((lastflag & CFKILL) == 0)
		kdelete();

	thisflag |= CFKILL;
	dotp = curwp->w_dotp;
	doto = curwp->w_doto;
	size = 0;

	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				/* hit the end of the buffer */
				goto out;
			++size;
		}
		while (inword() != FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				/* hit the end of the buffer */
				goto out;
			++size;
		}
	}
out:
	curwp->w_dotp = dotp;
	curwp->w_doto = doto;
	return (ldelete(size, KFORW));
}

/*
 * Kill backwards by "n" words.  The rules for success and failure are now
 * different, to prevent strange behavior at the start of the buffer.  The
 * command only fails if something goes wrong with the actual delete of the
 * characters.  It is successful even if no characters are deleted, or if you
 * say delete 5 words, and there are only 4 words left.  I considered making
 * the first call to "backchar" special, but decided that that would just be
 * weird. Normally this is bound to "M-Rubout" and to "M-Backspace".
 */
/* ARGSUSED */
int
delbword(int f, int n)
{
	RSIZE	size;
	int s;

	if ((s = checkdirty(curbp)) != TRUE)
		return (s);
	if (curbp->b_flag & BFREADONLY) {
		ewprintf("Buffer is read-only");
		return (FALSE);
	}

	if (n < 0)
		return (FALSE);

	/* purge kill buffer */
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	if (backchar(FFRAND, 1) == FALSE)
		/* hit buffer start */
		return (TRUE);

	/* one deleted */
	size = 1;
	while (n--) {
		while (inword() == FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				/* hit buffer start */
				goto out;
			++size;
		}
		while (inword() != FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				/* hit buffer start */
				goto out;
			++size;
		}
	}
	if (forwchar(FFRAND, 1) == FALSE)
		return (FALSE);

	/* undo assumed delete */
	--size;
out:
	return (ldelete(size, KBACK));
}

/*
 * Return TRUE if the character at dot is a character that is considered to be
 * part of a word. The word character list is hard coded. Should be settable.
 */
int
inword(void)
{
	/* can't use lgetc in ISWORD due to bug in OSK cpp */
	return (curwp->w_doto != llength(curwp->w_dotp) &&
	    ISWORD(curwp->w_dotp->l_text[curwp->w_doto]));
}
