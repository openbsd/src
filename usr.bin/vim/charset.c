/*	$OpenBSD: charset.c,v 1.1.1.1 1996/09/07 21:40:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

/*
 * chartab[] is used
 * - to quickly recognize ID characters
 * - to quickly recognize file name characters
 * - to store the size of a character on the screen: Printable is 1 position,
 *	 2 otherwise
 */
static char_u chartab[256];
static int	  chartab_initialized = FALSE;

#define CHAR_MASK		0x3		/* low two bits for size */
#define CHAR_ID			0x4		/* third bit set for ID chars */
#define CHAR_IF			0x8		/* fourth bit set for file name chars */

/*
 * init_chartab(): Fill chartab[] with flags for ID and file name characters
 * and the size of characters on the screen (1 or 2 positions).
 * Also fills b_chartab[] with flags for keyword characters for current
 * buffer.
 *
 * Return FAIL if 'iskeyword', 'isident', 'isfname' or 'isprint' option has an
 * error, OK otherwise.
 */
	int
init_chartab()
{
	int		c;
	int		c2;
	char_u	*p;
	int		i;
	int		tilde;
	int		do_isalpha;

	/*
	 * Set the default size for printable characters:
	 * From <Space> to '~' is 1 (printable), others are 2 (not printable).
	 * This also inits all 'isident' and 'isfname' flags to FALSE.
	 */
	c = 0;
	while (c < ' ')
		chartab[c++] = 2;
	while (c <= '~')
		chartab[c++] = 1;
	while (c < 256)
		chartab[c++] = 2;
	
	/*
	 * Init word char flags all to FALSE
	 */
	if (curbuf != NULL)
		for (c = 0; c < 256; ++c)
			curbuf->b_chartab[c] = FALSE;

	/*
	 * In lisp mode the '-' character is included in keywords.
	 */
	if (curbuf->b_p_lisp)
		curbuf->b_chartab['-'] = TRUE;

	/* Walk through the 'isident', 'iskeyword', 'isfname' and 'isprint'
	 * options Each option is a list of characters, character numbers or
	 * ranges, separated by commas, e.g.: "200-210,x,#-178,-"
	 */
	for (i = 0; i < 4; ++i)
	{
		if (i == 0)
			p = p_isi;				/* first round: 'isident' */
		else if (i == 1)
			p = p_isp;				/* second round: 'isprint' */
		else if (i == 2)
			p = p_isf;				/* third round: 'isfname' */
		else	/* i == 3 */
			p = curbuf->b_p_isk;	/* fourth round: 'iskeyword' */

		while (*p)
		{
			tilde = FALSE;
			do_isalpha = FALSE;
			if (*p == '^' && p[1] != NUL)
			{
				tilde = TRUE;
				++p;
			}
			if (isdigit(*p))
				c = getdigits(&p);
			else
				c = *p++;
			c2 = -1;
			if (*p == '-' && p[1] != NUL)
			{
				++p;
				if (isdigit(*p))
					c2 = getdigits(&p);
				else
					c2 = *p++;
			}
			if (c < 0 || (c2 < c && c2 != -1) || c2 >= 256 ||
													!(*p == NUL || *p == ','))
				return FAIL;

			if (c2 == -1)		/* not a range */
			{
				/*
				 * A single '@' (not "@-@"):
				 * Decide on letters being ID/printable/keyword chars with
				 * standard function isalpha(). This takes care of locale.
				 */
				if (c == '@')
				{
					do_isalpha = TRUE;
					c = 1;
					c2 = 255;
				}
				else
					c2 = c;
			}
			while (c <= c2)
			{
				if (!do_isalpha || isalpha(c))
				{
					if (i == 0)					/* (re)set ID flag */
					{
						if (tilde)
							chartab[c] &= ~CHAR_ID;
						else
							chartab[c] |= CHAR_ID;
					}
					else if (i == 1)			/* set printable to 1 or 2 */
					{
						if (c < ' ' || c > '~')
							chartab[c] = (chartab[c] & ~CHAR_MASK) +
															  (tilde ? 2 : 1);
					}
					else if (i == 2)			/* (re)set fname flag */
					{
						if (tilde)
							chartab[c] &= ~CHAR_IF;
						else
							chartab[c] |= CHAR_IF;
					}
					else /* i == 3 */			/* (re)set keyword flag */
						curbuf->b_chartab[c] = !tilde;
				}
				++c;
			}
			p = skip_to_option_part(p);
		}
	}
	chartab_initialized = TRUE;
	return OK;
}

/*
 * Translate any special characters in buf[bufsize].
 * If there is not enough room, not all characters will be translated.
 */
	void
trans_characters(buf, bufsize)
	char_u	*buf;
	int		bufsize;
{
	int		len;			/* length of string needing translation */
	int		room;			/* room in buffer after string */
	char_u	*new;			/* translated character */
	int		new_len;		/* length of new[] */

	len = STRLEN(buf);
	room = bufsize - len;
	while (*buf)
	{
		new = transchar(*buf);
		new_len = STRLEN(new);
		if (new_len > 1)
		{
			room -= new_len - 1;
			if (room <= 0)
				return;
			vim_memmove(buf + new_len, buf + 1, (size_t)len);
		}
		vim_memmove(buf, new, (size_t)new_len);
		buf += new_len;
		--len;
	}
}

/*
 * Catch 22: chartab[] can't be initialized before the options are
 * initialized, and initializing options may cause transchar() to be called!
 * When chartab_initialized == FALSE don't use chartab[].
 */
	char_u *
transchar(c)
	int	 c;
{
	static char_u	buf[5];
	int				i;

	i = 0;
	if (IS_SPECIAL(c))		/* special key code, display as ~@ char */
	{
		buf[0] = '~';
		buf[1] = '@';
		i = 2;
		c = K_SECOND(c);
	}
	if ((!chartab_initialized && c >= ' ' && c <= '~') ||
				 (chartab[c] & CHAR_MASK) == 1)		/* printable character */
	{
		buf[i] = c;
		buf[i + 1] = NUL;
	}
	else
		transchar_nonprint(buf + i, c);
	return buf;
}

	void
transchar_nonprint(buf, c)
	char_u		*buf;
	int			c;
{
	if (c <= 0x7f)									/* 0x00 - 0x1f and 0x7f */
	{
		if (c == NL)
			c = NUL;					/* we use newline in place of a NUL */
		buf[0] = '^';
		buf[1] = c ^ 0x40;				/* DEL displayed as ^? */
		buf[2] = NUL;
	}
	else if (c >= ' ' + 0x80 && c <= '~' + 0x80)	/* 0xa0 - 0xfe */
	{
		buf[0] = '|';
		buf[1] = c - 0x80;
		buf[2] = NUL;
	}
	else											/* 0x80 - 0x9f and 0xff */
	{
		buf[0] = '~';
		buf[1] = (c - 0x80) ^ 0x40;		/* 0xff displayed as ~? */
		buf[2] = NUL;
	}
}

/*
 * return the number of characters 'c' will take on the screen
 * This is used very often, keep it fast!!!
 */
	int
charsize(c)
	register int c;
{
	if (IS_SPECIAL(c))
		return (chartab[K_SECOND(c)] & CHAR_MASK) + 2;
	return (chartab[c] & CHAR_MASK);
}

/*
 * Return the number of characters string 's' will take on the screen, 
 * counting TABs as two characters: "^I".
 */
	int
strsize(s)
	register char_u *s;
{
	register int	len = 0;

	while (*s)
		len += charsize(*s++);
	return len;
}

/*
 * Return the number of characters 'c' will take on the screen, taking
 * into account the size of a tab.
 * Use a define to make it fast, this is used very often!!!
 * Also see getvcol() below.
 */

#define RET_WIN_BUF_CHARTABSIZE(wp, buf, c, col) \
   	if ((c) == TAB && !(wp)->w_p_list) \
	{ \
		register int ts; \
		ts = (buf)->b_p_ts; \
   		return (int)(ts - (col % ts)); \
	} \
   	else \
		return charsize(c);

	int
chartabsize(c, col)
	register int	c;
	colnr_t			col;
{
	RET_WIN_BUF_CHARTABSIZE(curwin, curbuf, c, col)
}

	int
win_chartabsize(wp, c, col)
	register WIN	*wp;
	register int	c;
	colnr_t			col;
{
	RET_WIN_BUF_CHARTABSIZE(wp, wp->w_buffer, c, col)
}

/*
 * return the number of characters the string 's' will take on the screen,
 * taking into account the size of a tab
 */
	int
linetabsize(s)
	char_u		*s;
{
	colnr_t	col = 0;

	while (*s != NUL)
		col += lbr_chartabsize(s++, col);
	return (int)col;
}

/*
 * return TRUE if 'c' is a normal identifier character
 * letters and characters from 'isident' option.
 */
	int
isidchar(c)
	int c;
{
	return (c < 0x100 && (chartab[c] & CHAR_ID));
}

/*
 * return TRUE if 'c' is a keyword character: Letters and characters from
 * 'iskeyword' option for current buffer.
 */
	int
iswordchar(c)
	int c;
{
	return (c < 0x100 && curbuf->b_chartab[c]);
}

/*
 * return TRUE if 'c' is a valid file-name character
 */
	int
isfilechar(c)
	int	c;
{
	return (c < 0x100 && (chartab[c] & CHAR_IF));
}

/*
 * return TRUE if 'c' is a printable character
 */
	int
isprintchar(c)
	int c;
{
	return (c < 0x100 && (chartab[c] & CHAR_MASK) == 1);
}

/*
 * like chartabsize(), but also check for line breaks on the screen
 */
	int
lbr_chartabsize(s, col)
	unsigned char	*s;
	colnr_t			col;
{
	if (!curwin->w_p_lbr && *p_sbr == NUL)
		RET_WIN_BUF_CHARTABSIZE(curwin, curbuf, *s, col)

	return win_lbr_chartabsize(curwin, s, col, NULL);
}

/*
 * This function is used very often, keep it fast!!!!
 * Warning: *head is only set if it's a non-zero value, init to 0 before
 * calling.
 */
	int
win_lbr_chartabsize(wp, s, col, head)
	WIN				*wp;
	unsigned char	*s;
	colnr_t			col;
	int				*head;
{
	int		c = *s;
	int		size;
	colnr_t	col2;
	colnr_t	colmax;
	int		added;
	int     numberextra;

/*
 * No 'linebreak' and 'showbreak': return quickly.
 */
	if (!wp->w_p_lbr && *p_sbr == NUL)
		RET_WIN_BUF_CHARTABSIZE(wp, wp->w_buffer, c, col)

/*
 * First get normal size, without 'linebreak'
 */
	size = win_chartabsize(wp, c, col);
/*
 * If 'linebreak' set check at a blank before a non-blank if the line needs a
 * break here
 */
	if (wp->w_p_lbr && isbreak(c) && !isbreak(s[1]) &&
											   !wp->w_p_list && wp->w_p_wrap)
	{
		numberextra = curwin->w_p_nu? 8: 0;
		/* count all characters from first non-blank after a blank up to next
		 * non-blank after a blank */
		col2 = col;
		colmax = (((col + numberextra) / Columns) + 1) * Columns;
		while ((c = *++s) != NUL && (isbreak(c) ||
							(!isbreak(c) && (col2 == col || !isbreak(s[-1])))))
		{
			col2 += win_chartabsize(wp, c, col2);
			if (col2 + numberextra >= colmax)			/* doesn't fit */
			{
				size = Columns - ((col + numberextra) % Columns);
				break;
			}
		}
	}
	
/*
 * May have to add something for 'showbreak' string at start of line
 * Set *head to the size of what we add.
 */
	added = 0;
	if (*p_sbr != NUL && wp->w_p_wrap && col)
	{
		numberextra = curwin->w_p_nu? 8: 0;
		col = (col + numberextra) % Columns;
		if (col == 0 || col + size > (colnr_t)Columns)
		{
			added = STRLEN(p_sbr);
			size += added;
			if (col != 0)
				added = 0;
		}
	}
	if (head != NULL)
		*head = added;
	return size;
}

/*
 * get virtual column number of pos
 * start: on the first position of this character (TAB, ctrl)
 * cursor: where the cursor is on this character (first char, except for TAB)
 * end: on the last position of this character (TAB, ctrl)
 */
	void
getvcol(wp, pos, start, cursor, end)
	WIN			*wp;
	FPOS		*pos;
	colnr_t		*start;
	colnr_t		*cursor;
	colnr_t		*end;
{
	int				col;
	colnr_t			vcol;
	char_u		   *ptr;
	int 			incr;
	int				head;
	int				ts = wp->w_buffer->b_p_ts;
	int				c;

	vcol = 0;
	ptr = ml_get_buf(wp->w_buffer, pos->lnum, FALSE);

	/*
	 * This function is used very often, do some speed optimizations.
	 * When 'list', 'linebreak' and 'showbreak' are not set use a simple loop.
	 */
	if (!wp->w_p_list && !wp->w_p_lbr && *p_sbr == NUL)
	{
		head = 0;
		for (col = pos->col; ; --col, ++ptr)
		{
			c = *ptr;
					/* make sure we don't go past the end of the line */
			if (c == NUL)
			{
				incr = 1;		/* NUL at end of line only takes one column */
				break;
			}
					/* A tab gets expanded, depending on the current column */
			if (c == TAB)
				incr = ts - (vcol % ts);
			else
				incr = charsize(c);

			if (col == 0)		/* character at pos.col */
				break;

			vcol += incr;
		}
	}
	else
	{
		for (col = pos->col; ; --col, ++ptr)
		{
					/* A tab gets expanded, depending on the current column */
			head = 0;
			incr = win_lbr_chartabsize(wp, ptr, vcol, &head);
					/* make sure we don't go past the end of the line */
			if (*ptr == NUL)
			{
				incr = 1;		/* NUL at end of line only takes one column */
				break;
			}

			if (col == 0)		/* character at pos.col */
				break;

			vcol += incr;
		}
	}
	if (start != NULL)
		*start = vcol + head;
	if (end != NULL)
		*end = vcol + incr - 1;
	if (cursor != NULL)
	{
		if (*ptr == TAB && (State & NORMAL) && !wp->w_p_list)
			*cursor = vcol + incr - 1;		/* cursor at end */
		else
			*cursor = vcol + head;			/* cursor at start */
	}
}
