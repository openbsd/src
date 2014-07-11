/*	$Id: cgi.c,v 1.1 2014/07/11 15:37:22 schwarze Exp $ */
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

enum	page {
	PAGE_INDEX,
	PAGE_SEARCH,
	PAGE_SHOW,
	PAGE__MAX
};

/*
 * A query as passed to the search function.
 */
struct	query {
	const char	*manpath; /* desired manual directory */
	const char	*arch; /* architecture */
	const char	*sec; /* manual section */
	const char	*expr; /* unparsed expression string */
	int		 legacy; /* whether legacy mode */
};

struct	req {
	struct query	  q;
	char		**p; /* array of available manpaths */
	size_t		  psz; /* number of available manpaths */
	enum page	  page;
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
static	void		 pg_index(const struct req *, char *);
static	void		 pg_search(const struct req *, char *);
static	void		 pg_show(const struct req *, char *);
static	void		 resp_begin_html(int, const char *);
static	void		 resp_begin_http(int, const char *);
static	void		 resp_end_html(void);
static	void		 resp_error_badrequest(const char *);
static	void		 resp_error_internal(void);
static	void		 resp_error_notfound(const char *);
static	void		 resp_index(const struct req *);
static	void		 resp_noresult(const struct req *,
				const char *);
static	void		 resp_search(const struct req *,
				struct manpage *, size_t);
static	void		 resp_searchform(const struct req *);

static	const char	 *scriptname; /* CGI script name */
static	const char	 *mandir; /* contains all manpath directories */
static	const char	 *cssdir; /* css directory */
static	const char	 *httphost; /* hostname used in the URIs */

static	const char * const pages[PAGE__MAX] = {
	"index", /* PAGE_INDEX */ 
	"search", /* PAGE_SEARCH */
	"show", /* PAGE_SHOW */
};

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
		printf("&expr=");
		http_print(req->q.expr ? req->q.expr : "");
	}
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
		printf("&amp;expr=");
		html_print(req->q.expr ? req->q.expr : "");
	}
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
	int		 legacy;

	memset(&req->q, 0, sizeof(struct query));
	req->q.manpath = req->p[0];

	legacy = -1;
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

		if (0 == strcmp(key, "expr"))
			req->q.expr = val;
		else if (0 == strcmp(key, "query"))
			req->q.expr = val;
		else if (0 == strcmp(key, "sec"))
			req->q.sec = val;
		else if (0 == strcmp(key, "sektion"))
			req->q.sec = val;
		else if (0 == strcmp(key, "arch"))
			req->q.arch = val;
		else if (0 == strcmp(key, "manpath"))
			req->q.manpath = val;
		else if (0 == strcmp(key, "apropos"))
			legacy = 0 == strcmp(val, "0");
	}

	/* Test for old man.cgi compatibility mode. */

	req->q.legacy = legacy > 0;

	/* 
	 * Section "0" means no section when in legacy mode.
	 * For some man.cgi scripts, "default" arch is none.
	 */

	if (req->q.legacy && NULL != req->q.sec)
		if (0 == strcmp(req->q.sec, "0"))
			req->q.sec = NULL;
	if (req->q.legacy && NULL != req->q.arch)
		if (0 == strcmp(req->q.arch, "default"))
			req->q.arch = NULL;
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
	int              c;

	hex[2] = '\0';

	for ( ; '\0' != *p; p++) {
		if ('%' == *p) {
			if ('\0' == (hex[0] = *(p + 1)))
				return(0);
			if ('\0' == (hex[1] = *(p + 2)))
				return(0);
			if (1 != sscanf(hex, "%x", &c))
				return(0);
			if ('\0' == c)
				return(0);

			*p = (char)c;
			memmove(p + 1, p + 3, strlen(p + 3) + 1);
		} else
			*p = '+' == *p ? ' ' : *p;
	}

	*p = '\0';
	return(1);
}

static void
resp_begin_http(int code, const char *msg)
{

	if (200 != code)
		printf("Status: %d %s\n", code, msg);

	puts("Content-Type: text/html; charset=utf-8\n"
	     "Cache-Control: no-cache\n"
	     "Pragma: no-cache\n"
	     "");

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
	       "<TITLE>System Manpage Reference</TITLE>\n"
	       "</HEAD>\n"
	       "<BODY>\n"
	       "<!-- Begin page content. //-->\n",
	       cssdir, cssdir);
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

	puts("<!-- Begin search form. //-->");
	printf("<DIV ID=\"mancgi\">\n"
	       "<FORM ACTION=\"%s/search\" METHOD=\"get\">\n"
	       "<FIELDSET>\n"
	       "<LEGEND>Search Parameters</LEGEND>\n"
	       "<INPUT TYPE=\"submit\" "
	       " VALUE=\"Search\"> for manuals matching \n"
	       "<INPUT TYPE=\"text\" NAME=\"expr\" VALUE=\"",
	       scriptname);
	html_print(req->q.expr ? req->q.expr : "");
	printf("\">, section "
	       "<INPUT TYPE=\"text\""
	       " SIZE=\"4\" NAME=\"sec\" VALUE=\"");
	html_print(req->q.sec ? req->q.sec : "");
	printf("\">, arch "
	       "<INPUT TYPE=\"text\""
	       " SIZE=\"8\" NAME=\"arch\" VALUE=\"");
	html_print(req->q.arch ? req->q.arch : "");
	printf("\">");
	if (req->psz > 1) {
		puts(", in <SELECT NAME=\"manpath\">");
		for (i = 0; i < (int)req->psz; i++) {
			printf("<OPTION ");
			if (NULL == req->q.manpath ? 0 == i :
			    0 == strcmp(req->q.manpath, req->p[i]))
				printf("SELECTED=\"selected\" ");
			printf("VALUE=\"");
			html_print(req->p[i]);
			printf("\">");
			html_print(req->p[i]);
			puts("</OPTION>");
		}
		puts("</SELECT>");
	}
	puts("&mdash;\n"
	     "<INPUT TYPE=\"reset\" VALUE=\"Reset\">\n"
	     "</FIELDSET>\n"
	     "</FORM>\n"
	     "</DIV>");
	puts("<!-- End search form. //-->");
}

static void
resp_index(const struct req *req)
{

	resp_begin_html(200, NULL);
	puts("<H1>\n"
	     "Online manuals with "
	     "<A HREF=\"http://mdocml.bsd.lv/\">mandoc</A>\n"
	     "</H1>");
	resp_searchform(req);
	puts("<P>\n"
	     "This web interface is documented in the "
	     "<A HREF=\"search?expr=Nm~^man\\.cgi$&amp;sec=8\">"
	     "man.cgi</A> manual, and the "
	     "<A HREF=\"search?expr=Nm~^apropos$&amp;sec=1\">"
	     "apropos</A> manual explains the query syntax.\n"
	     "</P>");
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
resp_error_notfound(const char *page)
{

	resp_begin_html(404, "Not Found");
	puts("<H1>Page Not Found</H1>\n"
	     "<P>\n"
	     "The page you're looking for, ");
	printf("<B>");
	html_print(page);
	printf("</B>,\n"
	       "could not be found.\n"
	       "Try searching from the\n"
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
		puts("Status: 303 See Other");
		printf("Location: http://%s%s/show/%s/%s?",
		    httphost, scriptname, req->q.manpath, r[0].file);
		http_printquery(req);
		puts("\n"
		     "Content-Type: text/html; charset=utf-8\n");
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
		       "<A HREF=\"%s/show/%s/%s?", 
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

/* ARGSUSED */
static void
pg_index(const struct req *req, char *path)
{

	resp_index(req);
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
	    "fragment,man=%s/search?sec=%%S&expr=Nm~^%%N$",
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
pg_show(const struct req *req, char *path)
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
pg_search(const struct req *req, char *path)
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
	search.deftype = TYPE_Nm | TYPE_Nd;
	search.flags = 0;

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
	int		 i;
	struct req	 req;
	char		*querystring, *path, *subpath;

	/* Scan our run-time environment. */

	if (NULL == (mandir = getenv("MAN_DIR")))
		mandir = "/man";

	if (NULL == (scriptname = getenv("SCRIPT_NAME")))
		scriptname = "";

	if (NULL == (cssdir = getenv("CSS_DIR")))
		cssdir = "";

	if (NULL == (httphost = getenv("HTTP_HOST")))
		httphost = "localhost";

	/*
	 * First we change directory into the mandir so that
	 * subsequent scanning for manpath directories is rooted
	 * relative to the same position.
	 */

	if (-1 == chdir(mandir)) {
		fprintf(stderr, "MAN_DIR: %s: %s\n",
		    mandir, strerror(errno));
		resp_error_internal();
		return(EXIT_FAILURE);
	} 

	memset(&req, 0, sizeof(struct req));
	pathgen(&req);

	/* Next parse out the query string. */

	if (NULL != (querystring = getenv("QUERY_STRING")))
		http_parse(&req, querystring);

	/*
	 * Now juggle paths to extract information.
	 * We want to extract our filetype (the file suffix), the
	 * initial path component, then the trailing component(s).
	 * Start with leading subpath component. 
	 */

	subpath = path = NULL;
	req.page = PAGE__MAX;

	if (NULL == (path = getenv("PATH_INFO")) || '\0' == *path)
		req.page = PAGE_INDEX;

	if (NULL != path && '/' == *path && '\0' == *++path)
		req.page = PAGE_INDEX;

	/* Resolve subpath component. */

	if (NULL != path && NULL != (subpath = strchr(path, '/')))
		*subpath++ = '\0';

	/* Map path into one we recognise. */

	if (NULL != path && '\0' != *path)
		for (i = 0; i < (int)PAGE__MAX; i++) 
			if (0 == strcmp(pages[i], path)) {
				req.page = (enum page)i;
				break;
			}

	/* Route pages. */

	switch (req.page) {
	case (PAGE_INDEX):
		pg_index(&req, subpath);
		break;
	case (PAGE_SEARCH):
		pg_search(&req, subpath);
		break;
	case (PAGE_SHOW):
		pg_show(&req, subpath);
		break;
	default:
		resp_error_notfound(path);
		break;
	}

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
