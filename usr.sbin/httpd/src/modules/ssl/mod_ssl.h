/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |
** | '_ ` _ \ / _ \ / _` |   / __/ __| |
** | | | | | | (_) | (_| |   \__ \__ \ | mod_ssl - Apache Interface to SSLeay
** |_| |_| |_|\___/ \__,_|___|___/___/_| http://www.engelschall.com/sw/mod_ssl/
**                      |_____|
**  mod_ssl.h
**  Global header
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
                             /* ``The Apache Group: a collection
                                  of talented individuals who are
                                  trying to perfect the art of
                                  never finishing something.''
                                             -- Rob Hartill         */
#ifndef MOD_SSL_H
#define MOD_SSL_H 1

/*
 * Power up our brain...
 */

/* OS headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>

/* SSLeay headers */
#include <ssl.h>
#include <err.h>
#include <x509.h>
#include <pem.h>
#include <crypto.h>
#include <evp.h>
#include <rand.h>

/* Apache headers */
#define CORE_PRIVATE
#include "ap_config.h"
#include "httpd.h"
#include "http_config.h"
#include "http_conf_globals.h"
#include "http_protocol.h"
#include "http_main.h"
#include "http_core.h"
#include "http_log.h"
#include "scoreboard.h"
#include "fnmatch.h"
#undef CORE_PRIVATE

/* mod_ssl headers */
#include "ssl_expr.h"
#include "ssl_util_ssl.h"

/*
 * Provide reasonable default for some defines
 */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  !FALSE
#endif
#ifndef UNSET
#define UNSET -1
#endif
#ifndef NUL
#define NUL   '\0'
#endif

/*
 * Provide reasonable defines for some types
 */
#ifndef BOOL
#define BOOL unsigned int
#endif
#ifndef UCHAR
#define UCHAR unsigned char
#endif

/*
 * Provide useful shorthands
 */
#define strEQ(s1,s2)     (strcmp(s1,s2)        == 0)
#define strNE(s1,s2)     (strcmp(s1,s2)        != 0)
#define strEQn(s1,s2,n)  (strncmp(s1,s2,n)     == 0)
#define strNEn(s1,s2,n)  (strncmp(s1,s2,n)     != 0)

#define strcEQ(s1,s2)    (strcasecmp(s1,s2)    == 0)
#define strcNE(s1,s2)    (strcasecmp(s1,s2)    != 0)
#define strcEQn(s1,s2,n) (strncasecmp(s1,s2,n) == 0)
#define strcNEn(s1,s2,n) (strncasecmp(s1,s2,n) != 0)

#define strIsEmpty(s)    (s == NULL || s[0] == NUL)

#define cfgMerge(el,unset)  new->el = add->el == unset ? base->el : add->el
#define cfgMergeArray(el)   new->el = ap_append_arrays(p, add->el, base->el)
#define cfgMergeTable(el)   new->el = ap_overlay_tables(p, add->el, base->el)
#define cfgMergeString(el)  cfgMerge(el, NULL)
#define cfgMergeBool(el)    cfgMerge(el, UNSET)
#define cfgMergeInt(el)     cfgMerge(el, UNSET)

#define myModConfig()    (SSLModConfigRec *)ap_ctx_get(ap_global_ctx, "ssl_module")
#define mySrvConfig(srv) (SSLSrvConfigRec *)ap_get_module_config(srv->module_config,  &ssl_module)
#define myDirConfig(req) (SSLDirConfigRec *)ap_get_module_config(req->per_dir_config, &ssl_module)

#define myCtxVarSet(mc,num,val)  mc->rCtx.pV##num = val
#define myCtxVarGet(mc,num,type) (type)(mc->rCtx.pV##num)

#define AP_ALL_CMD(name, args, desc) \
        { "SSL"#name, ssl_cmd_SSL##name, NULL, RSRC_CONF|OR_AUTHCFG, args, desc },
#define AP_SRV_CMD(name, args, desc) \
        { "SSL"#name, ssl_cmd_SSL##name, NULL, RSRC_CONF, args, desc },
#define AP_DIR_CMD(name, type, args, desc) \
        { "SSL"#name, ssl_cmd_SSL##name, NULL, OR_##type, args, desc },
#define AP_END_CMD \
        { NULL }

/*
 * SSL Logging
 */
#define SSL_LOG_NONE    (1<<0)
#define SSL_LOG_ERROR   (1<<1)
#define SSL_LOG_WARN    (1<<2)
#define SSL_LOG_INFO    (1<<3)
#define SSL_LOG_TRACE   (1<<4)
#define SSL_LOG_DEBUG   (1<<5)
#define SSL_LOG_MASK    (SSL_LOG_ERROR|SSL_LOG_WARN|SSL_LOG_INFO|SSL_LOG_TRACE|SSL_LOG_DEBUG)

#define SSL_ADD_NONE     (1<<8)
#define SSL_ADD_ERRNO    (1<<9)
#define SSL_ADD_SSLERR   (1<<10)
#define SSL_NO_TIMESTAMP (1<<11)
#define SSL_NO_LEVELID   (1<<12)
#define SSL_NO_NEWLINE   (1<<13)

/*
 * Defaults for the configuration
 */

#ifndef SSL_SESSION_CACHE_TIMEOUT
#define SSL_SESSION_CACHE_TIMEOUT  300
#endif

/*
 * Support for file locking: Try to determine whether we should use fcntl() or
 * flock().  Would be better ap_config.h could provide this... :-(
  */
#if defined(USE_FCNTL_SERIALIZED_ACCEPT)
#define SSL_USE_FCNTL 1
#include <fcntl.h>
#endif
#if defined(USE_FLOCK_SERIALIZED_ACCEPT)
#define SSL_USE_FLOCK 1
#include <sys/file.h>
#endif
#if !defined(SSL_USE_FCNTL) && !defined(SSL_USE_FLOCK)
#define SSL_USE_FLOCK 1
#if !defined(MPE) && !defined(WIN32)
#include <sys/file.h>
#endif
#ifndef LOCK_UN
#undef SSL_USE_FLOCK
#define SSL_USE_FCNTL 1
#include <fcntl.h>
#endif
#endif
#ifdef AIX
#undef SSL_USE_FLOCK
#define SSL_USE_FCNTL 1
#include <fcntl.h>
#endif

/*
 * Support for Mutex
 */
#ifndef WIN32
#define SSL_MUTEX_LOCK_MODE ( S_IRUSR|S_IWUSR )
#else
#define SSL_MUTEX_LOCK_MODE (_S_IREAD|_S_IWRITE )
#endif
#ifdef USE_SYSVSEM_SERIALIZED_ACCEPT
#define SSL_CAN_USE_SEM
#define SSL_HAVE_IPCSEM
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#endif
#ifdef WIN32
#define SSL_CAN_USE_SEM
#define SSL_HAVE_W32SEM
#include "multithread.h"
#include <process.h>
#endif

/*
 * Support for DBM library
 */
#ifndef WIN32
#define SSL_DBM_FILE_MODE ( S_IRUSR|S_IWUSR )
#else
#define SSL_USE_SDBM
#define SSL_DBM_FILE_MODE ( _S_IREAD|_S_IWRITE )
#endif

#ifdef SSL_USE_SDBM
#include "ssl_util_sdbm.h"
#define ssl_dbm_open     sdbm_open
#define ssl_dbm_close    sdbm_close
#define ssl_dbm_store    sdbm_store
#define ssl_dbm_fetch    sdbm_fetch
#define ssl_dbm_delete   sdbm_delete
#define ssl_dbm_firstkey sdbm_firstkey
#define ssl_dbm_nextkey  sdbm_nextkey
#define SSL_DBM_FILE_SUFFIX_DIR ".dir"
#define SSL_DBM_FILE_SUFFIX_PAG ".pag"
#else
#include <ndbm.h>
#define ssl_dbm_open     dbm_open
#define ssl_dbm_close    dbm_close
#define ssl_dbm_store    dbm_store
#define ssl_dbm_fetch    dbm_fetch
#define ssl_dbm_delete   dbm_delete
#define ssl_dbm_firstkey dbm_firstkey
#define ssl_dbm_nextkey  dbm_nextkey
#if defined(__FreeBSD__) || (defined(DB_LOCK) && defined(DB_SHMEM))
#define SSL_DBM_FILE_SUFFIX_DIR ".db"
#define SSL_DBM_FILE_SUFFIX_PAG ".db"
#else
#define SSL_DBM_FILE_SUFFIX_DIR ".dir"
#define SSL_DBM_FILE_SUFFIX_PAG ".pag"
#endif
#endif

/*
 * Check for SSLeay 0.9.0 and below
 * and enabled Extended API (EAPI)
 */
#if SSL_LIBRARY_VERSION < 0x0900
#error "SSLeay versions below 0.9.0 are no longer supported"
#endif
#ifndef EAPI
#error "mod_ssl requires Extended API (EAPI)"
#endif

/*
 * The own data structures
 */
typedef struct {
    pool *pPool;
    pool *pSubPool;
    array_header *aData;
} ssl_ds_array;

typedef struct {
    pool *pPool;
    pool *pSubPool;
    array_header *aKey;
    array_header *aData;
} ssl_ds_table;

/*
 * Define the SSL options
 */
#define SSL_OPT_NONE           (0)
#define SSL_OPT_RELSET         (1<<0)
#define SSL_OPT_COMPATENVVARS  (1<<1)
#define SSL_OPT_EXPORTCERTDATA (1<<2)
#define SSL_OPT_FAKEBASICAUTH  (1<<3)
#define SSL_OPT_ALL            (SSL_OPT_COMPATENVVAR|SSL_OPT_EXPORTCERTDATA|SSL_OPT_FAKEBASICAUTH)
typedef int ssl_opt_t;

/*
 * Define the SSL Protocol options
 */

#define SSL_PROTOCOL_NONE  (0)
#define SSL_PROTOCOL_SSLV2 (1<<0)
#define SSL_PROTOCOL_SSLV3 (1<<1)
#define SSL_PROTOCOL_TLSV1 (1<<2)
#define SSL_PROTOCOL_ALL   (SSL_PROTOCOL_SSLV2|SSL_PROTOCOL_SSLV3|SSL_PROTOCOL_TLSV1)
typedef int ssl_proto_t;

/*
 * Define the SSL verify levels
 */
typedef enum {
    SSL_CVERIFY_UNSET           = UNSET,
    SSL_CVERIFY_NONE            = 0,
    SSL_CVERIFY_OPTIONAL        = 1,
    SSL_CVERIFY_REQUIRE         = 2,
    SSL_CVERIFY_OPTIONAL_NO_CA  = 3
} ssl_verify_t;

/*
 * Define the SSL pass phrase dialog types
 */
typedef enum {
    SSL_PPTYPE_UNSET   = UNSET,
    SSL_PPTYPE_BUILTIN = 0,
    SSL_PPTYPE_FILTER  = 1
} ssl_pphrase_t;

/*
 * Define the Path Checking modes
 */
#define SSL_PCM_EXISTS     1
#define SSL_PCM_ISREG      2
#define SSL_PCM_ISDIR      4
#define SSL_PCM_ISNONZERO  8
typedef unsigned int ssl_pathcheck_t;

/*
 * Define the SSL session cache modes and structures
 */
typedef enum {
    SSL_SCMODE_UNSET = UNSET,
    SSL_SCMODE_NONE  = 0,
    SSL_SCMODE_DBM   = 1,
    SSL_SCMODE_SHM   = 2
} ssl_scmode_t;

typedef struct {
    UCHAR *ucaKey;
    int    nKey;
    UCHAR *ucaData;
    int    nData;
    time_t tExpiresAt;
} ssl_scinfo_t;

/*
 * Define the SSL mutex modes
 */
typedef enum {
    SSL_MUTEXMODE_UNSET  = UNSET,
    SSL_MUTEXMODE_NONE   = 0,
    SSL_MUTEXMODE_FILE   = 1,
    SSL_MUTEXMODE_SEM    = 2
} ssl_mutexmode_t;

/*
 * Define the SSL requirement structure
 */
typedef struct {
    char     *cpExpr;
    ssl_expr *mpExpr;
} ssl_require_t;

/*
 * Define the SSL random number generator seeding source
 */
typedef enum {
    SSL_RSCTX_STARTUP = 1,
    SSL_RSCTX_CONNECT = 2
} ssl_rsctx_t;
typedef enum {
    SSL_RSSRC_BUILTIN = 1,
    SSL_RSSRC_FILE    = 2,
    SSL_RSSRC_EXEC    = 3
} ssl_rssrc_t;
typedef struct {
    ssl_rsctx_t  nCtx;
    ssl_rssrc_t  nSrc;
    char        *cpPath;
    int          nBytes;
} ssl_randseed_t;

/*
 * Define the structure of an ASN.1 anything
 */
typedef struct {
    long int       nData;
    unsigned char *cpData;
} ssl_asn1_t;

/*
 * Define the mod_ssl per-module configuration structure
 * (i.e. the global configuration for each httpd process)
 */

typedef struct {
    pool           *pPool;
    BOOL            bFixed;

    /*
     * global config data
     */
    int             nInitCount;

    RSA            *pRSATmpKey;
    int             nSessionCacheMode;
    char           *szSessionCacheDataFile;
    ssl_mutexmode_t nMutexMode;
    char           *szMutexFile;
    int             nMutexFD;
    int             nMutexSEMID;
    array_header   *aRandSeed;

    ssl_ds_table   *tPublicCert;
    ssl_ds_table   *tPrivateKey;

    /*
     * arbitrary global context data
     */
    struct {
        void *pV1, *pV2, *pV3, *pV4, *pV5, *pV6, *pV7, *pV8, *pV9;
    } rCtx;
} SSLModConfigRec;

/*
 * Define the mod_ssl per-server configuration structure
 * (i.e. the configuration for the main server
 *  and all <VirtualHost> contexts)
 */
typedef struct {
    BOOL         bEnabled;
    char        *szCertificateFile;
    char        *szKeyFile;
    char        *szCACertificatePath;
    char        *szCACertificateFile;
    char        *szLogFile;
    char        *szCipherSuite;
    FILE        *fileLogFile;
    int          nLogLevel;
    int          nVerifyDepth;
    ssl_verify_t nVerifyClient;
    X509        *px509Certificate;
    RSA         *prsaKey;
    SSL_CTX     *pSSLCtx;
    int          nSessionCacheTimeout;
    int          nPassPhraseDialogType;
    char        *szPassPhraseDialogPath;
    ssl_proto_t  nProtocol;
} SSLSrvConfigRec;

/*
 * Define the mod_ssl per-directory configuration structure
 * (i.e. the local configuration for all <Directory>
 *  and .htaccess contexts)
 */
typedef struct {
    BOOL          bSSLRequired;
    array_header *aRequirement;
    ssl_opt_t     nOptions;
    ssl_opt_t     nOptionsAdd;
    ssl_opt_t     nOptionsDel;
    char         *szCipherSuite;
    ssl_verify_t  nVerifyClient;
    int           nVerifyDepth;
#ifdef SSL_EXPERIMENTAL
    char         *szCACertificatePath;
    char         *szCACertificateFile;
#endif
} SSLDirConfigRec;

/*
 *  function prototypes
 */

/*  API glue structures  */
extern module MODULE_VAR_EXPORT ssl_module;

/*  configuration handling   */
void         ssl_config_global_create(void);
void         ssl_config_global_fix(void);
BOOL         ssl_config_global_isfixed(void);
void        *ssl_config_server_create(pool *, server_rec *);
void        *ssl_config_server_merge(pool *, void *, void *);
void        *ssl_config_perdir_create(pool *, char *);
void        *ssl_config_perdir_merge(pool *, void *, void *);
const char  *ssl_cmd_SSLMutex(cmd_parms *, char *, char *);
const char  *ssl_cmd_SSLPassPhraseDialog(cmd_parms *, char *, char *);
const char  *ssl_cmd_SSLRandomSeed(cmd_parms *, char *, char *, char *, char *);
const char  *ssl_cmd_SSLEngine(cmd_parms *, char *, int flag);
const char  *ssl_cmd_SSLCipherSuite(cmd_parms *, SSLDirConfigRec *, char *);
const char  *ssl_cmd_SSLCertificateFile(cmd_parms *, char *, char *);
const char  *ssl_cmd_SSLCertificateKeyFile(cmd_parms *, char *, char *);
const char  *ssl_cmd_SSLCACertificatePath(cmd_parms *, SSLDirConfigRec *, char *);
const char  *ssl_cmd_SSLCACertificateFile(cmd_parms *, SSLDirConfigRec *, char *);
const char  *ssl_cmd_SSLVerifyClient(cmd_parms *, SSLDirConfigRec *, char *level);
const char  *ssl_cmd_SSLVerifyDepth(cmd_parms *, SSLDirConfigRec *, char *);
const char  *ssl_cmd_SSLSessionCache(cmd_parms *, char *, char *);
const char  *ssl_cmd_SSLSessionCacheTimeout(cmd_parms *, char *, char *);
const char  *ssl_cmd_SSLLog(cmd_parms *, char *, char *);
const char  *ssl_cmd_SSLLogLevel(cmd_parms *, char *, char *);
const char  *ssl_cmd_SSLProtocol(cmd_parms *, char *, const char *);
const char  *ssl_cmd_SSLOptions(cmd_parms *, SSLDirConfigRec *, const char *);
const char  *ssl_cmd_SSLRequireSSL(cmd_parms *, SSLDirConfigRec *, char *);
const char  *ssl_cmd_SSLRequire(cmd_parms *, SSLDirConfigRec *, char *);

/*  module initialization  */
void         ssl_init_Module(server_rec *, pool *);
void         ssl_init_SSLLibrary(server_rec *);
void         ssl_init_GetCertAndKey(server_rec *, pool *, SSLSrvConfigRec *);
STACK       *ssl_init_FindCAList(server_rec *, pool *, char *, char *);
void         ssl_init_Child(server_rec *, pool *);

/*  Apache API hooks  */
void         ssl_hook_AddModule(module *);
void         ssl_hook_RemoveModule(module *);
char        *ssl_hook_RewriteCommand(cmd_parms *, void *config, const char *);
void         ssl_hook_NewConnection(conn_rec *);
void         ssl_hook_TimeoutConnection(int);
void         ssl_hook_CloseConnection(void *);
int          ssl_hook_Auth(request_rec *);
int          ssl_hook_Access(request_rec *);
int          ssl_hook_Fixup(request_rec *);
int          ssl_hook_ReadReq(request_rec *);
int          ssl_hook_Handler(request_rec *);

/*  SSLeay callbacks */
RSA         *ssl_callback_TmpRSA(SSL *, int);
int          ssl_callback_SSLVerify(int, X509_STORE_CTX *);
int          ssl_callback_NewSessionCacheEntry(SSL *, SSL_SESSION *);
SSL_SESSION *ssl_callback_GetSessionCacheEntry(SSL *, unsigned char *, int, int *);
void         ssl_callback_DelSessionCacheEntry(SSL_CTX *, SSL_SESSION *);
void         ssl_callback_LogTracingState(SSL *, int, int);

/*  Session Cache Support  */
void         ssl_scache_init(server_rec *, pool *);
void         ssl_scache_store(server_rec *, SSL_SESSION *, int);
SSL_SESSION *ssl_scache_retrieve(server_rec *, UCHAR *, int);
void         ssl_scache_remove(server_rec *, SSL_SESSION *);
void         ssl_scache_expire(server_rec *);
char        *ssl_scache_id2sz(UCHAR *, int);
void         ssl_scache_dbm_init(server_rec *, pool *);
void         ssl_scache_dbm_store(server_rec *, ssl_scinfo_t *);
void         ssl_scache_dbm_retrieve(server_rec *, ssl_scinfo_t *);
void         ssl_scache_dbm_remove(server_rec *, ssl_scinfo_t *);
void         ssl_scache_dbm_expire(server_rec *);

/*  Pass Phrase Support  */
void         ssl_pphrase_Handle(server_rec *, pool *);
int          ssl_pphrase_Handle_CB(char *, int, int);

/*  Data Structures */
ssl_ds_array *ssl_ds_array_make(pool *, int);
BOOL          ssl_ds_array_isempty(ssl_ds_array *);
void         *ssl_ds_array_push(ssl_ds_array *);
void         *ssl_ds_array_get(ssl_ds_array *, int);
void          ssl_ds_array_wipeout(ssl_ds_array *);
void          ssl_ds_array_kill(ssl_ds_array *);
ssl_ds_table *ssl_ds_table_make(pool *, int);
BOOL          ssl_ds_table_isempty(ssl_ds_table *);
void         *ssl_ds_table_push(ssl_ds_table *, char *);
void         *ssl_ds_table_get(ssl_ds_table *, char *);
void          ssl_ds_table_wipeout(ssl_ds_table *);
void          ssl_ds_table_kill(ssl_ds_table *);

/*  Mutex Support  */
void         ssl_mutex_init(server_rec *, pool *);
void         ssl_mutex_open(server_rec *, pool *);
void         ssl_mutex_on(void);
void         ssl_mutex_off(void);
void         ssl_mutex_file_create(server_rec *, pool *);
void         ssl_mutex_file_open(server_rec *, pool *);
void         ssl_mutex_file_remove(void *);
BOOL         ssl_mutex_file_acquire(void);
BOOL         ssl_mutex_file_release(void);
void         ssl_mutex_sem_create(server_rec *, pool *);
void         ssl_mutex_sem_open(server_rec *, pool *);
void         ssl_mutex_sem_remove(void *);
BOOL         ssl_mutex_sem_acquire(void);
BOOL         ssl_mutex_sem_release(void);

/*  Logfile Support  */
void         ssl_log_open(server_rec *, pool *);
void         ssl_log(server_rec *, int, const char *, ...);
void         ssl_die(void);

/*  Variables  */
void         ssl_var_register(void);
void         ssl_var_unregister(void);
char        *ssl_var_lookup(pool *, server_rec *, conn_rec *, request_rec *, char *);

/*  I/O  */
void         ssl_io_register(void);
void         ssl_io_unregister(void);
long         ssl_io_data_cb(BIO *, int, char *, int, long, long);

/*  PRNG  */
int          ssl_rand_seed(server_rec *, pool *, ssl_rsctx_t);

/*  Extensions  */
void         ssl_ext_register(void);
void         ssl_ext_unregister(void);

/*  Compatibility  */
#ifdef SSL_COMPAT
char        *ssl_compat_directive(server_rec *, pool *, const char *);
void         ssl_compat_variables(request_rec *);
#endif

/*  Utility Functions  */
char        *ssl_util_vhostid(pool *, server_rec *);
void         ssl_util_strupper(char *);
void         ssl_util_uuencode(char *, const char *, BOOL);
void         ssl_util_uuencode_binary(unsigned char *, const unsigned char *, int, BOOL);
FILE        *ssl_util_ppopen(server_rec *, pool *, char *);
int          ssl_util_ppopen_child(void *, child_info *);
void         ssl_util_ppclose(server_rec *, pool *, FILE *);
char        *ssl_util_readfilter(server_rec *, pool *, char *);
BOOL         ssl_util_path_check(ssl_pathcheck_t, char *);
char        *ssl_util_ptxtsub(pool *, const char *, const char *, char *);
void         ssl_util_thread_setup(void);

#endif /* MOD_SSL_H */
