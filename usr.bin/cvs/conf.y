/*	$OpenBSD: conf.y,v 1.3 2004/09/27 12:39:29 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */
/*
 * Configuration parser for the CVS daemon
 *
 * Thanks should go to Henning Brauer for providing insight on some
 * questions I had regarding my grammar.
 */

%{
#include <sys/types.h>
#include <sys/queue.h>

#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "cvsd.h"
#include "cvs.h"
#include "log.h"
#include "file.h"


#define CVS_ACL_MAXRULES     256

#define CVS_ACL_DENY         0
#define CVS_ACL_ALLOW        1

#define CVS_ACL_LOGOPT    0x01
#define CVS_ACL_QUICKOPT  0x02


struct conf_macro {
	char  *cm_name;
	char  *cm_val;

	SIMPLEQ_ENTRY(conf_macro) cm_list;
};


struct acl_user {
	uid_t  au_uid;
	SLIST_ENTRY(acl_user) au_list;
};


struct acl_rule {
	u_int8_t   ar_id;
	u_int8_t   ar_act;
	u_int8_t   ar_opts;
	u_int8_t   ar_op;
	char      *ar_path;
	char      *ar_tag;

	SLIST_HEAD(, acl_user) ar_users;
	TAILQ_ENTRY(acl_rule)  ar_list;
};





typedef struct {
	union {
		u_int64_t         num;
		char             *string;
		struct acl_rule  *rule;
		struct acl_user  *user_list;
		struct cvsd_addr *addr;
	} v;

	int lineno;
} YYSTYPE;




int    lgetc    (FILE *);
int    lungetc  (int, FILE *);


int    yyerror  (const char *, ...);
int    yylex    (void);
int    yyparse  (void);
int    lookup   (const char *);
int    kw_cmp   (const void *, const void *);

int         cvs_conf_setmacro (char *, char *);
const char* cvs_conf_getmacro (const char *);

int    cvs_acl_addrule   (struct acl_rule *);
u_int  cvs_acl_matchuid  (struct acl_rule *, uid_t);
u_int  cvs_acl_matchtag  (const char *, const char *);
u_int  cvs_acl_matchpath (const char *, const char *);


/* parse buffer for easier macro expansion */
static char  *conf_pbuf = NULL;
static int    conf_pbind = 0;



static const char      *conf_file;
static FILE            *conf_fin;
static int              conf_lineno = 1;
static int              conf_errors = 0;

static SIMPLEQ_HEAD(, conf_macro) conf_macros;

/* ACL rules */
static TAILQ_HEAD(, acl_rule) acl_rules;
static u_int            acl_nrules = 0;
static u_int            acl_defact = CVS_ACL_DENY;

%}

%token	LISTEN CVSROOT MINCHILD MAXCHILD REQSOCK
%token	ALLOW DENY LOG QUICK ON TAG FROM USER GROUP
%token  ANY ADD CHECKOUT COMMIT DIFF HISTORY UPDATE
%token  <v.string>    STRING
%type   <v.num>       action number options operation
%type	<v.rule>      aclrule
%type	<v.addr>      address
%type   <v.userlist>
%type   <v.string>    pathspec tagspec
%%

conf		: /* empty */
		| conf '\n'
		| conf cfline '\n'
		| conf error '\n'	{ conf_errors++; }
		;

cfline		: macro_assign
		| directive
		| aclrule
		;

macro_assign	: STRING '=' STRING
		{
			if (cvs_conf_setmacro($1, $3) < 0) {
				free($1);
				free($3);
				YYERROR;
			}
		}
		;

directive	: LISTEN address
		{
			cvsd_set(CVSD_SET_ADDR, $2);
			free($2);
		}
		| CVSROOT STRING
		{
			cvsd_set(CVSD_SET_ROOT, $2);
			free($2);
		}
		| USER STRING
		{
			cvsd_set(CVSD_SET_USER, $2);
			free($2);
		}
		| GROUP STRING
		{
			cvsd_set(CVSD_SET_GROUP, $2);
			free($2);
		}
		| MINCHILD number	{ cvsd_set(CVSD_SET_CHMIN, $2); }
		| MAXCHILD number	{ cvsd_set(CVSD_SET_CHMAX, $2); }
		| REQSOCK STRING
		{
			cvsd_set(CVSD_SET_SOCK, $2);
			free($2);
		}
		;

address		: STRING
		{
			struct cvsd_addr *adp;

			adp = (struct cvsd_addr *)malloc(sizeof(*adp));
			if (adp == NULL) {
				YYERROR;
			}

			$$ = adp;
		}
		;

aclrule		: action options operation pathspec tagspec userspec
		{
			struct acl_rule *arp;

			arp = (struct acl_rule *)malloc(sizeof(*arp));
			if (arp == NULL) {
				free($4);
				free($5);
				YYERROR;
			}
			arp->ar_act = $1;
			arp->ar_opts = $2;
			arp->ar_op = $3;
			SLIST_INIT(&arp->ar_users);
			arp->ar_path = $4;
			arp->ar_tag = $5;

			$$ = arp;
		}
		;

action		: ALLOW		{ $$ = CVS_ACL_ALLOW; }
		| DENY		{ $$ = CVS_ACL_DENY;  }
		;

options		: /* empty */	{ $$ = 0; }
		| LOG		{ $$ = CVS_ACL_LOGOPT; }
		| QUICK		{ $$ = CVS_ACL_QUICKOPT; }
		| LOG QUICK	{ $$ = CVS_ACL_LOGOPT | CVS_ACL_QUICKOPT; }
		;

operation	: ADD		{ $$ = CVS_OP_ADD; }
		| ANY		{ $$ = CVS_OP_ANY; }
		| COMMIT	{ $$ = CVS_OP_COMMIT; }
		| TAG		{ $$ = CVS_OP_TAG; }
		;

pathspec	: /* empty */	{ $$ = NULL; }
		| ON STRING	{ $$ = $2;   }
		;

tagspec		: /* empty */	{ $$ = NULL; }
		| TAG STRING	{ $$ = $2; }
		;

userspec	: /* empty */
		| FROM userlist
		;

userlist	: user
		| userlist ',' user
		;

user		: STRING
		{
			uid_t uid;
			char *ep;
			struct passwd *pw;
			struct acl_user *aup;

			uid = (uid_t)strtol($1, &ep, 10);
			if (*ep != '\0')
				pw = getpwnam($1);
			else
				pw = getpwuid(uid);
			if (pw == NULL) {
				yyerror("invalid username or ID `%s'", $1);
				YYERROR;
			}

			aup = (struct acl_user *)malloc(sizeof(*aup));
			if (aup == NULL) {
				yyerror("failed to allocate ACL user data");
				YYERROR;
			}
			aup->au_uid = pw->pw_uid;
		}
		;

number		: STRING
		{
			char *ep;
			long res;
			res = strtol($1, &ep, 0);
			if ((res == LONG_MIN) || (res == LONG_MAX)) {
				yyerror("%sflow while converting number `%s'",
				    res == LONG_MIN ? "under" : "over", $1);
				free($1);
				YYERROR;
			}
			else if (*ep != '\0') {
				yyerror("invalid number `%s'", $1);
				free($1);
				YYERROR;
			}

			$$ = (u_int64_t)res;
			free($1);
		}
		;

%%


struct conf_kw {
	char  *kw_str;
	u_int  kw_id;
};



static const struct conf_kw keywords[] = {
	{ "add",     ADD     },
	{ "allow",   ALLOW   },
	{ "any",     ANY     },
	{ "commit",  COMMIT  },
	{ "cvsroot", CVSROOT },
	{ "deny",    DENY    },
	{ "from",    FROM    },
	{ "group",   GROUP   },
	{ "listen",  LISTEN  },
	{ "log",     LOG     },
	{ "on",      ON      },
	{ "quick",   QUICK   },
	{ "reqsock", REQSOCK },
	{ "tag",     TAG     },
	{ "user",    USER    },

};

int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct conf_kw *)e)->kw_str));
}


int
lookup(const char *tstr)
{
	int type;
	const struct conf_kw *kwp;

	kwp = bsearch(tstr, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);
	if (kwp != NULL)
		type = kwp->kw_id;
	else
		type = STRING;
	return (type);
}



int
lgetc(FILE *f)
{
	int c;

	/* check if we've got something in the parse buffer first */
	if (conf_pbuf != NULL) {
		c = conf_pbuf[conf_pbind++];
		if (c != '\0')
			return (c);

		free(conf_pbuf);
		conf_pbuf = NULL;
		conf_pbind = 0;
	}

	c = getc(f);
	if ((c == '\t') || (c == ' ')) {
		do {
			c = getc(f);
		} while ((c == ' ') || (c == '\t'));
		lungetc(c, f);
		c = ' ';
	}
	else if (c == '\\')
		c = getc(f);

	return (c);
}


int
lungetc(int c, FILE *f)
{
	if ((conf_pbuf != NULL) && (conf_pbind > 0)) {
		conf_pbind--;
		return (0);
	}

	return ungetc(c, f);


}





int
yylex(void)
{
	int c;
	char buf[1024], *bp, *ep;
	const char *mval;

lex_start:
	bp = buf;
	ep = buf + sizeof(buf) - 1;

	yylval.lineno = conf_lineno;

	/* skip whitespace */
	while ((c = lgetc(conf_fin)) == ' ')
		;

	if (c == '#') {
		do {
			c = lgetc(conf_fin);
		} while ((c != '\n') && (c != EOF));
	}

	if (c == EOF)
		c = 0;
	else if (c == '\n')
		yylval.lineno = conf_lineno++;
	else if (c == '$') {
		c = lgetc(conf_fin);
		do {
			*bp++ = (char)c;
			if (bp == ep) {
				yyerror("macro name too long");
				return (-1);
			}
			c = lgetc(conf_fin);
		} while (isalnum(c) || c == '_');
		lungetc(c, conf_fin);
		*bp = '\0';

		mval = cvs_conf_getmacro(buf);
		if (mval == NULL) {
			yyerror("undefined macro `%s'", buf);
			return (-1);
		}

		conf_pbuf = strdup(mval);
		conf_pbind = 0;
		goto lex_start;
	}
	else if ((c == '=') || (c == ','))
		; /* nothing */
	else {
		do {
			*bp++ = (char)c;
			if (bp == ep) {
				yyerror("string too long");
				return (-1);
			}

			c = lgetc(conf_fin);
		} while ((c != EOF) && (c != ' ') && (c != '\n'));
		lungetc(c, conf_fin);
		*bp = '\0';
		c = lookup(buf);
		if (c == STRING) {
			yylval.v.string = strdup(buf);
			if (yylval.v.string == NULL) {
				cvs_log(LP_ERRNO,
				    "failed to copy token string");
				return (-1);
			}
		}
	}

	return (c);
}



int
yyerror(const char *fmt, ...)
{
	char *nfmt;
	va_list vap;

	va_start(vap, fmt);

	if (asprintf(&nfmt, "%s:%d: %s", conf_file, yylval.lineno, fmt) == -1) {
		cvs_log(LP_ERRNO, "failed to allocate message buffer");
		return (-1);
	}
	cvs_vlog(LP_ERR, nfmt, vap);

	free(nfmt);
	va_end(vap);
	return (0);

}


/*
 * cvs_conf_setmacro()
 *
 * Add an entry in the macro list for the macro whose name is <macro> and
 * whose value is <val>.
 * Returns 0 on success, or -1 on failure.
 */

int
cvs_conf_setmacro(char *macro, char *val)
{
	struct conf_macro *cmp;

	cmp = (struct conf_macro *)malloc(sizeof(*cmp));
	if (cmp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate macro");
		return (-1);
	}

	/* these strings were already dup'ed by the lexer */
	cmp->cm_name = macro;
	cmp->cm_val = val;

	SIMPLEQ_INSERT_TAIL(&conf_macros, cmp, cm_list);

	return (0);
}


/*
 * cvs_conf_getmacro()
 *
 * Get a macro <macro>'s associated value.  Returns the value string on
 * success, or NULL if no such macro exists.
 */

const char*
cvs_conf_getmacro(const char *macro)
{
	struct conf_macro *cmp;

	SIMPLEQ_FOREACH(cmp, &conf_macros, cm_list)
		if (strcmp(cmp->cm_name, macro) == 0)
			return (cmp->cm_val);

	return (NULL);
}


/*
 * cvs_conf_read()
 *
 * Parse the contents of the configuration file <conf>.
 */

int
cvs_conf_read(const char *conf)
{
	struct conf_macro *cmp;

	SIMPLEQ_INIT(&conf_macros);
	TAILQ_INIT(&acl_rules);
	acl_nrules = 0;

	cvs_log(LP_INFO, "using configuration file `%s'", conf);
	conf_file = conf;
	conf_fin = fopen(conf, "r");
	if (conf_fin == NULL) {
		cvs_log(LP_ERRNO, "failed to open configuration `%s'", conf);
		return (-1);
	}

	if (yyparse() != 0)
		conf_lineno = -1;

	(void)fclose(conf_fin);

	/* we can get rid of macros now */
	while ((cmp = SIMPLEQ_FIRST(&conf_macros)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&conf_macros, cm_list);
		free(cmp->cm_name);
		free(cmp->cm_val);
		free(cmp);
	}

	cvs_log(LP_INFO, "config %s parsed successfully", conf);

	return (conf_lineno);
}


/*
 * cvs_acl_addrule()
 *
 * Add a rule to the currently loaded ACL rules.
 */

int
cvs_acl_addrule(struct acl_rule *rule)
{
	if (acl_nrules == CVS_ACL_MAXRULES) {
		cvs_log(LP_ERR, "failed to add ACL rule: Ruleset full");
		return (-1);
	}

	TAILQ_INSERT_TAIL(&acl_rules, rule, ar_list);
	return (0);
}


/*
 * cvs_acl_eval()
 *
 * Evaluate a thingamajimmie against the currently loaded ACL ruleset.
 * Returns CVS_ACL_ALLOW if the operation is permitted, CVS_ACL_DENY otherwise.
 */

u_int
cvs_acl_eval(struct cvs_op *op)
{
	u_int res;
	CVSFILE *cf;
	struct acl_rule *rule;

	/* deny by default */
	res = acl_defact;

	TAILQ_FOREACH(rule, &acl_rules, ar_list) {
		if (((op->co_op != CVS_OP_ANY) && (op->co_op != rule->ar_op)) ||
		    !cvs_acl_matchuid(rule, op->co_uid) ||
		    !cvs_acl_matchtag(op->co_tag, rule->ar_tag))
			continue;

		/* see if one of the files has a matching path */
		TAILQ_FOREACH(cf, &(op->co_files), cf_list)
			if (!cvs_acl_matchpath(cf->cf_path, rule->ar_path))
				continue;

		res = rule->ar_act;

		if (rule->ar_opts & CVS_ACL_LOGOPT)
			cvs_log(LP_WARN, "act=%u, tag=%s, uid=%u",
			    op->co_op, op->co_tag, op->co_uid);
		if (rule->ar_opts & CVS_ACL_QUICKOPT)
			break;
	}

	return (res);
}


/*
 * cvs_acl_matchuid()
 *
 * Check if an ACL rule has a UID matching <uid>.  If no user is specified
 * for a given rule, any UID will match.
 * Returns 1 if this is the case, 0 otherwise.
 */

u_int
cvs_acl_matchuid(struct acl_rule *rule, uid_t uid)
{
	struct acl_user *aup;

	if (SLIST_EMPTY(&(rule->ar_users)))
		return (1);

	SLIST_FOREACH(aup, &(rule->ar_users), au_list)
		if (aup->au_uid == uid)
			return (1);
	return (0);
}


/*
 * cvs_acl_matchtag()
 *
 * Returns 1 if this is the case, 0 otherwise.
 */

u_int
cvs_acl_matchtag(const char *tag1, const char *tag2)
{
	if ((tag1 == NULL) && (tag2 == NULL))   /* HEAD */
		return (1);

	if ((tag1 != NULL) && (tag2 != NULL) &&
	    (strcmp(tag1, tag2) == 0))
		return (1);

	return (0);
}


/*
 * cvs_acl_matchpath()
 *
 * Check if the path <op_path> is a subpath of <acl_path>.
 * Returns 1 if this is the case, 0 otherwise.
 */

u_int
cvs_acl_matchpath(const char *op_path, const char *acl_path)
{
	size_t len;
	char rop_path[MAXPATHLEN];

	/* if the ACL path is NULL, apply on all paths */
	if (acl_path == NULL)
		return (1);

	if (realpath(op_path, rop_path) == NULL) {
		cvs_log(LP_ERRNO, "failed to convert `%s' to a real path",
		    op_path);
		return (0);
	}

	len = strlen(rop_path);

	if (strncmp(rop_path, acl_path, len) == 0)
		return (1);

	return (0);
}
