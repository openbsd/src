/*	$OpenBSD: mkalias.c,v 1.10 2002/07/19 02:38:40 deraadt Exp $ */

/*
 * Copyright (c) 1997 Mats O Jansson <moj@stacken.kth.se>
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
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$OpenBSD: mkalias.c,v 1.10 2002/07/19 02:38:40 deraadt Exp $";
#endif

#include <ctype.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ypdb.h"
#include "ypdef.h"

extern char *__progname;		/* from crt0.o */

void
split_address(char *address, int len, char *user, char *host)
{
	char *c, *s, *r;
	int  i = 0;

	if (strchr(address, '@')) {
		s = user;
		for (c = address; i < len; i++) {
			if (*c == '@') {
				*s = '\0';
				s = host;
			} else {
				*s++ = *c;
			}
			c++;
		}
		*s = '\0';
	}

	if ((r = strrchr(address, '!'))) {
		s = host;
		for (c = address; i < len; i++) {
			if (c == r) {
				*s = '\0';
				s = user;
			} else {
				*s++ = *c;
			}
			c++;
		}
		*s = '\0';
	}
}

int
check_host(char *address, char *host, int dflag, int uflag, int Eflag)
{
	char answer[PACKETSZ];
	int  status;

	if ((dflag && strchr(address, '@')) ||
	    (uflag && strchr(address, '!')))
		return(0);

	if ((_res.options & RES_INIT) == 0)
		res_init();

	status = res_search(host, C_IN, T_AAAA, answer, sizeof(answer));

	if (status == -1)
		status = res_search(host, C_IN, T_A, answer, sizeof(answer));

	if (status == -1 && Eflag)
		status = res_search(host, C_IN, T_MX, answer, sizeof(answer));

	return(status == -1);
}

void
capitalize(char *name, int len)
{
	char last = ' ';
	char *c;
	int  i = 0;

	for (c = name; i < len; i++) {
		if (*c == '.')
			last = '.';
		c++;
	}

	i = 0;
	if (last == '.') {
		for (c = name; i < len; i++) {
			if (last == '.')
				*c = toupper(*c);
			last = *c++;
		}
	}
}

int
main(int argc, char *argv[])
{
	int	usage = 0;
	int	eflag = 0;
	int	dflag = 0;
	int	nflag = 0;
	int	sflag = 0;
	int	uflag = 0;
	int	vflag = 0;
	int	Eflag = 0;
	int	ch, fd;
	char	*input = NULL;
	char	*output = NULL;
	DBM	*db;
	datum	key, val;
	char	*slash;
	DBM	*new_db = NULL;
	static	char mapname[] = "ypdbXXXXXXXXXX";
	char	db_mapname[MAXPATHLEN], db_outfile[MAXPATHLEN],
		db_tempname[MAXPATHLEN];
	int	status;
	char	user[4096], host[4096]; /* XXX: DB bsize = 4096 in ypdb.c */
	char	datestr[11];
	char	myname[MAXHOSTNAMELEN];

	while ((ch = getopt(argc, argv, "Edensuv")) != -1)
		switch (ch) {
		case 'E':
			eflag++;	/* Check hostname */
			Eflag++;	/* .. even check MX records */
			break;
		case 'd':
			dflag++;	/* Don't check DNS hostname */
			break;
		case 'e':
			eflag++;	/* Check hostname */
			break;
		case 'n':
			nflag++;	/* Capitalize name parts */
			break;
		case 's':
			sflag++;	/* Don't know... */
			break;
		case 'u':
			uflag++;	/* Don't check UUCP hostname */
			break;
		case 'v':
			vflag++;	/* Verbose */
			break;
		default:
			usage++;
			break;
		}

	if (optind == argc) {
		usage++;
	} else {
		input = argv[optind++];
		if (optind < argc)
			output = argv[optind++];
		if (optind < argc)
			usage++;
	}

	if (usage) {
		fprintf(stderr,
		    "usage: %s [-v] [-e|-E [-d] [-u]] [-n] input [output]\n",
		    __progname);
		exit(1);
	}

	db = ypdb_open(input, O_RDONLY, 0444);
	if (db == NULL) {
		fprintf(stderr,
		    "%s: Unable to open input database %s\n",
		    __progname, input);
		exit(1);
	}

	if (output != NULL) {
		if (strlen(output) + strlen(YPDB_SUFFIX) > MAXPATHLEN) {
			fprintf(stderr,"%s: %s: file name too long\n",
			    __progname, output);
		}
		snprintf(db_outfile, sizeof(db_outfile),
		    "%s%s", output, YPDB_SUFFIX);

		slash = strrchr(output, '/');
		if (slash != NULL)
			slash[1] = 0;			/* truncate to dir */
		else
			*output = 0;			/* elminate */

		/* note: output is now directory where map goes ! */

		if (strlen(output) + strlen(mapname) +
		    strlen(YPDB_SUFFIX) > MAXPATHLEN) {
			fprintf(stderr,"%s: %s: directory name too long\n",
				__progname, output);
			exit(1);
		}

		snprintf(db_tempname, sizeof(db_tempname), "%s%s%s", output,
		    mapname, YPDB_SUFFIX);
		fd = mkstemps(db_tempname, 3);
		if (fd == -1)
			goto fail;
		close(fd);

		strncpy(db_mapname, db_tempname, strlen(db_tempname) - 3);
		db_mapname[sizeof(db_mapname) - 1] = '\0';

		new_db = ypdb_open(db_mapname, O_RDWR|O_TRUNC, 0444);
		if (new_db == NULL) {
fail:
			if (fd != -1)
				unlink(db_tempname);
			fprintf(stderr,
			    "%s: Unable to open output database %s\n",
			    __progname, db_outfile);
			exit(1);
		}
	}

	for (key = ypdb_firstkey(db); key.dptr != NULL;
	    key = ypdb_nextkey(db)) {
		val = ypdb_fetch(db, key);

		if (val.dptr == NULL)
			continue;			/* No value */
		if (*key.dptr == '@' && key.dsize == 1)
			continue;			/* Sendmail token */
		if (strncmp(key.dptr, "YP_", 3)==0)	/* YP token */
			continue;
		if (strchr(val.dptr, ','))
			continue;			/* List... */
		if (strchr(val.dptr, '|'))
			continue;			/* Pipe... */

		if (!(strchr(val.dptr, '@') || strchr(val.dptr, '!')))
			continue;		/* Skip local users */

		split_address(val.dptr, val.dsize, user, host);

		if (eflag && check_host(val.dptr, host, dflag, uflag, Eflag)) {
			printf("Invalid host %s in %*.*s:%*.*s\n",
			    host, key.dsize, key.dsize, key.dptr,
			    val.dsize, val.dsize, val.dptr);
			continue;
		}

		if (nflag)
			capitalize(key.dptr, key.dsize);

		if (new_db != NULL) {
			status = ypdb_store(new_db, val, key, YPDB_INSERT);
			if (status != 0) {
				printf("%s: problem storing %*.*s %*.*s\n",
				    __progname, val.dsize, val.dsize, val.dptr,
				    key.dsize, key.dsize, key.dptr);
			}
		}

		if (vflag) {
			printf("%*.*s --> %*.*s\n",
			    val.dsize, val.dsize, val.dptr,
			    key.dsize, key.dsize, key.dptr);
		}

	}

	if (new_db != NULL) {
		snprintf(datestr, sizeof datestr, "%010u", time(NULL));
		key.dptr = YP_LAST_KEY;
		key.dsize = strlen(YP_LAST_KEY);
		val.dptr = datestr;
		val.dsize = strlen(datestr);
		status = ypdb_store(new_db, key, val, YPDB_INSERT);
		if (status != 0) {
			printf("%s: problem storing %*.*s %*.*s\n",
			    __progname,
			    key.dsize, key.dsize, key.dptr,
			    val.dsize, val.dsize, val.dptr);
		}
	}

	if (new_db != NULL) {
		gethostname(myname, sizeof(myname));
		key.dptr = YP_MASTER_KEY;
		key.dsize = strlen(YP_MASTER_KEY);
		val.dptr = myname;
		val.dsize = strlen(myname);
		status = ypdb_store(new_db, key, val, YPDB_INSERT);
		if (status != 0) {
			printf("%s: problem storing %*.*s %*.*s\n",
			    __progname,
			    key.dsize, key.dsize, key.dptr,
			    val.dsize, val.dsize, val.dptr);
		}
	}

	ypdb_close(db);

	if (new_db != NULL) {
		ypdb_close(new_db);
		if (rename(db_tempname, db_outfile) < 0) {
			perror("rename");
			fprintf(stderr,"rename %s -> %s failed!\n",
			    db_tempname, db_outfile);
			exit(1);
		}
	}
	return(0);
}
