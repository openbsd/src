/*	$OpenBSD: macros.h,v 1.1.1.1 1996/09/07 21:40:27 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * macros.h: macro definitions for often used code
 */

/*
 * pchar(lp, c) - put character 'c' at position 'lp'
 */
#define pchar(lp, c) (*(ml_get_buf(curbuf, (lp).lnum, TRUE) + (lp).col) = (c))

/*
 * Position comparisons
 */
#define lt(a, b) (((a).lnum != (b).lnum) \
				   ? ((a).lnum < (b).lnum) : ((a).col < (b).col))

#define ltoreq(a, b) (((a).lnum != (b).lnum) \
				   ? ((a).lnum < (b).lnum) : ((a).col <= (b).col))

#define equal(a, b) (((a).lnum == (b).lnum) && ((a).col == (b).col))

/*
 * lineempty() - return TRUE if the line is empty
 */
#define lineempty(p) (*ml_get(p) == NUL)

/*
 * bufempty() - return TRUE if the current buffer is empty
 */
#define bufempty() (curbuf->b_ml.ml_line_count == 1 && *ml_get((linenr_t)1) == NUL)

/*
 * On some systems toupper()/tolower() only work on lower/uppercase characters
 */
#ifdef BROKEN_TOUPPER
# define TO_UPPER(c)	(islower(c) ? toupper(c) : (c))
# define TO_LOWER(c)	(isupper(c) ? tolower(c) : (c))
#else
# define TO_UPPER		toupper
# define TO_LOWER		tolower
#endif

#ifdef HAVE_LANGMAP
/* 
 * Adjust chars in a language according to 'langmap' option.
 * NOTE that there is NO overhead if 'langmap' is not set; but even
 * when set we only have to do 2 ifs and an array lookup.
 * Don't apply 'langmap' if the character comes from the Stuff buffer.
 * The do-while is just to ignore a ';' after the macro.
 */
# define LANGMAP_ADJUST(c, condition) do { \
		if (*p_langmap && (condition) && !KeyStuffed && (c) < 256) \
			c = langmap_mapchar[c]; \
	} while (0)
#endif

/*
 * isbreak() is used very often if 'linebreak' is set, use a macro to make
 * it work fast.
 */
#define isbreak(c) (breakat_flags[(char_u)(c)])
