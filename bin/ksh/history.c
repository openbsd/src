/*	$OpenBSD: history.c,v 1.59 2017/05/29 13:09:17 tb Exp $	*/

/*
 * command history
 */

/*
 *	This file contains
 *	a)	the original in-memory history  mechanism
 *	b)	a more complicated mechanism done by  pc@hillside.co.uk
 *		that more closely follows the real ksh way of doing
 *		things.
 */

#include <sys/stat.h>
#include <sys/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "sh.h"

#ifdef HISTORY

static void	history_write(void);
static FILE	*history_open(void);
static int	history_load(Source *);
static void	history_close(void);

static int	hist_execute(char *);
static int	hist_replace(char **, const char *, const char *, int);
static char   **hist_get(const char *, int, int);
static char   **hist_get_oldest(void);
static void	histbackup(void);

static FILE	*histfh;
static char   **current;	/* current position in history[] */
static char    *hname;		/* current name of history file */
static int	hstarted;	/* set after hist_init() called */
static Source	*hist_source;
static uint32_t	line_co;

static struct stat last_sb;

int
c_fc(char **wp)
{
	struct shf *shf;
	struct temp *tf = NULL;
	char *p, *editor = NULL;
	int gflag = 0, lflag = 0, nflag = 0, sflag = 0, rflag = 0;
	int optc, ret;
	char *first = NULL, *last = NULL;
	char **hfirst, **hlast, **hp;
	static int depth;

	if (depth != 0) {
		bi_errorf("history function called recursively");
		return 1;
	}

	if (!Flag(FTALKING_I)) {
		bi_errorf("history functions not available");
		return 1;
	}

	while ((optc = ksh_getopt(wp, &builtin_opt,
	    "e:glnrs0,1,2,3,4,5,6,7,8,9,")) != -1)
		switch (optc) {
		case 'e':
			p = builtin_opt.optarg;
			if (strcmp(p, "-") == 0)
				sflag++;
			else {
				size_t len = strlen(p) + 4;
				editor = str_nsave(p, len, ATEMP);
				strlcat(editor, " $_", len);
			}
			break;
		case 'g': /* non-at&t ksh */
			gflag++;
			break;
		case 'l':
			lflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'r':
			rflag++;
			break;
		case 's':	/* posix version of -e - */
			sflag++;
			break;
		  /* kludge city - accept -num as -- -num (kind of) */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			p = shf_smprintf("-%c%s",
					optc, builtin_opt.optarg);
			if (!first)
				first = p;
			else if (!last)
				last = p;
			else {
				bi_errorf("too many arguments");
				return 1;
			}
			break;
		case '?':
			return 1;
		}
	wp += builtin_opt.optind;

	/* Substitute and execute command */
	if (sflag) {
		char *pat = NULL, *rep = NULL;

		if (editor || lflag || nflag || rflag) {
			bi_errorf("can't use -e, -l, -n, -r with -s (-e -)");
			return 1;
		}

		/* Check for pattern replacement argument */
		if (*wp && **wp && (p = strchr(*wp + 1, '='))) {
			pat = str_save(*wp, ATEMP);
			p = pat + (p - *wp);
			*p++ = '\0';
			rep = p;
			wp++;
		}
		/* Check for search prefix */
		if (!first && (first = *wp))
			wp++;
		if (last || *wp) {
			bi_errorf("too many arguments");
			return 1;
		}

		hp = first ? hist_get(first, false, false) :
		    hist_get_newest(false);
		if (!hp)
			return 1;
		depth++;
		ret = hist_replace(hp, pat, rep, gflag);
		depth--;
		return ret;
	}

	if (editor && (lflag || nflag)) {
		bi_errorf("can't use -l, -n with -e");
		return 1;
	}

	if (!first && (first = *wp))
		wp++;
	if (!last && (last = *wp))
		wp++;
	if (*wp) {
		bi_errorf("too many arguments");
		return 1;
	}
	if (!first) {
		hfirst = lflag ? hist_get("-16", true, true) :
		    hist_get_newest(false);
		if (!hfirst)
			return 1;
		/* can't fail if hfirst didn't fail */
		hlast = hist_get_newest(false);
	} else {
		/* POSIX says not an error if first/last out of bounds
		 * when range is specified; at&t ksh and pdksh allow out of
		 * bounds for -l as well.
		 */
		hfirst = hist_get(first, (lflag || last) ? true : false,
		    lflag ? true : false);
		if (!hfirst)
			return 1;
		hlast = last ? hist_get(last, true, lflag ? true : false) :
		    (lflag ? hist_get_newest(false) : hfirst);
		if (!hlast)
			return 1;
	}
	if (hfirst > hlast) {
		char **temp;

		temp = hfirst; hfirst = hlast; hlast = temp;
		rflag = !rflag; /* POSIX */
	}

	/* List history */
	if (lflag) {
		char *s, *t;
		const char *nfmt = nflag ? "\t" : "%d\t";

		for (hp = rflag ? hlast : hfirst;
		    hp >= hfirst && hp <= hlast; hp += rflag ? -1 : 1) {
			shf_fprintf(shl_stdout, nfmt,
			    hist_source->line - (int) (histptr - hp));
			/* print multi-line commands correctly */
			for (s = *hp; (t = strchr(s, '\n')); s = t)
				shf_fprintf(shl_stdout, "%.*s\t", ++t - s, s);
			shf_fprintf(shl_stdout, "%s\n", s);
		}
		shf_flush(shl_stdout);
		return 0;
	}

	/* Run editor on selected lines, then run resulting commands */

	tf = maketemp(ATEMP, TT_HIST_EDIT, &genv->temps);
	if (!(shf = tf->shf)) {
		bi_errorf("cannot create temp file %s - %s",
		    tf->name, strerror(errno));
		return 1;
	}
	for (hp = rflag ? hlast : hfirst;
	    hp >= hfirst && hp <= hlast; hp += rflag ? -1 : 1)
		shf_fprintf(shf, "%s\n", *hp);
	if (shf_close(shf) == EOF) {
		bi_errorf("error writing temporary file - %s", strerror(errno));
		return 1;
	}

	/* Ignore setstr errors here (arbitrary) */
	setstr(local("_", false), tf->name, KSH_RETURN_ERROR);

	/* XXX: source should not get trashed by this.. */
	{
		Source *sold = source;

		ret = command(editor ? editor : "${FCEDIT:-/bin/ed} $_", 0);
		source = sold;
		if (ret)
			return ret;
	}

	{
		struct stat statb;
		XString xs;
		char *xp;
		int n;

		if (!(shf = shf_open(tf->name, O_RDONLY, 0, 0))) {
			bi_errorf("cannot open temp file %s", tf->name);
			return 1;
		}

		n = fstat(shf->fd, &statb) < 0 ? 128 :
		    statb.st_size + 1;
		Xinit(xs, xp, n, hist_source->areap);
		while ((n = shf_read(xp, Xnleft(xs, xp), shf)) > 0) {
			xp += n;
			if (Xnleft(xs, xp) <= 0)
				XcheckN(xs, xp, Xlength(xs, xp));
		}
		if (n < 0) {
			bi_errorf("error reading temp file %s - %s",
			    tf->name, strerror(shf->errno_));
			shf_close(shf);
			return 1;
		}
		shf_close(shf);
		*xp = '\0';
		strip_nuls(Xstring(xs, xp), Xlength(xs, xp));
		depth++;
		ret = hist_execute(Xstring(xs, xp));
		depth--;
		return ret;
	}
}

/* Save cmd in history, execute cmd (cmd gets trashed) */
static int
hist_execute(char *cmd)
{
	Source *sold;
	int ret;
	char *p, *q;

	histbackup();

	for (p = cmd; p; p = q) {
		if ((q = strchr(p, '\n'))) {
			*q++ = '\0'; /* kill the newline */
			if (!*q) /* ignore trailing newline */
				q = NULL;
		}
		histsave(++(hist_source->line), p, 1);

		shellf("%s\n", p); /* POSIX doesn't say this is done... */
		if ((p = q)) /* restore \n (trailing \n not restored) */
			q[-1] = '\n';
	}

	/* Commands are executed here instead of pushing them onto the
	 * input 'cause posix says the redirection and variable assignments
	 * in
	 *	X=y fc -e - 42 2> /dev/null
	 * are to effect the repeated commands environment.
	 */
	/* XXX: source should not get trashed by this.. */
	sold = source;
	ret = command(cmd, 0);
	source = sold;
	return ret;
}

static int
hist_replace(char **hp, const char *pat, const char *rep, int global)
{
	char *line;

	if (!pat)
		line = str_save(*hp, ATEMP);
	else {
		char *s, *s1;
		int pat_len = strlen(pat);
		int rep_len = strlen(rep);
		int len;
		XString xs;
		char *xp;
		int any_subst = 0;

		Xinit(xs, xp, 128, ATEMP);
		for (s = *hp; (s1 = strstr(s, pat)) && (!any_subst || global);
		    s = s1 + pat_len) {
			any_subst = 1;
			len = s1 - s;
			XcheckN(xs, xp, len + rep_len);
			memcpy(xp, s, len);		/* first part */
			xp += len;
			memcpy(xp, rep, rep_len);	/* replacement */
			xp += rep_len;
		}
		if (!any_subst) {
			bi_errorf("substitution failed");
			return 1;
		}
		len = strlen(s) + 1;
		XcheckN(xs, xp, len);
		memcpy(xp, s, len);
		xp += len;
		line = Xclose(xs, xp);
	}
	return hist_execute(line);
}

/*
 * get pointer to history given pattern
 * pattern is a number or string
 */
static char **
hist_get(const char *str, int approx, int allow_cur)
{
	char **hp = NULL;
	int n;

	if (getn(str, &n)) {
		hp = histptr + (n < 0 ? n : (n - hist_source->line));
		if ((long)hp < (long)history) {
			if (approx)
				hp = hist_get_oldest();
			else {
				bi_errorf("%s: not in history", str);
				hp = NULL;
			}
		} else if (hp > histptr) {
			if (approx)
				hp = hist_get_newest(allow_cur);
			else {
				bi_errorf("%s: not in history", str);
				hp = NULL;
			}
		} else if (!allow_cur && hp == histptr) {
			bi_errorf("%s: invalid range", str);
			hp = NULL;
		}
	} else {
		int anchored = *str == '?' ? (++str, 0) : 1;

		/* the -1 is to avoid the current fc command */
		n = findhist(histptr - history - 1, 0, str, anchored);
		if (n < 0) {
			bi_errorf("%s: not in history", str);
			hp = NULL;
		} else
			hp = &history[n];
	}
	return hp;
}

/* Return a pointer to the newest command in the history */
char **
hist_get_newest(int allow_cur)
{
	if (histptr < history || (!allow_cur && histptr == history)) {
		bi_errorf("no history (yet)");
		return NULL;
	}
	if (allow_cur)
		return histptr;
	return histptr - 1;
}

/* Return a pointer to the oldest command in the history */
static char **
hist_get_oldest(void)
{
	if (histptr <= history) {
		bi_errorf("no history (yet)");
		return NULL;
	}
	return history;
}

/******************************/
/* Back up over last histsave */
/******************************/
static void
histbackup(void)
{
	static int last_line = -1;

	if (histptr >= history && last_line != hist_source->line) {
		hist_source->line--;
		afree(*histptr, APERM);
		histptr--;
		last_line = hist_source->line;
	}
}

/*
 * Return the current position.
 */
char **
histpos(void)
{
	return current;
}

int
histnum(int n)
{
	int	last = histptr - history;

	if (n < 0 || n >= last) {
		current = histptr;
		return last;
	} else {
		current = &history[n];
		return n;
	}
}

/*
 * This will become unnecessary if hist_get is modified to allow
 * searching from positions other than the end, and in either
 * direction.
 */
int
findhist(int start, int fwd, const char *str, int anchored)
{
	char	**hp;
	int	maxhist = histptr - history;
	int	incr = fwd ? 1 : -1;
	int	len = strlen(str);

	if (start < 0 || start >= maxhist)
		start = maxhist;

	hp = &history[start];
	for (; hp >= history && hp <= histptr; hp += incr)
		if ((anchored && strncmp(*hp, str, len) == 0) ||
		    (!anchored && strstr(*hp, str)))
			return hp - history;

	return -1;
}

int
findhistrel(const char *str)
{
	int	maxhist = histptr - history;
	int	start = maxhist - 1;
	int	rec = atoi(str);

	if (rec == 0)
		return -1;
	if (rec > 0) {
		if (rec > maxhist)
			return -1;
		return rec - 1;
	}
	if (rec > maxhist)
		return -1;
	return start + rec + 1;
}

/*
 *	set history
 *	this means reallocating the dataspace
 */
void
sethistsize(int n)
{
	if (n > 0 && n != histsize) {
		int cursize = histptr - history;

		/* save most recent history */
		if (n < cursize) {
			memmove(history, histptr - n, n * sizeof(char *));
			cursize = n;
		}

		history = areallocarray(history, n, sizeof(char *), APERM);

		histsize = n;
		histptr = history + cursize;
	}
}

/*
 *	set history file
 *	This can mean reloading/resetting/starting history file
 *	maintenance
 */
void
sethistfile(const char *name)
{
	/* if not started then nothing to do */
	if (hstarted == 0)
		return;

	/* if the name is the same as the name we have */
	if (hname && strcmp(hname, name) == 0)
		return;
	/*
	 * its a new name - possibly
	 */
	if (hname) {
		afree(hname, APERM);
		hname = NULL;
		/* let's reset the history */
		histptr = history - 1;
		hist_source->line = 0;
	}

	history_close();
	hist_init(hist_source);
}

/*
 *	initialise the history vector
 */
void
init_histvec(void)
{
	if (history == NULL) {
		histsize = HISTORYSIZE;
		history = areallocarray(NULL, histsize, sizeof(char *), APERM);
		histptr = history - 1;
	}
}

static void
history_lock(int operation)
{
	while (flock(fileno(histfh), operation) != 0) {
		if (errno == EINTR || errno == EAGAIN)
			continue;
		else
			break;
	}
}

/*
 *	Routines added by Peter Collinson BSDI(Europe)/Hillside Systems to
 *	a) permit HISTSIZE to control number of lines of history stored
 *	b) maintain a physical history file
 *
 *	It turns out that there is a lot of ghastly hackery here
 */


/*
 * save command in history
 */
void
histsave(int lno, const char *cmd, int dowrite)
{
	struct stat	sb;
	char		**hp, *c, *cp, *encoded;

	if (dowrite && histfh) {
		history_lock(LOCK_EX);
		if (fstat(fileno(histfh), &sb) != -1) {
			if (timespeccmp(&sb.st_mtim, &last_sb.st_mtim, ==))
				; /* file is unchanged */
			else {
				/* reset history */
				histptr = history - 1;
				hist_source->line = 0;
				history_load(hist_source);
			}
		}
	}

	c = str_save(cmd, APERM);
	if ((cp = strrchr(c, '\n')) != NULL)
		*cp = '\0';

	hp = histptr;
	if (++hp >= history + histsize) { /* remove oldest command */
		afree(*history, APERM);
		for (hp = history; hp < history + histsize - 1; hp++)
			hp[0] = hp[1];
	}
	*hp = c;
	histptr = hp;

	if (dowrite && histfh) {
		/* append to file */
		if (fseeko(histfh, 0, SEEK_END) == 0 &&
		    stravis(&encoded, c, VIS_SAFE | VIS_NL) != -1) {
			fprintf(histfh, "%s\n", encoded);
			fflush(histfh);
			fstat(fileno(histfh), &last_sb);
			line_co++;
			history_write();
			free(encoded);
		}
		history_lock(LOCK_UN);
	}
}

static FILE *
history_open(void)
{
	struct stat	sb;
	FILE		*f;
	int		fd, fddup;

	if ((fd = open(hname, O_RDWR | O_CREAT | O_EXLOCK, 0600)) == -1)
		return NULL;
	if (fstat(fd, &sb) == -1 || sb.st_uid != getuid()) {
		close(fd);
		return NULL;
	}
	fddup = savefd(fd);
	if (fddup != fd)
		close(fd);

	if ((f = fdopen(fddup, "r+")) == NULL)
		close(fddup);
	else
		last_sb = sb;

	return f;
}

static void
history_close(void)
{
	if (histfh) {
		fflush(histfh);
		fclose(histfh);
		histfh = NULL;
	}
}

static int
history_load(Source *s)
{
	char		*p, encoded[LINE + 1], line[LINE + 1];

	rewind(histfh);

	/* just read it all; will auto resize history upon next command */
	for (line_co = 1; ; line_co++) {
		p = fgets(encoded, sizeof(encoded), histfh);
		if (p == NULL || feof(histfh) || ferror(histfh))
			break;
		if ((p = strchr(encoded, '\n')) == NULL) {
			bi_errorf("history file is corrupt");
			return 1;
		}
		*p = '\0';
		s->line = line_co;
		s->cmd_offset = line_co;
		strunvis(line, encoded);
		histsave(line_co, line, 0);
	}

	history_write();

	return 0;
}

void
hist_init(Source *s)
{
	if (Flag(FTALKING) == 0)
		return;

	hstarted = 1;

	hist_source = s;

	hname = str_val(global("HISTFILE"));
	if (hname == NULL)
		return;
	hname = str_save(hname, APERM);
	histfh = history_open();
	if (histfh == NULL)
		return;

	history_load(s);

	history_lock(LOCK_UN);
}

static void
history_write(void)
{
	char		*cmd, *encoded;
	int		i;

	/* see if file has grown over 25% */
	if (line_co < histsize + (histsize / 4))
		return;

	/* rewrite the whole caboodle */
	rewind(histfh);
	if (ftruncate(fileno(histfh), 0) == -1) {
		bi_errorf("failed to rewrite history file - %s",
		    strerror(errno));
	}
	for (i = 0; i < histsize; i++) {
		cmd = history[i];
		if (cmd == NULL)
			break;

		if (stravis(&encoded, cmd, VIS_SAFE | VIS_NL) != -1) {
			if (fprintf(histfh, "%s\n", encoded) == -1) {
				free(encoded);
				return;
			}
			free(encoded);
		}
	}

	line_co = histsize;

	fflush(histfh);
	fstat(fileno(histfh), &last_sb);
}

void
hist_finish(void)
{
	history_close();
}

#else /* HISTORY */

/* No history to be compiled in: dummy routines to avoid lots more ifdefs */
void
init_histvec(void)
{
}
void
hist_init(Source *s)
{
}
void
hist_finish(void)
{
}
void
histsave(int lno, const char *cmd, int dowrite)
{
	errorf("history not enabled");
}
#endif /* HISTORY */
