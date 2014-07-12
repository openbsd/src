/*
 * C and T preprocessor, and integrated lexer
 * (c) Thomas Pornin 1999 - 2002
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define VERS_MAJ	1
#define VERS_MIN	3
/* uncomment the following if you cannot set it with a compiler flag */
/* #define STAND_ALONE */

#include "tune.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stddef.h>
#include <limits.h>
#include <time.h>
#include "ucppi.h"
#include "mem.h"
#include "nhash.h"
#ifdef UCPP_MMAP
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#endif

/*
 * The standard path where includes are looked for.
 */
#ifdef STAND_ALONE
static char *include_path_std[] = { STD_INCLUDE_PATH, 0 };
#endif
static char **include_path;
static size_t include_path_nb = 0;

int no_special_macros = 0;
int emit_dependencies = 0, emit_defines = 0, emit_assertions = 0;
FILE *emit_output;

#ifdef STAND_ALONE
static char *system_macros_def[] = { STD_MACROS, 0 };
static char *system_assertions_def[] = { STD_ASSERT, 0 };
#endif

char *current_filename = 0, *current_long_filename = 0;
static int current_incdir = -1;

#ifndef NO_UCPP_ERROR_FUNCTIONS
/*
 * "ouch" is the name for an internal ucpp error. If AUDIT is not defined,
 * no code calling this function will be generated; a "ouch" may still be
 * emitted by getmem() (in mem.c) if MEM_CHECK is defined, but this "ouch"
 * does not use this function.
 */
void ucpp_ouch(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ouch, ", current_filename);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	die();
}

/*
 * report an error, with current_filename, line, and printf-like syntax
 */
void ucpp_error(long line, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (line > 0)
		fprintf(stderr, "%s: line %ld: ", current_filename, line);
	else if (line == 0) fprintf(stderr, "%s: ", current_filename);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	if (line >= 0) {
		struct stack_context *sc = report_context();
		size_t i;

		for (i = 0; sc[i].line >= 0; i ++)
			fprintf(stderr, "\tincluded from %s:%ld\n",
				sc[i].long_name ? sc[i].long_name : sc[i].name,
				sc[i].line);
		freemem(sc);
	}
	va_end(ap);
}

/*
 * like error(), with the mention "warning"
 */
void ucpp_warning(long line, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (line > 0)
		fprintf(stderr, "%s: warning: line %ld: ",
			current_filename, line);
	else if (line == 0)
		fprintf(stderr, "%s: warning: ", current_filename);
	else fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	if (line >= 0) {
		struct stack_context *sc = report_context();
		size_t i;

		for (i = 0; sc[i].line >= 0; i ++)
			fprintf(stderr, "\tincluded from %s:%ld\n",
				sc[i].long_name ? sc[i].long_name : sc[i].name,
				sc[i].line);
		freemem(sc);
	}
	va_end(ap);
}
#endif	/* NO_UCPP_ERROR_FUNCTIONS */

/*
 * Some memory allocations are manually garbage-collected; essentially,
 * strings duplicated in the process of macro replacement. Each such
 * string is referenced in the garbage_fifo, which is cleared when all
 * nested macros have been resolved.
 */

struct garbage_fifo {
	char **garbage;
	size_t ngarb, memgarb;
};

/*
 * throw_away() marks a string to be collected later
 */
void throw_away(struct garbage_fifo *gf, char *n)
{
	wan(gf->garbage, gf->ngarb, n, gf->memgarb);
}

/*
 * free marked strings
 */
void garbage_collect(struct garbage_fifo *gf)
{
	size_t i;

	for (i = 0; i < gf->ngarb; i ++) freemem(gf->garbage[i]);
	gf->ngarb = 0;
}

static void init_garbage_fifo(struct garbage_fifo *gf)
{
	gf->garbage = getmem((gf->memgarb = GARBAGE_LIST_MEMG)
		* sizeof(char *));
	gf->ngarb = 0;
}

static void free_garbage_fifo(struct garbage_fifo *gf)
{
	garbage_collect(gf);
	freemem(gf->garbage);
	freemem(gf);
}

/*
 * order is important: it must match the token-constants declared as an
 * enum in the header file.
 */
char *operators_name[] = {
	" ", "\n", " ",
	"0000", "name", "bunch", "pragma", "context",
	"\"dummy string\"", "'dummy char'",
	"/", "/=", "-", "--", "-=", "->", "+", "++", "+=", "<", "<=", "<<",
	"<<=", ">", ">=", ">>", ">>=", "=", "==",
#ifdef CAST_OP
	"=>",
#endif
	"~", "!=", "&", "&&", "&=", "|", "||", "|=", "%", "%=", "*", "*=",
	"^", "^=", "!",
	"{", "}", "[", "]", "(", ")", ",", "?", ";",
	":", ".", "...", "#", "##", " ", "ouch", "<:", ":>", "<%", "%>",
	"%:", "%:%:"
};

/* the ascii representation of a token */
#ifdef SEMPER_FIDELIS
#define tname(x)	(ttWHI((x).type) ? " " : S_TOKEN((x).type) \
			? (x).name : operators_name[(x).type])
#else
#define tname(x)	(S_TOKEN((x).type) ? (x).name \
			: operators_name[(x).type])
#endif

char *token_name(struct token *t)
{
	return tname(*t);
}

/*
 * To speed up deeply nested and repeated inclusions, we:
 * -- use a hash table to remember where we found each file
 * -- remember when the file is protected by a #ifndef/#define/#endif
 *    construction; we can then avoid including several times a file
 *    when this is not necessary.
 * -- remember in which directory, in the include path, the file was found.
 */
struct found_file {
	hash_item_header head;    /* first field */
	char *name;
	char *protect;
};

/*
 * For files from system include path.
 */
struct found_file_sys {
	hash_item_header head;    /* first field */
	struct found_file *rff;
	int incdir;
};

static HTT found_files, found_files_sys;
static int found_files_init_done = 0, found_files_sys_init_done = 0;

static struct found_file *new_found_file(void)
{
	struct found_file *ff = getmem(sizeof(struct found_file));

	ff->name = 0;
	ff->protect = 0;
	return ff;
}

static void del_found_file(void *m)
{
	struct found_file *ff = (struct found_file *)m;

	if (ff->name) freemem(ff->name);
	if (ff->protect) freemem(ff->protect);
	freemem(ff);
}

static struct found_file_sys *new_found_file_sys(void)
{
	struct found_file_sys *ffs = getmem(sizeof(struct found_file_sys));

	ffs->rff = 0;
	ffs->incdir = -1;
	return ffs;
}

static void del_found_file_sys(void *m)
{
	struct found_file_sys *ffs = (struct found_file_sys *)m;

	freemem(ffs);
}

/*
 * To keep up with the #ifndef/#define/#endif protection mechanism
 * detection.
 */
struct protect protect_detect;
static struct protect *protect_detect_stack = 0;

void set_init_filename(char *x, int real_file)
{
	if (current_filename) freemem(current_filename);
	current_filename = sdup(x);
	current_long_filename = 0;
	current_incdir = -1;
	if (real_file) {
		protect_detect.macro = 0;
		protect_detect.state = 1;
		protect_detect.ff = new_found_file();
		protect_detect.ff->name = sdup(x);
		HTT_put(&found_files, protect_detect.ff, x);
	} else {
		protect_detect.state = 0;
	}
}

static void init_found_files(void)
{
	if (found_files_init_done) HTT_kill(&found_files);
	HTT_init(&found_files, del_found_file);
	found_files_init_done = 1;
	if (found_files_sys_init_done) HTT_kill(&found_files_sys);
	HTT_init(&found_files_sys, del_found_file_sys);
	found_files_sys_init_done = 1;
}

/*
 * Set the lexer state at the beginning of a file.
 */
static void reinit_lexer_state(struct lexer_state *ls, int wb)
{
#ifndef NO_UCPP_BUF
	ls->input_buf = wb ? getmem(INPUT_BUF_MEMG) : 0;
#ifdef UCPP_MMAP
	ls->from_mmap = 0;
#endif
#endif
	ls->input = 0;
	ls->ebuf = ls->pbuf = 0;
	ls->nlka = 0;
	ls->macfile = 0;
	ls->discard = 1;
	ls->last = 0;		/* we suppose '\n' is not 0 */
	ls->line = 1;
	ls->ltwnl = 1;
	ls->oline = 1;
	ls->pending_token = 0;
	ls->cli = 0;
	ls->copy_line[COPY_LINE_LENGTH - 1] = 0;
	ls->ifnest = 0;
	ls->condf[0] = ls->condf[1] = 0;
}

/*
 * Initialize the struct lexer_state, with optional input and output buffers.
 */
void init_buf_lexer_state(struct lexer_state *ls, int wb)
{
	reinit_lexer_state(ls, wb);
#ifndef NO_UCPP_BUF
	ls->output_buf = wb ? getmem(OUTPUT_BUF_MEMG) : 0;
#endif
	ls->sbuf = 0;
	ls->output_fifo = 0;

	ls->ctok = getmem(sizeof(struct token));
	ls->ctok->name = getmem(ls->tknl = TOKEN_NAME_MEMG);
	ls->pending_token = 0;

	ls->flags = 0;
	ls->count_trigraphs = 0;
	ls->gf = getmem(sizeof(struct garbage_fifo));
	init_garbage_fifo(ls->gf);
	ls->condcomp = 1;
	ls->condnest = 0;
#ifdef INMACRO_FLAG
	ls->inmacro = 0;
	ls->macro_count = 0;
#endif
}

/*
 * Initialize the (complex) struct lexer_state.
 */
void init_lexer_state(struct lexer_state *ls)
{
	init_buf_lexer_state(ls, 1);
	ls->input = 0;
}

/*
 * Restore what is needed from a lexer_state. This is used for #include.
 */
static void restore_lexer_state(struct lexer_state *ls,
	struct lexer_state *lsbak)
{
#ifndef NO_UCPP_BUF
	freemem(ls->input_buf);
	ls->input_buf = lsbak->input_buf;
#ifdef UCPP_MMAP
	ls->from_mmap = lsbak->from_mmap;
	ls->input_buf_sav = lsbak->input_buf_sav;
#endif
#endif
	ls->input = lsbak->input;
	ls->ebuf = lsbak->ebuf;
	ls->pbuf = lsbak->pbuf;
	ls->nlka = lsbak->nlka;
	ls->discard = lsbak->discard;
	ls->line = lsbak->line;
	ls->oline = lsbak->oline;
	ls->ifnest = lsbak->ifnest;
	ls->condf[0] = lsbak->condf[0];
	ls->condf[1] = lsbak->condf[1];
}

/*
 * close input file operations on a struct lexer_state
 */
static void close_input(struct lexer_state *ls)
{
#ifdef UCPP_MMAP
	if (ls->from_mmap) {
		munmap((void *)ls->input_buf, ls->ebuf);
		ls->from_mmap = 0;
		ls->input_buf = ls->input_buf_sav;
	}
#endif
	if (ls->input) {
		fclose(ls->input);
		ls->input = 0;
	}
}

/*
 * file_context (and the two functions push_ and pop_) are used to save
 * all that is needed when including a file.
 */
static struct file_context {
	struct lexer_state ls;
	char *name, *long_name;
	int incdir;
} *ls_stack;
static size_t ls_depth = 0;

static void push_file_context(struct lexer_state *ls)
{
	struct file_context fc;

	fc.name = current_filename;
	fc.long_name = current_long_filename;
	fc.incdir = current_incdir;
	mmv(&(fc.ls), ls, sizeof(struct lexer_state));
	aol(ls_stack, ls_depth, fc, LS_STACK_MEMG);
	ls_depth --;
	aol(protect_detect_stack, ls_depth, protect_detect, LS_STACK_MEMG);
	protect_detect.macro = 0;
}

static void pop_file_context(struct lexer_state *ls)
{
#ifdef AUDIT
	if (ls_depth <= 0) ouch("prepare to meet thy creator");
#endif
	close_input(ls);
	restore_lexer_state(ls, &(ls_stack[-- ls_depth].ls));
	if (protect_detect.macro) freemem(protect_detect.macro);
	protect_detect = protect_detect_stack[ls_depth];
	if (current_filename) freemem(current_filename);
	current_filename = ls_stack[ls_depth].name;
	current_long_filename = ls_stack[ls_depth].long_name;
	current_incdir = ls_stack[ls_depth].incdir;
	if (ls_depth == 0) {
		freemem(ls_stack);
		freemem(protect_detect_stack);
	}
}

/*
 * report_context() returns the list of successive includers of the
 * current file, ending with a dummy entry with a negative line number.
 * The caller is responsible for freeing the returned pointer.
 */
struct stack_context *report_context(void)
{
	struct stack_context *sc;
	size_t i;

	sc = getmem((ls_depth + 1) * sizeof(struct stack_context));
	for (i = 0; i < ls_depth; i ++) {
		sc[i].name = ls_stack[ls_depth - i - 1].name;
		sc[i].long_name = ls_stack[ls_depth - i - 1].long_name;
		sc[i].line = ls_stack[ls_depth - i - 1].ls.line - 1;
	}
	sc[ls_depth].line = -1;
	return sc;
}

/*
 * init_lexer_mode() is used to end initialization of a struct lexer_state
 * if it must be used for a lexer
 */
void init_lexer_mode(struct lexer_state *ls)
{
	ls->flags = DEFAULT_LEXER_FLAGS;
	ls->output_fifo = getmem(sizeof(struct token_fifo));
	ls->output_fifo->art = ls->output_fifo->nt = 0;
	ls->toplevel_of = ls->output_fifo;
	ls->save_ctok = ls->ctok;
}

/*
 * release memory used by a struct lexer_state; this implies closing
 * any input stream held by this structure.
 */
void free_lexer_state(struct lexer_state *ls)
{
	close_input(ls);
#ifndef NO_UCPP_BUF
	if (ls->input_buf) {
		freemem(ls->input_buf);
		ls->input_buf = 0;
	}
	if (ls->output_buf) {
		freemem(ls->output_buf);
		ls->output_buf = 0;
	}
#endif
	if (ls->ctok && (!ls->output_fifo || ls->output_fifo->nt == 0)) {
		freemem(ls->ctok->name);
		freemem(ls->ctok);
		ls->ctok = 0;
	}
	if (ls->gf) {
		free_garbage_fifo(ls->gf);
		ls->gf = 0;
	}
	if (ls->output_fifo) {
		freemem(ls->output_fifo);
		ls->output_fifo = 0;
	}
}

/*
 * Print line information.
 */
static void print_line_info(struct lexer_state *ls, unsigned long flags)
{
	char *fn = current_long_filename ?
		current_long_filename : current_filename;
	char *b, *d;

	b = getmem(50 + strlen(fn));
	if (flags & GCC_LINE_NUM) {
		sprintf(b, "# %ld \"%s\"\n", ls->line, fn);
	} else {
		sprintf(b, "#line %ld \"%s\"\n", ls->line, fn);
	}
	for (d = b; *d; d ++) put_char(ls, (unsigned char)(*d));
	freemem(b);
}

/*
 * Enter a file; this implies the possible emission of a #line directive.
 * The flags used are passed as second parameter instead of being
 * extracted from the struct lexer_state.
 *
 * As a command-line option, gcc-like directives (with only a '#',
 * without 'line') may be produced.
 *
 * enter_file() returns 1 if a (CONTEXT) token was produced, 0 otherwise.
 */
int enter_file(struct lexer_state *ls, unsigned long flags)
{
	char *fn = current_long_filename ?
		current_long_filename : current_filename;

	if (!(flags & LINE_NUM)) return 0;
	if ((flags & LEXER) && !(flags & TEXT_OUTPUT)) {
		struct token t;

		t.type = CONTEXT;
		t.line = ls->line;
		t.name = fn;
		print_token(ls, &t, 0);
		return 1;
	}
	print_line_info(ls, flags);
	ls->oline --;	/* emitted #line troubled oline */
	return 0;
}

#ifdef UCPP_MMAP
/*
 * We open() the file, then fdopen() it and fseek() to its end. If the
 * fseek() worked, we try to mmap() the file, up to the point where we
 * arrived.
 * On an architecture where end-of-lines are multibytes and translated
 * into single '\n', bad things could happen. We strongly hope that, if
 * we could fseek() to the end but could not mmap(), then we can get back.
 */
static void *find_file_map;
static size_t map_length;

FILE *fopen_mmap_file(char *name)
{
	FILE *f;
	int fd;
	long l;

	find_file_map = 0;
	fd = open(name, O_RDONLY, 0);
	if (fd < 0) return 0;
	l = lseek(fd, 0, SEEK_END);
	f = fdopen(fd, "r");
	if (!f) {
		close(fd);
		return 0;
	}
	if (l < 0) return f;	/* not seekable */
	map_length = l;
	if ((find_file_map = mmap(0, map_length, PROT_READ,
		MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		/* we could not mmap() the file; get back */
		find_file_map = 0;
		if (fseek(f, 0, SEEK_SET)) {
			/* bwaah... can't get back. This file is cursed. */
			fclose(f);
			return 0;
		}
	}
	return f;
}

void set_input_file(struct lexer_state *ls, FILE *f)
{
	ls->input = f;
	if (find_file_map) {
		ls->from_mmap = 1;
		ls->input_buf_sav = ls->input_buf;
		ls->input_buf = find_file_map;
		ls->pbuf = 0;
		ls->ebuf = map_length;
	} else {
		ls->from_mmap = 0;
	}
}
#endif

/*
 * Find a file by looking through the include path.
 * return value: a FILE * on the file, opened in "r" mode, or 0.
 *
 * find_file_error will contain:
 *   FF_ERROR      on error (file not found or impossible to read)
 *   FF_PROTECT    file is protected and therefore useless to read
 *   FF_KNOWN      file is already known
 *   FF_UNKNOWN    file was not already known
 */
static int find_file_error;

enum { FF_ERROR, FF_PROTECT, FF_KNOWN, FF_UNKNOWN };

static FILE *find_file(char *name, int localdir)
{
	FILE *f;
	int i, incdir = -1;
	size_t nl = strlen(name);
	char *s = 0;
	struct found_file *ff = 0, *nff;
	int lf = 0;
	int nffa = 0;

	find_file_error = FF_ERROR;
	protect_detect.state = -1;
	protect_detect.macro = 0;
	if (localdir) {
		int i;
		char *rfn = current_long_filename ? current_long_filename
			: current_filename;

		for (i = strlen(rfn) - 1; i >= 0; i --)
#ifdef MSDOS
			if (rfn[i] == '\\') break;
#else
			if (rfn[i] == '/') break;
#endif
#if defined MSDOS
		if (i >= 0 && *name != '\\' && (nl < 2 || name[1] != ':'))
#elif defined AMIGA
		if (i >= 0 && *name != '/' && (nl < 2 || name[1] != ':'))
#else
		if (i >= 0 && *name != '/')
#endif
		{
			/*
			 * current file is somewhere else, and the provided
			 * file name is not absolute, so we must adjust the
			 * base for looking for the file; besides,
			 * found_files and found_files_loc are irrelevant
			 * for this search.
			 */
			s = getmem(i + 2 + nl);
			mmv(s, rfn, i);
#ifdef MSDOS
			s[i] = '\\';
#else
			s[i] = '/';
#endif
			mmv(s + i + 1, name, nl);
			s[i + 1 + nl] = 0;
			ff = HTT_get(&found_files, s);
		} else ff = HTT_get(&found_files, name);
	}
	if (!ff) {
		struct found_file_sys *ffs = HTT_get(&found_files_sys, name);

		if (ffs) {
			ff = ffs->rff;
			incdir = ffs->incdir;
		}
	}
	/*
	 * At that point: if the file was found in the cache, ff points to
	 * the cached descriptive structure; its name is s if s is not 0,
	 * name otherwise.
	 */
	if (ff) goto found_file_cache;

	/*
	 * This is the first time we find the file, or it was not protected.
	 */
	protect_detect.ff = new_found_file();
	nffa = 1;
	if (localdir &&
#ifdef UCPP_MMAP
		(f = fopen_mmap_file(s ? s : name))
#else
		(f = fopen(s ? s : name, "r"))
#endif
		) {
		lf = 1;
		goto found_file;
	}
	/*
	 * If s contains a name, that name is now irrelevant: it was a
	 * filename for a search in the current directory, and the file
	 * was not found.
	 */
	if (s) {
		freemem(s);
		s = 0;
	}
	for (i = 0; (size_t)i < include_path_nb; i ++) {
		size_t ni = strlen(include_path[i]);

		s = getmem(ni + nl + 2);
		mmv(s, include_path[i], ni);
#ifdef AMIGA
	/* contributed by Volker Barthelmann */
		if (ni == 1 && *s == '.') {
			*s = 0;
			ni = 0;
		}
		if (ni > 0 && s[ni - 1] != ':' && s[ni - 1] != '/') {
			s[ni] = '/';
			mmv(s + ni + 1, name, nl + 1);
		} else {
			mmv(s + ni, name, nl + 1);
		}
#else
		s[ni] = '/';
		mmv(s + ni + 1, name, nl + 1);
#endif
#ifdef MSDOS
		/* on msdos systems, replace all / by \ */
		{
			char *c;

			for (c = s; *c; c ++) if (*c == '/') *c = '\\';
		}
#endif
		incdir = i;
		if ((ff = HTT_get(&found_files, s)) != 0) {
			/*
			 * The file is known, but not as a system include
			 * file under the name provided.
			 */
			struct found_file_sys *ffs = new_found_file_sys();

			ffs->rff = ff;
			ffs->incdir = incdir;
			HTT_put(&found_files_sys, ffs, name);
			freemem(s);
			s = 0;
			if (nffa) {
				del_found_file(protect_detect.ff);
				protect_detect.ff = 0;
				nffa = 0;
			}
			goto found_file_cache;
		}
#ifdef UCPP_MMAP
		f = fopen_mmap_file(s);
#else
		f = fopen(s, "r");
#endif
		if (f) goto found_file;
		freemem(s);
		s = 0;
	}
zero_out:
	if (s) freemem(s);
	if (nffa) {
		del_found_file(protect_detect.ff);
		protect_detect.ff = 0;
		nffa = 0;
	}
	return 0;

	/*
	 * This part is invoked when the file was found in the
	 * cache.
	 */
found_file_cache:
	if (ff->protect) {
		if (get_macro(ff->protect)) {
			/* file is protected, do not include it */
			find_file_error = FF_PROTECT;
			goto zero_out;
		}
		/* file is protected but the guardian macro is
		   not available; disable guardian detection. */
		protect_detect.state = 0;
	}
	protect_detect.ff = ff;
#ifdef UCPP_MMAP
	f = fopen_mmap_file(HASH_ITEM_NAME(ff));
#else
	f = fopen(HASH_ITEM_NAME(ff), "r");
#endif
	if (!f) goto zero_out;
	find_file_error = FF_KNOWN;
	goto found_file_2;

	/*
	 * This part is invoked when we found a new file, which was not
	 * yet referenced. If lf == 1, then the file was found directly,
	 * otherwise it was found in some system include directory.
	 * A new found_file structure has been allocated and is in
	 * protect_detect.ff
	 */
found_file:
	if (f && ((emit_dependencies == 1 && lf && current_incdir == -1)
		|| emit_dependencies == 2)) {
		fprintf(emit_output, " %s", s ? s : name);
	}
	nff = protect_detect.ff;
	nff->name = sdup(name);
#ifdef AUDIT
	if (
#endif
	HTT_put(&found_files, nff, s ? s : name)
#ifdef AUDIT
	) ouch("filename collided with a wraith")
#endif
	;
	if (!lf) {
		struct found_file_sys *ffs = new_found_file_sys();

		ffs->rff = nff;
		ffs->incdir = incdir;
		HTT_put(&found_files_sys, ffs, name);
	}
	if (s) freemem(s);
	s = 0;
	find_file_error = FF_UNKNOWN;
	ff = nff;

found_file_2:
	if (s) freemem(s);
	current_long_filename = HASH_ITEM_NAME(ff);
#ifdef NO_LIBC_BUF
	setbuf(f, 0);
#endif
	current_incdir = incdir;
	return f;
}

/*
 * Find the named file by looking through the end of the include path.
 * This is for #include_next directives.
 * #include_next <foo> and #include_next "foo" are considered identical,
 * for all practical purposes.
 */
static FILE *find_file_next(char *name)
{
	int i;
	size_t nl = strlen(name);
	FILE *f;
	struct found_file *ff;

	find_file_error = FF_ERROR;
	protect_detect.state = -1;
	protect_detect.macro = 0;
	for (i = current_incdir + 1; (size_t)i < include_path_nb; i ++) {
		char *s;
		size_t ni = strlen(include_path[i]);

		s = getmem(ni + nl + 2);
		mmv(s, include_path[i], ni);
		s[ni] = '/';
		mmv(s + ni + 1, name, nl + 1);
#ifdef MSDOS
		/* on msdos systems, replace all / by \ */
		{
			char *c;

			for (c = s; *c; c ++) if (*c == '/') *c = '\\';
		}
#endif
		ff = HTT_get(&found_files, s);
		if (ff) {
			/* file was found in the cache */
			if (ff->protect) {
				if (get_macro(ff->protect)) {
					find_file_error = FF_PROTECT;
					freemem(s);
					return 0;
				}
				/* file is protected but the guardian macro is
				   not available; disable guardian detection. */
				protect_detect.state = 0;
			}
			protect_detect.ff = ff;
#ifdef UCPP_MMAP
			f = fopen_mmap_file(HASH_ITEM_NAME(ff));
#else
			f = fopen(HASH_ITEM_NAME(ff), "r");
#endif
			if (!f) {
				/* file is referenced but yet unavailable. */
				freemem(s);
				return 0;
			}
			find_file_error = FF_KNOWN;
			freemem(s);
			s = HASH_ITEM_NAME(ff);
		} else {
#ifdef UCPP_MMAP
			f = fopen_mmap_file(s);
#else
			f = fopen(s, "r");
#endif
			if (f) {
				if (emit_dependencies == 2) {
					fprintf(emit_output, " %s", s);
				}
				ff = protect_detect.ff = new_found_file();
				ff->name = sdup(s);
#ifdef AUDIT
				if (
#endif
				HTT_put(&found_files, ff, s)
#ifdef AUDIT
				) ouch("filename collided with a wraith")
#endif
				;
				find_file_error = FF_UNKNOWN;
				freemem(s);
				s = HASH_ITEM_NAME(ff);
			}
		}
		if (f) {
			current_long_filename = s;
			current_incdir = i;
			return f;
		}
		freemem(s);
	}
	return 0;
}

/*
 * The #if directive. This function parse the expression, performs macro
 * expansion (and handles the "defined" operator), and call eval_expr.
 * return value: 1 if the expression is true, 0 if it is false, -1 on error.
 */
static int handle_if(struct lexer_state *ls)
{
	struct token_fifo tf, tf1, tf2, tf3, *save_tf;
	long l = ls->line;
	unsigned long z;
	int ret = 0, ltww = 1;

	/* first, get the whole line */
	tf.art = tf.nt = 0;
	while (!next_token(ls) && ls->ctok->type != NEWLINE) {
		struct token t;

		if (ltww && ttMWS(ls->ctok->type)) continue;
		ltww = ttMWS(ls->ctok->type);
		t.type = ls->ctok->type;
		t.line = l;
		if (S_TOKEN(ls->ctok->type)) {
			t.name = sdup(ls->ctok->name);
			throw_away(ls->gf, t.name);
		}
		aol(tf.t, tf.nt, t, TOKEN_LIST_MEMG);
	}
	if (ltww && tf.nt) if ((-- tf.nt) == 0) freemem(tf.t);
	if (tf.nt == 0) {
		error(l, "void condition for a #if/#elif");
		return -1;
	}
	/* handle the "defined" operator */
	tf1.art = tf1.nt = 0;
	while (tf.art < tf.nt) {
		struct token *ct, rt;
		struct macro *m;
		size_t nidx, eidx;

		ct = tf.t + (tf.art ++);
		if (ct->type == NAME && !strcmp(ct->name, "defined")) {
			if (tf.art >= tf.nt) goto store_token;
			nidx = tf.art;
			if (ttMWS(tf.t[nidx].type))
				if (++ nidx >= tf.nt) goto store_token;
			if (tf.t[nidx].type == NAME) {
				eidx = nidx;
				goto check_macro;
			}
			if (tf.t[nidx].type != LPAR) goto store_token;
			if (++ nidx >= tf.nt) goto store_token;
			if (ttMWS(tf.t[nidx].type))
				if (++ nidx >= tf.nt) goto store_token;
			if (tf.t[nidx].type != NAME) goto store_token;
			eidx = nidx + 1;
			if (eidx >= tf.nt) goto store_token;
			if (ttMWS(tf.t[eidx].type))
				if (++ eidx >= tf.nt) goto store_token;
			if (tf.t[eidx].type != RPAR) goto store_token;
			goto check_macro;
		}
	store_token:
		aol(tf1.t, tf1.nt, *ct, TOKEN_LIST_MEMG);
		continue;

	check_macro:
		m = get_macro(tf.t[nidx].name);
		rt.type = NUMBER;
		rt.name = m ? "1L" : "0L";
		aol(tf1.t, tf1.nt, rt, TOKEN_LIST_MEMG);
		tf.art = eidx + 1;
	}
	freemem(tf.t);
	if (tf1.nt == 0) {
		error(l, "void condition (after expansion) for a #if/#elif");
		return -1;
	}

	/* perform all macro substitutions */
	tf2.art = tf2.nt = 0;
	save_tf = ls->output_fifo;
	ls->output_fifo = &tf2;
	while (tf1.art < tf1.nt) {
		struct token *ct;

		ct = tf1.t + (tf1.art ++);
		if (ct->type == NAME) {
			struct macro *m = get_macro(ct->name);

			if (m) {
				if (substitute_macro(ls, m, &tf1, 0,
#ifdef NO_PRAGMA_IN_DIRECTIVE
					1,
#else
					0,
#endif
					ct->line)) {
					ls->output_fifo = save_tf;
					goto error1;
				}
				continue;
			}
		} else if ((ct->type == SHARP || ct->type == DIG_SHARP)
			&& (ls->flags & HANDLE_ASSERTIONS)) {
			/* we have an assertion; parse it */
			int nnp, ltww = 1;
			size_t i = tf1.art;
			struct token_fifo atl;
			char *aname;
			struct assert *a;
			int av = 0;
			struct token rt;

			atl.art = atl.nt = 0;
			while (i < tf1.nt && ttMWS(tf1.t[i].type)) i ++;
			if (i >= tf1.nt) goto assert_error;
			if (tf1.t[i].type != NAME) goto assert_error;
			aname = tf1.t[i ++].name;
			while (i < tf1.nt && ttMWS(tf1.t[i].type)) i ++;
			if (i >= tf1.nt) goto assert_generic;
			if (tf1.t[i].type != LPAR) goto assert_generic;
			i ++;
			for (nnp = 1; nnp && i < tf1.nt; i ++) {
				if (ltww && ttMWS(tf1.t[i].type)) continue;
				if (tf1.t[i].type == LPAR) nnp ++;
				else if (tf1.t[i].type == RPAR
					&& (-- nnp) == 0) {
					tf1.art = i + 1;
					break;
				}
				ltww = ttMWS(tf1.t[i].type);
				aol(atl.t, atl.nt, tf1.t[i], TOKEN_LIST_MEMG);
			}
			if (nnp) goto assert_error;
			if (ltww && atl.nt && (-- atl.nt) == 0) freemem(atl.t);
			if (atl.nt == 0) goto assert_error;

			/* the assertion is in aname and atl; check it */
			a = get_assertion(aname);
			if (a) for (i = 0; i < a->nbval; i ++)
				if (!cmp_token_list(&atl, a->val + i)) {
					av = 1;
					break;
				}
			rt.type = NUMBER;
			rt.name = av ? "1" : "0";
			aol(tf2.t, tf2.nt, rt, TOKEN_LIST_MEMG);
			if (atl.nt) freemem(atl.t);
			continue;

		assert_generic:
			tf1.art = i;
			rt.type = NUMBER;
			rt.name = get_assertion(aname) ? "1" : "0";
			aol(tf2.t, tf2.nt, rt, TOKEN_LIST_MEMG);
			continue;

		assert_error:
			error(l, "syntax error for assertion in #if");
			ls->output_fifo = save_tf;
			goto error1;
		}
		aol(tf2.t, tf2.nt, *ct, TOKEN_LIST_MEMG);
	}
	ls->output_fifo = save_tf;
	freemem(tf1.t);
	if (tf2.nt == 0) {
		error(l, "void condition (after expansion) for a #if/#elif");
		return -1;
	}

	/*
	 * suppress whitespace and replace rogue identifiers by 0
	 */
	tf3.art = tf3.nt = 0;
	while (tf2.art < tf2.nt) {
		struct token *ct = tf2.t + (tf2.art ++);

		if (ttMWS(ct->type)) continue;
		if (ct->type == NAME) {
			/*
			 * a rogue identifier; we replace it with "0".
			 */
			struct token rt;

			rt.type = NUMBER;
			rt.name = "0";
			aol(tf3.t, tf3.nt, rt, TOKEN_LIST_MEMG);
			continue;
		}
		aol(tf3.t, tf3.nt, *ct, TOKEN_LIST_MEMG);
	}
	freemem(tf2.t);

	if (tf3.nt == 0) {
		error(l, "void condition (after expansion) for a #if/#elif");
		return -1;
	}
	eval_line = l;
	z = eval_expr(&tf3, &ret, (ls->flags & WARN_STANDARD) != 0);
	freemem(tf3.t);
	if (ret) return -1;
	return (z != 0);

error1:
	if (tf1.nt) freemem(tf1.t);
	if (tf2.nt) freemem(tf2.t);
	return -1;
}

/*
 * A #include was found; parse the end of line, replace macros if
 * necessary.
 *
 * If nex is set to non-zero, the directive is considered as a #include_next
 * (extension to C99, mimicked from GNU)
 */
static int handle_include(struct lexer_state *ls, unsigned long flags, int nex)
{
	int c, string_fname = 0;
	char *fname;
	unsigned char *fname2;
	size_t fname_ptr = 0;
	long l = ls->line;
	int x, y;
	FILE *f;
	struct token_fifo tf, tf2, *save_tf;
	size_t nl;
	int tgd;
	struct lexer_state alt_ls;

#define left_angle(t)	((t) == LT || (t) == LEQ || (t) == LSH \
			|| (t) == ASLSH || (t) == DIG_LBRK || (t) == LBRA)
#define right_angle(t)	((t) == GT || (t) == RSH || (t) == ARROW \
			|| (t) == DIG_RBRK || (t) == DIG_RBRA)

	while ((c = grap_char(ls)) >= 0 && c != '\n') {
		if (space_char(c)) {
			discard_char(ls);
			continue;
		}
		if (c == '<') {
			discard_char(ls);
			while ((c = grap_char(ls)) >= 0) {
				discard_char(ls);
				if (c == '\n') goto include_last_chance;
				if (c == '>') break;
				aol(fname, fname_ptr, (char)c, FNAME_MEMG);
			}
			aol(fname, fname_ptr, (char)0, FNAME_MEMG);
			string_fname = 0;
			goto do_include;
		} else if (c == '"') {
			discard_char(ls);
			while ((c = grap_char(ls)) >= 0) {
				discard_char(ls);
				if (c == '\n') {
				/* macro replacements won't save that one */
					if (fname_ptr) freemem(fname);
					goto include_error;
				}
				if (c == '"') break;
				aol(fname, fname_ptr, (char)c, FNAME_MEMG);
			}
			aol(fname, fname_ptr, (char)0, FNAME_MEMG);
			string_fname = 1;
			goto do_include;
		}
		goto include_macro;
	}

include_last_chance:
	/*
	 * We found a '<' but not the trailing '>'; so we tokenize the
	 * line, and try to act upon it. The standard lets us free in that
	 * matter, and no sane programmer would use such a construct, but
	 * it is no reason not to support it.
	 */
	if (fname_ptr == 0) goto include_error;
	fname2 = getmem(fname_ptr + 1);
	mmv(fname2 + 1, fname, fname_ptr);
	fname2[0] = '<';
	/*
	 * We merely copy the lexer_state structure; this should be ok,
	 * since we do want to share the memory structure (garbage_fifo),
	 * and do not touch any other context-full thing.
	 */
	alt_ls = *ls;
	alt_ls.input = 0;
	alt_ls.input_string = fname2;
	alt_ls.pbuf = 0;
	alt_ls.ebuf = fname_ptr + 1;
	tf.art = tf.nt = 0;
	while (!next_token(&alt_ls)) {
		if (!ttMWS(alt_ls.ctok->type)) {
			struct token t;

			t.type = alt_ls.ctok->type;
			t.line = l;
			if (S_TOKEN(alt_ls.ctok->type)) {
				t.name = sdup(alt_ls.ctok->name);
				throw_away(alt_ls.gf, t.name);
			}
			aol(tf.t, tf.nt, t, TOKEN_LIST_MEMG);
		}
	}
	freemem(fname2);
	if (alt_ls.pbuf < alt_ls.ebuf) goto include_error;
		/* tokenizing failed */
	goto include_macro2;
	
include_error:
	error(l, "invalid '#include'");
	return 1;

include_macro:
	tf.art = tf.nt = 0;
	while (!next_token(ls) && ls->ctok->type != NEWLINE) {
		if (!ttMWS(ls->ctok->type)) {
			struct token t;

			t.type = ls->ctok->type;
			t.line = l;
			if (S_TOKEN(ls->ctok->type)) {
				t.name = sdup(ls->ctok->name);
				throw_away(ls->gf, t.name);
			}
			aol(tf.t, tf.nt, t, TOKEN_LIST_MEMG);
		}
	}
include_macro2:
	tf2.art = tf2.nt = 0;
	save_tf = ls->output_fifo;
	ls->output_fifo = &tf2;
	while (tf.art < tf.nt) {
		struct token *ct;

		ct = tf.t + (tf.art ++);
		if (ct->type == NAME) {
			struct macro *m = get_macro(ct->name);
			if (m) {
				if (substitute_macro(ls, m, &tf, 0,
#ifdef NO_PRAGMA_IN_DIRECTIVE
					1,
#else
					0,
#endif
					ct->line)) {
					ls->output_fifo = save_tf;
					return -1;
				}
				continue;
			}
		}
		aol(tf2.t, tf2.nt, *ct, TOKEN_LIST_MEMG);
	}
	freemem(tf.t);
	ls->output_fifo = save_tf;
	for (x = 0; (size_t)x < tf2.nt && ttWHI(tf2.t[x].type); x ++);
	for (y = tf2.nt - 1; y >= 0 && ttWHI(tf2.t[y].type); y --);
	if ((size_t)x >= tf2.nt) goto include_macro_err;
	if (tf2.t[x].type == STRING) {
		if (y != x) goto include_macro_err;
		if (tf2.t[x].name[0] == 'L') {
			if (ls->flags & WARN_STANDARD)
				warning(l, "wide string for #include");
			fname = sdup(tf2.t[x].name);
			nl = strlen(fname);
			*(fname + nl - 1) = 0;
			mmvwo(fname, fname + 2, nl - 2);
		} else {
			fname = sdup(tf2.t[x].name);
			nl = strlen(fname);
			*(fname + nl - 1) = 0;
			mmvwo(fname, fname + 1, nl - 1);
		}
		string_fname = 1;
	} else if (left_angle(tf2.t[x].type) && right_angle(tf2.t[y].type)) {
		int i, j;

		if (ls->flags & WARN_ANNOYING) warning(l, "reconstruction "
			"of <foo> in #include");
		for (j = 0, i = x; i <= y; i ++) if (!ttWHI(tf2.t[i].type))
			j += strlen(tname(tf2.t[i]));
		fname = getmem(j + 1);
		for (j = 0, i = x; i <= y; i ++) {
			if (ttWHI(tf2.t[i].type)) continue;
			strcpy(fname + j, tname(tf2.t[i]));
			j += strlen(tname(tf2.t[i]));
		}
		*(fname + j - 1) = 0;
		mmvwo(fname, fname + 1, j);
		string_fname = 0;
	} else goto include_macro_err;
	freemem(tf2.t);
	goto do_include_next;

include_macro_err:
	error(l, "macro expansion did not produce a valid filename "
		"for #include");
	if (tf2.nt) freemem(tf2.t);
	return 1;

do_include:
	tgd = 1;
	while (!next_token(ls)) {
		if (tgd && !ttWHI(ls->ctok->type)
			&& (ls->flags & WARN_STANDARD)) {
			warning(l, "trailing garbage in #include");
			tgd = 0;
		}
		if (ls->ctok->type == NEWLINE) break;
	}

	/* the increment of ls->line is intended so that the line
	   numbering is reported correctly in report_context() even if
	   the #include is at the end of the file with no trailing newline */
	if (ls->ctok->type != NEWLINE) ls->line ++;
do_include_next:
	if (!(ls->flags & LEXER) && (ls->flags & KEEP_OUTPUT))
		put_char(ls, '\n');
	push_file_context(ls);
	reinit_lexer_state(ls, 1);
#ifdef MSDOS
	/* on msdos systems, replace all / by \ */
	{
		char *d;

		for (d = fname; *d; d ++) if (*d == '/') *d = '\\';
	}
#endif
	f = nex ? find_file_next(fname) : find_file(fname, string_fname);
	if (!f) {
		current_filename = 0;
		pop_file_context(ls);
		if (find_file_error == FF_ERROR) {
			error(l, "file '%s' not found", fname);
			freemem(fname);
			return 1;
		}
		/* file was found, but it is useless to include it again */
		freemem(fname);
		return 0;
	}
#ifdef UCPP_MMAP
	set_input_file(ls, f);
#else
	ls->input = f;
#endif
	current_filename = fname;
	enter_file(ls, flags);
	return 0;

#undef left_angle
#undef right_angle
}

/*
 * for #line directives
 */
static int handle_line(struct lexer_state *ls, unsigned long flags)
{
	char *fname;
	long l = ls->line;
	struct token_fifo tf, tf2, *save_tf;
	size_t nl, j;
	unsigned long z;

	tf.art = tf.nt = 0;
	while (!next_token(ls) && ls->ctok->type != NEWLINE) {
		if (!ttMWS(ls->ctok->type)) {
			struct token t;

			t.type = ls->ctok->type;
			t.line = l;
			if (S_TOKEN(ls->ctok->type)) {
				t.name = sdup(ls->ctok->name);
				throw_away(ls->gf, t.name);
			}
			aol(tf.t, tf.nt, t, TOKEN_LIST_MEMG);
		}
	}
	tf2.art = tf2.nt = 0;
	save_tf = ls->output_fifo;
	ls->output_fifo = &tf2;
	while (tf.art < tf.nt) {
		struct token *ct;

		ct = tf.t + (tf.art ++);
		if (ct->type == NAME) {
			struct macro *m = get_macro(ct->name);
			if (m) {
				if (substitute_macro(ls, m, &tf, 0,
#ifdef NO_PRAGMA_IN_DIRECTIVE
					1,
#else
					0,
#endif
					ct->line)) {
					ls->output_fifo = save_tf;
					return -1;
				}
				continue;
			}
		}
		aol(tf2.t, tf2.nt, *ct, TOKEN_LIST_MEMG);
	}
	freemem(tf.t);
	for (tf2.art = 0; tf2.art < tf2.nt && ttWHI(tf2.t[tf2.art].type);
		tf2.art ++);
	ls->output_fifo = save_tf;
	if (tf2.art == tf2.nt || (tf2.t[tf2.art].type != NUMBER
		&& tf2.t[tf2.art].type != CHAR)) {
		error(l, "not a valid number for #line");
		goto line_macro_err;
	}
	for (j = 0; tf2.t[tf2.art].name[j]; j ++)
		if (tf2.t[tf2.art].name[j] < '0'
			|| tf2.t[tf2.art].name[j] > '9')
			if (ls->flags & WARN_STANDARD)
				warning(l, "non-standard line number in #line");
	if (catch(eval_exception)) goto line_macro_err;
	z = strtoconst(tf2.t[tf2.art].name);
	if (j > 10 || z > 2147483647U) {
		error(l, "out-of-bound line number for #line");
		goto line_macro_err;
	}
	ls->oline = ls->line = z;
	if ((++ tf2.art) < tf2.nt) {
		size_t i;

		for (i = tf2.art; i < tf2.nt && ttMWS(tf2.t[i].type); i ++);
		if (i < tf2.nt) {
			if (tf2.t[i].type != STRING) {
				error(l, "not a valid filename for #line");
				goto line_macro_err;
			}
			if (tf2.t[i].name[0] == 'L') {
				if (ls->flags & WARN_STANDARD) {
					warning(l, "wide string for #line");
				}
				fname = sdup(tf2.t[i].name);
				nl = strlen(fname);
				*(fname + nl - 1) = 0;
				mmvwo(fname, fname + 2, nl - 2);
			} else {
				fname = sdup(tf2.t[i].name);
				nl = strlen(fname);
				*(fname + nl - 1) = 0;
				mmvwo(fname, fname + 1, nl - 1);
			}
			if (current_filename) freemem(current_filename);
			current_filename = fname;
		}
		for (i ++; i < tf2.nt && ttMWS(tf2.t[i].type); i ++);
		if (i < tf2.nt && (ls->flags & WARN_STANDARD)) {
			warning(l, "trailing garbage in #line");
		}
	}
	freemem(tf2.t);
	enter_file(ls, flags);
	return 0;

line_macro_err:
	if (tf2.nt) freemem(tf2.t);
	return 1;
}

/*
 * a #error directive: we emit the message without any modification
 * (except the usual backslash+newline and trigraphs)
 */
static void handle_error(struct lexer_state *ls)
{
	int c;
	size_t p = 0, lp = 128;
	long l = ls->line;
	unsigned char *buf = getmem(lp);

	while ((c = grap_char(ls)) >= 0 && c != '\n') {
		discard_char(ls);
		wan(buf, p, (unsigned char)c, lp);
	}
	wan(buf, p, 0, lp);
	error(l, "#error%s", buf);
	freemem(buf);
}

/*
 * convert digraph tokens to their standard equivalent.
 */
static int undig(int type)
{
	static int ud[6] = { LBRK, RBRK, LBRA, RBRA, SHARP, DSHARP };

	return ud[type - DIG_LBRK];
}

#ifdef PRAGMA_TOKENIZE
/*
 * Make a compressed representation of a token list; the contents of
 * the token_fifo are freed. Values equal to 0 are replaced by
 * PRAGMA_TOKEN_END (by default, (unsigned char)'\n') and the compressed
 * string is padded by a 0 (so that it may be * handled like a string).
 * Digraph tokens are replaced by their non-digraph equivalents.
 */
struct comp_token_fifo compress_token_list(struct token_fifo *tf)
{
	struct comp_token_fifo ct;
	size_t l;

	for (l = 0, tf->art = 0; tf->art < tf->nt; tf->art ++) {
		l ++;
		if (S_TOKEN(tf->t[tf->art].type))
			l += strlen(tf->t[tf->art].name) + 1;
	}
	ct.t = getmem((ct.length = l) + 1);
	for (l = 0, tf->art = 0; tf->art < tf->nt; tf->art ++) {
		int tt = tf->t[tf->art].type;

		if (tt == 0) tt = PRAGMA_TOKEN_END;
		if (tt > DIGRAPH_TOKENS && tt < DIGRAPH_TOKENS_END)
			tt = undig(tt);
		ct.t[l ++] = tt;
		if (S_TOKEN(tt)) {
			char *tn = tf->t[tf->art].name;
			size_t sl = strlen(tn);

			mmv(ct.t + l, tn, sl);
			l += sl;
			ct.t[l ++] = PRAGMA_TOKEN_END;
			freemem(tn);
		}
	}
	ct.t[l] = 0;
	if (tf->nt) freemem(tf->t);
	ct.rp = 0;
	return ct;
}
#endif

/*
 * A #pragma directive: we make a PRAGMA token containing the rest of
 * the line.
 *
 * We strongly hope that we are called only in LEXER mode.
 */
static void handle_pragma(struct lexer_state *ls)
{
	unsigned char *buf;
	struct token t;
	long l = ls->line;

#ifdef PRAGMA_TOKENIZE
	struct token_fifo tf;

	tf.art = tf.nt = 0;
	while (!next_token(ls) && ls->ctok->type != NEWLINE)
		if (!ttMWS(ls->ctok->type)) break;
	if (ls->ctok->type != NEWLINE) {
		do {
			struct token t;

			t.type = ls->ctok->type;
			if (ttMWS(t.type)) continue;
			if (S_TOKEN(t.type)) t.name = sdup(ls->ctok->name);
			aol(tf.t, tf.nt, t, TOKEN_LIST_MEMG);
		} while (!next_token(ls) && ls->ctok->type != NEWLINE);
	}
	if (tf.nt == 0) {
		/* void pragma are silently ignored */
		return;
	}
	buf = (compress_token_list(&tf)).t;
#else
	int c, x = 1, y = 32;

	while ((c = grap_char(ls)) >= 0 && c != '\n') {
		discard_char(ls);
		if (!space_char(c)) break;
	}
	/* void #pragma are ignored */
	if (c == '\n') return;
	buf = getmem(y);
	buf[0] = c;
	while ((c = grap_char(ls)) >= 0 && c != '\n') {
		discard_char(ls);
		wan(buf, x, c, y);
	}
	for (x --; x >= 0 && space_char(buf[x]); x --);
	x ++;
	wan(buf, x, 0, y);
#endif
	t.type = PRAGMA;
	t.line = l;
	t.name = (char *)buf;
	aol(ls->output_fifo->t, ls->output_fifo->nt, t, TOKEN_LIST_MEMG);
	throw_away(ls->gf, (char *)buf);
}

/*
 * We saw a # at the beginning of a line (or preceeded only by whitespace).
 * We check the directive name and act accordingly.
 */
static int handle_cpp(struct lexer_state *ls, int sharp_type)
{
#define condfset(x)	do { \
		ls->condf[(x) / 32] |= 1UL << ((x) % 32); \
	} while (0)
#define condfclr(x)	do { \
		ls->condf[(x) / 32] &= ~(1UL << ((x) % 32)); \
	} while (0)
#define condfval(x)	((ls->condf[(x) / 32] & (1UL << ((x) % 32))) != 0)

	long l = ls->line;
	unsigned long save_flags = ls->flags;
	int ret = 0;

	save_flags = ls->flags;
	ls->flags |= LEXER;
	while (!next_token(ls)) {
		int t = ls->ctok->type;

		switch (t) {
		case COMMENT:
			if (ls->flags & WARN_ANNOYING) {
				warning(l, "comment in the middle of "
					"a cpp directive");
			}
			/* fall through */
		case NONE:
			continue;
		case NEWLINE:
			/* null directive */
			if (ls->flags & WARN_ANNOYING) {
				/* truly an annoying warning; null directives
				   are rare but may increase readability of
				   some source files, and they are legal */
				warning(l, "null cpp directive");
			}
			if (!(ls->flags & LEXER)) put_char(ls, '\n');
			goto handle_exit2;
		case NAME:
			break;
		default:
			if (ls->flags & FAIL_SHARP) {
                                /* LPS 20050602 - ignores '#!' if on the first line */
                                if( ( l == 1 ) &&
                                    ( ls->condcomp ) )
                                {
					ret = 1;
                                }
                                else
                                /* LPS 20050602 */
				if (ls->condcomp) {
					error(l, "rogue '#'");
					ret = 1;
				} else {
					if (ls->flags & WARN_STANDARD) {
						warning(l, "rogue '#' in code "
							"compiled out");
						ret = 0;
					}
				}
				ls->flags = save_flags;
				goto handle_warp_ign;
			} else {
				struct token u;

				u.type = sharp_type;
				u.line = l;
				ls->flags = save_flags;
				print_token(ls, &u, 0);
				print_token(ls, ls->ctok, 0);
				if (ls->flags & WARN_ANNOYING) {
					warning(l, "rogue '#' dumped");
				}
				goto handle_exit3;
			}
		}
		if (ls->condcomp) {
			if (!strcmp(ls->ctok->name, "define")) {
				ret = handle_define(ls);
				goto handle_exit;
			} else if (!strcmp(ls->ctok->name, "undef")) {
				ret = handle_undef(ls);
				goto handle_exit;
			} else if (!strcmp(ls->ctok->name, "if")) {
				if ((++ ls->ifnest) > 63) goto too_many_if;
				condfclr(ls->ifnest - 1);
				ret = handle_if(ls);
				if (ret > 0) ret = 0;
				else if (ret == 0) {
					ls->condcomp = 0;
					ls->condmet = 0;
					ls->condnest = ls->ifnest - 1;
				}
				else ret = 1;
				goto handle_exit;
			} else if (!strcmp(ls->ctok->name, "ifdef")) {
				if ((++ ls->ifnest) > 63) goto too_many_if;
				condfclr(ls->ifnest - 1);
				ret = handle_ifdef(ls);
				if (ret > 0) ret = 0;
				else if (ret == 0) {
					ls->condcomp = 0;
					ls->condmet = 0;
					ls->condnest = ls->ifnest - 1;
				}
				else ret = 1;
				goto handle_exit;
			} else if (!strcmp(ls->ctok->name, "ifndef")) {
				if ((++ ls->ifnest) > 63) goto too_many_if;
				condfclr(ls->ifnest - 1);
				ret = handle_ifndef(ls);
				if (ret > 0) ret = 0;
				else if (ret == 0) {
					ls->condcomp = 0;
					ls->condmet = 0;
					ls->condnest = ls->ifnest - 1;
				}
				else ret = 1;
				goto handle_exit;
			} else if (!strcmp(ls->ctok->name, "else")) {
				if (ls->ifnest == 0
					|| condfval(ls->ifnest - 1)) {
					error(l, "rogue #else");
					ret = 1;
					goto handle_warp;
				}
				condfset(ls->ifnest - 1);
				if (ls->ifnest == 1) protect_detect.state = 0;
				ls->condcomp = 0;
				ls->condmet = 1;
				ls->condnest = ls->ifnest - 1;
				goto handle_warp;
			} else if (!strcmp(ls->ctok->name, "elif")) {
				if (ls->ifnest == 0
					|| condfval(ls->ifnest - 1)) {
					error(l, "rogue #elif");
					ret = 1;
					goto handle_warp_ign;
				}
				if (ls->ifnest == 1) protect_detect.state = 0;
				ls->condcomp = 0;
				ls->condmet = 1;
				ls->condnest = ls->ifnest - 1;
				goto handle_warp_ign;
			} else if (!strcmp(ls->ctok->name, "endif")) {
				if (ls->ifnest == 0) {
					error(l, "unmatched #endif");
					ret = 1;
					goto handle_warp;
				}
				if ((-- ls->ifnest) == 0
					&& protect_detect.state == 2) {
					protect_detect.state = 3;
				}
				goto handle_warp;
			} else if (!strcmp(ls->ctok->name, "include")) {
				ret = handle_include(ls, save_flags, 0);
				goto handle_exit3;
			} else if (!strcmp(ls->ctok->name, "include_next")) {
				ret = handle_include(ls, save_flags, 1);
				goto handle_exit3;
			} else if (!strcmp(ls->ctok->name, "pragma")) {
				if (!(save_flags & LEXER)) {
#ifdef PRAGMA_DUMP
					/* dump #pragma in output */
					struct token u;

					u.type = sharp_type;
					u.line = l;
					ls->flags = save_flags;
					print_token(ls, &u, 0);
					print_token(ls, ls->ctok, 0);
					while (ls->flags |= LEXER,
						!next_token(ls)) {
						long save_line;

						ls->flags &= ~LEXER;
						save_line = ls->line;
						ls->line = l;
						print_token(ls, ls->ctok, 0);
						ls->line = save_line;
						if (ls->ctok->type == NEWLINE)
							break;
					}
					goto handle_exit3;
#else
					if (ls->flags & WARN_PRAGMA)
						warning(l, "#pragma ignored "
							"and not dumped");
					goto handle_warp_ign;
#endif
				}
				if (!(ls->flags & HANDLE_PRAGMA))
					goto handle_warp_ign;
				handle_pragma(ls);
				goto handle_exit;
			} else if (!strcmp(ls->ctok->name, "error")) {
				ret = 1;
				handle_error(ls);
				goto handle_exit;
			} else if (!strcmp(ls->ctok->name, "line")) {
				ret = handle_line(ls, save_flags);
				goto handle_exit;
			} else if ((ls->flags & HANDLE_ASSERTIONS)
				&& !strcmp(ls->ctok->name, "assert")) {
				ret = handle_assert(ls);
				goto handle_exit;
			} else if ((ls->flags & HANDLE_ASSERTIONS)
				&& !strcmp(ls->ctok->name, "unassert")) {
				ret = handle_unassert(ls);
				goto handle_exit;
			}
		} else {
			if (!strcmp(ls->ctok->name, "else")) {
				if (condfval(ls->ifnest - 1)
					&& (ls->flags & WARN_STANDARD)) {
					warning(l, "rogue #else in code "
						"compiled out");
				}
				if (ls->condnest == ls->ifnest - 1) {
					if (!ls->condmet) ls->condcomp = 1;
				}
				condfset(ls->ifnest - 1);
				if (ls->ifnest == 1) protect_detect.state = 0;
				goto handle_warp;
			} else if (!strcmp(ls->ctok->name, "elif")) {
				if (condfval(ls->ifnest - 1)
					&& (ls->flags & WARN_STANDARD)) {
					warning(l, "rogue #elif in code "
						"compiled out");
				}
				if (ls->condnest != ls->ifnest - 1
					|| ls->condmet)
					goto handle_warp_ign;
				if (ls->ifnest == 1) protect_detect.state = 0;
				ret = handle_if(ls);
				if (ret > 0) {
					ls->condcomp = 1;
					ls->condmet = 1;
					ret = 0;
				} else if (ret < 0) ret = 1;
				goto handle_exit;
			} else if (!strcmp(ls->ctok->name, "endif")) {
				if ((-- ls->ifnest) == ls->condnest) {
					if (ls->ifnest == 0 &&
						protect_detect.state == 2)
						protect_detect.state = 3;
					ls->condcomp = 1;
				}
				goto handle_warp;
			} else if (!strcmp(ls->ctok->name, "if")
				|| !strcmp(ls->ctok->name, "ifdef")
				|| !strcmp(ls->ctok->name, "ifndef")) {
				if ((++ ls->ifnest) > 63) goto too_many_if;
				condfclr(ls->ifnest - 1);
			}
			goto handle_warp_ign;
		}
		/*
		 * Unrecognized directive. We emit either an error or
		 * an annoying warning, depending on a command-line switch.
		 */
		if (ls->flags & FAIL_SHARP) {
			error(l, "unknown cpp directive '#%s'",
				ls->ctok->name);
			goto handle_warp_ign;
		} else {
			struct token u;

			u.type = sharp_type;
			u.line = l;
			ls->flags = save_flags;
			print_token(ls, &u, 0);
			print_token(ls, ls->ctok, 0);
			if (ls->flags & WARN_ANNOYING) {
				warning(l, "rogue '#' dumped");
			}
		}
	}
	return 1;

handle_warp_ign:
	while (!next_token(ls)) if (ls->ctok->type == NEWLINE) break;
	goto handle_exit;
handle_warp:
	while (!next_token(ls)) {
		if (!ttWHI(ls->ctok->type) && (ls->flags & WARN_STANDARD)) {
			warning(l, "trailing garbage in "
				"preprocessing directive");
		}
		if (ls->ctok->type == NEWLINE) break;
	}
handle_exit:
	if (!(ls->flags & LEXER)) put_char(ls, '\n');
handle_exit3:
	if (protect_detect.state == 1) {
		protect_detect.state = 0;
	} else if (protect_detect.state == -1) {
		/* just after the #include */
		protect_detect.state = 1;
	}
handle_exit2:
	ls->flags = save_flags;
	return ret;
too_many_if:
	error(l, "too many levels of conditional inclusion (max 63)");
	ret = 1;
	goto handle_warp;
#undef condfset
#undef condfclr
#undef condfval
}

/*
 * This is the main entry function. It maintains count of #, and call the
 * appropriate functions when it encounters a cpp directive or a macro
 * name.
 * return value: positive on error; CPPERR_EOF means "end of input reached"
 */
int cpp(struct lexer_state *ls)
{
	int r = 0;

	while (next_token(ls)) {
		if (protect_detect.state == 3) {
			/*
			 * At that point, protect_detect.ff->protect might
			 * be non-zero, if the file has been recursively
			 * included, and a guardian detected.
			 */
			if (!protect_detect.ff->protect) {
				/* Cool ! A new guardian has been detected. */
				protect_detect.ff->protect =
					protect_detect.macro;
			} else if (protect_detect.macro) {
				/* We found a guardian but an old one. */
				freemem(protect_detect.macro);
			}
			protect_detect.macro = 0;
		}
		if (ls->ifnest) {
			error(ls->line, "unterminated #if construction "
				"(depth %ld)", ls->ifnest);
			r = CPPERR_NEST;
		}
		if (ls_depth == 0) return CPPERR_EOF;
		close_input(ls);
		if (!(ls->flags & LEXER) && !ls->ltwnl) {
			put_char(ls, '\n');
			ls->ltwnl = 1;
		}
		pop_file_context(ls);
		ls->oline ++;
		if (enter_file(ls, ls->flags)) {
			ls->ctok->type = NEWLINE;
			ls->ltwnl = 1;
			break;
		}
	}
	if (!(ls->ltwnl && (ls->ctok->type == SHARP
		|| ls->ctok->type == DIG_SHARP))
		&& protect_detect.state == 1 && !ttWHI(ls->ctok->type)) {
		/* the first non-whitespace token encountered is not
		   a sharp introducing a cpp directive */
		protect_detect.state = 0;
	}
	if (protect_detect.state == 3 && !ttWHI(ls->ctok->type)) {
		/* a non-whitespace token encountered after the #endif */
		protect_detect.state = 0;
	}
	if (ls->condcomp) {
		if (ls->ltwnl && (ls->ctok->type == SHARP
			|| ls->ctok->type == DIG_SHARP)) {
			int x = handle_cpp(ls, ls->ctok->type);

			ls->ltwnl = 1;
			return r ? r : x;
		}
		if (ls->ctok->type == NAME) {
			struct macro *m;

			if ((m = get_macro(ls->ctok->name)) != 0) {
				int x;

				x = substitute_macro(ls, m, 0, 1, 0,
					ls->ctok->line);
				if (!(ls->flags & LEXER))
					garbage_collect(ls->gf);
				return r ? r : x;
			}
			if (!(ls->flags & LEXER))
				print_token(ls, ls->ctok, 0);
		}
	} else {
		if (ls->ltwnl && (ls->ctok->type == SHARP
			|| ls->ctok->type == DIG_SHARP)) {
			int x = handle_cpp(ls, ls->ctok->type);

			ls->ltwnl = 1;
			return r ? r : x;
		}
	}
	if (ls->ctok->type == NEWLINE) ls->ltwnl = 1;
	else if (!ttWHI(ls->ctok->type)) ls->ltwnl = 0;
	return r ? r : -1;
}

#ifndef STAND_ALONE
/*
 * llex() and lex() are the lexing functions, when the preprocessor is
 * linked to another code. llex() should be called only by lex().
 */
static int llex(struct lexer_state *ls)
{
	struct token_fifo *tf = ls->output_fifo;
	int r;

	if (tf->nt != 0) {
		if (tf->art < tf->nt) {
#ifdef INMACRO_FLAG
			if (!ls->inmacro) {
				ls->inmacro = 1;
				ls->macro_count ++;
			}
#endif
			ls->ctok = tf->t + (tf->art ++);
			if (ls->ctok->type > DIGRAPH_TOKENS
				&& ls->ctok->type < DIGRAPH_TOKENS_END) {
				ls->ctok->type = undig(ls->ctok->type);
			}
			return 0;
		} else {
#ifdef INMACRO_FLAG
			ls->inmacro = 0;
#endif
			freemem(tf->t);
			tf->art = tf->nt = 0;
			garbage_collect(ls->gf);
			ls->ctok = ls->save_ctok;
		}
	}
	r = cpp(ls);
	if (ls->ctok->type > DIGRAPH_TOKENS
		&& ls->ctok->type < LAST_MEANINGFUL_TOKEN) {
		ls->ctok->type = undig(ls->ctok->type);
	}
	if (r > 0) return r;
	if (r < 0) return 0;
	return llex(ls);
}

/*
 * lex() reads the next token from the processed stream and stores it
 * into ls->ctok.
 * return value: non zero on error (including CPPERR_EOF, which is not
 * quite an error)
 */
int lex(struct lexer_state *ls)
{
	int r;

	do {
		r = llex(ls);
#ifdef SEMPER_FIDELIS
	} while (!r && !ls->condcomp);
#else
	} while (!r && (!ls->condcomp || (ttWHI(ls->ctok->type) &&
		(!(ls->flags & LINE_NUM) || ls->ctok->type != NEWLINE))));
#endif
	return r;
}
#endif

/*
 * check_cpp_errors() must be called when the end of input is reached;
 * it checks pending errors due to truncated constructs (actually none,
 * this is reserved for future evolutions).
 */
int check_cpp_errors(struct lexer_state *ls)
{
	if (ls->flags & KEEP_OUTPUT) {
		put_char(ls, '\n');
	}
	if (emit_dependencies) fputc('\n', emit_output);
#ifndef NO_UCPP_BUF
	if (!(ls->flags & LEXER)) {
		flush_output(ls);
	}
#endif
	if ((ls->flags & WARN_TRIGRAPHS) && ls->count_trigraphs)
		warning(0, "%ld trigraph(s) encountered", ls->count_trigraphs);
	return 0;
}

/*
 * init_cpp() initializes static tables inside ucpp. It needs not be
 * called more than once.
 */
void init_cpp(void)
{
	init_cppm();
}

/*
 * (re)init the global tables.
 * If standard_assertions is non 0, init the assertions table.
 */
void init_tables(int with_assertions)
{
	time_t t;
	struct tm *ct;

	init_buf_lexer_state(&dsharp_lexer, 0);
#ifdef PRAGMA_TOKENIZE
	init_buf_lexer_state(&tokenize_lexer, 0);
#endif
	time(&t);
	ct = localtime(&t);
#ifdef NOSTRFTIME
	/* we have a quite old compiler, that does not know the
	   (standard since 1990) strftime() function. */
	{
		char *c = asctime(ct);

		compile_time[0] = '"';
		mmv(compile_time + 1, c + 11, 8);
		compile_time[9] = '"';
		compile_time[10] = 0;
		compile_date[0] = '"';
		mmv(compile_date + 1, c + 4, 7);
		mmv(compile_date + 8, c + 20, 4);
		compile_date[12] = '"';
		compile_date[13] = 0;
	}
#else
	strftime(compile_time, 12, "\"%H:%M:%S\"", ct);
	strftime(compile_date, 24, "\"%b %d %Y\"", ct);
#endif
	init_macros();
	if (with_assertions) init_assertions();
	init_found_files();
}

/*
 * Resets the include path.
 */
void init_include_path(char *incpath[])
{
	if (include_path_nb) {
		size_t i;

		for (i = 0; i < include_path_nb; i ++)
			freemem(include_path[i]);
		freemem(include_path);
		include_path_nb = 0;
	}
	if (incpath) {
		int i;

		for (i = 0; incpath[i]; i ++)
			aol(include_path, include_path_nb,
				sdup(incpath[i]), INCPATH_MEMG);
	}
}

/*
 * add_incpath() adds "path" to the standard include path.
 */
void add_incpath(char *path)
{
	aol(include_path, include_path_nb, sdup(path), INCPATH_MEMG);
}

/*
 * This function cleans the memory. It should release all allocated
 * memory structures and may be called even if the current pre-processing
 * is not finished or reported an error.
 */
void wipeout()
{
	struct lexer_state ls;

	if (include_path_nb > 0) {
		size_t i;

		for (i = 0; i < include_path_nb; i ++)
			freemem(include_path[i]);
		freemem(include_path);
		include_path = 0;
		include_path_nb = 0;
	}
	if (current_filename) freemem(current_filename);
	current_filename = 0;
	current_long_filename = 0;
	current_incdir = -1;
	protect_detect.state = 0;
	if (protect_detect.macro) freemem(protect_detect.macro);
	protect_detect.macro = 0;
	protect_detect.ff = 0;
	init_lexer_state(&ls);
	while (ls_depth > 0) pop_file_context(&ls);
	free_lexer_state(&ls);
	free_lexer_state(&dsharp_lexer);
#ifdef PRAGMA_TOKENIZE
	free_lexer_state(&tokenize_lexer);
#endif
	if (found_files_init_done) HTT_kill(&found_files);
	found_files_init_done = 0;
	if (found_files_sys_init_done) HTT_kill(&found_files_sys);
	found_files_sys_init_done = 0;
	wipe_macros();
	wipe_assertions();
}

#ifdef STAND_ALONE
/*
 * print some help
 */
static void usage(char *command_name)
{
	fprintf(stderr,
	"Usage: %s [options] [file]\n"
	"language options:\n"
	"  -C              keep comments in output\n"
	"  -s              keep '#' when no cpp directive is recognized\n"
	"  -l              do not emit line numbers\n"
	"  -lg             emit gcc-like line numbers\n"
	"  -CC             disable C++-like comments\n"
	"  -a, -na, -a0    handle (or not) assertions\n"
	"  -V              disable macros with extra arguments\n"
	"  -u              understand UTF-8 in source\n"
	"  -X              enable -a, -u and -Y\n"
	"  -c90            mimic C90 behaviour\n"
	"  -t              disable trigraph support\n"
	"warning options:\n"
	"  -wt             emit a final warning when trigaphs are encountered\n"
	"  -wtt            emit warnings for each trigaph encountered\n"
	"  -wa             emit warnings that are usually useless\n"
	"  -w0             disable standard warnings\n"
	"directory options:\n"
	"  -I directory    add 'directory' before the standard include path\n"
	"  -J directory    add 'directory' after the standard include path\n"
	"  -zI             do not use the standard include path\n"
	"  -M              emit Makefile-like dependencies instead of normal "
			"output\n"
	"  -Ma             emit also dependancies for system files\n"
	"  -o file         store output in file\n"
	"macro and assertion options:\n"
	"  -Dmacro         predefine 'macro'\n"
	"  -Dmacro=def     predefine 'macro' with 'def' content\n"
	"  -Umacro         undefine 'macro'\n"
	"  -Afoo(bar)      assert foo(bar)\n"
	"  -Bfoo(bar)      unassert foo(bar)\n"
	"  -Y              predefine system-dependant macros\n"
	"  -Z              do not predefine special macros\n"
	"  -d              emit defined macros\n"
	"  -e              emit assertions\n"
	"misc options:\n"
	"  -v              print version number and settings\n"
	"  -h              show this help\n",
	command_name);
}

/*
 * print version and compile-time settings
 */
static void version(void)
{
	size_t i;

	fprintf(stderr, "ucpp version %d.%d\n", VERS_MAJ, VERS_MIN);
	fprintf(stderr, "search path:\n");
	for (i = 0; i < include_path_nb; i ++)
		fprintf(stderr, "  %s\n", include_path[i]);
}

/*
 * parse_opt() initializes many things according to the command-line
 * options.
 * Return values:
 * 0  on success
 * 1  on semantic error (redefinition of a special macro, for instance)
 * 2  on syntaxic error (unknown options for instance)
 */
static int parse_opt(int argc, char *argv[], struct lexer_state *ls)
{
	int i, ret = 0;
	char *filename = 0;
	int with_std_incpath = 1;
	int print_version = 0, print_defs = 0, print_asserts = 0;
	int system_macros = 0, standard_assertions = 1;

	init_lexer_state(ls);
	ls->flags = DEFAULT_CPP_FLAGS;
	emit_output = ls->output = stdout;
	for (i = 1; i < argc; i ++) if (argv[i][0] == '-') {
		if (!strcmp(argv[i], "-h")) {
			return 2;
		} else if (!strcmp(argv[i], "-C")) {
			ls->flags &= ~DISCARD_COMMENTS;
		} else if (!strcmp(argv[i], "-CC")) {
			ls->flags &= ~CPLUSPLUS_COMMENTS;
		} else if (!strcmp(argv[i], "-a")) {
			ls->flags |= HANDLE_ASSERTIONS;
		} else if (!strcmp(argv[i], "-na")) {
			ls->flags |= HANDLE_ASSERTIONS;
			standard_assertions = 0;
		} else if (!strcmp(argv[i], "-a0")) {
			ls->flags &= ~HANDLE_ASSERTIONS;
		} else if (!strcmp(argv[i], "-V")) {
			ls->flags &= ~MACRO_VAARG;
		} else if (!strcmp(argv[i], "-u")) {
			ls->flags |= UTF8_SOURCE;
		} else if (!strcmp(argv[i], "-X")) {
			ls->flags |= HANDLE_ASSERTIONS;
			ls->flags |= UTF8_SOURCE;
			system_macros = 1;
		} else if (!strcmp(argv[i], "-c90")) {
			ls->flags &= ~MACRO_VAARG;
			ls->flags &= ~CPLUSPLUS_COMMENTS;
			c99_compliant = 0;
			c99_hosted = -1;
		} else if (!strcmp(argv[i], "-t")) {
			ls->flags &= ~HANDLE_TRIGRAPHS;
		} else if (!strcmp(argv[i], "-wt")) {
			ls->flags |= WARN_TRIGRAPHS;
		} else if (!strcmp(argv[i], "-wtt")) {
			ls->flags |= WARN_TRIGRAPHS_MORE;
		} else if (!strcmp(argv[i], "-wa")) {
			ls->flags |= WARN_ANNOYING;
		} else if (!strcmp(argv[i], "-w0")) {
			ls->flags &= ~WARN_STANDARD;
			ls->flags &= ~WARN_PRAGMA;
		} else if (!strcmp(argv[i], "-s")) {
			ls->flags &= ~FAIL_SHARP;
		} else if (!strcmp(argv[i], "-l")) {
			ls->flags &= ~LINE_NUM;
		} else if (!strcmp(argv[i], "-lg")) {
			ls->flags |= GCC_LINE_NUM;
		} else if (!strcmp(argv[i], "-M")) {
			ls->flags &= ~KEEP_OUTPUT;
			emit_dependencies = 1;
		} else if (!strcmp(argv[i], "-Ma")) {
			ls->flags &= ~KEEP_OUTPUT;
			emit_dependencies = 2;
		} else if (!strcmp(argv[i], "-Y")) {
			system_macros = 1;
		} else if (!strcmp(argv[i], "-Z")) {
			no_special_macros = 1;
		} else if (!strcmp(argv[i], "-d")) {
			ls->flags &= ~KEEP_OUTPUT;
			print_defs = 1;
		} else if (!strcmp(argv[i], "-e")) {
			ls->flags &= ~KEEP_OUTPUT;
			print_asserts = 1;
		} else if (!strcmp(argv[i], "-zI")) {
			with_std_incpath = 0;
		} else if (!strcmp(argv[i], "-I") || !strcmp(argv[i], "-J")) {
			i ++;
		} else if (!strcmp(argv[i], "-o")) {
			if ((++ i) >= argc) {
				error(-1, "missing filename after -o");
				return 2;
			}
			if (argv[i][0] == '-' && argv[i][1] == 0) {
				emit_output = ls->output = stdout;
			} else {
				ls->output = fopen(argv[i], "w");
				if (!ls->output) {
					error(-1, "failed to open for "
						"writing: %s", argv[i]);
					return 2;
				}
				emit_output = ls->output;
			}
		} else if (!strcmp(argv[i], "-v")) {
			print_version = 1;
		} else if (argv[i][1] != 'I' && argv[i][1] != 'J'
			&& argv[i][1] != 'D' && argv[i][1] != 'U'
			&& argv[i][1] != 'A' && argv[i][1] != 'B')
			warning(-1, "unknown option '%s'", argv[i]);
	} else {
		if (filename != 0) {
			error(-1, "spurious filename '%s'", argv[i]);
			return 2;
		}
		filename = argv[i];
	}
	init_tables(ls->flags & HANDLE_ASSERTIONS);
	init_include_path(0);
	if (filename) {
#ifdef UCPP_MMAP
		FILE *f = fopen_mmap_file(filename);

		ls->input = 0;
		if (f) set_input_file(ls, f);
#else
		ls->input = fopen(filename, "r");
#endif
		if (!ls->input) {
			error(-1, "file '%s' not found", filename);
			return 1;
		}
#ifdef NO_LIBC_BUF
		setbuf(ls->input, 0);
#endif
		set_init_filename(filename, 1);
	} else {
		ls->input = stdin;
		set_init_filename("<stdin>", 0);
	}
	for (i = 1; i < argc; i ++)
		if (argv[i][0] == '-' && argv[i][1] == 'I')
			add_incpath(argv[i][2] ? argv[i] + 2 : argv[i + 1]);
	if (system_macros) for (i = 0; system_macros_def[i]; i ++)
		ret = ret || define_macro(ls, system_macros_def[i]);
	for (i = 1; i < argc; i ++)
		if (argv[i][0] == '-' && argv[i][1] == 'D')
			ret = ret || define_macro(ls, argv[i] + 2);
	for (i = 1; i < argc; i ++)
		if (argv[i][0] == '-' && argv[i][1] == 'U')
			ret = ret || undef_macro(ls, argv[i] + 2);
	if (ls->flags & HANDLE_ASSERTIONS) {
		if (standard_assertions)
			for (i = 0; system_assertions_def[i]; i ++)
				make_assertion(system_assertions_def[i]);
		for (i = 1; i < argc; i ++)
			if (argv[i][0] == '-' && argv[i][1] == 'A')
				ret = ret || make_assertion(argv[i] + 2);
		for (i = 1; i < argc; i ++)
			if (argv[i][0] == '-' && argv[i][1] == 'B')
				ret = ret || destroy_assertion(argv[i] + 2);
	} else {
		for (i = 1; i < argc; i ++)
			if (argv[i][0] == '-'
				&& (argv[i][1] == 'A' || argv[i][1] == 'B'))
				warning(-1, "assertions disabled");
	}
	if (with_std_incpath) {
		for (i = 0; include_path_std[i]; i ++)
			add_incpath(include_path_std[i]);
	}
	for (i = 1; i < argc; i ++)
		if (argv[i][0] == '-' && argv[i][1] == 'J')
			add_incpath(argv[i][2] ? argv[i] + 2 : argv[i + 1]);

	if (print_version) {
		version();
		return 1;
	}
	if (print_defs) {
		print_defines();
		emit_defines = 1;
	}
	if (print_asserts && (ls->flags & HANDLE_ASSERTIONS)) {
		print_assertions();
		emit_assertions = 1;
	}
	return ret;
}

int main(int argc, char *argv[])
{
	struct lexer_state ls;
	int r, fr = 0;

	init_cpp();
	if ((r = parse_opt(argc, argv, &ls)) != 0) {
		if (r == 2) usage(argv[0]);
		return EXIT_FAILURE;
	}
	enter_file(&ls, ls.flags);
	while ((r = cpp(&ls)) < CPPERR_EOF) fr = fr || (r > 0);
	fr = fr || check_cpp_errors(&ls);
	free_lexer_state(&ls);
	wipeout();
#ifdef MEM_DEBUG
	report_leaks();
#endif
	return fr ? EXIT_FAILURE : EXIT_SUCCESS;
}
#endif
