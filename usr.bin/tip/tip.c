/*	$OpenBSD: tip.c,v 1.44 2010/06/29 23:32:52 nicm Exp $	*/
/*	$NetBSD: tip.c,v 1.13 1997/04/20 00:03:05 mellon Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

/*
 * tip - UNIX link to other systems
 *  tip [-v] [-speed] system-name
 * or
 *  cu phone-number [-s speed] [-l line]
 */

#include <sys/types.h>
#include <sys/socket.h>

#include "tip.h"
#include "pathnames.h"

int	disc = TTYDISC;		/* tip normally runs this way */
char	PNbuf[256];			/* This limits the size of a number */

static void	intprompt(int);
static void	tipin(void);
static int	escape(void);

int
main(int argc, char *argv[])
{
	char *sys = NULL, sbuf[12], *p;
	int i, pair[2];

	/* XXX preserve previous braindamaged behavior */
	setboolean(value(DC), 1);

	if (strcmp(__progname, "cu") == 0) {
		cumode = 1;
		cumain(argc, argv);
		goto cucommon;
	}

	if (argc > 4) {
		fprintf(stderr, "usage: tip [-nv] [-speed] [system-name]\n");
		exit(1);
	}
	if (!isatty(0)) {
		fprintf(stderr, "%s: must be interactive\n", __progname);
		exit(1);
	}

	for (; argc > 1; argv++, argc--) {
		if (argv[1][0] != '-')
			sys = argv[1];
		else switch (argv[1][1]) {

		case 'v':
			vflag++;
			break;

		case 'n':
			noesc++;
			break;

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			setnumber(value(BAUDRATE), atoi(&argv[1][1]));
			break;

		default:
			fprintf(stderr, "%s: %s, unknown option\n", __progname,
			    argv[1]);
			break;
		}
	}

	(void)signal(SIGINT, cleanup);
	(void)signal(SIGQUIT, cleanup);
	(void)signal(SIGHUP, cleanup);
	(void)signal(SIGTERM, cleanup);
	(void)signal(SIGCHLD, SIG_DFL);

	if ((i = hunt(sys)) == 0) {
		printf("all ports busy\n");
		exit(3);
	}
	if (i == -1) {
		printf("link down\n");
		(void)uu_unlock(uucplock);
		exit(3);
	}
	setbuf(stdout, NULL);
	loginit();
	vinit();				/* init variables */
	setparity("none");			/* set the parity table */

	if (ttysetup(number(value(BAUDRATE)))) {
		fprintf(stderr, "%s: bad baud rate %ld\n", __progname,
		    number(value(BAUDRATE)));
		(void)uu_unlock(uucplock);
		exit(3);
	}
	con();

cucommon:
	/*
	 * From here down the code is shared with
	 * the "cu" version of tip.
	 */

	i = fcntl(FD, F_GETFL);
	if (i == -1) {
		perror("fcntl");
		cleanup(0);
	}
	i = fcntl(FD, F_SETFL, i & ~O_NONBLOCK);
	if (i == -1) {
		perror("fcntl");
		cleanup(0);
	}

	tcgetattr(0, &defterm);
	gotdefterm = 1;
	term = defterm;
	term.c_lflag &= ~(ICANON|IEXTEN|ECHO);
	term.c_iflag &= ~(INPCK|ICRNL);
	term.c_oflag &= ~OPOST;
	term.c_cc[VMIN] = 1;
	term.c_cc[VTIME] = 0;
	defchars = term;
	term.c_cc[VINTR] = term.c_cc[VQUIT] = term.c_cc[VSUSP] =
	    term.c_cc[VDSUSP] = term.c_cc[VDISCARD] =
	    term.c_cc[VLNEXT] = _POSIX_VDISABLE;
	raw();

	(void)signal(SIGALRM, timeout);

	if (value(LINEDISC) != TTYDISC) {
		int ld = (int)value(LINEDISC);
		ioctl(FD, TIOCSETD, &ld);
	}		

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pair) != 0) {
		(void)uu_unlock(uucplock);
		err(3, "socketpair");
	}

	/*
	 * Everything's set up now:
	 *	connection established (hardwired or dialup)
	 *	line conditioned (baud rate, mode, etc.)
	 *	internal data structures (variables)
	 * so, fork one process for local side and one for remote.
	 */
	printf(cumode ? "Connected\r\n" : "\07connected\r\n");
	tipin_pid = getpid();
	switch (tipout_pid = fork()) {
	case -1:
		(void)uu_unlock(uucplock);
		err(3, "fork");
	case 0:
		close(pair[1]);
		tipin_fd = pair[0];
		tipout();
	default:
		close(pair[0]);
		tipout_fd = pair[1];
		tipin();
	}
	/*NOTREACHED*/
	exit(0);
}

void
con(void)
{
	if (CM != NULL)
		parwrite(FD, CM, size(CM));
	logent(value(HOST), DV, "call completed");
}

void
cleanup(int signo)
{
	(void)uu_unlock(uucplock);
	if (odisc)
		ioctl(0, TIOCSETD, &odisc);
	unraw();
	if (signo && tipout_pid) {
		kill(tipout_pid, signo);
		wait(NULL);
	}
	exit(0);
}

/*
 * put the controlling keyboard into raw mode
 */
void
raw(void)
{
	tcsetattr(0, TCSADRAIN, &term);
}


/*
 * return keyboard to normal mode
 */
void
unraw(void)
{
	if (gotdefterm)
		tcsetattr(0, TCSADRAIN, &defterm);
}

static	jmp_buf promptbuf;

/*
 * Print string ``s'', then read a string
 *  in from the terminal.  Handles signals & allows use of
 *  normal erase and kill characters.
 */
int
prompt(char *s, char *p, size_t sz)
{
	int c;
	char *b = p;
	sig_t oint, oquit;

	stoprompt = 0;
	oint = signal(SIGINT, intprompt);
	oquit = signal(SIGQUIT, SIG_IGN);
	unraw();
	printf("%s", s);
	if (setjmp(promptbuf) == 0)
		while ((c = getchar()) != EOF && (*p = c) != '\n' && --sz > 0)
			p++;
	*p = '\0';

	raw();
	(void)signal(SIGINT, oint);
	(void)signal(SIGQUIT, oquit);
	return (stoprompt || p == b);
}

/*
 * Interrupt service routine during prompting
 */
/*ARGSUSED*/
static void
intprompt(int signo)
{
	(void)signal(SIGINT, SIG_IGN);
	stoprompt = 1;
	printf("\r\n");
	longjmp(promptbuf, 1);
}

/*
 * ****TIPIN   TIPIN****
 */
static void
tipin(void)
{
	int bol = 1;
	int gch;
	char ch;

	/*
	 * Kinda klugey here...
	 *   check for scripting being turned on from the .tiprc file,
	 *   but be careful about just using setscript(), as we may
	 *   send a SIGEMT before tipout has a chance to set up catching
	 *   it; so wait a second, then setscript()
	 */
	if (boolean(value(SCRIPT))) {
		sleep(1);
		setscript();
	}

	while (1) {
		gch = getchar()&STRIP_PAR;
		/* XXX does not check for EOF */
		if ((gch == character(value(ESCAPE))) && bol) {
			if (!noesc) {
				if (!(gch = escape()))
					continue;
			}
		} else if (!cumode && gch == character(value(RAISECHAR))) {
			setboolean(value(RAISE), !boolean(value(RAISE)));
			continue;
		} else if (gch == '\r') {
			bol = 1;
			ch = gch;
			parwrite(FD, &ch, 1);
			if (boolean(value(HALFDUPLEX)))
				printf("\r\n");
			continue;
		} else if (!cumode && gch == character(value(FORCE)))
			gch = getchar()&STRIP_PAR;
		bol = any(gch, value(EOL));
		if (boolean(value(RAISE)) && islower(gch))
			gch = toupper(gch);
		ch = gch;
		parwrite(FD, &ch, 1);
		if (boolean(value(HALFDUPLEX)))
			printf("%c", ch);
	}
}

extern esctable_t etable[];

/*
 * Escape handler --
 *  called on recognition of ``escapec'' at the beginning of a line
 */
static int
escape(void)
{
	int gch;
	esctable_t *p;
	char c = character(value(ESCAPE));

	gch = (getchar()&STRIP_PAR);
	/* XXX does not check for EOF */
	for (p = etable; p->e_char; p++)
		if (p->e_char == gch) {
			printf("%s", ctrl(c));
			(*p->e_func)(gch);
			return (0);
		}
	/* ESCAPE ESCAPE forces ESCAPE */
	if (c != gch)
		parwrite(FD, &c, 1);
	return (gch);
}

int
any(int cc, char *p)
{
	char c = cc;
	while (p && *p)
		if (*p++ == c)
			return (1);
	return (0);
}

size_t
size(char *s)
{
	size_t i = 0;

	while (s && *s++)
		i++;
	return (i);
}

char *
interp(char *s)
{
	static char buf[256];
	char *p = buf, c, *q;

	while ((c = *s++)) {
		for (q = "\nn\rr\tt\ff\033E\bb"; *q; q++)
			if (*q++ == c) {
				*p++ = '\\'; *p++ = *q;
				goto next;
			}
		if (c < 040) {
			*p++ = '^'; *p++ = c + 'A'-1;
		} else if (c == 0177) {
			*p++ = '^'; *p++ = '?';
		} else
			*p++ = c;
	next:
		;
	}
	*p = '\0';
	return (buf);
}

char *
ctrl(char c)
{
	static char s[3];

	if (c < 040 || c == 0177) {
		s[0] = '^';
		s[1] = c == 0177 ? '?' : c+'A'-1;
		s[2] = '\0';
	} else {
		s[0] = c;
		s[1] = '\0';
	}
	return (s);
}

/*
 * Help command
 */
void
help(int c)
{
	esctable_t *p;

	printf("%c\r\n", c);
	for (p = etable; p->e_char; p++) {
		printf("%2s", ctrl(character(value(ESCAPE))));
		printf("%-2s     %s\r\n", ctrl(p->e_char), p->e_help);
	}
}

/*
 * Set up the "remote" tty's state
 */
int
ttysetup(int speed)
{
	struct termios	cntrl;

	if (tcgetattr(FD, &cntrl))
		return (-1);
	cfsetspeed(&cntrl, speed);
	cntrl.c_cflag &= ~(CSIZE|PARENB);
	cntrl.c_cflag |= CS8;
	if (boolean(value(DC)))
		cntrl.c_cflag |= CLOCAL;
	if (boolean(value(HARDWAREFLOW)))
		cntrl.c_cflag |= CRTSCTS;
	cntrl.c_iflag &= ~(ISTRIP|ICRNL);
	cntrl.c_oflag &= ~OPOST;
	cntrl.c_lflag &= ~(ICANON|ISIG|IEXTEN|ECHO);
	cntrl.c_cc[VMIN] = 1;
	cntrl.c_cc[VTIME] = 0;
	if (boolean(value(TAND)))
		cntrl.c_iflag |= IXOFF;
	return (tcsetattr(FD, TCSAFLUSH, &cntrl));
}

static char partab[0200];

/*
 * Do a write to the remote machine with the correct parity.
 * We are doing 8 bit wide output, so we just generate a character
 * with the right parity and output it.
 */
void
parwrite(int fd, char *buf, size_t n)
{
	int i;
	char *bp;

	bp = buf;
	if (bits8 == 0)
		for (i = 0; i < n; i++) {
			*bp = partab[(*bp) & 0177];
			bp++;
		}
	if (write(fd, buf, n) < 0) {
		if (errno == EIO)
			tipabort("Lost carrier.");
		/* this is questionable */
		perror("write");
	}
}

/*
 * Build a parity table with appropriate high-order bit.
 */
void
setparity(char *defparity)
{
	int i, flip, clr, set;
	char *parity;
	extern const unsigned char evenpartab[];

	if (value(PARITY) == NULL)
		value(PARITY) = defparity;
	parity = value(PARITY);
	if (strcmp(parity, "none") == 0) {
		bits8 = 1;
		return;
	}
	bits8 = 0;
	flip = 0;
	clr = 0377;
	set = 0;
	if (strcmp(parity, "odd") == 0)
		flip = 0200;			/* reverse bit 7 */
	else if (strcmp(parity, "zero") == 0)
		clr = 0177;			/* turn off bit 7 */
	else if (strcmp(parity, "one") == 0)
		set = 0200;			/* turn on bit 7 */
	else if (strcmp(parity, "even") != 0) {
		(void) fprintf(stderr, "%s: unknown parity value\r\n", parity);
		(void) fflush(stderr);
	}
	for (i = 0; i < 0200; i++)
		partab[i] = ((evenpartab[i] ^ flip) | set) & clr;
}
