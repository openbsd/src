/*
 *		Word mode commands.
 * The routines in this file
 * implement commands that work word at
 * a time. There are all sorts of word mode
 * commands.
 */
#include	"def.h"

/*
 * Move the cursor backward by
 * "n" words. All of the details of motion
 * are performed by the "backchar" and "forwchar"
 * routines.
 */
/*ARGSUSED*/
backword(f, n)
{
	if (n < 0) return forwword(f | FFRAND, -n);
	if (backchar(FFRAND, 1) == FALSE)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
		while (inword() != FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
	}
	return forwchar(FFRAND, 1);
}

/*
 * Move the cursor forward by
 * the specified number of words. All of the
 * motion is done by "forwchar".
 */
/*ARGSUSED*/
forwword(f, n)
{
	if (n < 0)
		return backword(f | FFRAND, -n);
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
		while (inword() != FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
	}
	return TRUE;
}

/*
 * Move the cursor forward by
 * the specified number of words. As you move,
 * convert any characters to upper case.
 */
/*ARGSUSED*/
upperword(f, n)
{
	register int	c;

	if (n < 0) return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISLOWER(c) != FALSE) {
				c = TOUPPER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFHARD);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
	}
	return TRUE;
}

/*
 * Move the cursor forward by
 * the specified number of words. As you move
 * convert characters to lower case.
 */
/*ARGSUSED*/
lowerword(f, n)
{
	register int	c;

	if (n < 0) return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISUPPER(c) != FALSE) {
				c = TOLOWER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFHARD);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
	}
	return TRUE;
}

/*
 * Move the cursor forward by
 * the specified number of words. As you move
 * convert the first character of the word to upper
 * case, and subsequent characters to lower case. Error
 * if you try and move past the end of the buffer.
 */
/*ARGSUSED*/
capword(f, n)
{
	register int	c;
	VOID		lchange();

	if (n < 0) return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				return TRUE;
		}
		if (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (ISLOWER(c) != FALSE) {
				c = TOUPPER(c);
				lputc(curwp->w_dotp, curwp->w_doto, c);
				lchange(WFHARD);
			}
			if (forwchar(FFRAND, 1) == FALSE)
				return TRUE;
			while (inword() != FALSE) {
				c = lgetc(curwp->w_dotp, curwp->w_doto);
				if (ISUPPER(c) != FALSE) {
					c = TOLOWER(c);
					lputc(curwp->w_dotp, curwp->w_doto, c);
					lchange(WFHARD);
				}
				if (forwchar(FFRAND, 1) == FALSE)
					return TRUE;
			}
		}
	}
	return TRUE;
}

/*
 * Kill forward by "n" words.
 */
/*ARGSUSED*/
delfword(f, n)
{
	register RSIZE	size;
	register LINE	*dotp;
	register int	doto;

	if (n < 0)
		return FALSE;
	if ((lastflag&CFKILL) == 0)		/* Purge kill buffer.	*/
		kdelete();
	thisflag |= CFKILL;
	dotp = curwp->w_dotp;
	doto = curwp->w_doto;
	size = 0;
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				goto out;	/* Hit end of buffer.	*/
			++size;
		}
		while (inword() != FALSE) {
			if (forwchar(FFRAND, 1) == FALSE)
				goto out;	/* Hit end of buffer.	*/
			++size;
		}
	}
out:
	curwp->w_dotp = dotp;
	curwp->w_doto = doto;
	return (ldelete(size, KFORW));
}

/*
 * Kill backwards by "n" words. The rules
 * for success and failure are now different, to prevent
 * strange behavior at the start of the buffer. The command
 * only fails if something goes wrong with the actual delete
 * of the characters. It is successful even if no characters
 * are deleted, or if you say delete 5 words, and there are
 * only 4 words left. I considered making the first call
 * to "backchar" special, but decided that that would just
 * be wierd. Normally this is bound to "M-Rubout" and
 * to "M-Backspace".
 */
/*ARGSUSED*/
delbword(f, n)
{
	register RSIZE	size;
	VOID		kdelete();

	if (n < 0) return FALSE;
	if ((lastflag&CFKILL) == 0)		/* Purge kill buffer.	*/
		kdelete();
	thisflag |= CFKILL;
	if (backchar(FFRAND, 1) == FALSE)
		return (TRUE);			/* Hit buffer start.	*/
	size = 1;				/* One deleted.		*/
	while (n--) {
		while (inword() == FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				goto out;	/* Hit buffer start.	*/
			++size;
		}
		while (inword() != FALSE) {
			if (backchar(FFRAND, 1) == FALSE)
				goto out;	/* Hit buffer start.	*/
			++size;
		}
	}
	if (forwchar(FFRAND, 1) == FALSE)
		return FALSE;
	--size;					/* Undo assumed delete. */
out:
	return ldelete(size, KBACK);
}

/*
 * Return TRUE if the character at dot
 * is a character that is considered to be
 * part of a word. The word character list is hard
 * coded. Should be setable.
 */
inword() {
/* can't use lgetc in ISWORD due to bug in OSK cpp */
	return curwp->w_doto != llength(curwp->w_dotp) && 
		ISWORD(curwp->w_dotp->l_text[curwp->w_doto]);
}

