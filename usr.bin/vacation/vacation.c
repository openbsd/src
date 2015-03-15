/*	$OpenBSD: vacation.c,v 1.36 2015/03/15 00:41:28 millert Exp $	*/
/*	$NetBSD: vacation.c,v 1.7 1995/04/29 05:58:27 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1987, 1993
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
**  Vacation
**  Copyright (c) 1983  Eric P. Allman
**  Berkeley, California
*/

#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <db.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>

/*
 *  VACATION -- return a message to the sender when on vacation.
 *
 *	This program is invoked as a message receiver.  It returns a
 *	message specified by the user to whomever sent the mail, taking
 *	care not to return a message too often to prevent "I am on
 *	vacation" loops.
 */

#define	MAXLINE	1024			/* max line from mail header */
#define	VDB	".vacation.db"		/* dbm's database */
#define	VMSG	".vacation.msg"		/* vacation message */

typedef struct alias {
	struct alias *next;
	char *name;
} ALIAS;
ALIAS *names;

DB *db;
char from[MAXLINE];
char subj[MAXLINE];

int junkmail(void);
int nsearch(char *, char *);
void readheaders(void);
int recent(void);
void sendmessage(char *);
void setinterval(time_t);
void setreply(void);
void usage(void);

#define	SECSPERDAY	(24 * 60 * 60)

int
main(int argc, char *argv[])
{
	int ch, iflag, flags;
	struct passwd *pw;
	time_t interval;
	struct stat sb;
	ALIAS *cur;

	opterr = iflag = 0;
	interval = -1;
	while ((ch = getopt(argc, argv, "a:Iir:")) != -1)
		switch ((char)ch) {
		case 'a':			/* alias */
			if (!(cur = (ALIAS *)malloc((u_int)sizeof(ALIAS))))
				break;
			cur->name = optarg;
			cur->next = names;
			names = cur;
			break;
		case 'I':			/* backward compatible */
		case 'i':			/* init the database */
			iflag = 1;
			break;
		case 'r':
			if (isdigit((unsigned char)*optarg)) {
				interval = atol(optarg) * SECSPERDAY;
				if (interval < 0)
					usage();
			} else
				interval = 0;	/* one time only */
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		if (!iflag)
			usage();
		if (!(pw = getpwuid(getuid()))) {
			syslog(LOG_ERR,
			    "no such user uid %u.", getuid());
			exit(1);
		}
	} else if (!(pw = getpwnam(*argv))) {
		syslog(LOG_ERR, "no such user %s.", *argv);
		exit(1);
	}
	if (chdir(pw->pw_dir)) {
		syslog(LOG_NOTICE,
		    "no such directory %s.", pw->pw_dir);
		exit(1);
	}

	/*
	 * dbopen(3) can not deal with a zero-length file w/o O_TRUNC.
	 */
	if (iflag == 1 || (stat(VDB, &sb) == 0 && sb.st_size == (off_t)0))
		flags = O_CREAT|O_RDWR|O_TRUNC;
	else
		flags = O_CREAT|O_RDWR;

	db = dbopen(VDB, flags, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (!db) {
		syslog(LOG_NOTICE, "%s: %m", VDB);
		exit(1);
	}

	if (interval != -1)
		setinterval(interval);

	if (iflag) {
		(void)(db->close)(db);
		exit(0);
	}

	if (!(cur = malloc((u_int)sizeof(ALIAS))))
		exit(1);
	cur->name = pw->pw_name;
	cur->next = names;
	names = cur;

	readheaders();
	if (!recent()) {
		setreply();
		(void)(db->close)(db);
		sendmessage(pw->pw_name);
	} else
		(void)(db->close)(db);
	exit(0);
	/* NOTREACHED */
}

/*
 * readheaders --
 *	read mail headers
 */
void
readheaders(void)
{
	char buf[MAXLINE], *p;
	int tome, cont;
	ALIAS *cur;

	cont = tome = 0;
	while (fgets(buf, sizeof(buf), stdin) && *buf != '\n')
		switch (*buf) {
		case 'A':		/* "Auto-Submitted:" */
		case 'a':
			cont = 0;
			if (strncasecmp(buf, "Auto-Submitted:", 15))
				break;
			for (p = buf + 15; isspace((unsigned char)*p); ++p)
				;
			/*
			 * RFC 3834 section 2:
			 * Automatic responses SHOULD NOT be issued in response
			 * to any message which contains an Auto-Submitted
			 * header where the field has any value other than "no".
			 */
			if ((p[0] == 'n' || p[0] == 'N') &&
			    (p[1] == 'o' || p[1] == 'O')) {
				for (p += 2; isspace((unsigned char)*p); ++p)
					;
				if (*p == '\0')
					break;	/* Auto-Submitted: no */
			}
			exit(0);
		case 'F':		/* "From " */
		case 'f':
			cont = 0;
			if (!strncasecmp(buf, "From ", 5)) {
				for (p = buf + 5; *p && *p != ' '; ++p)
					;
				*p = '\0';
				(void)strlcpy(from, buf + 5, sizeof(from));
				from[strcspn(from, "\n")] = '\0';
				if (junkmail())
					exit(0);
			}
			break;
		case 'L':		/* "List-Id:" */
		case 'l':
			cont = 0;
			/*
			 * If present (with any value), message is coming from a
			 * mailing list, cf. RFC2919.
			 */
			if (strncasecmp(buf, "List-Id:", 8) == 0)
				exit(0);
			break;
		case 'R':		/* "Return-Path:" */
		case 'r':
			cont = 0;
			if (strncasecmp(buf, "Return-Path:",
			    sizeof("Return-Path:")-1) ||
			    (buf[12] != ' ' && buf[12] != '\t'))
				break;
			for (p = buf + 12; isspace((unsigned char)*p); ++p)
				;
			if (strlcpy(from, p, sizeof(from)) >= sizeof(from)) {
				syslog(LOG_NOTICE,
				    "Return-Path %s exceeds limits", p);
				exit(1);
			}
			from[strcspn(from, "\n")] = '\0';
			if (junkmail())
				exit(0);
			break;
		case 'P':		/* "Precedence:" */
		case 'p':
			cont = 0;
			if (strncasecmp(buf, "Precedence:", 11))
				break;
			for (p = buf + 11; isspace((unsigned char)*p); ++p)
				;
			if (!strncasecmp(p, "junk", 4) ||
			    !strncasecmp(p, "bulk", 4) ||
			    !strncasecmp(p, "list", 4))
				exit(0);
			break;
		case 'S':		/* Subject: */
		case 's':
			cont = 0;
			if (strncasecmp(buf, "Subject:",
			    sizeof("Subject:")-1) ||
			    (buf[8] != ' ' && buf[8] != '\t'))
				break;
			for (p = buf + 8; isspace((unsigned char)*p); ++p)
				;
			if (strlcpy(subj, p, sizeof(subj)) >= sizeof(subj)) {
				syslog(LOG_NOTICE,
				    "Subject %s exceeds limits", p);
				exit(1);
			}
			subj[strcspn(subj, "\n")] = '\0';
			break;
		case 'C':		/* "Cc:" */
		case 'c':
			if (strncasecmp(buf, "Cc:", 3))
				break;
			cont = 1;
			goto findme;
		case 'T':		/* "To:" */
		case 't':
			if (strncasecmp(buf, "To:", 3))
				break;
			cont = 1;
			goto findme;
		default:
			if (!isspace((unsigned char)*buf) || !cont || tome) {
				cont = 0;
				break;
			}
findme:			for (cur = names; !tome && cur; cur = cur->next)
				tome += nsearch(cur->name, buf);
		}
	if (!tome)
		exit(0);
	if (!*from) {
		syslog(LOG_NOTICE,
		    "no initial \"From\" or \"Return-Path\"line.");
		exit(1);
	}
}

/*
 * nsearch --
 *	do a nice, slow, search of a string for a substring.
 */
int
nsearch(char *name, char *str)
{
	int len;

	for (len = strlen(name); *str; ++str)
		if (!strncasecmp(name, str, len))
			return(1);
	return(0);
}

/*
 * junkmail --
 *	read the header and return if automagic/junk/bulk/list mail
 */
int
junkmail(void)
{
	static struct ignore {
		char	*name;
		int	len;
	} ignore[] = {
		{ "-request", 8 },
		{ "postmaster", 10 },
		{ "uucp", 4 },
		{ "mailer-daemon", 13 },
		{ "mailer", 6 },
		{ "-relay", 6 },
		{ NULL, 0 }
	};
	struct ignore *cur;
	int len;
	char *p;

	/*
	 * This is mildly amusing, and I'm not positive it's right; trying
	 * to find the "real" name of the sender, assuming that addresses
	 * will be some variant of:
	 *
	 * From site!site!SENDER%site.domain%site.domain@site.domain
	 */
	if (!(p = strchr(from, '%'))) {
		if (!(p = strchr(from, '@'))) {
			if ((p = strrchr(from, '!')))
				++p;
			else
				p = from;
			for (; *p; ++p)
				;
		}
	}
	len = p - from;
	for (cur = ignore; cur->name; ++cur)
		if (len >= cur->len &&
		    !strncasecmp(cur->name, p - cur->len, cur->len))
			return(1);
	return(0);
}

#define	VIT	"__VACATION__INTERVAL__TIMER__"

/*
 * recent --
 *	find out if user has gotten a vacation message recently.
 *	use bcopy for machines with alignment restrictions
 */
int
recent(void)
{
	time_t then, next;
	DBT key, data;

	/* get interval time */
	key.data = VIT;
	key.size = sizeof(VIT);
	if ((db->get)(db, &key, &data, 0))
		next = SECSPERDAY * 7;
	else
		bcopy(data.data, &next, sizeof(next));

	/* get record for this address */
	key.data = from;
	key.size = strlen(from);
	if (!(db->get)(db, &key, &data, 0)) {
		bcopy(data.data, &then, sizeof(then));
		if (next == 0 ||
		    then + next > time(NULL))
			return(1);
	}
	return(0);
}

/*
 * setinterval --
 *	store the reply interval
 */
void
setinterval(time_t interval)
{
	DBT key, data;

	key.data = VIT;
	key.size = sizeof(VIT);
	data.data = &interval;
	data.size = sizeof(interval);
	(void)(db->put)(db, &key, &data, 0);
}

/*
 * setreply --
 *	store that this user knows about the vacation.
 */
void
setreply(void)
{
	DBT key, data;
	time_t now;

	key.data = from;
	key.size = strlen(from);
	(void)time(&now);
	data.data = &now;
	data.size = sizeof(now);
	(void)(db->put)(db, &key, &data, 0);
}

/*
 * sendmessage --
 *	exec sendmail to send the vacation file to sender
 */
void
sendmessage(char *myname)
{
	char buf[MAXLINE];
	FILE *mfp, *sfp;
	int pvect[2], i;

	mfp = fopen(VMSG, "r");
	if (mfp == NULL) {
		syslog(LOG_NOTICE, "no ~%s/%s file.", myname, VMSG);
		exit(1);
	}
	if (pipe(pvect) < 0) {
		syslog(LOG_ERR, "pipe: %m");
		exit(1);
	}
	i = vfork();
	if (i < 0) {
		syslog(LOG_ERR, "fork: %m");
		exit(1);
	}
	if (i == 0) {
		dup2(pvect[0], 0);
		close(pvect[0]);
		close(pvect[1]);
		close(fileno(mfp));
		execl(_PATH_SENDMAIL, "sendmail", "-f", myname, "--",
		    from, (char *)NULL);
		syslog(LOG_ERR, "can't exec %s: %m", _PATH_SENDMAIL);
		_exit(1);
	}
	close(pvect[0]);
	sfp = fdopen(pvect[1], "w");
	if (sfp == NULL) {
		/* XXX could not fdopen; likely out of memory */
		fclose(mfp);
		close(pvect[1]);
		return;
	}
	fprintf(sfp, "To: %s\n", from);
	fputs("Auto-Submitted: auto-replied\n", sfp);
	while (fgets(buf, sizeof buf, mfp)) {
		char *s = strstr(buf, "$SUBJECT");

		if (s) {
			*s = 0;
			fputs(buf, sfp);
			fputs(subj, sfp);
			fputs(s+8, sfp);
		} else {
			fputs(buf, sfp);
		}
	}
	fclose(mfp);
	fclose(sfp);
}

void
usage(void)
{
	syslog(LOG_NOTICE, "uid %u: usage: vacation [-i] [-a alias] login",
	    getuid());
	exit(1);
}
