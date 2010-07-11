/*	$OpenBSD: value.c,v 1.32 2010/07/11 23:16:42 chl Exp $	*/
/*	$NetBSD: value.c,v 1.6 1997/02/11 09:24:09 mrg Exp $	*/

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

#include <paths.h>

#include "tip.h"

/*
 * Variable manipulation.
 */

value_t vtable[] = {
	{ "beautify",	  V_BOOL,	       "be",     NULL, 0 },
	{ "baudrate",	  V_NUMBER,	       "ba",     NULL, 0 },
	{ "connect",      V_STRING|V_READONLY, "cm",     NULL, 0 },
	{ "device",       V_STRING|V_READONLY, "dv",     NULL, 0 },
	{ "eofread",	  V_STRING,	       "eofr",   NULL, 0 },
	{ "eofwrite",	  V_STRING,	       "eofw",   NULL, 0 },
	{ "eol",	  V_STRING,	       NULL,     NULL, 0 },
	{ "escape",	  V_CHAR,	       "es",     NULL, 0 },
	{ "exceptions",	  V_STRING,	       "ex",     NULL, 0 },
	{ "force",	  V_CHAR,	       "fo",     NULL, 0 },
	{ "framesize",	  V_NUMBER,	       "fr",     NULL, 0 },
	{ "host",	  V_STRING|V_READONLY, "ho",     NULL, 0 },
	{ "log",	  V_STRING,	       NULL,     NULL, 0 },
	{ "prompt",	  V_CHAR,	       "pr",     NULL, 0 },
	{ "raise",	  V_BOOL,	       "ra",     NULL, 0 },
	{ "raisechar",	  V_CHAR,	       "rc",     NULL, 0 },
	{ "record",	  V_STRING,	       "rec",    NULL, 0 },
	{ "remote",	  V_STRING|V_READONLY, NULL,     NULL, 0 },
	{ "script",	  V_BOOL,	       "sc",     NULL, 0 },
	{ "tabexpand",	  V_BOOL,	       "tab",    NULL, 0 },
	{ "verbose",	  V_BOOL,	       "verb",   NULL, 0 },
	{ "SHELL",	  V_STRING,	       NULL,     NULL, 0 },
	{ "HOME",	  V_STRING,	       NULL,	 NULL, 0 },
	{ "echocheck",	  V_BOOL,	       "ec",	 NULL, 0 },
	{ "disconnect",	  V_STRING,	       "di",	 NULL, 0 },
	{ "tandem",	  V_BOOL,	       "ta",	 NULL, 0 },
	{ "linedelay",	  V_NUMBER,	       "ldelay", NULL, 0 },
	{ "chardelay",	  V_NUMBER,	       "cdelay", NULL, 0 },
	{ "etimeout",	  V_NUMBER,	       "et",	 NULL, 0 },
	{ "rawftp",	  V_BOOL,	       "raw",	 NULL, 0 },
	{ "halfduplex",	  V_BOOL,	       "hdx",	 NULL, 0 },
	{ "localecho",	  V_BOOL,	       "le",	 NULL, 0 },
	{ "parity",	  V_STRING,	       "par",	 NULL, 0 },
	{ "hardwareflow", V_BOOL,	       "hf",	 NULL, 0 },
	{ "linedisc",	  V_NUMBER,	       "ld",	 NULL, 0 },
	{ "direct",	  V_BOOL,	       "dc",	 NULL, 0 },
	{ NULL,           0,	                NULL,    NULL, 0 },
};

static int	vlookup(char *);
static void	vtoken(char *);
static size_t	vprint(value_t *);
static void	vprintall(void);
static char    *vinterp(char *, int);

/* Get a string value. */
char *
vgetstr(int value)
{
	value_t	*vp = &vtable[value];
	int	 type;

	type = vp->v_flags & V_TYPEMASK;
	if (type != V_STRING)
		errx(1, "variable %s not a string", vp->v_name);
	return (vp->v_string);
}

/* Get a number value. */
int
vgetnum(int value)
{
	value_t	*vp = &vtable[value];
	int	 type;

	type = vp->v_flags & V_TYPEMASK;
	if (type != V_NUMBER && type != V_BOOL && type != V_CHAR)
		errx(1, "variable %s not a number", vp->v_name);
	return (vp->v_number);
}

/* Set a string value. */
void
vsetstr(int value, char *string)
{
	value_t	*vp = &vtable[value];
	int	 type;

	type = vp->v_flags & V_TYPEMASK;
	if (type != V_STRING)
		errx(1, "variable %s not a string", vp->v_name);

	if (value == RECORD && string != NULL)
		string = expand(string);

	free(vp->v_string);
	if (string != NULL) {
		vp->v_string = strdup(string);
		if (vp->v_string == NULL)
			err(1, "strdup");
	} else
		vp->v_string = NULL;
}

/* Set a number value. */
void
vsetnum(int value, int number)
{
	value_t	*vp = &vtable[value];
	int	 type;

	type = vp->v_flags & V_TYPEMASK;
	if (type != V_NUMBER && type != V_BOOL && type != V_CHAR)
		errx(1, "variable %s not a number", vp->v_name);

	vp->v_number = number;
}

/* Print a single variable and its value. */
static size_t
vprint(value_t *p)
{
	char	*cp;
	size_t	 width;

	width = size(p->v_name);
	switch (p->v_flags & V_TYPEMASK) {
	case V_BOOL:
		if (!p->v_number) {
			width++;
			putchar('!');
		}
		printf("%s", p->v_name);
		break;
	case V_STRING:
		printf("%s=", p->v_name);
		width++;
		if (p->v_string) {
			cp = interp(p->v_string);
			width += size(cp);
			printf("%s", cp);
		}
		break;
	case V_NUMBER:
		width += 6;
		printf("%s=%-5d", p->v_name, p->v_number);
		break;
	case V_CHAR:
		printf("%s=", p->v_name);
		width++;
		if (p->v_number) {
			cp = ctrl(p->v_number);
			width += size(cp);
			printf("%s", cp);
		}
		break;
	}
	return (width);
}

/* Print all variables. */
static void
vprintall(void)
{
	value_t	*vp;
	size_t	 width;

#define MIDDLE 35
	width = 0;
	for (vp = vtable; vp->v_name; vp++) {
		if (vp->v_flags & V_READONLY)
			continue;
		if (width > 0 && width < MIDDLE) {
			while (width++ < MIDDLE)
				putchar(' ');
		}
		width += vprint(vp);
		if (width > MIDDLE) {
			printf("\r\n");
			width = 0;
		}
	}
#undef MIDDLE
}

/* Find index of variable by name or abbreviation. */
static int
vlookup(char *s)
{
	value_t *vp;
	u_int	 i;

	for (i = 0; vtable[i].v_name != NULL; i++) {
		vp = &vtable[i];
		if (strcmp(vp->v_name, s) == 0 ||
		    (vp->v_abbrev != NULL && strcmp(vp->v_abbrev, s) == 0))
			return (i);
	}
	return (-1);
}

void
vinit(void)
{
	struct passwd	*pw;
	value_t		*vp;
	char		 file[FILENAME_MAX], *cp;
	int		 written;
	FILE		*fp;

	/* Read environment variables. */
	if ((cp = getenv("HOME")) != NULL)
		vsetstr(HOME, cp);
	else {
		pw = getpwuid(getuid());
		if (pw != NULL && pw->pw_dir != NULL)
			vsetstr(HOME, pw->pw_dir);
		else
			vsetstr(HOME, "/");
	}
	if ((cp = getenv("SHELL")) != NULL)
		vsetstr(SHELL, cp);
	else
		vsetstr(SHELL, _PATH_BSHELL);

	/* Clear the table and set the defaults. */
	for (vp = vtable; vp->v_name != NULL; vp++) {
		vp->v_string = NULL;
		vp->v_number = 0;
	}
	vsetnum(BEAUTIFY, 1);
	vsetnum(ESCAPE, '~');
	vsetnum(FORCE, CTRL('p'));
	vsetnum(PROMPT, '\n');
	vsetnum(TAND, 1);
	vsetnum(VERBOSE, 1);
	vsetstr(LOG, _PATH_ACULOG);

	/* Read the .tiprc file in the HOME directory. */
	written = snprintf(file, sizeof(file), "%s/.tiprc", vgetstr(HOME));
	if (written < 0 || written >= sizeof(file)) {
		(void)fprintf(stderr, "Home directory path too long: %s\n",
		    vgetstr(HOME));
	} else {
		if ((fp = fopen(file, "r")) != NULL) {
			char *tp;

			while (fgets(file, sizeof(file), fp) != NULL) {
				if (vflag)
					printf("set %s", file);
				if ((tp = strrchr(file, '\n')))
					*tp = '\0';
				vlex(file);
			}
			fclose(fp);
		}
	}
}

void
vlex(char *s)
{
	char *cp;

	if (strcmp(s, "all") == 0)
		vprintall();
	else {
		do {
			if ((cp = vinterp(s, ' ')))
				cp++;
			vtoken(s);
			s = cp;
		} while (s);
	}
}

/* Set a variable from a token. */
static void
vtoken(char *s)
{
	value_t 	*vp;
	int	 	i, value;
	char		*cp;
	const char	*cause;

	if ((cp = strchr(s, '='))) {
		*cp = '\0';
		if ((i = vlookup(s)) != -1) {
			vp = &vtable[i];
			if (vp->v_flags & V_READONLY) {
				printf("access denied\r\n");
				return;
			}
			cp++;
			switch (vp->v_flags & V_TYPEMASK) {
			case V_STRING:
				vsetstr(i, cp);
				break;
			case V_BOOL:
				vsetnum(i, 1);
				break;
			case V_NUMBER:
				value = strtonum(cp, 0, INT_MAX, &cause);
				if (cause != NULL) {
					printf("%s: number %s\r\n", s, cause);
					return;
				}
				vsetnum(i, value);
				break;
			case V_CHAR:
				if (cp[0] != '\0' && cp[1] != '\0') {
					printf("%s: character too big\r\n", s);
					return;
				}
				vsetnum(i, *cp);
			}
			vp->v_flags |= V_CHANGED;
			return;
		}
	} else if ((cp = strchr(s, '?'))) {
		*cp = '\0';
		if ((i = vlookup(s)) != -1) {
			if (vprint(&vtable[i]) > 0)
				printf("\r\n");
			return;
		}
	} else {
		if (*s != '!')
			i = vlookup(s);
		else
			i = vlookup(s + 1);
		if (i != -1) {
			vp = &vtable[i];
			if (vp->v_flags & V_READONLY) {
				printf("%s: access denied\r\n", s);
				return;
			}
			if ((vp->v_flags & V_TYPEMASK) != V_BOOL) {
				printf("%s: not a boolean\r\n", s);
				return;
			}
			vsetnum(i, *s != '!');
			vp->v_flags |= V_CHANGED;
			return;
		}
	}
	printf("%s: unknown variable\r\n", s);
}

static char *
vinterp(char *s, int stop)
{
	char *p = s, c;
	int num;

	while ((c = *s++) && c != stop) {
		switch (c) {

		case '^':
			if (*s)
				*p++ = *s++ - 0100;
			else
				*p++ = c;
			break;

		case '\\':
			num = 0;
			c = *s++;
			if (c >= '0' && c <= '7')
				num = (num<<3)+(c-'0');
			else {
				char *q = "n\nr\rt\tb\bf\f";

				for (; *q; q++)
					if (c == *q++) {
						*p++ = *q;
						goto cont;
					}
				*p++ = c;
			cont:
				break;
			}
			if ((c = *s++) >= '0' && c <= '7') {
				num = (num<<3)+(c-'0');
				if ((c = *s++) >= '0' && c <= '7')
					num = (num<<3)+(c-'0');
				else
					s--;
			} else
				s--;
			*p++ = num;
			break;

		default:
			*p++ = c;
		}
	}
	*p = '\0';
	return (c == stop ? s-1 : NULL);
}
