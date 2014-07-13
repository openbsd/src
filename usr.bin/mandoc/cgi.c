/*	$Id: cgi.c,v 1.9 2014/07/13 09:58:52 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014 Ingo Schwarze <schwarze@usta.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "main.h"
#include "manpath.h"
#include "mansearch.h"
#include "cgi.h"

/*
 * A query as passed to the search function.
 */
struct	query {
	const char	*manpath; /* desired manual directory */
	const char	*arch; /* architecture */
	const char	*sec; /* manual section */
	const char	*expr; /* unparsed expression string */
	int		 equal; /* match whole names, not substrings */
};

struct	req {
	struct query	  q;
	char		**p; /* array of available manpaths */
	size_t		  psz; /* number of available manpaths */
};

static	void		 catman(const struct req *, const char *);
static	int	 	 cmp(const void *, const void *);
static	void		 format(const struct req *, const char *);
static	void		 html_print(const char *);
static	void		 html_printquery(const struct req *);
static	void		 html_putchar(char);
static	int 		 http_decode(char *);
static	void		 http_parse(struct req *, char *);
static	void		 http_print(const char *);
static	void 		 http_putchar(char);
static	void		 http_printquery(const struct req *);
static	void		 pathgen(struct req *);
static	void		 pg_search(const struct req *);
static	void		 pg_show(const struct req *, const char *);
static	void		 resp_begin_html(int, const char *);
static	void		 resp_begin_http(int, const char *);
static	void		 resp_end_html(void);
static	void		 resp_error_badrequest(const char *);
static	void		 resp_error_internal(void);
static	void		 resp_index(const struct req *);
static	void		 resp_noresult(const struct req *,
				const char *);
static	void		 resp_search(const struct req *,
				struct manpage *, size_t);
static	void		 resp_searchform(const struct req *);

static	const char	 *scriptname; /* CGI script name */
static	const char	 *httphost; /* hostname used in the URIs */

static	const char *const sec_numbers[] = {
    "0", "1", "2", "3", "3p", "4", "5", "6", "7", "8", "9"
};
static	const char *const sec_names[] = {
    "All Sections",
    "1 - General Commands",
    "2 - System Calls",
    "3 - Subroutines",
    "3p - Perl Subroutines",
    "4 - Special Files",
    "5 - File Formats",
    "6 - Games",
    "7 - Macros and Conventions",
    "8 - Maintenance Commands",
    "9 - Kernel Interface"
};
static	const int sec_MAX = sizeof(sec_names) / sizeof(char *);

static	const char *const arch_names[] = {
    "amd64",       "alpha",       "armish",      "armv7",
    "aviion",      "hppa",        "hppa64",      "i386",
    "ia64",        "landisk",     "loongson",    "luna88k",
    "macppc",      "mips64",      "octeon",      "sgi",
    "socppc",      "solbourne",   "sparc",       "sparc64",
    "vax",         "zaurus",
    "amiga",       "arc",         "arm32",       "atari",
    "beagle",      "cats",        "hp300",       "mac68k",
    "mvme68k",     "mvme88k",     "mvmeppc",     "palm",
    "pc532",       "pegasos",     "pmax",        "powerpc",
    "sun3",        "wgrisc",      "x68k"
};
static	const int arch_MAX = sizeof(arch_names) / sizeof(char *);

/*
 * Print a character, escaping HTML along the way.
 * This will pass non-ASCII straight to output: be warned!
 */
static void
html_putchar(char c)
{

	switch (c) {
	case ('"'):
		printf("&quote;");
		break;
	case ('&'):
		printf("&amp;");
		break;
	case ('>'):
		printf("&gt;");
		break;
	case ('<'):
		printf("&lt;");
		break;
	default:
		putchar((unsigned char)c);
		break;
	}
}

static void
http_printquery(const struct req *req)
{

	if (NULL != req->q.manpath) {
		printf("&manpath=");
		http_print(req->q.manpath);
	}
	if (NULL != req->q.sec) {
		printf("&sec=");
		http_print(req->q.sec);
	}
	if (NULL != req->q.arch) {
		printf("&arch=");
		http_print(req->q.arch);
	}
	if (NULL != req->q.expr) {
		printf("&query=");
		http_print(req->q.expr);
	}
	if (0 == req->q.equal)
		printf("&apropos=1");
}

static void
html_printquery(const struct req *req)
{

	if (NULL != req->q.manpath) {
		printf("&amp;manpath=");
		html_print(req->q.manpath);
	}
	if (NULL != req->q.sec) {
		printf("&amp;sec=");
		html_print(req->q.sec);
	}
	if (NULL != req->q.arch) {
		printf("&amp;arch=");
		html_print(req->q.arch);
	}
	if (NULL != req->q.expr) {
		printf("&amp;query=");
		html_print(req->q.expr);
	}
	if (0 == req->q.equal)
		printf("&amp;apropos=1");
}

static void
http_print(const char *p)
{

	if (NULL == p)
		return;
	while ('\0' != *p)
		http_putchar(*p++);
}

/*
 * Call through to html_putchar().
 * Accepts NULL strings.
 */
static void
html_print(const char *p)
{
	
	if (NULL == p)
		return;
	while ('\0' != *p)
		html_putchar(*p++);
}

/*
 * Parse out key-value pairs from an HTTP request variable.
 * This can be either a cookie or a POST/GET string, although man.cgi
 * uses only GET for simplicity.
 */
static void
http_parse(struct req *req, char *p)
{
	char            *key, *val;

	memset(&req->q, 0, sizeof(struct query));
	req->q.manpath = req->p[0];
	req->q.equal = 1;

	while ('\0' != *p) {
		key = p;
		val = NULL;

		p += (int)strcspn(p, ";&");
		if ('\0' != *p)
			*p++ = '\0';
		if (NULL != (val = strchr(key, '=')))
			*val++ = '\0';

		if ('\0' == *key || NULL == val || '\0' == *val)
			continue;

		/* Just abort handling. */

		if ( ! http_decode(key))
			break;
		if (NULL != val && ! http_decode(val))
			break;

		if (0 == strcmp(key, "query"))
			req->q.expr = val;
		else if (0 == strcmp(key, "manpath"))
			req->q.manpath = val;
		else if (0 == strcmp(key, "apropos"))
			req->q.equal = !strcmp(val, "0");
		else if (0 == strcmp(key, "sec") ||
			 0 == strcmp(key, "sektion")) {
			if (strcmp(val, "0"))
				req->q.sec = val;
		} else if (0 == strcmp(key, "arch")) {
			if (strcmp(val, "default"))
				req->q.arch = val;
		}
	}
}

static void
http_putchar(char c)
{

	if (isalnum((unsigned char)c)) {
		putchar((unsigned char)c);
		return;
	} else if (' ' == c) {
		putchar('+');
		return;
	}
	printf("%%%.2x", c);
}

/*
 * HTTP-decode a string.  The standard explanation is that this turns
 * "%4e+foo" into "n foo" in the regular way.  This is done in-place
 * over the allocated string.
 */
static int
http_decode(char *p)
{
	char             hex[3];
	char		*q;
	int              c;

	hex[2] = '\0';

	q = p;
	for ( ; '\0' != *p; p++, q++) {
		if ('%' == *p) {
			if ('\0' == (hex[0] = *(p + 1)))
				return(0);
			if ('\0' == (hex[1] = *(p + 2)))
				return(0);
			if (1 != sscanf(hex, "%x", &c))
				return(0);
			if ('\0' == c)
				return(0);

			*q = (char)c;
			p += 2;
		} else
			*q = '+' == *p ? ' ' : *p;
	}

	*q = '\0';
	return(1);
}

static void
resp_begin_http(int code, const char *msg)
{

	if (200 != code)
		printf("Status: %d %s\r\n", code, msg);

	printf("Content-Type: text/html; charset=utf-8\r\n"
	     "Cache-Control: no-cache\r\n"
	     "Pragma: no-cache\r\n"
	     "\r\n");

	fflush(stdout);
}

static void
resp_begin_html(int code, const char *msg)
{

	resp_begin_http(code, msg);

	printf("<!DOCTYPE HTML PUBLIC "
	       " \"-//W3C//DTD HTML 4.01//EN\""
	       " \"http://www.w3.org/TR/html4/strict.dtd\">\n"
	       "<HTML>\n"
	       "<HEAD>\n"
	       "<META HTTP-EQUIV=\"Content-Type\""
	       " CONTENT=\"text/html; charset=utf-8\">\n"
	       "<LINK REL=\"stylesheet\" HREF=\"%s/man-cgi.css\""
	       " TYPE=\"text/css\" media=\"all\">\n"
	       "<LINK REL=\"stylesheet\" HREF=\"%s/man.css\""
	       " TYPE=\"text/css\" media=\"all\">\n"
	       "<TITLE>%s</TITLE>\n"
	       "</HEAD>\n"
	       "<BODY>\n"
	       "<!-- Begin page content. //-->\n",
	       CSS_DIR, CSS_DIR, CUSTOMIZE_TITLE);
}

static void
resp_end_html(void)
{

	puts("</BODY>\n"
	     "</HTML>");
}

static void
resp_searchform(const struct req *req)
{
	int		 i;

	puts(CUSTOMIZE_BEGIN);
	puts("<!-- Begin search form. //-->");
	printf("<DIV ID=\"mancgi\">\n"
	       "<FORM ACTION=\"%s\" METHOD=\"get\">\n"
	       "<FIELDSET>\n"
	       "<LEGEND>Manual Page Search Parameters</LEGEND>\n",
	       scriptname);

	/* Write query input box. */

	printf(	"<TABLE><TR><TD>\n"
		"<INPUT TYPE=\"text\" NAME=\"query\" VALUE=\"");
	if (NULL != req->q.expr)
		html_print(req->q.expr);
	puts("\" SIZE=\"40\">");

	/* Write submission and reset buttons. */

	printf(	"<INPUT TYPE=\"submit\" VALUE=\"Submit\">\n"
		"<INPUT TYPE=\"reset\" VALUE=\"Reset\">\n");

	/* Write show radio button */

	printf(	"</TD><TD>\n"
		"<INPUT TYPE=\"radio\" ");
	if (req->q.equal)
		printf("CHECKED ");
	printf(	"NAME=\"apropos\" ID=\"show\" VALUE=\"0\">\n"
		"<LABEL FOR=\"show\">Show named manual page</LABEL>\n");

	/* Write section selector. */

	printf(	"</TD></TR><TR><TD>\n"
		"<SELECT NAME=\"sec\">");
	for (i = 0; i < sec_MAX; i++) {
		printf("<OPTION VALUE=\"%s\"", sec_numbers[i]);
		if (NULL != req->q.sec &&
		    0 == strcmp(sec_numbers[i], req->q.sec))
			printf(" SELECTED");
		printf(">%s</OPTION>\n", sec_names[i]);
	}
	puts("</SELECT>");

	/* Write architecture selector. */

	puts("<SELECT NAME=\"arch\">");
	for (i = 0; i < arch_MAX; i++) {
		printf("<OPTION VALUE=\"%s\"", arch_names[i]);
		if (NULL != req->q.arch &&
		    0 == strcmp(arch_names[i], req->q.arch))
			printf(" SELECTED");
		printf(">%s</OPTION>\n", arch_names[i]);
	}
	puts("</SELECT>");

	/* Write manpath selector. */

	if (req->psz > 1) {
		puts("<SELECT NAME=\"manpath\">");
		for (i = 0; i < (int)req->psz; i++) {
			printf("<OPTION ");
			if (NULL == req->q.manpath ? 0 == i :
			    0 == strcmp(req->q.manpath, req->p[i]))
				printf("SELECTED ");
			printf("VALUE=\"");
			html_print(req->p[i]);
			printf("\">");
			html_print(req->p[i]);
			puts("</OPTION>");
		}
		puts("</SELECT>");
	}

	/* Write search radio button */

	printf(	"</TD><TD>\n"
		"<INPUT TYPE=\"radio\" ");
	if (0 == req->q.equal)
		printf("CHECKED ");
	printf(	"NAME=\"apropos\" ID=\"search\" VALUE=\"1\">\n"
		"<LABEL FOR=\"search\">Search with apropos query</LABEL>\n");

	puts("</TD></TR></TABLE>\n"
	     "</FIELDSET>\n"
	     "</FORM>\n"
	     "</DIV>");
	puts("<!-- End search form. //-->");
}

static void
resp_index(const struct req *req)
{

	resp_begin_html(200, NULL);
	resp_searchform(req);
	printf("<P>\n"
	       "This web interface is documented in the "
	       "<A HREF=\"%s/mandoc/man8/man.cgi.8\">man.cgi</A> "
	       "manual, and the "
	       "<A HREF=\"%s/mandoc/man1/apropos.1\">apropos</A> "
	       "manual explains the query syntax.\n"
	       "</P>\n",
	       scriptname, scriptname);
	resp_end_html();
}

static void
resp_noresult(const struct req *req, const char *msg)
{
	resp_begin_html(200, NULL);
	resp_searchform(req);
	puts("<P>");
	puts(msg);
	puts("</P>");
	resp_end_html();
}

static void
resp_error_badrequest(const char *msg)
{

	resp_begin_html(400, "Bad Request");
	puts("<H1>Bad Request</H1>\n"
	     "<P>\n");
	puts(msg);
	printf("Try again from the\n"
	       "<A HREF=\"%s\">main page</A>.\n"
	       "</P>", scriptname);
	resp_end_html();
}

static void
resp_error_internal(void)
{
	resp_begin_html(500, "Internal Server Error");
	puts("<P>Internal Server Error</P>");
	resp_end_html();
}

static void
resp_search(const struct req *req, struct manpage *r, size_t sz)
{
	size_t		 i;

	if (1 == sz) {
		/*
		 * If we have just one result, then jump there now
		 * without any delay.
		 */
		printf("Status: 303 See Other\r\n");
		printf("Location: http://%s%s/%s/%s?",
		    httphost, scriptname, req->q.manpath, r[0].file);
		http_printquery(req);
		printf("\r\n"
		     "Content-Type: text/html; charset=utf-8\r\n"
		     "\r\n");
		return;
	}

	qsort(r, sz, sizeof(struct manpage), cmp);

	resp_begin_html(200, NULL);
	resp_searchform(req);
	puts("<DIV CLASS=\"results\">");
	puts("<TABLE>");

	for (i = 0; i < sz; i++) {
		printf("<TR>\n"
		       "<TD CLASS=\"title\">\n"
		       "<A HREF=\"%s/%s/%s?", 
		    scriptname, req->q.manpath, r[i].file);
		html_printquery(req);
		printf("\">");
		html_print(r[i].names);
		printf("</A>\n"
		       "</TD>\n"
		       "<TD CLASS=\"desc\">");
		html_print(r[i].output);
		puts("</TD>\n"
		     "</TR>");
	}

	puts("</TABLE>\n"
	     "</DIV>");
	resp_end_html();
}

static void
catman(const struct req *req, const char *file)
{
	FILE		*f;
	size_t		 len;
	int		 i;
	char		*p;
	int		 italic, bold;

	if (NULL == (f = fopen(file, "r"))) {
		resp_error_badrequest(
		    "You specified an invalid manual file.");
		return;
	}

	resp_begin_html(200, NULL);
	resp_searchform(req);
	puts("<DIV CLASS=\"catman\">\n"
	     "<PRE>");

	while (NULL != (p = fgetln(f, &len))) {
		bold = italic = 0;
		for (i = 0; i < (int)len - 1; i++) {
			/* 
			 * This means that the catpage is out of state.
			 * Ignore it and keep going (although the
			 * catpage is bogus).
			 */

			if ('\b' == p[i] || '\n' == p[i])
				continue;

			/*
			 * Print a regular character.
			 * Close out any bold/italic scopes.
			 * If we're in back-space mode, make sure we'll
			 * have something to enter when we backspace.
			 */

			if ('\b' != p[i + 1]) {
				if (italic)
					printf("</I>");
				if (bold)
					printf("</B>");
				italic = bold = 0;
				html_putchar(p[i]);
				continue;
			} else if (i + 2 >= (int)len)
				continue;

			/* Italic mode. */

			if ('_' == p[i]) {
				if (bold)
					printf("</B>");
				if ( ! italic)
					printf("<I>");
				bold = 0;
				italic = 1;
				i += 2;
				html_putchar(p[i]);
				continue;
			}

			/* 
			 * Handle funny behaviour troff-isms.
			 * These grok'd from the original man2html.c.
			 */

			if (('+' == p[i] && 'o' == p[i + 2]) ||
					('o' == p[i] && '+' == p[i + 2]) ||
					('|' == p[i] && '=' == p[i + 2]) ||
					('=' == p[i] && '|' == p[i + 2]) ||
					('*' == p[i] && '=' == p[i + 2]) ||
					('=' == p[i] && '*' == p[i + 2]) ||
					('*' == p[i] && '|' == p[i + 2]) ||
					('|' == p[i] && '*' == p[i + 2]))  {
				if (italic)
					printf("</I>");
				if (bold)
					printf("</B>");
				italic = bold = 0;
				putchar('*');
				i += 2;
				continue;
			} else if (('|' == p[i] && '-' == p[i + 2]) ||
					('-' == p[i] && '|' == p[i + 1]) ||
					('+' == p[i] && '-' == p[i + 1]) ||
					('-' == p[i] && '+' == p[i + 1]) ||
					('+' == p[i] && '|' == p[i + 1]) ||
					('|' == p[i] && '+' == p[i + 1]))  {
				if (italic)
					printf("</I>");
				if (bold)
					printf("</B>");
				italic = bold = 0;
				putchar('+');
				i += 2;
				continue;
			}

			/* Bold mode. */
			
			if (italic)
				printf("</I>");
			if ( ! bold)
				printf("<B>");
			bold = 1;
			italic = 0;
			i += 2;
			html_putchar(p[i]);
		}

		/* 
		 * Clean up the last character.
		 * We can get to a newline; don't print that. 
		 */

		if (italic)
			printf("</I>");
		if (bold)
			printf("</B>");

		if (i == (int)len - 1 && '\n' != p[i])
			html_putchar(p[i]);

		putchar('\n');
	}

	puts("</PRE>\n"
	     "</DIV>\n"
	     "</BODY>\n"
	     "</HTML>");

	fclose(f);
}

static void
format(const struct req *req, const char *file)
{
	struct mparse	*mp;
	int		 fd;
	struct mdoc	*mdoc;
	struct man	*man;
	void		*vp;
	enum mandoclevel rc;
	char		 opts[PATH_MAX + 128];

	if (-1 == (fd = open(file, O_RDONLY, 0))) {
		resp_error_badrequest(
		    "You specified an invalid manual file.");
		return;
	}

	mp = mparse_alloc(MPARSE_SO, MANDOCLEVEL_FATAL, NULL,
	    req->q.manpath);
	rc = mparse_readfd(mp, fd, file);
	close(fd);

	if (rc >= MANDOCLEVEL_FATAL) {
		fprintf(stderr, "fatal mandoc error: %s/%s\n",
		    req->q.manpath, file);
		resp_error_internal();
		return;
	}

	snprintf(opts, sizeof(opts),
	    "fragment,man=%s?query=%%N&amp;sec=%%S",
	    scriptname);

	mparse_result(mp, &mdoc, &man, NULL);
	if (NULL == man && NULL == mdoc) {
		fprintf(stderr, "fatal mandoc error: %s/%s\n",
		    req->q.manpath, file);
		resp_error_internal();
		mparse_free(mp);
		return;
	}

	resp_begin_html(200, NULL);
	resp_searchform(req);

	vp = html_alloc(opts);

	if (NULL != mdoc)
		html_mdoc(vp, mdoc);
	else
		html_man(vp, man);

	puts("</BODY>\n"
	     "</HTML>");

	html_free(vp);
	mparse_free(mp);
}

static void
pg_show(const struct req *req, const char *path)
{
	char		*sub;

	if (NULL == path || NULL == (sub = strchr(path, '/'))) {
		resp_error_badrequest(
		    "You did not specify a page to show.");
		return;
	} 
	*sub++ = '\0';

	/*
	 * Begin by chdir()ing into the manpath.
	 * This way we can pick up the database files, which are
	 * relative to the manpath root.
	 */

	if (-1 == chdir(path)) {
		resp_error_badrequest(
		    "You specified an invalid manpath.");
		return;
	}

	if ('c' == *sub)
		catman(req, sub);
	else
		format(req, sub);
}

static void
pg_search(const struct req *req)
{
	struct mansearch	  search;
	struct manpaths		  paths;
	struct manpage		 *res;
	char			**cp;
	const char		 *ep, *start;
	size_t			  ressz;
	int			  i, sz;

	/*
	 * Begin by chdir()ing into the root of the manpath.
	 * This way we can pick up the database files, which are
	 * relative to the manpath root.
	 */

	if (-1 == (chdir(req->q.manpath))) {
		resp_error_badrequest(
		    "You specified an invalid manpath.");
		return;
	}

	search.arch = req->q.arch;
	search.sec = req->q.sec;
	search.deftype = req->q.equal ? TYPE_Nm : (TYPE_Nm | TYPE_Nd);
	search.flags = req->q.equal ? MANSEARCH_MAN : 0;

	paths.sz = 1;
	paths.paths = mandoc_malloc(sizeof(char *));
	paths.paths[0] = mandoc_strdup(".");

	/*
	 * Poor man's tokenisation: just break apart by spaces.
	 * Yes, this is half-ass.  But it works for now.
	 */

	ep = req->q.expr;
	while (ep && isspace((unsigned char)*ep))
		ep++;

	sz = 0;
	cp = NULL;
	while (ep && '\0' != *ep) {
		cp = mandoc_reallocarray(cp, sz + 1, sizeof(char *));
		start = ep;
		while ('\0' != *ep && ! isspace((unsigned char)*ep))
			ep++;
		cp[sz] = mandoc_malloc((ep - start) + 1);
		memcpy(cp[sz], start, ep - start);
		cp[sz++][ep - start] = '\0';
		while (isspace((unsigned char)*ep))
			ep++;
	}

	if (0 == mansearch(&search, &paths, sz, cp, "Nd", &res, &ressz))
		resp_noresult(req, "You entered an invalid query.");
	else if (0 == ressz)
		resp_noresult(req, "No results found.");
	else
		resp_search(req, res, ressz);

	for (i = 0; i < sz; i++)
		free(cp[i]);
	free(cp);

	for (i = 0; i < (int)ressz; i++) {
		free(res[i].file);
		free(res[i].names);
		free(res[i].output);
	}
	free(res);

	free(paths.paths[0]);
	free(paths.paths);
}

int
main(void)
{
	struct req	 req;
	const char	*path;
	char		*querystring;
	int		 i;

	/* Scan our run-time environment. */

	if (NULL == (scriptname = getenv("SCRIPT_NAME")))
		scriptname = "";

	if (NULL == (httphost = getenv("HTTP_HOST")))
		httphost = "localhost";

	/*
	 * First we change directory into the MAN_DIR so that
	 * subsequent scanning for manpath directories is rooted
	 * relative to the same position.
	 */

	if (-1 == chdir(MAN_DIR)) {
		fprintf(stderr, "MAN_DIR: %s: %s\n",
		    MAN_DIR, strerror(errno));
		resp_error_internal();
		return(EXIT_FAILURE);
	} 

	memset(&req, 0, sizeof(struct req));
	pathgen(&req);

	/* Next parse out the query string. */

	if (NULL != (querystring = getenv("QUERY_STRING")))
		http_parse(&req, querystring);

	/* Dispatch to the three different pages. */

	path = getenv("PATH_INFO");
	if (NULL == path)
		path = "";
	else if ('/' == *path)
		path++;

	if ('\0' != *path)
		pg_show(&req, path);
	else if (NULL != req.q.expr)
		pg_search(&req);
	else
		resp_index(&req);

	for (i = 0; i < (int)req.psz; i++)
		free(req.p[i]);
	free(req.p);
	return(EXIT_SUCCESS);
}

static int
cmp(const void *p1, const void *p2)
{

	return(strcasecmp(((const struct manpage *)p1)->names,
	    ((const struct manpage *)p2)->names));
}

/*
 * Scan for indexable paths.
 */
static void
pathgen(struct req *req)
{
	FILE	*fp;
	char	*dp;
	size_t	 dpsz;

	if (NULL == (fp = fopen("manpath.conf", "r")))
		return;

	while (NULL != (dp = fgetln(fp, &dpsz))) {
		if ('\n' == dp[dpsz - 1])
			dpsz--;
		req->p = mandoc_realloc(req->p,
		    (req->psz + 1) * sizeof(char *));
		req->p[req->psz++] = mandoc_strndup(dp, dpsz);
	}
}
