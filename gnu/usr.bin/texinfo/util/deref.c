/*
 * deref.c
 *
 * Make all texinfo references into the one argument form.
 * 
 * Arnold Robbins
 * arnold@gnu.org
 * Written: December, 1991
 * Released: November, 1998
 *
 * Copyright, 1991, 1998 Arnold David Robbins
 *
 * DEREF is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * DEREF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 */

/*
 * LIMITATIONS:
 *	One texinfo cross reference per line.
 *	Cross references may not cross newlines.
 *	Use of fgets for input (to be fixed).
 */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/* for gcc on the 3B1, delete if this gives you grief */
extern int fclose(FILE *fp);
extern int fprintf(FILE *fp, const char *str, ...);
/* extern int sprintf(char *str, const char *fmt, ...); */
extern int fputs(char *buf, FILE *fp);

extern char *strerror(int errno);
extern char *strchr(char *cp, int ch);
extern int strncmp(const char *s1, const char *s2, int count);

extern int errno;

void process(FILE *fp);
void repair(char *line, char *ref, int toffset);

int Errs = 0;
char *Name = "stdin";
int Line = 0;
char *Me;

/* main --- handle arguments, global vars for errors */

int
main(int argc, char **argv)
{
	FILE *fp;

	Me = argv[0];

	if (argc == 1)
		process(stdin);
	else
		for (argc--, argv++; *argv != NULL; argc--, argv++) {
			if (argv[0][0] == '-' && argv[0][1] == '\0') {
				Name = "stdin";
				Line = 0;
				process(stdin);
			} else if ((fp = fopen(*argv, "r")) != NULL) {
				Name = *argv;
				Line = 0;
				process(fp);
				fclose(fp);
			} else {
				fprintf(stderr, "%s: can not open: %s\n",
					*argv, strerror(errno));
				Errs++;
			}
		}
	return Errs != 0;
}

/* isref --- decide if we've seen a texinfo cross reference */

int
isref(char *cp)
{
	if (strncmp(cp, "@ref{", 5) == 0)
		return 5;
	if (strncmp(cp, "@xref{", 6) == 0)
		return 6;
	if (strncmp(cp, "@pxref{", 7) == 0)
		return 7;
	return 0;
}

/* process --- read files, look for references, fix them up */

void
process(FILE *fp)
{
	char buf[BUFSIZ];
	char *cp;
	int count;

	while (fgets(buf, sizeof buf, fp) != NULL) {
		Line++;
		cp = strchr(buf, '@');
		if (cp == NULL) {
			fputs(buf, stdout);
			continue;
		}
		do {
			count = isref(cp);
			if (count == 0) {
				cp++;
				cp = strchr(cp, '@');
				if (cp == NULL) {
					fputs(buf, stdout);
					goto next;
				}
				continue;
			}
			/* got one */
			repair(buf, cp, count);
			break;
		} while (cp != NULL);
	next: ;
	}
}

/* repair --- turn all texinfo cross references into the one argument form */

void
repair(char *line, char *ref, int toffset)
{
	int braces = 1;		/* have seen first left brace */
	char *cp;

	ref += toffset;

	/* output line up to and including left brace in reference */
	for (cp = line; cp <= ref; cp++)
		putchar(*cp);

	/* output node name */
	for (; *cp && *cp != '}' && *cp != ',' && *cp != '\n'; cp++)
		putchar(*cp);

	if (*cp != '}')	{	/* could have been one arg xref */
		/* skip to matching right brace */
		for (; braces > 0; cp++) {
			switch (*cp) {
			case '@':
				cp++;	/* blindly skip next character */
				break;
			case '{':
				braces++;
				break;
			case '}':
				braces--;
				break;
			case '\n':
			case '\0':
				Errs++;
				fprintf(stderr,
					"%s: %s: %d: mismatched braces\n",
					Me, Name, Line);
				goto out;
			default:
				break;
			}
		}
	out:
		;
	}

	putchar('}');
	if (*cp == '}')
		cp++;

	/* now the rest of the line */
	for (; *cp; cp++)
		putchar(*cp);
	return;
}

/* strerror --- return error string, delete if in your library */

char *
strerror(int errno)
{
	static char buf[100];
	extern int sys_nerr;
	extern char *sys_errlist[];

	if (errno < sys_nerr && errno >= 0)
		return sys_errlist[errno];

	sprintf(buf, "unknown error %d", errno);
	return buf;
}
