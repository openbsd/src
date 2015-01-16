/*	$OpenBSD: commands.c,v 1.71 2015/01/16 06:40:13 deraadt Exp $	*/
/*	$NetBSD: commands.c,v 1.14 1996/03/24 22:03:48 jtk Exp $	*/

/*
 * Copyright (c) 1988, 1990, 1993
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

#include "telnet_locl.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#ifdef SKEY
#include <sys/wait.h>
#define PATH_SKEY	"/usr/bin/skey"
#endif

static unsigned long sourceroute(char *arg, char **cpp, int *lenp);

int tos = -1;

char	*hostname;

typedef struct {
	char	*name;		/* command name */
	char	*help;		/* help string (NULL for no help) */
	int	(*handler)(int, char **);/* routine which executes command */
	int	needconnect;	/* Do we need to be connected to execute? */
} Command;

static char line[256];
static char saveline[256];
static int margc;
static char *margv[20];

#ifdef SKEY
int
skey_calc(int argc, char **argv)
{
	int status;

	if(argc != 3) {
		printf("usage: %s sequence challenge\n", argv[0]);
		return 0;
	}

	switch(fork()) {
	case 0:
		execv(PATH_SKEY, argv);
		exit (1);
	case -1:
		err(1, "fork");
		break;
	default:
		(void) wait(&status);
		if (WIFEXITED(status))
			return (WEXITSTATUS(status));
		return (0);
	}
}
#endif

static void
makeargv(void)
{
    char *cp, *cp2, c;
    char **argp = margv;

    margc = 0;
    cp = line;
    if (*cp == '!') {		/* Special case shell escape */
	strlcpy(saveline, line, sizeof(saveline)); /* save for shell command */
	*argp++ = "!";		/* No room in string to get this */
	margc++;
	cp++;
    }
    while ((c = *cp)) {
	int inquote = 0;
	while (isspace((unsigned char)c))
	    c = *++cp;
	if (c == '\0')
	    break;
	*argp++ = cp;
	margc += 1;
	for (cp2 = cp; c != '\0'; c = *++cp) {
	    if (inquote) {
		if (c == inquote) {
		    inquote = 0;
		    continue;
		}
	    } else {
		if (c == '\\') {
		    if ((c = *++cp) == '\0')
			break;
		} else if (c == '"') {
		    inquote = '"';
		    continue;
		} else if (c == '\'') {
		    inquote = '\'';
		    continue;
		} else if (isspace((unsigned char)c))
		    break;
	    }
	    *cp2++ = c;
	}
	*cp2 = '\0';
	if (c == '\0')
	    break;
	cp++;
    }
    *argp++ = 0;
}

/*
 * Make a character string into a number.
 *
 * Todo:  1.  Could take random integers (12, 0x12, 012, 0b1).
 */

static char
special(char *s)
{
	char c;
	char b;

	switch (*s) {
	case '^':
		b = *++s;
		if (b == '?') {
		    c = b | 0x40;		/* DEL */
		} else {
		    c = b & 0x1f;
		}
		break;
	default:
		c = *s;
		break;
	}
	return c;
}

/*
 * Construct a control character sequence
 * for a special character.
 */
static char *
control(cc_t c)
{
	static char buf[5];
	/*
	 * The only way I could get the Sun 3.5 compiler
	 * to shut up about
	 *	if ((unsigned int)c >= 0x80)
	 * was to assign "c" to an unsigned int variable...
	 * Arggg....
	 */
	unsigned int uic = (unsigned int)c;

	if (uic == 0x7f)
		return ("^?");
	if (c == (cc_t)_POSIX_VDISABLE) {
		return "off";
	}
	if (uic >= 0x80) {
		buf[0] = '\\';
		buf[1] = ((c>>6)&07) + '0';
		buf[2] = ((c>>3)&07) + '0';
		buf[3] = (c&07) + '0';
		buf[4] = 0;
	} else if (uic >= 0x20) {
		buf[0] = c;
		buf[1] = 0;
	} else {
		buf[0] = '^';
		buf[1] = '@'+c;
		buf[2] = 0;
	}
	return (buf);
}

/*
 *	The following are data structures and routines for
 *	the "send" command.
 *
 */

struct sendlist {
    char	*name;		/* How user refers to it (case independent) */
    char	*help;		/* Help information (0 ==> no help) */
    int		needconnect;	/* Need to be connected */
    int		narg;		/* Number of arguments */
    int		(*handler)();	/* Routine to perform (for special ops) */
    int		nbyte;		/* Number of bytes to send this command */
    int		what;		/* Character to be sent (<0 ==> special) */
};


static int
	send_esc(void),
	send_help(void),
	send_docmd(char *),
	send_dontcmd(char *),
	send_willcmd(char *),
	send_wontcmd(char *);

static struct sendlist Sendlist[] = {
    { "ao",	"Send Telnet Abort output",		1, 0, 0, 2, AO },
    { "ayt",	"Send Telnet 'Are You There'",		1, 0, 0, 2, AYT },
    { "brk",	"Send Telnet Break",			1, 0, 0, 2, BREAK },
    { "break",	0,					1, 0, 0, 2, BREAK },
    { "ec",	"Send Telnet Erase Character",		1, 0, 0, 2, EC },
    { "el",	"Send Telnet Erase Line",		1, 0, 0, 2, EL },
    { "escape",	"Send current escape character",	1, 0, send_esc, 1, 0 },
    { "ga",	"Send Telnet 'Go Ahead' sequence",	1, 0, 0, 2, GA },
    { "ip",	"Send Telnet Interrupt Process",	1, 0, 0, 2, IP },
    { "intp",	0,					1, 0, 0, 2, IP },
    { "interrupt", 0,					1, 0, 0, 2, IP },
    { "intr",	0,					1, 0, 0, 2, IP },
    { "nop",	"Send Telnet 'No operation'",		1, 0, 0, 2, NOP },
    { "eor",	"Send Telnet 'End of Record'",		1, 0, 0, 2, EOR },
    { "abort",	"Send Telnet 'Abort Process'",		1, 0, 0, 2, ABORT },
    { "susp",	"Send Telnet 'Suspend Process'",	1, 0, 0, 2, SUSP },
    { "eof",	"Send Telnet End of File Character",	1, 0, 0, 2, xEOF },
    { "synch",	"Perform Telnet 'Synch operation'",	1, 0, dosynch, 2, 0 },
    { "getstatus", "Send request for STATUS",		1, 0, get_status, 6, 0 },
    { "?",	"Display send options",			0, 0, send_help, 0, 0 },
    { "help",	0,					0, 0, send_help, 0, 0 },
    { "do",	0,					0, 1, send_docmd, 3, 0 },
    { "dont",	0,					0, 1, send_dontcmd, 3, 0 },
    { "will",	0,					0, 1, send_willcmd, 3, 0 },
    { "wont",	0,					0, 1, send_wontcmd, 3, 0 },
    { 0 }
};

#define	GETSEND(name) ((struct sendlist *) genget(name, (char **) Sendlist, \
				sizeof(struct sendlist)))

static int
sendcmd(int argc, char **argv)
{
    int count;		/* how many bytes we are going to need to send */
    int i;
    struct sendlist *s;	/* pointer to current command */
    int success = 0;
    int needconnect = 0;

    if (argc < 2) {
	printf("need at least one argument for 'send' command\r\n");
	printf("'send ?' for help\r\n");
	return 0;
    }
    /*
     * First, validate all the send arguments.
     * In addition, we see how much space we are going to need, and
     * whether or not we will be doing a "SYNCH" operation (which
     * flushes the network queue).
     */
    count = 0;
    for (i = 1; i < argc; i++) {
	s = GETSEND(argv[i]);
	if (s == 0) {
	    printf("Unknown send argument '%s'\r\n'send ?' for help.\r\n",
			argv[i]);
	    return 0;
	} else if (Ambiguous(s)) {
	    printf("Ambiguous send argument '%s'\r\n'send ?' for help.\r\n",
			argv[i]);
	    return 0;
	}
	if (i + s->narg >= argc) {
	    fprintf(stderr,
	    "Need %d argument%s to 'send %s' command.  'send %s ?' for help.\r\n",
		s->narg, s->narg == 1 ? "" : "s", s->name, s->name);
	    return 0;
	}
	count += s->nbyte;
	if (s->handler == send_help) {
	    send_help();
	    return 0;
	}

	i += s->narg;
	needconnect += s->needconnect;
    }
    if (!connected && needconnect) {
	printf("?Need to be connected first.\r\n");
	printf("'send ?' for help\r\n");
	return 0;
    }
    /* Now, do we have enough room? */
    if (NETROOM() < count) {
	printf("There is not enough room in the buffer TO the network\r\n");
	printf("to process your request.  Nothing will be done.\r\n");
	printf("('send synch' will throw away most data in the network\r\n");
	printf("buffer, if this might help.)\r\n");
	return 0;
    }
    /* OK, they are all OK, now go through again and actually send */
    count = 0;
    for (i = 1; i < argc; i++) {
	if ((s = GETSEND(argv[i])) == 0) {
	    fprintf(stderr, "Telnet 'send' error - argument disappeared!\r\n");
	    quit();
	}
	if (s->handler) {
	    count++;
	    success += (*s->handler)((s->narg > 0) ? argv[i+1] : 0,
				  (s->narg > 1) ? argv[i+2] : 0);
	    i += s->narg;
	} else {
	    NET2ADD(IAC, s->what);
	    printoption("SENT", IAC, s->what);
	}
    }
    return (count == success);
}

static int send_tncmd(void (*func)(int, int), char *cmd, char *name);

static int
send_esc(void)
{
    NETADD(escape);
    return 1;
}

static int
send_docmd(char *name)
{
    return(send_tncmd(send_do, "do", name));
}

static int
send_dontcmd(char *name)
{
    return(send_tncmd(send_dont, "dont", name));
}

static int
send_willcmd(char *name)
{
    return(send_tncmd(send_will, "will", name));
}

static int
send_wontcmd(char *name)
{
    return(send_tncmd(send_wont, "wont", name));
}

int
send_tncmd(void (*func)(int, int), char *cmd, char *name)
{
    char **cpp;
    extern char *telopts[];
    int val = 0;

    if (isprefix(name, "help") || isprefix(name, "?")) {
	int col, len;

	printf("Usage: send %s <value|option>\r\n", cmd);
	printf("\"value\" must be from 0 to 255\r\n");
	printf("Valid options are:\r\n\t");

	col = 8;
	for (cpp = telopts; *cpp; cpp++) {
	    len = strlen(*cpp) + 3;
	    if (col + len > 65) {
		printf("\r\n\t");
		col = 8;
	    }
	    printf(" \"%s\"", *cpp);
	    col += len;
	}
	printf("\r\n");
	return 0;
    }
    cpp = (char **)genget(name, telopts, sizeof(char *));
    if (Ambiguous(cpp)) {
	fprintf(stderr,"'%s': ambiguous argument ('send %s ?' for help).\r\n",
					name, cmd);
	return 0;
    }
    if (cpp) {
	val = cpp - telopts;
    } else {
	char *cp = name;

	while (*cp >= '0' && *cp <= '9') {
	    val *= 10;
	    val += *cp - '0';
	    cp++;
	}
	if (*cp != 0) {
	    fprintf(stderr, "'%s': unknown argument ('send %s ?' for help).\r\n",
					name, cmd);
	    return 0;
	} else if (val < 0 || val > 255) {
	    fprintf(stderr, "'%s': bad value ('send %s ?' for help).\r\n",
					name, cmd);
	    return 0;
	}
    }
    if (!connected) {
	printf("?Need to be connected first.\r\n");
	return 0;
    }
    (*func)(val, 1);
    return 1;
}

static int
send_help(void)
{
    struct sendlist *s;	/* pointer to current command */
    for (s = Sendlist; s->name; s++) {
	if (s->help)
	    printf("%-15s %s\r\n", s->name, s->help);
    }
    return(0);
}

/*
 * The following are the routines and data structures referred
 * to by the arguments to the "toggle" command.
 */

static int
lclchars(int unused)
{
    donelclchars = 1;
    return 1;
}

static int
togdebug(int unused)
{
    if (net > 0 &&
	(setsockopt(net, SOL_SOCKET, SO_DEBUG, &debug, sizeof(debug))) == -1) {
	    perror("setsockopt (SO_DEBUG)");
    }
    return 1;
}

static int
togcrlf(int unused)
{
    if (crlf) {
	printf("Will send carriage returns as telnet <CR><LF>.\r\n");
    } else {
	printf("Will send carriage returns as telnet <CR><NUL>.\r\n");
    }
    return 1;
}

int binmode;

static int
togbinary(int val)
{
    donebinarytoggle = 1;

    if (val >= 0) {
	binmode = val;
    } else {
	if (my_want_state_is_will(TELOPT_BINARY) &&
				my_want_state_is_do(TELOPT_BINARY)) {
	    binmode = 1;
	} else if (my_want_state_is_wont(TELOPT_BINARY) &&
				my_want_state_is_dont(TELOPT_BINARY)) {
	    binmode = 0;
	}
	val = binmode ? 0 : 1;
    }

    if (val == 1) {
	if (my_want_state_is_will(TELOPT_BINARY) &&
					my_want_state_is_do(TELOPT_BINARY)) {
	    printf("Already operating in binary mode with remote host.\r\n");
	} else {
	    printf("Negotiating binary mode with remote host.\r\n");
	    tel_enter_binary(3);
	}
    } else {
	if (my_want_state_is_wont(TELOPT_BINARY) &&
					my_want_state_is_dont(TELOPT_BINARY)) {
	    printf("Already in network ascii mode with remote host.\r\n");
	} else {
	    printf("Negotiating network ascii mode with remote host.\r\n");
	    tel_leave_binary(3);
	}
    }
    return 1;
}

static int
togrbinary(int val)
{
    donebinarytoggle = 1;

    if (val == -1)
	val = my_want_state_is_do(TELOPT_BINARY) ? 0 : 1;

    if (val == 1) {
	if (my_want_state_is_do(TELOPT_BINARY)) {
	    printf("Already receiving in binary mode.\r\n");
	} else {
	    printf("Negotiating binary mode on input.\r\n");
	    tel_enter_binary(1);
	}
    } else {
	if (my_want_state_is_dont(TELOPT_BINARY)) {
	    printf("Already receiving in network ascii mode.\r\n");
	} else {
	    printf("Negotiating network ascii mode on input.\r\n");
	    tel_leave_binary(1);
	}
    }
    return 1;
}

static int
togxbinary(int val)
{
    donebinarytoggle = 1;

    if (val == -1)
	val = my_want_state_is_will(TELOPT_BINARY) ? 0 : 1;

    if (val == 1) {
	if (my_want_state_is_will(TELOPT_BINARY)) {
	    printf("Already transmitting in binary mode.\r\n");
	} else {
	    printf("Negotiating binary mode on output.\r\n");
	    tel_enter_binary(2);
	}
    } else {
	if (my_want_state_is_wont(TELOPT_BINARY)) {
	    printf("Already transmitting in network ascii mode.\r\n");
	} else {
	    printf("Negotiating network ascii mode on output.\r\n");
	    tel_leave_binary(2);
	}
    }
    return 1;
}


static int togglehelp(int);

struct togglelist {
    char	*name;			/* name of toggle */
    char	*help;			/* help message */
    int		(*handler)(int);	/* routine to do actual setting */
    int		*variable;
    char	*actionexplanation;
    int		needconnect;	/* Need to be connected */
};

static struct togglelist Togglelist[] = {
    { "autoflush",
	"flushing of output when sending interrupt characters",
	    0,
		&autoflush,
		    "flush output when sending interrupt characters" },
    { "autosynch",
	"automatic sending of interrupt characters in urgent mode",
	    0,
		&autosynch,
		    "send interrupt characters in urgent mode" },
    { "autologin",
	"automatic sending of login name",
	    0,
		&autologin,
		    "send login name" },
    { "skiprc",
	"don't read ~/.telnetrc file",
	    0,
		&skiprc,
		    "skip reading of ~/.telnetrc file" },
    { "binary",
	"sending and receiving of binary data",
	    togbinary,
		0,
		    0 },
    { "inbinary",
	"receiving of binary data",
	    togrbinary,
		0,
		    0 },
    { "outbinary",
	"sending of binary data",
	    togxbinary,
		0,
		    0 },
    { "crlf",
	"sending carriage returns as telnet <CR><LF>",
	    togcrlf,
		&crlf,
		    0 },
    { "crmod",
	"mapping of received carriage returns",
	    0,
		&crmod,
		    "map carriage return on output" },
    { "localchars",
	"local recognition of certain control characters",
	    lclchars,
		&localchars,
		    "recognize certain control characters" },
    { " ", "", 0, 0 },		/* empty line */
    { "debug",
	"debugging",
	    togdebug,
		&debug,
		    "turn on socket level debugging" },
    { "netdata",
	"printing of hexadecimal network data (debugging)",
	    0,
		&netdata,
		    "print hexadecimal representation of network traffic" },
    { "prettydump",
	"output of \"netdata\" to user readable format (debugging)",
	    0,
		&prettydump,
		    "print user readable output for \"netdata\"" },
    { "options",
	"viewing of options processing (debugging)",
	    0,
		&showoptions,
		    "show option processing" },
    { "termdata",
	"(debugging) toggle printing of hexadecimal terminal data",
	    0,
		&termdata,
		    "print hexadecimal representation of terminal traffic" },
    { "?",
	0,
	    togglehelp },
    { "help",
	0,
	    togglehelp },
    { 0 }
};

static int
togglehelp(int unused)
{
    struct togglelist *c;

    for (c = Togglelist; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s toggle %s\r\n", c->name, c->help);
	    else
		printf("\r\n");
	}
    }
    printf("\r\n");
    printf("%-15s %s\r\n", "?", "display help information");
    return 0;
}

static void
settogglehelp(int set)
{
    struct togglelist *c;

    for (c = Togglelist; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s %s %s\r\n", c->name, set ? "enable" : "disable",
						c->help);
	    else
		printf("\r\n");
	}
    }
}

#define	GETTOGGLE(name) (struct togglelist *) \
		genget(name, (char **) Togglelist, sizeof(struct togglelist))

static int
toggle(int argc, char *argv[])
{
    int retval = 1;
    char *name;
    struct togglelist *c;

    if (argc < 2) {
	fprintf(stderr,
	    "Need an argument to 'toggle' command.  'toggle ?' for help.\r\n");
	return 0;
    }
    argc--;
    argv++;
    while (argc--) {
	name = *argv++;
	c = GETTOGGLE(name);
	if (Ambiguous(c)) {
	    fprintf(stderr, "'%s': ambiguous argument ('toggle ?' for help).\r\n",
					name);
	    return 0;
	} else if (c == 0) {
	    fprintf(stderr, "'%s': unknown argument ('toggle ?' for help).\r\n",
					name);
	    return 0;
	} else if (!connected && c->needconnect) {
	    printf("?Need to be connected first.\r\n");
	    printf("'send ?' for help\r\n");
	    return 0;
	} else {
	    if (c->variable) {
		*c->variable = !*c->variable;		/* invert it */
		if (c->actionexplanation) {
		    printf("%s %s.\r\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
		}
	    }
	    if (c->handler) {
		retval &= (*c->handler)(-1);
	    }
	}
    }
    return retval;
}

/*
 * The following perform the "set" command.
 */

struct termios new_tc = { 0 };

struct setlist {
    char *name;				/* name */
    char *help;				/* help information */
    void (*handler)(const char *);
    cc_t *charp;			/* where it is located at */
};

static struct setlist Setlist[] = {
#ifdef	KLUDGELINEMODE
    { "echo", 	"character to toggle local echoing on/off", 0, &echoc },
#endif
    { "escape",	"character to escape back to telnet command mode", 0, &escape },
    { "rlogin", "rlogin escape character", 0, &rlogin },
    { "tracefile", "file to write trace information to", SetNetTrace, (cc_t *)NetTraceFile},
    { " ", "" },
    { " ", "The following need 'localchars' to be toggled true", 0, 0 },
    { "flushoutput", "character to cause an Abort Output", 0, &termFlushChar },
    { "interrupt", "character to cause an Interrupt Process", 0, &termIntChar },
    { "quit",	"character to cause an Abort process", 0, &termQuitChar },
    { "eof",	"character to cause an EOF ", 0, &termEofChar },
    { " ", "" },
    { " ", "The following are for local editing in linemode", 0, 0 },
    { "erase",	"character to use to erase a character", 0, &termEraseChar },
    { "kill",	"character to use to erase a line", 0, &termKillChar },
    { "lnext",	"character to use for literal next", 0, &termLiteralNextChar },
    { "susp",	"character to cause a Suspend Process", 0, &termSuspChar },
    { "reprint", "character to use for line reprint", 0, &termRprntChar },
    { "worderase", "character to use to erase a word", 0, &termWerasChar },
    { "start",	"character to use for XON", 0, &termStartChar },
    { "stop",	"character to use for XOFF", 0, &termStopChar },
    { "forw1",	"alternate end of line character", 0, &termForw1Char },
    { "forw2",	"alternate end of line character", 0, &termForw2Char },
    { "ayt",	"alternate AYT character", 0, &termAytChar },
    { 0 }
};

static struct setlist *
getset(char *name)
{
    return (struct setlist *)
		genget(name, (char **) Setlist, sizeof(struct setlist));
}

void
set_escape_char(char *s)
{
	if (rlogin != _POSIX_VDISABLE) {
		rlogin = (s && *s) ? special(s) : _POSIX_VDISABLE;
		printf("Telnet rlogin escape character is '%s'.\r\n",
					control(rlogin));
	} else {
		escape = (s && *s) ? special(s) : _POSIX_VDISABLE;
		printf("Telnet escape character is '%s'.\r\n", control(escape));
	}
}

static int
setcmd(int argc, char *argv[])
{
    int value;
    struct setlist *ct;
    struct togglelist *c;

    if (argc < 2 || argc > 3) {
	printf("Format is 'set Name Value'\r\n'set ?' for help.\r\n");
	return 0;
    }
    if ((argc == 2) && (isprefix(argv[1], "?") || isprefix(argv[1], "help"))) {
	for (ct = Setlist; ct->name; ct++)
	    printf("%-15s %s\r\n", ct->name, ct->help);
	printf("\r\n");
	settogglehelp(1);
	printf("%-15s %s\r\n", "?", "display help information");
	return 0;
    }

    ct = getset(argv[1]);
    if (ct == 0) {
	c = GETTOGGLE(argv[1]);
	if (c == 0) {
	    fprintf(stderr, "'%s': unknown argument ('set ?' for help).\r\n",
			argv[1]);
	    return 0;
	} else if (Ambiguous(c)) {
	    fprintf(stderr, "'%s': ambiguous argument ('set ?' for help).\r\n",
			argv[1]);
	    return 0;
	} else if (!connected && c->needconnect) {
	    printf("?Need to be connected first.\r\n");
	    printf("'send ?' for help\r\n");
	    return 0;
	}

	if (c->variable) {
	    if ((argc == 2) || (strcmp("on", argv[2]) == 0))
		*c->variable = 1;
	    else if (strcmp("off", argv[2]) == 0)
		*c->variable = 0;
	    else {
		printf("Format is 'set togglename [on|off]'\r\n'set ?' for help.\r\n");
		return 0;
	    }
	    if (c->actionexplanation) {
		printf("%s %s.\r\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
	    }
	}
	if (c->handler)
	    (*c->handler)(1);
    } else if (argc != 3) {
	printf("Format is 'set Name Value'\r\n'set ?' for help.\r\n");
	return 0;
    } else if (Ambiguous(ct)) {
	fprintf(stderr, "'%s': ambiguous argument ('set ?' for help).\r\n",
			argv[1]);
	return 0;
    } else if (ct->handler) {
	(*ct->handler)(argv[2]);
	printf("%s set to \"%s\".\r\n", ct->name, (char *)ct->charp);
    } else {
	if (strcmp("off", argv[2])) {
	    value = special(argv[2]);
	} else {
	    value = _POSIX_VDISABLE;
	}
	*(ct->charp) = (cc_t)value;
	printf("%s character is '%s'.\r\n", ct->name, control(*(ct->charp)));
    }
    slc_check();
    return 1;
}

static int
unsetcmd(int argc, char *argv[])
{
    struct setlist *ct;
    struct togglelist *c;
    char *name;

    if (argc < 2) {
	fprintf(stderr,
	    "Need an argument to 'unset' command.  'unset ?' for help.\r\n");
	return 0;
    }
    if (isprefix(argv[1], "?") || isprefix(argv[1], "help")) {
	for (ct = Setlist; ct->name; ct++)
	    printf("%-15s %s\r\n", ct->name, ct->help);
	printf("\r\n");
	settogglehelp(0);
	printf("%-15s %s\r\n", "?", "display help information");
	return 0;
    }

    argc--;
    argv++;
    while (argc--) {
	name = *argv++;
	ct = getset(name);
	if (ct == 0) {
	    c = GETTOGGLE(name);
	    if (c == 0) {
		fprintf(stderr, "'%s': unknown argument ('unset ?' for help).\r\n",
			name);
		return 0;
	    } else if (Ambiguous(c)) {
		fprintf(stderr, "'%s': ambiguous argument ('unset ?' for help).\r\n",
			name);
		return 0;
	    }
	    if (c->variable) {
		*c->variable = 0;
		if (c->actionexplanation) {
		    printf("%s %s.\r\n", *c->variable? "Will" : "Won't",
							c->actionexplanation);
		}
	    }
	    if (c->handler)
		(*c->handler)(0);
	} else if (Ambiguous(ct)) {
	    fprintf(stderr, "'%s': ambiguous argument ('unset ?' for help).\r\n",
			name);
	    return 0;
	} else if (ct->handler) {
	    (*ct->handler)(NULL);
	    printf("%s reset to \"%s\".\r\n", ct->name, (char *)ct->charp);
	} else {
	    *(ct->charp) = _POSIX_VDISABLE;
	    printf("%s character is '%s'.\r\n", ct->name, control(*(ct->charp)));
	}
    }
    return 1;
}

/*
 * The following are the data structures and routines for the
 * 'mode' command.
 */
#ifdef	KLUDGELINEMODE
static int
dokludgemode(int unused)
{
    kludgelinemode = 1;
    send_wont(TELOPT_LINEMODE, 1);
    send_dont(TELOPT_SGA, 1);
    send_dont(TELOPT_ECHO, 1);
    return 1;
}
#endif

static int
dolinemode(int unused)
{
#ifdef	KLUDGELINEMODE
    if (kludgelinemode)
	send_dont(TELOPT_SGA, 1);
#endif
    send_will(TELOPT_LINEMODE, 1);
    send_dont(TELOPT_ECHO, 1);
    return 1;
}

static int
docharmode(int unused)
{
#ifdef	KLUDGELINEMODE
    if (kludgelinemode)
	send_do(TELOPT_SGA, 1);
    else
#endif
    send_wont(TELOPT_LINEMODE, 1);
    send_do(TELOPT_ECHO, 1);
    return 1;
}

static int
dolmmode(int bit, int on)
{
    unsigned char c;

    if (my_want_state_is_wont(TELOPT_LINEMODE)) {
	printf("?Need to have LINEMODE option enabled first.\r\n");
	printf("'mode ?' for help.\r\n");
	return 0;
    }

    if (on)
	c = (linemode | bit);
    else
	c = (linemode & ~bit);
    lm_mode(&c, 1, 1);
    return 1;
}

int
tn_setmode(int bit)
{
    return dolmmode(bit, 1);
}

int
tn_clearmode(int bit)
{
    return dolmmode(bit, 0);
}

struct modelist {
	char	*name;		/* command name */
	char	*help;		/* help string */
	int	(*handler)(int);/* routine which executes command */
	int	needconnect;	/* Do we need to be connected to execute? */
	int	arg1;
};

static int modehelp(int);

static struct modelist ModeList[] = {
    { "character", "Disable LINEMODE option",	docharmode, 1 },
#ifdef	KLUDGELINEMODE
    { "",	"(or disable obsolete line-by-line mode)", 0 },
#endif
    { "line",	"Enable LINEMODE option",	dolinemode, 1 },
#ifdef	KLUDGELINEMODE
    { "",	"(or enable obsolete line-by-line mode)", 0 },
#endif
    { "", "", 0 },
    { "",	"These require the LINEMODE option to be enabled", 0 },
    { "isig",	"Enable signal trapping",	tn_setmode, 1, MODE_TRAPSIG },
    { "+isig",	0,				tn_setmode, 1, MODE_TRAPSIG },
    { "-isig",	"Disable signal trapping",	tn_clearmode, 1, MODE_TRAPSIG },
    { "edit",	"Enable character editing",	tn_setmode, 1, MODE_EDIT },
    { "+edit",	0,				tn_setmode, 1, MODE_EDIT },
    { "-edit",	"Disable character editing",	tn_clearmode, 1, MODE_EDIT },
    { "softtabs", "Enable tab expansion",	tn_setmode, 1, MODE_SOFT_TAB },
    { "+softtabs", 0,				tn_setmode, 1, MODE_SOFT_TAB },
    { "-softtabs", "Disable character editing",	tn_clearmode, 1, MODE_SOFT_TAB },
    { "litecho", "Enable literal character echo", tn_setmode, 1, MODE_LIT_ECHO },
    { "+litecho", 0,				tn_setmode, 1, MODE_LIT_ECHO },
    { "-litecho", "Disable literal character echo", tn_clearmode, 1, MODE_LIT_ECHO },
    { "help",	0,				modehelp, 0 },
#ifdef	KLUDGELINEMODE
    { "kludgeline", 0,				dokludgemode, 1 },
#endif
    { "", "", 0 },
    { "?",	"Print help information",	modehelp, 0 },
    { 0 },
};

static int
modehelp(int unused)
{
    struct modelist *mt;

    printf("format is:  'mode Mode', where 'Mode' is one of:\r\n\r\n");
    for (mt = ModeList; mt->name; mt++) {
	if (mt->help) {
	    if (*mt->help)
		printf("%-15s %s\r\n", mt->name, mt->help);
	    else
		printf("\r\n");
	}
    }
    return 0;
}

#define	GETMODECMD(name) (struct modelist *) \
		genget(name, (char **) ModeList, sizeof(struct modelist))

static int
modecmd(int argc, char *argv[])
{
    struct modelist *mt;

    if (argc != 2) {
	printf("'mode' command requires an argument\r\n");
	printf("'mode ?' for help.\r\n");
    } else if ((mt = GETMODECMD(argv[1])) == 0) {
	fprintf(stderr, "Unknown mode '%s' ('mode ?' for help).\r\n", argv[1]);
    } else if (Ambiguous(mt)) {
	fprintf(stderr, "Ambiguous mode '%s' ('mode ?' for help).\r\n", argv[1]);
    } else if (mt->needconnect && !connected) {
	printf("?Need to be connected first.\r\n");
	printf("'mode ?' for help.\r\n");
    } else if (mt->handler) {
	return (*mt->handler)(mt->arg1);
    }
    return 0;
}

/*
 * The following data structures and routines implement the
 * "display" command.
 */

static int
display(int argc, char *argv[])
{
    struct togglelist *tl;
    struct setlist *sl;

#define	dotog(tl)	if (tl->variable && tl->actionexplanation) { \
			    if (*tl->variable) { \
				printf("will"); \
			    } else { \
				printf("won't"); \
			    } \
			    printf(" %s.\r\n", tl->actionexplanation); \
			}

#define	doset(sl)   if (sl->name && *sl->name != ' ') { \
			if (sl->handler == 0) \
			    printf("%-15s [%s]\r\n", sl->name, control(*sl->charp)); \
			else \
			    printf("%-15s \"%s\"\r\n", sl->name, (char *)sl->charp); \
		    }

    if (argc == 1) {
	for (tl = Togglelist; tl->name; tl++) {
	    dotog(tl);
	}
	printf("\r\n");
	for (sl = Setlist; sl->name; sl++) {
	    doset(sl);
	}
    } else {
	int i;

	for (i = 1; i < argc; i++) {
	    sl = getset(argv[i]);
	    tl = GETTOGGLE(argv[i]);
	    if (Ambiguous(sl) || Ambiguous(tl)) {
		printf("?Ambiguous argument '%s'.\r\n", argv[i]);
		return 0;
	    } else if (!sl && !tl) {
		printf("?Unknown argument '%s'.\r\n", argv[i]);
		return 0;
	    } else {
		if (tl) {
		    dotog(tl);
		}
		if (sl) {
		    doset(sl);
		}
	    }
	}
    }
/*@*/optionstatus();
    return 1;
#undef	doset
#undef	dotog
}

/*
 * The following are the data structures, and many of the routines,
 * relating to command processing.
 */

/*
 * Set the escape character.
 */
static int
setescape(int argc, char *argv[])
{
	char *arg;
	char buf[50];

	printf(
	    "Deprecated usage - please use 'set escape%s%s' in the future.\r\n",
				(argc > 2)? " ":"", (argc > 2)? argv[1]: "");
	if (argc > 2)
		arg = argv[1];
	else {
		printf("new escape character: ");
		(void) fgets(buf, sizeof(buf), stdin);
		arg = buf;
	}
	if (arg[0] != '\0')
		escape = arg[0];
	printf("Escape character is '%s'.\r\n", control(escape));
	(void) fflush(stdout);
	return 1;
}

static int
togcrmod(int unused1, char *unused2[])
{
    crmod = !crmod;
    printf("Deprecated usage - please use 'toggle crmod' in the future.\r\n");
    printf("%s map carriage return on output.\r\n", crmod ? "Will" : "Won't");
    (void) fflush(stdout);
    return 1;
}

int
telnetsuspend(int unused1, char *unused2[])
{
    setcommandmode();
    {
	long oldrows, oldcols, newrows, newcols, err;

	err = (TerminalWindowSize(&oldrows, &oldcols) == 0) ? 1 : 0;
	(void) kill(0, SIGTSTP);
	/*
	 * If we didn't get the window size before the SUSPEND, but we
	 * can get them now (?), then send the NAWS to make sure that
	 * we are set up for the right window size.
	 */
	if (TerminalWindowSize(&newrows, &newcols) && connected &&
	    (err || ((oldrows != newrows) || (oldcols != newcols)))) {
		sendnaws();
	}
    }
    /* reget parameters in case they were changed */
    TerminalSaveState();
    setconnmode(0);
    return 1;
}

int
shell(int argc, char *argv[])
{
    long oldrows, oldcols, newrows, newcols, err;

    setcommandmode();

    err = (TerminalWindowSize(&oldrows, &oldcols) == 0) ? 1 : 0;
    switch(vfork()) {
    case -1:
	perror("Fork failed\r\n");
	break;

    case 0:
	{
	    /*
	     * Fire up the shell in the child.
	     */
	    char *shellp, *shellname;

	    shellp = getenv("SHELL");
	    if (shellp == NULL)
		shellp = "/bin/sh";
	    if ((shellname = strrchr(shellp, '/')) == 0)
		shellname = shellp;
	    else
		shellname++;
	    if (argc > 1)
		execl(shellp, shellname, "-c", &saveline[1], (char *)NULL);
	    else
		execl(shellp, shellname, (char *)NULL);
	    perror("Execl");
	    _exit(1);
	}
    default:
	    (void)wait((int *)0);	/* Wait for the shell to complete */

	    if (TerminalWindowSize(&newrows, &newcols) && connected &&
		(err || ((oldrows != newrows) || (oldcols != newcols)))) {
		    sendnaws();
	    }
	    break;
    }
    return 1;
}

static void
close_connection(void)
{
	if (connected) {
		(void) shutdown(net, 2);
		printf("Connection closed.\r\n");
		(void)close(net);
		connected = 0;
		resettermname = 1;
		/* reset options */
		tninit();
	}
}

static int
bye(int argc, char *argv[])
{
	close_connection();
	longjmp(toplevel, 1);
}

void
quit(void)
{
	close_connection();
	Exit(0);
}

static int
quitcmd(int unused1, char *unused2[])
{
	quit();
}

static int
logout(int unused1, char *unused2[])
{
	send_do(TELOPT_LOGOUT, 1);
	(void) netflush();
	return 1;
}


/*
 * The SLC command.
 */

struct slclist {
	char	*name;
	char	*help;
	void	(*handler)(int);
	int	arg;
};

static void slc_help(int);

struct slclist SlcList[] = {
    { "export",	"Use local special character definitions",
						slc_mode_export,	0 },
    { "import",	"Use remote special character definitions",
						slc_mode_import,	1 },
    { "check",	"Verify remote special character definitions",
						slc_mode_import,	0 },
    { "help",	0,				slc_help,		0 },
    { "?",	"Print help information",	slc_help,		0 },
    { 0 },
};

static void
slc_help(int unused)
{
    struct slclist *c;

    for (c = SlcList; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s %s\r\n", c->name, c->help);
	    else
		printf("\r\n");
	}
    }
}

static struct slclist *
getslc(char *name)
{
    return (struct slclist *)
		genget(name, (char **) SlcList, sizeof(struct slclist));
}

static int
slccmd(int argc, char *argv[])
{
    struct slclist *c;

    if (argc != 2) {
	fprintf(stderr,
	    "Need an argument to 'slc' command.  'slc ?' for help.\r\n");
	return 0;
    }
    c = getslc(argv[1]);
    if (c == 0) {
	fprintf(stderr, "'%s': unknown argument ('slc ?' for help).\r\n",
    				argv[1]);
	return 0;
    }
    if (Ambiguous(c)) {
	fprintf(stderr, "'%s': ambiguous argument ('slc ?' for help).\r\n",
    				argv[1]);
	return 0;
    }
    (*c->handler)(c->arg);
    slcstate();
    return 1;
}

/*
 * The ENVIRON command.
 */

struct envlist {
	char	*name;
	char	*help;
	void	(*handler)();
	int	narg;
};

static void	env_help(void);
static void	env_undefine(const char *);
static void	env_export(const char *);
static void	env_unexport(const char *);
static void	env_send(const char *);
static void	env_list(void);
static struct env_lst *env_find(const char *var);

struct envlist EnvList[] = {
    { "define",	"Define an environment variable",
						(void (*)())env_define,	2 },
    { "undefine", "Undefine an environment variable",
						env_undefine,	1 },
    { "export",	"Mark an environment variable for automatic export",
						env_export,	1 },
    { "unexport", "Don't mark an environment variable for automatic export",
						env_unexport,	1 },
    { "send",	"Send an environment variable", env_send,	1 },
    { "list",	"List the current environment variables",
						env_list,	0 },
    { "help",	0,				env_help,		0 },
    { "?",	"Print help information",	env_help,		0 },
    { 0 },
};

static void
env_help(void)
{
    struct envlist *c;

    for (c = EnvList; c->name; c++) {
	if (c->help) {
	    if (*c->help)
		printf("%-15s %s\r\n", c->name, c->help);
	    else
		printf("\r\n");
	}
    }
}

static struct envlist *
getenvcmd(char *name)
{
    return (struct envlist *)
		genget(name, (char **) EnvList, sizeof(struct envlist));
}

static int
env_cmd(int argc, char *argv[])
{
    struct envlist *c;

    if (argc < 2) {
	fprintf(stderr,
	    "Need an argument to 'environ' command.  'environ ?' for help.\r\n");
	return 0;
    }
    c = getenvcmd(argv[1]);
    if (c == 0) {
	fprintf(stderr, "'%s': unknown argument ('environ ?' for help).\r\n",
    				argv[1]);
	return 0;
    }
    if (Ambiguous(c)) {
	fprintf(stderr, "'%s': ambiguous argument ('environ ?' for help).\r\n",
    				argv[1]);
	return 0;
    }
    if (c->narg + 2 != argc) {
	fprintf(stderr,
	    "Need %s%d argument%s to 'environ %s' command.  'environ ?' for help.\r\n",
		c->narg < argc + 2 ? "only " : "",
		c->narg, c->narg == 1 ? "" : "s", c->name);
	return 0;
    }
    (*c->handler)(argv[2], argv[3]);
    return 1;
}

struct env_lst {
	struct env_lst *next;	/* pointer to next structure */
	struct env_lst *prev;	/* pointer to previous structure */
	char *var;		/* pointer to variable name */
	char *value;		/* pointer to variable value */
	int export;		/* 1 -> export with default list of variables */
	int welldefined;	/* A well defined variable */
};

struct env_lst envlisthead;

static struct env_lst *
env_find(const char *var)
{
	struct env_lst *ep;

	for (ep = envlisthead.next; ep; ep = ep->next) {
		if (strcmp(ep->var, var) == 0)
			return(ep);
	}
	return(NULL);
}

void
env_init(void)
{
	extern char **environ;
	char **epp, *cp;
	struct env_lst *ep;

	for (epp = environ; *epp; epp++) {
		if ((cp = strchr(*epp, '='))) {
			*cp = '\0';
			ep = env_define(*epp, cp+1);
			ep->export = 0;
			*cp = '=';
		}
	}
	/*
	 * Special case for DISPLAY variable.  If it is ":0.0" or
	 * "unix:0.0", we have to get rid of "unix" and insert our
	 * hostname.
	 */
	if ((ep = env_find("DISPLAY"))
	    && ((*ep->value == ':')
		|| (strncmp(ep->value, "unix:", 5) == 0))) {
		char hbuf[HOST_NAME_MAX+1];
		char *cp2 = strchr(ep->value, ':');

		gethostname(hbuf, sizeof hbuf);

		/* If this is not the full name, try to get it via DNS */
		if (strchr(hbuf, '.') == 0) {
			struct hostent *he = gethostbyname(hbuf);
			if (he != 0)
				strncpy(hbuf, he->h_name, sizeof hbuf-1);
			hbuf[sizeof hbuf-1] = '\0';
		}

		if (asprintf (&cp, "%s%s", hbuf, cp2) == -1)
			err(1, "asprintf");

		free(ep->value);
		ep->value = cp;
	}
	/*
	 * If USER is not defined, but LOGNAME is, then add
	 * USER with the value from LOGNAME.  By default, we
	 * don't export the USER variable.
	 */
	if ((env_find("USER") == NULL) && (ep = env_find("LOGNAME"))) {
		env_define("USER", ep->value);
		env_unexport("USER");
	}
	env_export("DISPLAY");
	env_export("PRINTER");
	env_export("XAUTHORITY");
}

struct env_lst *
env_define(const char *var, const char *value)
{
	struct env_lst *ep;

	if ((ep = env_find(var))) {
		if (ep->var)
			free(ep->var);
		if (ep->value)
			free(ep->value);
	} else {
		if ((ep = malloc(sizeof(struct env_lst))) == NULL)
			err(1, "malloc");
		ep->next = envlisthead.next;
		envlisthead.next = ep;
		ep->prev = &envlisthead;
		if (ep->next)
			ep->next->prev = ep;
	}
	ep->welldefined = opt_welldefined(var);
	ep->export = 1;
	if ((ep->var = strdup(var)) == NULL)
		err(1, "strdup");
	if ((ep->value = strdup(value)) == NULL)
		err(1, "strdup");
	return(ep);
}

static void
env_undefine(const char *var)
{
	struct env_lst *ep;

	if ((ep = env_find(var))) {
		ep->prev->next = ep->next;
		if (ep->next)
			ep->next->prev = ep->prev;
		if (ep->var)
			free(ep->var);
		if (ep->value)
			free(ep->value);
		free(ep);
	}
}

static void
env_export(const char *var)
{
	struct env_lst *ep;

	if ((ep = env_find(var)))
		ep->export = 1;
}

static void
env_unexport(const char *var)
{
	struct env_lst *ep;

	if ((ep = env_find(var)) != NULL)
		ep->export = 0;
}

static void
env_send(const char *var)
{
	struct env_lst *ep;

	if (my_state_is_wont(TELOPT_NEW_ENVIRON)
		) {
		fprintf(stderr,
		    "Cannot send '%s': Telnet ENVIRON option not enabled\r\n",
									var);
		return;
	}
	ep = env_find(var);
	if (ep == 0) {
		fprintf(stderr, "Cannot send '%s': variable not defined\r\n",
									var);
		return;
	}
	env_opt_start_info();
	env_opt_add(ep->var);
	env_opt_end(0);
}

static void
env_list(void)
{
	struct env_lst *ep;

	for (ep = envlisthead.next; ep; ep = ep->next) {
		printf("%c %-20s %s\r\n", ep->export ? '*' : ' ',
					ep->var, ep->value);
	}
}

char *
env_default(int init, int welldefined)
{
	static struct env_lst *nep = NULL;

	if (init) {
		nep = &envlisthead;
		return NULL;
	}
	if (nep) {
		while ((nep = nep->next)) {
			if (nep->export && (nep->welldefined == welldefined))
				return(nep->var);
		}
	}
	return(NULL);
}

char *
env_getvalue(const char *var, int exported_only)
{
	struct env_lst *ep;

	if ((ep = env_find(var)) && (!exported_only || ep->export))
		return(ep->value);
	return(NULL);
}

static void
connection_status(int local_only)
{
	if (!connected)
		printf("No connection.\r\n");
	else {
		printf("Connected to %s.\r\n", hostname);
		if (!local_only) {
			int mode = getconnmode();

			printf("Operating ");
			if (my_want_state_is_will(TELOPT_LINEMODE)) {
				printf("with LINEMODE option\r\n"
				    "%s line editing\r\n"
				    "%s catching of signals\r\n",
				    (mode & MODE_EDIT) ? "Local" : "No",
				    (mode & MODE_TRAPSIG) ? "Local" : "No");
				slcstate();
#ifdef	KLUDGELINEMODE
			} else if (kludgelinemode &&
			    my_want_state_is_dont(TELOPT_SGA)) {
				printf("in obsolete linemode\r\n");
#endif
			} else {
				printf("in single character mode\r\n");
				if (localchars)
					printf("Catching signals locally\r\n");
			}

			printf("%s character echo\r\n",
			    (mode & MODE_ECHO) ? "Local" : "Remote");
			if (my_want_state_is_will(TELOPT_LFLOW))
				printf("%s flow control\r\n",
				    (mode & MODE_FLOW) ? "Local" : "No");
		}
	}
	printf("Escape character is '%s'.\r\n", control(escape));
	(void) fflush(stdout);
}

/*
 * Print status about the connection.
 */
static int
status(int argc, char *argv[])
{
	connection_status(0);
	return 1;
}

#ifdef	SIGINFO
/*
 * Function that gets called when SIGINFO is received.
 */
void
ayt_status(int sig)
{
	connection_status(1);
}
#endif

static Command *getcmd(char *name);

static void
cmdrc(char *m1, char *m2)
{
    static char rcname[128];
    Command *c;
    FILE *rcfile;
    int gotmachine = 0;
    int l1 = strlen(m1);
    int l2 = strlen(m2);
    char m1save[HOST_NAME_MAX+1];

    if (skiprc)
	return;

    strlcpy(m1save, m1, sizeof(m1save));
    m1 = m1save;

    if (rcname[0] == 0) {
	char *home = getenv("HOME");

	if (home == NULL || *home == '\0')
	    return;
	snprintf (rcname, sizeof(rcname), "%s/.telnetrc",
		  home ? home : "");
    }

    if ((rcfile = fopen(rcname, "r")) == 0) {
	return;
    }

    for (;;) {
	if (fgets(line, sizeof(line), rcfile) == NULL)
	    break;
	if (line[0] == 0)
	    break;
	if (line[0] == '#')
	    continue;
	if (gotmachine) {
	    if (!isspace((unsigned char)line[0]))
		gotmachine = 0;
	}
	if (gotmachine == 0) {
	    if (isspace((unsigned char)line[0]))
		continue;
	    if (strncasecmp(line, m1, l1) == 0)
		strncpy(line, &line[l1], sizeof(line) - l1);
	    else if (strncasecmp(line, m2, l2) == 0)
		strncpy(line, &line[l2], sizeof(line) - l2);
	    else if (strncasecmp(line, "DEFAULT", 7) == 0)
		strncpy(line, &line[7], sizeof(line) - 7);
	    else
		continue;
	    if (line[0] != ' ' && line[0] != '\t' && line[0] != '\n')
		continue;
	    gotmachine = 1;
	}
	makeargv();
	if (margv[0] == 0)
	    continue;
	c = getcmd(margv[0]);
	if (Ambiguous(c)) {
	    printf("?Ambiguous command: %s\r\n", margv[0]);
	    continue;
	}
	if (c == 0) {
	    printf("?Invalid command: %s\r\n", margv[0]);
	    continue;
	}
	/*
	 * This should never happen...
	 */
	if (c->needconnect && !connected) {
	    printf("?Need to be connected first for %s.\r\n", margv[0]);
	    continue;
	}
	(*c->handler)(margc, margv);
    }
    fclose(rcfile);
}

int
tn(int argc, char *argv[])
{
    struct addrinfo hints, *res, *res0;
    int error;
    struct sockaddr_in sin;
    unsigned long temp;
    char *srp = 0;
    int srlen;
    char *cmd, *hostp = 0, *portp = 0, *user = 0, *aliasp = 0;
    int retry;
    const int niflags = NI_NUMERICHOST;

    /* clear the socket address prior to use */
    memset(&sin, 0, sizeof(sin));

    if (connected) {
	printf("?Already connected to %s\r\n", hostname);
	return 0;
    }
    if (argc < 2) {
	strlcpy(line, "open ", sizeof(line));
	printf("(to) ");
	(void) fgets(&line[strlen(line)], sizeof(line) - strlen(line), stdin);
	makeargv();
	argc = margc;
	argv = margv;
    }
    cmd = *argv;
    --argc; ++argv;
    while (argc) {
	if (strcmp(*argv, "help") == 0 || isprefix(*argv, "?"))
	    goto usage;
	if (strcmp(*argv, "-l") == 0) {
	    --argc; ++argv;
	    if (argc == 0)
		goto usage;
	    if ((user = strdup(*argv++)) == NULL)
		err(1, "strdup");
	    --argc;
	    continue;
	}
	if (strcmp(*argv, "-b") == 0) {
	    --argc; ++argv;
	    if (argc == 0)
		goto usage;
	    aliasp = *argv++;
	    --argc;
	    continue;
	}
	if (strcmp(*argv, "-a") == 0) {
	    --argc; ++argv;
	    autologin = 1;
	    continue;
	}
	if (hostp == 0) {
	    hostp = *argv++;
	    --argc;
	    continue;
	}
	if (portp == 0) {
	    portp = *argv++;
	    --argc;
	    continue;
	}
    usage:
	printf("usage: %s [-a] [-b hostalias] [-l user] host-name [port]\r\n", cmd);
	return 0;
    }
    if (hostp == 0)
	goto usage;

    if (hostp[0] == '@' || hostp[0] == '!') {
	if ((hostname = strrchr(hostp, ':')) == NULL)
	    hostname = strrchr(hostp, '@');
	hostname++;
	srp = 0;
	temp = sourceroute(hostp, &srp, &srlen);
	if (temp == 0) {
	    herror(srp);
	    return 0;
	} else if (temp == -1) {
	    printf("Bad source route option: %s\r\n", hostp);
	    return 0;
	} else {
	    abort();
	}
    } else
    {
	hostname = hostp;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	if (portp == NULL) {
	    portp = "telnet";
	    telnetport = 1;
	} else if (*portp == '-') {
	    portp++;
	    telnetport = 1;
	} else
	    telnetport = 0;
	h_errno = 0;
	error = getaddrinfo(hostp, portp, &hints, &res0);
	if (error) {
	    if (error == EAI_SERVICE)
		warnx("%s: bad port", portp);
	    else
		warnx("%s: %s", hostp, gai_strerror(error));
	    if (h_errno)
		herror(hostp);
	    return 0;
	}
    }

    net = -1;
    retry = 0;
    for (res = res0; res; res = res->ai_next) {
	if (1 /* retry */) {
	    char hbuf[NI_MAXHOST];

	    if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf),
		    NULL, 0, niflags) != 0) {
		strlcpy(hbuf, "(invalid)", sizeof(hbuf));
	    }
	    printf("Trying %s...\r\n", hbuf);
	}
	net = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (net < 0)
	    continue;

	if (rtableid >= 0 && (setsockopt(net, SOL_SOCKET, SO_RTABLE, &rtableid,
	    sizeof(rtableid)) == -1))
		perror("setsockopt (SO_RTABLE)");

	if (aliasp) {
	    struct addrinfo ahints, *ares;

	    memset(&ahints, 0, sizeof(ahints));
	    ahints.ai_family = family;
	    ahints.ai_socktype = SOCK_STREAM;
	    ahints.ai_flags = AI_PASSIVE;
	    error = getaddrinfo(aliasp, "0", &ahints, &ares);
	    if (error) {
		warn("%s: %s", aliasp, gai_strerror(error));
		close(net);
		net = -1;
		continue;
	    }
	    if (bind(net, ares->ai_addr, ares->ai_addrlen) < 0) {
		perror(aliasp);
		(void) close(net);   /* dump descriptor */
		net = -1;
		freeaddrinfo(ares);
		continue;
            }
	    freeaddrinfo(ares);
	}
	if (srp && res->ai_family == AF_INET
	 && setsockopt(net, IPPROTO_IP, IP_OPTIONS, srp, srlen) < 0)
		perror("setsockopt (IP_OPTIONS)");
	if (res->ai_family == AF_INET) {
	    if (tos < 0)
		tos = IPTOS_LOWDELAY;	/* Low Delay bit */
	    if (tos
		&& (setsockopt(net, IPPROTO_IP, IP_TOS, &tos, sizeof(int)) < 0)
		&& (errno != ENOPROTOOPT))
		    perror("telnet: setsockopt (IP_TOS) (ignored)");
	}

	if (debug) {
		int one = 1;

		if (setsockopt(net, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			perror("setsockopt (SO_DEBUG)");
	}

	if (connect(net, res->ai_addr, res->ai_addrlen) < 0) {
	    char hbuf[NI_MAXHOST];

	    if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf),
		    NULL, 0, niflags) != 0) {
		strlcpy(hbuf, "(invalid)", sizeof(hbuf));
	    }
	    fprintf(stderr, "telnet: connect to address %s: %s\n", hbuf,
		strerror(errno));

	    close(net);
	    net = -1;
	    retry++;
	    continue;
	}

	connected++;
	break;
    }
    freeaddrinfo(res0);
    if (net < 0) {
	return 0;
    }
    cmdrc(hostp, hostname);
    if (autologin && user == NULL) {
	struct passwd *pw;

	user = getlogin();
	if (user == NULL ||
	    (pw = getpwnam(user)) == NULL || pw->pw_uid != getuid()) {
		if ((pw = getpwuid(getuid())) != NULL)
			user = pw->pw_name;
		else
			user = NULL;
	}
    }
    if (user) {
	env_define("USER", user);
	env_export("USER");
    }
    connection_status(1);
    if (setjmp(peerdied) == 0)
	telnet(user);
    (void)close(net);
    ExitString("Connection closed by foreign host.\r\n",1);
}

#define HELPINDENT (sizeof ("connect"))

static char
	openhelp[] =	"connect to a site",
	closehelp[] =	"close current connection",
	logouthelp[] =	"forcibly logout remote user and close the connection",
	quithelp[] =	"exit telnet",
	statushelp[] =	"print status information",
	helphelp[] =	"print help information",
	sendhelp[] =	"transmit special characters ('send ?' for more)",
	sethelp[] = 	"set operating parameters ('set ?' for more)",
	unsethelp[] = 	"unset operating parameters ('unset ?' for more)",
	togglestring[] ="toggle operating parameters ('toggle ?' for more)",
	slchelp[] =	"change state of special charaters ('slc ?' for more)",
	displayhelp[] =	"display operating parameters",
	zhelp[] =	"suspend telnet",
#ifdef SKEY
	skeyhelp[] =	"compute response to s/key challenge",
#endif
	shellhelp[] =	"invoke a subshell",
	envhelp[] =	"change environment variables ('environ ?' for more)",
	modestring[] = "try to enter line or character mode ('mode ?' for more)";

static int	help(int, char**);

static Command cmdtab[] = {
	{ "close",	closehelp,	bye,		1 },
	{ "logout",	logouthelp,	logout,		1 },
	{ "display",	displayhelp,	display,	0 },
	{ "mode",	modestring,	modecmd,	0 },
	{ "open",	openhelp,	tn,		0 },
	{ "quit",	quithelp,	quitcmd,	0 },
	{ "send",	sendhelp,	sendcmd,	0 },
	{ "set",	sethelp,	setcmd,		0 },
	{ "unset",	unsethelp,	unsetcmd,	0 },
	{ "status",	statushelp,	status,		0 },
	{ "toggle",	togglestring,	toggle,		0 },
	{ "slc",	slchelp,	slccmd,		0 },

	{ "z",		zhelp,		telnetsuspend,	0 },
	{ "!",		shellhelp,	shell,		0 },
	{ "environ",	envhelp,	env_cmd,	0 },
	{ "?",		helphelp,	help,		0 },
#ifdef SKEY
	{ "skey",	skeyhelp,	skey_calc,	0 },
#endif		
	{ 0,		0,		0,		0 }
};

static char	crmodhelp[] =	"deprecated command -- use 'toggle crmod' instead";
static char	escapehelp[] =	"deprecated command -- use 'set escape' instead";

static Command cmdtab2[] = {
	{ "help",	0,		help,		0 },
	{ "escape",	escapehelp,	setescape,	0 },
	{ "crmod",	crmodhelp,	togcrmod,	0 },
	{ 0,		0,		0,		0 }
};


static Command *
getcmd(char *name)
{
    Command *cm;

    if ((cm = (Command *) genget(name, (char **) cmdtab, sizeof(Command))))
	return cm;
    return (Command *) genget(name, (char **) cmdtab2, sizeof(Command));
}

void
command(int top, char *tbuf, int cnt)
{
    Command *c;

    setcommandmode();
    if (!top) {
	putchar('\n');
    } else {
	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
    }
    for (;;) {
	if (rlogin == _POSIX_VDISABLE)
		printf("%s> ", prompt);
	if (tbuf) {
	    char *cp;
	    cp = line;
	    while (cnt > 0 && (*cp++ = *tbuf++) != '\n')
		cnt--;
	    tbuf = 0;
	    if (cp == line || *--cp != '\n' || cp == line)
		goto getline;
	    *cp = '\0';
	    if (rlogin == _POSIX_VDISABLE)
		printf("%s\r\n", line);
	} else {
	getline:
	    if (rlogin != _POSIX_VDISABLE)
		printf("%s> ", prompt);
	    if (fgets(line, sizeof(line), stdin) == NULL) {
		if (feof(stdin) || ferror(stdin))
		    quit();
		break;
	    }
	}
	if (line[0] == 0)
	    break;
	makeargv();
	if (margv[0] == 0) {
	    break;
	}
	c = getcmd(margv[0]);
	if (Ambiguous(c)) {
	    printf("?Ambiguous command\r\n");
	    continue;
	}
	if (c == 0) {
	    printf("?Invalid command\r\n");
	    continue;
	}
	if (c->needconnect && !connected) {
	    printf("?Need to be connected first.\r\n");
	    continue;
	}
	if ((*c->handler)(margc, margv)) {
	    break;
	}
    }
    if (!top) {
	if (!connected)
	    longjmp(toplevel, 1);
	setconnmode(0);
    }
}

/*
 * Help command.
 */
static int
help(int argc, char *argv[])
{
	Command *c;

	if (argc == 1) {
		printf("Commands may be abbreviated.  Commands are:\r\n\r\n");
		for (c = cmdtab; c->name; c++)
			if (c->help) {
				printf("%-*s\t%s\r\n", (int)HELPINDENT, c->name,
								    c->help);
			}
		return 0;
	}
	while (--argc > 0) {
		char *arg;
		arg = *++argv;
		c = getcmd(arg);
		if (Ambiguous(c))
			printf("?Ambiguous help command %s\r\n", arg);
		else if (c == (Command *)0)
			printf("?Invalid help command %s\r\n", arg);
		else
			printf("%s\r\n", c->help);
	}
	return 0;
}

/*
 * Source route is handed in as
 *	[!]@hop1@hop2...[@|:]dst
 * If the leading ! is present, it is a
 * strict source route, otherwise it is
 * assmed to be a loose source route.
 *
 * We fill in the source route option as
 *	hop1,hop2,hop3...dest
 * and return a pointer to hop1, which will
 * be the address to connect() to.
 *
 * Arguments:
 *	arg:	pointer to route list to decipher
 *
 *	cpp: 	If *cpp is not equal to NULL, this is a
 *		pointer to a pointer to a character array
 *		that should be filled in with the option.
 *
 *	lenp:	pointer to an integer that contains the
 *		length of *cpp if *cpp != NULL.
 *
 * Return values:
 *
 *	Returns the address of the host to connect to.  If the
 *	return value is -1, there was a syntax error in the
 *	option, either unknown characters, or too many hosts.
 *	If the return value is 0, one of the hostnames in the
 *	path is unknown, and *cpp is set to point to the bad
 *	hostname.
 *
 *	*cpp:	If *cpp was equal to NULL, it will be filled
 *		in with a pointer to our static area that has
 *		the option filled in.  This will be 32bit aligned.
 *
 *	*lenp:	This will be filled in with how long the option
 *		pointed to by *cpp is.
 *
 */

static unsigned long
sourceroute(char *arg, char **cpp, int *lenp)
{
	static char lsr[44];
	char *cp, *cp2, *lsrp, *lsrep;
	struct in_addr addr;
	struct hostent *host = 0;
	char c;

	/*
	 * Verify the arguments, and make sure we have
	 * at least 7 bytes for the option.
	 */
	if (cpp == NULL || lenp == NULL)
		return((unsigned long)-1);
	if (*cpp != NULL && *lenp < 7)
		return((unsigned long)-1);
	/*
	 * Decide whether we have a buffer passed to us,
	 * or if we need to use our own static buffer.
	 */
	if (*cpp) {
		lsrp = *cpp;
		lsrep = lsrp + *lenp;
	} else {
		*cpp = lsrp = lsr;
		lsrep = lsrp + 44;
	}

	cp = arg;

	/*
	 * Next, decide whether we have a loose source
	 * route or a strict source route, and fill in
	 * the begining of the option.
	 */
	if (*cp == '!') {
		cp++;
		*lsrp++ = IPOPT_SSRR;
	} else
		*lsrp++ = IPOPT_LSRR;

	if (*cp != '@')
		return((unsigned long)-1);

	lsrp++;		/* skip over length, we'll fill it in later */
	*lsrp++ = 4;

	cp++;

	addr.s_addr = 0;

	for (c = 0;;) {
		if (c == ':')
			cp2 = 0;
		else for (cp2 = cp; (c = *cp2); cp2++) {
			if (c == ',') {
				*cp2++ = '\0';
				if (*cp2 == '@')
					cp2++;
			} else if (c == '@') {
				*cp2++ = '\0';
			} else if (c == ':') {
				*cp2++ = '\0';
			} else
				continue;
			break;
		}
		if (!c)
			cp2 = 0;

		if ((addr.s_addr = inet_addr(cp)) == INADDR_NONE) {
			if ((host = gethostbyname(cp)) == NULL) {
				*cpp = cp;
				return(0);
			}
			memcpy(&addr, host->h_addr_list[0], sizeof addr);
		}
		memcpy(lsrp, &addr, 4);
		lsrp += 4;
		if (cp2)
			cp = cp2;
		else
			break;
		/*
		 * Check to make sure there is space for next address
		 */
		if (lsrp + 4 > lsrep)
			return((unsigned long)-1);
	}
	if ((*(*cpp+IPOPT_OLEN) = lsrp - *cpp) <= 7) {
		*cpp = 0;
		*lenp = 0;
		return((unsigned long)-1);
	}
	*lsrp++ = IPOPT_NOP; /* 32 bit word align it */
	*lenp = lsrp - *cpp;
	return(addr.s_addr);
}
