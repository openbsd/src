/*	$OpenBSD: sort.c,v 1.46 2015/03/19 13:11:05 jmc Exp $	*/

/*-
 * Copyright (C) 2009 Gabor Kovesdan <gabor@FreeBSD.org>
 * Copyright (C) 2012 Oleg Moskalenko <mom040267@gmail.com>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <md5.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include "coll.h"
#include "file.h"
#include "sort.h"

#define	OPTIONS	"bCcdfgHhik:Mmno:RrS:st:T:uVz"

static bool need_random;
static const char *random_source;

MD5_CTX md5_ctx;

struct sort_opts sort_opts_vals;

bool debug_sort;
bool need_hint;

static bool gnusort_numeric_compatibility;

static struct sort_mods default_sort_mods_object;
struct sort_mods * const default_sort_mods = &default_sort_mods_object;

static bool print_symbols_on_debug;

/*
 * Arguments from file (when file0-from option is used:
 */
static size_t argc_from_file0 = (size_t)-1;
static char **argv_from_file0;

/*
 * Placeholder symbols for options which have no single-character equivalent
 */
enum {
	SORT_OPT = CHAR_MAX + 1,
	HELP_OPT,
	FF_OPT,
	BS_OPT,
	VERSION_OPT,
	DEBUG_OPT,
	RANDOMSOURCE_OPT,
	COMPRESSPROGRAM_OPT,
	QSORT_OPT,
	HEAPSORT_OPT,
	RADIXSORT_OPT,
	MMAP_OPT
};

#define	NUMBER_OF_MUTUALLY_EXCLUSIVE_FLAGS 6
static const char mutually_exclusive_flags[NUMBER_OF_MUTUALLY_EXCLUSIVE_FLAGS] = { 'M', 'n', 'g', 'R', 'h', 'V' };

static const struct option long_options[] = {
    { "batch-size", required_argument, NULL, BS_OPT },
    { "buffer-size", required_argument, NULL, 'S' },
    { "check", optional_argument, NULL, 'c' },
    { "check=silent|quiet", optional_argument, NULL, 'C' },
    { "compress-program", required_argument, NULL, COMPRESSPROGRAM_OPT },
    { "debug", no_argument, NULL, DEBUG_OPT },
    { "dictionary-order", no_argument, NULL, 'd' },
    { "field-separator", required_argument, NULL, 't' },
    { "files0-from", required_argument, NULL, FF_OPT },
    { "general-numeric-sort", no_argument, NULL, 'g' },
    { "heapsort", no_argument, NULL, HEAPSORT_OPT },
    { "help", no_argument, NULL, HELP_OPT },
    { "human-numeric-sort", no_argument, NULL, 'h' },
    { "ignore-leading-blanks", no_argument, NULL, 'b' },
    { "ignore-case", no_argument, NULL, 'f' },
    { "ignore-nonprinting", no_argument, NULL, 'i' },
    { "key", required_argument, NULL, 'k' },
    { "merge", no_argument, NULL, 'm' },
    { "mergesort", no_argument, NULL, 'H' },
    { "mmap", no_argument, NULL, MMAP_OPT },
    { "month-sort", no_argument, NULL, 'M' },
    { "numeric-sort", no_argument, NULL, 'n' },
    { "output", required_argument, NULL, 'o' },
    { "qsort", no_argument, NULL, QSORT_OPT },
    { "radixsort", no_argument, NULL, RADIXSORT_OPT },
    { "random-sort", no_argument, NULL, 'R' },
    { "random-source", required_argument, NULL, RANDOMSOURCE_OPT },
    { "reverse", no_argument, NULL, 'r' },
    { "sort", required_argument, NULL, SORT_OPT },
    { "stable", no_argument, NULL, 's' },
    { "temporary-directory", required_argument, NULL, 'T' },
    { "unique", no_argument, NULL, 'u' },
    { "version", no_argument, NULL, VERSION_OPT },
    { "version-sort", no_argument, NULL, 'V' },
    { "zero-terminated", no_argument, NULL, 'z' },
    { NULL, no_argument, NULL, 0 }
};

/*
 * Check where sort modifier is present
 */
static bool
sort_modifier_empty(struct sort_mods *sm)
{

	if (sm == NULL)
		return true;
	return !(sm->Mflag || sm->Vflag || sm->nflag || sm->gflag ||
	    sm->rflag || sm->Rflag || sm->hflag || sm->dflag || sm->fflag);
}

/*
 * Print out usage text.
 */
static __dead void
usage(int exit_val)
{

	fprintf(exit_val ? stderr : stdout,
	    "usage: %s [-bCcdfgHhiMmnRrsuVz] [-k field1[,field2]] [-o output] "
	    "[-S size]\n\t[-T dir] [-t char] [file ...]\n", getprogname());
	exit(exit_val);
}

/*
 * Read input file names from a file (file0-from option).
 */
static void
read_fns_from_file0(const char *fn)
{
	if (fn) {
		struct file0_reader f0r;
		FILE *f;

		f = fopen(fn, "r");
		if (f == NULL)
			err(2, "%s", fn);

		memset(&f0r, 0, sizeof(f0r));
		f0r.f = f;

		while (!feof(f)) {
			char *line = read_file0_line(&f0r);

			if (line && *line) {
				if (argc_from_file0 == (size_t)-1)
					argc_from_file0 = 0;
				++argc_from_file0;
				argv_from_file0 = sort_reallocarray(argv_from_file0,
				    argc_from_file0, sizeof(char *));
				argv_from_file0[argc_from_file0 - 1] =
				    sort_strdup(line);
			}
		}
		closefile(f, fn);
	}
}

/*
 * Check how much RAM is available for the sort.
 */
static void
set_hw_params(void)
{
	long pages, psize;

	pages = sysconf(_SC_PHYS_PAGES);
	if (pages < 1) {
		warn("sysconf pages");
		pages = 1;
	}
	psize = sysconf(_SC_PAGESIZE);
	if (psize < 1) {
		warn("sysconf psize");
		psize = 4096;
	}

	free_memory = (unsigned long long) pages * (unsigned long long) psize;
	available_free_memory = free_memory / 2;

	if (available_free_memory < 1024)
		available_free_memory = 1024;
}

/*
 * Convert "plain" symbol to wide symbol, with default value.
 */
static void
conv_mbtowc(wchar_t *wc, const char *c, const wchar_t def)
{

	if (wc && c) {
		int res;

		res = mbtowc(wc, c, MB_CUR_MAX);
		if (res < 1)
			*wc = def;
	}
}

/*
 * Set current locale symbols.
 */
static void
set_locale(void)
{
	struct lconv *lc;
	const char *locale;

	setlocale(LC_ALL, "");

	lc = localeconv();

	if (lc) {
		/* obtain LC_NUMERIC info */
		/* Convert to wide char form */
		conv_mbtowc(&symbol_decimal_point, lc->decimal_point,
		    symbol_decimal_point);
		conv_mbtowc(&symbol_thousands_sep, lc->thousands_sep,
		    symbol_thousands_sep);
		conv_mbtowc(&symbol_positive_sign, lc->positive_sign,
		    symbol_positive_sign);
		conv_mbtowc(&symbol_negative_sign, lc->negative_sign,
		    symbol_negative_sign);
	}

	if (getenv("GNUSORT_NUMERIC_COMPATIBILITY"))
		gnusort_numeric_compatibility = true;

	locale = setlocale(LC_COLLATE, NULL);
	if (locale != NULL) {
		char *tmpl;
		const char *byteclocale;

		tmpl = sort_strdup(locale);
		byteclocale = setlocale(LC_COLLATE, "C");
		if (byteclocale && strcmp(byteclocale, tmpl) == 0) {
			byte_sort = true;
		} else {
			byteclocale = setlocale(LC_COLLATE, "POSIX");
			if (byteclocale && strcmp(byteclocale, tmpl) == 0)
				byte_sort = true;
			else
				setlocale(LC_COLLATE, tmpl);
		}
		sort_free(tmpl);
	}
	if (!byte_sort)
		sort_mb_cur_max = MB_CUR_MAX;
}

/*
 * Set directory temporary files.
 */
static void
set_tmpdir(void)
{
	char *td;

	td = getenv("TMPDIR");
	if (td != NULL)
		tmpdir = sort_strdup(td);
}

/*
 * Parse -S option.
 */
static unsigned long long
parse_memory_buffer_value(const char *value)
{

	if (value == NULL)
		return available_free_memory;
	else {
		char *endptr;
		unsigned long long membuf;

		endptr = NULL;
		errno = 0;
		membuf = strtoll(value, &endptr, 10);

		if (errno != 0) {
			warn("Wrong memory buffer specification");
			membuf = available_free_memory;
		} else {
			switch (*endptr){
			case 'Y':
				membuf *= 1024;
				/* FALLTHROUGH */
			case 'Z':
				membuf *= 1024;
				/* FALLTHROUGH */
			case 'E':
				membuf *= 1024;
				/* FALLTHROUGH */
			case 'P':
				membuf *= 1024;
				/* FALLTHROUGH */
			case 'T':
				membuf *= 1024;
				/* FALLTHROUGH */
			case 'G':
				membuf *= 1024;
				/* FALLTHROUGH */
			case 'M':
				membuf *= 1024;
				/* FALLTHROUGH */
			case '\0':
			case 'K':
				membuf *= 1024;
				/* FALLTHROUGH */
			case 'b':
				break;
			case '%':
				membuf = (available_free_memory * membuf) /
				    100;
				break;
			default:
				warnc(EINVAL, "%s", optarg);
				membuf = available_free_memory;
			}
		}
		return membuf;
	}
}

/*
 * Signal handler that clears the temporary files.
 */
static void
sig_handler(int sig __unused, siginfo_t *siginfo __unused,
    void *context __unused)
{

	clear_tmp_files();
	exit(2);
}

/*
 * Set signal handler on panic signals.
 */
static void
set_signal_handler(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = &sig_handler;
	sa.sa_flags = SA_SIGINFO;

	if (sigaction(SIGTERM, &sa, NULL) < 0) {
		warn("sigaction(SIGTERM)");
		return;
	}
	if (sigaction(SIGHUP, &sa, NULL) < 0) {
		warn("sigaction(SIGHUP)");
		return;
	}
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		warn("sigaction(SIGINT)");
		return;
	}
	if (sigaction(SIGQUIT, &sa, NULL) < 0) {
		warn("sigaction(SIGQUIT)");
		return;
	}
	if (sigaction(SIGABRT, &sa, NULL) < 0) {
		warn("sigaction(SIGABRT)");
		return;
	}
	if (sigaction(SIGBUS, &sa, NULL) < 0) {
		warn("sigaction(SIGBUS)");
		return;
	}
	if (sigaction(SIGSEGV, &sa, NULL) < 0) {
		warn("sigaction(SIGSEGV)");
		return;
	}
	if (sigaction(SIGUSR1, &sa, NULL) < 0) {
		warn("sigaction(SIGUSR1)");
		return;
	}
	if (sigaction(SIGUSR2, &sa, NULL) < 0) {
		warn("sigaction(SIGUSR2)");
		return;
	}
}

/*
 * Print "unknown" message and exit with status 2.
 */
static void
unknown(const char *what)
{

	errx(2, "Unknown feature: %s", what);
}

/*
 * Check whether contradictory input options are used.
 */
static void
check_mutually_exclusive_flags(char c, bool *mef_flags)
{
	int i, fo_index, mec;
	bool found_others, found_this;

	found_others = found_this =false;
	fo_index = 0;

	for (i = 0; i < NUMBER_OF_MUTUALLY_EXCLUSIVE_FLAGS; i++) {
		mec = mutually_exclusive_flags[i];

		if (mec != c) {
			if (mef_flags[i]) {
				if (found_this)
					errx(1, "%c:%c: mutually exclusive flags", c, mec);
				found_others = true;
				fo_index = i;
			}
		} else {
			if (found_others)
				errx(1, "%c:%c: mutually exclusive flags", c, mutually_exclusive_flags[fo_index]);
			mef_flags[i] = true;
			found_this = true;
		}
	}
}

/*
 * Initialise sort opts data.
 */
static void
set_sort_opts(void)
{

	memset(&default_sort_mods_object, 0,
	    sizeof(default_sort_mods_object));
	memset(&sort_opts_vals, 0, sizeof(sort_opts_vals));
	default_sort_mods_object.func =
	    get_sort_func(&default_sort_mods_object);
}

/*
 * Set a sort modifier on a sort modifiers object.
 */
static bool
set_sort_modifier(struct sort_mods *sm, int c)
{

	if (sm) {
		switch (c){
		case 'b':
			sm->bflag = true;
			break;
		case 'd':
			sm->dflag = true;
			break;
		case 'f':
			sm->fflag = true;
			break;
		case 'g':
			sm->gflag = true;
			need_hint = true;
			break;
		case 'i':
			sm->iflag = true;
			break;
		case 'R':
			sm->Rflag = true;
			need_random = true;
			break;
		case 'M':
			initialise_months();
			sm->Mflag = true;
			need_hint = true;
			break;
		case 'n':
			sm->nflag = true;
			need_hint = true;
			print_symbols_on_debug = true;
			break;
		case 'r':
			sm->rflag = true;
			break;
		case 'V':
			sm->Vflag = true;
			break;
		case 'h':
			sm->hflag = true;
			need_hint = true;
			print_symbols_on_debug = true;
			break;
		default:
			return false;
		}
		sort_opts_vals.complex_sort = true;
		sm->func = get_sort_func(sm);
	}
	return true;
}

/*
 * Parse POS in -k option.
 */
static int
parse_pos(const char *s, struct key_specs *ks, bool *mef_flags, bool second)
{
	regmatch_t pmatch[4];
	regex_t re;
	char *c, *f;
	const char *sregexp = "^([0-9]+)(\\.[0-9]+)?([bdfirMngRhV]+)?$";
	size_t len, nmatch;
	int ret;

	ret = -1;
	nmatch = 4;
	c = f = NULL;

	if (regcomp(&re, sregexp, REG_EXTENDED) != 0)
		return -1;

	if (regexec(&re, s, nmatch, pmatch, 0) != 0)
		goto end;

	if (pmatch[0].rm_eo <= pmatch[0].rm_so)
		goto end;

	if (pmatch[1].rm_eo <= pmatch[1].rm_so)
		goto end;

	len = pmatch[1].rm_eo - pmatch[1].rm_so;
	f = sort_malloc((len + 1) * sizeof(char));

	strncpy(f, s + pmatch[1].rm_so, len);
	f[len] = '\0';

	if (second) {
		errno = 0;
		ks->f2 = (size_t) strtoul(f, NULL, 10);
		if (errno != 0)
			err(2, "-k");
		if (ks->f2 == 0) {
			warn("0 field in key specs");
			goto end;
		}
	} else {
		errno = 0;
		ks->f1 = (size_t) strtoul(f, NULL, 10);
		if (errno != 0)
			err(2, "-k");
		if (ks->f1 == 0) {
			warn("0 field in key specs");
			goto end;
		}
	}

	if (pmatch[2].rm_eo > pmatch[2].rm_so) {
		len = pmatch[2].rm_eo - pmatch[2].rm_so - 1;
		c = sort_malloc((len + 1) * sizeof(char));

		strncpy(c, s + pmatch[2].rm_so + 1, len);
		c[len] = '\0';

		if (second) {
			errno = 0;
			ks->c2 = (size_t) strtoul(c, NULL, 10);
			if (errno != 0)
				err(2, "-k");
		} else {
			errno = 0;
			ks->c1 = (size_t) strtoul(c, NULL, 10);
			if (errno != 0)
				err(2, "-k");
			if (ks->c1 == 0) {
				warn("0 column in key specs");
				goto end;
			}
		}
	} else {
		if (second)
			ks->c2 = 0;
		else
			ks->c1 = 1;
	}

	if (pmatch[3].rm_eo > pmatch[3].rm_so) {
		regoff_t i = 0;

		for (i = pmatch[3].rm_so; i < pmatch[3].rm_eo; i++) {
			check_mutually_exclusive_flags(s[i], mef_flags);
			if (s[i] == 'b') {
				if (second)
					ks->pos2b = true;
				else
					ks->pos1b = true;
			} else if (!set_sort_modifier(&(ks->sm), s[i]))
				goto end;
		}
	}

	ret = 0;

end:

	if (c)
		sort_free(c);
	if (f)
		sort_free(f);
	regfree(&re);

	return ret;
}

/*
 * Parse -k option value.
 */
static int
parse_k(const char *s, struct key_specs *ks)
{
	int ret = -1;
	bool mef_flags[NUMBER_OF_MUTUALLY_EXCLUSIVE_FLAGS] =
	    { false, false, false, false, false, false };

	if (s && *s) {
		char *sptr;

		sptr = strchr(s, ',');
		if (sptr) {
			size_t size1;
			char *pos1, *pos2;

			size1 = sptr - s;

			if (size1 < 1)
				return -1;
			pos1 = sort_malloc((size1 + 1) * sizeof(char));

			strncpy(pos1, s, size1);
			pos1[size1] = '\0';

			ret = parse_pos(pos1, ks, mef_flags, false);

			sort_free(pos1);
			if (ret < 0)
				return ret;

			pos2 = sort_strdup(sptr + 1);
			ret = parse_pos(pos2, ks, mef_flags, true);
			sort_free(pos2);
		} else
			ret = parse_pos(s, ks, mef_flags, false);
	}

	return ret;
}

/*
 * Parse POS in +POS -POS option.
 */
static int
parse_pos_obs(const char *s, int *nf, int *nc, char *sopts)
{
	regex_t re;
	regmatch_t pmatch[4];
	char *c, *f;
	const char *sregexp = "^([0-9]+)(\\.[0-9]+)?([A-Za-z]+)?$";
	int ret;
	size_t len, nmatch;

	ret = -1;
	nmatch = 4;
	c = f = NULL;
	*nc = *nf = 0;

	if (regcomp(&re, sregexp, REG_EXTENDED) != 0)
		return -1;

	if (regexec(&re, s, nmatch, pmatch, 0) != 0)
		goto end;

	if (pmatch[0].rm_eo <= pmatch[0].rm_so)
		goto end;

	if (pmatch[1].rm_eo <= pmatch[1].rm_so)
		goto end;

	len = pmatch[1].rm_eo - pmatch[1].rm_so;
	f = sort_malloc((len + 1) * sizeof(char));

	strncpy(f, s + pmatch[1].rm_so, len);
	f[len] = '\0';

	errno = 0;
	*nf = (size_t) strtoul(f, NULL, 10);
	if (errno != 0)
		errx(2, "Invalid key position");

	if (pmatch[2].rm_eo > pmatch[2].rm_so) {
		len = pmatch[2].rm_eo - pmatch[2].rm_so - 1;
		c = sort_malloc((len + 1) * sizeof(char));

		strncpy(c, s + pmatch[2].rm_so + 1, len);
		c[len] = '\0';

		errno = 0;
		*nc = (size_t) strtoul(c, NULL, 10);
		if (errno != 0)
			errx(2, "Invalid key position");
	}

	if (pmatch[3].rm_eo > pmatch[3].rm_so) {

		len = pmatch[3].rm_eo - pmatch[3].rm_so;

		strncpy(sopts, s + pmatch[3].rm_so, len);
		sopts[len] = '\0';
	}

	ret = 0;

end:
	if (c)
		sort_free(c);
	if (f)
		sort_free(f);
	regfree(&re);

	return ret;
}

/*
 * "Translate" obsolete +POS1 -POS2 syntax into new -kPOS1,POS2 syntax
 */
static void
fix_obsolete_keys(int *argc, char **argv)
{
	char sopt[129];
	int i;

	for (i = 1; i < *argc; i++) {
		char *arg1;

		arg1 = argv[i];

		if (strlen(arg1) > 1 && arg1[0] == '+') {
			int c1, f1;
			char sopts1[128];

			sopts1[0] = 0;
			c1 = f1 = 0;

			if (parse_pos_obs(arg1 + 1, &f1, &c1, sopts1) < 0)
				continue;
			else {
				f1 += 1;
				c1 += 1;
				if (i + 1 < *argc) {
					char *arg2 = argv[i + 1];

					if (strlen(arg2) > 1 &&
					    arg2[0] == '-') {
						int c2, f2;
						char sopts2[128];

						sopts2[0] = 0;
						c2 = f2 = 0;

						if (parse_pos_obs(arg2 + 1,
						    &f2, &c2, sopts2) >= 0) {
							int j;
							if (c2 > 0)
								f2 += 1;
							snprintf(sopt,
							    sizeof(sopt),
							    "-k%d.%d%s,%d.%d%s",
							    f1, c1, sopts1, f2,
							    c2, sopts2);
							argv[i] = sort_strdup(sopt);
							for (j = i + 1; j + 1 < *argc; j++)
								argv[j] = argv[j + 1];
							*argc -= 1;
							continue;
						}
					}
				}
				snprintf(sopt, sizeof(sopt), "-k%d.%d%s",
				    f1, c1, sopts1);
				argv[i] = sort_strdup(sopt);
			}
		}
	}
}

/*
 * Set random seed
 */
static void
set_random_seed(void)
{
	if (!need_random)
		return;

	MD5Init(&md5_ctx);
	if (random_source != NULL) {
		unsigned char buf[BUFSIZ];
		size_t nr;
		FILE *fp;

		if ((fp = fopen(random_source, "r")) == NULL)
			err(2, "%s", random_source);
		while ((nr = fread(buf, 1, sizeof(buf), fp)) != 0)
			MD5Update(&md5_ctx, buf, nr);
		if (ferror(fp))
			err(2, "%s", random_source);
		fclose(fp);
	} else {
		unsigned char rsd[1024];

		arc4random_buf(rsd, sizeof(rsd));
		MD5Update(&md5_ctx, rsd, sizeof(rsd));
	}
}

/*
 * Main function.
 */
int
main(int argc, char *argv[])
{
	char *outfile, *real_outfile;
	int c, result;
	size_t i;
	bool mef_flags[NUMBER_OF_MUTUALLY_EXCLUSIVE_FLAGS] =
	    { false, false, false, false, false, false };

	result = 0;
	outfile = sort_strdup("-");
	real_outfile = NULL;

	struct sort_mods *sm = &default_sort_mods_object;

	init_tmp_files();

	set_signal_handler();

	set_hw_params();
	set_locale();
	set_tmpdir();
	set_sort_opts();

	fix_obsolete_keys(&argc, argv);

	while (((c = getopt_long(argc, argv, OPTIONS, long_options, NULL))
	    != -1)) {

		check_mutually_exclusive_flags(c, mef_flags);

		if (!set_sort_modifier(sm, c)) {

			switch (c) {
			case 'c':
				sort_opts_vals.cflag = true;
				if (optarg) {
					if (!strcmp(optarg, "diagnose-first"))
						;
					else if (!strcmp(optarg, "silent") ||
					    !strcmp(optarg, "quiet"))
						sort_opts_vals.csilentflag = true;
					else if (*optarg)
						unknown(optarg);
				}
				break;
			case 'C':
				sort_opts_vals.cflag = true;
				sort_opts_vals.csilentflag = true;
				break;
			case 'k':
			{
				sort_opts_vals.complex_sort = true;
				sort_opts_vals.kflag = true;

				keys_num++;
				keys = sort_reallocarray(keys, keys_num,
				    sizeof(struct key_specs));
				memset(&(keys[keys_num - 1]), 0,
				    sizeof(struct key_specs));

				if (parse_k(optarg, &(keys[keys_num - 1]))
				    < 0) {
					errc(2, EINVAL, "-k %s", optarg);
				}

				break;
			}
			case 'm':
				sort_opts_vals.mflag = true;
				break;
			case 'o':
				sort_free(outfile);
				outfile = sort_strdup(optarg);
				break;
			case 's':
				sort_opts_vals.sflag = true;
				break;
			case 'S':
				available_free_memory =
				    parse_memory_buffer_value(optarg);
				break;
			case 'T':
				tmpdir = sort_strdup(optarg);
				break;
			case 't':
				while (strlen(optarg) > 1) {
					if (optarg[0] != '\\') {
						errc(2, EINVAL, "%s", optarg);
					}
					optarg += 1;
					if (*optarg == '0') {
						*optarg = 0;
						break;
					}
				}
				sort_opts_vals.tflag = true;
				sort_opts_vals.field_sep = btowc(optarg[0]);
				if (sort_opts_vals.field_sep == WEOF) {
					errno = EINVAL;
					err(2, NULL);
				}
				if (!gnusort_numeric_compatibility) {
					if (symbol_decimal_point == sort_opts_vals.field_sep)
						symbol_decimal_point = WEOF;
					if (symbol_thousands_sep == sort_opts_vals.field_sep)
						symbol_thousands_sep = WEOF;
					if (symbol_negative_sign == sort_opts_vals.field_sep)
						symbol_negative_sign = WEOF;
					if (symbol_positive_sign == sort_opts_vals.field_sep)
						symbol_positive_sign = WEOF;
				}
				break;
			case 'u':
				sort_opts_vals.uflag = true;
				/* stable sort for the correct unique val */
				sort_opts_vals.sflag = true;
				break;
			case 'z':
				sort_opts_vals.zflag = true;
				break;
			case SORT_OPT:
				if (optarg) {
					if (!strcmp(optarg, "general-numeric"))
						set_sort_modifier(sm, 'g');
					else if (!strcmp(optarg, "human-numeric"))
						set_sort_modifier(sm, 'h');
					else if (!strcmp(optarg, "numeric"))
						set_sort_modifier(sm, 'n');
					else if (!strcmp(optarg, "month"))
						set_sort_modifier(sm, 'M');
					else if (!strcmp(optarg, "random"))
						set_sort_modifier(sm, 'R');
					else
						unknown(optarg);
				}
				break;
			case QSORT_OPT:
				sort_opts_vals.sort_method = SORT_QSORT;
				break;
			case 'H':
				sort_opts_vals.sort_method = SORT_MERGESORT;
				break;
			case MMAP_OPT:
				use_mmap = true;
				break;
			case HEAPSORT_OPT:
				sort_opts_vals.sort_method = SORT_HEAPSORT;
				break;
			case RADIXSORT_OPT:
				sort_opts_vals.sort_method = SORT_RADIXSORT;
				break;
			case RANDOMSOURCE_OPT:
				random_source = strdup(optarg);
				break;
			case COMPRESSPROGRAM_OPT:
				compress_program = strdup(optarg);
				break;
			case FF_OPT:
				read_fns_from_file0(optarg);
				break;
			case BS_OPT:
			{
				errno = 0;
				long mof = strtol(optarg, NULL, 10);
				if (errno != 0)
					err(2, "--batch-size");
				if (mof >= 2)
					max_open_files = (size_t) mof + 1;
			}
				break;
			case VERSION_OPT:
				printf("%s\n", VERSION);
				exit(EXIT_SUCCESS);
				/* NOTREACHED */
				break;
			case DEBUG_OPT:
				debug_sort = true;
				break;
			case HELP_OPT:
				usage(0);
				/* NOTREACHED */
				break;
			default:
				usage(2);
				/* NOTREACHED */
			}
		}
	}

	argc -= optind;
	argv += optind;

	if (sort_opts_vals.cflag && sort_opts_vals.mflag)
		errx(1, "%c:%c: mutually exclusive flags", 'm', 'c');

	if (keys_num == 0) {
		keys_num = 1;
		keys = sort_realloc(keys, sizeof(struct key_specs));
		memset(&(keys[0]), 0, sizeof(struct key_specs));
		keys[0].c1 = 1;
		keys[0].pos1b = default_sort_mods->bflag;
		keys[0].pos2b = default_sort_mods->bflag;
		memcpy(&(keys[0].sm), default_sort_mods,
		    sizeof(struct sort_mods));
	}

	for (i = 0; i < keys_num; i++) {
		struct key_specs *ks;

		ks = &(keys[i]);

		if (sort_modifier_empty(&(ks->sm)) && !(ks->pos1b) &&
		    !(ks->pos2b)) {
			ks->pos1b = sm->bflag;
			ks->pos2b = sm->bflag;
			memcpy(&(ks->sm), sm, sizeof(struct sort_mods));
		}

		ks->sm.func = get_sort_func(&(ks->sm));
	}

	if (argv_from_file0) {
		argc = argc_from_file0;
		argv = argv_from_file0;
	}

	if (debug_sort) {
		printf("Memory to be used for sorting: %llu\n",
		    available_free_memory);
		printf("Using collate rules of %s locale\n",
		    setlocale(LC_COLLATE, NULL));
		if (byte_sort)
			printf("Byte sort is used\n");
		if (print_symbols_on_debug) {
			printf("Decimal Point: <%lc>\n", symbol_decimal_point);
			if (symbol_thousands_sep)
				printf("Thousands separator: <%lc>\n",
				    symbol_thousands_sep);
			printf("Positive sign: <%lc>\n", symbol_positive_sign);
			printf("Negative sign: <%lc>\n", symbol_negative_sign);
		}
	}

	set_random_seed();

	/* Case when the outfile equals one of the input files: */
	if (strcmp(outfile, "-")) {
		int i;

		for (i = 0; i < argc; ++i) {
			if (strcmp(argv[i], outfile) == 0) {
				real_outfile = sort_strdup(outfile);
				for (;;) {
					const size_t size = strlen(outfile) + strlen(".tmp") + 1;
					outfile = sort_realloc(outfile, size);
					strlcat(outfile, ".tmp", size);
					if (access(outfile, F_OK) < 0)
						break;
				}
				tmp_file_atexit(outfile);
			}
		}
	}

	if (!sort_opts_vals.cflag && !sort_opts_vals.mflag) {
		struct file_list fl;
		struct sort_list list;

		sort_list_init(&list);
		file_list_init(&fl, true);

		if (argc < 1)
			procfile("-", &list, &fl);
		else {
			while (argc > 0) {
				procfile(*argv, &list, &fl);
				--argc;
				++argv;
			}
		}

		if (fl.count < 1)
			sort_list_to_file(&list, outfile);
		else {
			if (list.count > 0) {
				char *flast = new_tmp_file_name();

				sort_list_to_file(&list, flast);
				file_list_add(&fl, flast, false);
			}
			merge_files(&fl, outfile);
		}

		file_list_clean(&fl);

		/*
		 * We are about to exit the program, so we can ignore
		 * the clean-up for speed
		 *
		 * sort_list_clean(&list);
		 */

	} else if (sort_opts_vals.cflag) {
		result = (argc == 0) ? (check("-")) : (check(*argv));
	} else if (sort_opts_vals.mflag) {
		struct file_list fl;

		file_list_init(&fl, false);
		file_list_populate(&fl, argc, argv, true);
		merge_files(&fl, outfile);
		file_list_clean(&fl);
	}

	if (real_outfile) {
		unlink(real_outfile);
		if (rename(outfile, real_outfile) < 0)
			err(2, "%s", real_outfile);
		sort_free(real_outfile);
	}

	sort_free(outfile);

	return result;
}
