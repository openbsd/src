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

#ifndef lint
static const char sccsid[] = "@(#)api.c	8.18 (Berkeley) 4/27/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
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
 * PUBLIC: SCR *api_fscreen __P((int, char *));
 */
SCR *
api_fscreen(id, name)
	int id;
	char *name;
{
	GS *gp;
	SCR *tsp;

	gp = __global_list;

	/* Search the displayed list. */
	for (tsp = gp->dq.cqh_first;
	    tsp != (void *)&gp->dq; tsp = tsp->q.cqe_next)
		if (name == NULL) {
			if (id == tsp->id)
				return (tsp);
		} else if (!strcmp(name, tsp->frp->name))
			return (tsp);

	/* Search the hidden list. */
	for (tsp = gp->hq.cqh_first;
	    tsp != (void *)&gp->hq; tsp = tsp->q.cqe_next)
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
 * PUBLIC: int api_aline __P((SCR *, recno_t, char *, size_t));
 */
int
api_aline(sp, lno, line, len)
	SCR *sp;
	recno_t lno;
	char *line;
	size_t len;
{
	return (db_append(sp, 1, lno, line, len));
}

/*
 * api_dline --
 *	Delete a line.
 *
 * PUBLIC: int api_dline __P((SCR *, recno_t));
 */
int
api_dline(sp, lno)
	SCR *sp;
	recno_t lno;
{
	return (db_delete(sp, lno));
}

/*
 * api_gline --
 *	Get a line.
 *
 * PUBLIC: int api_gline __P((SCR *, recno_t, char **, size_t *));
 */
int
api_gline(sp, lno, linepp, lenp)
	SCR *sp;
	recno_t lno;
	char **linepp;
	size_t *lenp;
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
 * PUBLIC: int api_iline __P((SCR *, recno_t, char *, size_t));
 */
int
api_iline(sp, lno, line, len)
	SCR *sp;
	recno_t lno;
	char *line;
	size_t len;
{
	return (db_insert(sp, lno, line, len));
}

/*
 * api_lline --
 *	Return the line number of the last line in the file.
 *
 * PUBLIC: int api_lline __P((SCR *, recno_t *));
 */
int
api_lline(sp, lnop)
	SCR *sp;
	recno_t *lnop;
{
	return (db_last(sp, lnop));
}

/*
 * api_sline --
 *	Set a line.
 *
 * PUBLIC: int api_sline __P((SCR *, recno_t, char *, size_t));
 */
int
api_sline(sp, lno, line, len)
	SCR *sp;
	recno_t lno;
	char *line;
	size_t len;
{
	return (db_set(sp, lno, line, len));
}

/*
 * api_getmark --
 *	Get the mark.
 *
 * PUBLIC: int api_getmark __P((SCR *, int, MARK *));
 */
int
api_getmark(sp, markname, mp)
	SCR *sp;
	int markname;
	MARK *mp;
{
	return (mark_get(sp, (ARG_CHAR_T)markname, mp, M_ERR));
}

/*
 * api_setmark --
 *	Set the mark.
 *
 * PUBLIC: int api_setmark __P((SCR *, int, MARK *));
 */
int
api_setmark(sp, markname, mp)
	SCR *sp;
	int markname;
	MARK *mp;
{
	return (mark_set(sp, (ARG_CHAR_T)markname, mp, 1));
}

/*
 * api_getcursor --
 *	Get the cursor.
 *
 * PUBLIC: int api_getcursor __P((SCR *, MARK *));
 */
int
api_getcursor(sp, mp)
	SCR *sp;
	MARK *mp;
{
	mp->lno = sp->lno;
	mp->cno = sp->cno;
	return (0);
}

/*
 * api_setcursor --
 *	Set the cursor.
 *
 * PUBLIC: int api_setcursor __P((SCR *, MARK *));
 */
int
api_setcursor(sp, mp)
	SCR *sp;
	MARK *mp;
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
 * PUBLIC: void api_emessage __P((SCR *, char *));
 */
void
api_emessage(sp, text)
	SCR *sp;
	char *text;
{
	msgq(sp, M_ERR, "%s", text);
}

/*
 * api_imessage --
 *	Print an informational message.
 *
 * PUBLIC: void api_imessage __P((SCR *, char *));
 */
void
api_imessage(sp, text)
	SCR *sp;
	char *text;
{
	msgq(sp, M_INFO, "%s", text);
}

/*
 * api_iscreen
 *	Create a new screen and return its id.
 *
 * PUBLIC: int api_iscreen __P((SCR *, char *, int *));
 */
int
api_iscreen(sp, file, idp)
	SCR *sp;
	char *file;
	int *idp;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	if (file) {
		ex_cinit(&cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, ap);
		ex_cadd(&cmd, &a, file, strlen(file));
	} else
		ex_cinit(&cmd, C_EDIT, 0, OOBLNO, OOBLNO, 0, NULL);
	cmd.flags |= E_NEWSCREEN;			/* XXX */
	if (cmd.cmd->fn(sp, &cmd))
		return (1);
	*idp = sp->nextdisp->id;
	return (0);
}

/*
 * api_escreen
 *	End a screen.
 *
 * PUBLIC: int api_escreen __P((SCR *));
 */
int
api_escreen(sp)
	SCR *sp;
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
 * PUBLIC: int api_swscreen __P((SCR *, SCR *));
 */
int
api_swscreen(sp, new)
      SCR *sp, *new;
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
 * PUBLIC: int api_map __P((SCR *, char *, char *, size_t));
 */
int
api_map(sp, name, map, len)
	SCR *sp;
	char *name, *map;
	size_t len;
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
 * PUBLIC: int api_unmap __P((SCR *, char *));
 */
int 
api_unmap(sp, name)
	SCR *sp;
	char *name;
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
 *
 * PUBLIC: int api_opts_get __P((SCR *, char *, char **));
 */
int
api_opts_get(sp, name, value)
	SCR *sp;
	char *name, **value;
{
	int offset;
	OPTLIST const *op;

	if ((op = opts_search(name)) == NULL)
		return (1);

	offset = op - optlist;
	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		MALLOC_RET(sp, *value, char *, strlen(op->name) + 2 + 1);
		(void)sprintf(*value,
		    "%s%s", O_ISSET(sp, offset) ? "" : "no", op->name);
		break;
	case OPT_NUM:
		MALLOC_RET(sp, *value, char *, 20);
		(void)sprintf(*value, "%ld", (u_long)O_VAL(sp, offset));
		break;
	case OPT_STR:
		if (O_STR(sp, offset) == NULL) {
			MALLOC_RET(sp, *value, char *, 2);
			value[0] = '\0';
		} else {
			MALLOC_RET(sp,
			    *value, char *, strlen(O_STR(sp, offset)) + 1);
			(void)sprintf(*value, "%s", O_STR(sp, offset));
		}
		break;
	}
	return (0);
}

/*
 * api_opts_set --
 *	Set options.
 *
 * PUBLIC: int api_opts_set __P((SCR *, char *));
 */
int 
api_opts_set(sp, name)
	SCR *sp;
	char *name;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	ex_cinit(&cmd, C_SET, 0, OOBLNO, OOBLNO, 0, ap);
	ex_cadd(&cmd, &a, name, strlen(name));
	return (cmd.cmd->fn(sp, &cmd));
}

/*
 * api_run_str --
 *      Execute a string as an ex command.
 *
 * PUBLIC: int api_run_str __P((SCR *, char *));
 */
int     
api_run_str(sp, cmd)
	SCR *sp;
	char *cmd;
{
	return (ex_run_str(sp, NULL, cmd, strlen(cmd), 0, 0));
}
