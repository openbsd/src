/*	$OpenBSD: wsconsctl.c,v 1.1 2000/07/01 23:52:45 mickey Exp $	*/
/*	$NetBSD: wsconsctl.c,v 1.2 1998/12/29 22:40:20 hannken Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <fcntl.h>
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "wsconsctl.h"

#define PATH_KEYBOARD		"/dev/wskbd0"
#define PATH_MOUSE		"/dev/wsmouse0"
#define PATH_DISPLAY		"/dev/ttyE0"

extern const char *__progname;		/* from crt0.o */

extern struct field keyboard_field_tab[];
extern struct field mouse_field_tab[];
extern struct field display_field_tab[];
extern int keyboard_field_tab_len;
extern int mouse_field_tab_len;
extern int display_field_tab_len;

static void usage __P((char *));
int main __P((int, char **));

static void
usage(msg)
	char *msg;
{
	if (msg != NULL)
		fprintf(stderr, "%s: %s\n\n", __progname, msg);

	fprintf(stderr, "usage: %s [-kmd] [-f file] [-n] name ...\n",
		__progname);
	fprintf(stderr, " -or-  %s [-kmd] [-f file] [-n] -w name=value ...\n",
		__progname);
	fprintf(stderr, " -or-  %s [-kmd] [-f file] [-n] -a\n", __progname);

	exit(1);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int i, ch, fd;
	int aflag, dflag, kflag, mflag, wflag;
	char *file, *sep, *p;
	struct field *f, *field_tab;
	int do_merge, field_tab_len;
	void (*getval) __P((int));
	void (*putval) __P((int));

	aflag = 0;
	dflag = 0;
	kflag = 0;
	mflag = 0;
	wflag = 0;
	file = NULL;
	sep = "=";

	while ((ch = getopt(argc, argv, "adf:kmnw")) != -1) {
		switch(ch) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			file = optarg;
			break;
		case 'k':
			kflag = 1;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'n':
			sep = NULL;
			break;
		case 'w':
			wflag = 1;
			break;
		case '?':
		default:
			usage(NULL);
		}
	}

	argc -= optind;
	argv += optind;

	if (dflag + kflag + mflag == 0)
		kflag = 1;
	if (dflag + kflag + mflag > 1)
		usage("only one of -k, -d or -m may be given");
	if (argc > 0 && aflag != 0)
		usage("excess arguments after -a");
	if (aflag != 0 && wflag != 0)
		usage("only one of -a or -w may be given");

	if (kflag) {
		if (file == NULL)
			file = PATH_KEYBOARD;
		field_tab = keyboard_field_tab;
		field_tab_len = keyboard_field_tab_len;
		getval = keyboard_get_values;
		putval = keyboard_put_values;
	} else if (mflag) {
		if (file == NULL)
			file = PATH_MOUSE;
		field_tab = mouse_field_tab;
		field_tab_len = mouse_field_tab_len;
		getval = mouse_get_values;
		putval = mouse_put_values;
	} else if (dflag) {
		if (file == NULL)
			file = PATH_DISPLAY;
		field_tab = display_field_tab;
		field_tab_len = display_field_tab_len;
		getval = display_get_values;
		putval = display_put_values;
	}

	field_setup(field_tab, field_tab_len);

	fd = open(file, O_WRONLY);
	if (fd < 0)
		fd = open(file, O_RDONLY);
	if (fd < 0)
		err(1, "%s", file);

	if (aflag != 0) {
		for (i = 0; i < field_tab_len; i++)
			if ((field_tab[i].flags & (FLG_NOAUTO|FLG_WRONLY)) == 0)
				field_tab[i].flags |= FLG_GET;
		(*getval)(fd);
		for (i = 0; i < field_tab_len; i++)
			if (field_tab[i].flags & FLG_NOAUTO)
				warnx("Use explicit arg to view %s.",
				      field_tab[i].name);
			else if (field_tab[i].flags & FLG_GET)
				pr_field(field_tab + i, sep);
	} else if (argc > 0) {
		if (wflag != 0) {
			for (i = 0; i < argc; i++) {
				p = strchr(argv[i], '=');
				if (p == NULL)
					errx(1, "'=' not found");
				if (p > argv[i] && *(p - 1) == '+') {
					*(p - 1) = '\0';
					do_merge = 1;
				} else
					do_merge = 0;
				*p++ = '\0';
				f = field_by_name(argv[i]);
				if ((f->flags & FLG_RDONLY) != 0)
					errx(1, "%s: read only", argv[i]);
				if (do_merge) {
					if ((f->flags & FLG_MODIFY) == 0)
						errx(1, "%s: can only be set",
						     argv[i]);
					f->flags |= FLG_GET;
					(*getval)(fd);
					f->flags &= ~FLG_GET;
				}
				rd_field(f, p, do_merge);
				f->flags |= FLG_SET;
				(*putval)(fd);
				f->flags &= ~FLG_SET;
			}
		} else {
			for (i = 0; i < argc; i++) {
				f = field_by_name(argv[i]);
				if ((f->flags & FLG_WRONLY) != 0)
					errx(1, "%s: read only", argv[i]);
				f->flags |= FLG_GET;
			}
			(*getval)(fd);
			for (i = 0; i < field_tab_len; i++)
				if (field_tab[i].flags & FLG_GET)
					pr_field(field_tab + i, sep);
		}
	} else
		usage(NULL);

	exit(0);
}
