/*
**  mod_define.c - Apache module for configuration defines ($xxx)
**
**  Copyright (c) 1998-2000 Ralf S. Engelschall <rse@engelschall.com>
**  Copyright (c) 1998-2000 Christian Reiber <chrei@en.muc.de>
**
**  Permission to use, copy, modify, and distribute this software for
**  any purpose with or without fee is hereby granted, provided that
**  the above copyright notice and this permission notice appear in all
**  copies.
**
**  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
**  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
**  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
**  IN NO EVENT SHALL THE AUTHORS AND COPYRIGHT HOLDERS AND THEIR
**  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
**  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
**  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
**  USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
**  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
**  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
**  OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
*/

/*
 *  HISTORY
 *
 *  v1.0: Originally written in December 1998 by
 *        Ralf S. Engelschall <rse@engelschall.com> and
 *        Christian Reiber <chrei@en.muc.de>
 *
 *  v1.1: Completely Overhauled in August 1999 by
 *        Ralf S. Engelschall <rse@engelschall.com>
 */

#include "ap_config.h"
#include "ap_ctype.h"
#include "httpd.h"
#include "http_config.h"
#include "http_conf_globals.h"
#include "http_core.h"
#include "http_log.h"

#ifndef EAPI
#error "This module requires the Extended API (EAPI) facilities."
#endif

/*
 * The global table of defines
 */

static table *tDefines         = NULL;   /* global table of defines */
static int    bOnceSeenADefine = FALSE;  /* optimization flag */

/*
 * Forward declaration
 */
static int   DefineIndex      (pool *, char *, int *, int *, char **);
static char *DefineFetch      (pool *, char *);
static char *DefineExpand     (pool *, char *, int, char *);
static void  DefineInit       (pool *);
static void  DefineCleanup    (void *);
static char *DefineRewriteHook(cmd_parms *, void *, const char *);

/*
 * Character classes for scanner function
 */
typedef enum {
    CC_ESCAPE, CC_DOLLAR, CC_BRACEOPEN, CC_BRACECLOSE,
    CC_IDCHAR1, CC_IDCHAR, CC_OTHER, CC_EOS
} CharClass;

/*
 * Scanner states for scanner function
 */
typedef enum {
    SS_NONE, SS_SKIP, SS_DOLLAR, SS_TOKEN_BRACED,
    SS_TOKEN_UNBRACED, SS_ERROR, SS_FOUND
} ScanState;

/*
 * Default meta characters
 */
#define DEFAULT_MC_ESCAPE      "\\"
#define DEFAULT_MC_DOLLAR      "$"
#define DEFAULT_MC_BRACEOPEN   "{"
#define DEFAULT_MC_BRACECLOSE  "}"

/*
 * Scanner for variable constructs $xxx and ${xxx}
 */
static int DefineIndex(pool *p, char *cpLine, int *pos, int *len, char **cpVar)
{
    int rc;
    char *cp;
    char *cp2;
    CharClass cc;
    char cEscape;
    char cDefine;
    char cBraceOpen;
    char cBraceClose;
    char *cpError;
    ScanState s;

    cEscape = DEFAULT_MC_ESCAPE[0];
    if ((cp = DefineFetch(p, "mod_define::escape")) != NULL)
        cEscape = cp[0];
    cDefine = DEFAULT_MC_DOLLAR[0];
    if ((cp = DefineFetch(p, "mod_define::dollar")) != NULL)
        cDefine = cp[0];
    cBraceOpen = DEFAULT_MC_BRACEOPEN[0];
    if ((cp = DefineFetch(p, "mod_define::braceopen")) != NULL)
        cBraceOpen = cp[0];
    cBraceClose = DEFAULT_MC_BRACECLOSE[0];
    if ((cp = DefineFetch(p, "mod_define::braceclose")) != NULL)
        cBraceClose = cp[0];

    rc = 0;
    *len = 0;
    cc = CC_OTHER;
    s = SS_NONE;
    for (cp = cpLine+(*pos); cc != CC_EOS; cp++) {
        if (*cp == cEscape)
            cc = CC_ESCAPE;
        else if (*cp == cDefine)
            cc = CC_DOLLAR;
        else if (*cp == cBraceOpen)
            cc = CC_BRACEOPEN;
        else if (*cp == cBraceClose)
            cc = CC_BRACECLOSE;
        else if (ap_isalpha(*cp))
            cc = CC_IDCHAR1;
        else if (ap_isdigit(*cp) || *cp == '_' || *cp == ':')
            cc = CC_IDCHAR;
        else if (*cp == '\0')
            cc = CC_EOS;
        else
            cc = CC_OTHER;
        switch (s) {
            case SS_NONE:
                switch (cc) {
                    case CC_ESCAPE:
                        s = SS_SKIP;
                        break;
                    case CC_DOLLAR:
                        s = SS_DOLLAR;
                        break;
                    default:
                        break;
                }
                break;
            case SS_SKIP:
                s = SS_NONE;
                continue;
                break;
            case SS_DOLLAR:
                switch (cc) {
                    case CC_BRACEOPEN:
                        s = SS_TOKEN_BRACED;
                        *pos = cp-cpLine-1;
                        (*len) = 2;
                        *cpVar = cp+1;
                        break;
                    case CC_IDCHAR1:
                        s = SS_TOKEN_UNBRACED;
                        *pos = cp-cpLine-1;
                        (*len) = 2;
                        *cpVar = cp;
                        break;
                    case CC_ESCAPE:
                        s = SS_SKIP;
                        break;
                    default:
                        s = SS_NONE;
                        break;
                }
                break;
            case SS_TOKEN_BRACED:
                switch (cc) {
                    case CC_IDCHAR1:
                    case CC_IDCHAR:
                        (*len)++;
                        break;
                    case CC_BRACECLOSE:
                        (*len)++;
                        cp2 = ap_palloc(p, cp-*cpVar+1);
                        ap_cpystrn(cp2, *cpVar, cp-*cpVar+1);
                        *cpVar = cp2;
                        s = SS_FOUND;
                        break;
                    default:
                        cpError = ap_psprintf(p, "Illegal character '%c' in identifier", *cp);
                        s = SS_ERROR;
                        break;
                }
                break;
            case SS_TOKEN_UNBRACED:
                switch (cc) {
                    case CC_IDCHAR1:
                    case CC_IDCHAR:
                        (*len)++;
                        break;
                    default:
                        cp2 = ap_palloc(p, cp-*cpVar+1);
                        ap_cpystrn(cp2, *cpVar, cp-*cpVar+1);
                        *cpVar = cp2;
                        s = SS_FOUND;
                        break;
                }
                break;
            case SS_FOUND:
            case SS_ERROR:
                break;
        }
        if (s == SS_ERROR) {
            fprintf(stderr, "Error\n");
            break;
        }
        else if (s == SS_FOUND) {
            rc = 1;
            break;
        }
    }
    return rc;
}

/*
 * Determine the value of a variable
 */
static char *DefineFetch(pool *p, char *cpVar)
{
    char *cpVal;

    /* first try out table */
    if ((cpVal = (char *)ap_table_get(tDefines, (char *)cpVar)) != NULL)
        return cpVal;
    /* second try the environment */
    if ((cpVal = getenv(cpVar)) != NULL)
        return cpVal;
    return NULL;
}

/*
 * Expand a variable
 */
static char *DefineExpand(pool *p, char *cpToken, int tok_len, char *cpVal)
{
    char *cp;
    int val_len, rest_len;

    val_len  = strlen(cpVal);
    rest_len = strlen(cpToken+tok_len);
    if (val_len < tok_len)
        memcpy(cpToken+val_len, cpToken+tok_len, rest_len+1);
    else if (val_len > tok_len)
        for (cp = cpToken+strlen(cpToken); cp > cpToken+tok_len-1; cp--)
            *(cp+(val_len-tok_len)) = *cp;
    memcpy(cpToken, cpVal, val_len);
    return NULL;
}

/*
 * The EAPI hook which is called after Apache has read a
 * configuration line and before it's actually processed
 */
static char *DefineRewriteHook(cmd_parms *cmd, void *config, const char *line)
{
    pool *p;
    char *cpBuf;
    char *cpLine;
    int pos;
    int len;
    char *cpError;
    char *cpVar;
    char *cpVal;
    server_rec *s;

    /* runtime optimization */
    if (!bOnceSeenADefine)
        return NULL;

    p  = cmd->pool;
    s  = cmd->server;

    /*
     * Search for:
     *  ....\$[a-zA-Z][:_a-zA-Z0-9]*....
     *  ....\${[a-zA-Z][:_a-zA-Z0-9]*}....
     */
    cpBuf = NULL;
    cpLine = (char *)line;
    pos = 0;
    while (DefineIndex(p, cpLine, &pos, &len, &cpVar)) {
#ifdef DEFINE_DEBUG
        {
        char prefix[1024];
        char marker[1024];
        int i;
        for (i = 0; i < pos; i++)
            prefix[i] = ' ';
        prefix[i] = '\0';
        for (i = 0; i < len; i++)
            marker[i] = '^';
        marker[i] = '\0';
        fprintf(stderr,
                "Found variable `%s' (pos: %d, len: %d)\n"
                "  %s\n"
                "  %s%s\n",
                cpVar, pos, len, cpLine, prefix, marker);
        }
#endif
        if (cpBuf == NULL) {
            cpBuf = ap_palloc(p, MAX_STRING_LEN);
            ap_cpystrn(cpBuf, line, MAX_STRING_LEN);
            cpLine = cpBuf;
        }
        if ((cpVal = DefineFetch(p, cpVar)) == NULL) {
            ap_log_error(APLOG_MARK, APLOG_ERR, s,
                         "mod_define: Variable '%s' not defined: file %s, line %d",
                         cpVar, cmd->config_file->name,
                         cmd->config_file->line_number);
            cpBuf = NULL;
            break;
        }
        if ((cpError = DefineExpand(p, cpLine+pos, len, cpVal)) != NULL) {
            ap_log_error(APLOG_MARK, APLOG_ERR, s,
                         "mod_define: %s: file %s, line %d",
                         cpError, cmd->config_file->name,
                         cmd->config_file->line_number);
            cpBuf = NULL;
            break;
        }
    }
    return cpBuf;
}

/*
 * Implementation of the `Define' configuration directive
 */
static const char *cmd_define(cmd_parms *cmd, void *config,
                              char *cpVar, char *cpVal)
{
    if (tDefines == NULL)
        DefineInit(cmd->pool);
    ap_table_set(tDefines, cpVar, cpVal);
    bOnceSeenADefine = TRUE;
    return NULL;
}

/*
 * Module Initialization
 */

static void DefineInit(pool *p)
{
    tDefines = ap_make_table(p, 10);
    /* predefine delimiters */
    ap_table_set(tDefines, "mod_define::escape", DEFAULT_MC_ESCAPE);
    ap_table_set(tDefines, "mod_define::dollar", DEFAULT_MC_DOLLAR);
    ap_table_set(tDefines, "mod_define::open",   DEFAULT_MC_BRACEOPEN);
    ap_table_set(tDefines, "mod_define::close",  DEFAULT_MC_BRACECLOSE);
    ap_register_cleanup(p, NULL, DefineCleanup, ap_null_cleanup);
    return;
}

/*
 * Module Cleanup
 */

static void DefineCleanup(void *data)
{
    /* reset private variables when config pool is cleared */
    tDefines         = NULL;
    bOnceSeenADefine = FALSE;
    return;
}

/*
 * Module Directive lists
 */
static const command_rec DefineDirectives[] = {
    { "Define", cmd_define, NULL, RSRC_CONF|ACCESS_CONF, TAKE2,
      "Define a configuration variable" },
    { NULL }
};

/*
 * Module API dispatch list
 */
module MODULE_VAR_EXPORT define_module = {
    STANDARD_MODULE_STUFF,
    NULL,                  /* module initializer                  */
    NULL,                  /* create per-dir    config structures */
    NULL,                  /* merge  per-dir    config structures */
    NULL,                  /* create per-server config structures */
    NULL,                  /* merge  per-server config structures */
    DefineDirectives,      /* table of config file commands       */
    NULL,                  /* [#8] MIME-typed-dispatched handlers */
    NULL,                  /* [#1] URI to filename translation    */
    NULL,                  /* [#4] validate user id from request  */
    NULL,                  /* [#5] check if the user is ok _here_ */
    NULL,                  /* [#2] check access by host address   */
    NULL,                  /* [#6] determine MIME type            */
    NULL,                  /* [#7] pre-run fixups                 */
    NULL,                  /* [#9] log a transaction              */
    NULL,                  /* [#3] header parser                  */
    NULL,                  /* child_init                          */
    NULL,                  /* child_exit                          */
    NULL,                  /* [#0] post read-request              */
    NULL,                  /* EAPI: add_module                    */
    NULL,                  /* EAPI: del_module                    */
    DefineRewriteHook,     /* EAPI: rewrite_command               */
    NULL                   /* EAPI: new_connection                */
};

