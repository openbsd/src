
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/******************************************************************************

NAME
   hashmap.c -- fill in scramble vector based on text hashes

SYNOPSIS
   void _nc_hash_map(void)

DESCRIPTION:
   This code attempts to recognize pairs of old and new lines in the physical
and virtual screens.  When a line pair is recognized, the old line index is
placed in the oldindex member of the virtual screen line, to be used by the
vertical-motion optimizer portion of the update logic (see hardscroll.c).

   Line pairs are recognized by applying a modified Heckel's algorithm,
sped up by hashing.  If a line hash is unique in both screens, those
lines must be a pair.  If the hashes of the two lines immediately following
lines known to be a pair are the same, the following lines are also a pair.
We apply these rules repeatedly until no more pairs are found.  The
modifications stem from the fact that there may already be oldindex info
associated with the virtual screen, which has to be respected.

   We don't worry about false pairs produced by hash collisions, on the
assumption that such cases are rare and will only make the latter stages
of update less efficient, not introduce errors.

HOW TO TEST THIS:

Use the following production:

hashmap: hashmap.c
	$(CC) -g -DHASHDEBUG hashmap.c hardscroll.c ../objects/lib_trace.o -o hashmap

AUTHOR
    Eric S. Raymond <esr@snark.thyrsus.com>, May 1996

*****************************************************************************/

#include <curses.priv.h>

MODULE_ID("Id: hashmap.c,v 1.12 1997/05/03 20:30:06 tom Exp $")

#ifdef HASHDEBUG
#define LINES	24
#define TEXTWIDTH	1
int oldnums[LINES], reallines[LINES];
static chtype oldtext[LINES][TEXTWIDTH], newtext[LINES][TEXTWIDTH];
#define OLDNUM(n)	oldnums[n]
#define REAL(m)		reallines[m]
#define OLDTEXT(n)	oldtext[n]
#define NEWTEXT(m)	newtext[m]
#undef T
#define T(x)		(void) printf x ; (void) putchar('\n');
#else
#include <curses.h>
#define OLDNUM(n)	newscr->_line[n].oldindex
#define REAL(m)		curscr->_line[m].oldindex
#define OLDTEXT(n)	curscr->_line[n].text
#define NEWTEXT(m)	newscr->_line[m].text
#define TEXTWIDTH	(curscr->_maxx+1)
#ifndef _NEWINDEX
#define _NEWINDEX	-1
#endif /* _NEWINDEX */
#endif /* HASHDEBUG */

/* Chris Torek's hash function (from his DB package). */
static inline unsigned long hash4(const void *key, size_t len)
{
    register long h, loop;
    register unsigned const char *k;

#define HASH4a   h = (h << 5) - h + *k++;
#define HASH4b   h = (h << 5) + h + *k++;
#define HASH4 HASH4b
    h = 0;
    k = (unsigned const char *)key;
    if (len > 0) {
	loop = (len + 8 - 1) >> 3;
	switch (len & (8 - 1)) {
	case 0:
	    do {	/* All fall throughs */
		HASH4;
	    case 7:
		HASH4;
	    case 6:
		HASH4;
	    case 5:
		HASH4;
	    case 4:
		HASH4;
	    case 3:
		HASH4;
	    case 2:
		HASH4;
	    case 1:
		HASH4;
	    } while (--loop);
	}
    }
    return ((unsigned long)h);
}

static inline unsigned long hash(chtype *text)
{
    return(hash4(text, TEXTWIDTH*sizeof(*text)));
}

void _nc_hash_map(void)
{
    typedef struct
    {
	unsigned long	hashval;
	int		oldcount, newcount;
	int		oldindex, newindex;
    }
    sym;
    sym hashtab[MAXLINES*2], *sp;
    register int i;
    long oldhash[MAXLINES], newhash[MAXLINES];
    bool keepgoing;

    /*
     * Set up and count line-hash values.
     */
    memset(hashtab, '\0', sizeof(sym) * MAXLINES);
    for (i = 0; i < LINES; i++)
    {
	unsigned long hashval = hash(OLDTEXT(i));

	for (sp = hashtab; sp->hashval; sp++)
	    if (sp->hashval == hashval)
		break;
	sp->hashval = hashval;	/* in case this is a new entry */
	oldhash[i] = hashval;
	sp->oldcount++;
	sp->oldindex = i;
    }
    for (i = 0; i < LINES; i++)
    {
	unsigned long hashval = hash(NEWTEXT(i));

	for (sp = hashtab; sp->hashval; sp++)
	    if (sp->hashval == hashval)
		break;
	sp->hashval = hashval;	/* in case this is a new entry */
	newhash[i] = hashval;
	sp->newcount++;
	sp->newindex = i;
    
	OLDNUM(i) = _NEWINDEX;
    }

    /*
     * Mark line pairs corresponding to unique hash pairs.
     * Note: we only do this where the new line doesn't
     * already have a valid oldindex -- this way we preserve the
     * information left in place by the software scrolling functions.
     */
    for (sp = hashtab; sp->hashval; sp++)
	if (sp->oldcount == 1 && sp->newcount == 1
	    && OLDNUM(sp->newindex) == _NEWINDEX)
	{
	    TR(TRACE_UPDATE | TRACE_MOVE,
	       ("new line %d is hash-identical to old line %d (unique)",
		   sp->newindex, sp->oldindex));
	    OLDNUM(sp->newindex) = sp->oldindex;
	}

    /*
     * Now for the tricky part.  We have unique pairs to use as anchors.
     * Use these to deduce the presence of spans of identical lines.
     */
    do {
	keepgoing = FALSE;

	for (i = 0; i < LINES-1; i++)
	    if (OLDNUM(i) != _NEWINDEX && OLDNUM(i+1) == _NEWINDEX)
	    {
		if (OLDNUM(i) + 1 < LINES
			    && newhash[i+1] == oldhash[OLDNUM(i) + 1])
		{
		    OLDNUM(i+1) = OLDNUM(i) + 1;
		    TR(TRACE_UPDATE | TRACE_MOVE,
		       ("new line %d is hash-identical to old line %d (forward continuation)",
			i+1, OLDNUM(i) + 1));
		    keepgoing = TRUE;
		}
	    }

	for (i = 0; i < LINES-1; i++)
	    if (OLDNUM(i) != _NEWINDEX && OLDNUM(i-1) == _NEWINDEX)
	    {
		if (OLDNUM(i) - 1 >= 0
			    && newhash[i-1] == oldhash[OLDNUM(i) - 1])
		{
		    OLDNUM(i-1) = OLDNUM(i) - 1;
		    TR(TRACE_UPDATE | TRACE_MOVE,
		       ("new line %d is hash-identical to old line %d (backward continuation)",
			i-1, OLDNUM(i) - 1));
		    keepgoing = TRUE;
		}
	    }
    } while
	(keepgoing);
}

#ifdef HASHDEBUG

int
main(int argc GCC_UNUSED, char *argv[] GCC_UNUSED)
{
    extern void	_nc_linedump(void);
    char	line[BUFSIZ], *st;
    int		n;

    for (n = 0; n < LINES; n++)
    {
	reallines[n] = n;
	oldnums[n] = _NEWINDEX;
	oldtext[n][0] = newtext[n][0] = '.';
    }

    _nc_tracing = TRACE_MOVE;
    for (;;)
    {
	/* grab a test command */
	if (fgets(line, sizeof(line), stdin) == (char *)NULL)
	    exit(EXIT_SUCCESS);

	switch(line[0])
	{
	case '#':	/* comment */
	    (void) fputs(line, stderr);
	    break;

	case 'l':	/* get initial line number vector */
	    for (n = 0; n < LINES; n++)
	    {
		reallines[n] = n;
		oldnums[n] = _NEWINDEX;
	    }
	    n = 0;
	    st = strtok(line, " ");
	    do {
		oldnums[n++] = atoi(st);
	    } while
		((st = strtok((char *)NULL, " ")) != 0);
	    break;

	case 'n':	/* use following letters as text of new lines */
	    for (n = 0; n < LINES; n++)
		newtext[n][0] = '.';
	    for (n = 0; n < LINES; n++)
		if (line[n+1] == '\n')
		    break;
		else
		    newtext[n][0] = line[n+1];
	    break;

	case 'o':	/* use following letters as text of old lines */
	    for (n = 0; n < LINES; n++)
		oldtext[n][0] = '.';
	    for (n = 0; n < LINES; n++)
		if (line[n+1] == '\n')
		    break;
		else
		    oldtext[n][0] = line[n+1];
	    break;

	case 'd':	/* dump state of test arrays */
	    _nc_linedump();
	    (void) fputs("Old lines: [", stdout);
	    for (n = 0; n < LINES; n++)
		putchar(oldtext[n][0]);
	    putchar(']');
	    putchar('\n');
	    (void) fputs("New lines: [", stdout);
	    for (n = 0; n < LINES; n++)
		putchar(newtext[n][0]);
	    putchar(']');
	    putchar('\n');
	    break;

	case 'h':	/* apply hash mapper and see scroll optimization */
	    _nc_hash_map();
	    (void) fputs("Result:\n", stderr);
	    _nc_linedump();
	    _nc_scroll_optimize();
	    (void) fputs("Done.\n", stderr);
	    break;
	}
    }
    return EXIT_SUCCESS;
}

#endif /* HASHDEBUG */

/* hashmap.c ends here */
