/*	$NetBSD: size.c,v 1.7 1996/01/14 23:07:12 pk Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)size.c	8.2 (Berkeley) 12/9/93";
#endif
static char rcsid[] = "$NetBSD: size.c,v 1.7 1996/01/14 23:07:12 pk Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <a.out.h>
#include <ar.h>
#include <ranlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

int ignore_bad_archive_entries = 1;

int	process_file __P((int, char *));
int	show_archive __P((int, char *, FILE *));
int	show_object __P((int, char *, FILE *));
void	*emalloc __P((size_t));
void	*erealloc __P((void *, size_t));
void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, eval;

	while ((ch = getopt(argc, argv, "w")) != EOF)
		switch(ch) {
		case 'w':
			ignore_bad_archive_entries = 0;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	eval = 0;
	if (*argv)
		do {
			eval |= process_file(argc, *argv);
		} while (*++argv);
	else
		eval |= process_file(1, "a.out");
	exit(eval);
}

/*
 * process_file()
 *	show symbols in the file given as an argument.  Accepts archive and
 *	object files as input.
 */
int
process_file(count, fname)
	int count;
	char *fname;
{
	struct exec exec_head;
	FILE *fp;
	int retval;
	char magic[SARMAG];
    
	if (!(fp = fopen(fname, "r"))) {
		warnx("cannot read %s", fname);
		return(1);
	}

	/*
	 * first check whether this is an object file - read a object
	 * header, and skip back to the beginning
	 */
	if (fread((char *)&exec_head, sizeof(exec_head), (size_t)1, fp) != 1) {
		warnx("%s: bad format", fname);
		(void)fclose(fp);
		return(1);
	}
	rewind(fp);

	/* this could be an archive */
	if (N_BADMAG(exec_head)) {
		if (fread(magic, sizeof(magic), (size_t)1, fp) != 1 ||
		    strncmp(magic, ARMAG, SARMAG)) {
			warnx("%s: not object file or archive", fname);
			(void)fclose(fp);
			return(1);
		}
		retval = show_archive(count, fname, fp);
	} else
		retval = show_objfile(count, fname, fp);
	(void)fclose(fp);
	return(retval);
}

/*
 * show_archive()
 *	show symbols in the given archive file
 */
int
show_archive(count, fname, fp)
	int count;
	char *fname;
	FILE *fp;
{
	struct ar_hdr ar_head;
	struct exec exec_head;
	int i, rval;
	long last_ar_off;
	char *p, *name;
	int baselen, namelen;

	baselen = strlen(fname) + 3;
	namelen = sizeof(ar_head.ar_name);
	name = emalloc(baselen + namelen);

	rval = 0;

	/* while there are more entries in the archive */
	while (fread((char *)&ar_head, sizeof(ar_head), (size_t)1, fp) == 1) {
		/* bad archive entry - stop processing this archive */
		if (strncmp(ar_head.ar_fmag, ARFMAG, sizeof(ar_head.ar_fmag))) {
			warnx("%s: bad format archive header", fname);
			(void)free(name);
			return(1);
		}

		/* remember start position of current archive object */
		last_ar_off = ftell(fp);

		/* skip ranlib entries */
		if (!strncmp(ar_head.ar_name, RANLIBMAG, sizeof(RANLIBMAG) - 1))
			goto skip;

		/*
		 * construct a name of the form "archive.a:obj.o:" for the
		 * current archive entry if the object name is to be printed
		 * on each output line
		 */
		p = name;
		if (count > 1)
			p += sprintf(p, "%s:", fname);
#ifdef AR_EFMT1
		/*
		 * BSD 4.4 extended AR format: #1/<namelen>, with name as the
		 * first <namelen> bytes of the file
		 */
		if (		(ar_head.ar_name[0] == '#') &&
				(ar_head.ar_name[1] == '1') &&
				(ar_head.ar_name[2] == '/') && 
				(isdigit(ar_head.ar_name[3]))) {

			int len = atoi(&ar_head.ar_name[3]);
			if (len > namelen) {
				p -= (long)name;
				name = (char *)erealloc(name, baselen+len);
				namelen = len;
				p += (long)name;
			}
			if (fread(p, len, 1, fp) != 1) {
				(void)fprintf(stderr,
				    "nm: %s: premature EOF.\n", name);
				(void)free(name);
				return 1;
			}
			p += len;
		} else
#endif
		for (i = 0; i < sizeof(ar_head.ar_name); ++i)
			if (ar_head.ar_name[i] && ar_head.ar_name[i] != ' ')
				*p++ = ar_head.ar_name[i];
		*p++ = '\0';

		/* get and check current object's header */
		if (fread((char *)&exec_head, sizeof(exec_head),
		    (size_t)1, fp) != 1) {
			warnx("%s: premature EOF", name);
			(void)free(name);
			return(1);
		}

		if (N_BADMAG(exec_head)) {
			if (!ignore_bad_archive_entries) {
				warnx("%s: bad format", name);
				rval = 1;
			}
		} else {
			(void)fseek(fp, (long)-sizeof(exec_head),
			    SEEK_CUR);
			rval |= show_objfile(2, name, fp);
		}

		/*
		 * skip to next archive object - it starts at the next
	 	 * even byte boundary
		 */
#define even(x) (((x) + 1) & ~1)
skip:		if (fseek(fp, last_ar_off + even(atol(ar_head.ar_size)),
		    SEEK_SET)) {
			warn("%s", fname);
			(void)free(name);
			return(1);
		}
	}
	(void)free(name);
	return(rval);
}

int
show_objfile(count, name, fp)
	int count;
	char *name;
	FILE *fp;
{
	static int first = 1;
	struct exec head;
	u_long total;

	if (fread((char *)&head, sizeof(head), (size_t)1, fp) != 1) {
		warnx("%s: cannot read header", name);
		return(1);
	}

	if (N_BADMAG(head)) {
		warnx("%s: bad format", name);
		return(1);
	}

	if (first) {
		first = 0;
		(void)printf("text\tdata\tbss\tdec\thex\n");
	}
	total = head.a_text + head.a_data + head.a_bss;
	(void)printf("%lu\t%lu\t%lu\t%lu\t%lx", head.a_text, head.a_data,
	    head.a_bss, total, total);
	if (count > 1)
		(void)printf("\t%s", name);
	(void)printf("\n");
	return (0);
}

void *
emalloc(size)
	size_t size;
{
	char *p;

	/* NOSTRICT */
	if (p = malloc(size))
		return(p);
	err(1, NULL);
}

void *
erealloc(p, size)
	void   *p;
	size_t size;
{
	/* NOSTRICT */
	if (p = realloc(p, size))
		return(p);
	err(1, NULL);
}

void
usage()
{
	(void)fprintf(stderr, "usage: size [-w] [file ...]\n");
	exit(1);
}
