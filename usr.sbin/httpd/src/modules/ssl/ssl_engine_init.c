/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |
** | '_ ` _ \ / _ \ / _` |   / __/ __| |
** | | | | | | (_) | (_| |   \__ \__ \ | mod_ssl - Apache Interface to SSLeay
** |_| |_| |_|\___/ \__,_|___|___/___/_| http://www.engelschall.com/sw/mod_ssl/
**                      |_____|
**  ssl_engine_init.c
**  Initialization of Servers
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

/* ====================================================================
 * Copyright (c) 1995-1999 Ben Laurie. All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by Ben Laurie
 *    for use in the Apache-SSL HTTP server project."
 *
 * 4. The name "Apache-SSL Server" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 5. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Ben Laurie
 *    for use in the Apache-SSL HTTP server project."
 *
 * THIS SOFTWARE IS PROVIDED BY BEN LAURIE ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL BEN LAURIE OR
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
                             /* ``Recursive, adj.;
                                  see Recursive.''
                                        -- Unknown   */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  Module Initialization
**  _________________________________________________________________
*/

/*
 *  Per-module initialization
 */
void ssl_init_Module(server_rec *s, pool *p)
{
    SSLModConfigRec *mc = myModConfig();
    SSLSrvConfigRec *sc;
    server_rec *s2;
    char *cp;
    int n;

    mc->nInitCount++;

    /*
     * Any init round fixes the global config
     */
    ssl_config_global_create(); /* just to avoid problems */
    ssl_config_global_fix();

    /*
     *  try to fix the configuration and open the dedicated SSL
     *  logfile as early as possible
     */
    for (s2 = s; s2 != NULL; s2 = s2->next) {
        sc = mySrvConfig(s2);

        /* Fix up stuff that may not have been set */
        if (sc->bEnabled == UNSET)
            sc->bEnabled = FALSE;
        if (sc->nVerifyClient == SSL_CVERIFY_UNSET)
            sc->nVerifyClient = SSL_CVERIFY_NONE;
        if (sc->nVerifyDepth == UNSET)
            sc->nVerifyDepth = 1;
        if (sc->nSessionCacheTimeout == UNSET)
            sc->nSessionCacheTimeout = SSL_SESSION_CACHE_TIMEOUT;
        if (sc->nPassPhraseDialogType == SSL_PPTYPE_UNSET)
            sc->nPassPhraseDialogType = SSL_PPTYPE_BUILTIN;

        /* Open the dedicated SSL logfile */
        ssl_log_open(s2, p);
    }

    if (mc->nInitCount == 1)
        ssl_log(s, SSL_LOG_INFO, "Init: 1st startup round (still not detached)");
    else if (mc->nInitCount == 2)
        ssl_log(s, SSL_LOG_INFO, "Init: 2nd startup round (already detached)");
    else
        ssl_log(s, SSL_LOG_INFO, "Init: %d%s restart round (already detached)",
                mc->nInitCount-2, (mc->nInitCount-2) == 1 ? "st" : "nd");

    /*
     *  The initialization phase inside the Apache API is totally bogus.
     *  We actually have three non-trivial problems:
     *
     *  1. Under Unix the API does a 2-round initialization of modules while
     *     under Win32 it doesn't. This means we have to make sure that at
     *     least the pass phrase dialog doesn't occur twice.  We overcome this
     *     problem by using a counter (mc->nInitCount) which has to
     *     survive the init rounds.
     *
     *  2. Between the first and the second round Apache detaches from
     *     the terminal under Unix. This means that our pass phrase dialog
     *     _has_ to be done in the first round and _cannot_ be done in the
     *     second round.
     *
     *  3. When Dynamic Shared Object (DSO) mechanism is used under Unix the
     *     module segment (code & data) gets unloaded and re-loaded between
     *     the first and the second round. This means no global data survives
     *     between first and the second init round. We overcome this by using
     *     an entry ("ssl_module") inside the ap_global_ctx.
     *
     *  The situation as a table:
     *
     *  Unix/static Unix/DSO          Win32     Action Required
     *              (-DSHARED_MODULE) (-DWIN32)
     *  ----------- ----------------- --------- -----------------------------------
     *  -           load module       -         -
     *  init        init              init      SSL library init, Pass Phrase Dialog
     *  detach      detach            -         -
     *  -           reload module     -         -
     *  init        init              -         SSL library init, mod_ssl init
     *
     *  Ok, now try to solve this totally ugly situation...
     */

#ifdef SHARED_MODULE
    ssl_init_SSLLibrary(s);
#else
    if (mc->nInitCount <= 2)
        ssl_init_SSLLibrary(s);
#endif
    if (mc->nInitCount == 1) {
        ssl_pphrase_Handle(s, p);
#ifndef WIN32
        return;
#endif
    }

    /*
     * Warn the user that he should use the session cache.
     * But we can operate without it, of course.
     */
    if (mc->nSessionCacheMode == SSL_SCMODE_UNSET) {
        ssl_log(s, SSL_LOG_WARN,
                "Init: Session Cache is not configured [hint: SSLSessionCache]");
        mc->nSessionCacheMode = SSL_SCMODE_NONE;
    }

    /*
     *  initialize the mutex handling and session caching
     */
    ssl_mutex_init(s, p);
    ssl_scache_init(s, p);

    /*
     * Seed the Pseudo Random Number Generator (PRNG)
     */
    n = ssl_rand_seed(s, p, SSL_RSCTX_STARTUP);
    ssl_log(s, SSL_LOG_INFO, "Init: Seeding PRNG with %d bytes of entropy", n);

    /*
     *  pre-generate the temporary RSA key
     */
    if (mc->pRSATmpKey == NULL) {
        ssl_log(s, SSL_LOG_INFO, "Init: Generating temporary (512 bit) RSA private key");
        mc->pRSATmpKey = RSA_generate_key(512, RSA_F4, NULL, NULL);
        if (mc->pRSATmpKey == NULL) {
#ifdef __OpenBSD__
            ssl_log(s, SSL_LOG_ERROR, "Init: Failed to generate temporary (512 bit) RSA private key (SSL won't work without an RSA capable shared library)");
	    ssl_log(s, SSL_LOG_ERROR, "Init: pkg_add ftp://ftp.openbsd.org/pub/OpenBSD/<version>/packages/<arch>/libssl-1.1.tgz if you are able to use RSA");
	    /* harmless in http only case. We'll get a fatal error below 
	     * if this didn't work and we try to init https servers 
             */ 
	    return; 
#else
            ssl_log(s, SSL_LOG_ERROR, "Init: Failed to generate temporary (512 bit) RSA private key");
            ssl_die();
#endif
        }
    }

    /*
     *  initialize servers
     */
    ssl_log(s, SSL_LOG_INFO, "Init: Initializing (virtual) servers for SSL");
    for (; s != NULL; s = s->next) {
        sc = mySrvConfig(s);

        /* 
         * Give out warnings when HTTPS is configured for
         * the HTTP port or vice versa
         */
        if (sc->bEnabled && s->port == DEFAULT_HTTP_PORT)
            ssl_log(s, SSL_LOG_WARN,
                    "Init: You configured HTTPS(%d) on the standard HTTP(%d) port!",
                    DEFAULT_HTTPS_PORT, DEFAULT_HTTP_PORT);
        if (!sc->bEnabled && s->port == DEFAULT_HTTPS_PORT)
            ssl_log(s, SSL_LOG_WARN,
                    "Init: You configured HTTP(%d) on the standard HTTPS(%d) port!",
                    DEFAULT_HTTP_PORT, DEFAULT_HTTPS_PORT);

        /* 
         * Either now skip this server when SSL is disabled for
         * it or give out some information about what we're
         * configuring.
         */
        if (!sc->bEnabled)
            continue;
        ssl_log(s, SSL_LOG_INFO,
                "Init: Configuring server %s for SSL protocol",
                ssl_util_vhostid(p, s));

        /*
         * Read the server certificate and key
         */
        ssl_init_GetCertAndKey(s, p, sc);
    }

    /*
     *  Announce mod_ssl and SSL library in HTTP Server field
     *  as ``mod_ssl/X.X.X OpenSSL/X.X.X''
     */
    if ((cp = ssl_var_lookup(p, NULL, NULL, NULL, "SSL_VERSION_PRODUCT")) != NULL && cp[0] != NUL)
        ap_add_version_component(cp);
    ap_add_version_component(ssl_var_lookup(p, NULL, NULL, NULL, "SSL_VERSION_INTERFACE"));
    ap_add_version_component(ssl_var_lookup(p, NULL, NULL, NULL, "SSL_VERSION_LIBRARY"));

    return;
}

/*
 *  Initialize SSL library (also already needed for the pass phrase dialog)
 */
void ssl_init_SSLLibrary(server_rec *s)
{
    ssl_log(s, SSL_LOG_INFO, "Init: Initializing %s library", SSL_LIBRARY_NAME);
#ifdef WIN32
    CRYPTO_malloc_init();
#endif
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
    ssl_util_thread_setup();
    return;
}

/*
 * Read the SSL Server Certificate and Key
 */
void ssl_init_GetCertAndKey(server_rec *s, pool *p, SSLSrvConfigRec *sc)
{
    SSLModConfigRec *mc = myModConfig();
    int nVerify;
    char *cpVHostID;
    SSL_CTX *ctx;
    STACK *skCAList;
    ssl_asn1_t *asn1;
    char *cp;

    /*
     * Create the server host:port string because we need it a lot
     */
    cpVHostID = ssl_util_vhostid(p, s);

    /*
     * Now check for important parameters and the
     * possibility that the user forgot to set them.
     */
    if (sc->szCertificateFile == NULL) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) No SSL Certificate set [hint: SSLCertificateFile]",
                cpVHostID);
        ssl_die();
    }

    /*
     *  Check for problematic re-initializations
     */
    if (sc->px509Certificate) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) Illegal attempt to re-initialise SSL for server "
                "(theoretically shouldn't happen!)", cpVHostID);
        ssl_die();
    }

    /*
     *  Create the new per-server SSL context
     */
    if (sc->nProtocol == SSL_PROTOCOL_NONE) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) No SSL protocols available [hint: SSLProtocol]",
                cpVHostID);
        ssl_die();
    }
    cp = ap_pstrcat(p, (sc->nProtocol & SSL_PROTOCOL_SSLV2 ? "SSLv2, " : ""), 
                       (sc->nProtocol & SSL_PROTOCOL_SSLV3 ? "SSLv3, " : ""), 
                       (sc->nProtocol & SSL_PROTOCOL_TLSV1 ? "TLSv1, " : ""), NULL);
    cp[strlen(cp)-2] = NUL;
    ssl_log(s, SSL_LOG_TRACE, 
            "Init: (%s) Creating new SSL context (protocols: %s)", cpVHostID, cp);
    if (sc->nProtocol == SSL_PROTOCOL_SSLV2)
        ctx = SSL_CTX_new(SSLv2_server_method());  /* only SSLv2 is left */ 
    else
        ctx = SSL_CTX_new(SSLv23_server_method()); /* be more flexible */
    if (!(sc->nProtocol & SSL_PROTOCOL_SSLV2))
        SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
    if (!(sc->nProtocol & SSL_PROTOCOL_SSLV3))
        SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
    if (!(sc->nProtocol & SSL_PROTOCOL_TLSV1)) 
        SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
    SSL_CTX_set_app_data(ctx, s);
    sc->pSSLCtx = ctx;

    /*
     *  Configure callbacks for SSL context
     */
    nVerify = SSL_VERIFY_NONE;
    if (sc->nVerifyClient == SSL_CVERIFY_REQUIRE)
        nVerify |= SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    if (   (sc->nVerifyClient == SSL_CVERIFY_OPTIONAL)
        || (sc->nVerifyClient == SSL_CVERIFY_OPTIONAL_NO_CA) )
        nVerify |= SSL_VERIFY_PEER;
    SSL_CTX_set_verify(ctx, nVerify,  ssl_callback_SSLVerify);
    SSL_CTX_sess_set_new_cb(ctx,      ssl_callback_NewSessionCacheEntry);
    SSL_CTX_sess_set_get_cb(ctx,      ssl_callback_GetSessionCacheEntry);
    SSL_CTX_sess_set_remove_cb(ctx,   ssl_callback_DelSessionCacheEntry);
    SSL_CTX_set_tmp_rsa_callback(ctx, ssl_callback_TmpRSA);
    SSL_CTX_set_info_callback(ctx,    ssl_callback_LogTracingState);

    /*
     *  Configure SSL Cipher Suite
     */
    ssl_log(s, SSL_LOG_TRACE,
            "Init: (%s) Configuring permitted SSL ciphers", cpVHostID);
    if (sc->szCipherSuite != NULL) {
        if (!SSL_CTX_set_cipher_list(sc->pSSLCtx, sc->szCipherSuite)) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure permitted SSL ciphers",
                    cpVHostID);
            ssl_die();
        }
    }

    /*
     * Configure Client Authentication details
     */
    if (sc->szCACertificateFile != NULL || sc->szCACertificatePath != NULL) {
        ssl_log(s, SSL_LOG_TRACE,
                "Init: (%s) Configuring client authentication", cpVHostID);
        if (!SSL_CTX_load_verify_locations(sc->pSSLCtx,
                                           sc->szCACertificateFile, 
                                           sc->szCACertificatePath)) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Init: (%s) Unable to configure verify locations "
                    "for client authentication", cpVHostID);
            ssl_die();
        }
        if ((skCAList = ssl_init_FindCAList(s, p, sc->szCACertificateFile,
                                            sc->szCACertificatePath)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR,
                    "Init: (%s) Unable to determine list of available "
                    "CA certificates for client authentication", cpVHostID);
            ssl_die();
        }
        SSL_CTX_set_client_CA_list(sc->pSSLCtx, skCAList);
    }

    /*
     * Give a warning when no CAs were configured but client authentication
     * should take place. This cannot work.
     */
    if (sc->nVerifyClient == SSL_CVERIFY_REQUIRE) {
        skCAList = SSL_CTX_get_client_CA_list(sc->pSSLCtx);
        if (sk_num(skCAList) == 0)
            ssl_log(s, SSL_LOG_WARN, 
                    "Init: Ops, you want to request client authentication, "
                    "but no CAs are known for verification!? "
                    "[Hint: SSLCACertificate*]");
    }

    /*
     *  Configure server certificate
     */
    ssl_log(s, SSL_LOG_TRACE,
            "Init: (%s) Configuring server certificate", cpVHostID);
    if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tPublicCert, cpVHostID)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) Ops, can't find server certificate?!", cpVHostID);
        ssl_die();
    }
    sc->px509Certificate = d2i_X509(NULL, &(asn1->cpData), asn1->nData);

    /*
     *  Configure server private key
     */
    ssl_log(s, SSL_LOG_TRACE,
            "Init: (%s) Configuring server private key", cpVHostID);
    if ((asn1 = (ssl_asn1_t *)ssl_ds_table_get(mc->tPrivateKey, cpVHostID)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR,
                "Init: (%s) Ops, can't find server private key?!", cpVHostID);
        ssl_die();
    }
    sc->prsaKey = d2i_RSAPrivateKey(NULL, &(asn1->cpData), asn1->nData);

    return;
}

static int ssl_init_FindCAList_X509NameCmp(X509_NAME **a, X509_NAME **b)
{
    return(X509_NAME_cmp(*a, *b));
}

STACK *ssl_init_FindCAList(server_rec *s, pool *pp, char *cpCAfile, char *cpCApath)
{
    STACK *skCAList;
    STACK *sk;
    DIR *dir;
    struct DIR_TYPE *direntry;
    char *cp;
    pool *p;
    int n;

    /*
     * Use a subpool so we don't bloat up the server pool which
     * is remains in memory for the complete operation time of
     * the server.
     */
    p = ap_make_sub_pool(pp);

    /*
     * Start with a empty stack/list where new
     * entries get added in sorted order.
     */
    skCAList = sk_new(ssl_init_FindCAList_X509NameCmp);

    /*
     * Process CA certificate bundle file
     */
    if (cpCAfile != NULL) {
        sk = SSL_load_client_CA_file(cpCAfile);
        for(n = 0; sk != NULL && n < sk_num(sk); n++) {
            ssl_log(s, SSL_LOG_TRACE,
                    "CA certificate: %s",
                    X509_NAME_oneline((X509_NAME *)sk_value(sk, n), NULL, 0));
            if (sk_find(skCAList, sk_value(sk, n)) < 0)
                sk_push(skCAList, sk_value(sk, n));
        }
    }

    /*
     * Process CA certificate path files
     */
    if (cpCApath != NULL) {
        dir = ap_popendir(p, cpCApath);
        while ((direntry = readdir(dir)) != NULL) {
            cp = ap_pstrcat(p, cpCApath, "/", direntry->d_name, NULL);
            sk = SSL_load_client_CA_file(cp);
            for(n = 0; sk != NULL && n < sk_num(sk); n++) {
                ssl_log(s, SSL_LOG_TRACE,
                        "CA certificate: %s",
                        X509_NAME_oneline((X509_NAME *)sk_value(sk, n), NULL, 0));
                if (sk_find(skCAList, sk_value(sk, n)) < 0)
                    sk_push(skCAList, sk_value(sk, n));
            }
        }
        ap_pclosedir(p, dir);
    }

    /*
     * Cleanup
     */
    sk_set_cmp_func(skCAList, NULL);
    ap_destroy_pool(p);

    return skCAList;
}

void ssl_init_Child(server_rec *s, pool *p)
{
     /* open the mutex lockfile */
     ssl_mutex_open(s, p);
     return;
}

