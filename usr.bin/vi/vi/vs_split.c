/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)vs_split.c	10.24 (Berkeley) 5/4/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "vi.h"

/*
 * vs_split --
 *	Create a new screen.
 *
 * PUBLIC: int vs_split __P((SCR *, SCR *, int));
 */
int
vs_split(sp, new, ccl)
	SCR *sp, *new;
	int ccl;		/* Colon-command line split. */
{
	GS *gp;
	SMAP *smp;
	size_t half;
	int issmallscreen, splitup;

	gp = sp->gp;

	/* Check to see if it's possible. */
	if (sp->rows < 4) {
		msgq(sp, M_ERR,
		    "222|Screen must be larger than %d lines to split", 4 - 1);
		return (1);
	}
	half = sp->rows / 2;
	if (ccl && half > 6)
		half = 6;

	/* Get a new screen map. */
	CALLOC(sp, _HMAP(new), SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	if (_HMAP(new) == NULL)
		return (1);
	_HMAP(new)->lno = sp->lno;
	_HMAP(new)->coff = 0;
	_HMAP(new)->soff = 1;

	/*
	 * Small screens: see vs_refresh.c section 6a.  Set a flag so
	 * we know to fix the screen up later.
	 */
	issmallscreen = IS_SMALL(sp);

	/* The columns in the screen don't change. */
	new->cols = sp->cols;

	/*
	 * Split the screen, and link the screens together.  If creating a
	 * screen to edit the colon command line or the cursor is in the top
	 * half of the current screen, the new screen goes under the current
	 * screen.  Else, it goes above the current screen.
	 *
	 * Recalculate current cursor position based on sp->lno, we're called
	 * with the cursor on the colon command line.  Then split the screen
	 * in half and update the shared information.
	 */
	splitup = !ccl && (vs_sm_cursor(sp, &smp) ? 0 : smp - HMAP) > half;
	if (splitup) {				/* Old is bottom half. */
		new->rows = sp->rows - half;	/* New. */
		new->woff = sp->woff;
		sp->rows = half;		/* Old. */
		sp->woff += new->rows;
						/* Link in before old. */
		CIRCLEQ_INSERT_BEFORE(&gp->dq, sp, new, q);

		/*
		 * If the parent is the bottom half of the screen, shift
		 * the map down to match on-screen text.
		 */
		memmove(_HMAP(sp), _HMAP(sp) + new->rows,
		    (sp->t_maxrows - new->rows) * sizeof(SMAP));
	} else {				/* Old is top half. */
		new->rows = half;		/* New. */
		sp->rows -= half;		/* Old. */
		new->woff = sp->woff + sp->rows;
						/* Link in after old. */
		CIRCLEQ_INSERT_AFTER(&gp->dq, sp, new, q);
	}

	/* Adjust maximum text count. */
	sp->t_maxrows = IS_ONELINE(sp) ? 1 : sp->rows - 1;
	new->t_maxrows = IS_ONELINE(new) ? 1 : new->rows - 1;

	/*
	 * Small screens: see vs_refresh.c, section 6a.
	 *
	 * The child may have different screen options sizes than the parent,
	 * so use them.  Guarantee that text counts aren't larger than the
	 * new screen sizes.
	 */
	if (issmallscreen) {
		/* Fix the text line count for the parent. */
		if (splitup)
			sp->t_rows -= new->rows;

		/* Fix the parent screen. */
		if (sp->t_rows > sp->t_maxrows)
			sp->t_rows = sp->t_maxrows;
		if (sp->t_minrows > sp->t_maxrows)
			sp->t_minrows = sp->t_maxrows;

		/* Fix the child screen. */
		new->t_minrows = new->t_rows = O_VAL(sp, O_WINDOW);
		if (new->t_rows > new->t_maxrows)
			new->t_rows = new->t_maxrows;
		if (new->t_minrows > new->t_maxrows)
			new->t_minrows = new->t_maxrows;
	} else {
		sp->t_minrows = sp->t_rows = IS_ONELINE(sp) ? 1 : sp->rows - 1;

		/*
		 * The new screen may be a small screen, even if the parent
		 * was not.  Don't complain if O_WINDOW is too large, we're
		 * splitting the screen so the screen is much smaller than
		 * normal.
		 */
		new->t_minrows = new->t_rows = O_VAL(sp, O_WINDOW);
		if (new->t_rows > new->rows - 1)
			new->t_minrows = new->t_rows =
			    IS_ONELINE(new) ? 1 : new->rows - 1;
	}

	/* Adjust the ends of the new and old maps. */
	_TMAP(sp) = IS_ONELINE(sp) ?
	    _HMAP(sp) : _HMAP(sp) + (sp->t_rows - 1);
	_TMAP(new) = IS_ONELINE(new) ?
	    _HMAP(new) : _HMAP(new) + (new->t_rows - 1);

	/* Reset the length of the default scroll. */
	if ((sp->defscroll = sp->t_maxrows / 2) == 0)
		sp->defscroll = 1;
	if ((new->defscroll = new->t_maxrows / 2) == 0)
		new->defscroll = 1;

	/* Draw the new screen from scratch, and add a status line. */
	F_SET(new, SC_SCR_REFORMAT | SC_STATUS);

	return (0);
}

/*
 * vs_discard --
 *	Discard the screen, folding the real-estate into a related screen,
 *	if one exists, and return that screen.
 *
 * PUBLIC: int vs_discard __P((SCR *, SCR **));
 */
int
vs_discard(sp, spp)
	SCR *sp, **spp;
{
	SCR *nsp;
	dir_t dir;

	/*
	 * Save the old screen's cursor information.
	 *
	 * XXX
	 * If called after file_end(), and the underlying file was a tmp
	 * file, it may have gone away.
	 */
	if (sp->frp != NULL) {
		sp->frp->lno = sp->lno;
		sp->frp->cno = sp->cno;
		F_SET(sp->frp, FR_CURSORSET);
	}

	/*
	 * Add into a previous screen and then into a subsequent screen, as
	 * they're the closest to the current screen.  If that doesn't work,
	 * there was no screen to join.
	 */
	if ((nsp = sp->q.cqe_prev) != (void *)&sp->gp->dq) {
		nsp->rows += sp->rows;
		sp = nsp;
		dir = FORWARD;
	} else if ((nsp = sp->q.cqe_next) != (void *)&sp->gp->dq) {
		nsp->woff = sp->woff;
		nsp->rows += sp->rows;
		sp = nsp;
		dir = BACKWARD;
	} else
		sp = NULL;

	if (spp != NULL)
		*spp = sp;
	if (sp == NULL)
		return (0);
		
	/*
	 * Make no effort to clean up the discarded screen's information.  If
	 * it's not exiting, we'll do the work when the user redisplays it.
	 *
	 * Small screens: see vs_refresh.c section 6a.  Adjust text line info,
	 * unless it's a small screen.
	 *
	 * Reset the length of the default scroll.
	 */
	if (!IS_SMALL(sp))
		sp->t_rows = sp->t_minrows = sp->rows - 1;
	sp->t_maxrows = sp->rows - 1;
	sp->defscroll = sp->t_maxrows / 2;
	*(HMAP + (sp->t_rows - 1)) = *TMAP;
	TMAP = HMAP + (sp->t_rows - 1);

	/*
	 * Draw the new screen from scratch, and add a status line.
	 *
	 * XXX
	 * We could play games with the map, if this were ever to be a
	 * performance problem, but I wrote the code a few times and it
	 * was never clean or easy.
	 */
	switch (dir) {
	case FORWARD:
		vs_sm_fill(sp, OOBLNO, P_TOP);
		break;
	case BACKWARD:
		vs_sm_fill(sp, OOBLNO, P_BOTTOM);
		break;
	default:
		abort();
	}

	F_SET(sp, SC_STATUS);
	return (0);
}

/*
 * vs_fg --
 *	Background the current screen, and foreground a new one.
 *
 * PUBLIC: int vs_fg __P((SCR *, CHAR_T *));
 */
int
vs_fg(csp, name)
	SCR *csp;
	CHAR_T *name;
{
	GS *gp;
	SCR *sp;

	if (vs_swap(csp, &sp, name))
		return (1);
	if (sp == NULL) {
		msgq_str(csp, M_ERR, name,
		    name == NULL ?
		    "223|There are no background screens" :
		    "224|There's no background screen editing a file named %s");
		return (1);
	}

	/* Move the old screen to the hidden queue. */
	gp = csp->gp;
	CIRCLEQ_REMOVE(&gp->dq, csp, q);
	CIRCLEQ_INSERT_TAIL(&gp->hq, csp, q);

	return (0);
}

/*
 * vs_bg --
 *	Background the screen, and switch to the next one.
 *
 * PUBLIC: int vs_bg __P((SCR *));
 */
int
vs_bg(csp)
	SCR *csp;
{
	GS *gp;
	SCR *sp;

	/* Try and join with another screen. */
	if (vs_discard(csp, &sp))
		return (1);
	if (sp == NULL) {
		msgq(csp, M_ERR,
		    "225|You may not background your only displayed screen");
		return (1);
	}

	/* Move the old screen to the hidden queue. */
	gp = csp->gp;
	CIRCLEQ_REMOVE(&gp->dq, csp, q);
	CIRCLEQ_INSERT_TAIL(&gp->hq, csp, q);

	/* Toss the screen map. */
	free(_HMAP(csp));
	_HMAP(csp) = NULL;

	/* Switch screens. */
	csp->nextdisp = sp;
	F_SET(csp, SC_SSWITCH);

	return (0);
}

/*
 * vs_swap --
 *	Swap the current screen with a hidden one.
 *
 * PUBLIC: int vs_swap __P((SCR *, SCR **, char *));
 */
int
vs_swap(csp, nsp, name)
	SCR *csp, **nsp;
	char *name;
{
	GS *gp;
	SCR *sp;
	int issmallscreen;

	/* Find the screen, or, if name is NULL, the first screen. */
	gp = csp->gp;
	for (sp = gp->hq.cqh_first;
	    sp != (void *)&gp->hq; sp = sp->q.cqe_next)
		if (name == NULL || !strcmp(sp->frp->name, name))
			break;
	if (sp == (void *)&gp->hq) {
		*nsp = NULL;
		return (0);
	}
	*nsp = sp;

	/*
	 * Save the old screen's cursor information.
	 *
	 * XXX
	 * If called after file_end(), and the underlying file was a tmp
	 * file, it may have gone away.
	 */
	if (csp->frp != NULL) {
		csp->frp->lno = csp->lno;
		csp->frp->cno = csp->cno;
		F_SET(csp->frp, FR_CURSORSET);
	}

	/* Switch screens. */
	csp->nextdisp = sp;
	F_SET(csp, SC_SSWITCH);

	/* Initialize terminal information. */
	VIP(sp)->srows = VIP(csp)->srows;

	issmallscreen = IS_SMALL(sp);

	/* Initialize screen information. */
	sp->cols = csp->cols;
	sp->rows = csp->rows;	/* XXX: Only place in vi that sets rows. */
	sp->woff = csp->woff;

	/*
	 * Small screens: see vs_refresh.c, section 6a.
	 *
	 * The new screens may have different screen options sizes than the
	 * old one, so use them.  Make sure that text counts aren't larger
	 * than the new screen sizes.
	 */
	if (issmallscreen) {
		sp->t_minrows = sp->t_rows = O_VAL(sp, O_WINDOW);
		if (sp->t_rows > csp->t_maxrows)
			sp->t_rows = sp->t_maxrows;
		if (sp->t_minrows > csp->t_maxrows)
			sp->t_minrows = sp->t_maxrows;
	} else
		sp->t_rows = sp->t_maxrows = sp->t_minrows = sp->rows - 1;

	/* Reset the length of the default scroll. */
	sp->defscroll = sp->t_maxrows / 2;

	/* Allocate a new screen map. */
	CALLOC_RET(sp, HMAP, SMAP *, SIZE_HMAP(sp), sizeof(SMAP));
	TMAP = HMAP + (sp->t_rows - 1);

	/* Fill the map. */
	if (vs_sm_fill(sp, sp->lno, P_FILL))
		return (1);

	/*
	 * The new screen replaces the old screen in the parent/child list.
	 * We insert the new screen after the old one.  If we're exiting,
	 * the exit will delete the old one, if we're foregrounding, the fg
	 * code will move the old one to the hidden queue.
	 */
	CIRCLEQ_REMOVE(&gp->hq, sp, q);
	CIRCLEQ_INSERT_AFTER(&gp->dq, csp, sp, q);

	/*
	 * Don't change the screen's cursor information other than to
	 * note that the cursor is wrong.
	 */
	F_SET(VIP(sp), VIP_CUR_INVALID);

	/* Draw the new screen from scratch, and add a status line. */
	F_SET(sp, SC_SCR_REDRAW | SC_STATUS);
	return (0);
}

/*
 * vs_resize --
 *	Change the absolute size of the current screen.
 *
 * PUBLIC: int vs_resize __P((SCR *, long, adj_t));
 */
int
vs_resize(sp, count, adj)
	SCR *sp;
	long count;
	adj_t adj;
{
	GS *gp;
	SCR *g, *s;
	size_t g_off, s_off;

	gp = sp->gp;

	/*
	 * Figure out which screens will grow, which will shrink, and
	 * make sure it's possible.
	 */
	if (count == 0)
		return (0);
	if (adj == A_SET) {
		if (sp->t_maxrows == count)
			return (0);
		if (sp->t_maxrows > count) {
			adj = A_DECREASE;
			count = sp->t_maxrows - count;
		} else {
			adj = A_INCREASE;
			count = count - sp->t_maxrows;
		}
	}

	g_off = s_off = 0;
	if (adj == A_DECREASE) {
		if (count < 0)
			count = -count;
		s = sp;
		if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count)
			goto toosmall;
		if ((g = sp->q.cqe_prev) == (void *)&gp->dq) {
			if ((g = sp->q.cqe_next) == (void *)&gp->dq)
				goto toobig;
			g_off = -count;
		} else
			s_off = count;
	} else {
		g = sp;
		if ((s = sp->q.cqe_next) != (void *)&gp->dq)
			if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count)
				s = NULL;
			else
				s_off = count;
		else
			s = NULL;
		if (s == NULL) {
			if ((s = sp->q.cqe_prev) == (void *)&gp->dq) {
toobig:				msgq(sp, M_BERR, adj == A_DECREASE ?
				    "227|The screen cannot shrink" :
				    "228|The screen cannot grow");
				return (1);
			}
			if (s->t_maxrows < MINIMUM_SCREEN_ROWS + count) {
toosmall:			msgq(sp, M_BERR,
				    "226|The screen can only shrink to %d rows",
				    MINIMUM_SCREEN_ROWS);
				return (1);
			}
			g_off = -count;
		}
	}

	/*
	 * Fix up the screens; we could optimize the reformatting of the
	 * screen, but this isn't likely to be a common enough operation
	 * to make it worthwhile.
	 */
	s->rows += -count;
	s->woff += s_off;
	g->rows += count;
	g->woff += g_off;

	g->t_rows += count;
	if (g->t_minrows == g->t_maxrows)
		g->t_minrows += count;
	g->t_maxrows += count;
	_TMAP(g) += count;
	F_SET(g, SC_SCR_REFORMAT | SC_STATUS);

	s->t_rows -= count;
	s->t_maxrows -= count;
	if (s->t_minrows > s->t_maxrows)
		s->t_minrows = s->t_maxrows;
	_TMAP(s) -= count;
	F_SET(s, SC_SCR_REFORMAT | SC_STATUS);

	return (0);
}
