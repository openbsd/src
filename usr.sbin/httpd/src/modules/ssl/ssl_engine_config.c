/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |
** | '_ ` _ \ / _ \ / _` |   / __/ __| |
** | | | | | | (_) | (_| |   \__ \__ \ | mod_ssl - Apache Interface to SSLeay
** |_| |_| |_|\___/ \__,_|___|___/___/_| http://www.engelschall.com/sw/mod_ssl/
**                      |_____|
**  ssl_engine_config.c
**  Apache Configuration Directives
*/

/* ====================================================================
 * Copyright (c) 1998-1999 Ralf S. Engelschall. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by
 *     Ralf S. Engelschall <rse@engelschall.com> for use in the
 *     mod_ssl project (http://www.engelschall.com/sw/mod_ssl/)."
 *
 * 4. The names "mod_ssl" must not be used to endorse or promote
 *    products derived from this software without prior written
 *    permission. For written permission, please contact
 *    rse@engelschall.com.
 *
 * 5. Products derived from this software may not be called "mod_ssl"
 *    nor may "mod_ssl" appear in their names without prior
 *    written permission of Ralf S. Engelschall.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by
 *     Ralf S. Engelschall <rse@engelschall.com> for use in the
 *     mod_ssl project (http://www.engelschall.com/sw/mod_ssl/)."
 *
 * THIS SOFTWARE IS PROVIDED BY RALF S. ENGELSCHALL ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL RALF S. ENGELSCHALL OR
 * HIS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

                                      /* ``Damned if you do,
                                           damned if you don't.''
                                               -- Unknown        */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  Support for Global Configuration
**  _________________________________________________________________
*/

void ssl_hook_AddModule(module *m)
{
    if (m == &ssl_module) {
        /*
         * Announce us for the configuration files
         */
        ap_add_config_define("MOD_SSL");

        /*
         * Link ourself into the Apache kernel
         */
        ssl_var_register();
        ssl_ext_register();
        ssl_io_register();
    }
    return;
}

void ssl_hook_RemoveModule(module *m)
{
    if (m == &ssl_module) {
        /*
         * Unlink ourself from the Apache kernel
         */
        ssl_var_unregister();
        ssl_ext_unregister();
        ssl_io_unregister();
    }
    return;
}

void ssl_config_global_create(void)
{
    pool *pPool;
    SSLModConfigRec *mc;

    mc = ap_ctx_get(ap_global_ctx, "ssl_module");
    if (mc == NULL) {
        /*
         * allocate an own subpool which survives server restarts
         */
        pPool = ap_make_sub_pool(NULL);
        mc = (SSLModConfigRec *)ap_palloc(pPool, sizeof(SSLModConfigRec));
        mc->pPool = pPool;
        mc->bFixed = FALSE;

        /*
         * initialize per-module configuration
         */
        mc->nInitCount             = 0;
        mc->pRSATmpKey             = NULL;
        mc->nSessionCacheMode      = SSL_SCMODE_UNSET;
        mc->szSessionCacheDataFile = NULL;
        mc->nMutexMode             = SSL_MUTEXMODE_UNSET;
        mc->szMutexFile            = NULL;
        mc->nMutexFD               = -1;
        mc->nMutexSEMID            = -1;
        mc->aRandSeed              = ap_make_array(pPool, 4, sizeof(ssl_randseed_t));

        mc->tPrivateKey            = ssl_ds_table_make(pPool, sizeof(ssl_asn1_t));
        mc->tPublicCert            = ssl_ds_table_make(pPool, sizeof(ssl_asn1_t));

        /*
         * And push it into Apache's global context
         */
        ap_ctx_set(ap_global_ctx, "ssl_module", mc);
    }
    return;
}

void ssl_config_global_fix(void)
{
    SSLModConfigRec *mc = myModConfig();
    mc->bFixed = TRUE;
    return;
}

BOOL ssl_config_global_isfixed(void)
{
    SSLModConfigRec *mc = myModConfig();
    return (mc->bFixed);
}


/*  _________________________________________________________________
**
**  Configuration handling
**  _________________________________________________________________
*/

/*
 *  Create per-server SSL configuration
 */
void *ssl_config_server_create(pool *p, server_rec *s)
{
    SSLSrvConfigRec *sc;

    ssl_config_global_create();

    sc = ap_palloc(p, sizeof(SSLSrvConfigRec));
    sc->bEnabled               = UNSET;
    sc->szCertificateFile      = NULL;
    sc->szKeyFile              = NULL;
    sc->szCACertificatePath    = NULL;
    sc->szCACertificateFile    = NULL;
    sc->szLogFile              = NULL;
    sc->szCipherSuite          = NULL;
    sc->nLogLevel              = SSL_LOG_NONE;
    sc->nVerifyDepth           = UNSET;
    sc->nVerifyClient          = SSL_CVERIFY_UNSET;
    sc->nSessionCacheTimeout   = UNSET;
    sc->nPassPhraseDialogType  = SSL_PPTYPE_UNSET;
    sc->szPassPhraseDialogPath = NULL;
    sc->nProtocol              = SSL_PROTOCOL_ALL;
    sc->fileLogFile            = NULL;
    sc->px509Certificate       = NULL;
    sc->prsaKey                = NULL;
    sc->pSSLCtx                = NULL;

    return sc;
}

/*
 *  Merge per-server SSL configurations
 */
void *ssl_config_server_merge(pool *p, void *basev, void *addv)
{
    SSLSrvConfigRec *base = (SSLSrvConfigRec *)basev;
    SSLSrvConfigRec *add  = (SSLSrvConfigRec *)addv;
    SSLSrvConfigRec *new  = (SSLSrvConfigRec *)ap_palloc(p, sizeof(SSLSrvConfigRec));

    cfgMergeBool(bEnabled);
    cfgMergeString(szCertificateFile);
    cfgMergeString(szKeyFile);
    cfgMergeString(szCACertificatePath);
    cfgMergeString(szCACertificateFile);
    cfgMergeString(szLogFile);
    cfgMergeString(szCipherSuite);
    cfgMerge(nLogLevel, SSL_LOG_NONE);
    cfgMergeInt(nVerifyDepth);
    cfgMerge(nVerifyClient, SSL_CVERIFY_UNSET);
    cfgMergeInt(nSessionCacheTimeout);
    cfgMerge(nPassPhraseDialogType, SSL_PPTYPE_UNSET);
    cfgMergeString(szPassPhraseDialogPath);
    cfgMerge(nProtocol, SSL_PROTOCOL_ALL);
    cfgMerge(fileLogFile, NULL);
    cfgMerge(px509Certificate, NULL);
    cfgMerge(prsaKey, NULL);
    cfgMerge(pSSLCtx, NULL);

    return new;
}

/*
 *  Create per-directory SSL configuration
 */
void *ssl_config_perdir_create(pool *p, char *dir)
{
    SSLDirConfigRec *dc = ap_palloc(p, sizeof(SSLDirConfigRec));

    dc->bSSLRequired  = FALSE;
    dc->aRequirement  = ap_make_array(p, 4, sizeof(ssl_require_t));
    dc->nOptions      = SSL_OPT_NONE|SSL_OPT_RELSET;
    dc->nOptionsAdd   = SSL_OPT_NONE;
    dc->nOptionsDel   = SSL_OPT_NONE;

    dc->szCipherSuite          = NULL;
    dc->nVerifyClient          = SSL_CVERIFY_UNSET;
    dc->nVerifyDepth           = UNSET;
#ifdef SSL_EXPERIMENTAL
    dc->szCACertificatePath    = NULL;
    dc->szCACertificateFile    = NULL;
#endif

    return dc;
}

/*
 *  Merge per-directory SSL configurations
 */
void *ssl_config_perdir_merge(pool *p, void *basev, void *addv)
{
    SSLDirConfigRec *base = (SSLDirConfigRec *)basev;
    SSLDirConfigRec *add  = (SSLDirConfigRec *)addv;
    SSLDirConfigRec *new  = (SSLDirConfigRec *)ap_palloc(p,
                                               sizeof(SSLDirConfigRec));

    cfgMerge(bSSLRequired, FALSE);
    cfgMergeArray(aRequirement);

    if (add->nOptions & SSL_OPT_RELSET) {
        new->nOptionsAdd = (base->nOptionsAdd & ~(add->nOptionsDel)) | add->nOptionsAdd;
        new->nOptionsDel = (base->nOptionsDel & ~(add->nOptionsAdd)) | add->nOptionsDel;
        new->nOptions    = (base->nOptions    & ~(new->nOptionsDel)) | new->nOptionsAdd;
    }
    else {
        new->nOptions    = add->nOptions;
        new->nOptionsAdd = add->nOptionsAdd;
        new->nOptionsDel = add->nOptionsDel;
    }

    cfgMergeString(szCipherSuite);
    cfgMerge(nVerifyClient, SSL_CVERIFY_UNSET);
    cfgMergeInt(nVerifyDepth);
#ifdef SSL_EXPERIMENTAL
    cfgMergeString(szCACertificatePath);
    cfgMergeString(szCACertificateFile);
#endif

    return new;
}

/*
 * Directive Rewriting
 */

char *ssl_hook_RewriteCommand(cmd_parms *cmd, void *config, const char *cmd_line)
{
#ifdef SSL_COMPAT
    return ssl_compat_directive(cmd->server, cmd->pool, cmd_line);
#else
    return NULL;
#endif
}

/*
 *  Configuration functions for particular directives
 */

const char *ssl_cmd_SSLMutex(
    cmd_parms *cmd, char *struct_ptr, char *arg)
{
    const char *err;
    SSLModConfigRec *mc = myModConfig();

    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
        return err;
    if (ssl_config_global_isfixed())
        return NULL;
    if (strcEQ(arg, "none")) {
        mc->nMutexMode  = SSL_MUTEXMODE_NONE;
    }
    else if (strlen(arg) > 5 && strcEQn(arg, "file:", 5)) {
        mc->nMutexMode  = SSL_MUTEXMODE_FILE;
        mc->szMutexFile = ap_psprintf(mc->pPool, "%s.%lu",
                                      ap_server_root_relative(cmd->pool, arg+5), 
                                      (unsigned long)getpid());
    }
    else if (strcEQ(arg, "sem")) {
#ifdef SSL_CAN_USE_SEM
        mc->nMutexMode  = SSL_MUTEXMODE_SEM;
#else
        return "SSLMutex: Semaphores not available on this platform";
#endif
    }
    else
        return "SSLMutex: Invalid argument";
    return NULL;
}

const char *ssl_cmd_SSLPassPhraseDialog(
    cmd_parms *cmd, char *struct_ptr, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    const char *err;

    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
        return err;
    if (strcEQ(arg, "builtin")) {
        sc->nPassPhraseDialogType  = SSL_PPTYPE_BUILTIN;
        sc->szPassPhraseDialogPath = NULL;
    }
    else if (strlen(arg) > 5 && strEQn(arg, "exec:", 5)) {
        sc->nPassPhraseDialogType  = SSL_PPTYPE_FILTER;
        sc->szPassPhraseDialogPath = ap_server_root_relative(cmd->pool, arg+5);
        if (!ssl_util_path_check(SSL_PCM_EXISTS, sc->szPassPhraseDialogPath))
            return ap_pstrcat(cmd->pool, "SSLPassPhraseDialog: file '",
                              sc->szPassPhraseDialogPath, "' not exists", NULL);
    }
    else
        return "SSLPassPhraseDialog: Invalid argument";
    return NULL;
}

const char *ssl_cmd_SSLRandomSeed(
    cmd_parms *cmd, char *struct_ptr, char *arg1, char *arg2, char *arg3)
{
    SSLModConfigRec *mc = myModConfig();
    const char *err;
    ssl_randseed_t *pRS;

    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
        return err;
    if (ssl_config_global_isfixed())
        return NULL;
    pRS = ap_push_array(mc->aRandSeed);
    if (strcEQ(arg1, "startup"))
        pRS->nCtx = SSL_RSCTX_STARTUP;
    else if (strcEQ(arg1, "connect"))
        pRS->nCtx = SSL_RSCTX_CONNECT;
    else
        return ap_pstrcat(cmd->pool, "SSLRandomSeed: "
                          "invalid context: `", arg1, "'");
    if (strlen(arg2) > 5 && strEQn(arg2, "file:", 5)) {
        pRS->nSrc   = SSL_RSSRC_FILE;
        pRS->cpPath = ap_pstrdup(mc->pPool, ap_server_root_relative(cmd->pool, arg2+5));
    }
    else if (strlen(arg2) > 5 && strEQn(arg2, "exec:", 5)) {
        pRS->nSrc   = SSL_RSSRC_EXEC;
        pRS->cpPath = ap_pstrdup(mc->pPool, ap_server_root_relative(cmd->pool, arg2+5));
    }
    else if (strcEQ(arg2, "builtin")) {
        pRS->nSrc   = SSL_RSSRC_BUILTIN;
        pRS->cpPath = NULL;
    }
    else {
        pRS->nSrc   = SSL_RSSRC_FILE;
        pRS->cpPath = ap_pstrdup(mc->pPool, ap_server_root_relative(cmd->pool, arg2));
    }
    if (pRS->nSrc != SSL_RSSRC_BUILTIN)
        if (!ssl_util_path_check(SSL_PCM_EXISTS, pRS->cpPath))
            return ap_pstrcat(cmd->pool, "SSLRandomSeed: source path '",
                              pRS->cpPath, "' not exists", NULL);
    if (arg3 == NULL) 
        pRS->nBytes = 0; /* read whole file */
    else {
        if (pRS->nSrc == SSL_RSSRC_BUILTIN)
            return "SSLRandomSeed: byte specification not "
                   "allowd for builtin seed source";
        pRS->nBytes = atoi(arg3);
        if (pRS->nBytes < 0)
            return "SSLRandomSeed: invalid number of bytes specified";
    }
    return NULL;
}

const char *ssl_cmd_SSLEngine(
    cmd_parms *cmd, char *struct_ptr, int flag)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);

    sc->bEnabled = (flag ? TRUE : FALSE);
    return NULL;
}

const char *ssl_cmd_SSLCipherSuite(
    cmd_parms *cmd, SSLDirConfigRec *dc, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);

    if (cmd->path == NULL || dc == NULL)
        sc->szCipherSuite = arg;
    else
        dc->szCipherSuite = arg;
    return NULL;
}

const char *ssl_cmd_SSLCertificateFile(
    cmd_parms *cmd, char *struct_ptr, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    char *cpPath;

    cpPath = ap_server_root_relative(cmd->pool, arg);
    if (!ssl_util_path_check(SSL_PCM_EXISTS|SSL_PCM_ISREG|SSL_PCM_ISNONZERO, cpPath))
        return ap_pstrcat(cmd->pool, "SSLCertificateFile: file '",
                          cpPath, "' not exists or empty", NULL);
    sc->szCertificateFile = cpPath;
    return NULL;
}

const char *ssl_cmd_SSLCertificateKeyFile(
    cmd_parms *cmd, char *struct_ptr, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    char *cpPath;

    cpPath = ap_server_root_relative(cmd->pool, arg);
    if (!ssl_util_path_check(SSL_PCM_EXISTS|SSL_PCM_ISREG|SSL_PCM_ISNONZERO, cpPath))
        return ap_pstrcat(cmd->pool, "SSLCertificateKeyFile: file '",
                          cpPath, "' not exists or empty", NULL);
    sc->szKeyFile = cpPath;
    return NULL;
}

const char *ssl_cmd_SSLCACertificatePath(
    cmd_parms *cmd, SSLDirConfigRec *dc, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    char *cpPath;

    cpPath = ap_server_root_relative(cmd->pool, arg);
    if (!ssl_util_path_check(SSL_PCM_EXISTS|SSL_PCM_ISDIR, cpPath))
        return ap_pstrcat(cmd->pool, "SSLCACertificatePath: directory '",
                          cpPath, "' not exists", NULL);
#ifdef SSL_EXPERIMENTAL
    if (cmd->path == NULL || dc == NULL)
        sc->szCACertificatePath = cpPath;
    else
        dc->szCACertificatePath = cpPath;
#else
    sc->szCACertificatePath = cpPath;
#endif
    return NULL;
}

const char *ssl_cmd_SSLCACertificateFile(
    cmd_parms *cmd, SSLDirConfigRec *dc, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    char *cpPath;

    cpPath = ap_server_root_relative(cmd->pool, arg);
    if (!ssl_util_path_check(SSL_PCM_EXISTS|SSL_PCM_ISREG|SSL_PCM_ISNONZERO, cpPath))
        return ap_pstrcat(cmd->pool, "SSLCACertificateKeyFile: file '",
                          cpPath, "' not exists or empty", NULL);
#ifdef SSL_EXPERIMENTAL
    if (cmd->path == NULL || dc == NULL)
        sc->szCACertificateFile = cpPath;
    else
        dc->szCACertificateFile = cpPath;
#else
    sc->szCACertificateFile = cpPath;
#endif
    return NULL;
}

const char *ssl_cmd_SSLVerifyClient(
    cmd_parms *cmd, SSLDirConfigRec *dc, char *level)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    int id;

    if (strEQ(level, "0") || strcEQ(level, "none"))
        id = SSL_CVERIFY_NONE;
    else if (strEQ(level, "1") || strcEQ(level, "optional"))
        id = SSL_CVERIFY_OPTIONAL;
    else if (strEQ(level, "2") || strcEQ(level, "require"))
        id = SSL_CVERIFY_REQUIRE;
    else if (strEQ(level, "3") || strcEQ(level, "optional_no_ca"))
        id = SSL_CVERIFY_OPTIONAL_NO_CA;
    else
        return "SSLVerifyClient: Invalid argument";
    if (cmd->path == NULL || dc == NULL)
        sc->nVerifyClient = id;
    else
        dc->nVerifyClient = id;
    return NULL;
}

const char *ssl_cmd_SSLVerifyDepth(
    cmd_parms *cmd, SSLDirConfigRec *dc, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    int d;

    d = atoi(arg);
    if (d < 0)
        return "SSLVerifyDepth: Invalid argument";
    if (cmd->path == NULL || dc == NULL)
        sc->nVerifyDepth = d;
    else
        dc->nVerifyDepth = d;
    return NULL;
}

const char *ssl_cmd_SSLSessionCache(
    cmd_parms *cmd, char *struct_ptr, char *arg)
{
    const char *err;
    SSLModConfigRec *mc = myModConfig();

    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
        return err;
    if (ssl_config_global_isfixed())
        return NULL;
    if (strcEQ(arg, "none")) {
        mc->nSessionCacheMode      = SSL_SCMODE_NONE;
        mc->szSessionCacheDataFile = NULL;
    }
    else if (strlen(arg) > 4 && strcEQn(arg, "dbm:", 4)) {
        mc->nSessionCacheMode      = SSL_SCMODE_DBM;
        mc->szSessionCacheDataFile = ap_pstrdup(mc->pPool,
                                     ap_server_root_relative(cmd->pool, arg+4));
    }
    else
        return "SSLSessionCache: Invalid argument";
    return NULL;
}

const char *ssl_cmd_SSLSessionCacheTimeout(
    cmd_parms *cmd, char *struct_ptr, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);

    sc->nSessionCacheTimeout = atoi(arg);
    if (sc->nSessionCacheTimeout < 0)
        return "SSLSessionCacheTimeout: Invalid argument";
    return NULL;
}

const char *ssl_cmd_SSLLog(
    cmd_parms *cmd, char *struct_ptr, char *arg)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    const char *err;

    if ((err = ap_check_cmd_context(cmd,  NOT_IN_LIMIT|NOT_IN_DIRECTORY
                                         |NOT_IN_LOCATION|NOT_IN_FILES )) != NULL)
        return err;
    sc->szLogFile = arg;
    return NULL;
}

const char *ssl_cmd_SSLLogLevel(
    cmd_parms *cmd, char *struct_ptr, char *level)
{
    SSLSrvConfigRec *sc = mySrvConfig(cmd->server);
    const char *err;

    if ((err = ap_check_cmd_context(cmd,  NOT_IN_LIMIT|NOT_IN_DIRECTORY
                                         |NOT_IN_LOCATION|NOT_IN_FILES )) != NULL)
        return err;
    if (strcEQ(level, "none"))
        sc->nLogLevel = SSL_LOG_NONE;
    else if (strcEQ(level, "error"))
        sc->nLogLevel = SSL_LOG_ERROR;
    else if (strcEQ(level, "warn"))
        sc->nLogLevel = SSL_LOG_WARN;
    else if (strcEQ(level, "info"))
        sc->nLogLevel = SSL_LOG_INFO;
    else if (strcEQ(level, "trace"))
        sc->nLogLevel = SSL_LOG_TRACE;
    else if (strcEQ(level, "debug"))
        sc->nLogLevel = SSL_LOG_DEBUG;
    else
        return "SSLLogLevel: Invalid argument";
    return NULL;
}

const char *ssl_cmd_SSLOptions(
    cmd_parms *cmd, SSLDirConfigRec *dc, const char *cpLine)
{
    ssl_opt_t opt;
    int first;
    char action;
    char *w;

    first = TRUE;
    while (cpLine[0] != NUL) {
        w = ap_getword_conf(cmd->pool, &cpLine);
        action = NUL;

        if (*w == '+' || *w == '-') {
            action = *(w++);
        }
        else if (first) {
            dc->nOptions = SSL_OPT_NONE;
            first = FALSE;
        }

        if (strcEQ(w, "CompatEnvVars"))
            opt = SSL_OPT_COMPATENVVARS;
        else if (strcEQ(w, "ExportCertData"))
            opt = SSL_OPT_EXPORTCERTDATA;
        else if (strcEQ(w, "FakeBasicAuth"))
            opt = SSL_OPT_FAKEBASICAUTH;
        else
            return ap_pstrcat(cmd->pool, "SSLOptions: Illegal option '", w, "'", NULL);

        if (action == '-') {
            dc->nOptionsAdd &= ~opt;
            dc->nOptionsDel |=  opt;
            dc->nOptions    &= ~opt;
        }
        else if (action == '+') {
            dc->nOptionsAdd |=  opt;
            dc->nOptionsDel &= ~opt;
            dc->nOptions    |=  opt;
        }
        else {
            dc->nOptions    = opt;
            dc->nOptionsAdd = opt;
            dc->nOptionsDel = SSL_OPT_NONE;
        }
    }
    return NULL;
}

const char *ssl_cmd_SSLRequireSSL(
    cmd_parms *cmd, SSLDirConfigRec *dc, char *cipher)
{
    dc->bSSLRequired = TRUE;
    return NULL;
}

const char *ssl_cmd_SSLRequire(
    cmd_parms *cmd, SSLDirConfigRec *dc, char *cpExpr)
{
    ssl_expr *mpExpr;
    ssl_require_t *pReqRec;

    if ((mpExpr = ssl_expr_comp(cmd->pool, cpExpr)) == NULL)
        return ap_pstrcat(cmd->pool, "SSLRequire: ", ssl_expr_get_error(), NULL);
    pReqRec = ap_push_array(dc->aRequirement);
    pReqRec->cpExpr = ap_pstrdup(cmd->pool, cpExpr);
    pReqRec->mpExpr = mpExpr;
    return NULL;
}

const char *ssl_cmd_SSLProtocol(
    cmd_parms *cmd, char *struct_ptr, const char *opt)
{
    SSLSrvConfigRec *sc;
    ssl_proto_t options, thisopt;
    char action;
    char *w;

    sc = mySrvConfig(cmd->server);
    options = SSL_PROTOCOL_NONE;
    while (opt[0] != NUL) {
        w = ap_getword_conf(cmd->pool, &opt);

        action = NUL;
        if (*w == '+' || *w == '-')
            action = *(w++);

        if (strcEQ(w, "SSLv2"))
            thisopt = SSL_PROTOCOL_SSLV2;
        else if (strcEQ(w, "SSLv3"))
            thisopt = SSL_PROTOCOL_SSLV3;
        else if (strcEQ(w, "TLSv1"))
            thisopt = SSL_PROTOCOL_TLSV1;
        else if (strcEQ(w, "all"))
            thisopt = SSL_PROTOCOL_ALL;
        else
            return ap_pstrcat(cmd->pool, "SSLProtocol: Illegal protocol '", w, "'", NULL);

        if (action == '-')
            options &= ~thisopt;
        else if (action == '+')
            options |= thisopt;
        else
            options = thisopt;
    }
    sc->nProtocol = options;
    return NULL;
}

