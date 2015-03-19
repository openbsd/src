/*	$OpenBSD: dired.c,v 1.70 2015/03/19 21:22:15 bcallah Exp $	*/

/* This file is in the public domain. */

/* dired module for mg 2a
 * by Robert A. Larson
 */

#include <sys/queue.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "def.h"
#include "funmap.h"
#include "kbd.h"

void		 dired_init(void);
static int	 dired(int, int);
static int	 d_otherwindow(int, int);
static int	 d_undel(int, int);
static int	 d_undelbak(int, int);
static int	 d_findfile(int, int);
static int	 d_ffotherwindow(int, int);
static int	 d_expunge(int, int);
static int	 d_copy(int, int);
static int	 d_del(int, int);
static int	 d_rename(int, int);
static int	 d_exec(int, struct buffer *, const char *, const char *, ...);
static int	 d_shell_command(int, int);
static int	 d_create_directory(int, int);
static int	 d_makename(struct line *, char *, size_t);
static int	 d_warpdot(struct line *, int *);
static int	 d_forwpage(int, int);
static int	 d_backpage(int, int);
static int	 d_forwline(int, int);
static int	 d_backline(int, int);
static int	 d_killbuffer_cmd(int, int);
static int	 d_refreshbuffer(int, int);
static void	 reaper(int);
static struct buffer	*refreshbuffer(struct buffer *);

extern struct keymap_s helpmap, cXmap, metamap;

static PF dirednul[] = {
	setmark,		/* ^@ */
	gotobol,		/* ^A */
	backchar,		/* ^B */
	rescan,			/* ^C */
	d_del,			/* ^D */
	gotoeol,		/* ^E */
	forwchar,		/* ^F */
	ctrlg,			/* ^G */
	NULL,			/* ^H */
};

static PF diredcl[] = {
	reposition,		/* ^L */
	d_findfile,		/* ^M */
	d_forwline,		/* ^N */
	rescan,			/* ^O */
	d_backline,		/* ^P */
	rescan,			/* ^Q */
	backisearch,		/* ^R */
	forwisearch,		/* ^S */
	rescan,			/* ^T */
	universal_argument,	/* ^U */
	d_forwpage,		/* ^V */
	rescan,			/* ^W */
	NULL			/* ^X */
};

static PF diredcz[] = {
	spawncli,		/* ^Z */
	NULL,			/* esc */
	rescan,			/* ^\ */
	rescan,			/* ^] */
	rescan,			/* ^^ */
	rescan,			/* ^_ */
	d_forwline,		/* SP */
	d_shell_command,	/* ! */
	rescan,			/* " */
	rescan,			/* # */
	rescan,			/* $ */
	rescan,			/* % */
	rescan,			/* & */
	rescan,			/* ' */
	rescan,			/* ( */
	rescan,			/* ) */
	rescan,			/* * */
	d_create_directory	/* + */
};

static PF diredc[] = {
	d_copy,			/* c */
	d_del,			/* d */
	d_findfile,		/* e */
	d_findfile,		/* f */
	d_refreshbuffer		/* g */
};

static PF diredn[] = {
	d_forwline,		/* n */
	d_ffotherwindow,	/* o */
	d_backline,		/* p */
	d_killbuffer_cmd,	/* q */
	d_rename,		/* r */
	rescan,			/* s */
	rescan,			/* t */
	d_undel,		/* u */
	rescan,			/* v */
	rescan,			/* w */
	d_expunge		/* x */
};

static PF direddl[] = {
	d_undelbak		/* del */
};

static PF diredbp[] = {
	d_backpage		/* v */	
};

static PF dirednull[] = {
	NULL
};

#ifndef	DIRED_XMAPS
#define	NDIRED_XMAPS	0	/* number of extra map sections */
#endif /* DIRED_XMAPS */

static struct KEYMAPE (1 + IMAPEXT) d_backpagemap = {
	1,
	1 + IMAPEXT,
	rescan,                 
	{
		{
		'v', 'v', diredbp, NULL
		}
	}
};

static struct KEYMAPE (7 + NDIRED_XMAPS + IMAPEXT) diredmap = {
	7 + NDIRED_XMAPS,
	7 + NDIRED_XMAPS + IMAPEXT,
	rescan,
	{
		{
			CCHR('@'), CCHR('H'), dirednul, (KEYMAP *) & helpmap
		},
		{
			CCHR('L'), CCHR('X'), diredcl, (KEYMAP *) & cXmap
		},
		{
			CCHR('['), CCHR('['), dirednull, (KEYMAP *) & 
			d_backpagemap
		},
		{
			CCHR('Z'), '+', diredcz, (KEYMAP *) & metamap
		},
		{
			'c', 'g', diredc, NULL
		},
		{
			'n', 'x', diredn, NULL
		},
		{
			CCHR('?'), CCHR('?'), direddl, NULL
		},
#ifdef	DIRED_XMAPS
		DIRED_XMAPS,	/* map sections for dired mode keys	 */
#endif /* DIRED_XMAPS */
	}
};

void
dired_init(void)
{
	funmap_add(dired, "dired");
	funmap_add(d_undelbak, "dired-unmark-backward");
	funmap_add(d_create_directory, "dired-create-directory");
	funmap_add(d_copy, "dired-do-copy");
	funmap_add(d_expunge, "dired-do-flagged-delete");
	funmap_add(d_findfile, "dired-find-file");
	funmap_add(d_ffotherwindow, "dired-find-file-other-window");
	funmap_add(d_del, "dired-flag-file-deletion");
	funmap_add(d_forwline, "dired-next-line");
	funmap_add(d_otherwindow, "dired-other-window");
	funmap_add(d_backline, "dired-previous-line");
	funmap_add(d_rename, "dired-do-rename");
	funmap_add(d_backpage, "dired-scroll-down");
	funmap_add(d_forwpage, "dired-scroll-up");
	funmap_add(d_undel, "dired-unmark");
	funmap_add(d_killbuffer_cmd, "quit-window");
	maps_add((KEYMAP *)&diredmap, "dired");
	dobindkey(fundamental_map, "dired", "^Xd");
}

/* ARGSUSED */
int
dired(int f, int n)
{
	char		 dname[NFILEN], *bufp, *slash;
	struct buffer	*bp;

	if (curbp->b_fname[0] != '\0') {
		(void)strlcpy(dname, curbp->b_fname, sizeof(dname));
		if ((slash = strrchr(dname, '/')) != NULL) {
			*(slash + 1) = '\0';
		}
	} else {
		if (getcwd(dname, sizeof(dname)) == NULL)
			dname[0] = '\0';
	}

	if ((bufp = eread("Dired: ", dname, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		return (FALSE);
	if ((bp = dired_(bufp)) == NULL)
		return (FALSE);

	curbp = bp;
	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

/* ARGSUSED */
int
d_otherwindow(int f, int n)
{
	char		 dname[NFILEN], *bufp, *slash;
	struct buffer	*bp;
	struct mgwin	*wp;

	if (curbp->b_fname[0] != '\0') {
		(void)strlcpy(dname, curbp->b_fname, sizeof(dname));
		if ((slash = strrchr(dname, '/')) != NULL) {
			*(slash + 1) = '\0';
		}
	} else {
		if (getcwd(dname, sizeof(dname)) == NULL)
			dname[0] = '\0';
	}

	if ((bufp = eread("Dired other window: ", dname, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if ((bp = dired_(bufp)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	return (TRUE);
}

/* ARGSUSED */
int
d_del(int f, int n)
{
	if (n < 0)
		return (FALSE);
	while (n--) {
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, 'D');
		if (lforw(curwp->w_dotp) != curbp->b_headp)
			curwp->w_dotp = lforw(curwp->w_dotp);
	}
	curwp->w_rflag |= WFEDIT | WFMOVE;
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

/* ARGSUSED */
int
d_undel(int f, int n)
{
	if (n < 0)
		return (d_undelbak(f, -n));
	while (n--) {
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, ' ');
		if (lforw(curwp->w_dotp) != curbp->b_headp)
			curwp->w_dotp = lforw(curwp->w_dotp);
	}
	curwp->w_rflag |= WFEDIT | WFMOVE;
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

/* ARGSUSED */
int
d_undelbak(int f, int n)
{
	if (n < 0)
		return (d_undel(f, -n));
	while (n--) {
		if (lback(curwp->w_dotp) != curbp->b_headp)
			curwp->w_dotp = lback(curwp->w_dotp);
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, ' ');
	}
	curwp->w_rflag |= WFEDIT | WFMOVE;
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

/* ARGSUSED */
int
d_findfile(int f, int n)
{
	struct buffer	*bp;
	int		 s;
	char		 fname[NFILEN];

	if ((s = d_makename(curwp->w_dotp, fname, sizeof(fname))) == ABORT)
		return (FALSE);
	if (s == TRUE)
		bp = dired_(fname);
	else
		bp = findbuffer(fname);
	if (bp == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFFULL) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] != 0)
		return (TRUE);
	return (readin(fname));
}

/* ARGSUSED */
int
d_ffotherwindow(int f, int n)
{
	char		 fname[NFILEN];
	int		 s;
	struct buffer	*bp;
	struct mgwin	*wp;

	if ((s = d_makename(curwp->w_dotp, fname, sizeof(fname))) == ABORT)
		return (FALSE);
	if ((bp = (s ? dired_(fname) : findbuffer(fname))) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (FALSE);
	curbp = bp;
	curwp = wp;
	if (bp->b_fname[0] != 0)
		return (TRUE);	/* never true for dired buffers */
	return (readin(fname));
}

/* ARGSUSED */
int
d_expunge(int f, int n)
{
	struct line	*lp, *nlp;
	char		 fname[NFILEN], sname[NFILEN];

	for (lp = bfirstlp(curbp); lp != curbp->b_headp; lp = nlp) {
		nlp = lforw(lp);
		if (llength(lp) && lgetc(lp, 0) == 'D') {
			switch (d_makename(lp, fname, sizeof(fname))) {
			case ABORT:
				dobeep();
				ewprintf("Bad line in dired buffer");
				return (FALSE);
			case FALSE:
				if (unlink(fname) < 0) {
					(void)xbasename(sname, fname, NFILEN);
					dobeep();
					ewprintf("Could not delete '%s'", sname);
					return (FALSE);
				}
				break;
			case TRUE:
				if (rmdir(fname) < 0) {
					(void)xbasename(sname, fname, NFILEN);
					dobeep();
					ewprintf("Could not delete directory "
					    "'%s'", sname);
					return (FALSE);
				}
				break;
			}
			lfree(lp);
			curwp->w_bufp->b_lines--;
			curwp->w_rflag |= WFFULL;
		}
	}
	return (TRUE);
}

/* ARGSUSED */
int
d_copy(int f, int n)
{
	char		 frname[NFILEN], toname[NFILEN], sname[NFILEN];
	char		*topath, *bufp;
	int		 ret;
	size_t		 off;
	struct buffer	*bp;

	if (d_makename(curwp->w_dotp, frname, sizeof(frname)) != FALSE) {
		dobeep();
		ewprintf("Not a file");
		return (FALSE);
	}
	off = strlcpy(toname, curbp->b_fname, sizeof(toname));
	if (off >= sizeof(toname) - 1) {	/* can't happen, really */
		dobeep();
		ewprintf("Directory name too long");
		return (FALSE);
	}
	(void)xbasename(sname, frname, NFILEN);
	bufp = eread("Copy %s to: ", toname, sizeof(toname),
	    EFDEF | EFNEW | EFCR, sname);
	if (bufp == NULL) 
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	topath = adjustname(toname, TRUE);
	ret = (copy(frname, topath) >= 0) ? TRUE : FALSE;
	if (ret != TRUE)
		return (ret);
	if ((bp = refreshbuffer(curbp)) == NULL)
		return (FALSE);
	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

/* ARGSUSED */
int
d_rename(int f, int n)
{
	char		 frname[NFILEN], toname[NFILEN];
	char		*topath, *bufp;
	int		 ret;
	size_t		 off;
	struct buffer	*bp;
	char		 sname[NFILEN];

	if (d_makename(curwp->w_dotp, frname, sizeof(frname)) != FALSE) {
		dobeep();
		ewprintf("Not a file");
		return (FALSE);
	}
	off = strlcpy(toname, curbp->b_fname, sizeof(toname));
	if (off >= sizeof(toname) - 1) {	/* can't happen, really */
		dobeep();
		ewprintf("Directory name too long");
		return (FALSE);
	}
	(void)xbasename(sname, frname, NFILEN);
	bufp = eread("Rename %s to: ", toname,
	    sizeof(toname), EFDEF | EFNEW | EFCR, sname);
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);

	topath = adjustname(toname, TRUE);
	ret = (rename(frname, topath) >= 0) ? TRUE : FALSE;
	if (ret != TRUE)
		return (ret);
	if ((bp = refreshbuffer(curbp)) == NULL)
		return (FALSE);
	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

/* ARGSUSED */
void
reaper(int signo __attribute__((unused)))
{
	int	save_errno = errno, status;

	while (waitpid(-1, &status, WNOHANG) >= 0)
		;
	errno = save_errno;
}

/*
 * Pipe the currently selected file through a shell command.
 */
/* ARGSUSED */
int
d_shell_command(int f, int n)
{
	char		 command[512], fname[PATH_MAX], *bufp;
	struct buffer	*bp;
	struct mgwin	*wp;
	char		 sname[NFILEN];

	bp = bfind("*Shell Command Output*", TRUE);
	if (bclear(bp) != TRUE)
		return (ABORT);

	if (d_makename(curwp->w_dotp, fname, sizeof(fname)) != FALSE) {
		dobeep();
		ewprintf("bad line");
		return (ABORT);
	}

	command[0] = '\0';
	(void)xbasename(sname, fname, NFILEN);
	bufp = eread("! on %s: ", command, sizeof(command), EFNEW, sname);
	if (bufp == NULL)
		return (ABORT);

	if (d_exec(0, bp, fname, "sh", "-c", command, NULL) != TRUE)
		return (ABORT);

	if ((wp = popbuf(bp, WNONE)) == NULL)
		return (ABORT);	/* XXX - free the buffer?? */
	curwp = wp;
	curbp = wp->w_bufp;
	return (TRUE);
}

/*
 * Pipe input file to cmd and insert the command's output in the
 * given buffer.  Each line will be prefixed with the given
 * number of spaces.
 */
static int
d_exec(int space, struct buffer *bp, const char *input, const char *cmd, ...)
{
	char	 buf[BUFSIZ];
	va_list	 ap;
	struct	 sigaction olda, newa;
	char	**argv = NULL, *cp;
	FILE	*fin;
	int	 fds[2] = { -1, -1 };
	int	 infd = -1;
	int	 ret = (ABORT), n;
	pid_t	 pid;

	if (sigaction(SIGCHLD, NULL, &olda) == -1)
		return (ABORT);

	/* Find the number of arguments. */
	va_start(ap, cmd);
	for (n = 2; va_arg(ap, char *) != NULL; n++)
		;
	va_end(ap);

	/* Allocate and build the argv. */
	if ((argv = calloc(n, sizeof(*argv))) == NULL) {
		dobeep();
		ewprintf("Can't allocate argv : %s", strerror(errno));
		goto out;
	}

	n = 1;
	argv[0] = (char *)cmd;
	va_start(ap, cmd);
	while ((argv[n] = va_arg(ap, char *)) != NULL)
		n++;
	va_end(ap);

	if (input == NULL)
		input = "/dev/null";

	if ((infd = open(input, O_RDONLY)) == -1) {
		dobeep();
		ewprintf("Can't open input file : %s", strerror(errno));
		goto out;
	}

	if (pipe(fds) == -1) {
		dobeep();
		ewprintf("Can't create pipe : %s", strerror(errno));
		goto out;
	}

	newa.sa_handler = reaper;
	newa.sa_flags = 0;
	if (sigaction(SIGCHLD, &newa, NULL) == -1)
		goto out;

	if ((pid = fork()) == -1) {
		dobeep();
		ewprintf("Can't fork");
		goto out;
	}

	switch (pid) {
	case 0: /* Child */
		close(fds[0]);
		dup2(infd, STDIN_FILENO);
		dup2(fds[1], STDOUT_FILENO);
		dup2(fds[1], STDERR_FILENO);
		if (execvp(argv[0], argv) == -1)
			ewprintf("Can't exec %s: %s", argv[0], strerror(errno));
		exit(1);
		break;
	default: /* Parent */
		close(infd);
		close(fds[1]);
		infd = fds[1] = -1;
		if ((fin = fdopen(fds[0], "r")) == NULL)
			goto out;
		while (fgets(buf, sizeof(buf), fin) != NULL) {
			cp = strrchr(buf, '\n');
			if (cp == NULL && !feof(fin)) {	/* too long a line */
				int c;
				addlinef(bp, "%*s%s...", space, "", buf);
				while ((c = getc(fin)) != EOF && c != '\n')
					;
				continue;
			} else if (cp)
				*cp = '\0';
			addlinef(bp, "%*s%s", space, "", buf);
		}
		fclose(fin);
		break;
	}
	ret = (TRUE);

out:
	if (sigaction(SIGCHLD, &olda, NULL) == -1)
		ewprintf("Warning, couldn't reset previous signal handler");
	if (fds[0] != -1)
		close(fds[0]);
	if (fds[1] != -1)
		close(fds[1]);
	if (infd != -1)
		close(infd);
	if (argv != NULL)
		free(argv);
	return ret;
}

/* ARGSUSED */
int
d_create_directory(int f, int n)
{
	int ret;
	struct buffer	*bp;

	ret = ask_makedir();
	if (ret != TRUE)
		return(ret);

	if ((bp = refreshbuffer(curbp)) == NULL)
		return (FALSE);

	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

/* ARGSUSED */
int
d_killbuffer_cmd(int f, int n)
{
	return(killbuffer_cmd(FFRAND, 0));
}

int
d_refreshbuffer(int f, int n)
{
	struct buffer *bp;

	if ((bp = refreshbuffer(curbp)) == NULL)
		return (FALSE);

	return (showbuffer(bp, curwp, WFFULL | WFMODE));
}

struct buffer *
refreshbuffer(struct buffer *bp)
{
	char	*tmp;

	tmp = strdup(bp->b_fname);
	if (tmp == NULL) {
		dobeep();
		ewprintf("Out of memory");
		return (NULL);
	}

	killbuffer(bp);

	/* dired_() uses findbuffer() to create new buffer */
	if ((bp = dired_(tmp)) == NULL) {
		free(tmp);
		return (NULL);
	}
	free(tmp);
	curbp = bp;

	return (bp);
}

static int
d_makename(struct line *lp, char *fn, size_t len)
{
	int	 start, nlen;
	char	*namep;

	if (d_warpdot(lp, &start) == FALSE)
		return (ABORT);
	namep = &lp->l_text[start];
	nlen = llength(lp) - start;

	if (snprintf(fn, len, "%s%.*s", curbp->b_fname, nlen, namep) >= len)
		return (ABORT); /* Name is too long. */

	/* Return TRUE if the entry is a directory. */
	return ((lgetc(lp, 2) == 'd') ? TRUE : FALSE);
}

#define NAME_FIELD	9

static int
d_warpdot(struct line *dotp, int *doto)
{
	char *tp = dotp->l_text;
	int off = 0, field = 0, len;

	/*
	 * Find the byte offset to the (space-delimited) filename
	 * field in formatted ls output.
	 */
	len = llength(dotp);
	while (off < len) {
		if (tp[off++] == ' ') {
			if (++field == NAME_FIELD) {
				*doto = off;
				return (TRUE);
			}
			/* Skip the space. */
			while (off < len && tp[off] == ' ')
				off++;
		}
	}
	/* We didn't find the field. */
	*doto = 0;
	return (FALSE);
}

static int
d_forwpage(int f, int n) 
{
	forwpage(f | FFRAND, n);
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

static int 
d_backpage (int f, int n)
{
	backpage(f | FFRAND, n);
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

static int
d_forwline (int f, int n)
{
	forwline(f | FFRAND, n);
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

static int
d_backline (int f, int n)
{
	backline(f | FFRAND, n);
	return (d_warpdot(curwp->w_dotp, &curwp->w_doto));
}

/*
 * XXX dname needs to have enough place to store an additional '/'.
 */
struct buffer *
dired_(char *dname)
{
	struct buffer	*bp;
	int		 i;
	size_t		 len;

	if ((dname = adjustname(dname, FALSE)) == NULL) {
		dobeep();
		ewprintf("Bad directory name");
		return (NULL);
	}
	/* this should not be done, instead adjustname() should get a flag */
	len = strlen(dname);
	if (dname[len - 1] != '/') {
		dname[len++] = '/';
		dname[len] = '\0';
	}
	if ((access(dname, R_OK | X_OK)) == -1) {
		if (errno == EACCES) {
			dobeep();
			ewprintf("Permission denied");
		}
		return (NULL);
	}
	if ((bp = findbuffer(dname)) == NULL) {
		dobeep();
		ewprintf("Could not create buffer");
		return (NULL);
	}
	if (bclear(bp) != TRUE)
		return (NULL);
	bp->b_flag |= BFREADONLY | BFIGNDIRTY;

	if ((d_exec(2, bp, NULL, "ls", "-al", dname, NULL)) != TRUE)
		return (NULL);

	/* Find the line with ".." on it. */
	bp->b_dotp = bfirstlp(bp);
	for (i = 0; i < bp->b_lines; i++) {
		bp->b_dotp = lforw(bp->b_dotp);
		if (d_warpdot(bp->b_dotp, &bp->b_doto) == FALSE)
			continue;
		if (strcmp(ltext(bp->b_dotp) + bp->b_doto, "..") == 0)
			break;
	}

	/* We want dot on the entry right after "..", if possible. */
	if (++i < bp->b_lines - 2)
		bp->b_dotp = lforw(bp->b_dotp);
	d_warpdot(bp->b_dotp, &bp->b_doto);

	(void)strlcpy(bp->b_fname, dname, sizeof(bp->b_fname));
	(void)strlcpy(bp->b_cwd, dname, sizeof(bp->b_cwd));
	if ((bp->b_modes[1] = name_mode("dired")) == NULL) {
		bp->b_modes[0] = name_mode("fundamental");
		dobeep();
		ewprintf("Could not find mode dired");
		return (NULL);
	}
	bp->b_nmodes = 1;
	return (bp);
}
