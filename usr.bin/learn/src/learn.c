/*
 * learn, from V7 UNIX: one of the earliest Computer Based Training (CBT)
 * programs still in existence.
 *
 * $OpenBSD: learn.c,v 1.2 1998/09/28 16:40:16 ian Exp $
 */

/****************************************************************
Copyright (C) AT&T 1995
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name of AT&T or any of its entities
not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

AT&T DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL AT&T OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.
****************************************************************/

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "learn.h"
#include "pathnames.h"

char	*direct	= _PATH_LLIB;
int	more;
char	*level;
int	speed;
char	*sname;
char	*todo;
FILE	*incopy	= NULL;
int	didok;
int	sequence	= 1;
int	comfile	= -1;
int	status;
int	wrong;
char	*pwline;
char	*playdir;
FILE	*scrin;
int	logging	= 0;	/* set to 0 to turn off logging */
int	ask;

int
main(int argc, char **argv)
{
	extern char * getlogin();
	void hangup(int signum);
	void intrpt(int signum);

	speed = 0;
	more = 1;
	pwline = getlogin();
	setbuf(stdout, malloc(BUFSIZ));
	selsub(argc, argv);
	signal(SIGHUP, hangup);
	signal(SIGINT, intrpt);
	while (more) {
		selunit();
		dounit();
		whatnow();
	}
	wrapup(0);
	return 0;
}

void hangup(int x)
{
	wrapup(1);
}

void intrpt(int x)
{
	char response[20], *p;

	signal(SIGINT, hangup);
	write(2, "\nInterrupt.\nWant to go on?  ", 28);
	p = response;
	*p = 'n';
	while (read(0, p, 1) == 1 && *p != '\n')
		p++;
	if (response[0] != 'y')
		wrapup(1);
	ungetc('\n', stdin);
	signal(SIGINT, intrpt);
}


char last[1024];
char logf[1024];
char subdir[1024];

copy(prompt, fin)
FILE *fin;
{
	FILE *fout, *f;
	char s[200], t[200], s1[200], *r, *tod;
	char nm[100];
	int *p, tv[2];
	extern int *action();
	extern char *wordb();
	int nmatch = 0;

	if (subdir[0]==0)
		snprintf(subdir, sizeof subdir, "%s/%s", _PATH_LLIB, sname);
	for (;;) {
		if (pgets(s, sizeof s, prompt, fin) == 0)
			if (fin == stdin) {
				/* fprintf(stderr, "Don't type control-D\n"); */
				/* this didn't work out very well */
				wrapup(1);	/* ian */
				continue;
			} else
				break;
		trim(s);
		/* change the sequence %s to lesson directory */
		/* if needed */
		for (r = s; *r; r++)
			if (*r == '%') {
				snprintf(s1, sizeof s1, s, subdir, subdir, subdir);
				strcpy(s, s1);
				break;
			}
		r = wordb(s, t);
		p = action(t);
		if (p && *p == ONCE) {	/* some actions done only once per script */
			if (wrong) {	/* we are on 2nd time */
				scopy(fin, NULL);
				continue;
			}
			strcpy(s, r);
			r = wordb(s, t);
			p = action(t);
		}
		if (p == 0) {
			if (comfile >= 0) {
				write(comfile, s, strlen(s));
				write(comfile, "\n", 1);
			}
			else {
				signal(SIGINT, SIG_IGN);
				status = mysys(s);
				signal(SIGINT, intrpt);
			}
			if (incopy) {
				fprintf(incopy, "%s\n", s);
				strcpy(last, s);
			}
			continue;
		}
		switch (*p) {
		case READY:
			if (incopy && r) {
				fprintf(incopy, "%s\n", r);
				strcpy(last, r);
			}
			return;
		case PRINT:
			if (wrong)
				scopy(fin, NULL);	/* don't repeat message */
			else if (r)
				list(r);
			else
				scopy(fin, stdout);
			break;
		case NOP:
			break;
		case MATCH:
			if (nmatch > 0)	/* we have already passed */
				scopy(fin, NULL);
			else if ((status = strcmp(r, last)) == 0) {	/* did we pass this time? */
				nmatch++;
				scopy(fin, stdout);
			} else
				scopy(fin, NULL);
			break;
		case BAD:
			if (strcmp(r, last) == 0) {
				scopy(fin, stdout);
			} else
				scopy(fin, NULL);
			break;
		case SUCCEED:
			scopy(fin, (status == 0) ? stdout : NULL);
			break;
		case FAIL:
			scopy(fin, (status != 0) ? stdout : NULL);
			break;
		case CREATE:
			fout = fopen(r, "w");
			scopy(fin, fout);
			fclose(fout);
			break;
		case CMP:
			status = cmp(r);	/* contains two file names */
			break;
		case MV:
			snprintf(nm, sizeof nm, "%s/L%s.%s", subdir, todo, r);
			fcopy(r, nm);
			break;
		case USER:
		case NEXT:
			more = 1;
			return;
		case COPYIN:
			incopy = fopen(".copy", "w");
			break;
		case UNCOPIN:
			fclose(incopy);
			incopy = NULL;
			break;
		case COPYOUT:
			maktee();
			break;
		case UNCOPOUT:
			untee();
			break;
		case PIPE:
			comfile = makpipe();
			break;
		case UNPIPE:
			close(comfile);
			wait(0);
			comfile = -1;
			break;
		case YES:
		case NO:
			if (incopy) {
				fprintf(incopy, "%s\n", s);
				strcpy(last, s);
			}
			return;
		case WHERE:
			printf("You are in lesson %s\n", todo);
			fflush(stdout);
			break;
		case BYE:
			more=0;
			return;
		case CHDIR:
			printf("cd not allowed\n");
			fflush(stdout);
			break;
		case LEARN:
			printf("You are already in learn.\n");
			fflush(stdout);
			break;
		case LOG:
			if (!logging)
				break;
			if (logf[0] == 0)
				snprintf(logf, sizeof logf, "%s/log/%s", direct, sname);
			f = fopen( (r? r : logf), "a");
			if (f == NULL)
				break;
			time(tv);
			tod = ctime(tv);
			tod[24] = 0;
			fprintf(f, "%s L%-6s %s %2d %s\n", tod,
				todo, status? "fail" : "pass", speed, pwline);
			fclose(f);
			break;
		}
	}
	return;
}

pgets(char *s, int len, int prompt, FILE *f)
{
	if (prompt) {
		if (comfile < 0)
			printf("$ ");
		fflush(stdout);
	}
	if (fgets(s, len, f) != NULL)
		return(1);
	else
		return(0);
}

/** Trim trailing newline */
void
trim(char *s)
{
	while (*s)
		s++;
	if (*--s == '\n')
		*s=0;
}

scopy(fi, fo)	/* copy fi to fo until a line with # */
FILE *fi, *fo;
{
	int c;

	while ((c = getc(fi)) != '#' && c != EOF) {
		do {
			if (fo != NULL)
				putc(c, fo);
			if (c == '\n')
				break;
		} while ((c = getc(fi)) != EOF);
	}
	if (c == '#')
		ungetc(c, fi);
	if (fo != NULL)
		fflush(fo);
}

cmp(r)	/* compare two files for status */
char *r;
{
	char *s;
	FILE *f1, *f2;
	int c1, c2, stat;

	for (s = r; *s != ' ' && *s != '\0'; s++)
		;
	*s++ = 0;	/* r contains file 1 */
	while (*s == ' ')
		s++;
	f1 = fopen(r, "r");
	f2 = fopen(s, "r");
	if (f1 == NULL || f2 == NULL)
		return(1);	/* failure */
	stat = 0;
	for (;;) {
		c1 = getc(f1);
		c2 = getc(f2);
		if (c1 != c2) {
			stat = 1;
			break;
		}
		if (c1 == EOF || c2 == EOF)
			break;
	}
	fclose(f1);
	fclose(f2);
	return(stat);
}

char *
wordb(s, t)	/* in s, t is prefix; return tail */
char *s, *t;
{
	int c;

	while (c = *s++) {
		if (c == ' ' || c == '\t')
			break;
		*t++ = c;
	}
	*t = 0;
	while (*s == ' ' || *s == '\t')
		s++;
	return(c ? s : NULL);
}


dounit()
{
	char tbuff[100];

	if (todo == 0)
		return;
	wrong = 0;
retry:
	start(todo);
	/* script = lesson */
	snprintf(tbuff, sizeof tbuff, "%s/%s/L%s", _PATH_LLIB, sname, todo);
	scrin = fopen(tbuff, "r");
	if (scrin == NULL) {
		fprintf(stderr, "No script.\n");
		err(1, tbuff);
		wrapup(1);
	}

	copy(0, scrin);
	if (more == 0)
		return;
	copy(1, stdin);
	if (more == 0)
		return;
	copy(0, scrin);

	if (comfile >= 0)
		close(comfile);
	wait(&didok);
	didok = (status == 0);
	if (!didok) {
		wrong++;
		printf("\nSorry, that's %snot right.  Do you want to try again?  ",
			wrong > 1 ? "still " : "");
		fflush(stdout);
		for(;;) {
			fgets(tbuff, sizeof tbuff, stdin);
			trim(tbuff);
			if (tbuff[0] == 'y') {
				printf("Try the problem again.\n");
				fflush(stdout);
				goto retry;
			} else if (strcmp(tbuff, "bye") == 0) {
				wrapup(1);
			} else if (tbuff[0] == 'n') {
				wrong = 0;
				printf("\nOK.  Lesson %s (%d)\n", todo, speed);
				printf("Skipping to next lesson.\n\n");
				fflush(stdout);
				break;
			} else {
				printf("Please type yes, no or bye:  ");
				fflush(stdout);
			}
		}
	}
	setdid(todo, sequence++);
}

int istop;

list(r)
char *r;
{
	void stop(int);
	FILE *ft;
	char s[200];

	if (r==0)
		return;
	istop = 1;
	signal(SIGINT, stop);
	ft = fopen(r, "r");
	if (ft != NULL) {
		while (fgets(s, sizeof s, ft) && istop)
			fputs(s, stdout);
		fclose(ft);
	}
	signal(SIGINT, intrpt);
}

void stop(int x)
{
	istop=0;
}

makpipe()
{
	int f[2];

	pipe(f);
	if (fork()==0) {
		close(f[1]);
		close(0);
		dup(f[0]);
		close(f[0]);
		execl ("/bin/sh", "sh", "-i", 0);
		execl ("/usr/bin/sh", "sh", "-i", 0);
		write(2,"Exec error\n",11);
	}
	close(f[0]);
	sleep(2);	/* so shell won't eat up too much input */
	return(f[1]);
}

static int oldout;
static char tee[100];

maktee()
{
	int fpip[2], in, out;

	if (tee[0] == 0)
		snprintf(tee, sizeof tee, "%s/tee", direct);
	pipe(fpip);
	in = fpip[0];
	out= fpip[1];
	if (fork() == 0) {
		signal(SIGINT, SIG_IGN);
		close(0);
		close(out);
		dup(in);
		close(in);
		execl (tee, "lrntee", 0);
		fprintf(stderr, "Tee exec failed\n");
		exit(1);
	}
	close(in);
	fflush(stdout);
	oldout = dup(1);
	close(1);
	if (dup(out) != 1)
		fprintf(stderr, "Error making tee for copyout\n");
	close(out);
	return(1);
}

untee()
{
	int x;

	fflush(stdout);
	close(1);
	dup(oldout);
	close(oldout);
	wait(&x);
}

# define SAME 0

struct keys {
	char *k_wd;
	int k_val;
} keybuff[] = {
	{"ready",	READY},
	{"answer",	READY},
	{"#print",	PRINT},
	{"#copyin",	COPYIN},
	{"#uncopyin",	UNCOPIN},
	{"#copyout",	COPYOUT},
	{"#uncopyout",	UNCOPOUT},
	{"#pipe",	PIPE},
	{"#unpipe",	UNPIPE},
	{"#succeed",	SUCCEED},
	{"#fail",	FAIL},
	{"bye",		BYE},
	{"chdir",	CHDIR},
	{"cd",		CHDIR},
	{"learn",	LEARN},
	{"#log",	LOG},
	{"yes",		YES},
	{"no",		NO},
	{"#mv",		MV},
	{"#user",	USER},
	{"#next",	NEXT},
	{"skip",	SKIP},
	{"#where",	WHERE},
	{"#match",	MATCH},
	{"#bad",	BAD},
	{"#create",	CREATE},
	{"#cmp",	CMP},
	{"#goto",	GOTO},
	{"#once",	ONCE},
	{"#",		NOP},
	{NULL,		0}
};

int *action(s)
char *s;
{
	struct keys *kp;
	for (kp=keybuff; kp->k_wd; kp++)
		if (strcmp(kp->k_wd, s) == SAME)
			return(&(kp->k_val));
	return(NULL);
}

# define NW 100
# define NWCH 800
struct whichdid {
	char *w_less;
	int w_seq;
} which[NW];
int nwh = 0;
char whbuff[NWCH];
char *whcp = whbuff;

setdid(lesson, sequence)
char *lesson;
{
	struct whichdid *pw;
	for(pw=which; pw < which+nwh; pw++)
		if (strcmp(pw->w_less, lesson) == SAME)
			{
			pw->w_seq = sequence;
			return;
			}
	pw=which+nwh++;
	if (nwh >= NW) {
		fprintf(stderr, "nwh>=NW\n");
		wrapup(1);
	}
	pw->w_seq = sequence;
	pw->w_less = whcp;
	while (*whcp++ = *lesson++);
	if (whcp >= whbuff + NWCH) {
		fprintf(stderr, "lesson name too long\n");
		wrapup(1);
	}
}

already(lesson, sequence)
char *lesson;
{
	struct whichdid *pw;
	for (pw=which; pw < which+nwh; pw++)
		if (strcmp(pw->w_less, lesson) == SAME)
			return(1);
	return(0);
}


#define	EASY	1
#define	MEDIUM	2
#define	HARD	3

mysys(s)
char *s;
{
	/* like "system" but rips off "mv", etc.*/
	/* also tries to guess if can get away with exec cmd */
	/* instead of sh cmd */
	char p[300];
	char *np[40];
	register char *t;
	int nv, type, stat;

	type = EASY;	/* we hope */
	for (t = s; *t && type != HARD; t++) {
		switch (*t) {
		case '*': 
		case '[': 
		case '?': 
		case '>': 
		case '<': 
		case '$':
		case '\'':
		case '"':
			type = MEDIUM;
			break;
		case '|': 
		case ';': 
		case '&':
			type = HARD;
			break;
		}
	}
	switch (type) {
	case HARD:
		return(system(s));
	case MEDIUM:
		strcpy(p, "exec ");
		strcat(p, s);
		return(system(p));
	case EASY:
		strcpy(p,s);
		nv = getargs(p, np);
		t=np[0];
		if ((strcmp(t, "mv") == 0)||
		    (strcmp(t, "cp") == 0)||
		    (strcmp(t, "rm") == 0)||
		    (strcmp(t, "ls") == 0) ) {
			if (fork() == 0) {
				char b[100];
				signal(SIGINT, SIG_DFL);
				strcpy(b, "/bin/");
				strcat(b, t);
				np[nv] = 0;
				execv(b, np);
				fprintf(stderr, "Execv failed\n");
				exit(1);
			}
			wait(&stat);
			return(stat);
		}
		return(system(s));
	}
}

/*
 * system():
 *	same as library version, except that resets
 *	default handling of signals in child, so that
 *	user gets the behavior he expects.
 */

int system(const char *s)
{
	int status, pid, w;
	void (*istat)(int), (*qstat)(int);

	istat = signal(SIGINT, SIG_IGN);	/* XXX should use sigaction() */
	qstat = signal(SIGQUIT, SIG_IGN);
	if ((pid = fork()) == 0) {
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		execl("/bin/sh", "sh", "-c", s, 0);
		_exit(127);
	}
	while ((w = wait(&status)) != pid && w != -1)
		;
	if (w == -1)
		status = -1;
	signal(SIGINT, istat);
	signal(SIGQUIT, qstat);
	return(status);
}

getargs(s, v)
char *s, **v;
{
	int i;

	i = 0;
	for (;;) {
		v[i++]=s;
		while (*s != 0 && *s!=' '&& *s != '\t')
			s++;
		if (*s == 0)
			break;
		*s++ =0;
		while (*s == ' ' || *s == '\t')
			s++;
		if (*s == 0)
			break;
	}
	return(i);
}


selsub(argc,argv)
char *argv[];
{
	char ans1[100], *cp;
	static char ans2[30];
	static char dirname[20];
	static char subname[20];

	if (argc > 1 && argv[1][0] == '-') {
		direct = argv[1]+1;
		argc--;
		argv++;
	}
	chknam(direct);
	if (chdir(direct) != 0) {
		fprintf(stderr, "can't cd to %s\n", direct);
		exit(1);
	}
	sname = argc > 1 ? argv[1] : 0;
	if (argc > 2)
		strcpy (level=ans2, argv[2]);
	else
		level = 0;
	if (argc > 3 )
		speed = atoi(argv[3]);
	if (!sname) {
		printf("These are the available courses -\n");
		list("Linfo");
		printf("If you want more information about the courses,\n");
		printf("or if you have never used 'learn' before,\n");
		printf("type 'return'; otherwise type the name of\n");
		printf("the course you want, followed by 'return'.\n");
		fflush(stdout);
		fgets(sname=subname, sizeof subname, stdin);
		trim(sname);
		if (sname[0] == '\0') {
			list("Xinfo");
			do {
				printf("\nWhich subject?  ");
				fflush(stdout);
				fgets(sname=subname, sizeof subname, stdin);
				trim(sname);
			} while (sname[0] == '\0');
		}
	}
	chknam(sname);
	if (!level) {
		printf("If you were in the middle of this subject\n");
		printf("and want to start where you left off, type\n");
		printf("the last lesson number the computer printed.\n");
		printf("To start at the beginning, just hit return.\n");
		fflush(stdout);
		fgets(ans2, sizeof ans2, stdin);
		trim(ans2);
		if (ans2[0]==0)
			strcpy(ans2,"0");
		for (cp=ans2; *cp; cp++)
			if (*cp == '(' || *cp == ' ')
				*cp= 0;
		level=ans2;
	}

	/* make new directory for user to play in */
	if ((playdir=mkdtemp(strdup("/tmp/plXXXXXX"))) == NULL ||
	     chdir(playdir) < 0) {
		fprintf(stderr, "Couldn't create playpen directory %s.\n", playdir);
		fprintf(stderr, "Bye.\n");
		exit(1);
	}

	/* after this point, we have a working directory. */
	/* have to call wrapup to clean up */
	snprintf(ans1, sizeof ans1, "%s/%s/Init", direct, sname);
	if (access(ans1, R_OK)==0) {
		snprintf(ans1, sizeof ans1, "%s/%s/Init %s", direct,sname, level);
		if (system(ans1) != 0) {
			printf("Leaving learn.\n");
			wrapup(1);
		}
	}
	if (level[0] == '-')	/* no lesson names start with - */
		ask = 1;
	start(level);
}

chknam(name)
char *name;
{
	if (access(name, R_OK|X_OK) < 0) {
		printf("Sorry, there is no subject or lesson named %s.\nBye.\n", name);
		exit(1);
	}
}


int	nsave	= 0;

selunit()
{
	char fnam[1024], s[1024];
	static char dobuff[50];
	char posslev[20][20];
	int diff[20], i, k, m, n, best, alts;
	FILE *f;
	char zb[200];
	static char saved[20];

	while (ask) {
		printf("What lesson? ");
		fflush(stdout);
		fgets(dobuff, sizeof dobuff, stdin);
		trim(dobuff);
		if (strcmp(dobuff, "bye") == 0)
			wrapup(0);
		level = todo = dobuff;
		snprintf(s, sizeof s, "%s/%s/L%s", _PATH_LLIB, sname, dobuff);
		if (access(s, R_OK) == 0)
			return;
		printf("no such lesson\n");
	}
	alts = 0;
retry:
	f=scrin;
	if (f==NULL) {
		snprintf(fnam, sizeof fnam, "%s/%s/L%s", _PATH_LLIB, sname, level);
		f = fopen(fnam, "r");
		if (f==NULL) {
			fprintf(stderr, "No script for lesson %s.\n", level);
			err(1, fnam);
			wrapup(1);
		}
		while (fgets(zb, 200, f)) {
			trim(zb);
			if (strcmp(zb, "#next")==0)
				break;
		}
	}
	if (feof(f)) {
		printf("Congratulations; you have finished this sequence.\n");
		fflush(stdout);
		todo = 0;
		return;
	}
	for(i=0; fgets(s, 50, f); i++) {
		sscanf(s, "%s %d", posslev[i], &diff[i]);
	}
	best = -1;
	/* cycle through lessons from random start */
	/* first try the current place, failing that back up to
	     last place there are untried alternatives (but only one backup) */
	n = grand()%i;
	for(k=0; k<i; k++) {
		m = (n+k)%i;
		if (already(posslev[m],0)) continue;
		if (best<0) best=m;
		/* real alternatives */
		alts++;
		if (abs(diff[m]-speed) < abs(diff[best]-speed))
			best=m;
	}
	if (best < 0 && nsave) {
		nsave--;
		strcpy(level, saved);
		goto retry;
	}
	if (best <0) {
		/* lessons exhausted or missing */
		printf("Sorry, there are no alternative lessons at this stage.\n");
		printf("See someone for help.\n");
		fflush(stdout);
		todo = 0;
		return;
	}
	strcpy (dobuff, posslev[best]);
	if (alts>1) {
		nsave=1;
		strcpy (saved, level);
	}
	todo = dobuff;
	fclose(f);
}

abs(x)
{
	return(x>=0? x: -x);
}

grand()
{
	static int garbage;
	int a[2], b;

	time(a);
	b = a[1]+10*garbage++;
	return(b&077777);
}

#define	ND	64

start(char *lesson)
{
	struct direct {
		int inode; 
		char name[14];
	};
	struct direct dv[ND], *dm, *dp;
	int f, c, n;
	char where [1024];

#if	0
	/* I'm not sure the point of this loop to unlink files, it may be
	 * some kind of cleanup. I'm sure I don't like unlinking files
	 * like this and, anyway, it would all have to be recoded using
	 * opendir() and readdir(). 	-- Ian
	 */
	f = open(".", 0);
	n = read(f, dv, ND*sizeof(*dp));
	n /= sizeof(*dp);
	if (n==ND)
		fprintf(stderr, "lesson too long\n");
	dm = dv+n;
	for(dp=dv; dp<dm; dp++)
		if (dp->inode) {
			n = strlen(dp->name);
			if (dp->name[n-2] == '.' && dp->name[n-1] == 'c')
				continue;
			c = dp->name[0];
			if (c>='a' && c<= 'z')
				unlink(dp->name);
		}
	close(f);
	if (ask)
		return;
#endif
	snprintf(where, sizeof where, "%s/%s/L%s", _PATH_LLIB, sname, lesson);
	if (access(where, R_OK)==0)	/* there is a file */
		return;
	fprintf(stderr, "No lesson %s\n",lesson);
	err(1, where);
	wrapup(1);
}

fcopy(new,old)
char *new, *old;
{
	char b[512];
	int n, fn, fo;
	fn = creat(new, 0666);
	fo = open(old,0);
	if (fo<0) return;
	if (fn<0) return;
	while ( (n=read(fo, b, 512)) > 0)
		write(fn, b, n);
	close(fn);
	close(fo);
}


whatnow()
{
	if (todo == 0) {
		more=0;
		return;
	}
	if (didok) {
		strcpy(level,todo);
		if (speed<=9) speed++;
	}
	else {
		speed -= 4;
		/* the 4 above means that 4 right, one wrong leave
		    you with the same speed. */
		if (speed <0) speed=0;
	}
	if (wrong) {
		speed -= 2;
		if (speed <0 ) speed = 0;
	}
	if (didok && more) {
		printf("\nGood.  Lesson %s (%d)\n\n",level, speed);
		fflush(stdout);
	}
}


wrapup(n)
int n;
{
	/* this routine does not use 'system' because it wants
	 interrupts turned off */
	int retval, pid, pidw;

	signal(SIGINT, SIG_IGN);
	chdir("..");
	if ( (pid=fork()) ==0) {
		signal(SIGHUP, SIG_IGN);
		execl("/bin/rm", "rm", "-r", playdir, 0);
		execl("/usr/bin/rm", "rm", "-r", playdir, 0);
		fprintf(stderr, "Can't find 'rm' command.\n");
		exit(0);
	}
	printf("Bye.\n"); /* not only does this reassure user but 
			it stalls for time while deleting directory */
	fflush(stdout);
	/* printf("Wantd %d got %d val %d\n",pid, pidw, retval); */
	exit(n);
}
