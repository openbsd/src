/* $OpenBSD: undo.c,v 1.29 2005/10/11 01:08:53 kjell Exp $ */
/*
 * Copyright (c) 2002 Vincent Labrecque <vincent@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "def.h"
#include "kbd.h"

#define MAX_FREE_RECORDS	32

/*
 * Local variables
 */
static LIST_HEAD(, undo_rec)	 undo_free;
static int			 undo_free_num;
static int			 nobound;

/*
 * Global variables
 */
/*
 * undo_disable_flag: Stop doing undo (useful when we know are
 *	going to deal with huge deletion/insertions
 *	that we don't plan to undo)
 */
int undo_disable_flag;

/*
 * Local functions
 */
static int find_dot(LINE *, int);
static int find_lo(int, LINE **, int *);
static struct undo_rec *new_undo_record(void);
static int drop_oldest_undo_record(void);

/*
 * find_dot, find_lo()
 *
 * Find an absolute dot in the buffer from a line/offset pair, and vice-versa.
 *
 * Since lines can be deleted while they are referenced by undo record, we
 * need to have an absolute dot to have something reliable.
 */
static int
find_dot(LINE *lp, int off)
{
	int	 count = 0;
	LINE	*p;

	for (p = curbp->b_linep; p != lp; p = lforw(p)) {
		if (count != 0) {
			if (p == curbp->b_linep) {
				ewprintf("Error: Undo stuff called with a"
				    "nonexistent line");
				return (FALSE);
			}
		}
		count += llength(p) + 1;
	}
	count += off;

	return (count);
}

static int
find_lo(int pos, LINE **olp, int *offset)
{
	LINE *p;

	p = curbp->b_linep;
	while (pos > llength(p)) {
		pos -= llength(p) + 1;
		if ((p = lforw(p)) == curbp->b_linep) {
			*olp = NULL;
			*offset = 0;
			return (FALSE);
		}
	}
	*olp = p;
	*offset = pos;

	return (TRUE);
}

static struct undo_rec *
new_undo_record(void)
{
	struct undo_rec *rec;

	rec = LIST_FIRST(&undo_free);
	if (rec != NULL) {
		LIST_REMOVE(rec, next);	/* Remove it from the free-list */
		undo_free_num--;
	} else {
		if ((rec = malloc(sizeof(*rec))) == NULL)
			panic("Out of memory in undo code (record)");
	}
	memset(rec, 0, sizeof(struct undo_rec));

	return (rec);
}

void
free_undo_record(struct undo_rec *rec)
{
	static int initialised = 0;

	/*
	 * On the first run, do initialisation of the free list.
	 */
	if (initialised == 0) {
		LIST_INIT(&undo_free);
		initialised = 1;
	}
	if (rec->content != NULL) {
		free(rec->content);
		rec->content = NULL;
	}
	if (undo_free_num >= MAX_FREE_RECORDS) {
		free(rec);
		return;
	}
	undo_free_num++;

	LIST_INSERT_HEAD(&undo_free, rec, next);
}

/*
 * Drop the oldest undo record in our list. Return 1 if we could remove it,
 * 0 if the undo list was empty.
 */
static int
drop_oldest_undo_record(void)
{
	struct undo_rec *rec;

	rec = LIST_END(&curbp->b_undo);
	if (rec != NULL) {
		undo_free_num--;
		LIST_REMOVE(rec, next);
		free_undo_record(rec);
		return (1);
	}
	return (0);
}

static __inline__ int
lastrectype(void)
{
	struct undo_rec *rec;

	if ((rec = LIST_FIRST(&curbp->b_undo)) != NULL)
		return (rec->type);
	return (0);
}

int
undo_enable(int on)
{
	int pon = undo_disable_flag;

	undo_disable_flag = (on == TRUE) ? 0 : 1;
	return (pon ? FALSE : TRUE);
}

void
undo_no_boundary(int flag)
{
	if (!undo_disable_flag)
		nobound = flag;
}

int
undo_add_boundary(void)
{
	struct undo_rec *rec;

	if (nobound)
		return (TRUE);

	if (lastrectype() == BOUNDARY)
		return (TRUE);
	
	rec = new_undo_record();
	rec->type = BOUNDARY;

	LIST_INSERT_HEAD(&curbp->b_undo, rec, next);

	return (TRUE);
}

int
undo_add_insert(LINE *lp, int offset, int size)
{
	REGION	reg;
	struct	undo_rec *rec;
	int	pos;

	if (undo_disable_flag)
		return (TRUE);
	reg.r_linep = lp;
	reg.r_offset = offset;
	reg.r_size = size;

	pos = find_dot(lp, offset);

	/*
	 * We try to reuse the last undo record to `compress' things.
	 */
	rec = LIST_FIRST(&curbp->b_undo);
	if (rec != NULL && rec->type == INSERT) {
		if (rec->pos + rec->region.r_size == pos) {
			rec->region.r_size += reg.r_size;
			return (TRUE);
		}
	}

	/*
	 * We couldn't reuse the last undo record, so prepare a new one.
	 */
	rec = new_undo_record();
	rec->pos = pos;
	rec->type = INSERT;
	memmove(&rec->region, &reg, sizeof(REGION));
	rec->content = NULL;

	undo_add_boundary();

	LIST_INSERT_HEAD(&curbp->b_undo, rec, next);

	return (TRUE);
}

/*
 * This of course must be done _before_ the actual deletion is done.
 */
int
undo_add_delete(LINE *lp, int offset, int size)
{
	REGION	reg;
	struct	undo_rec *rec;
	int	pos;

	if (undo_disable_flag)
		return (TRUE);

	reg.r_linep = lp;
	reg.r_offset = offset;
	reg.r_size = size;

	pos = find_dot(lp, offset);

	if (offset == llength(lp))	/* if it's a newline... */
		undo_add_boundary();
	else if ((rec = LIST_FIRST(&curbp->b_undo)) != NULL) {
		/*
		 * Separate this command from the previous one if we're not
		 * just before the previous record...
		 */
		if (rec->type == DELETE) {
			if (rec->pos - rec->region.r_size != pos)
				undo_add_boundary();
		}
	}
	rec = new_undo_record();
	rec->pos = pos;

	rec->type = DELETE;
	memmove(&rec->region, &reg, sizeof(REGION));
	do {
		rec->content = malloc(reg.r_size + 1);
	} while ((rec->content == NULL) && drop_oldest_undo_record());

	if (rec->content == NULL)
		panic("Out of memory");

	region_get_data(&reg, rec->content, reg.r_size);

	if (lastrectype() != DELETE)
		undo_add_boundary();

	LIST_INSERT_HEAD(&curbp->b_undo, rec, next);

	return (TRUE);
}

/*
 * This of course must be called before the change takes place.
 */
int
undo_add_change(LINE *lp, int offset, int size)
{
	if (undo_disable_flag)
		return (TRUE);
	undo_add_boundary();
	nobound = TRUE;
	undo_add_delete(lp, offset, size);
	undo_add_insert(lp, offset, size);
	nobound = FALSE;
	undo_add_boundary();

	return (TRUE);
}

/*
 * Show the undo records for the current buffer in a new buffer.
 */
int
undo_dump(int f, int n)
{
	struct	 undo_rec *rec;
	BUFFER	*bp;
	MGWIN	*wp;
	char	 buf[4096], tmp[1024];
	int	 num;

	/*
	 * Prepare the buffer for insertion.
	 */
	if ((bp = bfind("*undo*", TRUE)) == NULL)
		return (FALSE);
	bp->b_flag |= BFREADONLY;
	bclear(bp);
	popbuf(bp);

	for (wp = wheadp; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_bufp == bp) {
			wp->w_dotp = bp->b_linep;
			wp->w_doto = 0;
		}
	}

	num = 0;
	for (rec = LIST_FIRST(&curbp->b_undo); rec != NULL;
	    rec = LIST_NEXT(rec, next)) {
		num++;
		snprintf(buf, sizeof(buf),
		    "Record %d =>\t %s at %d ", num,
		    (rec->type == DELETE) ? "DELETE":
		    (rec->type == INSERT) ? "INSERT":
		    (rec->type == BOUNDARY) ? "----" : "UNKNOWN",
		    rec->pos);

		if (rec->content) {
			strlcat(buf, "\"", sizeof(buf));
			snprintf(tmp, sizeof(tmp), "%.*s", rec->region.r_size,
			    rec->content);
			strlcat(buf, tmp, sizeof(buf));
			strlcat(buf, "\"", sizeof(buf));
		}
		snprintf(tmp, sizeof(tmp), " [%d]", rec->region.r_size);
		strlcat(buf, tmp, sizeof(buf));
		addlinef(bp, "%s", buf);
	}
	return (TRUE);
}

/*
 * After the user did action1, then action2, then action3:
 *
 *	[action3] <--- Undoptr
 *	[action2]
 *	[action1]
 *	 ------
 *	 [undo]
 *
 * After undo:
 *
 *	[undo of action3]
 *	[action2] <--- Undoptr
 *	[action1]
 *	 ------
 *	 [undo]
 *
 * After another undo:
 *
 *
 *	[undo of action2]
 *	[undo of action3]
 *	[action1]  <--- Undoptr
 *	 ------
 *	 [undo]
 *
 * Note that the "undo of actionX" have no special meaning. Only when
 * we undo a deletion, the insertion will be recorded just as if it
 * was typed on the keyboard. Resulting in the inverse operation being
 * saved in the list.
 *
 * If undoptr reaches the bottom of the list, or if we moved between
 * two undo actions, we make it point back at the topmost record. This is
 * how we handle redoing.
 */
int
undo(int f, int n)
{
	struct undo_rec	*ptr, *nptr;
	int 		 done, rval;
	LINE		*lp;
	int		 offset, save, dot;
	static int	 nulled = FALSE;

	dot = find_dot(curwp->w_dotp, curwp->w_doto);

	ptr = curbp->b_undoptr;

	/* if we moved, make ptr point back to the top of the list */
	if ((ptr == NULL && nulled == TRUE) || curbp->b_undopos != dot) {
		ptr = LIST_FIRST(&curbp->b_undo);
		nulled = TRUE;
	}

	rval = TRUE;
	while (n--) {
		/* if we have a spurious boundary, free it and move on.... */
		while (ptr && ptr->type == BOUNDARY) {
			nptr = LIST_NEXT(ptr, next);
			LIST_REMOVE(ptr, next);
			free_undo_record(ptr);
			ptr = nptr;
		}
		/*
		 * Ptr is NULL, but on the next run, it will point to the
		 * top again, redoing all stuff done in the buffer since
		 * its creation.
		 */
		if (ptr == NULL) {
			ewprintf("No further undo information");
			rval = FALSE;
			nulled = TRUE;
			break;
		}
		nulled = FALSE;

		/*
		 * Loop while we don't get a boundary specifying we've
		 * finished the current action...
		 */

		undo_add_boundary();

		save = nobound;
		nobound = TRUE;

		done = 0;
		do {
			/*
			 * Move to where this has to apply
			 *
			 * Boundaries are put as position 0 (to save
			 * lookup time in find_dot) so we must
			 * not move there...
			 */
			if (ptr->type != BOUNDARY) {
				if (find_lo(ptr->pos, &lp,
				    &offset) == FALSE) {
					ewprintf("Internal error in Undo!");
					rval = FALSE;
					break;
				}
				curwp->w_dotp = lp;
				curwp->w_doto = offset;
			}

			/*
			 * Do operation^-1
			 */
			switch (ptr->type) {
			case INSERT:
				ldelete(ptr->region.r_size, KFORW);
				break;
			case DELETE:
				region_put_data(ptr->content,
				    ptr->region.r_size);
				break;
			case BOUNDARY:
				done = 1;
				break;
			default:
				break;
			}

			/* And move to next record */
			ptr = LIST_NEXT(ptr, next);
		} while (ptr != NULL && !done);

		nobound = save;
		undo_add_boundary();

		ewprintf("Undo!");
	}
	/*
	 * Record where we are. (we have to save our new position at the end
	 * since we change the dot when undoing....)
	 */
	curbp->b_undoptr = ptr;

	curbp->b_undopos = find_dot(curwp->w_dotp, curwp->w_doto);

	return (rval);
}
