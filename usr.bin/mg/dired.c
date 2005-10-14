/*	$OpenBSD: dired.c,v 1.25 2005/10/14 15:41:33 deraadt Exp $	*/

/* This file is in the public domain. */

/* dired module for mg 2a
 * by Robert A. Larson
 */

#include "def.h"
#include "kbd.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>

#ifndef NO_DIRED

void		 dired_init(void);
static int	 d_makename(LINE  *, char *, int);

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
#ifndef NO_HELP
	NULL,			/* ^H */
#endif /* !NO_HELP */
};

static PF diredcl[] = {
	reposition,		/* ^L */
	d_findfile,		/* ^M */
	forwline,		/* ^N */
	rescan,			/* ^O */
	backline,		/* ^P */
	rescan,			/* ^Q */
	backisearch,		/* ^R */
	forwisearch,		/* ^S */
	rescan,			/* ^T */
	universal_argument,	/* ^U */
	forwpage,		/* ^V */
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
	forwline		/* SP */
};

static PF diredc[] = {
	d_copy,			/* c */
	d_del,			/* d */
	d_findfile,		/* e */
	d_findfile		/* f */
};

static PF diredn[] = {
	forwline,		/* n */
	d_ffotherwindow,	/* o */
	backline,		/* p */
	rescan,			/* q */
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

#ifndef	DIRED_XMAPS
#define	NDIRED_XMAPS	0	/* number of extra map sections */
#endif /* DIRED_XMAPS */

static struct KEYMAPE (6 + NDIRED_XMAPS + IMAPEXT) diredmap = {
	6 + NDIRED_XMAPS,
	6 + NDIRED_XMAPS + IMAPEXT,
	rescan,
	{
#ifndef NO_HELP
		{
			CCHR('@'), CCHR('H'), dirednul, (KEYMAP *) & helpmap
		},
#else /* !NO_HELP */
		{
			CCHR('@'), CCHR('G'), dirednul, NULL
		},
#endif /* !NO_HELP */
		{
			CCHR('L'), CCHR('X'), diredcl, (KEYMAP *) & cXmap
		},
		{
			CCHR('Z'), ' ', diredcz, (KEYMAP *) & metamap
		},
		{
			'c', 'f', diredc, NULL
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
	maps_add((KEYMAP *)&diredmap, "dired");
}

/* ARGSUSED */
int
dired(int f, int n)
{
	char	     dirname[NFILEN], *bufp, *slash;
	BUFFER	    *bp;

	if (curbp->b_fname && curbp->b_fname[0] != '\0') {
		(void)strlcpy(dirname, curbp->b_fname, sizeof(dirname));
		if ((slash = strrchr(dirname, '/')) != NULL) {
			*(slash + 1) = '\0';
		}
	} else {
		if (getcwd(dirname, sizeof(dirname)) == NULL)
			dirname[0] = '\0';
	}

	if ((bufp = eread("Dired: ", dirname, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	if (bufp[0] == '\0')
		return (FALSE);
	if ((bp = dired_(bufp)) == NULL)
		return (FALSE);

	curbp = bp;
	return (showbuffer(bp, curwp, WFHARD | WFMODE));
}

/* ARGSUSED */
int
d_otherwindow(int f, int n)
{
	char	 dirname[NFILEN], *bufp, *slash;
	BUFFER	*bp;
	MGWIN	*wp;

	if (curbp->b_fname && curbp->b_fname[0] != '\0') {
		(void)strlcpy(dirname, curbp->b_fname, sizeof(dirname));
		if ((slash = strrchr(dirname, '/')) != NULL) {
			*(slash + 1) = '\0';
		}
	} else {
		if (getcwd(dirname, sizeof(dirname)) == NULL)
			dirname[0] = '\0';
	}

	if ((bufp = eread("Dired other window: ", dirname, NFILEN,
	    EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if ((bp = dired_(bufp)) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp)) == NULL)
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
		if (lforw(curwp->w_dotp) != curbp->b_linep)
			curwp->w_dotp = lforw(curwp->w_dotp);
	}
	curwp->w_flag |= WFEDIT | WFMOVE;
	curwp->w_doto = 0;
	return (TRUE);
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
		if (lforw(curwp->w_dotp) != curbp->b_linep)
			curwp->w_dotp = lforw(curwp->w_dotp);
	}
	curwp->w_flag |= WFEDIT | WFMOVE;
	curwp->w_doto = 0;
	return (TRUE);
}

/* ARGSUSED */
int
d_undelbak(int f, int n)
{
	if (n < 0)
		return (d_undel(f, -n));
	while (n--) {
		if (llength(curwp->w_dotp) > 0)
			lputc(curwp->w_dotp, 0, ' ');
		if (lback(curwp->w_dotp) != curbp->b_linep)
			curwp->w_dotp = lback(curwp->w_dotp);
	}
	curwp->w_doto = 0;
	curwp->w_flag |= WFEDIT | WFMOVE;
	return (TRUE);
}

/* ARGSUSED */
int
d_findfile(int f, int n)
{
	BUFFER	*bp;
	int	 s;
	char	 fname[NFILEN];

	if ((s = d_makename(curwp->w_dotp, fname, sizeof(fname))) == ABORT)
		return (FALSE);
	if (s == TRUE)
		bp = dired_(fname);
	else
		bp = findbuffer(fname);
	if (bp == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFHARD) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] != 0)
		return (TRUE);
	return (readin(fname));
}

/* ARGSUSED */
int
d_ffotherwindow(int f, int n)
{
	char	fname[NFILEN];
	int	s;
	BUFFER *bp;
	MGWIN  *wp;

	if ((s = d_makename(curwp->w_dotp, fname, sizeof(fname))) == ABORT)
		return (FALSE);
	if ((bp = (s ? dired_(fname) : findbuffer(fname))) == NULL)
		return (FALSE);
	if ((wp = popbuf(bp)) == NULL)
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
	LINE	*lp, *nlp;
	char	 fname[NFILEN];

	for (lp = lforw(curbp->b_linep); lp != curbp->b_linep; lp = nlp) {
		nlp = lforw(lp);
		if (llength(lp) && lgetc(lp, 0) == 'D') {
			switch (d_makename(lp, fname, sizeof(fname))) {
			case ABORT:
				ewprintf("Bad line in dired buffer");
				return (FALSE);
			case FALSE:
				if (unlink(fname) < 0) {
					ewprintf("Could not delete '%s'",
					    basename(fname));
					return (FALSE);
				}
				break;
			case TRUE:
				if (rmdir(fname) < 0) {
					ewprintf("Could not delete directory '%s'",
					    basename(fname));
					return (FALSE);
				}
				break;
			}
			lfree(lp);
			curwp->w_flag |= WFHARD;
		}
	}
	return (TRUE);
}

/* ARGSUSED */
int
d_copy(int f, int n)
{
	char	frname[NFILEN], toname[NFILEN], *bufp;
	int	stat;
	size_t	off;
	BUFFER *bp;

	if (d_makename(curwp->w_dotp, frname, sizeof(frname)) != FALSE) {
		ewprintf("Not a file");
		return (FALSE);
	}
	off = strlcpy(toname, curbp->b_fname, sizeof(toname));
	if (off >= sizeof(toname) - 1) {	/* can't happen, really */
		ewprintf("Directory name too long");
		return (FALSE);
	}
	if ((bufp = eread("Copy %s to: ", toname, sizeof(toname),
	    EFDEF | EFNEW | EFCR, basename(frname))) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	stat = (copy(frname, toname) >= 0) ? TRUE : FALSE;
	if (stat != TRUE)
		return (stat);
	bp = dired_(curbp->b_fname);
	return (showbuffer(bp, curwp, WFHARD | WFMODE));
}

/* ARGSUSED */
int
d_rename(int f, int n)
{
	char	frname[NFILEN], toname[NFILEN], *bufp;
	int	stat;
	size_t	off;
	BUFFER *bp;

	if (d_makename(curwp->w_dotp, frname, sizeof(frname)) != FALSE) {
		ewprintf("Not a file");
		return (FALSE);
	}
	off = strlcpy(toname, curbp->b_fname, sizeof(toname));
	if (off >= sizeof(toname) - 1) {	/* can't happen, really */
		ewprintf("Directory name too long");
		return (FALSE);
	}
	if ((bufp = eread("Rename %s to: ", toname,
	    sizeof(toname), EFDEF | EFNEW | EFCR, basename(frname))) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	stat = (rename(frname, toname) >= 0) ? TRUE : FALSE;
	if (stat != TRUE)
		return (stat);
	bp = dired_(curbp->b_fname);
	return (showbuffer(bp, curwp, WFHARD | WFMODE));
}

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
int
d_shell_command(int f, int n)
{
	char	 command[512], fname[MAXPATHLEN], buf[BUFSIZ], *bufp, *cp;
	int	 infd, fds[2];
	pid_t	 pid;
	struct	 sigaction olda, newa;
	BUFFER	*bp;
	MGWIN	*wp;
	FILE	*fin;

	bp = bfind("*Shell Command Output*", TRUE);
	if (bclear(bp) != TRUE)
		return (ABORT);

	if (d_makename(curwp->w_dotp, fname, sizeof(fname)) != FALSE) {
		ewprintf("bad line");
		return (ABORT);
	}

	command[0] = '\0';
	if ((bufp = eread("! on %s: ", command, sizeof(command), EFNEW,
	    basename(fname))) == NULL)
		return (ABORT);
	infd = open(fname, O_RDONLY);
	if (infd == -1) {
		ewprintf("Can't open input file : %s", strerror(errno));
		return (FALSE);
	}
	if (pipe(fds) == -1) {
		ewprintf("Can't create pipe : %s", strerror(errno));
		close(infd);
		return (FALSE);
	}

	newa.sa_handler = reaper;
	newa.sa_flags = 0;
	if (sigaction(SIGCHLD, &newa, &olda) == -1) {
		close(infd);
		close(fds[0]);
		close(fds[1]);
		return (ABORT);
	}
	pid = fork();
	switch (pid) {
	case -1:
		ewprintf("Can't fork");
		return (ABORT);
	case 0:
		close(fds[0]);
		dup2(infd, STDIN_FILENO);
		dup2(fds[1], STDOUT_FILENO);
		dup2(fds[1], STDERR_FILENO);
		execl("/bin/sh", "sh", "-c", bufp, (char *)NULL);
		exit(1);
	default:
		close(infd);
		close(fds[1]);
		fin = fdopen(fds[0], "r");
		if (fin == NULL)	/* "r" is surely a valid mode! */
			panic("can't happen");
		while (fgets(buf, sizeof(buf), fin) != NULL) {
			cp = strrchr(buf, '\n');
			if (cp == NULL && !feof(fin)) {	/* too long a line */
				int c;
				addlinef(bp, "%s...", buf);
				while ((c = getc(fin)) != EOF && c != '\n')
					;
				continue;
			} else if (cp)
				*cp = '\0';
			addline(bp, buf);
		}
		fclose(fin);
		close(fds[0]);
		break;
	}
	wp = popbuf(bp);
	if (wp == NULL)
		return (ABORT);	/* XXX - free the buffer?? */
	curwp = wp;
	curbp = wp->w_bufp;
	if (sigaction(SIGCHLD, &olda, NULL) == -1)
		ewprintf("Warning, couldn't reset previous signal handler");
	return (TRUE);
}

int
d_create_directory(int f, int n)
{
	char	 tocreate[MAXPATHLEN], *bufp;
	size_t  off;
	BUFFER	*bp;

	off = strlcpy(tocreate, curbp->b_fname, sizeof(tocreate));
	if (off >= sizeof(tocreate) - 1)
		return (FALSE);
	if ((bufp = eread("Create directory: ", tocreate,
	    sizeof(tocreate), EFDEF | EFNEW | EFCR)) == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
	if (mkdir(tocreate, 0755) == -1) {
		ewprintf("Creating directory: %s, %s", strerror(errno),
		    tocreate);
		return (FALSE);
	}
	bp = dired_(curbp->b_fname);
	return (showbuffer(bp, curwp, WFHARD | WFMODE));
}

#define NAME_FIELD	8

static int
d_makename(LINE *lp, char *fn, int len)
{
	int	 i;
	char	*p, *ep;

	strlcpy(fn, curbp->b_fname, len);
	if ((p = lp->l_text) == NULL)
		return (ABORT);
	ep = lp->l_text + llength(lp);
	p++; /* skip action letter, if any */
	for (i = 0; i < NAME_FIELD; i++) {
		while (p < ep && isspace(*p))
			p++;
		while (p < ep && !isspace(*p))
			p++;
		while (p < ep && isspace(*p))
			p++;
		if (p == ep)
			return (ABORT);
	}
	strlcat(fn, p, len);
	return ((lgetc(lp, 2) == 'd') ? TRUE : FALSE);
}

/*
 * XXX dirname needs to have enough place to store an additional '/'.
 */
BUFFER *
dired_(char *dirname)
{
	BUFFER	*bp;
	FILE	*dirpipe;
	char	 line[256];
	int	 len;

	if ((dirname = adjustname(dirname)) == NULL) {
		ewprintf("Bad directory name");
		return (NULL);
	}
	/* this should not be done, instead adjustname() should get a flag */
	len = strlen(dirname);
	if (dirname[len - 1] != '/') {
		dirname[len++] = '/';
		dirname[len] = '\0';
	}
	if ((bp = findbuffer(dirname)) == NULL) {
		ewprintf("Could not create buffer");
		return (NULL);
	}
	if (bclear(bp) != TRUE)
		return (NULL);
	bp->b_flag |= BFREADONLY;
	if (snprintf(line, sizeof(line), "ls -al %s", dirname)
	    >= sizeof(line)) {
		ewprintf("Path too long");
		return (NULL);
	}
	if ((dirpipe = popen(line, "r")) == NULL) {
		ewprintf("Problem opening pipe to ls");
		return (NULL);
	}
	line[0] = line[1] = ' ';
	while (fgets(&line[2], sizeof(line) - 2, dirpipe) != NULL) {
		line[strlen(line) - 1] = '\0';	/* remove ^J	 */
		(void) addline(bp, line);
	}
	if (pclose(dirpipe) == -1) {
		ewprintf("Problem closing pipe to ls : %s",
		    strerror(errno));
		return (NULL);
	}
	bp->b_dotp = lforw(bp->b_linep);	/* go to first line */
	(void) strlcpy(bp->b_fname, dirname, sizeof(bp->b_fname));
	if ((bp->b_modes[1] = name_mode("dired")) == NULL) {
		bp->b_modes[0] = name_mode("fundamental");
		ewprintf("Could not find mode dired");
		return (NULL);
	}
	bp->b_nmodes = 1;
	return (bp);
}

#endif /* !NO_DIRED */
