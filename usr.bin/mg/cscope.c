/*	$OpenBSD: cscope.c,v 1.7 2014/11/16 00:59:25 guenther Exp $	*/

/*
 * This file is in the public domain.
 *
 * Author: Sunil Nimmagadda <sunil@sunilnimmagadda.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>

#include <ctype.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "def.h"

#define CSSYMBOL      0
#define CSDEFINITION  1
#define CSCALLEDFUNCS 2
#define CSCALLERFUNCS 3
#define CSTEXT        4
#define CSEGREP       6
#define CSFINDFILE    7
#define CSINCLUDES    8

struct cstokens {
	const char *fname;
	const char *function;
	const char *lineno;
	const char *pattern;
};

struct csmatch {
	TAILQ_ENTRY(csmatch) entry;
	int lineno;
};

struct csrecord {
	TAILQ_ENTRY(csrecord) entry;
	char *filename;
	TAILQ_HEAD(matches, csmatch) matches;
};

static TAILQ_HEAD(csrecords, csrecord) csrecords = TAILQ_HEAD_INITIALIZER(csrecords);
static struct csrecord *addentryr;
static struct csrecord *currecord;
static struct csmatch  *curmatch;
static const char      *addentryfn;
static const char      *csprompt[] = {
	"Find this symbol: ",
	"Find this global definition: ",
	"Find functions called by this function: ",
	"Find functions calling this function: ",
	"Find this text string: ",
	"Change this text string: ",
	"Find this egrep pattern: ",
	"Find this file: ",
	"Find files #including this file: "
};

static int  addentry(struct buffer *, char *);
static void csflush(void);
static int  do_cscope(int);
static int  csexists(const char *);
static int  getattr(char *, struct cstokens *);
static int  jumptomatch(void);
static void prettyprint(struct buffer *, struct cstokens *);
static const char *ltrim(const char *);

/*
 * Find this symbol. Bound to C-c s s
 */
/* ARGSUSED */
int
cssymbol(int f, int n)
{
	return (do_cscope(CSSYMBOL));
}

/*
 * Find this global definition. Bound to C-c s d
 */
/* ARGSUSED */int
csdefinition(int f, int n)
{
	return (do_cscope(CSDEFINITION));
}

/*
 * Find functions called by this function. Bound to C-c s l
 */
/* ARGSUSED */
int
csfuncalled(int f, int n)
{
	return (do_cscope(CSCALLEDFUNCS));
}

/*
 * Find functions calling this function. Bound to C-c s c
 */
/* ARGSUSED */
int
cscallerfuncs(int f, int n)
{
	return (do_cscope(CSCALLERFUNCS));
}

/*
 * Find this text. Bound to C-c s t
 */
/* ARGSUSED */
int
csfindtext(int f, int n)
{
	return (do_cscope(CSTEXT));
}

/*
 * Find this egrep pattern. Bound to C-c s e
 */
/* ARGSUSED */
int
csegrep(int f, int n)
{
	return (do_cscope(CSEGREP));
}

/*
 * Find this file. Bound to C-c s f
 */
/* ARGSUSED */
int
csfindfile(int f, int n)
{
	return (do_cscope(CSFINDFILE));
}

/*
 * Find files #including this file. Bound to C-c s i
 */
/* ARGSUSED */
int
csfindinc(int f, int n)
{
	return (do_cscope(CSINCLUDES));
}

/*
 * Create list of files to index in the given directory
 * using cscope-indexer.
 */
/* ARGSUSED */
int
cscreatelist(int f, int n)
{
	struct buffer *bp;
	struct stat sb;
	FILE *fpipe;
	char dir[NFILEN], cmd[BUFSIZ], title[BUFSIZ], *line, *bufp;
	size_t len;
	int clen;
	
	if (getbufcwd(dir, sizeof(dir)) == FALSE)
		dir[0] = '\0';
	
	bufp = eread("Index files in directory: ", dir, 
	    sizeof(dir), EFCR | EFDEF | EFNEW | EFNUL);
	
	if (bufp == NULL)
		return (ABORT);
	else if (bufp[0] == '\0')
		return (FALSE);
		
	if (stat(dir, &sb) == -1) {
		dobeep();
		ewprintf("stat: %s", strerror(errno));
		return (FALSE);
	} else if (S_ISDIR(sb.st_mode) == 0) {
		dobeep();
		ewprintf("%s: Not a directory", dir);
		return (FALSE);
	}
	
	if (csexists("cscope-indexer") == FALSE) {
		dobeep();
		ewprintf("no such file or directory, cscope-indexer");
		return (FALSE);
	}
	
	clen = snprintf(cmd, sizeof(cmd), "cscope-indexer -v %s", dir);
	if (clen < 0 || clen >= sizeof(cmd))
		return (FALSE);

	if ((fpipe = popen(cmd, "r")) == NULL) {
		dobeep();
		ewprintf("problem opening pipe");
		return (FALSE);
	}
	
	bp = bfind("*cscope*", TRUE);
	if (bclear(bp) != TRUE) {
		pclose(fpipe);
		return (FALSE);
	}
	bp->b_flag |= BFREADONLY;

	clen = snprintf(title, sizeof(title), "%s%s",
	    "Creating cscope file list 'cscope.files' in: ", dir);
	if (clen < 0 || clen >= sizeof(title)) {
		pclose(fpipe);
		return (FALSE);
	}
	addline(bp, title);
	addline(bp, "");
	/* All lines are NUL terminated */
	while ((line = fgetln(fpipe, &len)) != NULL) {
		line[len - 1] = '\0';
		addline(bp, line);
	}
	pclose(fpipe);
	return (popbuftop(bp, WNONE));	
}

/*
 * Next Symbol. Bound to C-c s n
 */
/* ARGSUSED */
int
csnextmatch(int f, int n)
{
	struct csrecord *r;
	struct csmatch *m;
	
	if (curmatch == NULL) {
		if ((r = TAILQ_FIRST(&csrecords)) == NULL) {
			dobeep();
			ewprintf("The *cscope* buffer does not exist yet");
			return (FALSE);
		}
		currecord = r;
		curmatch = TAILQ_FIRST(&r->matches);
	} else {
		m = TAILQ_NEXT(curmatch, entry);
		if (m == NULL) {
			r = TAILQ_NEXT(currecord, entry);
			if (r == NULL) {
				dobeep();
				ewprintf("The end of *cscope* buffer has been"
				    " reached");
				return (FALSE);
			} else {
				currecord = r;
				curmatch = TAILQ_FIRST(&currecord->matches);
			}
		} else
			curmatch = m;
	}
	return (jumptomatch());
}

/*
 * Previous Symbol. Bound to C-c s p
 */
/* ARGSUSED */
int
csprevmatch(int f, int n)
{
	struct csmatch *m;
	struct csrecord *r;

	if (curmatch == NULL)
		return (FALSE);
	else {
		m  = TAILQ_PREV(curmatch, matches, entry);
		if (m)
			curmatch = m;
		else {
			r = TAILQ_PREV(currecord, csrecords, entry);
			if (r == NULL) {
				dobeep();
				ewprintf("The beginning of *cscope* buffer has"
				    " been reached");
				return (FALSE);
			} else {
				currecord = r;
				curmatch = TAILQ_LAST(&currecord->matches,
				    matches);
			}
		}
	}
	return (jumptomatch());
}

/*
 * Next file.
 */
int
csnextfile(int f, int n)
{
	struct csrecord *r;
	
	if (curmatch == NULL) {
		if ((r = TAILQ_FIRST(&csrecords)) == NULL) {
			dobeep();
			ewprintf("The *cscope* buffer does not exist yet");
			return (FALSE);
		}

	} else {
		if ((r = TAILQ_NEXT(currecord, entry)) == NULL) {
			dobeep();
			ewprintf("The end of *cscope* buffer has been reached");
			return (FALSE);
		}
	}
	currecord = r;
	curmatch = TAILQ_FIRST(&currecord->matches);
	return (jumptomatch());	
}

/*
 * Previous file.
 */
int
csprevfile(int f, int n)
{
	struct csrecord *r;
	
	if (curmatch == NULL) {
		if ((r = TAILQ_FIRST(&csrecords)) == NULL) {
			dobeep();
			ewprintf("The *cscope* buffer does not exist yet");
			return (FALSE);
		}

	} else {
		if ((r = TAILQ_PREV(currecord, csrecords, entry)) == NULL) {
			dobeep();
			ewprintf("The beginning of *cscope* buffer has been"
			    " reached");
			return (FALSE);
		}
	}
	currecord = r;
	curmatch = TAILQ_FIRST(&currecord->matches);
	return (jumptomatch());	
}

/*
 * The current symbol location is extracted from currecord->filename and 
 * curmatch->lineno. Load the file similar to filevisit and goto the 
 * lineno recorded.
 */
int
jumptomatch(void)
{
	struct buffer *bp;
	char *adjf;
	
	if (curmatch == NULL || currecord == NULL)
		return (FALSE);
	adjf = adjustname(currecord->filename, TRUE);
	if (adjf == NULL)
		return (FALSE);
	if ((bp = findbuffer(adjf)) == NULL)
		return (FALSE);
	curbp = bp;
	if (showbuffer(bp, curwp, WFFULL) != TRUE)
		return (FALSE);
	if (bp->b_fname[0] == '\0') {
		if (readin(adjf) != TRUE)
			killbuffer(bp);
	}
	gotoline(FFARG, curmatch->lineno);
	return (TRUE);
	
}

/*
 * Ask for the symbol, construct cscope commandline with the symbol
 * and passed in index. Popen cscope, read the output into *cscope* 
 * buffer and pop it.
 */
int
do_cscope(int i)
{
	struct buffer *bp;
	FILE *fpipe;
	char pattern[MAX_TOKEN], cmd[BUFSIZ], title[BUFSIZ];
	char *p, *buf;
	int clen, nores = 0;
	size_t len;

	/* If current buffer isn't a source file just return */
	if (fnmatch("*.[chy]", curbp->b_fname, 0) != 0) {
		dobeep();
		ewprintf("C-c s not defined");
		return (FALSE);
	}
	
	if (curtoken(0, 1, pattern) == FALSE)
		return (FALSE);	
	p = eread(csprompt[i], pattern, MAX_TOKEN, EFNEW | EFCR | EFDEF);
	if (p == NULL)
		return (ABORT);
	else if (p[0] == '\0')
		return (FALSE);

	if (csexists("cscope") == FALSE) {
		dobeep();
		ewprintf("no such file or directory, cscope");
		return (FALSE);
	}
	
	csflush();
	clen = snprintf(cmd, sizeof(cmd), "cscope -L -%d %s 2>/dev/null",
	    i, pattern);
	if (clen < 0 || clen >= sizeof(cmd))
		return (FALSE);

	if ((fpipe = popen(cmd, "r")) == NULL) {
		dobeep();
		ewprintf("problem opening pipe");
		return (FALSE);
	}
	
	bp = bfind("*cscope*", TRUE);
	if (bclear(bp) != TRUE) {
		pclose(fpipe);
		return (FALSE);
	}
	bp->b_flag |= BFREADONLY;

	clen = snprintf(title, sizeof(title), "%s%s", csprompt[i], pattern);
	if (clen < 0 || clen >= sizeof(title)) {
		pclose(fpipe);
		return (FALSE);
	}
	addline(bp, title);
	addline(bp, "");
	addline(bp, "-------------------------------------------------------------------------------");
	/* All lines are NUL terminated */
	while ((buf = fgetln(fpipe, &len)) != NULL) {
		buf[len - 1] = '\0';
		if (addentry(bp, buf) != TRUE)
			return (FALSE);
		nores = 1;
	}; 
	pclose(fpipe);
	addline(bp, "-------------------------------------------------------------------------------");
	if (nores == 0)
		ewprintf("No matches were found.");
	return (popbuftop(bp, WNONE));
}

/*
 * For each line read from cscope output, extract the tokens,
 * add them to list and pretty print a line in *cscope* buffer.
 */
int
addentry(struct buffer *bp, char *csline)
{
	struct csrecord *r;
	struct csmatch *m;
	struct cstokens t;
	int lineno;
	char buf[BUFSIZ];
	const char *errstr;

	r = NULL;
	if (getattr(csline, &t) == FALSE)
		return (FALSE);

	lineno = strtonum(t.lineno, INT_MIN, INT_MAX, &errstr);
	if (errstr)
		return (FALSE);
		
	if (addentryfn == NULL || strcmp(addentryfn, t.fname) != 0) {
		if ((r = malloc(sizeof(struct csrecord))) == NULL)
			return (FALSE);
		addentryr = r;
		if ((r->filename = strndup(t.fname, NFILEN)) == NULL)
			goto cleanup;
		addentryfn = r->filename;
		TAILQ_INIT(&r->matches);
		if ((m = malloc(sizeof(struct csmatch))) == NULL)
			goto cleanup;
		m->lineno = lineno;
		TAILQ_INSERT_TAIL(&r->matches, m, entry);
		TAILQ_INSERT_TAIL(&csrecords, r, entry);
		addline(bp, "");
		if (snprintf(buf, sizeof(buf), "*** %s", t.fname) < 0)
			goto cleanup;
		addline(bp, buf);
	} else {
		if ((m = malloc(sizeof(struct csmatch))) == NULL)
			goto cleanup;
		m->lineno = lineno;
		TAILQ_INSERT_TAIL(&addentryr->matches, m, entry);
	}
	prettyprint(bp, &t);
	return (TRUE);
cleanup:
	free(r);
	return (FALSE);
}

/*
 * Cscope line: <filename> <function> <lineno> <pattern>
 */
int
getattr(char *line, struct cstokens *t)
{
	char *p;

	if ((p = strchr(line, ' ')) == NULL)
		return (FALSE);
	*p++ = '\0';
	t->fname = line;
	line = p;

	if ((p = strchr(line, ' ')) == NULL)
		return (FALSE);
	*p++ = '\0';
	t->function = line;
	line = p;

	if ((p = strchr(line, ' ')) == NULL)
		return (FALSE);
	*p++ = '\0';
	t->lineno = line;

	if (*p == '\0')
		return (FALSE);
	t->pattern = p;

	return (TRUE);
}

void
prettyprint(struct buffer *bp, struct cstokens *t)
{
	char buf[BUFSIZ];

	if (snprintf(buf, sizeof(buf), "%s[%s]\t\t%s",
	    t->function, t->lineno, ltrim(t->pattern)) < 0)
		return;
	addline(bp, buf);
}

const char *
ltrim(const char *s)
{
	while (isblank((unsigned char)*s))
		s++;
	return s;
}

void
csflush(void)
{
	struct csrecord *r;
	struct csmatch *m;
	
	while ((r = TAILQ_FIRST(&csrecords)) != NULL) {
		free(r->filename);
		while ((m = TAILQ_FIRST(&r->matches)) != NULL) {
			TAILQ_REMOVE(&r->matches, m, entry);
			free(m);
		}
		TAILQ_REMOVE(&csrecords, r, entry);
		free(r);
	}
	addentryr = NULL;
	addentryfn = NULL;
	currecord = NULL;
	curmatch = NULL;
}

/*
 * Check if the cmd exists in $PATH. Split on ":" and iterate through
 * all paths in $PATH.
 */
int
csexists(const char *cmd)
{
       char fname[NFILEN], *dir, *path, *pathc, *tmp;
       int  cmdlen, dlen;

       /* Special case if prog contains '/' */
       if (strchr(cmd, '/')) {
               if (access(cmd, F_OK) == -1)
                       return (FALSE);
               else
                       return (TRUE);
       }
       if ((tmp = getenv("PATH")) == NULL)
               return (FALSE);
       if ((pathc = path = strndup(tmp, NFILEN)) == NULL) {
               dobeep();
               ewprintf("out of memory");
               return (FALSE);
       }
       cmdlen = strlen(cmd);
       while ((dir = strsep(&path, ":")) != NULL) {
               if (*dir == '\0')
                       *dir = '.';

               dlen = strlen(dir);
               while (dir[dlen-1] == '/')
                       dir[--dlen] = '\0';     /* strip trailing '/' */

               if (dlen + 1 + cmdlen >= sizeof(fname))  {
                       dobeep();
                       ewprintf("path too long");
                       goto cleanup;
               }
               snprintf(fname, sizeof(fname), "%s/%s", dir, cmd);
               if(access(fname, F_OK) == 0) {
		       free(pathc);
		       return (TRUE);
	       }
       }
cleanup:
	free(pathc);
	return (FALSE);
}
