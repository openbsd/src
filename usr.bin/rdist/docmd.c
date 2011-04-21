/*	$OpenBSD: docmd.c,v 1.24 2011/04/21 02:44:15 krw Exp $	*/

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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "defs.h"
#include "y.tab.h"

/*
 * Functions for rdist that do command (cmd) related activities.
 */

#include <sys/socket.h>
#include <netdb.h>

struct subcmd	       *subcmds;		/* list of sub-commands for 
						   current cmd */
struct namelist	       *filelist;		/* list of source files */
extern struct cmd      *cmds;			/* Initialized by yyparse() */
time_t			lastmod;		/* Last modify time */

extern char 		target[BUFSIZ];
extern char 	       *ptarget;
extern int		activechildren;
extern int		maxchildren;
extern int		amchild;
extern char	       *path_rdistd;

static void closeconn(void);
static void notify(char *, struct namelist *, time_t);
static void checkcmd(struct cmd *);
static void markfailed(struct cmd *, struct cmd *);
static int remotecmd(char *, char *, char *, char *);
static int makeconn(char *);
static void doarrow(struct cmd *, char **);
static void rcmptime(struct stat *, struct subcmd *, char **);
static void cmptime(char *, struct subcmd *, char **);
static void dodcolon(struct cmd *, char **);
static void docmdhost(struct cmd *, char **);
static void docmd(struct cmd *, int, char **);

/*
 * Signal end of connection.
 */
static void
closeconn(void)
{
	debugmsg(DM_CALL, "closeconn() called\n");

	if (rem_w >= 0) {
		/* We don't care if the connection is still good or not */
		signal(SIGPIPE, SIG_IGN);	

		(void) sendcmd(C_FERRMSG, NULL);
		(void) close(rem_w);
		(void) close(rem_r); /* This can't hurt */
		rem_w = -1;
		rem_r = -1;
	}
}

/*
 * Notify the list of people the changes that were made.
 * rhost == NULL if we are mailing a list of changes compared to at time
 * stamp file.
 */
static void
notify(char *rhost, struct namelist *to, time_t lmod)
{
	int fd;
	ssize_t len;
	FILE *pf;
	struct stat stb;
	static char buf[BUFSIZ];
	extern char *locuser;
	char *file, *user;

	if (IS_ON(options, DO_VERIFY) || to == NULL)
		return;

	if ((file = getnotifyfile()) == NULL)
		return;

	if (!IS_ON(options, DO_QUIET)) {
		message(MT_INFO, "notify %s%s %s", 
			(rhost) ? "@" : "",
			(rhost) ? rhost : "", getnlstr(to));
	}

	if (nflag)
		return;

	debugmsg(DM_MISC, "notify() temp file = '%s'", file);

	if ((fd = open(file, O_RDONLY)) < 0) {
		error("%s: open for reading failed: %s", file, SYSERR);
		return;
	}
	if (fstat(fd, &stb) < 0) {
		error("%s: fstat failed: %s", file, SYSERR);
		(void) close(fd);
		return;
	}
	if (stb.st_size == 0) {
		(void) close(fd);
		return;
	}
	/*
	 * Create a pipe to mailing program.
	 * Set IFS to avoid possible security problem with users
	 * setting "IFS=/".
	 */
	(void) snprintf(buf, sizeof(buf), "IFS=\" \t\"; export IFS; %s -oi -t", 
		       _PATH_SENDMAIL);
	pf = popen(buf, "w");
	if (pf == NULL) {
		error("notify: \"%s\" failed\n", _PATH_SENDMAIL);
		(void) unlink(file);
		(void) close(fd);
		return;
	}
	/*
	 * Output the proper header information.
	 */
	(void) fprintf(pf, "Auto-Submitted: auto-generated\n");
	(void) fprintf(pf, "From: rdist (Remote distribution program)\n");
	(void) fprintf(pf, "To:");
	if (!any('@', to->n_name) && rhost != NULL)
		(void) fprintf(pf, " %s@%s", to->n_name, rhost);
	else
		(void) fprintf(pf, " %s", to->n_name);
	to = to->n_next;
	while (to != NULL) {
		if (!any('@', to->n_name) && rhost != NULL)
			(void) fprintf(pf, ", %s@%s", to->n_name, rhost);
		else
			(void) fprintf(pf, ", %s", to->n_name);
		to = to->n_next;
	}
	(void) putc('\n', pf);

	if ((user = getlogin()) == NULL)
		user = locuser;

	if (rhost != NULL)
		(void) fprintf(pf, 
			 "Subject: files updated by %s from %s to %s\n",
			 locuser, host, rhost);
	else
		(void) fprintf(pf, "Subject: files updated after %s\n", 
			       ctime(&lmod));
	(void) putc('\n', pf);
	(void) putc('\n', pf);
	(void) fprintf(pf, "Options: %s\n\n", getondistoptlist(options));

	while ((len = read(fd, buf, sizeof(buf))) > 0)
		(void) fwrite(buf, 1, len, pf);

	(void) pclose(pf);
	(void) close(fd);
	(void) unlink(file);
}

/* 
 * XXX Hack for NFS.  If a hostname from the distfile
 * ends with a '+', then the normal restriction of
 * skipping files that are on an NFS filesystem is
 * bypassed.  We always strip '+' to be consistent.
 */
static void
checkcmd(struct cmd *cmd)
{
	int l;

	if (!cmd || !(cmd->c_name)) {
		debugmsg(DM_MISC, "checkcmd() NULL cmd parameter");
		return;
	}

	l = strlen(cmd->c_name);
	if (l <= 0)
		return;
	if (cmd->c_name[l-1] == '+') {
		cmd->c_flags |= CMD_NOCHKNFS;
		cmd->c_name[l-1] = CNULL;
	}
}

/*
 * Mark all other entries for this command (cmd)
 * as assigned.
 */
void
markassigned(struct cmd *cmd, struct cmd *cmdlist)
{
	struct cmd *pcmd;
	
	for (pcmd = cmdlist; pcmd; pcmd = pcmd->c_next) {
		checkcmd(pcmd);
		if (pcmd->c_type == cmd->c_type &&
		    strcmp(pcmd->c_name, cmd->c_name)==0)
			pcmd->c_flags |= CMD_ASSIGNED;
	}
}

/*
 * Mark the command "cmd" as failed for all commands in list cmdlist.
 */
static void
markfailed(struct cmd *cmd, struct cmd *cmdlist)
{
	struct cmd *pc;

	if (!cmd) {
		debugmsg(DM_MISC, "markfailed() NULL cmd parameter");
		return;
	}

	checkcmd(cmd);
	cmd->c_flags |= CMD_CONNFAILED;
	for (pc = cmdlist; pc; pc = pc->c_next) {
		checkcmd(pc);
		if (pc->c_type == cmd->c_type &&
		    strcmp(pc->c_name, cmd->c_name)==0)
			pc->c_flags |= CMD_CONNFAILED;
	}
}

static int
remotecmd(char *rhost, char *luser, char *ruser, char *cmd)
{
	int desc;
#if	defined(DIRECT_RCMD)
	static int port = -1;
#endif	/* DIRECT_RCMD */

	debugmsg(DM_MISC, "local user = %s remote user = %s\n", luser, ruser);
	debugmsg(DM_MISC, "Remote command = '%s'\n", cmd);

	(void) fflush(stdout);
	(void) fflush(stderr);
	(void) signal(SIGALRM, sighandler);
	(void) alarm(RTIMEOUT);

#if	defined(DIRECT_RCMD)
	(void) signal(SIGPIPE, sighandler);

	if (port < 0) {
		struct servent *sp;
		
		if ((sp = getservbyname("shell", "tcp")) == NULL)
				fatalerr("shell/tcp: unknown service");
		port = sp->s_port;
	}

	if (becomeroot() != 0)
		exit(1);
	desc = rcmd(&rhost, port, luser, ruser, cmd, 0);
	if (becomeuser() != 0)
		exit(1);
#else	/* !DIRECT_RCMD */
	debugmsg(DM_MISC, "Remote shell command = '%s'\n",
	    path_remsh ? path_remsh : "default");
	(void) signal(SIGPIPE, SIG_IGN);
	desc = rcmdsh(&rhost, -1, luser, ruser, cmd, path_remsh);
	if (desc > 0)
		(void) signal(SIGPIPE, sighandler);
#endif	/* DIRECT_RCMD */

	(void) alarm(0);

	return(desc);
}

/*
 * Create a connection to the rdist server on the machine rhost.
 * Return 0 if the connection fails or 1 if it succeeds.
 */
static int
makeconn(char *rhost)
{
	char *ruser, *cp;
	static char *cur_host = NULL;
	extern char *locuser;
	extern int64_t min_freefiles, min_freespace;
	extern char *remotemsglist;
	char tuser[BUFSIZ], buf[BUFSIZ];
	u_char respbuff[BUFSIZ];
	int n;

	debugmsg(DM_CALL, "makeconn(%s)", rhost);

	/*
	 * See if we're already connected to this host
	 */
	if (cur_host != NULL && rem_w >= 0) {
		if (strcmp(cur_host, rhost) == 0)
			return(1);
		closeconn();
	}

	/*
	 * Determine remote user and current host names
	 */
	cur_host = rhost;
	cp = strchr(rhost, '@');

	if (cp != NULL) {
		char c = *cp;

		*cp = CNULL;
		(void) strlcpy((char *)tuser, rhost, sizeof(tuser));
		*cp = c;
		rhost = cp + 1;
		ruser = tuser;
		if (*ruser == CNULL)
			ruser = locuser;
		else if (!okname(ruser))
			return(0);
	} else
		ruser = locuser;

	if (!IS_ON(options, DO_QUIET))
		message(MT_VERBOSE, "updating host %s", rhost);

	(void) snprintf(buf, sizeof(buf), "%.*s -S",
			(int)(sizeof(buf)-5), path_rdistd);
		
	if ((rem_r = rem_w = remotecmd(rhost, locuser, ruser, buf)) < 0)
		return(0);

	/*
	 * First thing received should be S_VERSION
	 */
	respbuff[0] = '\0';
	n = remline(respbuff, sizeof(respbuff), TRUE);
	if (n <= 0 || respbuff[0] != S_VERSION) {
		if (n > 0)
		    error("Unexpected input from server: \"%s\".", respbuff);
		else
		    error("No input from server.");
		closeconn();
		return(0);
	}

	/*
	 * For future compatibility we check to see if the server
	 * sent it's version number to us.  If it did, we use it,
	 * otherwise, we send our version number to the server and let
	 * it decide if it can handle our protocol version.
	 */
	if (respbuff[1] == CNULL) {
		/*
		 * The server wants us to send it our version number
		 */
		(void) sendcmd(S_VERSION, "%d", VERSION);
		if (response() < 0) 
			return(0);
	} else {
		/*
		 * The server sent it's version number to us
		 */
		proto_version = atoi(&respbuff[1]);
		if (proto_version != VERSION) {
			fatalerr(
		  "Server version (%d) is not the same as local version (%d).",
			      proto_version, VERSION);
			return(0);
		}
	}

	/*
	 * Send config commands
	 */
	if (host[0]) {
		(void) sendcmd(C_SETCONFIG, "%c%s", SC_HOSTNAME, host);
		if (response() < 0)
			return(0);
	}
	if (min_freespace) {
		(void) sendcmd(C_SETCONFIG, "%c%lld", SC_FREESPACE, 
			       min_freespace);
		if (response() < 0)
			return(0);
	}
	if (min_freefiles) {
		(void) sendcmd(C_SETCONFIG, "%c%lld", SC_FREEFILES, 
			       min_freefiles);
		if (response() < 0)
			return(0);
	}
	if (remotemsglist) {
		(void) sendcmd(C_SETCONFIG, "%c%s", SC_LOGGING, remotemsglist);
		if (response() < 0)
			return(0);
	}
	if (strcmp(defowner, "bin") != 0) {
		(void) sendcmd(C_SETCONFIG, "%c%s", SC_DEFOWNER, defowner);
		if (response() < 0)
			return(0);
	}
	if (strcmp(defgroup, "bin") != 0) {
		(void) sendcmd(C_SETCONFIG, "%c%s", SC_DEFGROUP, defgroup);
		if (response() < 0)
			return(0);
	}

	return(1);
}

/*
 * Process commands for sending files to other machines.
 */
static void
doarrow(struct cmd *cmd, char **filev)
{
	struct namelist *f;
	struct subcmd *sc;
	char **cpp;
	int n, ddir, destdir;
	volatile opt_t opts = options;
	struct namelist *files;
	struct subcmd *sbcmds;
	char *rhost;
	volatile int didupdate = 0;

        if (setjmp_ok) {
		error("reentrant call to doarrow");
		abort();
	}

	if (!cmd) {
		debugmsg(DM_MISC, "doarrow() NULL cmd parameter");
		return;
	}

	files = cmd->c_files;
	sbcmds = cmd->c_cmds;
	rhost = cmd->c_name;

	if (files == NULL) {
		error("No files to be updated on %s for target \"%s\"", 
		      rhost, cmd->c_label);
		return;
	}

	debugmsg(DM_CALL, "doarrow(%x, %s, %x) start", 
		 files, A(rhost), sbcmds);

	if (nflag)
		(void) printf("updating host %s\n", rhost);
	else {
		if (cmd->c_flags & CMD_CONNFAILED) {
			debugmsg(DM_MISC,
				 "makeconn %s failed before; skipping\n",
				 rhost);
			return;
		}

		if (setjmp(finish_jmpbuf)) {
			setjmp_ok = FALSE;
			debugmsg(DM_MISC, "setjmp to finish_jmpbuf");
			markfailed(cmd, cmds);
			return;
		}
		setjmp_ok = TRUE;

		if (!makeconn(rhost)) {
			setjmp_ok = FALSE;
			markfailed(cmd, cmds);
			return;
		}
	}

	subcmds = sbcmds;
	filelist = files;

	n = 0;
	for (sc = sbcmds; sc != NULL; sc = sc->sc_next) {
		if (sc->sc_type != INSTALL)
			continue;
		n++;
	/*
	 * destination is a directory if one of the following is true:
	 * a) more than one name specified on left side of -> directive
	 * b) basename of destination in "install" directive is "."
	 *    (e.g. install /tmp/.;)
	 * c) name on left side of -> directive is a directory on local system.
 	 *
 	 * We need 2 destdir flags (destdir and ddir) because single directory
 	 * source is handled differently.  In this case, ddir is 0 (which
 	 * tells install() not to send DIRTARGET directive to remote rdistd)
 	 * and destdir is 1 (which tells remfilename() how to build the FILE
 	 * variables correctly).  In every other case, destdir and ddir will
 	 * have the same value.
	 */
  	ddir = files->n_next != NULL;	/* destination is a directory */
	if (!ddir) {
		struct stat s;
 		int isadir = 0;

		if (lstat(files->n_name, &s) == 0)
 			isadir = S_ISDIR(s.st_mode);
 		if (!isadir && sc->sc_name && *sc->sc_name)
 			ddir = !strcmp(xbasename(sc->sc_name),".");
 		destdir = isadir | ddir;
 	} else
 		destdir = ddir;

	debugmsg(DM_MISC,
		 "Debug files->n_next= %d, destdir=%d, ddir=%d",
		 files->n_next, destdir, ddir);
 
	if (!sc->sc_name || !*sc->sc_name) {
		destdir = 0;
		ddir = 0;
	}

	debugmsg(DM_MISC,
		 "Debug sc->sc_name=%x, destdir=%d, ddir=%d",
		 sc->sc_name, destdir, ddir);

	for (f = files; f != NULL; f = f->n_next) {
		if (filev) {
			for (cpp = filev; *cpp; cpp++)
				if (strcmp(f->n_name, *cpp) == 0)
					goto found;
			continue;
		}
	found:
		if (install(f->n_name, sc->sc_name, ddir, destdir,
				sc->sc_options) > 0)
			++didupdate;
		opts = sc->sc_options;
	}

	} /* end loop for each INSTALL command */

	/* if no INSTALL commands present, do default install */
	if (!n) {
		for (f = files; f != NULL; f = f->n_next) {
			if (filev) {
				for (cpp = filev; *cpp; cpp++)
					if (strcmp(f->n_name, *cpp) == 0)
						goto found2;
				continue;
			}
		found2:
			/* ddir & destdir set to zero for default install */
			if (install(f->n_name, NULL, 0, 0, options) > 0)
				++didupdate;
		}
	}

	/*
	 * Run any commands for the entire cmd
	 */
	if (didupdate > 0) {
		runcmdspecial(cmd, opts);
		didupdate = 0;
	}

	if (!nflag)
		(void) signal(SIGPIPE, cleanup);

	for (sc = sbcmds; sc != NULL; sc = sc->sc_next)
		if (sc->sc_type == NOTIFY)
			notify(rhost, sc->sc_args, (time_t) 0);

	if (!nflag) {
		struct linkbuf *nextl, *l;

		for (l = ihead; l != NULL; freelinkinfo(l), l = nextl) {
			nextl = l->nextp;
			if (contimedout || IS_ON(opts, DO_IGNLNKS) || 
			    l->count == 0)
				continue;
			message(MT_WARNING, "%s: Warning: %d %s link%s",
				l->pathname, abs(l->count),	
				(l->count > 0) ? "missing" : "extra",
				(l->count == 1) ? "" : "s");
		}
		ihead = NULL;
	}
	setjmp_ok = FALSE;
}

int
okname(char *name)
{
	char *cp = name;
	int c, isbad;

	for (isbad = FALSE; *cp && !isbad; ++cp) {
		c = *cp;
		if (c & 0200)
			isbad = TRUE;
		if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
			isbad = TRUE;
	}

	if (isbad) {
		error("Invalid user name \"%s\"\n", name);
		return(0);
	}
	return(1);
}

static void
rcmptime(struct stat *st, struct subcmd *sbcmds, char **env)
{
	DIR *d;
	DIRENTRY *dp;
	char *cp;
	char *optarget;
	int len;

	debugmsg(DM_CALL, "rcmptime(%x) start", st);

	if ((d = opendir((char *) target)) == NULL) {
		error("%s: open directory failed: %s", target, SYSERR);
		return;
	}
	optarget = ptarget;
	len = ptarget - target;
	while ((dp = readdir(d)) != NULL) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (len + 1 + (int)strlen(dp->d_name) >= BUFSIZ - 1) {
			error("%s/%s: Name too long\n", target, dp->d_name);
			continue;
		}
		ptarget = optarget;
		*ptarget++ = '/';
		cp = dp->d_name;
		while ((*ptarget++ = *cp++) != '\0')
			;
		ptarget--;
		cmptime(target, sbcmds, env);
	}
	(void) closedir((DIR *) d);
	ptarget = optarget;
	*ptarget = '\0';
}

/*
 * Compare the mtime of file to the list of time stamps.
 */
static void
cmptime(char *name, struct subcmd *sbcmds, char **env)
{
	struct subcmd *sc;
	struct stat stb;

	debugmsg(DM_CALL, "cmptime(%s)", name);

	if (except(name))
		return;

	if (nflag) {
		(void) printf("comparing dates: %s\n", name);
		return;
	}

	/*
	 * first time cmptime() is called?
	 */
	if (ptarget == NULL) {
		if (exptilde(target, name, sizeof(target)) == NULL)
			return;
		ptarget = name = target;
		while (*ptarget)
			ptarget++;
	}
	if (access(name, R_OK) < 0 || stat(name, &stb) < 0) {
		error("%s: cannot access file: %s", name, SYSERR);
		return;
	}

	if (S_ISDIR(stb.st_mode)) {
		rcmptime(&stb, sbcmds, env);
		return;
	} else if (!S_ISREG(stb.st_mode)) {
		error("%s: not a plain file", name);
		return;
	}

	if (stb.st_mtime > lastmod) {
		message(MT_INFO, "%s: file is newer", name);
		for (sc = sbcmds; sc != NULL; sc = sc->sc_next) {
			char buf[BUFSIZ];
			if (sc->sc_type != SPECIAL)
				continue;
			if (sc->sc_args != NULL && !inlist(sc->sc_args, name))
				continue;
			(void) snprintf(buf, sizeof(buf), "%s=%s;%s", 
				        E_LOCFILE, name, sc->sc_name);
			message(MT_CHANGE, "special \"%s\"", buf);
			if (*env) {
				size_t len = strlen(*env) + strlen(name) + 2;
				*env = (char *) xrealloc(*env, len);
				(void) strlcat(*env, name, len);
				(void) strlcat(*env, ":", len);
			}
			if (IS_ON(options, DO_VERIFY))
				continue;

			runcommand(buf);
		}
	}
}

/*
 * Process commands for comparing files to time stamp files.
 */
static void
dodcolon(struct cmd *cmd, char **filev)
{
	struct subcmd *sc;
	struct namelist *f;
	char *cp, **cpp;
	struct stat stb;
	struct namelist *files = cmd->c_files;
	struct subcmd *sbcmds = cmd->c_cmds;
	char *env, *stamp = cmd->c_name;

	debugmsg(DM_CALL, "dodcolon()");

	if (files == NULL) {
		error("No files to be updated for target \"%s\"", 
		      cmd->c_label);
		return;
	}
	if (stat(stamp, &stb) < 0) {
		error("%s: stat failed: %s", stamp, SYSERR);
		return;
	}

	debugmsg(DM_MISC, "%s: mtime %d\n", stamp, stb.st_mtime);

	env = NULL;
	for (sc = sbcmds; sc != NULL; sc = sc->sc_next) {
		if (sc->sc_type == CMDSPECIAL) {
			env = (char *) xmalloc(sizeof(E_FILES) + 3);
			(void) snprintf(env, sizeof(E_FILES) + 3,
					"%s='", E_FILES);
			break;
		}
	}

	subcmds = sbcmds;
	filelist = files;

	lastmod = stb.st_mtime;
	if (!nflag && !IS_ON(options, DO_VERIFY))
		/*
		 * Set atime and mtime to current time
		 */
		(void) setfiletime(stamp, (time_t) 0, (time_t) 0);

	for (f = files; f != NULL; f = f->n_next) {
		if (filev) {
			for (cpp = filev; *cpp; cpp++)
				if (strcmp(f->n_name, *cpp) == 0)
					goto found;
			continue;
		}
	found:
		ptarget = NULL;
		cmptime(f->n_name, sbcmds, &env);
	}

	for (sc = sbcmds; sc != NULL; sc = sc->sc_next) {
		if (sc->sc_type == NOTIFY)
			notify(NULL, sc->sc_args, (time_t)lastmod);
		else if (sc->sc_type == CMDSPECIAL && env) {
			size_t len = strlen(env);
			if (env[len - 1] == ':')
				env[--len] = CNULL;
			len += 2 + strlen(sc->sc_name) + 1;
			env = xrealloc(env, len);
			(void) strlcat(env, "';", len);
			(void) strlcat(env, sc->sc_name, len);
			message(MT_CHANGE, "cmdspecial \"%s\"", env);
			if (!nflag && IS_OFF(options, DO_VERIFY))
				runcommand(env);
			(void) free(env);
			env = NULL;	/* so cmdspecial is only called once */
		}
	}
	if (!nflag && !IS_ON(options, DO_VERIFY) && (cp = getnotifyfile()))
		(void) unlink(cp);
}

/*
 * Return TRUE if file is in the exception list.
 */
int
except(char *file)
{
	struct	subcmd *sc;
	struct	namelist *nl;

	debugmsg(DM_CALL, "except(%s)", file);

	for (sc = subcmds; sc != NULL; sc = sc->sc_next) {
		if (sc->sc_type == EXCEPT) {
			for (nl = sc->sc_args; nl != NULL; nl = nl->n_next)
				if (strcmp(file, nl->n_name) == 0)
					return(1);
  			continue;
		}
		if (sc->sc_type == PATTERN) {
			for (nl = sc->sc_args; nl != NULL; nl = nl->n_next) {
				char ebuf[BUFSIZ];
				int ecode = 0;

				/* allocate and compile n_regex as needed */
				if (nl->n_regex == NULL) {
					nl->n_regex = (regex_t *)
					    xmalloc(sizeof(regex_t));
					ecode = regcomp(nl->n_regex, nl->n_name,
							REG_NOSUB);
				}
				if (ecode == 0) {
					ecode = regexec(nl->n_regex, file, 0,
					    NULL, 0);
				}
				switch (ecode) {
				case REG_NOMATCH:
					break;
				case 0:
					return(1);	/* match! */
				default:
					regerror(ecode, nl->n_regex, ebuf,
						 sizeof(ebuf));
					error("Regex error \"%s\" for \"%s\".",
					      ebuf, nl->n_name);
					return(0);
				}
			}
		}
	}
	return(0);
}

/*
 * Do a specific command for a specific host
 */
static void
docmdhost(struct cmd *cmd, char **filev)
{
	checkcmd(cmd);

	/*
	 * If we're multi-threaded and we're the parent, spawn a 
	 * new child process.
	 */
	if (do_fork && !amchild) {
		pid_t pid;

		/*
		 * If we're at maxchildren, wait for number of active
		 * children to fall below max number of children.
		 */
		while (activechildren >= maxchildren)
			waitup();

		pid = spawn(cmd, cmds);
		if (pid == 0)
			/* Child */
			amchild = 1;
		else
			/* Parent */
			return;
	}

	/*
	 * Disable NFS checks
	 */
	if (cmd->c_flags & CMD_NOCHKNFS)
		FLAG_OFF(options, DO_CHKNFS);

	if (!nflag) {
		currenthost = (cmd->c_name) ? cmd->c_name : "<unknown>";
#if	defined(SETARGS) || defined(HAVE_SETPROCTITLE)
		setproctitle("update %s", currenthost);
#endif 	/* SETARGS || HAVE_SETPROCTITLE */
	}

	switch (cmd->c_type) {
	case ARROW:
		doarrow(cmd, filev);
		break;
	case DCOLON:
		dodcolon(cmd, filev);
		break;
	default:
		fatalerr("illegal command type %d", cmd->c_type);
	}
}

/*
 * Do a specific command (cmd)
 */
static void
docmd(struct cmd *cmd, int argc, char **argv)
{
	struct namelist *f;
	int i;

	if (argc) {
		for (i = 0; i < argc; i++) {
			if (cmd->c_label != NULL &&
			    strcmp(cmd->c_label, argv[i]) == 0) {
				docmdhost(cmd, NULL);
				return;
			}
			for (f = cmd->c_files; f != NULL; f = f->n_next)
				if (strcmp(f->n_name, argv[i]) == 0) {
					docmdhost(cmd, &argv[i]);
					return;
				}
		}
	} else
		docmdhost(cmd, NULL);
}

/*
 *
 * Multiple hosts are updated at once via a "ring" of at most
 * maxchildren rdist processes.  The parent rdist fork()'s a child
 * for a given host.  That child will update the given target files
 * and then continue scanning through the remaining targets looking
 * for more work for a given host.  Meanwhile, the parent gets the
 * next target command and makes sure that it hasn't encountered
 * that host yet since the children are responsible for everything
 * for that host.  If no children have done this host, then check
 * to see if the number of active proc's is less than maxchildren.
 * If so, then spawn a new child for that host.  Otherwise, wait
 * for a child to finish.
 *
 */

/*
 * Do the commands in cmds (initialized by yyparse).
 */
void
docmds(struct namelist *hostlist, int argc, char **argv)
{
	struct cmd *c;
	char *cp;
	int i;

	(void) signal(SIGHUP, sighandler);
	(void) signal(SIGINT, sighandler);
	(void) signal(SIGQUIT, sighandler);
	(void) signal(SIGTERM, sighandler);

	if (!nflag)
		mysetlinebuf(stdout);	/* Make output (mostly) clean */

#if	defined(USE_STATDB)
	if (!nflag && (dostatdb || juststatdb)) {
		extern long reccount;
		message(MT_INFO, "Making stat database [%s] ... \n", 
			       gettimestr());
		if (mkstatdb() < 0)
			error("Warning: Make stat database failed.");
		message(MT_INFO,
			      "Stat database created: %d files stored [%s].\n",
			       reccount, gettimestr());
		if (juststatdb)
			return;
	}
#endif	/* USE_STATDB */

	/*
	 * Print errors for any command line targets we didn't find.
	 * If any errors are found, return to main() which will then exit.
	 */
	for (i = 0; i < argc; i++) {
		int found;

		for (found = FALSE, c = cmds; c != NULL; c = c->c_next) {
			if (c->c_label && argv[i] && 
			    strcmp(c->c_label, argv[i]) == 0) {
				found = TRUE;
				break;
			}
		}
		if (!found)
			error("Label \"%s\" is not defined in the distfile.", 
			      argv[i]);
	}
	if (nerrs)
		return;

	/*
	 * Main command loop.  Loop through all the commands.
	 */
	for (c = cmds; c != NULL; c = c->c_next) {
		checkcmd(c);
		if (do_fork) {
			/*
			 * Let the children take care of their assigned host
			 */
			if (amchild) {
				if (strcmp(c->c_name, currenthost) != 0)
					continue;
			} else if (c->c_flags & CMD_ASSIGNED) {
				/* This cmd has been previously assigned */
				debugmsg(DM_MISC, "prev assigned: %s\n",
					 c->c_name);
				continue;
			}
		}

		if (hostlist) {
			/* Do specific hosts as specified on command line */
			struct namelist *nlptr;

			for (nlptr = hostlist; nlptr; nlptr = nlptr->n_next)
				/*
				 * Try an exact match and then a match
				 * without '@' (if present).
				 */
				if ((strcmp(c->c_name, nlptr->n_name) == 0) ||
				    ((cp = strchr(c->c_name, '@')) &&
				     strcmp(++cp, nlptr->n_name) == 0))
					docmd(c, argc, argv);
			continue;
		} else
			/* Do all of the command */
			docmd(c, argc, argv);
	}

	if (do_fork) {
		/*
		 * We're multi-threaded, so do appropriate shutdown
		 * actions based on whether we're the parent or a child.
		 */
		if (amchild) {
			if (!IS_ON(options, DO_QUIET))
				message(MT_VERBOSE, "updating of %s finished", 
					currenthost);
			closeconn();
			cleanup(0);
			exit(nerrs);
		}

		/*
		 * Wait for all remaining active children to finish
		 */
		while (activechildren > 0) {
			debugmsg(DM_MISC, 
				 "Waiting for %d children to finish.\n",
				 activechildren);
			waitup();
		}
	} else if (!nflag) {
		/*
		 * We're single-threaded so close down current connection
		 */
		closeconn();
		cleanup(0);
	}
}
