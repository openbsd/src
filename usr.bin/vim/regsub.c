/*	$OpenBSD: regsub.c,v 1.1.1.1 1996/09/07 21:40:25 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 * NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE
 *
 * This is NOT the original regular expression code as written by
 * Henry Spencer. This code has been modified specifically for use
 * with the VIM editor, and should not be used apart from compiling
 * VIM. If you want a good regular expression library, get the
 * original code. The copyright notice that follows is from the
 * original.
 *
 * NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE
 *
 * vim_regsub
 *
 *		Copyright (c) 1986 by University of Toronto.
 *		Written by Henry Spencer.  Not derived from licensed software.
 *
 *		Permission is granted to anyone to use this software for any
 *		purpose on any computer system, and to redistribute it freely,
 *		subject to the following restrictions:
 *
 *		1. The author is not responsible for the consequences of use of
 *				this software, no matter how awful, even if they arise
 *				from defects in it.
 *
 *		2. The origin of this software must not be misrepresented, either
 *				by explicit claim or by omission.
 *
 *		3. Altered versions must be plainly marked as such, and must not
 *				be misrepresented as being the original software.
 *
 * $Log: regsub.c,v $
 * Revision 1.1.1.1  1996/09/07 21:40:25  downsj
 * Initial import of vim 4.2.
 *
 * This is meant to replace nvi in the tree.  Vim, in general, works better,
 * provides more features, and does not suffer from the license problems
 * being imposed upon nvi.
 *
 * On the other hand, vim lacks a non-visual ex mode, in addition to open mode.
 *
 * This includes the GUI (X11) code, but doesn't try to compile it.
 *
 * Revision 1.2  88/04/28  08:11:25  tony
 * First modification of the regexp library. Added an external variable
 * 'reg_ic' which can be set to indicate that case should be ignored.
 * Added a new parameter to vim_regexec() to indicate that the given string
 * comes from the beginning of a line and is thus eligible to match
 * 'beginning-of-line'.
 *
 * Revisions by Olaf 'Rhialto' Seibert, rhialto@mbfys.kun.nl:
 * Changes for vi: (the semantics of several things were rather different)
 * - Added lexical analyzer, because in vi magicness of characters
 *   is rather difficult, and may change over time.
 * - Added support for \< \> \1-\9 and ~
 * - Left some magic stuff in, but only backslashed: \| \+
 * - * and \+ still work after \) even though they shouldn't.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"

#ifndef __ARGS
# define __ARGS(a)	a
#endif

#include <stdio.h>
#include "regexp.h"

#ifdef LATTICE
# include <sys/types.h>		/* for size_t */
#endif

#ifndef CHARBITS
#define UCHARAT(p)      ((int)*(char_u *)(p))
#else
#define UCHARAT(p)      ((int)*(p)&CHARBITS)
#endif

extern char_u 	   *reg_prev_sub;

	/* This stuff below really confuses cc on an SGI -- webb */
#ifdef __sgi
# undef __ARGS
# define __ARGS(x)	()
#endif

	/*
	 * We should define ftpr as a pointer to a function returning a pointer to
	 * a function returning a pointer to a function ...
	 * This is impossible, so we declare a pointer to a function returning a
	 * pointer to a function returning void. This should work for all compilers.
	 */
typedef void (*(*fptr) __ARGS((char_u *, int)))();

static fptr do_upper __ARGS((char_u *, int));
static fptr do_Upper __ARGS((char_u *, int));
static fptr do_lower __ARGS((char_u *, int));
static fptr do_Lower __ARGS((char_u *, int));

	static fptr
do_upper(d, c)
	char_u *d;
	int c;
{
	*d = TO_UPPER(c);

	return (fptr)NULL;
}

	static fptr
do_Upper(d, c)
	char_u *d;
	int c;
{
	*d = TO_UPPER(c);

	return (fptr)do_Upper;
}

	static fptr
do_lower(d, c)
	char_u *d;
	int c;
{
	*d = TO_LOWER(c);

	return (fptr)NULL;
}

	static fptr
do_Lower(d, c)
	char_u *d;
	int c;
{
	*d = TO_LOWER(c);

	return (fptr)do_Lower;
}

/*
 * regtilde: replace tildes in the pattern by the old pattern
 *
 * Short explanation of the tilde: it stands for the previous replacement
 * pattern. If that previous pattern also contains a ~ we should go back
 * a step further... but we insert the previous pattern into the current one
 * and remember that.
 * This still does not handle the case where "magic" changes. TODO?
 *
 * New solution: The tilde's are parsed once before the first call to
 * vim_regsub(). In the old solution (tilde handled in regsub()) is was
 * possible to get an endless loop.
 */
	char_u *
regtilde(source, magic)
	char_u	*source;
	int		magic;
{
	char_u	*newsub = NULL;
	char_u	*tmpsub;
	char_u	*p;
	int		len;
	int		prevlen;

	for (p = source; *p; ++p)
	{
		if ((*p == '~' && magic) || (*p == '\\' && *(p + 1) == '~' && !magic))
		{
			if (reg_prev_sub)
			{
					/* length = len(current) - 1 + len(previous) + 1 */
				prevlen = STRLEN(reg_prev_sub);
				tmpsub = alloc((unsigned)(STRLEN(source) + prevlen));
				if (tmpsub)
				{
						/* copy prefix */
					len = (int)(p - source);	/* not including ~ */
					STRNCPY(tmpsub, source, len);
						/* interpretate tilde */
					STRCPY(tmpsub + len, reg_prev_sub);
						/* copy postfix */
					if (!magic)
						++p;					/* back off \ */
					STRCAT(tmpsub + len, p + 1);

					vim_free(newsub);
					newsub = tmpsub;
					p = newsub + len + prevlen;
				}
			}
			else if (magic)
				STRCPY(p, p + 1);				/* remove '~' */
			else
				STRCPY(p, p + 2);				/* remove '\~' */
		}
		else if (*p == '\\' && p[1])			/* skip escaped characters */
			++p;
	}

	vim_free(reg_prev_sub);
	if (newsub)
	{
		source = newsub;
		reg_prev_sub = newsub;
	}
	else
		reg_prev_sub = strsave(source);
	return source;
}

/*
 - vim_regsub - perform substitutions after a regexp match
 *
 * If copy is TRUE really copy into dest, otherwise dest is not written to.
 *
 * Returns the size of the replacement, including terminating \0.
 */
	int
vim_regsub(prog, source, dest, copy, magic)
	regexp		   *prog;
	char_u		   *source;
	char_u		   *dest;
	int 			copy;
	int 			magic;
{
	register char_u  	*src;
	register char_u  	*dst;
	register char_u	 	*s;
	register int		c;
	register int		no;
	fptr				func = (fptr)NULL;

	if (prog == NULL || source == NULL || dest == NULL)
	{
		emsg(e_null);
		return 0;
	}
	if (UCHARAT(prog->program) != MAGIC)
	{
		emsg(e_re_corr);
		return 0;
	}
	src = source;
	dst = dest;

	while ((c = *src++) != '\0')
	{
		no = -1;
		if (c == '&' && magic)
			no = 0;
		else if (c == '\\' && *src != NUL)
		{
			if (*src == '&' && !magic)
			{
				++src;
				no = 0;
			}
			else if ('0' <= *src && *src <= '9')
			{
				no = *src++ - '0';
			}
			else if (vim_strchr((char_u *)"uUlLeE", *src))
			{
				switch (*src++)
				{
				case 'u':	func = (fptr)do_upper;
							continue;
				case 'U':	func = (fptr)do_Upper;
							continue;
				case 'l':	func = (fptr)do_lower;
							continue;
				case 'L':	func = (fptr)do_Lower;
							continue;
				case 'e':
				case 'E':	func = (fptr)NULL;
							continue;
				}
			}
		}
		if (no < 0)           /* Ordinary character. */
		{
			if (c == '\\' && *src != NUL)
			{
				/* Check for abbreviations -- webb */
				switch (*src)
				{
					case 'r':	c = CR;			break;
					case 'n':	c = NL;			break;
					case 't':	c = TAB;		break;
					/* Oh no!  \e already has meaning in subst pat :-( */
					/* case 'e':	c = ESC;		break; */
					case 'b':	c = Ctrl('H');	break;
					default:
						/* Normal character, not abbreviation */
						c = *src;
						break;
				}
				src++;
			}
			if (copy)
			{
				if (func == (fptr)NULL)		/* just copy */
					*dst = c;
				else						/* change case */
					func = (fptr)(func(dst, c));
							/* Turbo C complains without the typecast */
			}
			dst++;
		}
		else if (prog->startp[no] != NULL && prog->endp[no] != NULL)
		{
			for (s = prog->startp[no]; s < prog->endp[no]; ++s)
			{
				if (copy && *s == '\0') /* we hit NUL. */
				{
					emsg(e_re_damg);
					goto exit;
				}
				/*
				 * Insert a CTRL-V in front of a CR, otherwise
				 * it will be replaced by a line break.
				 */
				if (*s == CR)
				{
					if (copy)
					{
						dst[0] = Ctrl('V');
						dst[1] = CR;
					}
					dst += 2;
				}
				else
				{
					if (copy)
					{
						if (func == (fptr)NULL)		/* just copy */
							*dst = *s;
						else						/* change case */
							func = (fptr)(func(dst, *s));
									/* Turbo C complains without the typecast */
					}
					++dst;
				}
			}
		}
	}
	if (copy)
		*dst = '\0';

exit:
	return (int)((dst - dest) + 1);
}
