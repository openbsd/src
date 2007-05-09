/*	$OpenBSD: getinfo.c,v 1.14 2007/05/09 03:07:19 deraadt Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
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

#ifndef lint
static char rcsid[] = "$OpenBSD: getinfo.c,v 1.14 2007/05/09 03:07:19 deraadt Exp $";
#endif /* not lint */

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>	
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	BFRAG		1024
#define	BSIZE		1024
#define	ESC		('[' & 037)	/* ASCII ESC */
#define	MAX_RECURSION	32		/* maximum getent recursion */
#define	SFRAG		100		/* cgetstr mallocs in SFRAG chunks */

#define RECOK	(char)0
#define TCERR	(char)1
#define	SHADOW	(char)2

static int 	 getent(char **, u_int *, char **, int, char *, int);
static char	*igetcap(char *, char *, int);
static int	 igetmatch(char *, char *);
static int	 igetclose(void);

int	igetnext(char **, char **);

/*
 * Igetcap searches the capability record buf for the capability cap with
 * type `type'.  A pointer to the value of cap is returned on success, NULL
 * if the requested capability couldn't be found.
 *
 * Specifying a type of ',' means that nothing should follow cap (,cap,).
 * In this case a pointer to the terminating ',' or NUL will be returned if
 * cap is found.
 *
 * If (cap, '@') or (cap, terminator, '@') is found before (cap, terminator)
 * return NULL.
 */
static char *
igetcap(char *buf, char *cap, int type)
{
	char *bp, *cp;

	bp = buf;
	for (;;) {
		/*
		 * Skip past the current capability field - it's either the
		 * name field if this is the first time through the loop, or
		 * the remainder of a field whose name failed to match cap.
		 */
		for (;;)
			if (*bp == '\0')
				return (NULL);
			else
				if (*bp++ == ',')
					break;

		/*
		 * Try to match (cap, type) in buf.
		 */
		for (cp = cap; *cp == *bp && *bp != '\0'; cp++, bp++)
			continue;
		if (*cp != '\0')
			continue;
		if (*bp == '@')
			return (NULL);
		if (type == ',') {
			if (*bp != '\0' && *bp != ',')
				continue;
			return(bp);
		}
		if (*bp != type)
			continue;
		bp++;
		return (*bp == '@' ? NULL : bp);
	}
	/* NOTREACHED */
}

/*
 * Getent implements the functions of igetent.  If fd is non-negative,
 * *db_array has already been opened and fd is the open file descriptor.  We
 * do this to save time and avoid using up file descriptors for use=
 * recursions.
 *
 * Getent returns the same success/failure codes as igetent.  On success, a
 * pointer to a malloc'ed capability record with all use= capabilities fully
 * expanded and its length (not including trailing ASCII NUL) are left in
 * *cap and *len.
 *
 * Basic algorithm:
 *	+ Allocate memory incrementally as needed in chunks of size BFRAG
 *	  for capability buffer.
 *	+ Recurse for each use=name and interpolate result.  Stop when all
 *	  names interpolated, a name can't be found, or depth exceeds
 *	  MAX_RECURSION.
 */
static int
getent(char **cap, u_int *len, char **db_array, int fd, char *name, int depth)
{
	char *r_end, *rp, **db_p;
	int myfd, eof, foundit;
	char *record, *s;
	int tc_not_resolved;
	
	/*
	 * Return with ``loop detected'' error if we've recursed more than
	 * MAX_RECURSION times.
	 */
	if (depth > MAX_RECURSION)
		return (-3);

	/*
	 * Allocate first chunk of memory.
	 */
	if ((record = malloc(BFRAG)) == NULL) {
		errno = ENOMEM;
		return (-2);
	}
	r_end = record + BFRAG;
	foundit = 0;
	rp = NULL;
	myfd = -1;
	/*
	 * Loop through database array until finding the record.
	 */

	for (db_p = db_array; *db_p != NULL; db_p++) {
		eof = 0;

		/*
		 * Open database if not already open.
		 */

		if (fd >= 0) {
			(void)lseek(fd, (off_t)0, SEEK_SET);
			myfd = 0;
		} else {
			fd = open(*db_p, O_RDONLY, 0);
			if (fd < 0) {
				/* No error on unfound file. */
				continue;
			}
			myfd = 1;
		}
		/*
		 * Find the requested capability record ...
		 */
		{
		char buf[BUFSIZ];
		char *b_end, *bp;
		int c;

		/*
		 * Loop invariants:
		 *	There is always room for one more character in record.
		 *	R_end always points just past end of record.
		 *	Rp always points just past last character in record.
		 *	B_end always points just past last character in buf.
		 *	Bp always points at next character in buf.
		 */
		b_end = buf;
		bp = buf;
		for (;;) {

			/*
			 * Read in a line implementing (\, newline)
			 * line continuation.
			 */
			rp = record;
			for (;;) {
				if (bp >= b_end) {
					int n;
		
					n = read(fd, buf, sizeof(buf));
					if (n <= 0) {
						if (myfd)
							(void)close(fd);
						if (n < 0) {
							free(record);
							return (-2);
						} else {
							fd = -1;
							eof = 1;
							break;
						}
					}
					b_end = buf+n;
					bp = buf;
				}
	
				c = *bp++;
				if (c == '\n') {
					if (bp >= b_end) {
						int n;

						n = read(fd, buf, sizeof(buf));
						if (n <= 0) {
							if (myfd)
								(void)close(fd);
							if (n < 0) {
								free(record);
								return (-2);
							} else {
								fd = -1;
								eof = 1;
								break;
							}
						}
						b_end = buf+n;
						bp = buf;
					}
					if (rp > record && isspace(*bp))
						continue;
					else
						break;
				}
				if (rp <= record || *(rp - 1) != ',' || !isspace(c))
					*rp++ = c;

				/*
				 * Enforce loop invariant: if no room 
				 * left in record buffer, try to get
				 * some more.
				 */
				if (rp >= r_end) {
					u_int pos;
					size_t newsize;

					pos = rp - record;
					newsize = r_end - record + BFRAG;
					s = realloc(record, newsize);
					if (s == NULL) {
						free(record);
						errno = ENOMEM;
						if (myfd)
							(void)close(fd);
						return (-2);
					}
					record = s;
					r_end = record + newsize;
					rp = record + pos;
				}
			}
			/* loop invariant lets us do this */
			*rp++ = '\0';
				
			/*
			 * Toss blank lines and comments.
			 */
			if (*record == '\0' || *record == '#')
				continue;
	
			/*
			 * See if this is the record we want ...
			 */
			if (igetmatch(record, name) == 0) {
				foundit = 1;
				break;	/* found it! */
			}

			/*
			 * If encountered eof check next file.
			 */
			if (eof)
				break;
		}
	}
		if (foundit)
			break;
	}

	if (!foundit)
		return (-1);

	/*
	 * Got the capability record, but now we have to expand all use=name
	 * references in it ...
	 */
	{
		char *newicap;
		int newilen;
		u_int ilen;
		int diff, iret, tclen;
		char *icap, *scan, *tc, *tcstart, *tcend;

		/*
		 * Loop invariants:
		 *	There is room for one more character in record.
		 *	R_end points just past end of record.
		 *	Rp points just past last character in record.
		 *	Scan points at remainder of record that needs to be
		 *	scanned for use=name constructs.
		 */
		scan = record;
		tc_not_resolved = 0;
		for (;;) {
			if ((tc = igetcap(scan, "use", '=')) == NULL)
				break;

			/*
			 * Find end of use=name and stomp on the trailing `,'
			 * (if present) so we can use it to call ourselves.
			 */
			s = tc + strcspn(tc, ",");
			if (*s == ',') {
				*s = '\0';
				++s;
			}
			tcstart = tc - 4;
			tclen = s - tcstart;
			tcend = s;

			iret = getent(&icap, &ilen, db_p, fd, tc, depth+1);
			newicap = icap;		/* Put into a register. */
			newilen = ilen;
			if (iret != 0) {
				/* an error */
				if (iret < -1) {
					if (myfd)
						(void)close(fd);
					free(record);
					return (iret);
				}
				if (iret == 1)
					tc_not_resolved = 1;
				/* couldn't resolve tc */
				if (iret == -1) {
					*(s - 1) = ',';			
					scan = s - 1;
					tc_not_resolved = 1;
					continue;
					
				}
			}
			/* not interested in name field of tc'ed record */
			s = newicap + strcspn(newicap, ",");
			if (*s == ',')
				++s;
			newilen -= s - newicap;
			newicap = s;

			/* make sure interpolated record is `,'-terminated */
			s += newilen;
			if (*(s-1) != ',') {
				*s = ',';	/* overwrite NUL with , */
				newilen++;
			}

			/*
			 * Make sure there's enough room to insert the
			 * new record.
			 */
			diff = newilen - tclen;
			if (diff >= r_end - rp) {
				u_int pos, tcpos, tcposend;
				size_t newsize;

				pos = rp - record;
				newsize = r_end - record + diff + BFRAG;
				tcpos = tcstart - record;
				tcposend = tcend - record;
				s = realloc(record, newsize);
				if (s == NULL) {
					free(record);
					errno = ENOMEM;
					if (myfd)
						(void)close(fd);
					free(icap);
					return (-2);
				}
				record = s;
				r_end = record + newsize;
				rp = record + pos;
				tcstart = record + tcpos;
				tcend = record + tcposend;
			}

			/*
			 * Insert tc'ed record into our record.
			 */
			s = tcstart + newilen;
			bcopy(tcend, s, (size_t)(rp - tcend));
			bcopy(newicap, tcstart, (size_t)newilen);
			rp += diff;
			free(icap);

			/*
			 * Start scan on `,' so next igetcap works properly
			 * (igetcap always skips first field).
			 */
			scan = s-1;
		}
	
	}
	/*
	 * Close file (if we opened it), give back any extra memory, and
	 * return capability, length and success.
	 */
	if (myfd)
		(void)close(fd);
	*len = rp - record - 1;	/* don't count NUL */
	if (r_end > rp) {
		if ((s = 
		     realloc(record, (size_t)(rp - record))) == NULL) {
			free(record);
			errno = ENOMEM;
			return (-2);
		} else
			record = s;
	}
		
	*cap = record;
	if (tc_not_resolved)
		return (1);
	return (0);
}	

/*
 * Igetmatch will return 0 if name is one of the names of the capability
 * record buf, -1 if not.
 */
static int
igetmatch(char *buf, char *name)
{
	char *np, *bp;

	/*
	 * Start search at beginning of record.
	 */
	bp = buf;
	for (;;) {
		/*
		 * Try to match a record name.
		 */
		np = name;
		for (;;)
			if (*np == '\0') {
				if (*bp == '|' || *bp == ',' || *bp == '\0')
					return (0);
				else
					break;
			} else {
				if (*bp++ != *np++)
					break;
			}

		/*
		 * Match failed, skip to next name in record.
		 */
		bp--;	/* a '|' or ',' may have stopped the match */
		for (;;)
			if (*bp == '\0' || *bp == ',')
				return (-1);	/* match failed totally */
			else
				if (*bp++ == '|')
					break;	/* found next name */
	}
}

static FILE *pfp;
static int slash;
static char **dbp;

static int
igetclose(void)
{
	if (pfp != NULL) {
		(void)fclose(pfp);
		pfp = NULL;
	}
	dbp = NULL;
	slash = 0;
	return(0);
}

/*
 * Igetnext() gets either the first or next entry in the logical database 
 * specified by db_array.  It returns 0 upon completion of the database, 1
 * upon returning an entry with more remaining, and -1 if an error occurs.
 */
int
igetnext(char **bp, char **db_array)
{
	size_t len;
	int status, done;
	char *cp, *line, *rp, *np, buf[BSIZE], nbuf[BSIZE];
	u_int dummy;

	if (dbp == NULL)
		dbp = db_array;

	if (pfp == NULL && (pfp = fopen(*dbp, "r")) == NULL) {
		(void)igetclose();
		return (-1);
	}
	for (;;) {
		line = fgetln(pfp, &len);
		if (line == NULL) {
			if (ferror(pfp)) {
				(void)igetclose();
				return (-1);
			} else {
				(void)fclose(pfp);
				pfp = NULL;
				if (*++dbp == NULL) {
					(void)igetclose();
					return (0);
				} else if ((pfp =
				    fopen(*dbp, "r")) == NULL) {
					(void)igetclose();
					return (-1);
				} else
					continue;
			}
		} else
			line[len - 1] = '\0';/* XXX - assumes newline */
		if (len == 1) {
			slash = 0;
			continue;
		}
		if (isspace(*line) ||
		    *line == ',' || *line == '#' || slash) {
			if (line[len - 2] == '\\')
				slash = 1;
			else
				slash = 0;
			continue;
		}
		if (line[len - 2] == '\\')
			slash = 1;
		else
			slash = 0;

		/* 
		 * Line points to a name line.
		 */
		done = 0;
		np = nbuf;
		for (;;) {
			for (cp = line; *cp != '\0'; cp++) {
				if (*cp == ',') {
					*np++ = ',';
					done = 1;
					break;
				}
				*np++ = *cp;
			}
			if (done) {
				*np = '\0';
				break;
			} else { /* name field extends beyond the line */
				line = fgetln(pfp, &len);
				if (line == NULL) {
					if (ferror(pfp)) {
						(void)igetclose();
						return (-1);
					}
					/* Move on to next file. */
					(void)fclose(pfp);
					pfp = NULL;
					++dbp;
					/* NUL terminate nbuf. */
					*np = '\0';
					break;
				} else
					/* XXX - assumes newline */
					line[len - 1] = '\0';
			}
		}
		rp = buf;
		for(cp = nbuf; *cp != '\0'; cp++)
			if (*cp == '|' || *cp == ',')
				break;
			else
				*rp++ = *cp;

		*rp = '\0';
		status = getent(bp, &dummy, db_array, -1, buf, 0);
		if (status == -2 || status == -3)
			(void)igetclose();

		return (status + 1);
	}
	/* NOTREACHED */
}
