/*	$OpenBSD: aclparse.y,v 1.1.1.1 2004/07/13 22:02:40 jfb Exp $	*/
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
%{
#include <sys/types.h>
#include <sys/queue.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

#include "cvsd.h"
#include "cvs.h"
#include "log.h"
#include "event.h"


#define CVS_ACL_DENY         0
#define CVS_ACL_ALLOW        1

#define CVS_ACL_LOGOPT    0x01
#define CVS_ACL_QUICKOPT  0x02


struct acl_user {
	uid_t  au_uid;
	SLIST_ENTRY(acl_user) au_list;
};


struct acl_rule {
	u_int8_t   ar_act;
	u_int8_t   ar_opts;
	u_int8_t   ar_op;
	char      *ar_path;
	char      *ar_tag;

	SLIST_HEAD(, acl_user) ar_users;
};




typedef struct {
	union {
		u_int32_t         num;
		char             *string;
		struct acl_user  *user_list;
	} v;

	int lineno;
} YYSTYPE;




int    lgetc    (FILE *);


int    yyerror  (const char *, ...);
int    yylex    (void);
int    lookup   (const char *);
int    kw_cmp   (const void *, const void *);

u_int  cvs_acl_matchuid  (struct acl_rule *, uid_t);
u_int  cvs_acl_matchtag  (const char *, const char *);
u_int  cvs_acl_matchpath (const char *, const char *);


static char            *acl_file;
static FILE            *acl_fin;
static int              acl_lineno = 1;
static int              acl_errors = 0;
static struct acl_rule *acl_rules;
static u_int            acl_nrules = 0;
static u_int            acl_defact = CVS_ACL_DENY;


static u_int            acl_opts = 0;


/*
 * cvs_acl_eval()
 *
 * Evaluate a thingamajimmie against the currently loaded ACL ruleset.
 * Returns CVS_ACL_ALLOW if the operation is permitted, CVS_ACL_DENY otherwise.
 */

u_int
cvs_acl_eval(struct cvs_op *op)
{
	u_int i, res;
	struct acl_rule *arp;

	/* deny by default */
	res = acl_defact;

	for (i = 0; i < acl_nrules; i++) {
		arp = &(acl_rules[i]);

		if ((op->co_op != arp->ar_op) ||
		    !cvs_acl_matchuid(arp, op->co_uid) ||
		    !cvs_acl_matchtag(op->co_tag, arp->ar_tag) ||
		    !cvs_acl_matchpath(op->co_path, arp->ar_path))
			continue;

		res = arp->ar_act;

		if (arp->ar_opts & CVS_ACL_LOGOPT)
			cvs_log(LP_WARN, "act=%u, path=%s, tag=%s, uid=%u",
			    op->co_op, op->co_path, op->co_tag, op->co_uid);
		if (arp->ar_opts & CVS_ACL_QUICKOPT)
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

	printf("comparing `%s' to `%s'\n", rop_path, acl_path);
	len = strlen(rop_path);

	if (strncmp(rop_path, acl_path, len) == 0)
		return (1);

	return (0);
}
%}

%token	ALLOW DENY LOG QUICK ON TAG FROM
%token  ADD CHECKOUT COMMIT DIFF HISTORY UPDATE
%token  <v.string>    STRING
%type   <v.num>       action options operation
%type   <v.userlist>
%type   <v.string>    pathspec tagspec
%%

ruleset		: /* empty */
		| ruleset '\n'
		| ruleset rule '\n'
		| ruleset error '\n'	{ acl_errors++; }
		;

rule		: action options operation pathspec tagspec userspec
		{
			void *tmp;
			struct acl_rule *arp;

			tmp = realloc(acl_rules,
			    (acl_nrules + 1) * sizeof(struct acl_rule));
			if (tmp == NULL) {
				cvs_log(LP_ERRNO, "failed to grow ACL ruleset");
				free($4);
				free($5);
				YYERROR;
			}
			acl_rules = (struct acl_rule *)tmp;
			arp = &(acl_rules[acl_nrules++]);

			arp->ar_act = $1;
			arp->ar_opts = $2;
			arp->ar_op = $3;
			SLIST_INIT(&arp->ar_users);
			arp->ar_path = $4;
			arp->ar_tag = $5;

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
%%


struct acl_kw {
	char  *kw_str;
	u_int  kw_id;
};



static const struct acl_kw keywords[] = {
	{ "add",     ADD    },
	{ "allow",   ALLOW  },
	{ "commit",  COMMIT },
	{ "deny",    DENY   },
	{ "from",    FROM   },
	{ "log",     LOG    },
	{ "on",      ON     },
	{ "quick",   QUICK  },
	{ "tag",     TAG    },
};

int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct acl_kw *)e)->kw_str));
}


int
lookup(const char *tstr)
{
	int type;
	const struct acl_kw *kwp;

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

	c = getc(f);
	if ((c == '\t') || (c == ' ')) {
		do {
			c = getc(f);
		} while ((c == ' ') || (c == '\t'));
		ungetc(c, f);
		c = ' ';
	}
	else if (c == '\\')
		c = getc(f);

	return (c);
}


int
yylex(void)
{
	int tok, c;
	char buf[1024], *bp, *ep;

	bp = buf;
	ep = buf + sizeof(buf) - 1;

	yylval.lineno = acl_lineno;

	/* skip whitespace */
	while ((c = lgetc(acl_fin)) == ' ')
		;

	if (c == '#') {
		do {
			c = lgetc(acl_fin);
		} while ((c != '\n') && (c != EOF));
	}
	else if (c == EOF)
		c = 0;
	else if (c == '\n')
		acl_lineno++;
	else if (c != ',') {
		do {
			*bp++ = c;
			if (bp == ep) {
				yyerror("string too long");
				return (-1);
			}

			c = lgetc(acl_fin);
			if (c == EOF)
				break;
		} while ((c != EOF) && (c != ' ') && (c != '\n'));
		ungetc(c, acl_fin);
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

	if (asprintf(&nfmt, "%s:%d: %s", acl_file, yylval.lineno, fmt) == -1) {
		cvs_log(LP_ERRNO, "failed to allocate message buffer");
		return (-1);
	}
	cvs_vlog(LP_ERR, nfmt, vap);

	free(nfmt);
	va_end(vap);
	return (0);

}


/*
 * cvs_acl_parse()
 *
 * Parse the contents of the ACL file <file>.
 */

int
cvs_acl_parse(const char *file)
{
	acl_file = strdup(file);
	if (acl_file == NULL) {
		cvs_log(LP_ERRNO, "failed to copy ACL file path");
		return (-1);
	}

	acl_fin = fopen(file, "r");
	if (acl_fin == NULL) {
		cvs_log(LP_ERRNO, "failed to open ACL file `%s'", file);
		return (-1);
	}

	if (yyparse() != 0)
		acl_lineno = -1;

	(void)fclose(acl_fin);

	return (acl_lineno);
}
