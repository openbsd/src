/*	$OpenBSD: kbd.c,v 1.21 2005/12/13 07:20:13 kjell Exp $	*/

/* This file is in the public domain. */

/*
 *	Terminal independent keyboard handling.
 */

#include "def.h"
#include "kbd.h"
#include "key.h"

#ifndef NO_MACRO
#include "macro.h"
#endif /* !NO_MACRO */

#ifndef METABIT
#define METABIT 0x80
#endif /* !METABIT */

#ifndef NO_DPROMPT
#define PROMPTL 80
char	 prompt[PROMPTL] = "", *promptp = prompt;
#endif /* !NO_DPROMPT */

static int	 use_metakey = TRUE;
static int	 pushed = FALSE;
static int	 pushedc;

struct map_element	*ele;

struct key key;

/*
 * Toggle the value of use_metakey
 */
int
do_meta(int f, int n)
{
	if (f & FFARG)
		use_metakey = n > 0;
	else
		use_metakey = !use_metakey;
	ewprintf("Meta keys %sabled", use_metakey ? "en" : "dis");
	return (TRUE);
}

#ifdef BSMAP
static int	 bs_map = BSMAP;
/*
 * Toggle backspace mapping
 */
int
bsmap(int f, int n)
{
	if (f & FFARG)
		bs_map = n > 0;
	else
		bs_map = !bs_map;
	ewprintf("Backspace mapping %sabled", bs_map ? "en" : "dis");
	return (TRUE);
}
#endif /* BSMAP */

void
ungetkey(int c)
{
	if (use_metakey && pushed && c == CCHR('['))
		pushedc |= METABIT;
	else
		pushedc = c;
	pushed = TRUE;
}

int
getkey(int flag)
{
	int	 c;

#ifndef NO_DPROMPT
	if (flag && !pushed) {
		if (prompt[0] != '\0' && ttwait(2000)) {
			/* avoid problems with % */
			ewprintf("%s", prompt);
			/* put the cursor back */
			update();
			epresf = KPROMPT;
		}
		if (promptp > prompt)
			*(promptp - 1) = ' ';
	}
#endif /* !NO_DPROMPT */
	if (pushed) {
		c = pushedc;
		pushed = FALSE;
	} else
		c = ttgetc();
#ifdef BSMAP
	if (bs_map)
		if (c == CCHR('H'))
			c = CCHR('?');
		else if (c == CCHR('?'))
			c = CCHR('H');
#endif /* BSMAP */
	if (use_metakey && (c & METABIT)) {
		pushedc = c & ~METABIT;
		pushed = TRUE;
		c = CCHR('[');
	}
#ifndef NO_DPROMPT
	if (flag && promptp < &prompt[PROMPTL - 5]) {
		promptp = getkeyname(promptp,
		    sizeof(prompt) - (promptp - prompt) - 1, c);
		*promptp++ = '-';
		*promptp = '\0';
	}
#endif /* !NO_DPROMPT */
	return (c);
}

/*
 * doscan scans a keymap for a keyboard character and returns a pointer
 * to the function associated with that character.  Sets ele to the
 * keymap element the keyboard was found in as a side effect.
 */
PF
doscan(KEYMAP *map, int c, KEYMAP **newmap)
{
	struct map_element	*elec = &map->map_element[0];
	struct map_element	*last = &map->map_element[map->map_num];
	PF		 ret;

	while (elec < last && c > elec->k_num)
		elec++;

	/* used by prefix and binding code */
	ele = elec;
	if (elec >= last || c < elec->k_base)
		ret = map->map_default;
	else
		ret = elec->k_funcp[c - elec->k_base];
	if (ret == NULL && newmap != NULL)
		*newmap = elec->k_prefmap;

	return (ret);
}

int
doin(void)
{
	KEYMAP	*curmap;
	PF	 funct;

#ifndef NO_DPROMPT
	*(promptp = prompt) = '\0';
#endif /* !NO_DPROMPT */
	curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
	key.k_count = 0;
	while ((funct = doscan(curmap, (key.k_chars[key.k_count++] =
	    getkey(TRUE)), &curmap)) == NULL)
		/* nothing */;
#ifndef NO_MACRO
	if (macrodef && macrocount < MAXMACRO)
		macro[macrocount++].m_funct = funct;
#endif /* !NO_MACRO */
	return ((*funct)(0, 1));
}

int
rescan(int f, int n)
{
	int	 c;
	KEYMAP	*curmap;
	int	 i;
	PF	 fp = NULL;
	int	 md = curbp->b_nmodes;

	for (;;) {
		if (ISUPPER(key.k_chars[key.k_count - 1])) {
			c = TOLOWER(key.k_chars[key.k_count - 1]);
			curmap = curbp->b_modes[md]->p_map;
			for (i = 0; i < key.k_count - 1; i++) {
				if ((fp = doscan(curmap, (key.k_chars[i]), &curmap))
				    != NULL)
					break;
			}
			if (fp == NULL) {
				if ((fp = doscan(curmap, c, NULL)) == NULL)
					while ((fp = doscan(curmap,
					    key.k_chars[key.k_count++] =
					    getkey(TRUE), &curmap)) == NULL)
						/* nothing */;
				if (fp != rescan) {
#ifndef NO_MACRO
					if (macrodef && macrocount <= MAXMACRO)
						macro[macrocount - 1].m_funct
						    = fp;
#endif /* !NO_MACRO */
					return ((*fp)(f, n));
				}
			}
		}
		/* try previous mode */
		if (--md < 0)
			return (ABORT);
		curmap = curbp->b_modes[md]->p_map;
		for (i = 0; i < key.k_count; i++) {
			if ((fp = doscan(curmap, (key.k_chars[i]), &curmap)) != NULL)
				break;
		}
		if (fp == NULL) {
			while ((fp = doscan(curmap, key.k_chars[i++] =
			    getkey(TRUE), &curmap)) == NULL)
				/* nothing */;
			key.k_count = i;
		}
		if (fp != rescan && i >= key.k_count - 1) {
#ifndef NO_MACRO
			if (macrodef && macrocount <= MAXMACRO)
				macro[macrocount - 1].m_funct = fp;
#endif /* !NO_MACRO */
			return ((*fp)(f, n));
		}
	}
}

int
universal_argument(int f, int n)
{
	KEYMAP	*curmap;
	PF	 funct;
	int	 c, nn = 4;

	if (f & FFUNIV)
		nn *= n;
	for (;;) {
		key.k_chars[0] = c = getkey(TRUE);
		key.k_count = 1;
		if (c == '-')
			return (negative_argument(f, nn));
		if (c >= '0' && c <= '9')
			return (digit_argument(f, nn));
		curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
		while ((funct = doscan(curmap, c, &curmap)) == NULL) {
			key.k_chars[key.k_count++] = c = getkey(TRUE);
		}
		if (funct != universal_argument) {
#ifndef NO_MACRO
			if (macrodef && macrocount < MAXMACRO - 1) {
				if (f & FFARG)
					macrocount--;
				macro[macrocount++].m_count = nn;
				macro[macrocount++].m_funct = funct;
			}
#endif /* !NO_MACRO */
			return ((*funct)(FFUNIV, nn));
		}
		nn <<= 2;
	}
}

/* ARGSUSED */
int
digit_argument(int f, int n)
{
	KEYMAP	*curmap;
	PF	 funct;
	int	 nn, c;

	nn = key.k_chars[key.k_count - 1] - '0';
	for (;;) {
		c = getkey(TRUE);
		if (c < '0' || c > '9')
			break;
		nn *= 10;
		nn += c - '0';
	}
	key.k_chars[0] = c;
	key.k_count = 1;
	curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
	while ((funct = doscan(curmap, c, &curmap)) == NULL) {
		key.k_chars[key.k_count++] = c = getkey(TRUE);
	}
#ifndef NO_MACRO
	if (macrodef && macrocount < MAXMACRO - 1) {
		if (f & FFARG)
			macrocount--;
		else
			macro[macrocount - 1].m_funct = universal_argument;
		macro[macrocount++].m_count = nn;
		macro[macrocount++].m_funct = funct;
	}
#endif /* !NO_MACRO */
	return ((*funct)(FFOTHARG, nn));
}

int
negative_argument(int f, int n)
{
	KEYMAP	*curmap;
	PF	 funct;
	int	 c;
	int	 nn = 0;

	for (;;) {
		c = getkey(TRUE);
		if (c < '0' || c > '9')
			break;
		nn *= 10;
		nn += c - '0';
	}
	if (nn)
		nn = -nn;
	else
		nn = -n;
	key.k_chars[0] = c;
	key.k_count = 1;
	curmap = curbp->b_modes[curbp->b_nmodes]->p_map;
	while ((funct = doscan(curmap, c, &curmap)) == NULL) {
		key.k_chars[key.k_count++] = c = getkey(TRUE);
	}
#ifndef NO_MACRO
	if (macrodef && macrocount < MAXMACRO - 1) {
		if (f & FFARG)
			macrocount--;
		else
			macro[macrocount - 1].m_funct = universal_argument;
		macro[macrocount++].m_count = nn;
		macro[macrocount++].m_funct = funct;
	}
#endif /* !NO_MACRO */
	return ((*funct)(FFNEGARG, nn));
}

/*
 * Insert a character.	While defining a macro, create a "LINE" containing
 * all inserted characters.
 */
int
selfinsert(int f, int n)
{
#ifndef NO_MACRO
	struct line	*lp;
#endif /* !NO_MACRO */
	int	 c;
	int	 count;

	if (n < 0)
		return (FALSE);
	if (n == 0)
		return (TRUE);
	c = key.k_chars[key.k_count - 1];
#ifndef NO_MACRO
	if (macrodef && macrocount < MAXMACRO) {
		if (f & FFARG)
			macrocount -= 2;

		/* last command was insert -- tack on the end */
		if (lastflag & CFINS) {
			macrocount--;
			/* Ensure the line can handle the new characters */
			if (maclcur->l_size < maclcur->l_used + n) {
				if (lrealloc(maclcur, maclcur->l_used + n) ==
				    FALSE)
					return (FALSE);
			}
			maclcur->l_used += n;
			/* Copy in the new data */
			for (count = maclcur->l_used - n;
			    count < maclcur->l_used; count++)
				maclcur->l_text[count] = c;
		} else {
			macro[macrocount - 1].m_funct = insert;
			if ((lp = lalloc(n)) == NULL)
				return (FALSE);
			lp->l_bp = maclcur;
			lp->l_fp = maclcur->l_fp;
			maclcur->l_fp = lp;
			maclcur = lp;
			for (count = 0; count < n; count++)
				lp->l_text[count] = c;
		}
		thisflag |= CFINS;
	}
#endif /* !NO_MACRO */
	if (c == '\n') {
		do {
			count = lnewline();
		} while (--n && count == TRUE);
		return (count);
	}

	/* overwrite mode */
	if (curbp->b_flag & BFOVERWRITE) {
		lchange(WFEDIT);
		while (curwp->w_doto < llength(curwp->w_dotp) && n--)
			lputc(curwp->w_dotp, curwp->w_doto++, c);
		if (n <= 0)
			return (TRUE);
	}
	return (linsert(n, c));
}

/*
 * This could be implemented as a keymap with everything defined as self-insert.
 */
int
quote(int f, int n)
{
	int	 c;

	key.k_count = 1;
	if ((key.k_chars[0] = getkey(TRUE)) >= '0' && key.k_chars[0] <= '7') {
		key.k_chars[0] -= '0';
		if ((c = getkey(TRUE)) >= '0' && c <= '7') {
			key.k_chars[0] <<= 3;
			key.k_chars[0] += c - '0';
			if ((c = getkey(TRUE)) >= '0' && c <= '7') {
				key.k_chars[0] <<= 3;
				key.k_chars[0] += c - '0';
			} else
				ungetkey(c);
		} else
			ungetkey(c);
	}
	return (selfinsert(f, n));
}
