/*	$OpenBSD: patch.c,v 1.23 2003/07/22 17:18:49 otto Exp $	*/

/*
 * patch - a program to apply diffs to original files
 * 
 * Copyright 1986, Larry Wall
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition is met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this condition and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * -C option added in 1998, original code by Marc Espie, based on FreeBSD
 * behaviour
 */

#ifndef lint
static const char rcsid[] = "$OpenBSD: patch.c,v 1.23 2003/07/22 17:18:49 otto Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "INTERN.h"
#include "common.h"
#include "EXTERN.h"
#include "util.h"
#include "pch.h"
#include "inp.h"
#include "backupfile.h"


static void	reinitialize_almost_everything(void);
static void	get_some_switches(void);
static LINENUM	locate_hunk(LINENUM);
static void	abort_hunk(void);
static void	apply_hunk(LINENUM);
static void	init_output(char *);
static void	init_reject(char *);
static void	copy_till(LINENUM);
static void	spew_output(void);
static void	dump_line(LINENUM);
static bool	patch_match(LINENUM, LINENUM, LINENUM);
static bool	similar(char *, char *, int);
static int	optcmp(const void *, const void *);
static char	decode_long_option(char *);

/* TRUE if -E was specified on command line.  */
static int	remove_empty_files = FALSE;

/* TRUE if -R was specified on command line.  */
static int	reverse_flag_specified = FALSE;

 /* buffer for stderr */
static char	serrbuf[BUFSIZ];

/* Apply a set of diffs as appropriate. */

int
main(int argc, char *argv[])
{
	int	hunk = 0, failed = 0, failtotal = 0, patch_seen = 0, i;
	LINENUM	where, newwhere, fuzz, mymaxfuzz;
	char	*tmpdir, *v;

	setbuf(stderr, serrbuf);
	for (i = 0; i < MAXFILEC; i++)
		filearg[i] = NULL;

	myuid = getuid();

	/* Cons up the names of the temporary files.  */
	tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL) {
		tmpdir = "/tmp";
	}
	if (asprintf(&TMPOUTNAME, "%s/patchoXXXXXXXXXX", tmpdir) == -1)
		fatal("cannot allocate memory");
	if ((i = mkstemp(TMPOUTNAME)) < 0)
		pfatal("can't create %s", TMPOUTNAME);
	close(i);

	if (asprintf(&TMPINNAME, "%s/patchiXXXXXXXXXX", tmpdir) == -1)
		fatal("cannot allocate memory");
	if ((i = mkstemp(TMPINNAME)) < 0)
		pfatal("can't create %s", TMPINNAME);
	close(i);

	if (asprintf(&TMPREJNAME, "%s/patchrXXXXXXXXXX", tmpdir) == -1)
		fatal("cannot allocate memory");
	if ((i = mkstemp(TMPREJNAME)) < 0)
		pfatal("can't create %s", TMPREJNAME);
	close(i);

	if (asprintf(&TMPPATNAME, "%s/patchpXXXXXXXXXX", tmpdir) == -1)
		fatal("cannot allocate memory");
	if ((i = mkstemp(TMPPATNAME)) < 0)
		pfatal("can't create %s", TMPPATNAME);
	close(i);

	v = getenv("SIMPLE_BACKUP_SUFFIX");
	if (v)
		simple_backup_suffix = v;
	else
		simple_backup_suffix = ORIGEXT;

	v = getenv("VERSION_CONTROL");
	backup_type = get_version(v);	/* OK to pass NULL. */

	/* parse switches */
	Argc = argc;
	Argv = argv;
	get_some_switches();

	/* make sure we clean up /tmp in case of disaster */
	set_signals(0);

	for (open_patch_file(filearg[1]); there_is_another_patch();
	    reinitialize_almost_everything()) {
		/* for each patch in patch file */

		patch_seen = TRUE;

		if (outname == NULL)
			outname = savestr(filearg[0]);

		/* for ed script just up and do it and exit */
		if (diff_type == ED_DIFF) {
			do_ed_script();
			continue;
		}
		/* initialize the patched file */
		if (!skip_rest_of_patch)
			init_output(TMPOUTNAME);

		/* initialize reject file */
		init_reject(TMPREJNAME);

		/* find out where all the lines are */
		if (!skip_rest_of_patch)
			scan_input(filearg[0]);

		/* from here on, open no standard i/o files, because malloc */
		/* might misfire and we can't catch it easily */

		/* apply each hunk of patch */
		hunk = 0;
		failed = 0;
		out_of_mem = FALSE;
		while (another_hunk()) {
			hunk++;
			fuzz = NULL;
			mymaxfuzz = pch_context();
			if (maxfuzz < mymaxfuzz)
				mymaxfuzz = maxfuzz;
			if (!skip_rest_of_patch) {
				do {
					where = locate_hunk(fuzz);
					if (hunk == 1 && where == NULL && !force) {
						/* dwim for reversed patch? */
						if (!pch_swap()) {
							if (fuzz == NULL)
								say("Not enough memory to try swapped hunk!  Assuming unswapped.\n");
							continue;
						}
						reverse = !reverse;
						/* try again */
						where = locate_hunk(fuzz);
						if (where == NULL) {
							/* didn't find it swapped */
							if (!pch_swap())
								/* put it back to normal */
								fatal("lost hunk on alloc error!\n");
							reverse = !reverse;
						} else if (noreverse) {
							if (!pch_swap())
								/* put it back to normal */
								fatal("lost hunk on alloc error!\n");
							reverse = !reverse;
							say("Ignoring previously applied (or reversed) patch.\n");
							skip_rest_of_patch = TRUE;
						} else if (batch) {
							if (verbose)
								say("%seversed (or previously applied) patch detected!  %s -R.",
								    reverse ? "R" : "Unr",
								    reverse ? "Assuming" : "Ignoring");
						} else {
							ask("%seversed (or previously applied) patch detected!  %s -R? [y] ",
							    reverse ? "R" : "Unr",
							    reverse ? "Assume" : "Ignore");
							if (*buf == 'n') {
								ask("Apply anyway? [n] ");
								if (*buf != 'y')
									skip_rest_of_patch = TRUE;
								where = NULL;
								reverse = !reverse;
								if (!pch_swap())
									/* put it back to normal */
									fatal("lost hunk on alloc error!\n");
							}
						}
					}
				} while (!skip_rest_of_patch && where == NULL &&
					 ++fuzz <= mymaxfuzz);

				if (skip_rest_of_patch) {	/* just got decided */
					fclose(ofp);
					ofp = NULL;
				}
			}
			newwhere = pch_newfirst() + last_offset;
			if (skip_rest_of_patch) {
				abort_hunk();
				failed++;
				if (verbose)
					say("Hunk #%d ignored at %ld.\n",
					    hunk, newwhere);
			} else if (where == NULL) {
				abort_hunk();
				failed++;
				if (verbose)
					say("Hunk #%d failed at %ld.\n",
					    hunk, newwhere);
			} else {
				apply_hunk(where);
				if (verbose) {
					say("Hunk #%d succeeded at %ld",
					    hunk, newwhere);
					if (fuzz)
						say(" with fuzz %ld", fuzz);
					if (last_offset)
						say(" (offset %ld line%s)",
						    last_offset,
						    last_offset == 1L ? "" : "s");
					say(".\n");
				}
			}
		}

		if (out_of_mem && using_plan_a) {
			Argc = Argc_last;
			Argv = Argv_last;
			say("\n\nRan out of memory using Plan A--trying again...\n\n");
			if (ofp)
				fclose(ofp);
			ofp = NULL;
			if (rejfp)
				fclose(rejfp);
			rejfp = NULL;
			continue;
		}
		assert(hunk);

		/* finish spewing out the new file */
		if (!skip_rest_of_patch)
			spew_output();

		/* and put the output where desired */
		ignore_signals();
		if (!skip_rest_of_patch) {
			struct stat	statbuf;
			char	*realout = outname;

			if (!check_only) {
				if (move_file(TMPOUTNAME, outname) < 0) {
					toutkeep = TRUE;
					realout = TMPOUTNAME;
					chmod(TMPOUTNAME, filemode);
				} else
					chmod(outname, filemode);

				if (remove_empty_files &&
				    stat(realout, &statbuf) == 0 &&
				    statbuf.st_size == 0) {
					if (verbose)
						say("Removing %s (empty after patching).\n",
						    realout);
					unlink(realout);
				}
			}
		}
		fclose(rejfp);
		rejfp = NULL;
		if (failed) {
			failtotal += failed;
			if (!*rejname) {
				if (strlcpy(rejname, outname,
				    sizeof(rejname)) >= sizeof(rejname))
					fatal("filename %s is too long\n", outname);
				if (strlcat(rejname, REJEXT,
				    sizeof(rejname)) >= sizeof(rejname))
					fatal("filename %s is too long\n", outname);
			}
			if (skip_rest_of_patch) {
				say("%d out of %d hunks ignored--saving rejects to %s\n",
				    failed, hunk, rejname);
			} else {
				say("%d out of %d hunks failed--saving rejects to %s\n",
				    failed, hunk, rejname);
			}
			if (!check_only && move_file(TMPREJNAME, rejname) < 0)
				trejkeep = TRUE;
		}
		set_signals(1);
	}
	if (!patch_seen)
		failtotal++;
	my_exit(failtotal);
	/* NOTREACHED */
}

/* Prepare to find the next patch to do in the patch file. */

static void
reinitialize_almost_everything(void)
{
	re_patch();
	re_input();

	input_lines = 0;
	last_frozen_line = 0;

	filec = 0;
	if (filearg[0] != NULL && !out_of_mem) {
		free(filearg[0]);
		filearg[0] = NULL;
	}
	if (outname != NULL) {
		free(outname);
		outname = NULL;
	}
	last_offset = 0;

	diff_type = 0;

	if (revision != NULL) {
		free(revision);
		revision = NULL;
	}
	reverse = reverse_flag_specified;
	skip_rest_of_patch = FALSE;

	get_some_switches();

	if (filec >= 2)
		fatal("you may not change to a different patch file\n");
}

static char *
nextarg(void)
{
	if (!--Argc)
		fatal("missing argument after `%s'\n", *Argv);
	return *++Argv;
}

/* Module for handling of long options.  */

struct option {
	char	*long_opt;
	char	short_opt;
};

static int
optcmp(const void *v1, const void *v2)
{
	const struct option *a = v1, *b = v2;

	return strcmp(a->long_opt, b->long_opt);
}

/* Decode Long options beginning with "--" to their short equivalents.  */

static char
decode_long_option(char *opt)
{
	/*
	 * This table must be sorted on the first field.  We also decode
	 * unimplemented options as those will be handled later anyway.
	 */
	static struct option options[] = {
		{"batch", 't'},
		{"check", 'C'},
		{"context", 'c'},
		{"debug", 'x'},
		{"directory", 'd'},
		{"ed", 'e'},
		{"force", 'f'},
		{"forward", 'N'},
		{"fuzz", 'F'},
		{"ifdef", 'D'},
		{"ignore-whitespace", 'l'},
		{"normal", 'n'},
		{"output", 'o'},
		{"prefix", 'B'},
		{"quiet", 's'},
		{"reject-file", 'r'},
		{"remove-empty-files", 'E'},
		{"reverse", 'R'},
		{"silent", 's'},
		{"skip", 'S'},
		{"strip", 'p'},
		{"suffix", 'b'},
		{"unified", 'u'},
		{"version", 'v'},
		{"version-control", 'V'},
	};
	struct option   key, *found;

	key.long_opt = opt;
	found = (struct option *) bsearch(&key, options,
	    sizeof(options) / sizeof(options[0]),
	    sizeof(options[0]), optcmp);
	return found ? found->short_opt : '\0';
}

/* Process switches and filenames up to next '+' or end of list. */

static void
get_some_switches(void)
{
	char	*s;

	rejname[0] = '\0';
	Argc_last = Argc;
	Argv_last = Argv;
	if (!Argc)
		return;
	for (Argc--, Argv++; Argc; Argc--, Argv++) {
		s = Argv[0];
		if (strEQ(s, "+")) {
			return;	/* + will be skipped by for loop */
		}
		if (*s != '-' || !s[1]) {
			if (filec == MAXFILEC)
				fatal("too many file arguments\n");
			filearg[filec++] = savestr(s);
		} else {
			char	opt;

			if (*(s + 1) == '-') {
				opt = decode_long_option(s + 2);
				s += strlen(s) - 1;
			} else
				opt = *++s;
			switch (opt) {
			case 'b':
				simple_backup_suffix = savestr(nextarg());
				break;
			case 'B':
				origprae = savestr(nextarg());
				break;
			case 'c':
				diff_type = CONTEXT_DIFF;
				break;
			case 'C':
				check_only = TRUE;
				break;
			case 'd':
				if (!*++s)
					s = nextarg();
				if (chdir(s) < 0)
					pfatal("can't cd to %s", s);
				break;
			case 'D':
				do_defines = TRUE;
				if (!*++s)
					s = nextarg();
				if (!isalpha(*s) && '_' != *s)
					fatal("argument to -D is not an identifier\n");
				snprintf(if_defined, sizeof if_defined,
				    "#ifdef %s\n", s);
				snprintf(not_defined, sizeof not_defined,
				    "#ifndef %s\n", s);
				snprintf(end_defined, sizeof end_defined,
				    "#endif /* %s */\n", s);
				break;
			case 'e':
				diff_type = ED_DIFF;
				break;
			case 'E':
				remove_empty_files = TRUE;
				break;
			case 'f':
				force = TRUE;
				break;
			case 'F':
				if (!*++s)
					s = nextarg();
				else if (*s == '=')
					s++;
				maxfuzz = atoi(s);
				break;
			case 'l':
				canonicalize = TRUE;
				break;
			case 'n':
				diff_type = NORMAL_DIFF;
				break;
			case 'N':
				noreverse = TRUE;
				break;
			case 'o':
				outname = savestr(nextarg());
				break;
			case 'p':
				if (!*++s)
					s = nextarg();
				else if (*s == '=')
					s++;
				strippath = atoi(s);
				break;
			case 'r':
				if (strlcpy(rejname, nextarg(),
				    sizeof(rejname)) >= sizeof(rejname))
					fatal("argument for -r is too long\n");
				break;
			case 'R':
				reverse = TRUE;
				reverse_flag_specified = TRUE;
				break;
			case 's':
				verbose = FALSE;
				break;
			case 'S':
				skip_rest_of_patch = TRUE;
				break;
			case 't':
				batch = TRUE;
				break;
			case 'u':
				diff_type = UNI_DIFF;
				break;
			case 'v':
				version();
				break;
			case 'V':
				backup_type = get_version(nextarg());
				break;
#ifdef DEBUGGING
			case 'x':
				if (!*++s)
					s = nextarg();
				debug = atoi(s);
				break;
#endif
			default:
				fprintf(stderr, "patch: unrecognized option `%s'\n",
				    Argv[0]);
				fprintf(stderr, "\
Usage: patch [options] [origfile [patchfile]] [+ [options] [origfile]]...\n\
Options:\n\
       [-cCeEflnNRsStuv] [-b backup-ext] [-B backup-prefix] [-d directory]\n\
       [-D symbol] [-Fmax-fuzz] [-o out-file] [-p[strip-count]]\n\
       [-r rej-name] [-V {numbered,existing,simple}]\n");
				my_exit(1);
			}
		}
	}
}

/*
 * Attempt to find the right place to apply this hunk of patch.
 */
static LINENUM
locate_hunk(LINENUM fuzz)
{
	LINENUM	first_guess = pch_first() + last_offset;
	LINENUM	offset;
	LINENUM	pat_lines = pch_ptrn_lines();
	LINENUM	max_pos_offset = input_lines - first_guess - pat_lines + 1;
	LINENUM	max_neg_offset = first_guess - last_frozen_line - 1 + pch_context();

	if (!pat_lines)		/* null range matches always */
		return first_guess;
	if (max_neg_offset >= first_guess)	/* do not try lines < 0 */
		max_neg_offset = first_guess - 1;
	if (first_guess <= input_lines && patch_match(first_guess, NULL, fuzz))
		return first_guess;
	for (offset = 1; ; offset++) {
		bool	check_after = (offset <= max_pos_offset);
		bool	check_before = (offset <= max_neg_offset);

		if (check_after && patch_match(first_guess, offset, fuzz)) {
#ifdef DEBUGGING
			if (debug & 1)
				say("Offset changing from %ld to %ld\n",
				    last_offset, offset);
#endif
			last_offset = offset;
			return first_guess + offset;
		} else if (check_before && patch_match(first_guess, -offset, fuzz)) {
#ifdef DEBUGGING
			if (debug & 1)
				say("Offset changing from %ld to %ld\n",
				    last_offset, -offset);
#endif
			last_offset = -offset;
			return first_guess - offset;
		} else if (!check_before && !check_after)
			return NULL;
	}
}

/* We did not find the pattern, dump out the hunk so they can handle it. */

static void
abort_hunk(void)
{
	LINENUM	i;
	LINENUM	pat_end = pch_end();
	/*
	 * add in last_offset to guess the same as the previous successful
	 * hunk
	 */
	LINENUM	oldfirst = pch_first() + last_offset;
	LINENUM	newfirst = pch_newfirst() + last_offset;
	LINENUM	oldlast = oldfirst + pch_ptrn_lines() - 1;
	LINENUM	newlast = newfirst + pch_repl_lines() - 1;
	char	*stars = (diff_type >= NEW_CONTEXT_DIFF ? " ****" : "");
	char	*minuses = (diff_type >= NEW_CONTEXT_DIFF ? " ----" : " -----");

	fprintf(rejfp, "***************\n");
	for (i = 0; i <= pat_end; i++) {
		switch (pch_char(i)) {
		case '*':
			if (oldlast < oldfirst)
				fprintf(rejfp, "*** 0%s\n", stars);
			else if (oldlast == oldfirst)
				fprintf(rejfp, "*** %ld%s\n", oldfirst, stars);
			else
				fprintf(rejfp, "*** %ld,%ld%s\n", oldfirst,
				    oldlast, stars);
			break;
		case '=':
			if (newlast < newfirst)
				fprintf(rejfp, "--- 0%s\n", minuses);
			else if (newlast == newfirst)
				fprintf(rejfp, "--- %ld%s\n", newfirst, minuses);
			else
				fprintf(rejfp, "--- %ld,%ld%s\n", newfirst,
				    newlast, minuses);
			break;
		case '\n':
			fprintf(rejfp, "%s", pfetch(i));
			break;
		case ' ':
		case '-':
		case '+':
		case '!':
			fprintf(rejfp, "%c %s", pch_char(i), pfetch(i));
			break;
		default:
			fatal("fatal internal error in abort_hunk\n");
		}
	}
}

/* We found where to apply it (we hope), so do it. */

static void
apply_hunk(LINENUM where)
{
	LINENUM	old = 1;
	LINENUM	lastline = pch_ptrn_lines();
	LINENUM	new = lastline + 1;
#define OUTSIDE 0
#define IN_IFNDEF 1
#define IN_IFDEF 2
#define IN_ELSE 3
	int	def_state = OUTSIDE;
	bool	R_do_defines = do_defines;
	LINENUM	pat_end = pch_end();

	where--;
	while (pch_char(new) == '=' || pch_char(new) == '\n')
		new++;

	while (old <= lastline) {
		if (pch_char(old) == '-') {
			copy_till(where + old - 1);
			if (R_do_defines) {
				if (def_state == OUTSIDE) {
					fputs(not_defined, ofp);
					def_state = IN_IFNDEF;
				} else if (def_state == IN_IFDEF) {
					fputs(else_defined, ofp);
					def_state = IN_ELSE;
				}
				fputs(pfetch(old), ofp);
			}
			last_frozen_line++;
			old++;
		} else if (new > pat_end) {
			break;
		} else if (pch_char(new) == '+') {
			copy_till(where + old - 1);
			if (R_do_defines) {
				if (def_state == IN_IFNDEF) {
					fputs(else_defined, ofp);
					def_state = IN_ELSE;
				} else if (def_state == OUTSIDE) {
					fputs(if_defined, ofp);
					def_state = IN_IFDEF;
				}
			}
			fputs(pfetch(new), ofp);
			new++;
		} else if (pch_char(new) != pch_char(old)) {
			say("Out-of-sync patch, lines %ld,%ld--mangled text or line numbers, maybe?\n",
			    pch_hunk_beg() + old,
			    pch_hunk_beg() + new);
#ifdef DEBUGGING
			say("oldchar = '%c', newchar = '%c'\n",
			    pch_char(old), pch_char(new));
#endif
			my_exit(1);
		} else if (pch_char(new) == '!') {
			copy_till(where + old - 1);
			if (R_do_defines) {
				fputs(not_defined, ofp);
				def_state = IN_IFNDEF;
			}
			while (pch_char(old) == '!') {
				if (R_do_defines) {
					fputs(pfetch(old), ofp);
				}
				last_frozen_line++;
				old++;
			}
			if (R_do_defines) {
				fputs(else_defined, ofp);
				def_state = IN_ELSE;
			}
			while (pch_char(new) == '!') {
				fputs(pfetch(new), ofp);
				new++;
			}
		} else {
			assert(pch_char(new) == ' ');
			old++;
			new++;
			if (R_do_defines && def_state != OUTSIDE) {
				fputs(end_defined, ofp);
				def_state = OUTSIDE;
			}
		}
	}
	if (new <= pat_end && pch_char(new) == '+') {
		copy_till(where + old - 1);
		if (R_do_defines) {
			if (def_state == OUTSIDE) {
				fputs(if_defined, ofp);
				def_state = IN_IFDEF;
			} else if (def_state == IN_IFNDEF) {
				fputs(else_defined, ofp);
				def_state = IN_ELSE;
			}
		}
		while (new <= pat_end && pch_char(new) == '+') {
			fputs(pfetch(new), ofp);
			new++;
		}
	}
	if (R_do_defines && def_state != OUTSIDE) {
		fputs(end_defined, ofp);
	}
}

/*
 * Open the new file.
 */
static void
init_output(char *name)
{
	ofp = fopen(name, "w");
	if (ofp == NULL)
		pfatal("can't create %s", name);
}

/*
 * Open a file to put hunks we can't locate.
 */
static void
init_reject(char *name)
{
	rejfp = fopen(name, "w");
	if (rejfp == NULL)
		pfatal("can't create %s", name);
}

/*
 * Copy input file to output, up to wherever hunk is to be applied.
 */
static void
copy_till(LINENUM lastline)
{
	LINENUM	R_last_frozen_line = last_frozen_line;

	if (R_last_frozen_line > lastline)
		fatal("misordered hunks! output would be garbled\n");
	while (R_last_frozen_line < lastline)
		dump_line(++R_last_frozen_line);
	last_frozen_line = R_last_frozen_line;
}

/*
 * Finish copying the input file to the output file.
 */
static void
spew_output(void)
{
#ifdef DEBUGGING
	if (debug & 256)
		say("il=%ld lfl=%ld\n", input_lines, last_frozen_line);
#endif
	if (input_lines)
		copy_till(input_lines);	/* dump remainder of file */
	fclose(ofp);
	ofp = NULL;
}

/*
 * Copy one line from input to output.
 */
static void
dump_line(LINENUM line)
{
	char	*s, R_newline = '\n';

	/* Note: string is not null terminated. */
	for (s=ifetch(line, 0); putc(*s, ofp) != R_newline; s++)
		;
}

/*
 * Does the patch pattern match at line base+offset?
 */
static bool
patch_match(LINENUM base, LINENUM offset, LINENUM fuzz)
{
	LINENUM	pline = 1 + fuzz;
	LINENUM	iline;
	LINENUM	pat_lines = pch_ptrn_lines() - fuzz;

	for (iline = base + offset + fuzz; pline <= pat_lines; pline++, iline++) {
		if (canonicalize) {
			if (!similar(ifetch(iline, (offset >= 0)),
			    pfetch(pline), pch_line_len(pline)))
				return FALSE;
		} else if (strnNE(ifetch(iline, (offset >= 0)),
		    pfetch(pline), pch_line_len(pline)))
			return FALSE;
	}
	return TRUE;
}

/*
 * Do two lines match with canonicalized white space?
 */
static bool
similar(char *a, char *b, int len)
{
	while (len) {
		if (isspace(*b)) {	/* whitespace (or \n) to match? */
			if (!isspace(*a))	/* no corresponding whitespace? */
				return FALSE;
			while (len && isspace(*b) && *b != '\n')
				b++, len--;	/* skip pattern whitespace */
			while (isspace(*a) && *a != '\n')
				a++;	/* skip target whitespace */
			if (*a == '\n' || *b == '\n')
				return (*a == *b);	/* should end in sync */
		} else if (*a++ != *b++)	/* match non-whitespace chars */
			return FALSE;
		else
			len--;	/* probably not necessary */
	}
	return TRUE;		/* actually, this is not reached */
	/* since there is always a \n */
}
