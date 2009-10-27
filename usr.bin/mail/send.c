/*	$OpenBSD: send.c,v 1.22 2009/10/27 23:59:40 deraadt Exp $	*/
/*	$NetBSD: send.c,v 1.6 1996/06/08 19:48:39 christos Exp $	*/

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
#include "extern.h"

static volatile sig_atomic_t sendsignal;	/* Interrupted by a signal? */

/*
 * Mail -- a mail program
 *
 * Mail to others.
 */

/*
 * Send message described by the passed pointer to the
 * passed output buffer.  Return -1 on error.
 * Adjust the status: field if need be.
 * If doign is given, suppress ignored header fields.
 * prefix is a string to prepend to each output line.
 */
int
sendmessage(struct message *mp, FILE *obuf, struct ignoretab *doign,
	    char *prefix)
{
	int count;
	FILE *ibuf;
	char line[LINESIZE];
	char visline[4 * LINESIZE - 3];
	int ishead, infld, ignoring = 0, dostat, firstline;
	char *cp, *cp2;
	int c = 0;
	int length;
	int prefixlen = 0;
	int rval;
	int dovis;
	struct sigaction act, saveint;
	sigset_t oset;

	sendsignal = 0;
	rval = -1;
	dovis = isatty(fileno(obuf));
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
	act.sa_handler = sendint;
	(void)sigaction(SIGINT, &act, &saveint);
	(void)sigprocmask(SIG_UNBLOCK, &intset, &oset);

	/*
	 * Compute the prefix string, without trailing whitespace
	 */
	if (prefix != NULL) {
		cp2 = 0;
		for (cp = prefix; *cp; cp++)
			if (*cp != ' ' && *cp != '\t')
				cp2 = cp;
		prefixlen = cp2 == 0 ? 0 : cp2 - prefix + 1;
	}
	ibuf = setinput(mp);
	count = mp->m_size;
	ishead = 1;
	dostat = doign == 0 || !isign("status", doign);
	infld = 0;
	firstline = 1;
	/*
	 * Process headers first
	 */
	while (count > 0 && ishead) {
		if (fgets(line, sizeof(line), ibuf) == NULL)
			break;
		count -= length = strlen(line);
		if (firstline) {
			/*
			 * First line is the From line, so no headers
			 * there to worry about
			 */
			firstline = 0;
			ignoring = doign == ignoreall;
		} else if (line[0] == '\n') {
			/*
			 * If line is blank, we've reached end of
			 * headers, so force out status: field
			 * and note that we are no longer in header
			 * fields
			 */
			if (dostat) {
				if (statusput(mp, obuf, prefix) == -1)
					goto out;
				dostat = 0;
			}
			ishead = 0;
			ignoring = doign == ignoreall;
		} else if (infld && (line[0] == ' ' || line[0] == '\t')) {
			/*
			 * If this line is a continuation (via space or tab)
			 * of a previous header field, just echo it
			 * (unless the field should be ignored).
			 * In other words, nothing to do.
			 */
		} else {
			/*
			 * Pick up the header field if we have one.
			 */
			for (cp = line; (c = *cp++) && c != ':' && !isspace(c);)
				;
			cp2 = --cp;
			while (isspace(*cp++))
				;
			if (cp[-1] != ':') {
				/*
				 * Not a header line, force out status:
				 * This happens in uucp style mail where
				 * there are no headers at all.
				 */
				if (dostat) {
					if (statusput(mp, obuf, prefix) == -1)
						goto out;
					dostat = 0;
				}
				if (doign != ignoreall)
					/* add blank line */
					(void)putc('\n', obuf);
				ishead = 0;
				ignoring = 0;
			} else {
				/*
				 * If it is an ignored field and
				 * we care about such things, skip it.
				 */
				*cp2 = 0;	/* temporarily null terminate */
				if (doign && isign(line, doign))
					ignoring = 1;
				else if (strcasecmp(line, "status") == 0) {
					/*
					 * If the field is "status," go compute
					 * and print the real Status: field
					 */
					if (dostat) {
						if (statusput(mp, obuf, prefix) == -1)
							goto out;
						dostat = 0;
					}
					ignoring = 1;
				} else {
					ignoring = 0;
					*cp2 = c;	/* restore */
				}
				infld = 1;
			}
		}
		if (!ignoring) {
			/*
			 * Strip trailing whitespace from prefix
			 * if line is blank.
			 */
			if (prefix != NULL) {
				if (length > 1)
					fputs(prefix, obuf);
				else
					(void)fwrite(prefix, sizeof(*prefix),
							prefixlen, obuf);
			}
			if (dovis) {
				length = strvis(visline, line, VIS_SAFE|VIS_NOSLASH);
				(void)fwrite(visline, sizeof(*visline), length, obuf);
			} else
				(void)fwrite(line, sizeof(*line), length, obuf);
			if (ferror(obuf))
				goto out;
		}
		if (sendsignal == SIGINT)
			goto out;
	}
	/*
	 * Copy out message body
	 */
	if (doign == ignoreall)
		count--;		/* skip final blank line */
	while (count > 0) {
		if (fgets(line, sizeof(line), ibuf) == NULL) {
			c = 0;
			break;
		}
		count -= c = strlen(line);
		if (prefix != NULL) {
			/*
			 * Strip trailing whitespace from prefix
			 * if line is blank.
			 */
			if (c > 1)
				fputs(prefix, obuf);
			else
				(void)fwrite(prefix, sizeof(*prefix),
						prefixlen, obuf);
		}
		/*
		 * We can't read the record file (or inbox for recipient)
		 * properly with 'From ' lines in the message body (from
		 * forwarded messages or sentences starting with "From "),
		 * so we will prepend those lines with a '>'.
		 */
		if (strncmp(line, "From ", 5) == 0)
			(void)fwrite(">", 1, 1, obuf); /* '>' before 'From ' */
		if (dovis) {
			length = strvis(visline, line, VIS_SAFE|VIS_NOSLASH);
			(void)fwrite(visline, sizeof(*visline), length, obuf);
		} else
			(void)fwrite(line, sizeof(*line), c, obuf);
		if (ferror(obuf) || sendsignal == SIGINT)
			goto out;
	}
	if (doign == ignoreall && c > 0 && line[c - 1] != '\n')
		/* no final blank line */
		if ((c = getc(ibuf)) != EOF && putc(c, obuf) == EOF)
			goto out;
	rval = 0;
out:
	sendsignal = 0;
	(void)sigprocmask(SIG_SETMASK, &oset, NULL);
	(void)sigaction(SIGINT, &saveint, NULL);
	return(rval);
}

/*
 * Output a reasonable looking status field.
 */
int
statusput(struct message *mp, FILE *obuf, char *prefix)
{
	char statout[3];
	char *cp = statout;

	if (mp->m_flag & MREAD)
		*cp++ = 'R';
	if ((mp->m_flag & MNEW) == 0)
		*cp++ = 'O';
	*cp = 0;
	if (statout[0]) {
		fprintf(obuf, "%sStatus: %s\n",
			prefix == NULL ? "" : prefix, statout);
		return(ferror(obuf) ? -1 : 0);
	}
	return(0);
}

/*
 * Interface between the argument list and the mail1 routine
 * which does all the dirty work.
 */
int
mail(struct name *to, struct name *cc, struct name *bcc, struct name *smopts,
     char *subject)
{
	struct header head;

	head.h_to = to;
	head.h_subject = subject;
	head.h_cc = cc;
	head.h_bcc = bcc;
	head.h_smopts = smopts;
	mail1(&head, 0);
	return(0);
}


/*
 * Send mail to a bunch of user names.  The interface is through
 * the mail routine below.
 */
int
sendmail(void *v)
{
	char *str = v;
	struct header head;

	head.h_to = extract(str, GTO);
	head.h_subject = NULL;
	head.h_cc = NULL;
	head.h_bcc = NULL;
	head.h_smopts = NULL;
	mail1(&head, 0);
	return(0);
}

/*
 * Mail a message on standard input to the people indicated
 * in the passed header.  (Internal interface).
 */
void
mail1(struct header *hp, int printheaders)
{
	char *cp;
	pid_t pid;
	char **namelist;
	struct name *to;
	FILE *mtf;

	/*
	 * Collect user's mail from standard input.
	 * Get the result as mtf.
	 */
	if ((mtf = collect(hp, printheaders)) == NULL)
		return;
	if (fsize(mtf) == 0) {
		if (value("skipempty") != NULL)
			goto out;
		if (hp->h_subject == NULL || *hp->h_subject == '\0')
			puts("No message, no subject; hope that's ok");
		else
			puts("Null message body; hope that's ok");
	}
	/*
	 * Now, take the user names from the combined
	 * to and cc lists and do all the alias
	 * processing.
	 */
	senderr = 0;
	to = usermap(cat(hp->h_bcc, cat(hp->h_to, hp->h_cc)));
	if (to == NULL) {
		puts("No recipients specified");
		senderr++;
	}
	/*
	 * Look through the recipient list for names with /'s
	 * in them which we write to as files directly.
	 */
	to = outof(to, mtf, hp);
	if (senderr)
		savedeadletter(mtf);
	to = elide(to);
	if (count(to) == 0)
		goto out;
	fixhead(hp, to);
	if ((mtf = infix(hp, mtf)) == NULL) {
		fputs(". . . message lost, sorry.\n", stderr);
		return;
	}
	namelist = unpack(hp->h_smopts, to);
	if (debug) {
		char **t;

		fputs("Sendmail arguments:", stdout);
		for (t = namelist; *t != NULL; t++)
			printf(" \"%s\"", *t);
		putchar('\n');
		goto out;
	}
	if ((cp = value("record")) != NULL)
		(void)savemail(expand(cp), mtf);
	/*
	 * Fork, set up the temporary mail file as standard
	 * input for "mail", and exec with the user list we generated
	 * far above.
	 */
	pid = fork();
	if (pid == -1) {
		warn("fork");
		savedeadletter(mtf);
		goto out;
	}
	if (pid == 0) {
		sigset_t nset;

		sigemptyset(&nset);
		sigaddset(&nset, SIGHUP);
		sigaddset(&nset, SIGINT);
		sigaddset(&nset, SIGQUIT);
		sigaddset(&nset, SIGTSTP);
		sigaddset(&nset, SIGTTIN);
		sigaddset(&nset, SIGTTOU);
		prepare_child(&nset, fileno(mtf), -1);
		if ((cp = value("sendmail")) != NULL)
			cp = expand(cp);
		else
			cp = _PATH_SENDMAIL;
		execv(cp, namelist);
		warn("%s", cp);
		_exit(1);
	}
	if (value("verbose") != NULL)
		(void)wait_child(pid);
	else
		free_child(pid);
out:
	(void)Fclose(mtf);
}

/*
 * Fix the header by glopping all of the expanded names from
 * the distribution list into the appropriate fields.
 */
void
fixhead(struct header *hp, struct name *tolist)
{
	struct name *np;

	hp->h_to = NULL;
	hp->h_cc = NULL;
	hp->h_bcc = NULL;
	for (np = tolist; np != NULL; np = np->n_flink)
		if ((np->n_type & GMASK) == GTO)
			hp->h_to =
				cat(hp->h_to, nalloc(np->n_name, np->n_type));
		else if ((np->n_type & GMASK) == GCC)
			hp->h_cc =
				cat(hp->h_cc, nalloc(np->n_name, np->n_type));
		else if ((np->n_type & GMASK) == GBCC)
			hp->h_bcc =
				cat(hp->h_bcc, nalloc(np->n_name, np->n_type));
}

/*
 * Prepend a header in front of the collected stuff
 * and return the new file.
 */
FILE *
infix(struct header *hp, FILE *fi)
{
	FILE *nfo, *nfi;
	int c, fd;
	char tempname[PATHSIZE];

	(void)snprintf(tempname, sizeof(tempname),
	    "%s/mail.RsXXXXXXXXXX", tmpdir);
	if ((fd = mkstemp(tempname)) == -1 ||
	    (nfo = Fdopen(fd, "w")) == NULL) {
		warn("%s", tempname);
		return(fi);
	}
	if ((nfi = Fopen(tempname, "r")) == NULL) {
		warn("%s", tempname);
		(void)Fclose(nfo);
		(void)rm(tempname);
		return(fi);
	}
	(void)rm(tempname);
	(void)puthead(hp, nfo, GTO|GSUBJECT|GCC|GBCC|GNL|GCOMMA);
	c = getc(fi);
	while (c != EOF) {
		(void)putc(c, nfo);
		c = getc(fi);
	}
	if (ferror(fi)) {
		warn("read");
		rewind(fi);
		return(fi);
	}
	(void)fflush(nfo);
	if (ferror(nfo)) {
		warn("%s", tempname);
		(void)Fclose(nfo);
		(void)Fclose(nfi);
		rewind(fi);
		return(fi);
	}
	(void)Fclose(nfo);
	(void)Fclose(fi);
	rewind(nfi);
	return(nfi);
}

/*
 * Dump the to, subject, cc header on the
 * passed file buffer.
 */
int
puthead(struct header *hp, FILE *fo, int w)
{
	int gotcha;

	gotcha = 0;
	if (hp->h_to != NULL && w & GTO)
		fmt("To:", hp->h_to, fo, w&GCOMMA), gotcha++;
	if (hp->h_subject != NULL && w & GSUBJECT)
		fprintf(fo, "Subject: %s\n", hp->h_subject), gotcha++;
	if (hp->h_cc != NULL && w & GCC)
		fmt("Cc:", hp->h_cc, fo, w&GCOMMA), gotcha++;
	if (hp->h_bcc != NULL && w & GBCC)
		fmt("Bcc:", hp->h_bcc, fo, w&GCOMMA), gotcha++;
	if (gotcha && w & GNL)
		(void)putc('\n', fo);
	return(0);
}

/*
 * Format the given header line to not exceed 72 characters.
 */
void
fmt(char *str, struct name *np, FILE *fo, int comma)
{
	int col, len;

	comma = comma ? 1 : 0;
	col = strlen(str);
	if (col)
		fputs(str, fo);
	for (; np != NULL; np = np->n_flink) {
		if (np->n_flink == NULL)
			comma = 0;
		len = strlen(np->n_name);
		col++;		/* for the space */
		if (col + len + comma > 72 && col > 4) {
			fputs("\n    ", fo);
			col = 4;
		} else
			putc(' ', fo);
		fputs(np->n_name, fo);
		if (comma)
			putc(',', fo);
		col += len + comma;
	}
	putc('\n', fo);
}

/*
 * Save the outgoing mail on the passed file.
 */
/*ARGSUSED*/
int
savemail(char *name, FILE *fi)
{
	FILE *fo;
	char buf[BUFSIZ];
	time_t now;
	mode_t m;

	m = umask(077);
	fo = Fopen(name, "a");
	(void)umask(m);
	if (fo == NULL) {
		warn("%s", name);
		return(-1);
	}
	(void)time(&now);
	fprintf(fo, "From %s %s", myname, ctime(&now));
	while (fgets(buf, sizeof(buf), fi) == buf) {
		/*
		 * We can't read the record file (or inbox for recipient)
		 * in the message body (from forwarded messages or sentences
		 * starting with "From "), so we will prepend those lines with
		 * a '>'.
		 */
		if (strncmp(buf, "From ", 5) == 0)
			(void)fwrite(">", 1, 1, fo);   /* '>' before 'From ' */
		(void)fwrite(buf, 1, strlen(buf), fo);
	}
	(void)putc('\n', fo);
	(void)fflush(fo);
	if (ferror(fo))
		warn("%s", name);
	(void)Fclose(fo);
	rewind(fi);
	return(0);
}

/*ARGSUSED*/
void
sendint(int s)
{

	sendsignal = s;
}
