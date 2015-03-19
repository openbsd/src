/*	$OpenBSD: grep.c,v 1.44 2015/03/19 21:48:05 bcallah Exp $	*/

/* This file is in the public domain */

#include <sys/queue.h>
#include <sys/types.h>
#include <ctype.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "def.h"
#include "kbd.h"
#include "funmap.h"

int	 globalwd = FALSE;
static int	 compile_goto_error(int, int);
int		 next_error(int, int);
static int	 grep(int, int);
static int	 gid(int, int);
static struct buffer	*compile_mode(const char *, const char *);
void grep_init(void);

static char compile_last_command[NFILEN] = "make ";

/*
 * Hints for next-error
 *
 * XXX - need some kind of callback to find out when those get killed.
 */
struct mgwin	*compile_win;
struct buffer	*compile_buffer;

static PF compile_pf[] = {
	compile_goto_error
};

static struct KEYMAPE (1) compilemap = {
	1,
	1,
	rescan,
	{
		{ CCHR('M'), CCHR('M'), compile_pf, NULL }
	}
};

void
grep_init(void)
{
	funmap_add(compile_goto_error, "compile-goto-error");
	funmap_add(next_error, "next-error");
	funmap_add(grep, "grep");
	funmap_add(compile, "compile");
	funmap_add(gid, "gid");
	maps_add((KEYMAP *)&compilemap, "compile");
}

/* ARGSUSED */
static int
grep(int f, int n)
{
	char	 cprompt[NFILEN], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;

	(void)strlcpy(cprompt, "grep -n ", sizeof(cprompt));
	if ((bufp = eread("Run grep: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (strlcat(cprompt, " /dev/null", sizeof(cprompt)) >= sizeof(cprompt))
		return (FALSE);

	if ((bp = compile_mode("*grep*", cprompt)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	return (TRUE);
}

/* ARGSUSED */
int
compile(int f, int n)
{
	char	 cprompt[NFILEN], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;

	(void)strlcpy(cprompt, compile_last_command, sizeof(cprompt));
	if ((bufp = eread("Compile command: ", cprompt, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (savebuffers(f, n) == ABORT)
		return (ABORT);
	(void)strlcpy(compile_last_command, bufp, sizeof(compile_last_command));

	if ((bp = compile_mode("*compile*", cprompt)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	gotoline(FFARG, 0);
	return (TRUE);
}

/* id-utils foo. */
/* ARGSUSED */
static int
gid(int f, int n)
{
	char	 command[NFILEN];
	char	 cprompt[NFILEN], *bufp;
	int	c;
	struct buffer	*bp;
	struct mgwin	*wp;
	int	 i, j, len;

	/* catch ([^\s(){}]+)[\s(){}]* */

	i = curwp->w_doto;
	/* Skip backwards over delimiters we are currently on */
	while (i > 0) {
		c = lgetc(curwp->w_dotp, i);
		if (isalnum(c) || c == '_')
			break;

		i--;
	}

	/* Skip the symbol itself */
	for (; i > 0; i--) {
		c = lgetc(curwp->w_dotp, i - 1);
		if (!isalnum(c) && c != '_')
			break;
	}
	/* Fill the symbol in cprompt[] */
	for (j = 0; j < sizeof(cprompt) - 1 && i < llength(curwp->w_dotp);
	    j++, i++) {
		c = lgetc(curwp->w_dotp, i);
		if (!isalnum(c) && c != '_')
			break;
		cprompt[j] = c;
	}
	cprompt[j] = '\0';

	if ((bufp = eread("Run gid (with args): ", cprompt, NFILEN,
	    (j ? EFDEF : 0) | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	len = snprintf(command, sizeof(command), "gid %s", cprompt);
	if (len < 0 || len >= sizeof(command))
		return (FALSE);

	if ((bp = compile_mode("*gid*", command)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	compile_win = curwp = wp;
	return (TRUE);
}

struct buffer *
compile_mode(const char *name, const char *command)
{
	struct buffer	*bp;
	FILE	*fpipe;
	char	*buf;
	size_t	 len;
	int	 ret, n;
	char	 cwd[NFILEN], qcmd[NFILEN];
	char	 timestr[NTIME];
	time_t	 t;

	n = snprintf(qcmd, sizeof(qcmd), "%s 2>&1", command);
	if (n < 0 || n >= sizeof(qcmd))
		return (NULL);

	bp = bfind(name, TRUE);
	if (bclear(bp) != TRUE)
		return (NULL);

	if (getbufcwd(bp->b_cwd, sizeof(bp->b_cwd)) != TRUE)
		return (NULL);
	addlinef(bp, "cd %s", bp->b_cwd);
	addline(bp, qcmd);
	addline(bp, "");

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		panic("Can't get current directory!");
	if (chdir(bp->b_cwd) == -1) {
		dobeep();
		ewprintf("Can't change dir to %s", bp->b_cwd);
		return (NULL);
	}
	if ((fpipe = popen(qcmd, "r")) == NULL) {
		dobeep();
		ewprintf("Problem opening pipe");
		return (NULL);
	}
	/*
	 * We know that our commands are nice and the last line will end with
	 * a \n, so we don't need to try to deal with the last line problem
	 * in fgetln.
	 */
	while ((buf = fgetln(fpipe, &len)) != NULL) {
		buf[len - 1] = '\0';
		addline(bp, buf);
	}
	ret = pclose(fpipe);
	t = time(NULL);
	strftime(timestr, sizeof(timestr), "%a %b %e %T %Y", localtime(&t));
	addline(bp, "");
	if (ret != 0)
		addlinef(bp, "Command exited abnormally with code %d"
		    " at %s", ret, timestr);
	else
		addlinef(bp, "Command finished at %s", timestr);

	bp->b_dotp = bfirstlp(bp);
	bp->b_modes[0] = name_mode("fundamental");
	bp->b_modes[1] = name_mode("compile");
	bp->b_nmodes = 1;

	compile_buffer = bp;

	if (chdir(cwd) == -1) {
		dobeep();
		ewprintf("Can't change dir back to %s", cwd);
		return (NULL);
	}
	return (bp);
}

/* ARGSUSED */
static int
compile_goto_error(int f, int n)
{
	struct buffer	*bp;
	struct mgwin	*wp;
	char	*fname, *line, *lp, *ln;
	int	 lineno;
	char	*adjf, path[NFILEN];
	const char *errstr;
	struct line	*last;

	compile_win = curwp;
	compile_buffer = curbp;
	last = blastlp(compile_buffer);

 retry:
	/* last line is compilation result */
	if (curwp->w_dotp == last)
		return (FALSE);

	if ((line = linetostr(curwp->w_dotp)) == NULL)
		return (FALSE);
	lp = line;
	if ((fname = strsep(&lp, ":")) == NULL || *fname == '\0')
		goto fail;
	if ((ln = strsep(&lp, ":")) == NULL || *ln == '\0')
		goto fail;
	lineno = (int)strtonum(ln, INT_MIN, INT_MAX, &errstr);
	if (errstr)
		goto fail;

	if (fname && fname[0] != '/') {
		if (getbufcwd(path, sizeof(path)) == FALSE)
			goto fail;
		if (strlcat(path, fname, sizeof(path)) >= sizeof(path))
			goto fail;
		adjf = path;
	} else {
		adjf = adjustname(fname, TRUE);
	}
	free(line);

	if (adjf == NULL)
		return (FALSE);

	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] == '\0')
		readin(adjf);
	gotoline(FFARG, lineno);
	return (TRUE);
fail:
	free(line);
	if (curwp->w_dotp != blastlp(curbp)) {
		curwp->w_dotp = lforw(curwp->w_dotp);
		curwp->w_rflag |= WFMOVE;
		goto retry;
	}
	dobeep();
	ewprintf("No more hits");
	return (FALSE);
}

/* ARGSUSED */
int
next_error(int f, int n)
{
	if (compile_win == NULL || compile_buffer == NULL) {
		dobeep();
		ewprintf("No compilation active");
		return (FALSE);
	}
	curwp = compile_win;
	curbp = compile_buffer;
	if (curwp->w_dotp == blastlp(curbp)) {
		dobeep();
		ewprintf("No more hits");
		return (FALSE);
	}
	curwp->w_dotp = lforw(curwp->w_dotp);
	curwp->w_rflag |= WFMOVE;

	return (compile_goto_error(f, n));
}

/*
 * Since we don't have variables (we probably should) these are command
 * processors for changing the values of mode flags.
 */
/* ARGSUSED */
int
globalwdtoggle(int f, int n)
{
	if (f & FFARG)
		globalwd = n > 0;
	else
		globalwd = !globalwd;

	sgarbf = TRUE;

	return (TRUE);
}
