/*	$OpenBSD: readpassphrase.c,v 1.8 2001/12/06 05:20:50 millert Exp $	*/

/*
 * Copyright (c) 2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: readpassphrase.c,v 1.8 2001/12/06 05:20:50 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <readpassphrase.h>

/* Shared with signal handler below to restore state. */
static struct termios term, oterm;
static struct sigaction sa, saveint, savehup, savequit, saveterm, savetstp;
static int input;
static volatile sig_atomic_t susp;

static void handler(int);

char *
readpassphrase(const char *prompt, char *buf, size_t bufsiz, int flags)
{
	char ch, *p, *end;
	int output;

	/* I suppose we could alloc on demand in this case (XXX). */
	if (bufsiz == 0) {
		errno = EINVAL;
		return(NULL);
	}

	/*
	 * Read and write to /dev/tty if available.  If not, read from
	 * stdin and write to stderr unless a tty is required.
	 */
	if ((input = output = open(_PATH_TTY, O_RDWR)) == -1) {
		if (flags & RPP_REQUIRE_TTY) {
			errno = ENOTTY;
			return(NULL);
		}
		input = STDIN_FILENO;
		output = STDERR_FILENO;
	}

	/*
	 * Catch signals that would otherwise cause the user to end
	 * up with echo turned off in the shell.  Don't worry about
	 * things like SIGALRM and SIGPIPE for now.
	 */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = handler;
	(void)sigaction(SIGINT, &sa, &saveint);
	(void)sigaction(SIGHUP, &sa, &savehup);
	(void)sigaction(SIGQUIT, &sa, &savequit);
	(void)sigaction(SIGTERM, &sa, &saveterm);
	sa.sa_flags = 0;	/* don't restart for SIGTSTP */
	(void)sigaction(SIGTSTP, &sa, &savetstp);

	/* Turn off echo if possible. */
	if (tcgetattr(input, &oterm) == 0) {
		memcpy(&term, &oterm, sizeof(term));
		if (!(flags & RPP_ECHO_ON) && (term.c_lflag & ECHO))
			term.c_lflag &= ~ECHO;
		if (term.c_cc[VSTATUS] != _POSIX_VDISABLE)
			term.c_cc[VSTATUS] = _POSIX_VDISABLE;
		(void)tcsetattr(input, TCSAFLUSH|TCSASOFT, &term);
	} else {
		memset(&term, 0, sizeof(term));
		memset(&oterm, 0, sizeof(oterm));
	}

redo:
	(void)write(output, prompt, strlen(prompt));
	end = buf + bufsiz - 1;
	for (p = buf; read(input, &ch, 1) == 1 && ch != '\n' && ch != '\r';) {
		if (p < end) {
			if ((flags & RPP_SEVENBIT))
				ch &= 0x7f;
			if (isalpha(ch)) {
				if ((flags & RPP_FORCELOWER))
					ch = tolower(ch);
				if ((flags & RPP_FORCEUPPER))
					ch = toupper(ch);
			}
			*p++ = ch;
		}
	}
	if (susp) {
		/* Back from suspend. */
		susp = 0;
		goto redo;
	}
	*p = '\0';
	if (!(term.c_lflag & ECHO))
		(void)write(output, "\n", 1);

	/* Restore old terminal settings and signals. */
	if (memcmp(&term, &oterm, sizeof(term)) != 0)
		(void)tcsetattr(input, TCSAFLUSH|TCSASOFT, &oterm);
	(void)sigaction(SIGINT, &saveint, NULL);
	(void)sigaction(SIGHUP, &savehup, NULL);
	(void)sigaction(SIGQUIT, &savequit, NULL);
	(void)sigaction(SIGTERM, &saveterm, NULL);
	(void)sigaction(SIGTSTP, &savetstp, NULL);
	if (input != STDIN_FILENO)
		(void)close(input);
	return(buf);
}

char *
getpass(const char *prompt)
{
	static char buf[_PASSWORD_LEN + 1];

	return(readpassphrase(prompt, buf, sizeof(buf), RPP_ECHO_OFF));
}

static void handler(int signo)
{
	struct sigaction osa;
	sigset_t nset;
	int save_errno;

	save_errno = errno;

	/* Restore tty modes */
	if (memcmp(&term, &oterm, sizeof(term)) != 0)
		(void)tcsetattr(input, TCSANOW|TCSASOFT, &oterm);

	/*
	 * Save old handler and set to original value.
	 * Unblock receipt of 'signo' and resend the signal so that
	 * it is caught by the pre-readpassphrase handler.
	 */
	switch (signo) {
	case SIGINT:
		(void)sigaction(signo, &saveint, &osa);
		break;
	case SIGHUP:
		(void)sigaction(signo, &savehup, &osa);
		break;
	case SIGQUIT:
		(void)sigaction(signo, &savequit, &osa);
		break;
	case SIGTERM:
		(void)sigaction(signo, &saveterm, &osa);
		break;
	case SIGTSTP:
		(void)sigaction(signo, &savetstp, &osa);
		susp = 1;
		break;
	}
	(void)sigemptyset(&nset);
	(void)sigaddset(&nset, signo);
	(void)sigprocmask(SIG_UNBLOCK, &nset, NULL);
	(void)kill(getpid(), signo);
	(void)sigprocmask(SIG_BLOCK, &nset, NULL);
	(void)sigaction(signo, &osa, NULL);

	/* Put tty modes back */
	if (memcmp(&term, &oterm, sizeof(term)) != 0)
		(void)tcsetattr(input, TCSANOW|TCSASOFT, &term);

	errno = save_errno;
}
