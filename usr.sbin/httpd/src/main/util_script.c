/*	$OpenBSD: util_script.c,v 1.18 2008/05/15 06:05:43 mbalmer Exp $ */

/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

#define CORE_PRIVATE
#include "httpd.h"
#include "http_config.h"
#include "http_conf_globals.h"
#include "http_main.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_core.h"		/* For document_root.  Sigh... */
#include "http_request.h"	/* for sub_req_lookup_uri() */
#include "util_script.h"
#include "util_date.h"		/* For parseHTTPdate() */


/*
 * Various utility functions which are common to a whole lot of
 * script-type extensions mechanisms, and might as well be gathered
 * in one place (if only to avoid creating inter-module dependancies
 * where there don't have to be).
 */

#define MALFORMED_MESSAGE "malformed header from script. Bad header="
#define MALFORMED_HEADER_LENGTH_TO_SHOW 30

/* If a request includes query info in the URL (stuff after "?"), and
 * the query info does not contain "=" (indicative of a FORM submission),
 * then this routine is called to create the argument list to be passed
 * to the CGI script.  When suexec is enabled, the suexec path, user, and
 * group are the first three arguments to be passed; if not, all three
 * must be NULL.  The query info is split into separate arguments, where
 * "+" is the separator between keyword arguments.
 */
static char **create_argv(pool *p, char *path, char *user, char *group,
			  char *av0, const char *args)
{
    int x, numwords;
    char **av;
    char *w;
    int idx = 0;

    /* count the number of keywords */

    for (x = 0, numwords = 1; args[x]; x++) {
        if (args[x] == '+') {
	    ++numwords;
	}
    }

    if (numwords > APACHE_ARG_MAX - 5) {
	numwords = APACHE_ARG_MAX - 5;	/* Truncate args to prevent overrun */
    }
    av = (char **) ap_palloc(p, (numwords + 5) * sizeof(char *));

    if (path) {
	av[idx++] = path;
    }
    if (user) {
	av[idx++] = user;
    }
    if (group) {
	av[idx++] = group;
    }

    av[idx++] = av0;

    for (x = 1; x <= numwords; x++) {
	w = ap_getword_nulls(p, &args, '+');
	ap_unescape_url(w);
	av[idx++] = ap_escape_shell_cmd(p, w);
    }
    av[idx] = NULL;
    return av;
}


static char *http2env(pool *a, char *w)
{
    char *res = ap_pstrcat(a, "HTTP_", w, NULL);
    char *cp = res;

    while (*++cp) {
	if (!ap_isalnum(*cp) && *cp != '_') {
	    *cp = '_';
	}
	else {
	    *cp = ap_toupper(*cp);
	}
    }

    return res;
}

API_EXPORT(char **) ap_create_environment(pool *p, table *t)
{
    array_header *env_arr = ap_table_elts(t);
    table_entry *elts = (table_entry *) env_arr->elts;
    char **env = (char **) ap_palloc(p, (env_arr->nelts + 2) * sizeof(char *));
    int i, j;
    char *tz;
    char *whack;

    j = 0;
    if (!ap_table_get(t, "TZ")) {
	tz = getenv("TZ");
	if (tz != NULL) {
	    env[j++] = ap_pstrcat(p, "TZ=", tz, NULL);
	}
    }
    for (i = 0; i < env_arr->nelts; ++i) {
        if (!elts[i].key) {
	    continue;
	}
	env[j] = ap_pstrcat(p, elts[i].key, "=", elts[i].val, NULL);
	whack = env[j];
	if (ap_isdigit(*whack)) {
	    *whack++ = '_';
	}
	while (*whack != '=') {
	    if (!ap_isalnum(*whack) && *whack != '_') {
		*whack = '_';
	    }
	    ++whack;
	}
	++j;
    }

    env[j] = NULL;
    return env;
}

API_EXPORT(void) ap_add_common_vars(request_rec *r)
{
    table *e;
    server_rec *s = r->server;
    conn_rec *c = r->connection;
    const char *rem_logname;
    char *env_path;
    const char *host;
    array_header *hdrs_arr = ap_table_elts(r->headers_in);
    table_entry *hdrs = (table_entry *) hdrs_arr->elts;
    int i;
    char servbuf[NI_MAXSERV];

    /* use a temporary table which we'll overlap onto
     * r->subprocess_env later
     */
    e = ap_make_table(r->pool, 25 + hdrs_arr->nelts);

    /* First, add environment vars from headers... this is as per
     * CGI specs, though other sorts of scripting interfaces see
     * the same vars...
     */

    for (i = 0; i < hdrs_arr->nelts; ++i) {
        if (!hdrs[i].key) {
	    continue;
	}

	/* A few headers are special cased --- Authorization to prevent
	 * rogue scripts from capturing passwords; content-type and -length
	 * for no particular reason.
	 */

	if (!strcasecmp(hdrs[i].key, "Content-type")) {
	    ap_table_addn(e, "CONTENT_TYPE", hdrs[i].val);
	}
	else if (!strcasecmp(hdrs[i].key, "Content-length")) {
	    ap_table_addn(e, "CONTENT_LENGTH", hdrs[i].val);
	}
	/*
	 * You really don't want to disable this check, since it leaves you
	 * wide open to CGIs stealing passwords and people viewing them
	 * in the environment with "ps -e".  But, if you must...
	 */
	else if (!strcasecmp(hdrs[i].key, "Authorization") 
		 || !strcasecmp(hdrs[i].key, "Proxy-Authorization")) {
	    continue;
	}
	else {
	    ap_table_addn(e, http2env(r->pool, hdrs[i].key), hdrs[i].val);
	}
    }

    if (!(env_path = ap_pstrdup(r->pool, getenv("PATH")))) {
	env_path = DEFAULT_PATH;
    }

    ap_table_addn(e, "PATH", env_path);
    ap_table_addn(e, "SERVER_SIGNATURE", ap_psignature("", r));
    ap_table_addn(e, "SERVER_SOFTWARE", ap_get_server_version());
    ap_table_addn(e, "SERVER_NAME", 
		  ap_escape_html(r->pool,ap_get_server_name(r)));
    ap_table_addn(e, "SERVER_ADDR", r->connection->local_ip);	/* Apache */
    ap_table_addn(e, "SERVER_PORT",
		  ap_psprintf(r->pool, "%u", ap_get_server_port(r)));
    host = ap_get_remote_host(c, r->per_dir_config, REMOTE_HOST);
    if (host) {
	ap_table_addn(e, "REMOTE_HOST", host);
    }
    ap_table_addn(e, "REMOTE_ADDR", c->remote_ip);
    ap_table_addn(e, "DOCUMENT_ROOT", ap_document_root(r));	/* Apache */
    ap_table_addn(e, "SERVER_ADMIN", s->server_admin);	/* Apache */
    ap_table_addn(e, "SCRIPT_FILENAME", r->filename);	/* Apache */

    servbuf[0] = '\0';
    if (!getnameinfo((struct sockaddr *)&c->remote_addr,
#ifndef HAVE_SOCKADDR_LEN
		     SA_LEN((struct sockaddr *)&c->remote_addr),
#else
		     c->remote_addr.ss_len,
#endif
		     NULL, 0, servbuf, sizeof(servbuf), NI_NUMERICSERV)){
	ap_table_addn(e, "REMOTE_PORT", ap_pstrdup(r->pool, servbuf));
    }

    if (c->user) {
	ap_table_addn(e, "REMOTE_USER", c->user);
    }
    if (c->ap_auth_type) {
	ap_table_addn(e, "AUTH_TYPE", c->ap_auth_type);
    }
    rem_logname = ap_get_remote_logname(r);
    if (rem_logname) {
	ap_table_addn(e, "REMOTE_IDENT", ap_pstrdup(r->pool, rem_logname));
    }

    /* Apache custom error responses. If we have redirected set two new vars */

    if (r->prev) {
        if (r->prev->args) {
	    ap_table_addn(e, "REDIRECT_QUERY_STRING", r->prev->args);
	}
	if (r->prev->uri) {
	    ap_table_addn(e, "REDIRECT_URL", r->prev->uri);
	}
    }

    ap_overlap_tables(r->subprocess_env, e, AP_OVERLAP_TABLES_SET);
}

/* This "cute" little function comes about because the path info on
 * filenames and URLs aren't always the same. So we take the two,
 * and find as much of the two that match as possible.
 */

API_EXPORT(int) ap_find_path_info(const char *uri, const char *path_info)
{
    int lu = strlen(uri);
    int lp = strlen(path_info);

    while (lu-- && lp-- && uri[lu] == path_info[lp]);

    if (lu == -1) {
	lu = 0;
    }

    while (uri[lu] != '\0' && uri[lu] != '/') {
        lu++;
    }
    return lu;
}

/* Obtain the Request-URI from the original request-line, returning
 * a new string from the request pool containing the URI or "".
 */
static char *original_uri(request_rec *r)
{
    char *first, *last;

    if (r->the_request == NULL) {
	return (char *) ap_pcalloc(r->pool, 1);
    }

    first = r->the_request;	/* use the request-line */

    while (*first && !ap_isspace(*first)) {
	++first;		/* skip over the method */
    }
    while (ap_isspace(*first)) {
	++first;		/*   and the space(s)   */
    }

    last = first;
    while (*last && !ap_isspace(*last)) {
	++last;			/* end at next whitespace */
    }

    return ap_pstrndup(r->pool, first, last - first);
}

API_EXPORT(void) ap_add_cgi_vars(request_rec *r)
{
    table *e = r->subprocess_env;

    ap_table_setn(e, "GATEWAY_INTERFACE", "CGI/1.1");
    ap_table_setn(e, "SERVER_PROTOCOL", r->protocol);
    ap_table_setn(e, "REQUEST_METHOD", r->method);
    ap_table_setn(e, "QUERY_STRING", r->args ? r->args : "");
    ap_table_setn(e, "REQUEST_URI", original_uri(r));

    /* Note that the code below special-cases scripts run from includes,
     * because it "knows" that the sub_request has been hacked to have the
     * args and path_info of the original request, and not any that may have
     * come with the script URI in the include command.  Ugh.
     */

    if (!strcmp(r->protocol, "INCLUDED")) {
	ap_table_setn(e, "SCRIPT_NAME", r->uri);
	if (r->path_info && *r->path_info) {
	    ap_table_setn(e, "PATH_INFO", r->path_info);
	}
    }
    else if (!r->path_info || !*r->path_info) {
	ap_table_setn(e, "SCRIPT_NAME", r->uri);
    }
    else {
	int path_info_start = ap_find_path_info(r->uri, r->path_info);

	ap_table_setn(e, "SCRIPT_NAME",
		      ap_pstrndup(r->pool, r->uri, path_info_start));

	ap_table_setn(e, "PATH_INFO", r->path_info);
    }

    if (r->path_info && r->path_info[0]) {
	/*
	 * To get PATH_TRANSLATED, treat PATH_INFO as a URI path.
	 * Need to re-escape it for this, since the entire URI was
	 * un-escaped before we determined where the PATH_INFO began.
	 */
	request_rec *pa_req;

	pa_req = ap_sub_req_lookup_uri(ap_escape_uri(r->pool, r->path_info), r);

	if (pa_req->filename) {
	    char *pt = ap_pstrcat(r->pool, pa_req->filename, pa_req->path_info,
				  NULL);
	    ap_table_setn(e, "PATH_TRANSLATED", pt);
	}
	ap_destroy_sub_req(pa_req);
    }
}


static int set_cookie_doo_doo(void *v, const char *key, const char *val)
{
    ap_table_addn(v, key, val);
    return 1;
}

API_EXPORT(int) ap_scan_script_header_err_core(request_rec *r, char *buffer,
				       int (*getsfunc) (char *, int, void *),
				       void *getsfunc_data)
{
    char x[MAX_STRING_LEN];
    char *w, *l;
    int p;
    int cgi_status = HTTP_OK;
    table *merge;
    table *cookie_table;

    if (buffer) {
	*buffer = '\0';
    }
    w = buffer ? buffer : x;

    ap_hard_timeout("read script header", r);

    /* temporary place to hold headers to merge in later */
    merge = ap_make_table(r->pool, 10);

    /* The HTTP specification says that it is legal to merge duplicate
     * headers into one.  Some browsers that support Cookies don't like
     * merged headers and prefer that each Set-Cookie header is sent
     * separately.  Lets humour those browsers by not merging.
     * Oh what a pain it is.
     */
    cookie_table = ap_make_table(r->pool, 2);
    ap_table_do(set_cookie_doo_doo, cookie_table, r->err_headers_out, "Set-Cookie", NULL);

    while (1) {

	if ((*getsfunc) (w, MAX_STRING_LEN - 1, getsfunc_data) == 0) {
	    ap_kill_timeout(r);
	    ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
			  "Premature end of script headers: %s", r->filename);
	    return HTTP_INTERNAL_SERVER_ERROR;
	}

	/* Delete terminal (CR?)LF */

	p = strlen(w);
        /* Indeed, the host's '\n':
           '\012' for UNIX; '\015' for MacOS; '\025' for OS/390
           -- whatever the script generates.
        */
	if (p > 0 && w[p - 1] == '\n') {
	    if (p > 1 && w[p - 2] == CR) {
		w[p - 2] = '\0';
	    }
	    else {
		w[p - 1] = '\0';
	    }
	}

	/*
	 * If we've finished reading the headers, check to make sure any
	 * HTTP/1.1 conditions are met.  If so, we're done; normal processing
	 * will handle the script's output.  If not, just return the error.
	 * The appropriate thing to do would be to send the script process a
	 * SIGPIPE to let it know we're ignoring it, close the channel to the
	 * script process, and *then* return the failed-to-meet-condition
	 * error.  Otherwise we'd be waiting for the script to finish
	 * blithering before telling the client the output was no good.
	 * However, we don't have the information to do that, so we have to
	 * leave it to an upper layer.
	 */
	if (w[0] == '\0') {
	    int cond_status = OK;

	    ap_kill_timeout(r);
	    if ((cgi_status == HTTP_OK) && (r->method_number == M_GET)) {
		cond_status = ap_meets_conditions(r);
	    }
	    ap_overlap_tables(r->err_headers_out, merge,
		AP_OVERLAP_TABLES_MERGE);
	    if (!ap_is_empty_table(cookie_table)) {
		/* the cookies have already been copied to the cookie_table */
		ap_table_unset(r->err_headers_out, "Set-Cookie");
		r->err_headers_out = ap_overlay_tables(r->pool,
		    r->err_headers_out, cookie_table);
	    }
	    return cond_status;
	}

	/* if we see a bogus header don't ignore it. Shout and scream */

	if (!(l = strchr(w, ':'))) {
	    char malformed[(sizeof MALFORMED_MESSAGE) + 1
			   + MALFORMED_HEADER_LENGTH_TO_SHOW];

	    strlcpy(malformed, MALFORMED_MESSAGE, sizeof(malformed));
	    strncat(malformed, w, MALFORMED_HEADER_LENGTH_TO_SHOW);

	    if (!buffer) {
		/* Soak up all the script output - may save an outright kill */
	        while ((*getsfunc) (w, MAX_STRING_LEN - 1, getsfunc_data)) {
		    continue;
		}
	    }

	    ap_kill_timeout(r);
	    ap_log_rerror(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, r,
			  "%s: %s", malformed, r->filename);
	    return HTTP_INTERNAL_SERVER_ERROR;
	}

	*l++ = '\0';
	while (ap_isspace(*l)) {
	    ++l;
	}

	if (!strcasecmp(w, "Content-type")) {
	    char *tmp;

	    /* Nuke trailing whitespace */

	    char *endp = l + strlen(l) - 1;
	    while (endp > l && ap_isspace(*endp)) {
		*endp-- = '\0';
	    }

	    tmp = ap_pstrdup(r->pool, l);
	    ap_content_type_tolower(tmp);
	    r->content_type = tmp;
	}
	/*
	 * If the script returned a specific status, that's what
	 * we'll use - otherwise we assume 200 OK.
	 */
	else if (!strcasecmp(w, "Status")) {
	    r->status = cgi_status = atoi(l);
	    r->status_line = ap_pstrdup(r->pool, l);
	}
	else if (!strcasecmp(w, "Location")) {
	    ap_table_set(r->headers_out, w, l);
	}
	else if (!strcasecmp(w, "Content-Length")) {
	    ap_table_set(r->headers_out, w, l);
	}
	else if (!strcasecmp(w, "Transfer-Encoding")) {
	    ap_table_set(r->headers_out, w, l);
	}
	/*
	 * If the script gave us a Last-Modified header, we can't just
	 * pass it on blindly because of restrictions on future values.
	 */
	else if (!strcasecmp(w, "Last-Modified")) {
	    time_t mtime = ap_parseHTTPdate(l);

	    ap_update_mtime(r, mtime);
	    ap_set_last_modified(r);
	}
	else if (!strcasecmp(w, "Set-Cookie")) {
	    ap_table_add(cookie_table, w, l);
	}
	else {
	    ap_table_add(merge, w, l);
	}
    }
}

static int getsfunc_FILE(char *buf, int len, void *f)
{
    return fgets(buf, len, (FILE *) f) != NULL;
}

API_EXPORT(int) ap_scan_script_header_err(request_rec *r, FILE *f,
					  char *buffer)
{
    return ap_scan_script_header_err_core(r, buffer, getsfunc_FILE, f);
}

static int getsfunc_BUFF(char *w, int len, void *fb)
{
    return ap_bgets(w, len, (BUFF *) fb) > 0;
}

API_EXPORT(int) ap_scan_script_header_err_buff(request_rec *r, BUFF *fb,
					       char *buffer)
{
    return ap_scan_script_header_err_core(r, buffer, getsfunc_BUFF, fb);
}

struct vastrs {
    va_list args;
    int arg;
    const char *curpos;
};

static int getsfunc_STRING(char *w, int len, void *pvastrs)
{
    struct vastrs *strs = (struct vastrs*) pvastrs;
    char *p;
    int t;
    
    if (!strs->curpos || !*strs->curpos) 
        return 0;
    p = strchr(strs->curpos, '\n');
    if (p)
        ++p;
    else
        p = strchr(strs->curpos, '\0');
    t = p - strs->curpos;
    if (t > len)
        t = len;
    strncpy (w, strs->curpos, t);
    w[t] = '\0';
    if (!strs->curpos[t]) {
        ++strs->arg;
        strs->curpos = va_arg(strs->args, const char *);
    }
    else
        strs->curpos += t;
    return t;    
}

/* ap_scan_script_header_err_strs() accepts additional const char* args...
 * each is treated as one or more header lines, and the first non-header
 * character is returned to **arg, **data.  (The first optional arg is
 * counted as 0.)
 */
API_EXPORT_NONSTD(int) ap_scan_script_header_err_strs(request_rec *r, 
                                                      char *buffer, 
                                                      const char **termch,
                                                      int *termarg, ...)
{
    struct vastrs strs;
    int res;

    va_start(strs.args, termarg);
    strs.arg = 0;
    strs.curpos = va_arg(strs.args, char*);
    res = ap_scan_script_header_err_core(r, buffer, getsfunc_STRING, (void *) &strs);
    if (termch)
        *termch = strs.curpos;
    if (termarg)
        *termarg = strs.arg;
    va_end(strs.args);
    return res;
}

API_EXPORT(void) ap_send_size(size_t size, request_rec *r)
{
    /* XXX: this -1 thing is a gross hack */
    if (size == (size_t)-1) {
	ap_rputs("    -", r);
    }
    else if (!size) {
	ap_rputs("   0k", r);
    }
    else if (size < 1024) {
	ap_rputs("   1k", r);
    }
    else if (size < 1048576) {
	ap_rprintf(r, "%4dk", (int)((size + 512) / 1024));
    }
    else if (size < 103809024) {
	ap_rprintf(r, "%4.1fM", size / 1048576.0);
    }
    else {
	ap_rprintf(r, "%4dM", (int)((size + 524288) / 1048576));
    }
}

API_EXPORT(int) ap_call_exec(request_rec *r, child_info *pinfo, char *argv0,
			     char **env, int shellcmd)
{
    int pid = 0;
    core_dir_config *conf;
    conf = (core_dir_config *) ap_get_module_config(r->per_dir_config,
						    &core_module);

    /* the fd on r->server->error_log is closed, but we need somewhere to
     * put the error messages from the log_* functions. So, we use stderr,
     * since that is better than allowing errors to go unnoticed.  Don't do
     * this on Win32, though, since we haven't fork()'d.
     */
    r->server->error_log = stderr;

    if (conf->limit_cpu != NULL) {
        if ((setrlimit(RLIMIT_CPU, conf->limit_cpu)) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			 "setrlimit: failed to set CPU usage limit");
	}
    }
    if (conf->limit_nproc != NULL) {
        if ((setrlimit(RLIMIT_NPROC, conf->limit_nproc)) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			 "setrlimit: failed to set process limit");
	}
    }
    if (conf->limit_mem != NULL) {
        if ((setrlimit(RLIMIT_DATA, conf->limit_mem)) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, r->server,
			 "setrlimit(RLIMIT_DATA): failed to set memory "
			 "usage limit");
	}
    }
    if (ap_suexec_enabled
	&& ((r->server->server_uid != ap_user_id)
	    || (r->server->server_gid != ap_group_id)
	    || (!strncmp("/~", r->uri, 2)))) {

	char *execuser, *grpname;
	struct passwd *pw;
	struct group *gr;

	if (!strncmp("/~", r->uri, 2)) {
	    gid_t user_gid;
	    char *username = ap_pstrdup(r->pool, r->uri + 2);
	    char *pos = strchr(username, '/');

	    if (pos) {
		*pos = '\0';
	    }

	    if ((pw = getpwnam(username)) == NULL) {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
			     "getpwnam: invalid username %s", username);
		return (pid);
	    }
	    execuser = ap_pstrcat(r->pool, "~", pw->pw_name, NULL);
	    user_gid = pw->pw_gid;

	    if ((gr = getgrgid(user_gid)) == NULL) {
	        if ((grpname = ap_palloc(r->pool, 16)) == NULL) {
		    return (pid);
		}
		else {
		    ap_snprintf(grpname, 16, "%ld", (long) user_gid);
		}
	    }
	    else {
		grpname = gr->gr_name;
	    }
	}
	else {
	    if ((pw = getpwuid(r->server->server_uid)) == NULL) {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
			     "getpwuid: invalid userid %ld",
			     (long) r->server->server_uid);
		return (pid);
	    }
	    execuser = ap_pstrdup(r->pool, pw->pw_name);

	    if ((gr = getgrgid(r->server->server_gid)) == NULL) {
		ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
			     "getgrgid: invalid groupid %ld",
			     (long) r->server->server_gid);
		return (pid);
	    }
	    grpname = gr->gr_name;
	}

	if (shellcmd) {
	    execle(SUEXEC_BIN, SUEXEC_BIN, execuser, grpname, argv0,
		   (char *)NULL, env);
	}

	else if ((conf->cgi_command_args == AP_FLAG_OFF)
            || (!r->args) || (!r->args[0])
            || strchr(r->args, '=')) {
	    execle(SUEXEC_BIN, SUEXEC_BIN, execuser, grpname, argv0,
		   (char *)NULL, env);
	}

	else {
	    execve(SUEXEC_BIN,
		   create_argv(r->pool, SUEXEC_BIN, execuser, grpname,
			       argv0, r->args),
		   env);
	}
    }
    else {
        if (shellcmd) {
	    execle(SHELL_PATH, SHELL_PATH, "-c", argv0, (char *)NULL, env);
	}

	else if ((conf->cgi_command_args == AP_FLAG_OFF)
            || (!r->args) || (!r->args[0])
            || strchr(r->args, '=')) {
	    execle(r->filename, argv0, (void*)NULL, env);
	}

	else {
	    execve(r->filename,
		   create_argv(r->pool, NULL, NULL, NULL, argv0, r->args),
		   env);
	}
    }
    return (pid);
}
