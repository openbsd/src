/*	$OpenBSD: common.c,v 1.7 1998/06/26 21:29:13 millert Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char RCSid[] = 
"$From: common.c,v 6.82 1998/03/23 23:27:33 michaelc Exp $";
#else
static char RCSid[] = 
"$OpenBSD: common.c,v 1.7 1998/06/26 21:29:13 millert Exp $";
#endif

static char sccsid[] = "@(#)common.c";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* !lint */

/*
 * Things common to both the client and server.
 */

#include "defs.h"
#if	defined(NEED_UTIME_H)
#include <utime.h>
#endif	/* defined(NEED_UTIME_H) */
#include <sys/socket.h>
#include <sys/wait.h>

/*
 * Variables common to both client and server
 */
char			host[MAXHOSTNAMELEN];	/* Name of this host */
UID_T			userid = (UID_T)-1;	/* User's UID */
GID_T			groupid = (GID_T)-1;	/* User's GID */
char		       *homedir = NULL;		/* User's $HOME */
char		       *locuser = NULL;		/* Local User's name */
int			isserver = FALSE;	/* We're the server */
int     		amchild = 0;		/* This PID is a child */
int			do_fork = 1;		/* Fork child process */
char		       *currenthost = NULL;	/* Current client hostname */
char		       *progname = NULL;	/* Name of this program */
int			rem_r = -1;		/* Client file descriptor */
int			rem_w = -1;		/* Client file descriptor */
struct passwd	       *pw = NULL;		/* Local user's pwd entry */
int 			contimedout = FALSE;	/* Connection timed out */
int			proto_version = -1;	/* Protocol version */
int			rtimeout = RTIMEOUT;	/* Response time out */
jmp_buf			finish_jmpbuf;		/* Finish() jmp buffer */
int			setjmp_ok = FALSE;	/* setjmp()/longjmp() status */
char		      **realargv;		/* Real main() argv */
int			realargc;		/* Real main() argc */
opt_t			options = 0;		/* Global install options */

/* 
 * Front end to write() that handles partial write() requests.
 */
extern WRITE_RETURN_T xwrite(fd, buf, len)
	int fd;
	void *buf;
	WRITE_AMT_T len;
{
    	WRITE_AMT_T nleft = len;
	WRITE_RETURN_T nwritten;
	register char *ptr = buf;
         
	while (nleft > 0) {
	    	if ((nwritten = write(fd, ptr, nleft)) <= 0) {
			return nwritten;
	    	}
	    	nleft -= nwritten;
	    	ptr += nwritten;
	}

	return len;
}

/*
 * Set program name
 */
extern void setprogname(argv)
	char **argv;
{
	register char *cp;

	if (!progname) {
		progname = strdup(argv[0]);
		if ((cp = strrchr(progname, '/')))
			progname = cp + 1;
	}
}

/*
 * Do run-time initialization
 */
extern int init(argc, argv, envp)
	/*ARGSUSED*/
	int argc;
	char **argv;
	char **envp;
{
	register int i;
	register char *cp;

	if (!isserver)
		(void) signal(SIGSEGV, sighandler);

	setprogname(argv);

	/*
	 * Save a copy of our argc and argv before setargs() overwrites them
	 */
	realargc = argc;
	realargv = (char **) xmalloc(sizeof(char *) * (argc+1));
	for (i = 0; i < argc; i++)
		realargv[i] = strdup(argv[i]);

#if	defined(SETARGS)
	setargs_settup(argc, argv, envp);
#endif	/* SETARGS */

	pw = getpwuid(userid = getuid());
	if (pw == NULL) {
		error("Your user id (%d) is not known to this system.",
		      getuid());
		return(-1);
	}

	debugmsg(DM_MISC, "UserID = %d pwname = '%s' home = '%s'\n",
		 userid, pw->pw_name, pw->pw_dir);
	homedir = strdup(pw->pw_dir);
	locuser = strdup(pw->pw_name);
	groupid = pw->pw_gid;
	gethostname(host, sizeof(host));
	if ((cp = strchr(host, '.')) != NULL)
	    	*cp = CNULL;

	/*
	 * If we're not root, disable paranoid ownership checks
	 * since normal users cannot chown() files.
	 */
	if (!isserver && userid != 0) {
		FLAG_ON(options, DO_NOCHKOWNER);
		FLAG_ON(options, DO_NOCHKGROUP);
	}

	return(0);
}

/*
 * Finish things up before ending.
 */
extern void finish()
{
	extern jmp_buf finish_jmpbuf;

	debugmsg(DM_CALL, 
		 "finish() called: do_fork = %d amchild = %d isserver = %d",
		 do_fork, amchild, isserver);
	cleanup();

	/*
	 * There's no valid finish_jmpbuf for the rdist master parent.
	 */
	if (!do_fork || amchild || isserver) {

		if (!setjmp_ok) {
#ifdef DEBUG_SETJMP
			error("attemping longjmp() without target");
			abort();
#else
			exit(1);
#endif
		}

		longjmp(finish_jmpbuf, 1);
		/*NOTREACHED*/
		error("Unexpected failure of longjmp() in finish()");
		exit(2);
	} else
		exit(1);
}

/*
 * Handle lost connections
 */
extern void lostconn()
{
	/* Prevent looping */
	(void) signal(SIGPIPE, SIG_IGN);

	rem_r = rem_w = -1;	/* Ensure we don't try to send to server */
	checkhostname();
	error("Lost connection to %s", 
	      (currenthost) ? currenthost : "(unknown)");

	finish();
}

/*
 * Do a core dump
 */
extern void coredump()
{
	error("Segmentation violation - dumping core [PID = %d, %s]",
	      getpid(), 
	      (isserver) ? "isserver" : ((amchild) ? "amchild" : "parent"));
	abort();
	/*NOTREACHED*/
	fatalerr("Abort failed - no core dump.  Exiting...");
}

/*
 * General signal handler
 */
extern void sighandler(sig)
	int sig;
{
	int save_errno = errno;

	debugmsg(DM_CALL, "sighandler() received signal %d\n", sig);

	switch (sig) {
	case SIGALRM:
		contimedout = TRUE;
		checkhostname();
		error("Response time out");
		finish();
		break;

	case SIGPIPE:
		lostconn();
		break;

	case SIGFPE:
		debug = !debug;
		break;

	case SIGSEGV:
		coredump();
		break;

	case SIGHUP:
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		finish();
		break;

	default:
		fatalerr("No signal handler defined for signal %d.", sig);
	}
	errno = save_errno;
}

/*
 * Function to actually send the command char and message to the
 * remote host.
 */
static int sendcmdmsg(cmd, msg)
	char cmd;
	char *msg;
{
	int len;

	if (rem_w < 0)
		return(-1);

	/*
	 * All commands except C_NONE should have a newline
	 */
	if (cmd != C_NONE && !strchr(msg + 1, '\n'))
		(void) strcat(msg + 1, "\n");

	if (cmd == C_NONE)
		len = strlen(msg);
	else {
		len = strlen(msg + 1) + 1;
		msg[0] = cmd;
	}

	debugmsg(DM_PROTO, ">>> Cmd = %c (\\%3.3o) Msg = \"%.*s\"",
		 cmd, cmd, 
		 (cmd == C_NONE) ? len-1 : len-2,
		 (cmd == C_NONE) ? msg : msg + 1);

	return(!(xwrite(rem_w, msg, len) == len));
}

/*
 * Send a command message to the remote host.
 * Called as sendcmd(char cmdchar, char *fmt, arg1, arg2, ...)
 * The fmt and arg? arguments are optional.
 */
#if	defined(ARG_TYPE) && ARG_TYPE == ARG_STDARG
/*
 * Stdarg frontend to sendcmdmsg()
 */
extern int sendcmd(char cmd, char *fmt, ...)
{
	static char buf[BUFSIZ];
	va_list args;

	va_start(args, fmt);
	if (fmt)
		(void) vsprintf((cmd == C_NONE) ? buf : buf + 1, fmt, args);
	else
		buf[1] = CNULL;
	va_end(args);

	return(sendcmdmsg(cmd, buf));
}
#endif	/* ARG_TYPE == ARG_STDARG */

#if	defined(ARG_TYPE) && ARG_TYPE == ARG_VARARGS
/*
 * Varargs frontend to sendcmdmsg()
 */
extern int sendcmd(va_alist)
	va_dcl
{
	static char buf[BUFSIZ];
	va_list args;
	char cmd;
	char *fmt;

	va_start(args);
	/* XXX The "int" is necessary as a workaround for broken varargs */
	cmd = (char) va_arg(args, int);
	fmt = va_arg(args, char *);
	if (fmt)
		(void) vsprintf((cmd == C_NONE) ? buf : buf + 1, fmt, args);
	else
		buf[1] = CNULL;
	va_end(args);

	return(sendcmdmsg(cmd, buf));
}
#endif	/* ARG_TYPE == ARG_VARARGS */

#if	!defined(ARG_TYPE)
/*
 * Stupid frontend to sendcmdmsg()
 */
/*VARARGS2*/
extern int sendcmd(cmd, fmt, a1, a2, a3, a4, a5, a6, a7, a8)
	char cmd;
	char *fmt;
{
	static char buf[BUFSIZ];

	if (fmt)
		(void) sprintf((cmd == C_NONE) ? buf : buf + 1, 
			       fmt, a1, a2, a3, a4, a5, a6, a7, a8);
	else
		buf[1] = CNULL;

	return(sendcmdmsg(cmd, buf));
}
#endif	/* !ARG_TYPE */

/*
 * Internal variables and routines for reading lines from the remote.
 */
static u_char rembuf[BUFSIZ];
static u_char *remptr;
static int remleft;

#define remc() (--remleft < 0 ? remmore() : *remptr++)

/*
 * Back end to remote read()
 */
static int remread(fd, buf, bufsiz)
	int fd;
	u_char *buf;
	int bufsiz;
{
	return(read(fd, (char *)buf, bufsiz));
}

static int remmore()
{
	(void) signal(SIGALRM, sighandler);
	(void) alarm(rtimeout);

	remleft = remread(rem_r, rembuf, sizeof(rembuf));

	(void) alarm(0);

	if (remleft < 0)
		return (-2);	/* error */
	if (remleft == 0)
		return (-1);	/* EOF */
	remptr = rembuf;
	remleft--;
	return (*remptr++);
}
	
/*
 * Read an input line from the remote.  Return the number of bytes
 * stored (equivalent to strlen(p)).  If `cleanup' is set, EOF at
 * the beginning of a line is returned as EOF (-1); other EOFs, or
 * errors, call cleanup() or lostconn().  In other words, unless
 * the third argument is nonzero, this routine never returns failure.
 */
extern int remline(buffer, space, doclean)
	register u_char *buffer;
	int space;
	int doclean;
{
	register int c, left = space;
	register u_char *p = buffer;

	if (rem_r < 0) {
		error("Cannot read remote input: Remote descriptor not open.");
		return(-1);
	}

	while (left > 0) {
		if ((c = remc()) < -1) {	/* error */
			if (doclean) {
				finish();
				/*NOTREACHED*/
			}
			lostconn();
			/*NOTREACHED*/
		}
		if (c == -1) {			/* got EOF */
			if (doclean) {
				if (left == space)
					return (-1);/* signal proper EOF */
				finish();	/* improper EOF */
				/*NOTREACHED*/
			}
			lostconn();
			/*NOTREACHED*/
		}
		if (c == '\n') {
			*p = CNULL;

			if (debug) {
				static char mbuf[BUFSIZ];

				(void) sprintf(mbuf, 
					"<<< Cmd = %c (\\%3.3o) Msg = \"%s\"", 
					       buffer[0], buffer[0], 
					       buffer + 1);

				debugmsg(DM_PROTO, "%s", mbuf);
			}

			return (space - left);
		}
		*p++ = c;
		left--;
	}

	/* this will probably blow the entire session */
	error("remote input line too long");
	p[-1] = CNULL;		/* truncate */
	return (space);
}

/*
 * Non-line-oriented remote read.
 */
int
readrem(p, space)
	char *p;
	register int space;
{
	if (remleft <= 0) {
		/*
		 * Set remote time out alarm.
		 */
		(void) signal(SIGALRM, sighandler);
		(void) alarm(rtimeout);

		remleft = remread(rem_r, rembuf, sizeof(rembuf));

		(void) alarm(0);
		remptr = rembuf;
	}

	if (remleft <= 0)
		return (remleft);
	if (remleft < space)
		space = remleft;

	bcopy((char *) remptr, p, space);

	remptr += space;
	remleft -= space;

	return (space);
}

/*
 * Get the user name for the uid.
 */
extern char *getusername(uid, file, opts)
	UID_T uid;
	char *file;
	opt_t opts;
{
	static char buf[100];
	static UID_T lastuid = (UID_T)-1;
	struct passwd *pwd = NULL;

	/*
	 * The value of opts may have changed so we always
	 * do the opts check.
	 */
  	if (IS_ON(opts, DO_NUMCHKOWNER)) { 
		(void) sprintf(buf, ":%d", uid);
		return(buf);
  	}

	/*
	 * Try to avoid getpwuid() call.
	 */
	if (lastuid == uid && buf[0] != '\0' && buf[0] != ':')
		return(buf);

	lastuid = uid;

	if ((pwd = getpwuid(uid)) == NULL) {
		message(MT_WARNING,
			"%s: No password entry for uid %d", file, uid);
		(void) sprintf(buf, ":%d", uid);
	} else
		(void) strcpy(buf, pwd->pw_name);

	return(buf);
}

/*
 * Get the group name for the gid.
 */
extern char *getgroupname(gid, file, opts)
	GID_T gid;
	char *file;
	opt_t opts;
{
	static char buf[100];
	static GID_T lastgid = (GID_T)-1;
	struct group *grp = NULL;

	/*
	 * The value of opts may have changed so we always
	 * do the opts check.
	 */
  	if (IS_ON(opts, DO_NUMCHKGROUP)) { 
		(void) sprintf(buf, ":%d", gid);
		return(buf);
  	}

	/*
	 * Try to avoid getgrgid() call.
	 */
	if (lastgid == gid && buf[0] != '\0' && buf[0] != ':')
		return(buf);

	lastgid = gid;

	if ((grp = (struct group *)getgrgid(gid)) == NULL) {
		message(MT_WARNING, "%s: No name for group %d", file, gid);
		(void) sprintf(buf, ":%d", gid);
	} else
		(void) strcpy(buf, grp->gr_name);

	return(buf);
}

/*
 * Read a response from the remote host.
 */
extern int response()
{
	static u_char resp[BUFSIZ];
	u_char *s;
	int n;

	debugmsg(DM_CALL, "response() start\n");

	n = remline(s = resp, sizeof(resp), 0);

	n--;
	switch (*s++) {
        case C_ACK:
		debugmsg(DM_PROTO, "received ACK\n");
		return(0);
	case C_LOGMSG:
		if (n > 0) {
			message(MT_CHANGE, "%s", s);
			return(1);
		}
		debugmsg(DM_PROTO, "received EMPTY logmsg\n");
		return(0);
	case C_NOTEMSG:
		if (s)
			message(MT_NOTICE, "%s", s);
		return(response());

	default:
		s--;
		n++;
		/* fall into... */

	case C_ERRMSG:	/* Normal error message */
		if (s)
			message(MT_NERROR, "%s", s);
		return(-1);

	case C_FERRMSG:	/* Fatal error message */
		if (s)
			message(MT_FERROR, "%s", s);
		finish();
	}
	/*NOTREACHED*/
}

/*
 * This should be in expand.c but the other routines call other modules
 * that we don't want to load in.
 *
 * Expand file names beginning with `~' into the
 * user's home directory path name. Return a pointer in buf to the
 * part corresponding to `file'.
 */
extern char *exptilde(ebuf, file)
	char *ebuf;
	register char *file;
{
	register char *s1, *s2, *s3;
	extern char *homedir;

	if (*file != '~') {
		(void) strcpy(ebuf, file);
		return(ebuf);
	}
	if (*++file == CNULL) {
		s2 = homedir;
		s3 = NULL;
	} else if (*file == '/') {
		s2 = homedir;
		s3 = file;
	} else {
		s3 = file;
		while (*s3 && *s3 != '/')
			s3++;
		if (*s3 == '/')
			*s3 = CNULL;
		else
			s3 = NULL;
		if (pw == NULL || strcmp(pw->pw_name, file) != 0) {
			if ((pw = getpwnam(file)) == NULL) {
				error("%s: unknown user name", file);
				if (s3 != NULL)
					*s3 = '/';
				return(NULL);
			}
		}
		if (s3 != NULL)
			*s3 = '/';
		s2 = pw->pw_dir;
	}
	for (s1 = ebuf; (*s1++ = *s2++); )
		;
	s2 = --s1;
	if (s3 != NULL) {
		s2++;
		while ((*s1++ = *s3++))
			;
	}
	return(s2);
}

#if	defined(DIRECT_RCMD)
/*
 * Set our effective user id to the user running us.
 * This should be the uid we do most of our work as.
 */
extern int becomeuser()
{
	int r = 0;

#if	defined(HAVE_SAVED_IDS)
	r = seteuid(userid);
#else
	r = setreuid(0, userid);
#endif	/* HAVE_SAVED_IDS */

	if (r < 0)
		error("becomeuser %d failed: %s (ruid = %d euid = %d)",
		      userid, SYSERR, getuid(), geteuid());

	return(r);
}
#endif	/* DIRECT_RCMD */

#if	defined(DIRECT_RCMD)
/*
 * Set our effective user id to "root" (uid = 0)
 */
extern int becomeroot()
{
	int r = 0;

#if	defined(HAVE_SAVED_IDS)
	r = seteuid(0);
#else
	r = setreuid(userid, 0);
#endif	/* HAVE_SAVED_IDS */

	if (r < 0)
		error("becomeroot failed: %s (ruid = %d euid = %d)",
		      SYSERR, getuid(), geteuid());

	return(r);
}
#endif	/* DIRECT_RCMD */

/*
 * Set access and modify times of a given file
 */
extern int setfiletime(file, atime, mtime)
	char *file;
	time_t atime;
	time_t mtime;
{
#if	SETFTIME_TYPE == SETFTIME_UTIMES
	struct timeval tv[2];

	if (atime != 0 && mtime != 0) {
		tv[0].tv_sec = atime;
		tv[1].tv_sec = mtime;
		tv[0].tv_usec = tv[1].tv_usec = (time_t) 0;
		return(utimes(file, tv));
	} else	/* Set to current time */
		return(utimes(file, NULL));

#endif	/* SETFTIME_UTIMES */

#if	SETFTIME_TYPE == SETFTIME_UTIME
	struct utimbuf utbuf;

	if (atime != 0 && mtime != 0) {
		utbuf.actime = atime;
		utbuf.modtime = mtime;
		return(utime(file, &utbuf));
	} else	/* Set to current time */
		return(utime(file, NULL));
#endif	/* SETFTIME_UTIME */

#if	!defined(SETFTIME_TYPE)
	There is no "SETFTIME_TYPE" defined!
#endif	/* SETFTIME_TYPE */
}

/*
 * Get version info
 */
extern char *getversion()
{
	static char buff[BUFSIZ];

	(void) sprintf(buff,
	"Version %s.%d (%s) - Protocol Version %d, Release %s, Patch level %d",
		       DISTVERSION, PATCHLEVEL, DISTSTATUS,
		       VERSION, DISTVERSION, PATCHLEVEL);

	return(buff);
}

/*
 * Execute a shell command to handle special cases.
 * This is now common to both server and client
 */
void runcommand(cmd)
	char *cmd;
{
	int fd[2], pid, i;
	int status;
	register char *cp, *s;
	char sbuf[BUFSIZ], buf[BUFSIZ];

	if (pipe(fd) < 0) {
		error("pipe of %s failed: %s", cmd, SYSERR);
		return;
	}

	if ((pid = fork()) == 0) {
		/*
		 * Return everything the shell commands print.
		 */
		(void) close(0);
		(void) close(1);
		(void) close(2);
		(void) open(_PATH_DEVNULL, O_RDONLY);
		(void) dup(fd[PIPE_WRITE]);
		(void) dup(fd[PIPE_WRITE]);
		(void) close(fd[PIPE_READ]);
		(void) close(fd[PIPE_WRITE]);
		(void) execl(_PATH_BSHELL, "sh", "-c", cmd, 0);
		_exit(127);
	}
	(void) close(fd[PIPE_WRITE]);
	s = sbuf;
	*s++ = C_LOGMSG;
	while ((i = read(fd[PIPE_READ], buf, sizeof(buf))) > 0) {
		cp = buf;
		do {
			*s++ = *cp++;
			if (cp[-1] != '\n') {
				if (s < (char *) &sbuf[sizeof(sbuf)-1])
					continue;
				*s++ = '\n';
			}
			/*
			 * Throw away blank lines.
			 */
			if (s == &sbuf[2]) {
				s--;
				continue;
			}
			if (isserver)
				(void) xwrite(rem_w, sbuf, s - sbuf);
			else {
				*s = CNULL;
				message(MT_INFO, "%s", sbuf+1);
			}
			s = &sbuf[1];
		} while (--i);
	}
	if (s > (char *) &sbuf[1]) {
		*s++ = '\n';
		if (isserver)
			(void) xwrite(rem_w, sbuf, s - sbuf);
		else {
			*s = CNULL;
			message(MT_INFO, "%s", sbuf+1);
		}
	}
	while ((i = wait(&status)) != pid && i != -1)
		;
	if (i == -1)
		status = -1;
	(void) close(fd[PIPE_READ]);
	if (status)
		error("shell returned %d", status);
	else if (isserver)
		ack();
}

/*
 * Malloc with error checking
 */
char *xmalloc(amt)
	int amt;
{
	char *ptr;
	extern POINTER *malloc();

	if ((ptr = (char *)malloc(amt)) == NULL)
		fatalerr("Cannot malloc %d bytes of memory.", amt);

	return(ptr);
}

/*
 * realloc with error checking
 */
char *xrealloc(baseptr, amt)
	char *baseptr;
	unsigned int amt;
{
	char *new;
	extern POINTER *realloc();

	if ((new = (char *)realloc(baseptr, amt)) == NULL)
		fatalerr("Cannot realloc %d bytes of memory.", amt);

	return(new);
}

/*
 * calloc with error checking
 */
char *xcalloc(num, esize)
	unsigned num;
	unsigned esize;
{
	char *ptr;
	extern POINTER *calloc();

	if ((ptr = (char *)calloc(num, esize)) == NULL)
		fatalerr("Cannot calloc %d * %d = %d bytes of memory.",
		      num, esize, num * esize);

	return(ptr);
}

/*
 * Private version of basename()
 */
extern char *xbasename(path)
	char *path;
{
	register char *cp;
 
	if ((cp = strrchr(path, '/')))
		return(cp+1);
	else
		return(path);
}

/*
 * Take a colon (':') seperated path to a file and
 * search until a component of that path is found and
 * return the found file name.
 */
extern char *searchpath(path)
	char *path;
{
	register char *cp;
	register char *file;
	struct stat statbuf;

	for (; ;) {
		if (!path)
			return(NULL);
		file = path;
		cp = strchr(path, ':');
		if (cp) {
			path = cp + 1;
			*cp = CNULL;
		} else
			path = NULL;
		if (stat(file, &statbuf) == 0)
			return(file);
		/* Put back what we zapped */
		if (path)
			*cp = ':';
	}
}

/*
 * Set line buffering.
 */
extern void
mysetlinebuf(fp)
	FILE *fp;
{
#if	SETBUF_TYPE == SETBUF_SETLINEBUF
	setlinebuf(fp);
#endif	/* SETBUF_SETLINEBUF */
#if	SETBUF_TYPE == SETBUF_SETVBUF
	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
#endif	/* SETBUF_SETVBUF */
#if	!defined(SETBUF_TYPE)
	No SETBUF_TYPE is defined!
#endif	/* SETBUF_TYPE */
}

/*
 * Our interface to system call to get a socket pair.
 */
int
getsocketpair(domain, type, protocol, sv)
	int domain;
	int type;
	int protocol;
	int sv[];
{
#if	SOCKPAIR_TYPE == SOCKPAIR_SOCKETPAIR
	return(socketpair(domain, type, protocol, sv));
#endif	/* SOCKPAIR_SOCKETPAIR */
#if	SOCKPAIR_TYPE == SOCKPAIR_SPIPE
	return(spipe(sv));
#endif	/* SOCKPAIR_SPIPE */
#if	!defined(SOCKPAIR_TYPE)
	No SOCKPAIR_TYPE is defined!
#endif	/* SOCKPAIR_TYPE */
}
