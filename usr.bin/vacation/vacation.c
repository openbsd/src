/*	$OpenBSD: vacation.c,v 1.10 1998/07/08 21:39:08 deraadt Exp $	*/
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
static char copyright[] =
"@(#) Copyright (c) 1983, 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)vacation.c	8.2 (Berkeley) 1/26/94";
#endif
static char rcsid[] = "$OpenBSD: vacation.c,v 1.10 1998/07/08 21:39:08 deraadt Exp $";
#endif /* not lint */

/*
**  Vacation
**  Copyright (c) 1983  Eric P. Allman
**  Berkeley, California
*/

#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <db.h>
#include <time.h>
#include <syslog.h>
#include <tzfile.h>
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

int junkmail __P((void));
int nsearch __P((char *, char *));
void readheaders __P((void));
int recent __P((void));
void sendmessage __P((char *));
void setinterval __P((time_t));
void setreply __P((void));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct passwd *pw;
	struct stat sb;
	ALIAS *cur;
	time_t interval;
	int ch, iflag, flags;

	opterr = iflag = 0;
	interval = -1;
	while ((ch = getopt(argc, argv, "a:Iir:")) != -1)
		switch((char)ch) {
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
			if (isdigit(*optarg)) {
				interval = atol(optarg) * SECSPERDAY;
				if (interval < 0)
					usage();
			}
			else
				interval = (time_t)LONG_MAX;	/* XXX */
			break;
		case '?':
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
			    "vacation: no such user uid %u.", getuid());
			exit(1);
		}
	}
	else if (!(pw = getpwnam(*argv))) {
		syslog(LOG_ERR, "vacation: no such user %s.", *argv);
		exit(1);
	}
	if (chdir(pw->pw_dir)) {
		syslog(LOG_NOTICE,
		    "vacation: no such directory %s.", pw->pw_dir);
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
		syslog(LOG_NOTICE, "vacation: %s: %m", VDB);
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
	}
	else
		(void)(db->close)(db);
	exit(0);
	/* NOTREACHED */
}

/*
 * readheaders --
 *	read mail headers
 */
void
readheaders()
{
	register ALIAS *cur;
	register char *p;
	int tome, cont;
	char buf[MAXLINE];

	cont = tome = 0;
	while (fgets(buf, sizeof(buf), stdin) && *buf != '\n')
		switch(*buf) {
		case 'F':		/* "From " */
			cont = 0;
			if (!strncmp(buf, "From ", 5)) {
				for (p = buf + 5; *p && *p != ' '; ++p);
				*p = '\0';
				(void)strcpy(from, buf + 5);
				if (p = strchr(from, '\n'))
					*p = '\0';
				if (junkmail())
					exit(0);
			}
			break;
		case 'P':		/* "Precedence:" */
			cont = 0;
			if (strncasecmp(buf, "Precedence", 10) ||
			    buf[10] != ':' && buf[10] != ' ' && buf[10] != '\t')
				break;
			if (!(p = strchr(buf, ':')))
				break;
			while (*++p && isspace(*p));
			if (!*p)
				break;
			if (!strncasecmp(p, "junk", 4) ||
			    !strncasecmp(p, "bulk", 4) ||
			    !strncasecmp(p, "list", 4))
				exit(0);
			break;
		case 'C':		/* "Cc:" */
			if (strncmp(buf, "Cc:", 3))
				break;
			cont = 1;
			goto findme;
		case 'T':		/* "To:" */
			if (strncmp(buf, "To:", 3))
				break;
			cont = 1;
			goto findme;
		default:
			if (!isspace(*buf) || !cont || tome) {
				cont = 0;
				break;
			}
findme:			for (cur = names; !tome && cur; cur = cur->next)
				tome += nsearch(cur->name, buf);
		}
	if (!tome)
		exit(0);
	if (!*from) {
		syslog(LOG_NOTICE, "vacation: no initial \"From\" line.");
		exit(1);
	}
}

/*
 * nsearch --
 *	do a nice, slow, search of a string for a substring.
 */
int
nsearch(name, str)
	register char *name, *str;
{
	register int len;

	for (len = strlen(name); *str; ++str)
		if (*str == *name && !strncasecmp(name, str, len))
			return(1);
	return(0);
}

/*
 * junkmail --
 *	read the header and return if automagic/junk/bulk/list mail
 */
int
junkmail()
{
	static struct ignore {
		char	*name;
		int	len;
	} ignore[] = {
		"-request", 8,		"postmaster", 10,	"uucp", 4,
		"mailer-daemon", 13,	"mailer", 6,		"-relay", 6,
		NULL, NULL,
	};
	register struct ignore *cur;
	register int len;
	register char *p;

	/*
	 * This is mildly amusing, and I'm not positive it's right; trying
	 * to find the "real" name of the sender, assuming that addresses
	 * will be some variant of:
	 *
	 * From site!site!SENDER%site.domain%site.domain@site.domain
	 */
	if (!(p = strchr(from, '%')))
		if (!(p = strchr(from, '@'))) {
			if (p = strrchr(from, '!'))
				++p;
			else
				p = from;
			for (; *p; ++p);
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
recent()
{
	DBT key, data;
	time_t then, next;

	/* get interval time */
	key.data = VIT;
	key.size = sizeof(VIT);
	if ((db->get)(db, &key, &data, 0))
		next = SECSPERDAY * DAYSPERWEEK;
	else
		bcopy(data.data, &next, sizeof(next));

	/* get record for this address */
	key.data = from;
	key.size = strlen(from);
	if (!(db->get)(db, &key, &data, 0)) {
		bcopy(data.data, &then, sizeof(then));
		if (next == (time_t)LONG_MAX ||			/* XXX */
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
setinterval(interval)
	time_t interval;
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
setreply()
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
sendmessage(myname)
	char *myname;
{
	FILE *mfp, *sfp;
	int i;
	int pvect[2];
	char buf[MAXLINE];

	mfp = fopen(VMSG, "r");
	if (mfp == NULL) {
		syslog(LOG_NOTICE, "vacation: no ~%s/%s file.", myname, VMSG);
		exit(1);
	}
	if (pipe(pvect) < 0) {
		syslog(LOG_ERR, "vacation: pipe: %m");
		exit(1);
	}
	i = vfork();
	if (i < 0) {
		syslog(LOG_ERR, "vacation: fork: %m");
		exit(1);
	}
	if (i == 0) {
		dup2(pvect[0], 0);
		close(pvect[0]);
		close(pvect[1]);
		close(fileno(mfp));
		execl(_PATH_SENDMAIL, "sendmail", "-f", myname, "--",
		    from, NULL);
		syslog(LOG_ERR, "vacation: can't exec %s: %m", _PATH_SENDMAIL);
		_exit(1);
	}
	close(pvect[0]);
	sfp = fdopen(pvect[1], "w");
	fprintf(sfp, "To: %s\n", from);
	while (fgets(buf, sizeof buf, mfp))
		fputs(buf, sfp);
	fclose(mfp);
	fclose(sfp);
}

void
usage()
{
	syslog(LOG_NOTICE, "uid %u: usage: vacation [-i] [-a alias] login",
	    getuid());
	exit(1);
}
