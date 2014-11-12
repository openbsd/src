/*	$OpenBSD: api.c,v 1.16 2014/11/12 04:28:41 bentley Exp $	*/

/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 * Copyright (c) 1995
 *	George V. Neville-Neil. All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"

extern GS *__global_list;			/* XXX */

/*
 * api_fscreen --
 *	Return a pointer to the screen specified by the screen id
 *	or a file name.
 *
 * PUBLIC: SCR *api_fscreen(int, char *);
 */
SCR *
api_fscreen(int id, char *name)
{
	GS *gp;
	SCR *tsp;

	gp = __global_list;

	/* Search the displayed list. */
	TAILQ_FOREACH(tsp, &gp->dq, q)
		if (name == NULL) {
			if (id == tsp->id)
				return (tsp);
		} else if (!strcmp(name, tsp->frp->name))
			return (tsp);

	/* Search the hidden list. */
	TAILQ_FOREACH(tsp, &gp->hq, q)
		if (name == NULL) {
			if (id == tsp->id)
				return (tsp);
		} else if (!strcmp(name, tsp->frp->name))
			return (tsp);
	return (NULL);
}

/*
 * api_aline --
 *	Append a line.
 *
 * PUBLIC: int api_aline(SCR *, recno_t, char *, size_t);
 */
int
api_aline(SCR *sp, recno_t lno, char *line, size_t len)
{
	return (db_append(sp, 1, lno, line, len));
}

/*
 * api_dline --
 *	Delete a line.
 *
 * PUBLIC: int api_dline(SCR *, recno_t);
 */
int
api_dline(SCR *sp, recno_t lno)
{
	return (db_delete(sp, lno));
}

/*
 * api_gline --
 *	Get a line.
 *
 * PUBLIC: int api_gline(SCR *, recno_t, char **, size_t *);
 */
int
api_gline(SCR *sp, recno_t lno, char **linepp, size_t *lenp)
{
	int isempty;

	if (db_eget(sp, lno, linepp, lenp, &isempty)) {
		if (isempty)
			msgq(sp, M_ERR, "209|The file is empty");
		return (1);
	}
	return (0);
}

/*
 * api_iline --
 *	Insert a line.
 *
 * PUBLIC: int api_iline(SCR *, recno_t, char *, size_t);
 */
int
api_iline(SCR *sp, recno_t lno, char *line, size_t len)
{
	return (db_insert(sp, lno, line, len));
}

/*
 * api_lline --
 *	Return the line number of the last line in the file.
 *
 * PUBLIC: int api_lline(SCR *, recno_t *);
 */
int
api_lline(SCR *sp, recno_t *lnop)
{
	return (db_last(sp, lnop));
}

/*
 * api_sline --
 *	Set a line.
 *
 * PUBLIC: int api_sline(SCR *, recno_t, char *, size_t);
 */
int
api_sline(SCR *sp, recno_t lno, char *line, size_t len)
{
	return (db_set(sp, lno, line, len));
}

/*
 * api_getmark --
 *	Get the mark.
 *
 * PUBLIC: int api_getmark(SCR *, int, MARK *);
 */
int
api_getmark(SCR *sp, int markname, MARK *mp)
{
	return (mark_get(sp, (ARG_CHAR_T)markname, mp, M_ERR));
}

/*
 * api_setmark --
 *	Set the mark.
 *
 * PUBLIC: int api_setmark(SCR *, int, MARK *);
 */
int
api_setmark(SCR *sp, int markname, MARK *mp)
{
	return (mark_set(sp, (ARG_CHAR_T)markname, mp, 1));
}

/*
 * api_nextmark --
 *	Return the first mark if next not set, otherwise return the
 *	subsequent mark.
 *
 * PUBLIC: int api_nextmark(SCR *, int, char *);
 */
int
api_nextmark(SCR *sp, int next, char *namep)
{
	LMARK *mp;

	mp = LIST_FIRST(&sp->ep->marks);
	if (next)
		LIST_FOREACH(mp, &sp->ep->marks, q)
			if (mp->name == *namep) {
				mp = LIST_NEXT(mp, q);
				break;
			}
	if (mp == NULL)
		return (1);
	*namep = mp->name;
	return (0);
}

/*
 * api_getcursor --
 *	Get the cursor.
 *
 * PUBLIC: int api_getcursor(SCR *, MARK *);
 */
int
api_getcursor(SCR *sp, MARK *mp)
{
	mp->lno = sp->lno;
	mp->cno = sp->cno;
	return (0);
}

/*
 * api_setcursor --
 *	Set the cursor.
 *
 * PUBLIC: int api_setcursor(SCR *, MARK *);
 */
int
api_setcursor(SCR *sp, MARK *mp)
{
	size_t len;

	if (db_get(sp, mp->lno, DBG_FATAL, NULL, &len))
		return (1);
	if (mp->cno < 0 || mp->cno > len) {
		msgq(sp, M_ERR, "Cursor set to nonexistent column");
		return (1);
	}

	/* Set the cursor. */
	sp->lno = mp->lno;
	sp->cno = mp->cno;
	return (0);
}

/*
 * api_emessage --
 *	Print an error message.
 *
 * PUBLIC: void api_emessage(SCR *, char *);
 */
void
api_emessage(SCR *sp, char *text)
{
	msgq(sp, M_ERR, "%s", text);
}

/*
 * api_imessage --
 *	Print an informational message.
 *
 * PUBLIC: void api_imessage(SCR *, char *);
 */
void
api_imessage(SCR *sp, char *text)
{
	msgq(sp, M_INFO, "%s", text);
}

/*
 * api_edit
 *	Create a new screen and return its id 
 *	or edit a new file in the current screen.
 *
 * PUBLIC: int api_edit(SCR *, char *, SCR **, int);
 */
int
api_edit(SCR *sp, char *file, SCR **spp, int newscreen)
{
	ARGS *ap[2], a;
	EXCMD cmd;

	if (file) {
		ex_cinit(&cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, ap);
		ex_cadd(&cmd, &a, file, strlen(file));
	} else
		ex_cinit(&cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, NULL);
	if (newscreen)
		cmd.flags |= E_NEWSCREEN;		/* XXX */
	if (cmd.cmd->fn(sp, &cmd))
		return (1);
	*spp = sp->nextdisp;
	return (0);
}

/*
 * api_escreen
 *	End a screen.
 *
 * PUBLIC: int api_escreen(SCR *);
 */
int
api_escreen(SCR *sp)
{
	EXCMD cmd;

	/*
	 * XXX
	 * If the interpreter exits anything other than the current
	 * screen, vi isn't going to update everything correctly.
	 */
	ex_cinit(&cmd, C_QUIT, 0, OOBLNO, OOBLNO, 0, NULL);
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_swscreen --
 *    Switch to a new screen.
 *
 * PUBLIC: int api_swscreen(SCR *, SCR *);
 */
int
api_swscreen(SCR *sp, SCR *new)
{
	/*
	 * XXX
	 * If the interpreter switches from anything other than the
	 * current screen, vi isn't going to update everything correctly.
	 */
	sp->nextdisp = new;
	F_SET(sp, SC_SSWITCH);

	return (0);
}

/*
 * api_map --
 *	Map a key.
 *
 * PUBLIC: int api_map(SCR *, char *, char *, size_t);
 */
int
api_map(SCR *sp, char *name, char *map, size_t len)
{
	ARGS *ap[3], a, b;
	EXCMD cmd;

	ex_cinit(&cmd, C_MAP, 0, OOBLNO, OOBLNO, 0, ap);
	ex_cadd(&cmd, &a, name, strlen(name));
	ex_cadd(&cmd, &b, map, len);
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_unmap --
 *	Unmap a key.
 *
 * PUBLIC: int api_unmap(SCR *, char *);
 */
int 
api_unmap(SCR *sp, char *name)
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cinit(&cmd, C_UNMAP, 0, OOBLNO, OOBLNO, 0, ap);
	ex_cadd(&cmd, &a, name, strlen(name));
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_opts_get --
 *	Return a option value as a string, in allocated memory.
 *	If the option is of type boolean, boolvalue is (un)set
 *	according to the value; otherwise boolvalue is -1.
 *
 * PUBLIC: int api_opts_get(SCR *, char *, char **, int *);
 */
int
api_opts_get(SCR *sp, char *name, char **value, int *boolvalue)
{
	OPTLIST const *op;
	int offset;
	size_t len;

	if ((op = opts_search(name)) == NULL) {
		opts_nomatch(sp, name);
		return (1);
	}

	offset = op - optlist;
	if (boolvalue != NULL)
		*boolvalue = -1;
	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		len = strlen(op->name) + 2 + 1;
		MALLOC_RET(sp, *value, char *, len);
		(void)snprintf(*value, len,
		    "%s%s", O_ISSET(sp, offset) ? "" : "no", op->name);
		if (boolvalue != NULL)
			*boolvalue = O_ISSET(sp, offset);
		break;
	case OPT_NUM:
		len = 20;
		MALLOC_RET(sp, *value, char *, len);
		(void)snprintf(*value, len, "%lu", (u_long)O_VAL(sp, offset));
		break;
	case OPT_STR:
		if (O_STR(sp, offset) == NULL) {
			MALLOC_RET(sp, *value, char *, 2);
			value[0] = '\0';
		} else {
			len = strlen(O_STR(sp, offset)) + 1;
			MALLOC_RET(sp,
			    *value, char *, len);
			(void)snprintf(*value, len, "%s", O_STR(sp, offset));
		}
		break;
	}
	return (0);
}

/*
 * api_opts_set --
 *	Set options.
 *
 * PUBLIC: int api_opts_set(SCR *, char *, char *, u_long, int);
 */
int
api_opts_set(SCR *sp, char *name, char *str_value, u_long num_value,
    int bool_value)
{
	ARGS *ap[2], a, b;
	OPTLIST const *op;
	int rval;
	size_t blen;
	char *bp;

	if ((op = opts_search(name)) == NULL) {
		opts_nomatch(sp, name);
		return (1);
	}

	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		GET_SPACE_RET(sp, bp, blen, 64);
		a.len = snprintf(bp, 64, "%s%s", bool_value ? "" : "no", name);
		if (a.len > 63)
			a.len = 63;
		break;
	case OPT_NUM:
		GET_SPACE_RET(sp, bp, blen, 64);
		a.len = snprintf(bp, 64, "%s=%lu", name, num_value);
		if (a.len > 63)
			a.len = 63;
		break;
	case OPT_STR:
		GET_SPACE_RET(sp, bp, blen, 1024);
		a.len = snprintf(bp, 1024, "%s=%s", name, str_value);
		if (a.len > 1023)
			a.len = 1023;
		break;
	}
	a.bp = bp;
	b.len = 0;
	b.bp = NULL;
	ap[0] = &a;
	ap[1] = &b;
	rval = opts_set(sp, ap, NULL);

	FREE_SPACE(sp, bp, blen);

	return (rval);
}

/*
 * api_run_str --
 *      Execute a string as an ex command.
 *
 * PUBLIC: int api_run_str(SCR *, char *);
 */
int     
api_run_str(SCR *sp, char *cmd)
{
	return (ex_run_str(sp, NULL, cmd, strlen(cmd), 0, 0));
}
