/*	$OpenBSD: cmd1.c,v 1.19 2001/11/16 17:10:06 millert Exp $	*/
/*	$NetBSD: cmd1.c,v 1.9 1997/07/09 05:29:48 mikel Exp $	*/

/*-
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
static char sccsid[] = "@(#)cmd1.c	8.2 (Berkeley) 4/20/95";
#else
static char rcsid[] = "$OpenBSD: cmd1.c,v 1.19 2001/11/16 17:10:06 millert Exp $";
#endif
#endif /* not lint */

#include "rcv.h"
#include "extern.h"

/*
 * Mail -- a mail program
 *
 * User commands.
 */

/*
 * Print the current active headings.
 * Don't change dot if invoker didn't give an argument.
 */

static int screen;

int
headers(v)
	void *v;
{
	int *msgvec = v;
	int n, mesg, flag, size;
	struct message *mp;

	size = screensize();
	n = msgvec[0];
	if (n != 0)
		screen = (n-1)/size;
	if (screen < 0)
		screen = 0;
	mp = &message[screen * size];
	if (mp >= &message[msgCount])
		mp = &message[msgCount - size];
	if (mp < &message[0])
		mp = &message[0];
	flag = 0;
	mesg = mp - &message[0];
	if (dot != &message[n-1])
		dot = mp;
	for (; mp < &message[msgCount]; mp++) {
		mesg++;
		if (mp->m_flag & MDELETED)
			continue;
		if (flag++ >= size)
			break;
		printhead(mesg);
	}
	if (flag == 0) {
		puts("No more mail.");
		return(1);
	}
	return(0);
}

/*
 * Scroll to the next/previous screen
 */
int
scroll(v)
	void *v;
{
	char *arg = v;
	int size, maxscreen;
	int cur[1];

	cur[0] = 0;
	size = screensize();
	maxscreen = (msgCount - 1) / size;
	switch (*arg) {
	case 0:
	case '+':
		if (screen >= maxscreen) {
			puts("On last screenful of messages");
			return(0);
		}
		screen++;
		break;

	case '-':
		if (screen <= 0) {
			puts("On first screenful of messages");
			return(0);
		}
		screen--;
		break;

	default:
		printf("Unrecognized scrolling command \"%s\"\n", arg);
		return(1);
	}
	return(headers(cur));
}

/*
 * Compute screen size.
 */
int
screensize()
{
	int s;
	char *cp;

	if ((cp = value("screen")) != NULL && (s = atoi(cp)) > 0)
		return(s);
	return(screenheight - 4);
}

/*
 * Print out the headlines for each message
 * in the passed message list.
 */
int
from(v)
	void *v;
{
	int *msgvec = v;
	int *ip;

	for (ip = msgvec; *ip != NULL; ip++)
		printhead(*ip);
	if (--ip >= msgvec)
		dot = &message[*ip - 1];
	return(0);
}

/*
 * Print out the header of a specific message.
 * This is a slight improvement to the standard one.
 */
void
printhead(mesg)
	int mesg;
{
	struct message *mp;
	char headline[LINESIZE], wcount[LINESIZE], *subjline, dispc, curind;
	char pbuf[BUFSIZ];
	struct headline hl;
	int subjlen;
	char *name;
	char *to, *from;
	struct name *np;
	char **ap;

	mp = &message[mesg-1];
	(void)readline(setinput(mp), headline, LINESIZE);
	if ((subjline = hfield("subject", mp)) == NULL)
		subjline = hfield("subj", mp);
	/*
	 * Bletch!
	 */
	curind = dot == mp ? '>' : ' ';
	dispc = ' ';
	if (mp->m_flag & MSAVED)
		dispc = '*';
	if (mp->m_flag & MPRESERVE)
		dispc = 'P';
	if ((mp->m_flag & (MREAD|MNEW)) == MNEW)
		dispc = 'N';
	if ((mp->m_flag & (MREAD|MNEW)) == 0)
		dispc = 'U';
	if (mp->m_flag & MBOX)
		dispc = 'M';
	parse(headline, &hl, pbuf);
	(void)snprintf(wcount, sizeof(wcount), "%4d/%-5d", mp->m_lines,
	    mp->m_size);
	subjlen = screenwidth - 44 - strlen(wcount);
	from = nameof(mp, 0);
	to = skin(hfield("to", mp));
	np = extract(from, GTO);
	np = delname(np, myname);
	if (altnames)
		for (ap = altnames; *ap; ap++)
			np = delname(np, *ap);
	if (np)
		/* not from me */
		name = value("show-rcpt") != NULL && to ? to : from;
	else
		/* from me - show TO */
		name = value("showto") != NULL && to ? to : from;
	if (subjline == NULL || subjlen < 0) { /* pretty pathetic */
		subjline="";
		subjlen=0;
	}
	if (name == to)
		printf("%c%c%3d TO %-14.14s  %16.16s %s %.*s\n",
			curind, dispc, mesg, name, hl.l_date, wcount,
			subjlen, subjline);
	else
		printf("%c%c%3d %-17.17s  %16.16s %s %.*s\n",
			curind, dispc, mesg, name, hl.l_date, wcount,
			subjlen, subjline);
}

/*
 * Print out the value of dot.
 */
int
pdot(v)
	void *v;
{
	printf("%d\n", (int)(dot - &message[0] + 1));
	return(0);
}

/*
 * Print out all the possible commands.
 */
int
pcmdlist(v)
	void *v;
{
	extern const struct cmd cmdtab[];
	const struct cmd *cp;
	int cc;

	puts("Commands are:");
	for (cc = 0, cp = cmdtab; cp->c_name != NULL; cp++) {
		cc += strlen(cp->c_name) + 2;
		if (cc > 72) {
			putchar('\n');
			cc = strlen(cp->c_name) + 2;
		}
		if ((cp+1)->c_name != NULL)
			printf("%s, ", cp->c_name);
		else
			puts(cp->c_name);
	}
	return(0);
}

/*
 * Pipe message to command
 */
int
pipeit(ml, sl)
	void *ml, *sl;
{
	int  *msgvec = ml;
	char *cmd    = sl;

	return(type1(msgvec, cmd, 0, 0));
}

/*
 * Paginate messages, honor ignored fields.
 */
int
more(v)
	void *v;
{
	int *msgvec = v;
	return(type1(msgvec, NULL, 1, 1));
}

/*
 * Paginate messages, even printing ignored fields.
 */
int
More(v)
	void *v;
{
	int *msgvec = v;

	return(type1(msgvec, NULL, 0, 1));
}

/*
 * Type out messages, honor ignored fields.
 */
int
type(v)
	void *v;
{
	int *msgvec = v;

	return(type1(msgvec, NULL, 1, 0));
}

/*
 * Type out messages, even printing ignored fields.
 */
int
Type(v)
	void *v;
{
	int *msgvec = v;

	return(type1(msgvec, NULL, 0, 0));
}

/*
 * Type out the messages requested.
 */
int
type1(msgvec, cmd, doign, page)
	int *msgvec;
	char *cmd;
	int doign, page;
{
	int nlines, *ip, restoreterm;
	struct message *mp;
	struct termios tbuf;
	char * volatile cp;
	FILE * volatile obuf;

	obuf = stdout;
	restoreterm = 0;

	/*
	 * start a pipe if needed.
	 */
	if (cmd) {
		restoreterm = (tcgetattr(fileno(stdin), &tbuf) == 0);
		obuf = Popen(cmd, "w");
		if (obuf == NULL) {
			warn("%s", cp);
			obuf = stdout;
		} else {
			(void)signal(SIGPIPE, SIG_IGN);
		}
	} else if (value("interactive") != NULL &&
	         (page || (cp = value("crt")) != NULL)) {
		nlines = 0;
		if (!page) {
			for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++)
				nlines += message[*ip - 1].m_lines;
		}
		if (page || nlines > (*cp ? atoi(cp) : realscreenheight)) {
			restoreterm = (tcgetattr(fileno(stdin), &tbuf) == 0);
			obuf = Popen(value("PAGER"), "w");
			if (obuf == NULL) {
				warn("%s", cp);
				obuf = stdout;
			} else
				(void)signal(SIGPIPE, SIG_IGN);
		}
	}

	/*
	 * send messages to the output.
	 */
	for (ip = msgvec; *ip && ip - msgvec < msgCount; ip++) {
		mp = &message[*ip - 1];
		touch(mp);
		dot = mp;
		if (cmd == NULL && value("quiet") == NULL)
			fprintf(obuf, "Message %d:\n", *ip);
		if (sendmessage(mp, obuf, doign ? ignore : 0, NULL) == -1)
			break;
	}

	if (obuf != stdout) {
		(void)Pclose(obuf);
		(void)signal(SIGPIPE, SIG_DFL);
		if (restoreterm)
			(void)tcsetattr(fileno(stdin), TCSADRAIN, &tbuf);
	}
	return(0);
}

/*
 * Print the top so many lines of each desired message.
 * The number of lines is taken from the variable "toplines"
 * and defaults to 5.
 */
int
top(v)
	void *v;
{
	int *msgvec = v;
	int *ip;
	struct message *mp;
	int c, topl, lines, lineb;
	char *valtop, linebuf[LINESIZE];
	FILE *ibuf;

	topl = 5;
	valtop = value("toplines");
	if (valtop != NULL) {
		topl = atoi(valtop);
		if (topl < 0 || topl > 10000)
			topl = 5;
	}
	lineb = 1;
	for (ip = msgvec; *ip && ip-msgvec < msgCount; ip++) {
		mp = &message[*ip - 1];
		touch(mp);
		dot = mp;
		if (value("quiet") == NULL)
			printf("Message %d:\n", *ip);
		ibuf = setinput(mp);
		c = mp->m_lines;
		if (!lineb)
			putchar('\n');
		for (lines = 0; lines < c && lines <= topl; lines++) {
			if (readline(ibuf, linebuf, sizeof(linebuf)) < 0)
				break;
			puts(linebuf);
			lineb = blankline(linebuf);
		}
	}
	return(0);
}

/*
 * Touch all the given messages so that they will
 * get mboxed.
 */
int
stouch(v)
	void *v;
{
	int *msgvec = v;
	int *ip;

	for (ip = msgvec; *ip != 0; ip++) {
		dot = &message[*ip-1];
		dot->m_flag |= MTOUCH;
		dot->m_flag &= ~MPRESERVE;
	}
	return(0);
}

/*
 * Make sure all passed messages get mboxed.
 */
int
mboxit(v)
	void *v;
{
	int *msgvec = v;
	int *ip;

	for (ip = msgvec; *ip != 0; ip++) {
		dot = &message[*ip-1];
		dot->m_flag |= MTOUCH|MBOX;
		dot->m_flag &= ~MPRESERVE;
	}
	return(0);
}

/*
 * List the folders the user currently has.
 */
int
folders(v)
	void *v;
{
	char *files = (char *)v;
	char dirname[PATHSIZE];
	char cmd[BUFSIZ];

	if (getfold(dirname, sizeof(dirname)) < 0) {
		strcpy(dirname, "$HOME");
	}

	snprintf(cmd, sizeof(cmd), "cd %s; %s %s", dirname, value("LISTER"),
		files && *files ? files : "");

	(void)run_command(value("SHELL"), 0, -1, -1, "-c", cmd, NULL);
	return(0);
}

/*
 * Update the mail file with any new messages that have
 * come in since we started reading mail.
 */
int
inc(v)
	void *v;
{
	int nmsg, mdot;

	nmsg = incfile();

	if (nmsg == 0) {
		puts("No new mail.");
	} else if (nmsg > 0) {
		mdot = newfileinfo(msgCount - nmsg);
		dot = &message[mdot - 1];
	} else {
		puts("\"inc\" command failed...");
	}

	return(0);
}
