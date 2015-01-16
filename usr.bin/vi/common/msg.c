/*	$OpenBSD: msg.c,v 1.22 2015/01/16 06:40:14 deraadt Exp $	*/

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
		p = msg_cat(sp, "020|Error: ", &len);
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
	fmt = msg_cat(sp, fmt, NULL);

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
		"293|added",
		"294|changed",
		"295|deleted",
		"296|joined",
		"297|moved",
		"298|shifted",
		"299|yanked",
	};
	static char * const lines[] = {
		"300|line",
		"301|lines",
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
			t = msg_cat(sp,
			    lines[sp->rptlines[cnt] == 1 ? 0 : 1], &len);
			memcpy(p, t, len);
			p += len;
			tlen += len;
			*p++ = ' ';
			++tlen;
			t = msg_cat(sp, *ap, &len);
			memcpy(p, t, len);
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
			(void)snprintf(p, ep - p,
			    msg_cat(sp, "317|%d files to edit", NULL), cnt);
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
		t = msg_cat(sp, "021|new file", &len);
		memcpy(p, t, len);
		p += len;
		needsep = 1;
	} else {
		if (F_ISSET(sp->frp, FR_NAMECHANGE)) {
			t = msg_cat(sp, "022|name changed", &len);
			memcpy(p, t, len);
			p += len;
			needsep = 1;
		}
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		if (F_ISSET(sp->ep, F_MODIFIED))
			t = msg_cat(sp, "023|modified", &len);
		else
			t = msg_cat(sp, "024|unmodified", &len);
		memcpy(p, t, len);
		p += len;
		needsep = 1;
	}
	if (F_ISSET(sp->frp, FR_UNLOCKED)) {
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		t = msg_cat(sp, "025|UNLOCKED", &len);
		memcpy(p, t, len);
		p += len;
		needsep = 1;
	}
	if (O_ISSET(sp, O_READONLY)) {
		if (needsep) {
			*p++ = ',';
			*p++ = ' ';
		}
		t = msg_cat(sp, "026|readonly", &len);
		memcpy(p, t, len);
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
			t = msg_cat(sp, "028|empty file", &len);
			memcpy(p, t, len);
			p += len;
		} else {
			t = msg_cat(sp, "027|line %lu of %lu [%ld%%]", &len);
			(void)snprintf(p, ep - p, t, lno, last,
			    (lno * 100) / last);
			p += strlen(p);
		}
	} else {
		t = msg_cat(sp, "029|line %lu", &len);
		(void)snprintf(p, ep - p, t, lno);
		p += strlen(p);
	}
#ifdef DEBUG
	(void)snprintf(p, ep - p, " (pid %ld)", (long)getpid());
	p += strlen(p);
#endif
	*p++ = '\n';
	len = p - bp;

	/*
	 * There's a nasty problem with long path names.  Cscope and tags files
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
 * msg_open --
 *	Open the message catalogs.
 *
 * PUBLIC: int msg_open(SCR *, char *);
 */
int
msg_open(SCR *sp, char *file)
{
	/*
	 * !!!
	 * Assume that the first file opened is the system default, and that
	 * all subsequent ones user defined.  Only display error messages
	 * if we can't open the user defined ones -- it's useful to know if
	 * the system one wasn't there, but if nvi is being shipped with an
	 * installed system, the file will be there, if it's not, then the
	 * message will be repeated every time nvi is started up.
	 */
	static int first = 1;
	DB *db;
	DBT data, key;
	recno_t msgno;
	char *p, *t, buf[PATH_MAX];

	if ((p = strrchr(file, '/')) != NULL && p[1] == '\0' &&
	    (((t = getenv("LC_MESSAGES")) != NULL && t[0] != '\0') ||
	    ((t = getenv("LANG")) != NULL && t[0] != '\0'))) {
		(void)snprintf(buf, sizeof(buf), "%s%s", file, t);
		p = buf;
	} else
		p = file;
	if ((db = dbopen(p,
	    O_NONBLOCK | O_RDONLY, 0, DB_RECNO, NULL)) == NULL) {
		if (first) {
			first = 0;
			return (1);
		}
		msgq_str(sp, M_SYSERR, p, "%s");
		return (1);
	}

	/*
	 * Test record 1 for the magic string.  The msgq call is here so
	 * the message catalog build finds it.
	 */
#define	VMC	"VI_MESSAGE_CATALOG"
	key.data = &msgno;
	key.size = sizeof(recno_t);
	msgno = 1;
	if (db->get(db, &key, &data, 0) != 0 ||
	    data.size != sizeof(VMC) - 1 ||
	    memcmp(data.data, VMC, sizeof(VMC) - 1)) {
		(void)db->close(db);
		if (first) {
			first = 0;
			return (1);
		}
		msgq_str(sp, M_ERR, p,
		    "030|The file %s is not a message catalog");
		return (1);
	}
	first = 0;

	if (sp->gp->msg != NULL)
		(void)sp->gp->msg->close(sp->gp->msg);
	sp->gp->msg = db;
	return (0);
}

/*
 * msg_close --
 *	Close the message catalogs.
 *
 * PUBLIC: void msg_close(GS *);
 */
void
msg_close(GS *gp)
{
	if (gp->msg != NULL)
		(void)gp->msg->close(gp->msg);
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
	switch (which) {
	case CMSG_CONF:
		return (msg_cat(sp, "268|confirm? [ynq]", lenp));
	case CMSG_CONT:
		return (msg_cat(sp, "269|Press any key to continue: ", lenp));
	case CMSG_CONT_EX:
		return (msg_cat(sp,
	    "270|Press any key to continue [: to enter more ex commands]: ",
		    lenp));
	case CMSG_CONT_R:
		return (msg_cat(sp, "161|Press Enter to continue: ", lenp));
	case CMSG_CONT_S:
		return (msg_cat(sp, "275| cont?", lenp));
	case CMSG_CONT_Q:
		return (msg_cat(sp,
		    "271|Press any key to continue [q to quit]: ", lenp));
	default:
		abort();
	}
	/* NOTREACHED */
}

/*
 * msg_cat --
 *	Return a single message from the catalog, plus its length.
 *
 * !!!
 * Only a single catalog message can be accessed at a time, if multiple
 * ones are needed, they must be copied into local memory.
 *
 * PUBLIC: const char *msg_cat(SCR *, const char *, size_t *);
 */
const char *
msg_cat(SCR *sp, const char *str, size_t *lenp)
{
	GS *gp;
	DBT data, key;
	recno_t msgno;

	/*
	 * If it's not a catalog message, i.e. has doesn't have a leading
	 * number and '|' symbol, we're done.
	 */
	if (isdigit(str[0]) &&
	    isdigit(str[1]) && isdigit(str[2]) && str[3] == '|') {
		key.data = &msgno;
		key.size = sizeof(recno_t);
		msgno = atoi(str);

		/*
		 * XXX
		 * Really sleazy hack -- we put an extra character on the
		 * end of the format string, and then we change it to be
		 * the nul termination of the string.  There ought to be
		 * a better way.  Once we can allocate multiple temporary
		 * memory buffers, maybe we can use one of them instead.
		 */
		gp = sp == NULL ? NULL : sp->gp;
		if (gp != NULL && gp->msg != NULL &&
		    gp->msg->get(gp->msg, &key, &data, 0) == 0 &&
		    data.size != 0) {
			if (lenp != NULL)
				*lenp = data.size - 1;
			((char *)data.data)[data.size - 1] = '\0';
			return (data.data);
		}
		str = &str[4];
	}
	if (lenp != NULL)
		*lenp = strlen(str);
	return (str);
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
