/*	$OpenBSD: digraph.c,v 1.2 1996/09/21 06:22:56 downsj Exp $	*/
/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * digraph.c: code for digraphs
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "option.h"

#ifdef DIGRAPHS

static int getexactdigraph __ARGS((int, int, int));
static void printdigraph __ARGS((char_u *));

static char_u	(*digraphnew)[3];			/* pointer to added digraphs */
static int		digraphcount = 0;			/* number of added digraphs */

#if defined(MSDOS) || defined(WIN32) || defined(OS2)
char_u	digraphdefault[][3] = 		/* standard MSDOS digraphs */
	   {{'C', ',', 128},	/* ~@ (SAS C can't handle the real char) */
		{'u', '"', 129},	/* Å */
		{'e', '\'', 130},	/* Ç */
		{'a', '^', 131},	/* É */
		{'a', '"', 132},	/* Ñ */
		{'a', '`', 133},	/* Ö */
		{'a', '@', 134},	/* Ü */
		{'c', ',', 135},	/* ~G (SAS C can't handle the real char) */
		{'e', '^', 136},	/* ~H (SAS C can't handle the real char) */
		{'e', '"', 137},	/* â */
		{'e', '`', 138},	/* ä */
		{'i', '"', 139},	/* ã */
		{'i', '^', 140},	/* å */
		{'i', '`', 141},	/* ç */
		{'A', '"', 142},	/* é */
		{'A', '@', 143},	/* è */
		{'E', '\'', 144},	/* ê */
		{'a', 'e', 145},	/* ë */
		{'A', 'E', 146},	/* í */
		{'o', '^', 147},	/* ì */
		{'o', '"', 148},	/* î */
		{'o', '`', 149},	/* ï */
		{'u', '^', 150},	/* ñ */
		{'u', '`', 151},	/* ó */
		{'y', '"', 152},	/* ò */
		{'O', '"', 153},	/* ô */
		{'U', '"', 154},	/* ö */
	    {'c', '|', 155},	/* õ */
	    {'$', '$', 156},	/* ú */
	    {'Y', '-', 157},	/* ~] (SAS C can't handle the real char) */
	    {'P', 't', 158},	/* û */
	    {'f', 'f', 159},	/* ü */
		{'a', '\'', 160},	/* † */
		{'i', '\'', 161},	/* ° */
		{'o', '\'', 162},	/* ¢ */
		{'u', '\'', 163},	/* xx (SAS C can't handle the real char) */
		{'n', '~', 164},	/* § */
		{'N', '~', 165},	/* • */
		{'a', 'a', 166},	/* ¶ */
		{'o', 'o', 167},	/* ß */
		{'~', '?', 168},	/* ® */
		{'-', 'a', 169},	/* © */
		{'a', '-', 170},	/* ™ */
		{'1', '2', 171},	/* ´ */
		{'1', '4', 172},	/* ¨ */
		{'~', '!', 173},	/* ≠ */
		{'<', '<', 174},	/* Æ */
		{'>', '>', 175},	/* Ø */

		{'s', 's', 225},	/* · */
		{'j', 'u', 230},	/* Ê */
		{'o', '/', 237},	/* Ì */
		{'+', '-', 241},	/* Ò */
		{'>', '=', 242},	/* Ú */
		{'<', '=', 243},	/* Û */
		{':', '-', 246},	/* ˆ */
		{'~', '~', 247},	/* ˜ */
		{'~', 'o', 248},	/* ¯ */
		{'2', '2', 253},	/* ˝ */
		{NUL, NUL, NUL}
		};

#else	/* !MSDOS && !WIN32 */
# ifdef MINT
char_u	digraphdefault[][3] = 		/* standard ATARI digraphs */
	   {{'C', ',', 128},	/* ~@ */
		{'u', '"', 129},	/* Å */
		{'e', '\'', 130},	/* Ç */
		{'a', '^', 131},	/* É */
		{'a', '"', 132},	/* Ñ */
		{'a', '`', 133},	/* Ö */
		{'a', '@', 134},	/* Ü */
		{'c', ',', 135},	/* ~G */
		{'e', '^', 136},	/* ~H */
		{'e', '"', 137},	/* â */
		{'e', '`', 138},	/* ä */
		{'i', '"', 139},	/* ã */
		{'i', '^', 140},	/* å */
		{'i', '`', 141},	/* ç */
		{'A', '"', 142},	/* é */
		{'A', '@', 143},	/* è */
		{'E', '\'', 144},	/* ê */
		{'a', 'e', 145},	/* ë */
		{'A', 'E', 146},	/* í */
		{'o', '^', 147},	/* ì */
		{'o', '"', 148},	/* î */
		{'o', '`', 149},	/* ï */
		{'u', '^', 150},	/* ñ */
		{'u', '`', 151},	/* ó */
		{'y', '"', 152},	/* ò */
		{'O', '"', 153},	/* ô */
		{'U', '"', 154},	/* ö */
	   	{'c', '|', 155},	/* õ */
	   	{'$', '$', 156},	/* ú */
	   	{'Y', '-', 157},	/* ~] */
	   	{'s', 's', 158},	/* û */
	    {'f', 'f', 159},	/* ü */
		{'a', '\'', 160},	/* † */
		{'i', '\'', 161},	/* ° */
		{'o', '\'', 162},	/* ¢ */
		{'u', '\'', 163},	/* £ */
		{'n', '~', 164},	/* § */
		{'N', '~', 165},	/* • */
		{'a', 'a', 166},	/* ¶ */
		{'o', 'o', 167},	/* ß */
		{'~', '?', 168},	/* ® */
		{'-', 'a', 169},	/* © */
		{'a', '-', 170},	/* ™ */
		{'1', '2', 171},	/* ´ */
		{'1', '4', 172},	/* ¨ */
		{'~', '!', 173},	/* ≠ */
		{'<', '<', 174},	/* Æ */
		{'>', '>', 175},	/* Ø */
		{'j', 'u', 230},	/* Ê */
		{'o', '/', 237},	/* Ì */
		{'+', '-', 241},	/* Ò */
		{'>', '=', 242},	/* Ú */
		{'<', '=', 243},	/* Û */
		{':', '-', 246},	/* ˆ */
		{'~', '~', 247},	/* ˜ */
		{'~', 'o', 248},	/* ¯ */
		{'2', '2', 253},	/* ˝ */
		{NUL, NUL, NUL}
		};

# else	/* !MINT */
#  ifdef _INCLUDE_HPUX_SOURCE

char_u	digraphdefault[][3] = 		/* default HPUX digraphs */
	   {{'A', '`', 161},	/* ° */
	    {'A', '^', 162},	/* ¢ */
	    {'E', '`', 163},	/* £ */
	    {'E', '^', 164},	/* § */
	    {'E', '"', 165},	/* • */
	    {'I', '^', 166},	/* ¶ */
	    {'I', '"', 167},	/* ß */
	    {'\'', '\'', 168},	/* ® */
	    {'`', '`', 169},	/* © */
		{'^', '^', 170},	/* ™ */
		{'"', '"', 171},	/* ´ */
		{'~', '~', 172},	/* ¨ */
		{'U', '`', 173},	/* ≠ */
		{'U', '^', 174},	/* Æ */
		{'L', '=', 175},	/* Ø */
		{'~', '_', 176},	/* ∞ */
		{'Y', '\'', 177},	/* ± */
		{'y', '\'', 178},	/* ≤ */
		{'~', 'o', 179},	/* ≥ */
		{'C', ',', 180},	/* ¥ */
		{'c', ',', 181},	/* µ */
		{'N', '~', 182},	/* ∂ */
		{'n', '~', 183},	/* ∑ */
		{'~', '!', 184},	/* ∏ */
		{'~', '?', 185},	/* π */
		{'o', 'x', 186},	/* ∫ */
		{'L', '-', 187},	/* ª */
		{'Y', '=', 188},	/* º */
		{'p', 'p', 189},	/* Ω */
		{'f', 'l', 190},	/* æ */
		{'c', '|', 191},	/* ø */
		{'a', '^', 192},	/* ¿ */
		{'e', '^', 193},	/* ¡ */
		{'o', '^', 194},	/* ¬ */
		{'u', '^', 195},	/* √ */
		{'a', '\'', 196},	/* ƒ */
		{'e', '\'', 197},	/* ≈ */
		{'o', '\'', 198},	/* ∆ */
		{'u', '\'', 199},	/* « */
		{'a', '`', 200},	/* » */
		{'e', '`', 201},	/* … */
		{'o', '`', 202},	/*   */
		{'u', '`', 203},	/* À */
		{'a', '"', 204},	/* Ã */
		{'e', '"', 205},	/* Õ */
		{'o', '"', 206},	/* Œ */
		{'u', '"', 207},	/* œ */
		{'A', 'o', 208},	/* – */
		{'i', '^', 209},	/* — */
		{'O', '/', 210},	/* “ */
		{'A', 'E', 211},	/* ” */
		{'a', 'o', 212},	/* ‘ */
		{'i', '\'', 213},	/* ’ */
		{'o', '/', 214},	/* ÷ */
		{'a', 'e', 215},	/* ◊ */
		{'A', '"', 216},	/* ÿ */
		{'i', '`', 217},	/* Ÿ */
		{'O', '"', 218},	/* ⁄ */
		{'U', '"', 219},	/* € */
		{'E', '\'', 220},	/* ‹ */
		{'i', '"', 221},	/* › */
		{'s', 's', 222},	/* ﬁ */
		{'O', '^', 223},	/* ﬂ */
		{'A', '\'', 224},	/* ‡ */
		{'A', '~', 225},	/* · */
		{'a', '~', 226},	/* ‚ */
		{'D', '-', 227},	/* „ */
		{'d', '-', 228},	/* ‰ */
		{'I', '\'', 229},	/* Â */
		{'I', '`', 230},	/* Ê */
		{'O', '\'', 231},	/* Á */
		{'O', '`', 232},	/* Ë */
		{'O', '~', 233},	/* È */
		{'o', '~', 234},	/* Í */
		{'S', '~', 235},	/* Î */
		{'s', '~', 236},	/* Ï */
		{'U', '\'', 237},	/* Ì */
		{'Y', '"', 238},	/* Ó */
		{'y', '"', 239},	/* Ô */
		{'p', '-', 240},	/*  */
		{'p', '~', 241},	/* Ò */
		{'~', '.', 242},	/* Ú */
		{'j', 'u', 243},	/* Û */
		{'P', 'p', 244},	/* Ù */
		{'3', '4', 245},	/* ı */
		{'-', '-', 246},	/* ˆ */
		{'1', '4', 247},	/* ˜ */
		{'1', '2', 248},	/* ¯ */
		{'a', '_', 249},	/* ˘ */
		{'o', '_', 250},	/* ˙ */
		{'<', '<', 251},	/* ˚ */
		{'x', 'x', 252},	/* ¸ */
		{'>', '>', 253},	/* ˝ */
		{'+', '-', 254},	/* ˛ */
		{'n', 'u', 255},	/* (char excluded, is EOF on some systems */
		{NUL, NUL, NUL}
		};

#  else	/* _INCLUDE_HPUX_SOURCE */

char_u	digraphdefault[][3] = 		/* standard ISO digraphs */
	   {{'~', '!', 161},	/* ° */
	    {'c', '|', 162},	/* ¢ */
	    {'$', '$', 163},	/* £ */
	    {'o', 'x', 164},	/* § */
	    {'Y', '-', 165},	/* • */
	    {'|', '|', 166},	/* ¶ */
	    {'p', 'a', 167},	/* ß */
	    {'"', '"', 168},	/* ® */
	    {'c', 'O', 169},	/* © */
		{'a', '-', 170},	/* ™ */
		{'<', '<', 171},	/* ´ */
		{'-', ',', 172},	/* ¨ */
		{'-', '-', 173},	/* ≠ */
		{'r', 'O', 174},	/* Æ */
		{'-', '=', 175},	/* Ø */
		{'~', 'o', 176},	/* ∞ */
		{'+', '-', 177},	/* ± */
		{'2', '2', 178},	/* ≤ */
		{'3', '3', 179},	/* ≥ */
		{'\'', '\'', 180},	/* ¥ */
		{'j', 'u', 181},	/* µ */
		{'p', 'p', 182},	/* ∂ */
		{'~', '.', 183},	/* ∑ */
		{',', ',', 184},	/* ∏ */
		{'1', '1', 185},	/* π */
		{'o', '-', 186},	/* ∫ */
		{'>', '>', 187},	/* ª */
		{'1', '4', 188},	/* º */
		{'1', '2', 189},	/* Ω */
		{'3', '4', 190},	/* æ */
		{'~', '?', 191},	/* ø */
		{'A', '`', 192},	/* ¿ */
		{'A', '\'', 193},	/* ¡ */
		{'A', '^', 194},	/* ¬ */
		{'A', '~', 195},	/* √ */
		{'A', '"', 196},	/* ƒ */
		{'A', '@', 197},	/* ≈ */
		{'A', 'E', 198},	/* ∆ */
		{'C', ',', 199},	/* « */
		{'E', '`', 200},	/* » */
		{'E', '\'', 201},	/* … */
		{'E', '^', 202},	/*   */
		{'E', '"', 203},	/* À */
		{'I', '`', 204},	/* Ã */
		{'I', '\'', 205},	/* Õ */
		{'I', '^', 206},	/* Œ */
		{'I', '"', 207},	/* œ */
		{'D', '-', 208},	/* – */
		{'N', '~', 209},	/* — */
		{'O', '`', 210},	/* “ */
		{'O', '\'', 211},	/* ” */
		{'O', '^', 212},	/* ‘ */
		{'O', '~', 213},	/* ’ */
		{'O', '"', 214},	/* ÷ */
		{'/', '\\', 215},	/* ◊ */
		{'O', '/', 216},	/* ÿ */
		{'U', '`', 217},	/* Ÿ */
		{'U', '\'', 218},	/* ⁄ */
		{'U', '^', 219},	/* € */
		{'U', '"', 220},	/* ‹ */
		{'Y', '\'', 221},	/* › */
		{'I', 'p', 222},	/* ﬁ */
		{'s', 's', 223},	/* ﬂ */
		{'a', '`', 224},	/* ‡ */
		{'a', '\'', 225},	/* · */
		{'a', '^', 226},	/* ‚ */
		{'a', '~', 227},	/* „ */
		{'a', '"', 228},	/* ‰ */
		{'a', '@', 229},	/* Â */
		{'a', 'e', 230},	/* Ê */
		{'c', ',', 231},	/* Á */
		{'e', '`', 232},	/* Ë */
		{'e', '\'', 233},	/* È */
		{'e', '^', 234},	/* Í */
		{'e', '"', 235},	/* Î */
		{'i', '`', 236},	/* Ï */
		{'i', '\'', 237},	/* Ì */
		{'i', '^', 238},	/* Ó */
		{'i', '"', 239},	/* Ô */
		{'d', '-', 240},	/*  */
		{'n', '~', 241},	/* Ò */
		{'o', '`', 242},	/* Ú */
		{'o', '\'', 243},	/* Û */
		{'o', '^', 244},	/* Ù */
		{'o', '~', 245},	/* ı */
		{'o', '"', 246},	/* ˆ */
		{':', '-', 247},	/* ˜ */
		{'o', '/', 248},	/* ¯ */
		{'u', '`', 249},	/* ˘ */
		{'u', '\'', 250},	/* ˙ */
		{'u', '^', 251},	/* ˚ */
		{'u', '"', 252},	/* ¸ */
		{'y', '\'', 253},	/* ˝ */
		{'i', 'p', 254},	/* ˛ */
		{'y', '"', 255},	/* (char excluded, is EOF on some systems */
		{NUL, NUL, NUL}
		};

#  endif	/* _INCLUDE_HPUX_SOURCE */
# endif	/* !MINT */
#endif	/* !MSDOS && !WIN32 */
 
/*
 * handle digraphs after typing a character
 */
	int
do_digraph(c)
	int		c;
{
	static int	backspaced;		/* character before K_BS */
	static int	lastchar;		/* last typed character */

	if (c == -1)				/* init values */
	{
		backspaced = -1;
	}
	else if (p_dg)
	{
		if (backspaced >= 0)
			c = getdigraph(backspaced, c, FALSE);
		backspaced = -1;
		if ((c == K_BS || c == Ctrl('H')) && lastchar >= 0)
			backspaced = lastchar;
	}
	lastchar = c;
	return c;
}

/*
 * lookup the pair char1, char2 in the digraph tables
 * if no match, return char2
 */
	static int
getexactdigraph(char1, char2, meta)
	int	char1;
	int	char2;
	int	meta;
{
	int		i;
	int		retval;

	if (IS_SPECIAL(char1) || IS_SPECIAL(char2))
		return char2;
	retval = 0;
	for (i = 0; ; ++i)			/* search added digraphs first */
	{
		if (i == digraphcount)	/* end of added table, search defaults */
		{
			for (i = 0; digraphdefault[i][0] != 0; ++i)
				if (digraphdefault[i][0] == char1 && digraphdefault[i][1] == char2)
				{
					retval = digraphdefault[i][2];
					break;
				}
			break;
		}
		if (digraphnew[i][0] == char1 && digraphnew[i][1] == char2)
		{
			retval = digraphnew[i][2];
			break;
		}
	}

	if (retval == 0)			/* digraph deleted or not found */
	{
		if (char1 == ' ' && meta)		/* <space> <char> --> meta-char */
			return (char2 | 0x80);
		return char2;
	}
	return retval;
}

/*
 * Get digraph.
 * Allow for both char1-char2 and char2-char1
 */
	int
getdigraph(char1, char2, meta)
	int	char1;
	int	char2;
	int	meta;
{
	int		retval;

	if (((retval = getexactdigraph(char1, char2, meta)) == char2) &&
														   (char1 != char2) &&
					((retval = getexactdigraph(char2, char1, meta)) == char1))
		return char2;
	return retval;
}

/*
 * put the digraphs in the argument string in the digraph table
 * format: {c1}{c2} char {c1}{c2} char ...
 */
	void
putdigraph(str)
	char_u *str;
{
	int		char1, char2, n;
	char_u	(*newtab)[3];
	int		i;

	while (*str)
	{
		str = skipwhite(str);
		if ((char1 = *str++) == 0 || (char2 = *str++) == 0)
			return;
		if (char1 == ESC || char2 == ESC)
		{
			EMSG("Escape not allowed in digraph");
			return;
		}
		str = skipwhite(str);
		if (!isdigit(*str))
		{
			emsg(e_number);
			return;
		}
		n = getdigits(&str);
		if (digraphnew)		/* search the table for existing entry */
		{
			for (i = 0; i < digraphcount; ++i)
				if (digraphnew[i][0] == char1 && digraphnew[i][1] == char2)
				{
					digraphnew[i][2] = n;
					break;
				}
			if (i < digraphcount)
				continue;
		}
		newtab = (char_u (*)[3])alloc(digraphcount * 3 + 3);
		if (newtab)
		{
			vim_memmove(newtab, digraphnew, (size_t)(digraphcount * 3));
			vim_free(digraphnew);
			digraphnew = newtab;
			digraphnew[digraphcount][0] = char1;
			digraphnew[digraphcount][1] = char2;
			digraphnew[digraphcount][2] = n;
			++digraphcount;
		}
	}
}

	void
listdigraphs()
{
	int		i;

	msg_outchar('\n');
	printdigraph(NULL);
	for (i = 0; digraphdefault[i][0] && !got_int; ++i)
	{
		if (getexactdigraph(digraphdefault[i][0], digraphdefault[i][1],
											   FALSE) == digraphdefault[i][2])
			printdigraph(digraphdefault[i]);
		mch_breakcheck();
	}
	for (i = 0; i < digraphcount && !got_int; ++i)
	{
		printdigraph(digraphnew[i]);
		mch_breakcheck();
	}
	must_redraw = CLEAR;	/* clear screen, because some digraphs may be wrong,
							 * in which case we messed up NextScreen */
}

	static void
printdigraph(p)
	char_u *p;
{
	char_u		buf[9];
	static int	len;

	if (p == NULL)
		len = 0;
	else if (p[2] != 0)
	{
		if (len > Columns - 11)
		{
			msg_outchar('\n');
			len = 0;
		}
		if (len)
			MSG_OUTSTR("   ");
		sprintf((char *)buf, "%c%c %c %3d", p[0], p[1], p[2], p[2]);
		msg_outstr(buf);
		len += 11;
	}
}

#endif /* DIGRAPHS */
