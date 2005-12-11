/*	$OpenBSD: history.c,v 1.32 2005/12/11 18:53:51 deraadt Exp $	*/

/*
 * command history
 *
 * only implements in-memory history.
 */

/*
 *	This file contains
 *	a)	the original in-memory history  mechanism
 *	b)	a more complicated mechanism done by  pc@hillside.co.uk
 *		that more closely follows the real ksh way of doing
 *		things. You need to have the mmap system call for this
 *		to work on your system
 */

#include "sh.h"
#include <sys/stat.h>

#ifdef HISTORY
# include <sys/file.h>
# include <sys/mman.h>

/*
 *	variables for handling the data file
 */
static int	histfd;
static int	hsize;

static int hist_count_lines(unsigned char *, int);
static int hist_shrink(unsigned char *, int);
static unsigned char *hist_skip_back(unsigned char *,int *,int);
static void histload(Source *, unsigned char *, int);
static void histinsert(Source *, int, unsigned char *);
static void writehistfile(int, char *);
static int sprinkle(int);

static int	hist_execute(char *);
static int	hist_replace(char **, const char *, const char *, int);
static char   **hist_get(const char *, int, int);
static char   **hist_get_oldest(void);
static void	histbackup(void);

static char   **current;	/* current position in history[] */
static char    *hname;		/* current name of history file */
static int	hstarted;	/* set after hist_init() called */
static Source	*hist_source;


int
c_fc(char **wp)
{
	struct shf *shf;
	struct temp *tf = NULL;
	char *p, *editor = (char *) 0;
	int gflag = 0, lflag = 0, nflag = 0, sflag = 0, rflag = 0;
	int optc;
	char *first = (char *) 0, *last = (char *) 0;
	char **hfirst, **hlast, **hp;

	if (!Flag(FTALKING_I)) {
		bi_errorf("history functions not available");
		return 1;
	}

	while ((optc = ksh_getopt(wp, &builtin_opt,
	    "e:glnrs0,1,2,3,4,5,6,7,8,9,")) != EOF)
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
		char *pat = (char *) 0, *rep = (char *) 0;

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
		return hist_replace(hp, pat, rep, gflag);
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

	tf = maketemp(ATEMP, TT_HIST_EDIT, &e->temps);
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
		int ret;

		ret = command(editor ? editor : "${FCEDIT:-/bin/ed} $_");
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

		n = fstat(shf_fileno(shf), &statb) < 0 ? 128 :
		    statb.st_size + 1;
		Xinit(xs, xp, n, hist_source->areap);
		while ((n = shf_read(xp, Xnleft(xs, xp), shf)) > 0) {
			xp += n;
			if (Xnleft(xs, xp) <= 0)
				XcheckN(xs, xp, Xlength(xs, xp));
		}
		if (n < 0) {
			bi_errorf("error reading temp file %s - %s",
			    tf->name, strerror(shf_errno(shf)));
			shf_close(shf);
			return 1;
		}
		shf_close(shf);
		*xp = '\0';
		strip_nuls(Xstring(xs, xp), Xlength(xs, xp));
		return hist_execute(Xstring(xs, xp));
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
				q = (char *) 0;
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
	ret = command(cmd);
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
	char **hp = (char **) 0;
	int n;

	if (getn(str, &n)) {
		hp = histptr + (n < 0 ? n : (n - hist_source->line));
		if (hp < history) {
			if (approx)
				hp = hist_get_oldest();
			else {
				bi_errorf("%s: not in history", str);
				hp = (char **) 0;
			}
		} else if (hp > histptr) {
			if (approx)
				hp = hist_get_newest(allow_cur);
			else {
				bi_errorf("%s: not in history", str);
				hp = (char **) 0;
			}
		} else if (!allow_cur && hp == histptr) {
			bi_errorf("%s: invalid range", str);
			hp = (char **) 0;
		}
	} else {
		int anchored = *str == '?' ? (++str, 0) : 1;

		/* the -1 is to avoid the current fc command */
		n = findhist(histptr - history - 1, 0, str, anchored);
		if (n < 0) {
			bi_errorf("%s: not in history", str);
			hp = (char **) 0;
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
		return (char **) 0;
	}
	if (allow_cur)
		return histptr;
	return histptr - 1;
}

/* Return a pointer to the newest command in the history */
static char **
hist_get_oldest(void)
{
	if (histptr <= history) {
		bi_errorf("no history (yet)");
		return (char **) 0;
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
		afree((void*)*histptr, APERM);
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

		history = (char **)aresize(history, n*sizeof(char *), APERM);

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
	if (histfd) {
		/* yes the file is open */
		(void) close(histfd);
		histfd = 0;
		hsize = 0;
		afree(hname, APERM);
		hname = NULL;
		/* let's reset the history */
		histptr = history - 1;
		hist_source->line = 0;
	}

	hist_init(hist_source);
}

/*
 *	initialise the history vector
 */
void
init_histvec(void)
{
	if (history == (char **)NULL) {
		histsize = HISTORYSIZE;
		history = (char **)alloc(histsize*sizeof (char *), APERM);
		histptr = history - 1;
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
	char **hp;
	char *c, *cp;

	c = str_save(cmd, APERM);
	if ((cp = strchr(c, '\n')) != NULL)
		*cp = '\0';

	if (histfd && dowrite)
		writehistfile(lno, c);

	hp = histptr;

	if (++hp >= history + histsize) { /* remove oldest command */
		afree((void*)*history, APERM);
		for (hp = history; hp < history + histsize - 1; hp++)
			hp[0] = hp[1];
	}
	*hp = c;
	histptr = hp;
}

/*
 *	Write history data to a file nominated by HISTFILE
 *	if HISTFILE is unset then history still happens, but
 *	the data is not written to a file
 *	All copies of ksh looking at the file will maintain the
 *	same history. This is ksh behaviour.
 *
 *	This stuff uses mmap()
 *	if your system ain't got it - then you'll have to undef HISTORYFILE
 */

/*
 *	Open a history file
 *	Format is:
 *	Bytes 1, 2: HMAGIC - just to check that we are dealing with
 *		    the correct object
 *	Then follows a number of stored commands
 *	Each command is
 *	<command byte><command number(4 bytes)><bytes><null>
 */
#define HMAGIC1		0xab
#define HMAGIC2		0xcd
#define COMMAND		0xff

void
hist_init(Source *s)
{
	unsigned char	*base;
	int	lines;
	int	fd;

	if (Flag(FTALKING) == 0)
		return;

	hstarted = 1;

	hist_source = s;

	hname = str_val(global("HISTFILE"));
	if (hname == NULL)
		return;
	hname = str_save(hname, APERM);

  retry:
	/* we have a file and are interactive */
	if ((fd = open(hname, O_RDWR|O_CREAT|O_APPEND, 0600)) < 0)
		return;

	histfd = savefd(fd, 0);

	(void) flock(histfd, LOCK_EX);

	hsize = lseek(histfd, 0L, SEEK_END);

	if (hsize == 0) {
		/* add magic */
		if (sprinkle(histfd)) {
			hist_finish();
			return;
		}
	}
	else if (hsize > 0) {
		/*
		 * we have some data
		 */
		base = (unsigned char *)mmap(0, hsize, PROT_READ,
		    MAP_FILE|MAP_PRIVATE, histfd, 0);
		/*
		 * check on its validity
		 */
		if (base == MAP_FAILED || *base != HMAGIC1 || base[1] != HMAGIC2) {
			if (base != MAP_FAILED)
				munmap((caddr_t)base, hsize);
			hist_finish();
			unlink(hname);
			goto retry;
		}
		if (hsize > 2) {
			lines = hist_count_lines(base+2, hsize-2);
			if (lines > histsize) {
				/* we need to make the file smaller */
				if (hist_shrink(base, hsize))
					unlink(hname);
				munmap((caddr_t)base, hsize);
				hist_finish();
				goto retry;
			}
		}
		histload(hist_source, base+2, hsize-2);
		munmap((caddr_t)base, hsize);
	}
	(void) flock(histfd, LOCK_UN);
	hsize = lseek(histfd, 0L, SEEK_END);
}

typedef enum state {
	shdr,		/* expecting a header */
	sline,		/* looking for a null byte to end the line */
	sn1,		/* bytes 1 to 4 of a line no */
	sn2, sn3, sn4
} State;

static int
hist_count_lines(unsigned char *base, int bytes)
{
	State state = shdr;
	int lines = 0;

	while (bytes--) {
		switch (state) {
		case shdr:
			if (*base == COMMAND)
				state = sn1;
			break;
		case sn1:
			state = sn2; break;
		case sn2:
			state = sn3; break;
		case sn3:
			state = sn4; break;
		case sn4:
			state = sline; break;
		case sline:
			if (*base == '\0')
				lines++, state = shdr;
		}
		base++;
	}
	return lines;
}

/*
 *	Shrink the history file to histsize lines
 */
static int
hist_shrink(unsigned char *oldbase, int oldbytes)
{
	int fd;
	char	nfile[1024];
	struct	stat statb;
	unsigned char *nbase = oldbase;
	int nbytes = oldbytes;

	nbase = hist_skip_back(nbase, &nbytes, histsize);
	if (nbase == NULL)
		return 1;
	if (nbase == oldbase)
		return 0;

	/*
	 *	create temp file
	 */
	(void) shf_snprintf(nfile, sizeof(nfile), "%s.%d", hname, procpid);
	if ((fd = open(nfile, O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0)
		return 1;

	if (sprinkle(fd)) {
		close(fd);
		unlink(nfile);
		return 1;
	}
	if (write(fd, nbase, nbytes) != nbytes) {
		close(fd);
		unlink(nfile);
		return 1;
	}
	/*
	 *	worry about who owns this file
	 */
	if (fstat(histfd, &statb) >= 0)
		fchown(fd, statb.st_uid, statb.st_gid);
	close(fd);

	/*
	 *	rename
	 */
	if (rename(nfile, hname) < 0)
		return 1;
	return 0;
}


/*
 *	find a pointer to the data `no' back from the end of the file
 *	return the pointer and the number of bytes left
 */
static unsigned char *
hist_skip_back(unsigned char *base, int *bytes, int no)
{
	int lines = 0;
	unsigned char *ep;

	for (ep = base + *bytes; --ep > base; ) {
		/* this doesn't really work: the 4 byte line number that is
		 * encoded after the COMMAND byte can itself contain the
		 * COMMAND byte....
		 */
		for (; ep > base && *ep != COMMAND; ep--)
			;
		if (ep == base)
			break;
		if (++lines == no) {
			*bytes = *bytes - ((char *)ep - (char *)base);
			return ep;
		}
	}
	return NULL;
}

/*
 *	load the history structure from the stored data
 */
static void
histload(Source *s, unsigned char *base, int bytes)
{
	State state;
	int	lno = 0;
	unsigned char	*line = NULL;

	for (state = shdr; bytes-- > 0; base++) {
		switch (state) {
		case shdr:
			if (*base == COMMAND)
				state = sn1;
			break;
		case sn1:
			lno = (((*base)&0xff)<<24);
			state = sn2;
			break;
		case sn2:
			lno |= (((*base)&0xff)<<16);
			state = sn3;
			break;
		case sn3:
			lno |= (((*base)&0xff)<<8);
			state = sn4;
			break;
		case sn4:
			lno |= (*base)&0xff;
			line = base+1;
			state = sline;
			break;
		case sline:
			if (*base == '\0') {
				/* worry about line numbers */
				if (histptr >= history && lno-1 != s->line) {
					/* a replacement ? */
					histinsert(s, lno, line);
				}
				else {
					s->line = lno;
					histsave(lno, (char *)line, 0);
				}
				state = shdr;
			}
		}
	}
}

/*
 *	Insert a line into the history at a specified number
 */
static void
histinsert(Source *s, int lno, unsigned char *line)
{
	char **hp;

	if (lno >= s->line-(histptr-history) && lno <= s->line) {
		hp = &histptr[lno-s->line];
		if (*hp)
			afree((void*)*hp, APERM);
		*hp = str_save((char *)line, APERM);
	}
}

/*
 *	write a command to the end of the history file
 *	This *MAY* seem easy but it's also necessary to check
 *	that the history file has not changed in size.
 *	If it has - then some other shell has written to it
 *	and we should read those commands to update our history
 */
static void
writehistfile(int lno, char *cmd)
{
	int	sizenow;
	unsigned char	*base;
	unsigned char	*new;
	int	bytes;
	unsigned char	hdr[5];

	(void) flock(histfd, LOCK_EX);
	sizenow = lseek(histfd, 0L, SEEK_END);
	if (sizenow != hsize) {
		/*
		 *	Things have changed
		 */
		if (sizenow > hsize) {
			/* someone has added some lines */
			bytes = sizenow - hsize;
			base = (unsigned char *)mmap(0, sizenow,
			    PROT_READ, MAP_FILE|MAP_PRIVATE, histfd, 0);
			if (base == MAP_FAILED)
				goto bad;
			new = base + hsize;
			if (*new != COMMAND) {
				munmap((caddr_t)base, sizenow);
				goto bad;
			}
			hist_source->line--;
			histload(hist_source, new, bytes);
			hist_source->line++;
			lno = hist_source->line;
			munmap((caddr_t)base, sizenow);
			hsize = sizenow;
		} else {
			/* it has shrunk */
			/* but to what? */
			/* we'll give up for now */
			goto bad;
		}
	}
	/*
	 *	we can write our bit now
	 */
	hdr[0] = COMMAND;
	hdr[1] = (lno>>24)&0xff;
	hdr[2] = (lno>>16)&0xff;
	hdr[3] = (lno>>8)&0xff;
	hdr[4] = lno&0xff;
	(void) write(histfd, hdr, 5);
	(void) write(histfd, cmd, strlen(cmd)+1);
	hsize = lseek(histfd, 0L, SEEK_END);
	(void) flock(histfd, LOCK_UN);
	return;
bad:
	hist_finish();
}

void
hist_finish(void)
{
	(void) flock(histfd, LOCK_UN);
	(void) close(histfd);
	histfd = 0;
}

/*
 *	add magic to the history file
 */
static int
sprinkle(int fd)
{
	static unsigned char mag[] = { HMAGIC1, HMAGIC2 };

	return(write(fd, mag, 2) != 2);
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
