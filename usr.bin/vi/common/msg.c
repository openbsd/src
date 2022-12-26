/*	$OpenBSD: msg.c,v 1.28 2022/12/26 19:16:03 jmc Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "../vi/vi.h"

/*
 * msgq --
 *	Display a message.
 *
 * PUBLIC: void msgq(SCR *, mtype_t, const char *, ...);
 */
void
msgq(SCR *sp, mtype_t mt, const char *fmt, ...)
{
	static int reenter;		/* STATIC: Re-entrancy check. */
	GS *gp;
	size_t blen, len, mlen, nlen;
	const char *p;
	char *bp, *mp;
        va_list ap;

	/*
	 * !!!
	 * It's possible to enter msg when there's no screen to hold the
	 * message.  If sp is NULL, ignore the special cases and put the
	 * message out to stderr.
	 */
	if (sp == NULL) {
		gp = NULL;
		if (mt == M_BERR)
			mt = M_ERR;
		else if (mt == M_VINFO)
			mt = M_INFO;
	} else {
		gp = sp->gp;
		switch (mt) {
		case M_BERR:
			if (F_ISSET(sp, SC_VI) && !O_ISSET(sp, O_VERBOSE)) {
				F_SET(gp, G_BELLSCHED);
				return;
			}
			mt = M_ERR;
			break;
		case M_VINFO:
			if (!O_ISSET(sp, O_VERBOSE))
				return;
			mt = M_INFO;
			/* FALLTHROUGH */
		case M_INFO:
			if (F_ISSET(sp, SC_EX_SILENT))
				return;
			break;
		case M_ERR:
		case M_SYSERR:
			break;
		default:
			abort();
		}
	}

	/*
	 * It's possible to reenter msg when it allocates space.  We're
	 * probably dead anyway, but there's no reason to drop core.
	 *
	 * XXX
	 * Yes, there's a race, but it should only be two instructions.
	 */
	if (reenter++)
		return;

	/* Get space for the message. */
	nlen = 1024;
	if (0) {
retry:		FREE_SPACE(sp, bp, blen);
		nlen *= 2;
	}
	bp = NULL;
	blen = 0;
	GET_SPACE_GOTO(sp, bp, blen, nlen);

	/*
	 * Error prefix.
	 *
	 * mp:	 pointer to the current next character to be written
	 * mlen: length of the already written characters
	 * blen: total length of the buffer
	 */
#define	REM	(blen - mlen)
	mp = bp;
	mlen = 0;
	if (mt == M_SYSERR) {
		p = "Error: ";
		len = strlen(p);
		if (REM < len)
			goto retry;
		memcpy(mp, p, len);
		mp += len;
		mlen += len;
	}

	/*
	 * If we're running an ex command that the user didn't enter, display
	 * the file name and line number prefix.
	 */
	if ((mt == M_ERR || mt == M_SYSERR) &&
	    sp != NULL && gp != NULL && gp->if_name != NULL) {
		for (p = gp->if_name; *p != '\0'; ++p) {
			len = snprintf(mp, REM, "%s", KEY_NAME(sp, *p));
			mp += len;
			if ((mlen += len) > blen)
				goto retry;
		}
		len = snprintf(mp, REM, ", %d: ", gp->if_lno);
		mp += len;
		if ((mlen += len) > blen)
			goto retry;
	}

	/* If nothing to format, we're done. */
	if (fmt == NULL) {
		len = 0;
		goto nofmt;
	}

	/* Format the arguments into the string. */
        va_start(ap, fmt);
	len = vsnprintf(mp, REM, fmt, ap);
	va_end(ap);
	if (len >= nlen)
		goto retry;

nofmt:	mp += len;
	if ((mlen += len) > blen)
		goto retry;
	if (mt == M_SYSERR) {
		len = snprintf(mp, REM, ": %s", strerror(errno));
		mp += len;
		if ((mlen += len) > blen)
			goto retry;
		mt = M_ERR;
	}

	/* Add trailing newline. */
	if ((mlen += 1) > blen)
		goto retry;
	*mp = '\n';

	if (sp != NULL)
		(void)ex_fflush(sp);
	if (gp != NULL)
		gp->scr_msg(sp, mt, bp, mlen);
	else
		(void)fprintf(stderr, "%.*s", (int)mlen, bp);

	/* Cleanup. */
	FREE_SPACE(sp, bp, blen);
alloc_err:
	reenter = 0;
}

/*
 * msgq_str --
 *	Display a message with an embedded string.
 *
 * PUBLIC: void msgq_str(SCR *, mtype_t, char *, char *);
 */
void
msgq_str(SCR *sp, mtype_t mtype, char *str, char *fmt)
{
	int nf, sv_errno;
	char *p;

	if (str == NULL) {
		msgq(sp, mtype, fmt);
		return;
	}

	sv_errno = errno;
	p = msg_print(sp, str, &nf);
	errno = sv_errno;
	msgq(sp, mtype, fmt, p);
	if (nf)
		FREE_SPACE(sp, p, 0);
}

/*
 * mod_rpt --
 *	Report on the lines that changed.
 *
 * !!!
 * Historic vi documentation (USD:15-8) claimed that "The editor will also
 * always tell you when a change you make affects text which you cannot see."
 * This wasn't true -- edit a large file and do "100d|1".  We don't implement
 * this semantic since it requires tracking each line that changes during a
 * command instead of just keeping count.
 *
 * Line counts weren't right in historic vi, either.  For example, given the
 * file:
 *	abc
 *	def
 * the command 2d}, from the 'b' would report that two lines were deleted,
 * not one.
 *
 * PUBLIC: void mod_rpt(SCR *);
 */
void
mod_rpt(SCR *sp)
{
	static char * const action[] = {
		"added",
		"changed",
		"deleted",
		"joined",
		"moved",
		"shifted",
		"yanked",
	};
	static char * const lines[] = {
		"line",
		"lines",
	};
	recno_t total;
	u_long rptval;
	int first, cnt;
	size_t blen, len, tlen;
	const char *t;
	char * const *ap;
	char *bp, *p;

	/* Change reports are turned off in batch mode. */
	if (F_ISSET(sp, SC_EX_SILENT))
		return;

	/* Reset changing line number. */
	sp->rptlchange = OOBLNO;

	/*
	 * Don't build a message if not enough changed.
	 *
	 * !!!
	 * And now, a vi clone test.  Historically, vi reported if the number
	 * of changed lines was > than the value, not >=, unless it was a yank
	 * command, which used >=.  No lie.  Furthermore, an action was never
	 * reported for a single line action.  This is consistent for actions
	 * other than yank, but yank didn't report single line actions even if
	 * the report edit option was set to 1.  In addition, setting report to
	 * 0 in the 4BSD historic vi was equivalent to setting it to 1, for an
	 * unknown reason (this bug was fixed in System III/V at some point).
	 * I got complaints, so nvi conforms to System III/V historic practice
	 * except that we report a yank of 1 line if report is set to 1.
	 */
#define	ARSIZE(a)	sizeof(a) / sizeof (*a)
#define	MAXNUM		25
	rptval = O_VAL(sp, O_REPORT);
	for (cnt = 0, total = 0; cnt < ARSIZE(action); ++cnt)
		total += sp->rptlines[cnt];
	if (total == 0)
		return;
	if (total <= rptval && sp->rptlines[L_YANKED] < rptval) {
		for (cnt = 0; cnt < ARSIZE(action); ++cnt)
			sp->rptlines[cnt] = 0;
		return;
	}

	/* Build and display the message. */
	GET_SPACE_GOTO(sp, bp, blen, sizeof(action) * MAXNUM + 1);
	for (p = bp, first = 1, tlen = 0,
	    ap = action, cnt = 0; cnt < ARSIZE(action); ++ap, ++cnt)
		if (sp->rptlines[cnt] != 0) {
			if (first)
				first = 0;
			else {
				*p++ = ';';
				*p++ = ' ';
				tlen += 2;
			}
			len = snprintf(p, MAXNUM, "%u ", sp->rptlines[cnt]);
			p += len;
			tlen += len;
			t = lines[sp->rptlines[cnt] == 1 ? 0 : 1];
			len = strlen(t);
			memcpy(p, t, len);
			p += len;
			tlen += len;
			*p++ = ' ';
			++tlen;
			len = strlen(*ap);
			memcpy(p, *ap, len);
			p += len;
			tlen += len;
			sp->rptlines[cnt] = 0;
		}

	/* Add trailing newline. */
	*p = '\n';
	++tlen;

	(void)ex_fflush(sp);
	sp->gp->scr_msg(sp, M_INFO, bp, tlen);

	FREE_SPACE(sp, bp, blen);
alloc_err:
	return;

#undef ARSIZE
#undef MAXNUM
}

/*
 * msgq_status --
 *	Report on the file's status.
 *
 * PUBLIC: void msgq_status(SCR *, recno_t, u_int);
 */
void
msgq_status(SCR *sp, recno_t lno, u_int flags)
{
	recno_t last;
	size_t blen, len;
	int cnt, needsep;
	const char *t;
	char **ap, *bp, *np, *p, *s, *ep;

	/* Get sufficient memory. */
	len = strlen(sp->frp->name);
	GET_SPACE_GOTO(sp, bp, blen, len * MAX_CHARACTER_COLUMNS + 128);
	p = bp;
	ep = bp + blen;

	/* Copy in the filename. */
	for (t = sp->frp->name; *t != '\0'; ++t) {
		len = KEY_LEN(sp, *t);
		memcpy(p, KEY_NAME(sp, *t), len);
		p += len;
	}
	np = p;
	*p++ = ':';
	*p++ = ' ';

	/* Copy in the argument count. */
	if (F_ISSET(sp, SC_STATUS_CNT) && sp->argv != NULL) {
		for (cnt = 0, ap = sp->argv; *ap != NULL; ++ap, ++cnt);
		if (cnt > 1) {
			(void)snprintf(p, ep - p, "%d files to edit", cnt);
			p += strlen(p);
			*p++ = ':';
			*p++ = ' ';
		}
		F_CLR(sp, SC_STATUS_CNT);
	}

	/*
	 * See nvi/exf.c:file_init() for a description of how and when the
	 * read-only bit is set.
	 *
	 * !!!
	 * The historic display for "name changed" was "[Not edited]".
	 */
	needsep = 0;
	if (F_ISSET(sp->frp, FR_NEWFILE)) {
		F_CLR(sp->frp, FR_NEWFILE);
		len = strlen("new file");
		memcpy(p, "new file", len);
		p += len;
		needsep = 1;
	} else {
		if (F_ISSET(sp->frp, FR_NAMECHANGE)) {
			len = strlen("name changed");
			memcpy(p, "name changed", len);
			p += len;
			needsep = 1;
		}
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		t = (F_ISSET(sp->ep, F_MODIFIED)) ? "modified" : "unmodified";
		len = strlen(t);
		memcpy(p, t, len);
		p += len;
		needsep = 1;
	}
	if (F_ISSET(sp->frp, FR_UNLOCKED)) {
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		len = strlen("UNLOCKED");
		memcpy(p, "UNLOCKED", len);
		p += len;
		needsep = 1;
	}
	if (O_ISSET(sp, O_READONLY)) {
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		len = strlen("readonly");
		memcpy(p, "readonly", len);
		p += len;
		needsep = 1;
	}
	if (needsep) {
		*p++ = ':';
		*p++ = ' ';
	}
	if (LF_ISSET(MSTAT_SHOWLAST)) {
		if (db_last(sp, &last))
			return;
		if (last == 0) {
			len = strlen("empty file");
			memcpy(p, "empty file", len);
			p += len;
		} else {
			(void)snprintf(p, ep - p, "line %lu of %lu [%lu%%]",
			    (unsigned long)lno, (unsigned long)last,
			    (unsigned long)(lno * 100) / last);
			p += strlen(p);
		}
	} else {
		(void)snprintf(p, ep - p, "line %lu", (unsigned long)lno);
		p += strlen(p);
	}
#ifdef DEBUG
	(void)snprintf(p, ep - p, " (pid %ld)", (long)getpid());
	p += strlen(p);
#endif
	*p++ = '\n';
	len = p - bp;

	/*
	 * There's a nasty problem with long path names.  Tags files
	 * can result in long paths and vi will request a continuation key from
	 * the user as soon as it starts the screen.  Unfortunately, the user
	 * has already typed ahead, and chaos results.  If we assume that the
	 * characters in the filenames and informational messages only take a
	 * single screen column each, we can trim the filename.
	 *
	 * XXX
	 * Status lines get put up at fairly awkward times.  For example, when
	 * you do a filter read (e.g., :read ! echo foo) in the top screen of a
	 * split screen, we have to repaint the status lines for all the screens
	 * below the top screen.  We don't want users having to enter continue
	 * characters for those screens.  Make it really hard to screw this up.
	 */
	s = bp;
	if (LF_ISSET(MSTAT_TRUNCATE) && len > sp->cols) {
		for (; s < np && (*s != '/' || (p - s) > sp->cols - 3); ++s);
		if (s == np) {
			s = p - (sp->cols - 5);
			*--s = ' ';
		}
		*--s = '.';
		*--s = '.';
		*--s = '.';
		len = p - s;
	}

	/* Flush any waiting ex messages. */
	(void)ex_fflush(sp);

	sp->gp->scr_msg(sp, M_INFO, s, len);

	FREE_SPACE(sp, bp, blen);
alloc_err:
	return;
}

/*
 * msg_cont --
 *	Return common continuation messages.
 *
 * PUBLIC: const char *msg_cmsg(SCR *, cmsg_t, size_t *);
 */
const char *
msg_cmsg(SCR *sp, cmsg_t which, size_t *lenp)
{
	const char *s;
	switch (which) {
	case CMSG_CONF:
		s = "confirm? [ynq]";
		break;
	case CMSG_CONT:
		s = "Press any key to continue: ";
		break;
	case CMSG_CONT_EX:
		s = "Press any key to continue [: to enter more ex commands]: ";
		break;
	case CMSG_CONT_R:
		s = "Press Enter to continue: ";
		break;
	case CMSG_CONT_S:
		s = " cont?";
		break;
	case CMSG_CONT_Q:
		s = "Press any key to continue [q to quit]: ";
		break;
	default:
		abort();
	}
	*lenp = strlen(s);
	return s;
}

/*
 * msg_print --
 *	Return a printable version of a string, in allocated memory.
 *
 * PUBLIC: char *msg_print(SCR *, const char *, int *);
 */
char *
msg_print(SCR *sp, const char *s, int *needfree)
{
	size_t blen, nlen;
	const char *cp;
	char *bp, *ep, *p, *t;

	*needfree = 0;

	for (cp = s; *cp != '\0'; ++cp)
		if (!isprint(*cp))
			break;
	if (*cp == '\0')
		return ((char *)s);	/* SAFE: needfree set to 0. */

	nlen = 0;
	if (0) {
retry:		if (sp == NULL)
			free(bp);
		else
			FREE_SPACE(sp, bp, blen);
		*needfree = 0;
	}
	nlen += 256;
	if (sp == NULL) {
		if ((bp = malloc(nlen)) == NULL)
			goto alloc_err;
		blen = 0;
	} else
		GET_SPACE_GOTO(sp, bp, blen, nlen);
	if (0) {
alloc_err:	return ("");
	}
	*needfree = 1;

	for (p = bp, ep = (bp + blen) - 1, cp = s; *cp != '\0' && p < ep; ++cp)
		for (t = KEY_NAME(sp, *cp); *t != '\0' && p < ep; *p++ = *t++);
	if (p == ep)
		goto retry;
	*p = '\0';
	return (bp);
}
