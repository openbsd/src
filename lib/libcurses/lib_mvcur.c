
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


/*
**	lib_mvcur.c
**
**	The routines for moving the physical cursor and scrolling:
**
**		void _nc_mvcur_init(void), mvcur_wrap(void)
**
**		int mvcur(int old_y, int old_x, int new_y, int new_x)
**
**		void _nc_mvcur_wrap(void)
**
**		int _nc_mvcur_scrolln(int n, int top, int bot, int maxy)
**
** Comparisons with older movement optimizers:  
**    SVr3 curses mvcur() can't use cursor_to_ll or auto_left_margin.
**    4.4BSD curses can't use cuu/cud/cuf/cub/hpa/vpa/tab/cbt for local
** motions.  It doesn't use tactics based on auto_left_margin.  Weirdly
** enough, it doesn't use its own hardware-scrolling routine to scroll up
** destination lines for out-of-bounds addresses!
**    old ncurses optimizer: less accurate cost computations (in fact,
** it was broken and had to be commented out!).
**
** Compile with -DMAIN to build an interactive tester/timer for the movement
** optimizer.  You can use it to investigate the optimizer's behavior.
** You can also use it for tuning the formulas used to determine whether
** or not full optimization is attempted.
**
** This code has a nasty tendency to find bugs in terminfo entries, because it
** exercises the non-cup movement capabilities heavily.  If you think you've
** found a bug, try deleting subsets of the following capabilities (arranged
** in decreasing order of suspiciousness): it, tab, cbt, hpa, vpa, cuu, cud,
** cuf, cub, cuu1, cud1, cuf1, cub1.  It may be that one or more are wrong.
**
** Note: you should expect this code to look like a resource hog in a profile.
** That's because it does a lot of I/O, through the tputs() calls.  The I/O 
** cost swamps the computation overhead (and as machines get faster, this
** will become even more true).  Comments in the test exerciser at the end
** go into detail about tuning and how you can gauge the optimizer's
** effectiveness.
**/

/****************************************************************************
 *
 * Constants and macros for optimizer tuning.
 *
 ****************************************************************************/

/*
 * The average overhead of a full optimization computation in character
 * transmission times.  If it's too high, the algorithm will be a bit
 * over-biased toward using cup rather than local motions; if it's too
 * low, the algorithm may spend more time than is strictly optimal
 * looking for non-cup motions.  Profile the optimizer using the `t'
 * command of the exerciser (see below), and round to the nearest integer.
 *
 * Yes, I (esr) thought about computing expected overhead dynamically, say
 * by derivation from a running average of optimizer times.  But the
 * whole point of this optimization is to *decrease* the frequency of
 * system calls. :-)
 */
#define COMPUTE_OVERHEAD	1	/* I use a 90MHz Pentium @ 9.6Kbps */

/*
 * LONG_DIST is the distance we consider to be just as costly to move over as a
 * cup sequence is to emit.  In other words, it's the length of a cup sequence
 * adjusted for average computation overhead.  The magic number is the length
 * of "\033[yy;xxH", the typical cup sequence these days.
 */
#define LONG_DIST		(8 - COMPUTE_OVERHEAD)

/*
 * Tell whether a motion is optimizable by local motions.  Needs to be cheap to
 * compute. In general, all the fast moves go to either the right or left edge
 * of the screen.  So any motion to a location that is (a) further away than
 * LONG_DIST and (b) further inward from the right or left edge than LONG_DIST,
 * we'll consider nonlocal.
 */
#define NOT_LOCAL(fy, fx, ty, tx)	((tx > LONG_DIST) && (tx < screen_lines - 1 - LONG_DIST) && (abs(ty-fy) + abs(tx-fx) > LONG_DIST))

/****************************************************************************
 *
 * External interfaces
 *
 ****************************************************************************/

/*
 * For this code to work OK, the following components must live in the
 * screen structure:
 *
 *	int		_char_padding;	// cost of character put
 *	int		_cr_cost;	// cost of (carriage_return)
 *	int		_cup_cost;	// cost of (cursor_address)
 *	int		_home_cost;	// cost of (cursor_home)
 *	int		_ll_cost;	// cost of (cursor_to_ll)
 *#ifdef TABS_OK
 *	int		_ht_cost;	// cost of (tab)
 *	int		_cbt_cost;	// cost of (backtab)
 *#endif TABS_OK
 *	int		_cub1_cost;	// cost of (cursor_left)
 *	int		_cuf1_cost;	// cost of (cursor_right)
 *	int		_cud1_cost;	// cost of (cursor_down)
 *	int		_cuu1_cost;	// cost of (cursor_up)
 *	int		_cub_cost;	// cost of (parm_cursor_left)
 *	int		_cuf_cost;	// cost of (parm_cursor_right)
 *	int		_cud_cost;	// cost of (parm_cursor_down)
 *	int		_cuu_cost;	// cost of (parm_cursor_up)
 *	int		_hpa_cost;	// cost of (column_address)
 *	int		_vpa_cost;	// cost of (row_address)
 *
 * The TABS_OK switch controls whether it is reliable to use tab/backtabs
 * for local motions.  On many systems, it's not, due to uncertainties about
 * tab delays and whether or not tabs will be expanded in raw mode.  If you
 * have parm_right_cursor, tab motions don't win you a lot anyhow.
 */

#include "curses.priv.h"
#include "term.h"

#define NLMAPPING	SP->_nl			/* nl() on? */
#define RAWFLAG		SP->_raw		/* raw() on? */
#define CURRENT_ATTR	SP->_current_attr	/* current phys attribute */
#define CURRENT_ROW	SP->_cursrow		/* phys cursor row */
#define CURRENT_COLUMN	SP->_curscol		/* phys cursor column */
#define REAL_ATTR	SP->_current_attr	/* phys current attribute */
#define WANT_CHAR(y, x)	SP->_newscr->_line[y].text[x]	/* desired state */
#define BAUDRATE	SP->_baudrate		/* bits per second */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#ifdef TRACE
bool no_optimize;	/* suppress optimization */
#endif /* TRACE */

#ifdef MAIN
#include <sys/time.h>

static bool profiling = FALSE;
static float diff;
#endif /* MAIN */

#define OPT_SIZE 512

static char	*address_cursor;
static int	carriage_return_length;
static int	cursor_home_length;
static int	cursor_to_ll_length;

static void save_curs(void);
static void restore_curs(void);

/****************************************************************************
 *
 * Initialization/wrapup (including cost pre-computation)
 *
 ****************************************************************************/

#define INFINITY	1000000	/* too high to use */

static int cost(char *cap, int affcnt)
/* compute the cost of a given operation */
{
    if (cap == (char *)NULL)
	return(INFINITY);
    else
    {
	char	*cp;
	float	cum_cost = 0;	

	for (cp = cap; *cp; cp++)
	{
	    /* extract padding, either mandatory or required */
	    if (cp[0] == '$' && cp[1] == '<' && strchr(cp, '>'))
	    {
		float	number = 0;

		for (cp += 2; *cp != '>'; cp++)
		{
		    if (isdigit(*cp))
			number = number * 10 + (*cp - '0');
		    else if (*cp == '.')
			number += (*++cp - 10) / 10.0;
		    else if (*cp == '*')
			number *= affcnt;
		}

		cum_cost += number * 10;
	    }
	    else
		cum_cost += SP->_char_padding;
	}

	return((int)cum_cost);
    }
}

void _nc_mvcur_init(SCREEN *sp)
/* initialize the cost structure */
{
    /*
     * 9 = 7 bits + 1 parity + 1 stop.
     */
    if (BAUDRATE != 0)
    	SP->_char_padding = (9 * 1000 * 10) / BAUDRATE;
    else
    	SP->_char_padding = 9 * 1000 * 10 / 9600; /* use some default if baudrate == 0 */

    /* non-parameterized local-motion strings */
    SP->_cr_cost = cost(carriage_return, 0);
    SP->_home_cost = cost(cursor_home, 0);
    SP->_ll_cost = cost(cursor_to_ll, 0);
#ifdef TABS_OK
    SP->_ht_cost = cost(tab, 0);
    SP->_cbt_cost = cost(back_tab, 0);
#endif /* TABS_OK */
    SP->_cub1_cost = cost(cursor_left, 0);
    SP->_cuf1_cost = cost(cursor_right, 0);
    SP->_cud1_cost = cost(cursor_down, 0);
    SP->_cuu1_cost = cost(cursor_up, 0);

    /*
     * Assumption: if the terminal has memory_relative addressing, the
     * initialization strings or smcup will set single-page mode so we
     * can treat it like absolute screen addressing.  This seems to be true
     * for all cursor_mem_address terminal types in the terminfo database.
     */
    address_cursor = cursor_address ? cursor_address : cursor_mem_address;

    /*
     * Parametrized local-motion strings.  This static cost computation
     * depends on the following assumptions:
     *
     * (1) They never have * padding.  In the entire master terminfo database
     *     as of March 1995, only the obsolete Zenith Z-100 pc violates this.
     *	   (Proportional padding is found mainly in insert, delete and scroll
     *     capabilities).
     *
     * (2) The average case of cup has two two-digit parameters.  Strictly,
     *     the average case for a 24 * 80 screen has ((10*10*(1 + 1)) +
     *     (14*10*(1 + 2)) + (10*70*(2 + 1)) + (14*70*4)) / (24*80) = 3.458
     *     digits of parameters.  On a 25x80 screen the average is 3.6197.
     *     On larger screens the value gets much closer to 4.
     *
     * (3) The average case of cub/cuf/hpa has 2 digits of parameters
     *     (strictly, (((10 * 1) + (70 * 2)) / 80) = 1.8750).
     *
     * (4) The average case of cud/cuu/vpa has 2 digits of parameters
     *     (strictly, (((10 * 1) + (14 * 2)) / 24) = 1.5833).
     *
     * All these averages depend on the assumption that all parameter values
     * are equally probable.
     */
    SP->_cup_cost = cost(tparm(address_cursor, 23, 23), 1);
    SP->_cub_cost = cost(tparm(parm_left_cursor, 23), 1);
    SP->_cuf_cost = cost(tparm(parm_right_cursor, 23), 1);
    SP->_cud_cost = cost(tparm(parm_down_cursor, 23), 1);
    SP->_cuu_cost = cost(tparm(parm_up_cursor, 23), 1);
    SP->_hpa_cost = cost(tparm(column_address, 23), 1);
    SP->_vpa_cost = cost(tparm(row_address, 23), 1);

    /* initialize screen for cursor access */
    if (enter_ca_mode)
    {
	TPUTS_TRACE("enter_ca_mode");
	putp(enter_ca_mode);
    }

    /* pre-compute some capability lengths */
    carriage_return_length = carriage_return ? strlen(carriage_return) : 0;
    cursor_home_length     = cursor_home     ? strlen(cursor_home)     : 0;
    cursor_to_ll_length    = cursor_to_ll    ? strlen(cursor_to_ll)    : 0;
}

void _nc_mvcur_wrap(void)
/* wrap up cursor-addressing mode */
{
    /* change_scroll_region may trash the cursor location */
    save_curs();
    if (change_scroll_region)
    {
	TPUTS_TRACE("change_scroll_region");
	putp(tparm(change_scroll_region, 0, screen_lines - 1));
    }
    restore_curs();

    if (exit_ca_mode)
    {
	TPUTS_TRACE("exit_ca_mode");
	putp(exit_ca_mode);
    }
}

/****************************************************************************
 *
 * Optimized cursor movement
 *
 ****************************************************************************/

/*
 * Perform repeated-append, returning cost
 */
static __inline int
repeated_append (int total, int num, int repeat, char *dst, char *src)
{
	register size_t src_len = strlen(src);
	register size_t dst_len = 0;

	if (dst)
	    dst_len = strlen(dst);
	if ((dst_len + repeat * src_len) < OPT_SIZE-1) {
		total += (num * repeat);
		if (dst) {
		    dst += dst_len;
		    while (repeat-- > 0) {
			(void) strcpy(dst, src);
			dst += src_len;
		    }
		}
	} else {
		total = INFINITY;
	}
	return total;
}

#ifndef NO_OPTIMIZE
#define NEXTTAB(fr)	(fr + init_tabs - (fr % init_tabs))
#define LASTTAB(fr)	(fr - init_tabs + (fr % init_tabs))

/* Note: we'd like to inline this for speed, but GNU C barfs on the attempt. */

static int
relative_move(char *result, int from_y,int from_x,int to_y,int to_x, bool ovw)
/* move via local motions (cuu/cuu1/cud/cud1/cub1/cub/cuf1/cuf/vpa/hpa) */
{
    int		n, vcost = 0, hcost = 0;
    bool	used_lf = FALSE;

    if (result)
	result[0] = '\0';

    if (to_y != from_y)
    {
	vcost = INFINITY;

	if (row_address)
	{
	    if (result)
		(void) strcpy(result, tparm(row_address, to_y));
	    vcost = SP->_vpa_cost;
	}

	if (to_y > from_y)
	{
	    n = (to_y - from_y);

	    if (parm_down_cursor && SP->_cud_cost < vcost)
	    {
		if (result)
		    (void) strcpy(result, tparm(parm_down_cursor, n));
		vcost = SP->_cud_cost;
	    }

	    if (cursor_down && (n * SP->_cud1_cost < vcost))
	    {
		if (result)
		    result[0] = '\0';
		if (cursor_down[0] == '\n')
		    used_lf = TRUE;
		vcost = repeated_append(vcost, SP->_cud1_cost, n, result, cursor_down);
	    }
	}
	else /* (to_y < from_y) */
	{
	    n = (from_y - to_y);

	    if (parm_up_cursor && SP->_cup_cost < vcost)
	    {
		if (result)
		    (void) strcpy(result, tparm(parm_up_cursor, n));
		vcost = SP->_cup_cost;
	    }

	    if (cursor_up && (n * SP->_cuu1_cost < vcost))
	    {
		if (result)
		    result[0] = '\0';
		vcost = repeated_append(vcost, SP->_cuu1_cost, n, result, cursor_up);
	    }
	}

	if (vcost == INFINITY)
	    return(INFINITY);
    }

    /*
     * It may be that we're using a cud1 capability of \n with the
     * side-effect of taking the cursor to column 0.  Deal with this.
     */
    if (used_lf && NLMAPPING && !RAWFLAG)
	from_x = 0;

    if (result)
	result += strlen(result);

    if (to_x != from_x)
    {
	char	try[OPT_SIZE];

	hcost = INFINITY;

	if (column_address)
	{
	    if (result)
		(void) strcpy(result, tparm(column_address, to_x));
	    hcost = SP->_hpa_cost;
	}

	if (to_x > from_x)
	{
	    n = to_x - from_x;

	    if (parm_right_cursor && SP->_cuf_cost < hcost)
	    {
		if (result)
		    (void) strcpy(result, tparm(parm_right_cursor, n));
		hcost = SP->_cuf_cost;
	    }

	    if (cursor_right)
	    {
		int	lhcost = 0;

		try[0] = '\0';

#ifdef TABS_OK
		/* use hard tabs, if we have them, to do as much as possible */
		if (init_tabs > 0 && tab)
		{
		    int	nxt, fr;

		    for (fr = from_x; (nxt = NEXTTAB(fr)) <= to_x; fr = nxt)
		    {
			lhcost = repeated_append(lhcost, SP->_ht_cost, 1, try, tab);
			if (lhcost == INFINITY)
				break;
		    }

		    n = to_x - fr;
		    from_x = fr;
		}
#endif /* TABS_OK */

#if defined(REAL_ATTR) && defined(WANT_CHAR) && 0
		/*
		 * If we have no attribute changes, overwrite is cheaper.
		 * Note: must suppress this by passing in ovw = FALSE whenever
		 * WANT_CHAR would return invalid data.  In particular, this 
		 * is true between the time a hardware scroll has been done
		 * and the time the structure WANT_CHAR would access has been
		 * updated.
		 */
		if (ovw)
		{
		    int	i;

		    for (i = 0; i < n; i++)
			if ((WANT_CHAR(to_y, from_x + i) & A_ATTRIBUTES) != CURRENT_ATTR)
			{
			    ovw = FALSE;
			    break;
			}
		}
		if (ovw)
		{
		    char	*sp;
		    int	i;

		    sp = try + strlen(try);

		    for (i = 0; i < n; i++)
			*sp++ = WANT_CHAR(to_y, from_x + i);
		    *sp = '\0';
		    lhcost += n * SP->_char_padding;
	        }
		else
#endif /* defined(REAL_ATTR) && defined(WANT_CHAR) */
		{
		    lhcost = repeated_append(lhcost, SP->_cuf1_cost, n, try, cursor_right);
		}

		if (lhcost < hcost)
		{
		    if (result)
			(void) strcpy(result, try);
		    hcost = lhcost;
		}
	    }
	}
	else /* (to_x < from_x) */
	{
	    n = from_x - to_x;

	    if (parm_left_cursor && SP->_cub_cost < hcost)
	    {
		if (result)
		    (void) strcpy(result, tparm(parm_left_cursor, n));
		hcost = SP->_cub_cost;
	    }

	    if (cursor_left)
	    {
		int	lhcost = 0;

		try[0] = '\0';

#ifdef TABS_OK
		if (init_tabs > 0 && back_tab)
		{
		    int	nxt, fr;

		    for (fr = from_x; (nxt = LASTTAB(fr)) >= to_x; fr = nxt)
		    {
			lhcost = repeated_append(lhcost, SP->_cbt_cost, 1, try, back_tab);
			if (lhcost == INFINITY)
				break;
		    }

		    n = to_x - fr;
		}
#endif /* TABS_OK */

		lhcost = repeated_append(lhcost, SP->_cub1_cost, n, try, cursor_left);

		if (lhcost < hcost)
		{
		    if (result)
			(void) strcpy(result, try);
		    hcost = lhcost;
		}
	    }
	}

	if (hcost == INFINITY)
	    return(INFINITY);
    }

    return(vcost + hcost);
}
#endif /* !NO_OPTIMIZE */

/*
 * With the machinery set up above, it's conceivable that
 * onscreen_mvcur could be modified into a recursive function that does
 * an alpha-beta search of motion space, as though it were a chess
 * move tree, with the weight function being boolean and the search
 * depth equated to length of string.  However, this would jack up the
 * computation cost a lot, especially on terminals without a cup
 * capability constraining the search tree depth.  So we settle for
 * the simpler method below.
 */

static __inline int
onscreen_mvcur(int yold,int xold,int ynew,int xnew, bool ovw)
/* onscreen move from (yold, xold) to (ynew, xnew) */
{
    char	use[OPT_SIZE], *sp;
    int		tactic = 0, newcost, usecost = INFINITY;

#ifdef MAIN
    struct timeval before, after;

    gettimeofday(&before, NULL);
#endif /* MAIN */

    /* tactic #0: use direct cursor addressing */
    sp = tparm(address_cursor, ynew, xnew);
    if (sp)
    {
	tactic = 0;
	(void) strcpy(use, sp);
	usecost = SP->_cup_cost;

#ifdef TRACE
	if (no_optimize)
	    xold = yold = -1;
#endif /* TRACE */

	/*
	 * We may be able to tell in advance that the full optimization
	 * will probably not be worth its overhead.  Also, don't try to
	 * use local movement if the current attribute is anything but
	 * A_NORMAL...there are just too many ways this can screw up
	 * (like, say, local-movement \n getting mapped to some obscure
	 * character because A_ALTCHARSET is on).
	 */
	if (yold == -1 || xold == -1  || 
	    REAL_ATTR != A_NORMAL || NOT_LOCAL(yold, xold, ynew, xnew))
	{
#ifdef MAIN
	    if (!profiling)
	    {
		(void) fputs("nonlocal\n", stderr);
		goto nonlocal;	/* always run the optimizer if profiling */
	    }
#else
	    goto nonlocal;
#endif /* MAIN */
	}
    }

#ifndef NO_OPTIMIZE
    /* tactic #1: use local movement */
    if (yold != -1 && xold != -1
		&& ((newcost=relative_move(NULL, yold, xold, ynew, xnew, ovw))!=INFINITY)
		&& newcost < usecost)
    {
	tactic = 1;
	usecost = newcost;
    }

    /* tactic #2: use carriage-return + local movement */
    if (yold < screen_lines - 1 && xold < screen_columns - 1)
    {
	if (carriage_return
		&& ((newcost=relative_move(NULL, yold,0,ynew,xnew, ovw)) != INFINITY)
		&& SP->_cr_cost + newcost < usecost)
	{
	    tactic = 2;
	    usecost = SP->_cr_cost + newcost;
	}
    }

    /* tactic #3: use home-cursor + local movement */
    if (cursor_home
	&& ((newcost=relative_move(NULL, 0, 0, ynew, xnew, ovw)) != INFINITY)
	&& SP->_home_cost + newcost < usecost)
    {
	tactic = 3;
	usecost = SP->_home_cost + newcost;
    }

    /* tactic #4: use home-down + local movement */
    if (cursor_to_ll
    	&& ((newcost=relative_move(NULL, screen_lines-1, 0, ynew, xnew, ovw)) != INFINITY)
	&& SP->_ll_cost + newcost < usecost)
    {
	tactic = 4;
	usecost = SP->_ll_cost + newcost;
    }

    /*
     * tactic #5: use left margin for wrap to right-hand side,
     * unless strange wrap behavior indicated by xenl might hose us.
     */
    if (auto_left_margin && !eat_newline_glitch
	&& yold > 0 && yold < screen_lines - 1 && cursor_left
	&& ((newcost=relative_move(NULL, yold-1, screen_columns-1, ynew, xnew, ovw)) != INFINITY)
	&& SP->_cr_cost + SP->_cub1_cost + newcost + newcost < usecost)
    {
	tactic = 5;
	usecost = SP->_cr_cost + SP->_cub1_cost + newcost;
    }

    /*
     * These cases are ordered by estimated relative frequency.
     */
    if (tactic)
    {
	if (tactic == 1)
	    (void) relative_move(use, yold, xold, ynew, xnew, ovw);
	else if (tactic == 2)
	{
	    (void) strcpy(use, carriage_return);
	    (void) relative_move(use + carriage_return_length,
				 yold,0,ynew,xnew, ovw);
	}
	else if (tactic == 3)
	{
	    (void) strcpy(use, cursor_home);
	    (void) relative_move(use + cursor_home_length,
				 0, 0, ynew, xnew, ovw);
	}
	else if (tactic == 4)
	{
	    (void) strcpy(use, cursor_to_ll);
	    (void) relative_move(use + cursor_to_ll_length,
				 screen_lines-1, 0, ynew, xnew, ovw);
	}
	else /* if (tactic == 5) */
	{
	    use[0] = '\0';
	    if (xold > 0)
		(void) strcat(use, carriage_return);
	    (void) strcat(use, cursor_left);
	    (void) relative_move(use + strlen(use),
				 yold-1, screen_columns-1, ynew, xnew, ovw);
	}
    }
#endif /* !NO_OPTIMIZE */

#ifdef MAIN
    gettimeofday(&after, NULL);
    diff = after.tv_usec - before.tv_usec
	+ (after.tv_sec - before.tv_sec) * 1000000;
    if (!profiling)
	(void) fprintf(stderr, "onscreen: %d msec, %f 28.8Kbps char-equivalents\n",
		       (int)diff, diff/288);
#endif /* MAIN */

 nonlocal:
    if (usecost != INFINITY)
    {
	TPUTS_TRACE("mvcur");
	tputs(use, 1, _nc_outch);
	return(OK);
    }
    else
	return(ERR);
}

int mvcur(int yold, int xold, int ynew, int xnew)
/* optimized cursor move from (yold, xold) to (ynew, xnew) */
{
    TR(TRACE_MOVE, ("mvcur(%d,%d,%d,%d) called", yold, xold, ynew, xnew));

    if (yold == ynew && xold == xnew)
	return(OK);

    /*
     * Most work here is rounding for terminal boundaries getting the
     * column position implied by wraparound or the lack thereof and
     * rolling up the screen to get ynew on the screen.
     */

    if (xnew >= screen_columns)
    {
	ynew += xnew / screen_columns;
	xnew %= screen_columns;
    }
    if (xold >= screen_columns)
    {
	int	l;

	l = (xold + 1) / screen_columns;
	yold += l;
	xold %= screen_columns;
	if (!auto_right_margin)
	{
	    while (l > 0) {
		if (newline)
		{
		    TPUTS_TRACE("newline");
		    tputs(newline, 0, _nc_outch);
		}
		else
		    putchar('\n');
		l--;
	    }
	    xold = 0;
	}
	if (yold > screen_lines - 1)
	{
	    ynew -= yold - (screen_lines - 1);
	    yold = screen_lines - 1;
	}
    }

#ifdef CURSES_OVERRUN	/* not used, it takes us out of sync with curscr */
    /*
     * The destination line is offscreen. Try to scroll the screen to
     * bring it onscreen.  Note: this is not a documented feature of the
     * API.  It's here for compatibility with archaic curses code, a
     * feature no one seems to have actually used in a long time.
     */
    if (ynew >= screen_lines)
    {
	if (mvcur_scrolln((ynew - (screen_lines - 1)), 0, screen_lines - 1, screen_lines - 1) == OK)
	    ynew = screen_lines - 1;
	else
	    return(ERR);
    }
#endif /* CURSES_OVERRUN */

    /* destination location is on screen now */
    return(onscreen_mvcur(yold, xold, ynew, xnew, TRUE));
}

/****************************************************************************
 *
 * Cursor save_restore
 *
 ****************************************************************************/

/* assumption: sc/rc is faster than cursor addressing */

static int	oy, ox;		/* ugh, mvcur_scrolln() needs to see this */

static void save_curs(void)
{
    if (save_cursor && restore_cursor)
    {
	TPUTS_TRACE("save_cursor");
	putp(save_cursor);
    }

    oy = CURRENT_ROW;
    ox = CURRENT_COLUMN;
}

static void restore_curs(void)
{
    if (save_cursor && restore_cursor)
    {
	TPUTS_TRACE("restore_cursor");
	putp(restore_cursor);
    }
    else
	onscreen_mvcur(-1, -1, oy, ox, FALSE);
}

/****************************************************************************
 *
 * Physical-scrolling support
 *
 ****************************************************************************/

int _nc_mvcur_scrolln(int n, int top, int bot, int maxy)
/* scroll region from top to bot by n lines */
{
    int i;

    TR(TRACE_MOVE, ("mvcur_scrolln(%d, %d, %d, %d)", n, top, bot, maxy));

    save_curs();

    /*
     * This code was adapted from Keith Bostic's hardware scrolling
     * support for 4.4BSD curses.  I (esr) translated it to use terminfo
     * capabilities, narrowed the call interface slightly, and cleaned
     * up some convoluted tests.  I also added support for the memory_above
     * memory_below, and non_dest_scroll_region capabilities.
     *
     * For this code to work, we must have either
     * change_scroll_region and scroll forward/reverse commands, or
     * insert and delete line capabilities.
     * When the scrolling region has been set, the cursor has to
     * be at the last line of the region to make the scroll
     * happen.
     *
     * This code makes one aesthetic decision in the opposite way from
     * BSD curses.  BSD curses preferred pairs of il/dl operations
     * over scrolls, allegedly because il/dl looked faster.  We, on
     * the other hand, prefer scrolls because (a) they're just as fast
     * on modern terminals and (b) using them avoids bouncing an
     * unchanged bottom section of the screen up and down, which is
     * visually nasty.
     */
    if (n > 0)
    {
	/*
	 * Do explicit clear to end of region if it's possible that the
	 * terminal might hold on to stuff we push off the end.
	 */
	if (non_dest_scroll_region || (memory_below && bot == maxy))
	{
	    if (bot == maxy && clr_eos)
	    {
		mvcur(-1, -1, lines - n, 0);
		TPUTS_TRACE("clr_eos");
		tputs(clr_eos, n, _nc_outch);
	    }
	    else if (clr_eol)
	    {
		for (i = 0; i < n; i++)
		{
		    mvcur(-1, -1, lines - n + i, 0);
		    TPUTS_TRACE("clr_eol");
		    tputs(clr_eol, n, _nc_outch);
		}
	    }
	}

	if (change_scroll_region && (scroll_forward || parm_index))
	{
	    TPUTS_TRACE("change_scroll_region");
	    tputs(tparm(change_scroll_region, top, bot), 0, _nc_outch);
	    onscreen_mvcur(-1, -1, bot, 0, TRUE);
	    if (parm_index != NULL)
	    {
		TPUTS_TRACE("parm_index");
		tputs(tparm(parm_index, n, 0), n, _nc_outch);
	    }
	    else
		for (i = 0; i < n; i++)
		{
		    TPUTS_TRACE("scroll_forward");
		    tputs(scroll_forward, 0, _nc_outch);
		}
	    TPUTS_TRACE("change_scroll_region");
	    tputs(tparm(change_scroll_region, 0, maxy), 0, _nc_outch);
	    restore_curs();
	    return(OK);
	}

	/* Scroll up the block. */
	if (parm_index && top == 0)
	{
	    onscreen_mvcur(oy, ox, bot, 0, TRUE);
	    TPUTS_TRACE("parm_index");
	    tputs(tparm(parm_index, n, 0), n, _nc_outch);
	}
	else if (parm_delete_line)
	{
	    onscreen_mvcur(oy, ox, top, 0, TRUE);
	    TPUTS_TRACE("parm_delete_line");
	    tputs(tparm(parm_delete_line, n, 0), n, _nc_outch);
	}
	else if (delete_line)
	{
	    onscreen_mvcur(oy, ox, top, 0, TRUE);
	    for (i = 0; i < n; i++)
	    {
		TPUTS_TRACE("parm_index");
		tputs(delete_line, 0, _nc_outch);
	    }
	}
	else if (scroll_forward && top == 0)
	{
	    onscreen_mvcur(oy, ox, bot, 0, TRUE);
	    for (i = 0; i < n; i++)
	    {
		TPUTS_TRACE("scroll_forward");
		tputs(scroll_forward, 0, _nc_outch);
	    }
	}
	else
	    return(ERR);

	/* Push down the bottom region. */
	if (parm_insert_line)
	{
	    onscreen_mvcur(top, 0, bot - n + 1, 0, FALSE);
	    TPUTS_TRACE("parm_insert_line");
	    tputs(tparm(parm_insert_line, n, 0), n, _nc_outch);
	}
	else if (insert_line)
	{
	    onscreen_mvcur(top, 0, bot - n + 1, 0, FALSE);
	    for (i = 0; i < n; i++)
	    {
		TPUTS_TRACE("insert_line");
		tputs(insert_line, 0, _nc_outch);
	    }
	}
	else
	    return(ERR);
	restore_curs();
    }
    else /* (n < 0) */
    {
	/*
	 * Explicitly clear if stuff pushed off top of region might 
	 * be saved by the terminal.
	 */
	if (non_dest_scroll_region || (memory_above && top == 0))
	    for (i = 0; i < n; i++)
	    {
		mvcur(-1, -1, i, 0);
		TPUTS_TRACE("clr_eol");
		tputs(clr_eol, n, _nc_outch);
	    }

	if (change_scroll_region && (scroll_reverse || parm_rindex))
	{
	    TPUTS_TRACE("change_scroll_region");
	    tputs(tparm(change_scroll_region, top, bot), 0, _nc_outch);
	    onscreen_mvcur(-1, -1, top, 0, TRUE);
	    if (parm_rindex)
	    {
		TPUTS_TRACE("parm_rindex");
		tputs(tparm(parm_rindex, -n, 0), -n, _nc_outch);
	    }
	    else
		for (i = n; i < 0; i++)
		{
		    TPUTS_TRACE("scroll_reverse");
		    tputs(scroll_reverse, 0, _nc_outch);
		}
	    TPUTS_TRACE("change_scroll_region");
	    tputs(tparm(change_scroll_region, 0, maxy), 0, _nc_outch);
	    restore_curs();
	    return(OK);
	}

	/* Preserve the bottom lines. */
	onscreen_mvcur(oy, ox, bot + n + 1, 0, TRUE);
	if (parm_rindex && bot == maxy)
	{
	    TPUTS_TRACE("parm_rindex");
	    tputs(tparm(parm_rindex, -n, 0), -n, _nc_outch);
	}
	else if (parm_delete_line)
	{
	    TPUTS_TRACE("parm_delete_line");
	    tputs(tparm(parm_delete_line, -n, 0), -n, _nc_outch);
	}
	else if (delete_line)
	    for (i = n; i < 0; i++)
	    {
		TPUTS_TRACE("delete_line");
		tputs(delete_line, 0, _nc_outch);
	    }
	else if (scroll_reverse && bot == maxy)
	    for (i = n; i < 0; i++)
	    {
		TPUTS_TRACE("scroll_reverse");
		tputs(scroll_reverse, 0, _nc_outch);
	    }
	else
	    return(ERR);

	/* Scroll the block down. */
	if (parm_insert_line)
	{
	    onscreen_mvcur(bot + n + 1, 0, top, 0, FALSE);
	    TPUTS_TRACE("parm_insert_line");
	    tputs(tparm(parm_insert_line, -n, 0), -n, _nc_outch);
	}
	else if (insert_line)
	{
	    onscreen_mvcur(bot + n + 1, 0, top, 0, FALSE);
	    for (i = n; i < 0; i++)
	    {
		TPUTS_TRACE("insert_line");
		tputs(insert_line, 0, _nc_outch);
	    }
	}
	else
	    return(ERR);
	restore_curs();
    }

    return(OK);
}

#ifdef MAIN
/****************************************************************************
 *
 * Movement optimizer test code
 *
 ****************************************************************************/

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include "tic.h"
#include "dump_entry.h"

char *_nc_progname = "mvcur";

static unsigned long xmits;

int tputs(const char *string, int affcnt, int (*outc)(int))
/* stub tputs() that dumps sequences in a visible form */
{
    if (profiling)
	xmits += strlen(string);
    else
	(void) fputs(_nc_visbuf(string), stdout);
    return(OK);
}

int putp(const char *string)
{
    return(tputs(string, 1, _nc_outch));
}

int _nc_outch(int ch)
{
    putc(ch, stdout);
    return OK;
}

static char	tname[BUFSIZ];

static void load_term(void)
{
    (void) setupterm(tname, STDOUT_FILENO, NULL);
}

static int roll(int n)
{
    int i, j;

    i = (RAND_MAX / n) * n;
    while ((j = rand()) >= i)
	continue;
    return (j % n);
}

int main(int argc, char *argv[])
{
    (void) strncpy(tname, getenv("TERM"), sizeof tname-1);
    load_term();
    _nc_setupscreen(lines, columns);
    baudrate();

    _nc_mvcur_init(SP);
#if HAVE_SETVBUF || HAVE_SETBUFFER
    /*
     * Undo the effects of our optimization hack, otherwise our interactive
     * prompts don't flush properly.
     */
#if HAVE_SETVBUF
    (void) setvbuf(SP->_ofp, malloc(BUFSIZ), _IOLBF, BUFSIZ);
#elif HAVE_SETBUFFER
    (void) setbuffer(SP->_ofp, malloc(BUFSIZ), BUFSIZ);
#endif
#endif /* HAVE_SETVBUF || HAVE_SETBUFFER */

    (void) puts("The mvcur tester.  Type ? for help");

    fputs("smcup:", stdout);
    putchar('\n');

    for (;;)
    {
	int	fy, fx, ty, tx, n, i;
	char	buf[BUFSIZ], capname[BUFSIZ];

	(void) fputs("> ", stdout);
	(void) fgets(buf, sizeof(buf), stdin);

	if (buf[0] == '?')
	{
(void) puts("?                -- display this help message");
(void) puts("fy fx ty tx      -- (4 numbers) display (fy,fx)->(ty,tx) move");
(void) puts("s[croll] n t b m -- display scrolling sequence");
(void) printf("r[eload]         -- reload terminal info for %s\n",
	      getenv("TERM"));
(void) puts("l[oad] <term>    -- load terminal info for type <term>");
(void) puts("nl               -- assume NL -> CR/LF when computing (default)");
(void) puts("nonl             -- don't assume NL -> CR/LF when computing");
(void) puts("d[elete] <cap>   -- delete named capability");
(void) puts("i[nspect]        -- display terminal capabilities");
(void) puts("c[ost]           -- dump cursor-optimization cost table");
(void) puts("o[optimize]      -- toggle movement optimization");
(void) puts("t[orture] <num>  -- torture-test with <num> random moves");
(void) puts("q[uit]           -- quit the program");
	}
	else if (sscanf(buf, "%d %d %d %d", &fy, &fx, &ty, &tx) == 4)
	{
	    struct timeval before, after;

	    putchar('"');

	    gettimeofday(&before, NULL);
	    mvcur(fy, fx, ty, tx);
	    gettimeofday(&after, NULL);

	    printf("\" (%ld msec)\n",
		after.tv_usec - before.tv_usec + (after.tv_sec - before.tv_sec) * 1000000);
	}
	else if (sscanf(buf, "s %d %d %d %d", &fy, &fx, &ty, &tx) == 4)
	{
	    struct timeval before, after;

	    putchar('"');

	    gettimeofday(&before, NULL);
	    _nc_mvcur_scrolln(fy, fx, ty, tx);
	    gettimeofday(&after, NULL);

	    printf("\" (%ld msec)\n",
		after.tv_usec - before.tv_usec + (after.tv_sec - before.tv_sec) * 1000000);
	}
	else if (buf[0] == 'r')
	{
	    (void) strncpy(tname, getenv("TERM"), sizeof tname-1);
	    load_term();
	}
	else if (sscanf(buf, "l %s", tname) == 1)
	{
	    load_term();
	}
	else if (strncmp(buf, "nl", 2) == 0)
	{
	    NLMAPPING = TRUE;
	    (void) puts("NL -> CR/LF will be assumed.");
	}
	else if (strncmp(buf, "nonl", 4) == 0)
	{
	    NLMAPPING = FALSE;
	    (void) puts("NL -> CR/LF will not be assumed.");
	}
	else if (sscanf(buf, "d %s", capname) == 1)
	{
	    struct name_table_entry const	*np = _nc_find_entry(capname,
							 _nc_info_hash_table);

	    if (np == NULL)
		(void) printf("No such capability as \"%s\"\n", capname);
	    else
	    {
		switch(np->nte_type)
		{
		case BOOLEAN:
		    cur_term->type.Booleans[np->nte_index] = FALSE;
		    (void) printf("Boolean capability `%s' (%d) turned off.\n",
				  np->nte_name, np->nte_index);
		    break;

		case NUMBER:
		    cur_term->type.Numbers[np->nte_index] = -1;
		    (void) printf("Number capability `%s' (%d) set to -1.\n",
				  np->nte_name, np->nte_index);
		    break;

		case STRING:
		    cur_term->type.Strings[np->nte_index] = (char *)NULL;
		    (void) printf("String capability `%s' (%d) deleted.\n",
				  np->nte_name, np->nte_index);
		    break;
		}
	    }
	}
	else if (buf[0] == 'i')
	{
	     dump_init((char *)NULL, F_TERMINFO, S_TERMINFO, 70, 0);
	     dump_entry(&cur_term->type, NULL);
	     putchar('\n');
	}
	else if (buf[0] == 'o')
	{
	     if (no_optimize)
	     {
		 no_optimize = FALSE;
		 (void) puts("Optimization is now on.");
	     }
	     else
	     {
		 no_optimize = TRUE;
		 (void) puts("Optimization is now off.");
	     }
	}
	/* 
	 * You can use the `t' test to profile and tune the movement
	 * optimizer.  Use iteration values in three digits or more.
	 * At above 5000 iterations the profile timing averages are stable
	 * to within a millisecond or three.
	 *
	 * The `overhead' field of the report will help you pick a
	 * COMPUTE_OVERHEAD figure appropriate for your processor and
	 * expected line speed.  The `total estimated time' is
	 * computation time plus a character-transmission time
	 * estimate computed from the number of transmits and the baud
	 * rate.
	 *
	 * Use this together with the `o' command to get a read on the
	 * optimizer's effectiveness.  Compare the total estimated times
	 * for `t' runs of the same length in both optimized and un-optimized
	 * modes.  As long as the optimized times are less, the optimizer
	 * is winning.
	 */
	else if (sscanf(buf, "t %d", &n) == 1)
	{
	    float cumtime = 0, perchar;
	    int speeds[] = {2400, 9600, 14400, 19200, 28800, 38400, 0};

	    srand((unsigned)(getpid() + time((time_t *)0)));
	    profiling = TRUE;
	    xmits = 0;
	    for (i = 0; i < n; i++)
	    {
		/* 
		 * This does a move test between two random locations,
		 * Random moves probably short-change the optimizer,
		 * which will work better on the short moves probably
		 * typical of doupdate()'s usage pattern.  Still,
		 * until we have better data...
		 */
#ifdef FIND_COREDUMP
		int from_y = roll(lines);
		int to_y = roll(lines);
		int from_x = roll(columns);
		int to_x = roll(columns);

		printf("(%d,%d) -> (%d,%d)\n", from_y, from_x, to_y, to_x);
		mvcur(from_y, from_x, to_y, to_x);
#else
		mvcur(roll(lines), roll(columns), roll(lines), roll(columns));
#endif /* FIND_COREDUMP */
		if (diff)
		    cumtime += diff;
	    }
	    profiling = FALSE;

	    /*
	     * Average milliseconds per character optimization time.
	     * This is the key figure to watch when tuning the optimizer.
	     */
	    perchar = cumtime / n;

	    (void) printf("%d moves (%ld chars) in %d msec, %f msec each:\n", 
			  n, xmits, (int)cumtime, perchar);

	    for (i = 0; speeds[i]; i++)
	    {
		/*
		 * Total estimated time for the moves, computation and 
		 * transmission both. Transmission time is an estimate
		 * assuming 9 bits/char, 8 bits + 1 stop bit.
		 */
		float totalest = cumtime + xmits * 9 * 1e6 / speeds[i];

		/*
		 * Per-character optimization overhead in character transmits
		 * at the current speed.  Round this to the nearest integer
		 * to figure COMPUTE_OVERHEAD for the speed.
		 */
		float overhead = speeds[i] * perchar / 1e6;

		(void) printf("%6d bps: %3.2f char-xmits overhead; total estimated time %15.2f\n",
			      speeds[i], overhead, totalest);
	    }
	}
	else if (buf[0] == 'c')
	{
	    (void) printf("char padding: %d\n", SP->_char_padding);
	    (void) printf("cr cost: %d\n", SP->_cr_cost);
	    (void) printf("cup cost: %d\n", SP->_cup_cost);
	    (void) printf("home cost: %d\n", SP->_home_cost);
	    (void) printf("ll cost: %d\n", SP->_ll_cost);
#ifdef TABS_OK
	    (void) printf("ht cost: %d\n", SP->_ht_cost);
	    (void) printf("cbt cost: %d\n", SP->_cbt_cost);
#endif /* TABS_OK */
	    (void) printf("cub1 cost: %d\n", SP->_cub1_cost);
	    (void) printf("cuf1 cost: %d\n", SP->_cuf1_cost);
	    (void) printf("cud1 cost: %d\n", SP->_cud1_cost);
	    (void) printf("cuu1 cost: %d\n", SP->_cuu1_cost);
	    (void) printf("cub cost: %d\n", SP->_cub_cost);
	    (void) printf("cuf cost: %d\n", SP->_cuf_cost);
	    (void) printf("cud cost: %d\n", SP->_cud_cost);
	    (void) printf("cuu cost: %d\n", SP->_cuu_cost);
	    (void) printf("hpa cost: %d\n", SP->_hpa_cost);
	    (void) printf("vpa cost: %d\n", SP->_vpa_cost);
	}
	else if (buf[0] == 'x' || buf[0] == 'q')
	    break;
	else
	    (void) puts("Invalid command.");
    }

    (void) fputs("rmcup:", stdout);
    _nc_mvcur_wrap();
    putchar('\n');

    return(0);
}

#endif /* MAIN */

/* lib_mvcur.c ends here */
