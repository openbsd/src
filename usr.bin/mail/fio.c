/*	$OpenBSD: fio.c,v 1.34 2014/12/16 18:31:06 millert Exp $	*/
/*	$NetBSD: fio.c,v 1.8 1997/07/07 22:57:55 phil Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include "rcv.h"
#include <sys/file.h>
#include <sys/wait.h>

#include <unistd.h>
#include <paths.h>
#include <errno.h>
#include <glob.h>
#include "extern.h"

/*
 * Mail -- a mail program
 *
 * File I/O.
 */

static volatile sig_atomic_t fiosignal;

/*
 * Wrapper for read() to catch EINTR.
 */
static ssize_t
myread(int fd, char *buf, int len)
{
	ssize_t nread;

	while ((nread = read(fd, buf, len)) == -1 && errno == EINTR)
		;
	return(nread);
}

/*
 * Set up the input pointers while copying the mail file into /tmp.
 */
void
setptr(FILE *ibuf, off_t offset)
{
	int c, count;
	char *cp, *cp2;
	struct message this;
	FILE *mestmp;
	int maybe, inhead, omsgCount;
	char linebuf[LINESIZE], pathbuf[PATHSIZE];

	/* Get temporary file. */
	(void)snprintf(pathbuf, sizeof(pathbuf), "%s/mail.XXXXXXXXXX", tmpdir);
	if ((c = mkstemp(pathbuf)) == -1 || (mestmp = Fdopen(c, "r+")) == NULL)
		err(1, "can't open %s", pathbuf);
	(void)rm(pathbuf);

	if (offset == 0) {
		msgCount = 0;
	} else {
		/* Seek into the file to get to the new messages */
		(void)fseeko(ibuf, offset, SEEK_SET);
		/*
		 * We need to make "offset" a pointer to the end of
		 * the temp file that has the copy of the mail file.
		 * If any messages have been edited, this will be
		 * different from the offset into the mail file.
		 */
		(void)fseeko(otf, (off_t)0, SEEK_END);
		offset = ftell(otf);
	}
	omsgCount = msgCount;
	maybe = 1;
	inhead = 0;
	this.m_flag = MUSED|MNEW;
	this.m_size = 0;
	this.m_lines = 0;
	this.m_block = 0;
	this.m_offset = 0;
	for (;;) {
		if (fgets(linebuf, sizeof(linebuf), ibuf) == NULL) {
			if (append(&this, mestmp))
				err(1, "temporary file");
			makemessage(mestmp, omsgCount);
			return;
		}
		count = strlen(linebuf);
		/*
		 * Transforms lines ending in <CR><LF> to just <LF>.
		 * This allows mail to be able to read Eudora mailboxes
		 * that reside on a DOS partition.
		 */
		if (count >= 2 && linebuf[count-1] == '\n' &&
		    linebuf[count - 2] == '\r') {
			linebuf[count - 2] = '\n';
			linebuf[count - 1] = '\0';
			count--;
		}

		(void)fwrite(linebuf, sizeof(*linebuf), count, otf);
		if (ferror(otf))
			err(1, "%s", pathbuf);
		if (count && linebuf[count - 1] == '\n')
			linebuf[count - 1] = '\0';
		if (maybe && linebuf[0] == 'F' && ishead(linebuf)) {
			msgCount++;
			if (append(&this, mestmp))
				err(1, "temporary file");
			this.m_flag = MUSED|MNEW;
			this.m_size = 0;
			this.m_lines = 0;
			this.m_block = blockof(offset);
			this.m_offset = offsetof(offset);
			inhead = 1;
		} else if (linebuf[0] == 0) {
			inhead = 0;
		} else if (inhead) {
			for (cp = linebuf, cp2 = "status";; cp++) {
				if ((c = (unsigned char)*cp2++) == 0) {
					while (isspace(*cp++))
						;
					if (cp[-1] != ':')
						break;
					while ((c = (unsigned char)*cp++) != '\0')
						if (c == 'R')
							this.m_flag |= MREAD;
						else if (c == 'O')
							this.m_flag &= ~MNEW;
					inhead = 0;
					break;
				}
				if (*cp != c && *cp != toupper(c))
					break;
			}
		}
		offset += count;
		this.m_size += count;
		this.m_lines++;
		maybe = linebuf[0] == 0;
	}
}

/*
 * Drop the passed line onto the passed output buffer.
 * If a write error occurs, return -1, else the count of
 * characters written, including the newline if requested.
 */
int
putline(FILE *obuf, char *linebuf, int outlf)
{
	int c;

	c = strlen(linebuf);
	(void)fwrite(linebuf, sizeof(*linebuf), c, obuf);
	if (outlf) {
		(void)putc('\n', obuf);
		c++;
	}
	if (ferror(obuf))
		return(-1);
	return(c);
}

/*
 * Read up a line from the specified input into the line
 * buffer.  Return the number of characters read.  Do not
 * include the newline (or carriage return) at the end.
 */
int
readline(FILE *ibuf, char *linebuf, int linesize, int *signo)
{
	struct sigaction act;
	struct sigaction savetstp;
	struct sigaction savettou;
	struct sigaction savettin;
	struct sigaction saveint;
	struct sigaction savehup;
	sigset_t oset;
	int n;

	/*
	 * Setup signal handlers if the caller asked us to catch signals.
	 * Note that we do not restart system calls since we need the
	 * read to be interruptible.
	 */
	if (signo) {
		fiosignal = 0;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = fioint;
		if (sigaction(SIGINT, NULL, &saveint) == 0 &&
		    saveint.sa_handler != SIG_IGN) {
			(void)sigaction(SIGINT, &act, &saveint);
			(void)sigprocmask(SIG_UNBLOCK, &intset, &oset);
		}
		if (sigaction(SIGHUP, NULL, &savehup) == 0 &&
		    savehup.sa_handler != SIG_IGN)
			(void)sigaction(SIGHUP, &act, &savehup);
		(void)sigaction(SIGTSTP, &act, &savetstp);
		(void)sigaction(SIGTTOU, &act, &savettou);
		(void)sigaction(SIGTTIN, &act, &savettin);
	}

	clearerr(ibuf);
	if (fgets(linebuf, linesize, ibuf) == NULL) {
		if (ferror(ibuf))
			clearerr(ibuf);
		n = -1;
	} else {
		n = strlen(linebuf);
		if (n > 0 && linebuf[n - 1] == '\n')
			linebuf[--n] = '\0';
		if (n > 0 && linebuf[n - 1] == '\r')
			linebuf[--n] = '\0';
	}

	if (signo) {
		(void)sigprocmask(SIG_SETMASK, &oset, NULL);
		(void)sigaction(SIGINT, &saveint, NULL);
		(void)sigaction(SIGHUP, &savehup, NULL);
		(void)sigaction(SIGTSTP, &savetstp, NULL);
		(void)sigaction(SIGTTOU, &savettou, NULL);
		(void)sigaction(SIGTTIN, &savettin, NULL);
		*signo = fiosignal;
	}

	return(n);
}

/*
 * Return a file buffer all ready to read up the
 * passed message pointer.
 */
FILE *
setinput(struct message *mp)
{

	fflush(otf);
	if (fseek(itf, (long)positionof(mp->m_block, mp->m_offset), SEEK_SET)
	    < 0)
		err(1, "fseek");
	return(itf);
}

/*
 * Take the data out of the passed ghost file and toss it into
 * a dynamically allocated message structure.
 */
void
makemessage(FILE *f, int omsgCount)
{
	size_t size;
	struct message *nmessage;

	size = (msgCount + 1) * sizeof(struct message);
	nmessage = (struct message *)realloc(message, size);
	if (nmessage == 0)
		errx(1, "Insufficient memory for %d messages",
		    msgCount);
	if (omsgCount == 0 || message == NULL)
		dot = nmessage;
	else
		dot = nmessage + (dot - message);
	message = nmessage;
	size -= (omsgCount + 1) * sizeof(struct message);
	fflush(f);
	(void)lseek(fileno(f), (off_t)sizeof(*message), SEEK_SET);
	if (myread(fileno(f), (void *) &message[omsgCount], size) != size)
		errx(1, "Message temporary file corrupted");
	message[msgCount].m_size = 0;
	message[msgCount].m_lines = 0;
	(void)Fclose(f);
}

/*
 * Append the passed message descriptor onto the temp file.
 * If the write fails, return 1, else 0
 */
int
append(struct message *mp, FILE *f)
{

	return(fwrite((char *) mp, sizeof(*mp), 1, f) != 1);
}

/*
 * Delete or truncate a file, but only if the file is a plain file.
 */
int
rm(char *name)
{
	struct stat sb;

	if (stat(name, &sb) < 0)
		return(-1);
	if (!S_ISREG(sb.st_mode)) {
		errno = EISDIR;
		return(-1);
	}
	if (unlink(name) == -1) {
		if (errno == EPERM)
			return(truncate(name, (off_t)0));
		else
			return(-1);
	}
	return(0);
}

static int sigdepth;		/* depth of holdsigs() */
static sigset_t nset, oset;
/*
 * Hold signals SIGHUP, SIGINT, and SIGQUIT.
 */
void
holdsigs(void)
{

	if (sigdepth++ == 0) {
		sigemptyset(&nset);
		sigaddset(&nset, SIGHUP);
		sigaddset(&nset, SIGINT);
		sigaddset(&nset, SIGQUIT);
		sigprocmask(SIG_BLOCK, &nset, &oset);
	}
}

/*
 * Release signals SIGHUP, SIGINT, and SIGQUIT.
 */
void
relsesigs(void)
{

	if (--sigdepth == 0)
		sigprocmask(SIG_SETMASK, &oset, NULL);
}

/*
 * Unblock and ignore a signal
 */
int
ignoresig(int sig, struct sigaction *oact, sigset_t *oset)
{
	struct sigaction act;
	sigset_t nset;
	int error;

	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	act.sa_handler = SIG_IGN;
	error = sigaction(sig, &act, oact);

	if (error == 0) {
		sigemptyset(&nset);
		sigaddset(&nset, sig);
		(void)sigprocmask(SIG_UNBLOCK, &nset, oset);
	} else if (oset != NULL)
		(void)sigprocmask(SIG_BLOCK, NULL, oset);

	return(error);
}

/*
 * Determine the size of the file possessed by
 * the passed buffer.
 */
off_t
fsize(FILE *iob)
{
	struct stat sbuf;

	if (fstat(fileno(iob), &sbuf) < 0)
		return(0);
	return(sbuf.st_size);
}

/*
 * Evaluate the string given as a new mailbox name.
 * Supported meta characters:
 *	%	for my system mail box
 *	%user	for user's system mail box
 *	#	for previous file
 *	&	invoker's mbox file
 *	+file	file in folder directory
 *	any shell meta character
 * Return the file name as a dynamic string.
 */
char *
expand(char *name)
{
	const int flags = GLOB_BRACE|GLOB_TILDE|GLOB_NOSORT;
	char xname[PATHSIZE];
	char cmdbuf[PATHSIZE];		/* also used for file names */
	char *match = NULL;
	glob_t names;

	/*
	 * The order of evaluation is "%" and "#" expand into constants.
	 * "&" can expand into "+".  "+" can expand into shell meta characters.
	 * Shell meta characters expand into constants.
	 * This way, we make no recursive expansion.
	 */
	switch (*name) {
	case '%':
		findmail(name[1] ? name + 1 : myname, xname, sizeof(xname));
		return(savestr(xname));
	case '#':
		if (name[1] != 0)
			break;
		if (prevfile[0] == 0) {
			puts("No previous file");
			return(NULL);
		}
		return(savestr(prevfile));
	case '&':
		if (name[1] == 0 && (name = value("MBOX")) == NULL)
			name = "~/mbox";
		/* fall through */
	}
	if (name[0] == '+' && getfold(cmdbuf, sizeof(cmdbuf)) >= 0) {
		(void)snprintf(xname, sizeof(xname), "%s/%s", cmdbuf, name + 1);
		name = savestr(xname);
	}
	/* catch the most common shell meta character */
	if (name[0] == '~' && homedir && (name[1] == '/' || name[1] == '\0')) {
		(void)snprintf(xname, sizeof(xname), "%s%s", homedir, name + 1);
		name = savestr(xname);
	}
	if (strpbrk(name, "~{[*?\\") == NULL)
		return(savestr(name));

	/* XXX - does not expand enviroment variables. */
	switch (glob(name, flags, NULL, &names)) {
	case 0:
		if (names.gl_pathc == 1)
			match = savestr(names.gl_pathv[0]);
		else
			fprintf(stderr, "\"%s\": Ambiguous.\n", name);
		break;
	case GLOB_NOSPACE:
		fprintf(stderr, "\"%s\": Out of memory.\n", name);
		break;
	case GLOB_NOMATCH:
		fprintf(stderr, "\"%s\": No match.\n", name);
		break;
	default:
		fprintf(stderr, "\"%s\": Expansion failed.\n", name);
		break;
	}
	globfree(&names);
	return(match);
}

/*
 * Determine the current folder directory name.
 */
int
getfold(char *name, int namelen)
{
	char *folder;

	if ((folder = value("folder")) == NULL)
		return(-1);
	if (*folder == '/')
		strlcpy(name, folder, namelen);
	else
		(void)snprintf(name, namelen, "%s/%s", homedir ? homedir : ".",
		    folder);
	return(0);
}

/*
 * Return the name of the dead.letter file.
 */
char *
getdeadletter(void)
{
	char *cp;

	if ((cp = value("DEAD")) == NULL || (cp = expand(cp)) == NULL)
		cp = expand("~/dead.letter");
	else if (*cp != '/') {
		char buf[PATHSIZE];

		(void)snprintf(buf, sizeof(buf), "~/%s", cp);
		cp = expand(buf);
	}
	return(cp);
}

/*
 * Signal handler used by readline() to catch SIGINT, SIGHUP, SIGTSTP,
 * SIGTTOU, SIGTTIN.
 */
void
fioint(int s)
{

	fiosignal = s;
}
