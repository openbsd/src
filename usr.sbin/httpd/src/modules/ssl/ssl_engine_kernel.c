/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |
** | '_ ` _ \ / _ \ / _` |   / __/ __| |
** | | | | | | (_) | (_| |   \__ \__ \ | mod_ssl - Apache Interface to SSLeay
** |_| |_| |_|\___/ \__,_|___|___/___/_| http://www.engelschall.com/sw/mod_ssl/
**                      |_____|
**  ssl_engine_kernel.c
**  The SSL engine kernel
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
                             /* ``It took me fifteen years to discover
                                  I had no talent for programming, but
                                  I couldn't give it up because by that
                                  time I was too famous.''
                                            -- Unknown                */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  SSL Engine Kernel
**  _________________________________________________________________
*/

/*
 *  Connect Handler:
 *  Connect SSL to the accepted socket
 *
 *  Usually we would need an Apache API hook which is triggered right after
 *  the socket is accepted for handling a new request. But Apache 1.3 doesn't
 *  provide such a hook, so we have to patch http_main.c and call this
 *  function directly.
 */
void ssl_hook_NewConnection(conn_rec *conn)
{
    server_rec *srvr;
    BUFF *fb;
    SSLSrvConfigRec *sc;
    SSL *ssl;
    char *cp;
    int rc;
    int n;

    /*
     * Get context
     */
    srvr = conn->server;
    fb   = conn->client;
    sc   = mySrvConfig(srvr);

    /*
     * Create SSL context
     */
    ap_ctx_set(fb->ctx, "ssl", NULL);

    /*
     * Immediately stop processing if SSL
     * is disabled for this connection
     */
    if (sc == NULL || !sc->bEnabled)
        return;

    /*
     * Remember the connection information for
     * later access inside callback functions
     */
    ssl_log(srvr, SSL_LOG_INFO, "Connection to child %d established (server %s)",
            conn->child_num, ssl_util_vhostid(conn->pool, srvr));

    /*
     * Seed the Pseudo Random Number Generator (PRNG)
     */
    n = ssl_rand_seed(srvr, conn->pool, SSL_RSCTX_CONNECT);
    ssl_log(srvr, SSL_LOG_TRACE, "Seeding PRNG with %d bytes of entropy", n);

    /*
     * Create a new SSL connection with the configured server SSL context and
     * attach this to the socket. Additionally we register this attachment
     * so we can detach later.
     */
    ssl = SSL_new(sc->pSSLCtx);
    SSL_set_app_data(ssl, conn);  /* conn_rec    (available now)     */
    SSL_set_app_data2(ssl, NULL); /* request_rec (available later)   */
    SSL_set_fd(ssl, fb->fd);
    ap_ctx_set(fb->ctx, "ssl", ssl);
    ap_register_cleanup(conn->pool, (void *)conn,
                        ssl_hook_CloseConnection, ssl_hook_CloseConnection);

    /*
     * Configure SSLeay BIO Data Logging
     */
    if (sc->nLogLevel >= SSL_LOG_DEBUG) {
        BIO_set_callback(SSL_get_rbio(ssl), ssl_io_data_cb);
        BIO_set_callback_arg(SSL_get_rbio(ssl), ssl);
    }

    /*
     * Configure the server certificate and private key
     * which should be used for this connection.
     */
    if (sc->szCertificateFile != NULL) {
        if (SSL_use_certificate(ssl, sc->px509Certificate) <= 0) {
            ssl_log(conn->server, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Unable to configure server certificate for connection");
            SSL_free(ssl);
            ap_ctx_set(fb->ctx, "ssl", NULL);
            ap_bsetflag(fb, B_EOF|B_EOUT, 1);
            conn->aborted = 1;
            return;
        }
        if (SSL_use_RSAPrivateKey(ssl, sc->prsaKey) <= 0) {
            ssl_log(conn->server, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Unable to configure server private key for connection");
            SSL_free(ssl);
            ap_ctx_set(fb->ctx, "ssl", NULL);
            ap_bsetflag(fb, B_EOF|B_EOUT, 1);
            conn->aborted = 1;
            return;
        }
    }

    /*
     * Predefine some client verification results
     */
    ap_ctx_set(fb->ctx, "ssl::client::dn", NULL);
    ap_ctx_set(fb->ctx, "ssl::verify::error", NULL);

    /* 
     * We have to manage a I/O timeout ourself, because Apache
     * does it the first time when reading the request, but we're
     * working some time before this happens.
     */
    ap_ctx_set(ap_global_ctx, "ssl::handshake::timeout", (void *)FALSE);
    ap_set_callback_and_alarm(ssl_hook_TimeoutConnection, srvr->timeout);

    /*
     * Now enter the SSL Handshake Phase
     */
    while (!SSL_is_init_finished(ssl)) {

        if ((rc = SSL_accept(ssl)) <= 0) {

            if (SSL_get_error(ssl, rc) == SSL_ERROR_ZERO_RETURN) {
                /*
                 * The case where the connection was closed before any data
                 * was transferred. That's not a real error and can occur
                 * sporadically with some clients.
                 */
                ssl_log(srvr, SSL_LOG_INFO,
                        "SSL handshake stopped: connection was closed");
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                while (!SSL_shutdown(ssl));
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);
                ap_bsetflag(fb, B_EOF|B_EOUT, 1);
                conn->aborted = 1;
                return;
            }
            else if (ERR_GET_REASON(ERR_peek_error()) == SSL_R_HTTP_REQUEST) {
                /*
                 * The case where SSLeay has recognized a HTTP request:
                 * This means the client speaks plain HTTP on our HTTPS
                 * port. Hmmmm...  At least for this error we can be more friendly
                 * and try to provide him with a HTML error page. We have only one
                 * problem: SSLeay has already read some bytes from the HTTP
                 * request. So we have to skip the request line manually and
                 * instead provide a faked one in order to continue the internal
                 * Apache processing.
                 *
                 * (This feature is only available for SSLeay 0.9.0 and higher)
                 */
                char ca[2];
                int rv;

                /* log the situation */
                ssl_log(srvr, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                        "SSL handshake failed: HTTP spoken on HTTPS port; "
                        "trying to send HTML error page");

                /* first: skip the remaining bytes of the request line */
                do {
                    do {
                        rv = read(fb->fd, ca, 1);
                    } while (rv == -1 && errno == EINTR);
                } while (rv > 0 && ca[0] != '\012' /*LF*/);

                /* second: fake the request line */
                fb->inbase = ap_palloc(fb->pool, fb->bufsiz);
                ap_cpystrn((char *)fb->inbase, "GET /mod_ssl:error:HTTP-request HTTP/1.0\r\n",
                           fb->bufsiz);
                fb->inptr = fb->inbase;
                fb->incnt = strlen((char *)fb->inptr);

                /* third: kick away the SSL stuff */
                SSL_set_shutdown(ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
                while (!SSL_shutdown(ssl));
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);

                /* finally: let Apache go on with processing */
                return;
            }
            else if (ap_ctx_get(ap_global_ctx, "ssl::handshake::timeout") == (void *)TRUE) {
                ssl_log(srvr, SSL_LOG_ERROR, 
                        "SSL handshake timed out (client %s, server %s)",
                        conn->remote_ip, ssl_util_vhostid(conn->pool, srvr));
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                while (!SSL_shutdown(ssl));
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);
                ap_bsetflag(fb, B_EOF|B_EOUT, 1);
                conn->aborted = 1;
                return;
            }
            else if (SSL_get_error(ssl, rc) == SSL_ERROR_SYSCALL) {
                if (errno == EINTR)
                    continue;
                ssl_log(srvr, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                        "SSL handshake interrupted by system");
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                while (!SSL_shutdown(ssl));
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);
                ap_bsetflag(fb, B_EOF|B_EOUT, 1);
                conn->aborted = 1;
                return;
            }
            else {
                /*
                 * Ok, anything else is a fatal error
                 */
                ssl_log(srvr, SSL_LOG_ERROR|SSL_ADD_SSLERR|SSL_ADD_ERRNO,
                        "SSL handshake failed (client %s, server %s)",
                        conn->remote_ip, ssl_util_vhostid(conn->pool, srvr));

                /*
                 * try to gracefully shutdown the connection:
                 * - send an own shutdown message (be gracefully)
                 * - don't wait for peer's shutdown message (deadloop)
                 * - kick away the SSL stuff immediately
                 * - block the socket, so Apache cannot operate any more
                 */
                SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
                while (!SSL_shutdown(ssl));
                SSL_free(ssl);
                ap_ctx_set(fb->ctx, "ssl", NULL);
                ap_bsetflag(fb, B_EOF|B_EOUT, 1);
                conn->aborted = 1;
                return;
            }
        }

        /*
         * Check for failed client authentication
         */
        if ((cp = (char *)ap_ctx_get(fb->ctx, "ssl::verify::error")) != NULL) {
            ssl_log(srvr, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "SSL client authentication failed: %s", cp);
            SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
            while (!SSL_shutdown(ssl));
            SSL_free(ssl);
            ap_ctx_set(fb->ctx, "ssl", NULL);
            ap_bsetflag(fb, B_EOF|B_EOUT, 1);
            conn->aborted = 1;
            return;
        }

        /*
         * Remember the peer certificate when
         * client authentication was done
         */
        if (sc->nVerifyClient != SSL_CVERIFY_NONE) {
            char *s;
            X509 *xs;
            if ((xs = SSL_get_peer_certificate(ssl)) != NULL) {
                s = X509_NAME_oneline(X509_get_subject_name(xs), NULL, 0);
                ap_ctx_set(fb->ctx, "ssl::client::dn", ap_pstrdup(conn->pool, s));
                free(s);
            }
        }

        /*
         * Make really sure that when a peer certificate
         * is required we really got one... (be paranoid)
         */
        if (   sc->nVerifyClient == SSL_CVERIFY_REQUIRE
            && ap_ctx_get(fb->ctx, "ssl::client::dn") == NULL) {
            ssl_log(srvr, SSL_LOG_ERROR,
                    "No acceptable peer certificate available");
            SSL_set_shutdown(ssl, SSL_RECEIVED_SHUTDOWN);
            while (!SSL_shutdown(ssl));
            SSL_free(ssl);
            ap_ctx_set(fb->ctx, "ssl", NULL);
            ap_bsetflag(fb, B_EOF|B_EOUT, 1);
            conn->aborted = 1;
            return;
        }
    }

    /*
     * Remove the timeout handling
     */
    ap_set_callback_and_alarm(NULL, 0);
    ap_ctx_set(ap_global_ctx, "ssl::handshake::timeout", (void *)FALSE);

    /*
     * Improve I/O throughput by using
     * SSLeay's read-ahead functionality
     * (don't used under Win32, because
     * there we use select())
     */
#ifndef WIN32
    SSL_set_read_ahead(ssl, TRUE);
#endif

    return;
}

/*
 * Signal handler function for the SSL handshake phase
 */
void ssl_hook_TimeoutConnection(int sig)
{
    /* we just set a flag for the handshake processing loop */
    ap_ctx_set(ap_global_ctx, "ssl::handshake::timeout", (void *)TRUE);
    return;
}

/*
 *  Close the SSL part of the socket connection
 */
void ssl_hook_CloseConnection(void *_conn)
{
    conn_rec *conn = _conn;
    SSL *ssl;

    /*
     * Optionally shutdown the SSL session
     */
    ssl = ap_ctx_get(conn->client->ctx, "ssl");
    if (ssl != NULL) {
        SSL_set_shutdown(ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
        while (!SSL_shutdown(ssl));
        SSL_free(ssl);
        ap_ctx_set(conn->client->ctx, "ssl", NULL);
    }

    /*
     * And finally log the connection close
     */
    ssl_log(conn->server, SSL_LOG_INFO, "Connection to child %d closed (server %s)",
            conn->child_num, ssl_util_vhostid(conn->pool, conn->server));

    return;
}

/*
 *  Post Read Request Handler
 */
int ssl_hook_ReadReq(request_rec *r)
{
    SSL *ssl;

    /*
     * Get the SSL connection structure and perform the 
     * delayed interlinking from SSL back to request_rec
     */
    ssl = ap_ctx_get(r->connection->client->ctx, "ssl");
    if (ssl != NULL)
        SSL_set_app_data2(ssl, r);

    /*
     * Force the mod_ssl content handler when URL indicates this
     */
    if (strEQn(r->uri, "/mod_ssl:", 9))
        r->handler = "mod_ssl:content-handler";
    if (ssl != NULL) {
        ap_ctx_set(r->ctx, "ap::http::method",  "https");
        ap_ctx_set(r->ctx, "ap::default::port", "443");
    }
    else {
        ap_ctx_set(r->ctx, "ap::http::method",  NULL);
        ap_ctx_set(r->ctx, "ap::default::port", NULL);
    }
    return DECLINED;
}

/*
 *  Content Handler
 */
int ssl_hook_Handler(request_rec *r)
{
    int port;
    char *thisport;
    char *thisurl;

    if (strNEn(r->uri, "/mod_ssl:", 9))
        return DECLINED;

    if (strEQ(r->uri, "/mod_ssl:error:HTTP-request")) {
        thisport = "";
        port = ap_get_server_port(r);
        if (!ap_is_default_port(port, r))
            thisport = ap_psprintf(r->pool, ":%u", port);
        thisurl = ap_psprintf(r->pool, "https://%s%s/",
                              ap_get_server_name(r), thisport);

        ap_table_setn(r->notes, "error-notes", ap_psprintf(r->pool,
                      "Reason: You're speaking plain HTTP to an SSL-enabled server port.<BR>\n"
                      "Instead use the HTTPS scheme to access this URL, please.<BR>\n"
                      "<BLOCKQUOTE>Hint: <A HREF=\"%s\"><B>%s</B></A></BLOCKQUOTE>",
                      thisurl, thisurl));
    }

    return HTTP_BAD_REQUEST;
}

/*
 *  Auth Handler:
 *  Fake a Basic authentication from the X509 client certificate.
 *
 *  This must be run fairly early on to prevent a real authentication from
 *  occuring, in particular it must be run before anything else that
 *  authenticates a user.  This means that the Module statement for this
 *  module should be LAST in the Configuration file.
 */
int ssl_hook_Auth(request_rec *r)
{
    SSLSrvConfigRec *sc = mySrvConfig(r->server);
    SSLDirConfigRec *dc = myDirConfig(r);
    char b1[MAX_STRING_LEN], b2[MAX_STRING_LEN];
    char *clientdn;

    /*
     * We decline operation in various situations..
     */
    if (!sc->bEnabled)
        return DECLINED;
    if (ap_ctx_get(r->connection->client->ctx, "ssl") == NULL)
        return DECLINED;
    if (!(dc->nOptions & SSL_OPT_FAKEBASICAUTH))
        return DECLINED;
    if (r->connection->user)
        return DECLINED;
    if ((clientdn = (char *)ap_ctx_get(r->connection->client->ctx, "ssl::client::dn")) == NULL)
        return DECLINED;

    /*
     * Fake a password - which one would be immaterial, as, it seems, an empty
     * password in the users file would match ALL incoming passwords, if only
     * we were using the standard crypt library routine. Unfortunately, SSLeay
     * "fixes" a "bug" in crypt and thus prevents blank passwords from
     * working.  (IMHO what they really fix is a bug in the users of the code
     * - failing to program correctly for shadow passwords).  We need,
     * therefore, to provide a password. This password can be matched by
     * adding the string "xxj31ZMTZzkVA" as the password in the user file.
     * This is just the crypted variant of the word "password" ;-)
     */
    ap_snprintf(b1, sizeof(b1), "%s:password", clientdn);
    ssl_util_uuencode(b2, b1, FALSE);
    ap_snprintf(b1, sizeof(b1), "Basic %s", b2);
    ap_table_set(r->headers_in, "Authorization", b1);

    return DECLINED;
}

/*
 *  Access Handler
 */
int ssl_hook_Access(request_rec *r)
{
    SSLDirConfigRec *dc;
    SSLSrvConfigRec *sc;
    SSL *ssl;
    SSL_CTX *ctx;
    array_header *apRequirement;
    ssl_require_t *pRequirements;
    ssl_require_t *pRequirement;
    char *cp;
    int ok;
    int i;
    BOOL renegotiate; 
#ifdef SSL_EXPERIMENTAL
    BOOL reconfigured_locations;
    STACK *skCAList;
    char *cpCAPath;
    char *cpCAFile;
#endif
    STACK *skCipherOld;
    STACK *skCipher;
    int nVerifyOld;
    int nVerify;
    int n;

    dc  = myDirConfig(r);
    sc  = mySrvConfig(r->server);
    ssl = ap_ctx_get(r->connection->client->ctx, "ssl");
    if (ssl != NULL)
        ctx = SSL_get_SSL_CTX(ssl);

    /*
     * Support for SSLRequireSSL directive
     */
    if (dc->bSSLRequired && ssl == NULL) {
        ap_log_reason("SSL connection required", r->filename, r);
        return FORBIDDEN;
    }

    /*
     * Check to see if SSL protocol is on
     */
    if (!sc->bEnabled)
        return DECLINED;
    if (ssl == NULL)
        return DECLINED;

    /*
     * Support for per-directory SSL connection parameters.  
     * 
     * This is implemented by forcing an SSL renegotiation with the
     * reconfigured parameter suite. But Apache's internal API processing
     * makes our life very hard, because when internal sub-requests occur we
     * nevertheless should avoid multiple unnecessary SSL handshakes (they
     * need network I/O and time to perform). But the optimization for
     * filtering out the unnecessary handshakes isn't such obvious.
     * Especially because while Apache is in its sub-request processing the
     * client could force additional handshakes, too. And these take place
     * perhaps without our notice. So the only possibility is to ask
     * SSLeay/OpenSSL whether the renegotiation has to be performed or not. It
     * has to performed when some parameters which were previously known (by
     * us) are not those we've now reconfigured (as known by SSLeay/OpenSSL).
     */
    renegotiate = FALSE;
#ifdef SSL_EXPERIMENTAL
    reconfigured_locations = FALSE;
#endif

    /* 
     * override of SSLCipherSuite
     */
    if (dc->szCipherSuite != NULL) {
        /* remember old cipher suite for comparison */
        if ((skCipherOld = SSL_get_ciphers(ssl)) != NULL)
            skCipherOld = sk_dup(skCipherOld);
        /* configure new cipher suite */
        if (!SSL_set_cipher_list(ssl, dc->szCipherSuite)) {
            ssl_log(r->server, SSL_LOG_WARN|SSL_ADD_SSLERR,
                    "Unable to reconfigure (per-directory) permitted SSL ciphers");
            return FORBIDDEN;
        }
        /* determine whether the cipher suite was actually changed */
        skCipher = SSL_get_ciphers(ssl);
        if ((skCipherOld == NULL && skCipher != NULL) ||
            (skCipherOld != NULL && skCipher == NULL)   )
            renegotiate = TRUE;
        else if (skCipherOld != NULL && skCipher != NULL) {
            for (n = 0; n < sk_num(skCipher); n++) {
                if (sk_find(skCipherOld, sk_value(skCipher, n)) < 0) {
                    renegotiate = TRUE;
                    break;
                }
            }
            for (n = 0; n < sk_num(skCipherOld); n++) {
                if (sk_find(skCipher, sk_value(skCipherOld, n)) < 0) {
                    renegotiate = TRUE;
                    break;
                }
            }
        }
        /* free old cipher suite */
        if (skCipherOld != NULL)
            sk_free(skCipherOld);
    }

    /*
     * override of SSLVerifyDepth:
     * This is handled by us manually inside the verify callback
     * function and not by SSLeay internally. And our function is
     * aware of both the per-server and per-directory contexts.
     * All we have to do is to force the renegotiation when the
     * maximum allowed depth is changed.
     */
    if (dc->nVerifyDepth != UNSET) {
        if (dc->nVerifyDepth != sc->nVerifyDepth)
            renegotiate = TRUE;
    }

    /* 
     * override of SSLVerifyClient
     */
    if (dc->nVerifyClient != SSL_CVERIFY_UNSET) {
        /* remember old verify mode */
        nVerifyOld = SSL_get_verify_mode(ssl);
        /* configure new verify mode */
        nVerify = SSL_VERIFY_NONE;
        if (dc->nVerifyClient == SSL_CVERIFY_REQUIRE)
            nVerify |= SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        if (   (dc->nVerifyClient == SSL_CVERIFY_OPTIONAL)
            || (dc->nVerifyClient == SSL_CVERIFY_OPTIONAL_NO_CA) )
            nVerify |= SSL_VERIFY_PEER;
        SSL_set_verify(ssl, nVerify, ssl_callback_SSLVerify);
        SSL_set_verify_result(ssl, X509_V_OK);
        /* determine whether the verify mode was actually changed */
        if (nVerify != nVerifyOld) 
            renegotiate = TRUE;
    }

    /*
     *  override SSLCACertificateFile & SSLCACertificatePath
     *  This is tagged experimental because it has to use an ugly kludge: We
     *  have to change the locations inside the SSL_CTX* (per-server global)
     *  instead inside SSL* (per-connection local) and reconfigure it to the
     *  old values later. That's problematic at least for the threaded process
     *  model of Apache under Win32 or when an error occurs. But unless
     *  OpenSSL provides a SSL_load_verify_locations() function we've no other
     *  chance to provide this functionality...
     */
#ifdef SSL_EXPERIMENTAL
    if (   (   dc->szCACertificateFile != NULL 
            && (   sc->szCACertificateFile == NULL 
                || (   sc->szCACertificateFile != NULL 
                    && strNE(dc->szCACertificateFile, sc->szCACertificateFile))))
        || (   dc->szCACertificatePath != NULL 
            && (   sc->szCACertificatePath == NULL 
                || (   sc->szCACertificatePath != NULL 
                    && strNE(dc->szCACertificatePath, sc->szCACertificatePath)))) ) {
        cpCAFile = dc->szCACertificateFile != NULL ?
                   dc->szCACertificateFile : sc->szCACertificateFile;
        cpCAPath = dc->szCACertificatePath != NULL ?
                   dc->szCACertificatePath : sc->szCACertificatePath;
        /*
           FIXME: This should be...
           if (!SSL_load_verify_locations(ssl, cpCAFile, cpCAPath)) { 
           ...but SSLeay/OpenSSL still doesn't provide this!
         */
        if (!SSL_CTX_load_verify_locations(ctx, cpCAFile, cpCAPath)) {
            ssl_log(r->server, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Unable to reconfigure verify locations "
                    "for client authentication");
            return FORBIDDEN;
        }
        if ((skCAList = ssl_init_FindCAList(r->server, r->pool, 
                                            cpCAFile, cpCAPath)) == NULL) {
            ssl_log(r->server, SSL_LOG_ERROR,
                    "Unable to determine list of available "
                    "CA certificates for client authentication");
            return FORBIDDEN;
        }
        SSL_set_client_CA_list(ssl, skCAList);
        renegotiate = TRUE;
        reconfigured_locations = TRUE;
    }
#endif /* SSL_EXPERIMENTAL */

    /* 
     * now do the renegotiation if anything was actually reconfigured
     */
    if (renegotiate) {
        /* 
         * Now we force the SSL renegotation by sending the Hello Request
         * message to the client. Here we have to do a workaround: Actually
         * SSLeay returns immediately after sending the Hello Request (the
         * intent AFAIK is because the SSL/TLS protocol says it's not a must
         * that the client replies to a Hello Request). But because we insist
         * on a reply (anything else is an error for us) we have to go to the
         * ACCEPT state manually. Using SSL_set_accept_state() doesn't work
         * here because it resets too much of the connection.  So we set the
         * state explicitly and continue the handshake manually.
         */
        ssl_log(r->server, SSL_LOG_INFO, "Requesting connection re-negotiation");
        SSL_renegotiate(ssl);
        SSL_do_handshake(ssl);
        if (SSL_get_state(ssl) != SSL_ST_OK) {
            ssl_log(r->server, SSL_LOG_ERROR, "Re-negotation request failed");
            return FORBIDDEN;
        }
        ssl_log(r->server, SSL_LOG_INFO, "Awaiting re-negotiation handshake");
        SSL_set_state(ssl, SSL_ST_ACCEPT);
        SSL_do_handshake(ssl);
        if (SSL_get_state(ssl) != SSL_ST_OK) {
            ssl_log(r->server, SSL_LOG_ERROR, 
                    "Re-negotiation handshake failed: Not accepted by client!?");
            return FORBIDDEN;
        }

        /*
         * Finally check for acceptable renegotiation results
         */
        if (dc->nVerifyClient != SSL_CVERIFY_NONE) {
            if (SSL_get_verify_result(ssl) != X509_V_OK) {
                ssl_log(r->server, SSL_LOG_ERROR, 
                        "Re-negotiation handshake failed: Client verification failed");
                return FORBIDDEN;
            }
            if (   dc->nVerifyClient == SSL_CVERIFY_REQUIRE 
                && SSL_get_peer_certificate(ssl) == NULL   ) {
                ssl_log(r->server, SSL_LOG_ERROR, 
                        "Re-negotiation handshake failed: Client certificate missing");
                return FORBIDDEN;
            }
        }
    }

    /*
     * Under old SSLeay we had to change the X509_STORE inside the SSL_CTX
     * instead inside the SSL structure, so we have to reconfigure it to the
     * old values. This should be changed with forthcoming OpenSSL version
     * when better functionality is avaiable.
     */
#ifdef SSL_EXPERIMENTAL
    if (renegotiate && reconfigured_locations) {
        if (!SSL_CTX_load_verify_locations(ctx,
                sc->szCACertificateFile, sc->szCACertificatePath)) {
            ssl_log(r->server, SSL_LOG_ERROR|SSL_ADD_SSLERR,
                    "Unable to reconfigure verify locations "
                    "to per-server configuration parameters");
            return FORBIDDEN;
        }
    }
#endif /* SSL_EXPERIMENTAL */

    /*
     * Check SSLRequire boolean expressions
     */
    apRequirement = dc->aRequirement;
    pRequirements  = (ssl_require_t *)apRequirement->elts;
    for (i = 0; i < apRequirement->nelts; i++) {
        pRequirement = &pRequirements[i];
        ok = ssl_expr_exec(r, pRequirement->mpExpr);
        if (ok < 0) {
            cp = ap_psprintf(r->pool, "Failed to execute SSL requirement expression: %s",
                             ssl_expr_get_error());
            ap_log_reason(cp, r->filename, r);
            return FORBIDDEN;
        }
        if (ok != 1) {
            ssl_log(r->server, SSL_LOG_INFO,
                    "Access to %s denied for %s (requirement expression not fulfilled)",
                    r->filename, r->connection->remote_ip);
            ssl_log(r->server, SSL_LOG_INFO,
                    "Failed expression: %s", pRequirement->cpExpr);
            ap_log_reason("SSL requirement expression not fulfilled "
                          "(see SSL logfile for more details)", r->filename, r);
            return FORBIDDEN;
        }
    }

    /*
     * Else access is granted...
     */
    return OK;
}

/*
 *   Fixup Handler
 */

static const char *ssl_hook_Fixup_vars[] = {
    "SSL_VERSION_INTERFACE",
    "SSL_VERSION_LIBRARY",
    "SSL_PROTOCOL",
    "SSL_CIPHER",
    "SSL_CIPHER_EXPORT",
    "SSL_CIPHER_USEKEYSIZE",
    "SSL_CIPHER_ALGKEYSIZE",
    "SSL_CLIENT_M_VERSION",
    "SSL_CLIENT_M_SERIAL",
    "SSL_CLIENT_V_START",
    "SSL_CLIENT_V_END",
    "SSL_CLIENT_S_DN",
    "SSL_CLIENT_S_DN_C",
    "SSL_CLIENT_S_DN_SP",
    "SSL_CLIENT_S_DN_L",
    "SSL_CLIENT_S_DN_O",
    "SSL_CLIENT_S_DN_OU",
    "SSL_CLIENT_S_DN_CN",
    "SSL_CLIENT_S_DN_Email",
    "SSL_CLIENT_I_DN",
    "SSL_CLIENT_I_DN_C",
    "SSL_CLIENT_I_DN_SP",
    "SSL_CLIENT_I_DN_L",
    "SSL_CLIENT_I_DN_O",
    "SSL_CLIENT_I_DN_OU",
    "SSL_CLIENT_I_DN_CN",
    "SSL_CLIENT_I_DN_Email",
    "SSL_CLIENT_A_KEY",
    "SSL_CLIENT_A_SIG",
    "SSL_SERVER_M_VERSION",
    "SSL_SERVER_M_SERIAL",
    "SSL_SERVER_V_START",
    "SSL_SERVER_V_END",
    "SSL_SERVER_S_DN",
    "SSL_SERVER_S_DN_C",
    "SSL_SERVER_S_DN_SP",
    "SSL_SERVER_S_DN_L",
    "SSL_SERVER_S_DN_O",
    "SSL_SERVER_S_DN_OU",
    "SSL_SERVER_S_DN_CN",
    "SSL_SERVER_S_DN_Email",
    "SSL_SERVER_I_DN",
    "SSL_SERVER_I_DN_C",
    "SSL_SERVER_I_DN_SP",
    "SSL_SERVER_I_DN_L",
    "SSL_SERVER_I_DN_O",
    "SSL_SERVER_I_DN_OU",
    "SSL_SERVER_I_DN_CN",
    "SSL_SERVER_I_DN_Email",
    "SSL_SERVER_A_KEY",
    "SSL_SERVER_A_SIG",
    NULL
};

int ssl_hook_Fixup(request_rec *r)
{
    SSLSrvConfigRec *sc = mySrvConfig(r->server);
    SSLDirConfigRec *dc = myDirConfig(r);
    table *e = r->subprocess_env;
    char *var;
    char *val;
    int i;

    /*
     * Check to see if SSL is on
     */
    if (!sc->bEnabled)
        return DECLINED;
    if (ap_ctx_get(r->connection->client->ctx, "ssl") == NULL)
        return DECLINED;

    /*
     * Annotate the SSI/CGI environment with standard SSL information
     */
    ap_table_set(e, "HTTPS", "on"); /* the HTTPS (=HTTP over SSL) flag! */
    for (i = 0; ssl_hook_Fixup_vars[i] != NULL; i++) {
        var = (char *)ssl_hook_Fixup_vars[i];
        val = ssl_var_lookup(r->pool, r->server, r->connection, r, var);
        if (!strIsEmpty(val))
            ap_table_set(e, var, val);
    }

    /*
     * On-demand bloat up the SSI/CGI environment with certificate data
     */
    if (dc->nOptions & SSL_OPT_EXPORTCERTDATA) {
        val = ssl_var_lookup(r->pool, r->server, r->connection, r, "SSL_CLIENT_CERT");
        ap_table_set(e, "SSL_CLIENT_CERT", val);
        val = ssl_var_lookup(r->pool, r->server, r->connection, r, "SSL_SERVER_CERT");
        ap_table_set(e, "SSL_SERVER_CERT", val);
    }

    /*
     * On-demand bloat up the SSI/CGI environment with compat variables
     */
#ifdef SSL_COMPAT
    if (dc->nOptions & SSL_OPT_COMPATENVVARS)
        ssl_compat_variables(r);
#endif

    return DECLINED;
}

/*  _________________________________________________________________
**
**  SSLeay Callback Functions
**  _________________________________________________________________
*/

/*
 * Handle out the already generated RSA key...
 */
RSA *ssl_callback_TmpRSA(SSL *pSSL, int nExport)
{
    SSLModConfigRec *mc = myModConfig();

    return mc->pRSATmpKey;
}

/*
 * This SSLeay callback function is called when SSLeay
 * does client authentication and verifies the certificate chain.
 */
int ssl_callback_SSLVerify(int ok, X509_STORE_CTX *ctx)
{
    SSL *ssl;
    conn_rec *conn;
    server_rec *s;
    request_rec *r;
    SSLSrvConfigRec *sc;
    SSLDirConfigRec *dc;
    X509 *xs;
    int errnum;
    int errdepth;
    char *cp;
    char *cp2;
    int depth;

    /*
     * Get Apache context back through SSLeay context
     */
    ssl  = (SSL *)X509_STORE_CTX_get_app_data(ctx);
    conn = (conn_rec *)SSL_get_app_data(ssl);
    r    = (request_rec *)SSL_get_app_data2(ssl);
    s    = conn->server;
    sc   = mySrvConfig(s);
    dc   = (r != NULL ? myDirConfig(r) : NULL);

    /*
     * Get verify ingredients
     */
    xs       = X509_STORE_CTX_get_current_cert(ctx);
    errnum   = X509_STORE_CTX_get_error(ctx);
    errdepth = X509_STORE_CTX_get_error_depth(ctx);

    /*
     * Log verification information
     */
    cp  = X509_NAME_oneline(X509_get_subject_name(xs), NULL, 0);
    cp2 = X509_NAME_oneline(X509_get_issuer_name(xs),  NULL, 0);
    ssl_log(s, SSL_LOG_TRACE,
            "Certificate Verification: depth: %d, subject: %s, issuer: %s",
            errdepth, cp != NULL ? cp : "-unknown-",
            cp2 != NULL ? cp2 : "-unknown");
    if (cp)
        free(cp);
    if (cp2)
        free(cp2);

    /*
     * Check for optionally acceptable non-verifiable issuer situation
     */
    if (   (   errnum == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT
            || errnum == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN
            || errnum == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY
            || errnum == X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE  )
        && sc->nVerifyClient == SSL_CVERIFY_OPTIONAL_NO_CA            ) {
        ssl_log(s, SSL_LOG_TRACE,
                "Certificate Verification: Verifiable Issuer is configured as "
                "optional, therefore we're accepting the certificate");
        ok = TRUE;
    }

    /*
     * If we already know it's not ok, log the real reason
     */
    if (!ok) {
        ssl_log(s, SSL_LOG_ERROR, "Certificate Verification: Error (%d): %s",
                errnum, X509_verify_cert_error_string(errnum));
        ap_ctx_set(conn->client->ctx, "ssl::client::dn", NULL);
        ap_ctx_set(conn->client->ctx, "ssl::verify::error",
                   X509_verify_cert_error_string(errnum));
    }

    /*
     * Finally check the depth of the certificate verification
     */
    if (dc != NULL && dc->nVerifyDepth != UNSET)
        depth = dc->nVerifyDepth;
    else 
        depth = sc->nVerifyDepth;
    if (errdepth > depth) {
        ssl_log(s, SSL_LOG_ERROR,
                "Certificate Verification: Certificate Chain too long "
                "(chain has %d certificates, but maximum allowed are only %d)", 
                errdepth, depth);
        ap_ctx_set(conn->client->ctx, "ssl::verify::error",
                   X509_verify_cert_error_string(X509_V_ERR_CERT_CHAIN_TOO_LONG));
        ok = FALSE;
    }

    /*
     * And finally signal SSLeay the (perhaps changed) state
     */
    return (ok);
}

/*
 *  This callback function is executed by SSLeay whenever a new SSL_SESSION is
 *  added to the internal SSLeay session cache. We use this hook to spread the
 *  SSL_SESSION also to the inter-process disk-cache to make share it with our
 *  other Apache pre-forked server processes.
 */
int ssl_callback_NewSessionCacheEntry(SSL *ssl, SSL_SESSION *pNew)
{
    conn_rec *conn;
    server_rec *s;
    SSLSrvConfigRec *sc;
    long t;

    /*
     * Get Apache context back through SSLeay context
     */
    conn = (conn_rec *)SSL_get_app_data(ssl);
    s    = conn->server;
    sc   = mySrvConfig(s);

    /*
     * Set the timeout also for the internal SSLeay cache, because this way
     * our inter-process cache is consulted only when it's really necessary.
     */
    t = (SSL_get_time(pNew) + sc->nSessionCacheTimeout);
    SSL_set_timeout(pNew, t);

    /*
     * Store the SSL_SESSION in the inter-process cache with the
     * same expire time, so it expires automatically there, too.
     */
    ssl_scache_store(s, pNew, t);

    /*
     * Log this cache operation
     */
    ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache: "
            "request=SET id=%s timeout=%ds (session caching)",
            ssl_scache_id2sz(pNew->session_id, pNew->session_id_length),
            t-time(NULL));

    /*
     * return 0 which means to SSLeay that the pNew is still
     * valid and was not freed by us with SSL_SESSION_free().
     */
    return 0;
}

/*
 *  This callback function is executed by SSLeay whenever a
 *  SSL_SESSION is looked up in the internal SSLeay cache and it
 *  was not found. We use this to lookup the SSL_SESSION in the
 *  inter-process disk-cache where it was perhaps stored by one
 *  of our other Apache pre-forked server processes.
 */
SSL_SESSION *ssl_callback_GetSessionCacheEntry(
    SSL *ssl, unsigned char *id, int idlen, int *pCopy)
{
    conn_rec *conn;
    server_rec *s;
    SSL_SESSION *pSession;

    /*
     * Get Apache context back through SSLeay context
     */
    conn = (conn_rec *)SSL_get_app_data(ssl);
    s    = conn->server;

    /*
     * Try to retrieve the SSL_SESSION from the inter-process cache
     */
    pSession = ssl_scache_retrieve(s, id, idlen);

    /*
     * Log this cache operation
     */
    if (pSession != NULL)
        ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache: "
                "request=GET status=FOUND id=%s (session reuse)",
                ssl_scache_id2sz(id, idlen));
    else
        ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache: "
                "request=GET status=MISSED id=%s (session renewal)",
                ssl_scache_id2sz(id, idlen));

    /*
     * Return NULL or the retrieved SSL_SESSION. But indicate (by
     * setting pCopy to 0) that the reference count on the
     * SSL_SESSION should not be incremented by the SSL library,
     * because we will no longer hold a reference to it ourself.
     */
    *pCopy = 0;
    return pSession;
}

/*
 *  This callback function is executed by SSLeay whenever a
 *  SSL_SESSION is removed from the the internal SSLeay cache.
 *  We use this to remove the SSL_SESSION in the inter-process
 *  disk-cache, too.
 */
void ssl_callback_DelSessionCacheEntry(
    SSL_CTX *ctx, SSL_SESSION *pSession)
{
    server_rec *s;

    /*
     * Get Apache context back through SSLeay context
     */
    s = (server_rec *)SSL_CTX_get_app_data(ctx);

    /*
     * Remove the SSL_SESSION from the inter-process cache
     */
    ssl_scache_remove(s, pSession);

    /*
     * Log this cache operation
     */
    ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache: "
            "request=REM status=OK id=%s (session dead)",
            ssl_scache_id2sz(pSession->session_id,
            pSession->session_id_length));

    return;
}

/*
 * This callback function is executed while SSLeay processes the
 * SSL handshake and does SSL record layer stuff. We use it to
 * trace SSLeay's processing in out SSL logfile.
 */
void ssl_callback_LogTracingState(SSL *ssl, int where, int rc)
{
    conn_rec *c;
    server_rec *s;
    SSLSrvConfigRec *sc;
    char *str;

    /*
     * find corresponding server
     */
    if ((c = (conn_rec *)SSL_get_app_data(ssl)) == NULL)
        return;
    s = c->server;
    if ((sc = mySrvConfig(s)) == NULL)
        return;

    /*
     * create the various trace messages
     */
    if (sc->nLogLevel >= SSL_LOG_TRACE) {
        if (where & SSL_CB_HANDSHAKE_START)
            ssl_log(s, SSL_LOG_TRACE, "%s: Handshake: start", SSL_LIBRARY_NAME);
        else if (where & SSL_CB_HANDSHAKE_DONE)
            ssl_log(s, SSL_LOG_TRACE, "%s: Handshake: done", SSL_LIBRARY_NAME);
        else if (where & SSL_CB_LOOP)
            ssl_log(s, SSL_LOG_TRACE, "%s: Loop: %s", 
                    SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
        else if (where & SSL_CB_READ)
            ssl_log(s, SSL_LOG_TRACE, "%s: Read: %s",
                    SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
        else if (where & SSL_CB_WRITE)
            ssl_log(s, SSL_LOG_TRACE, "%s: Write: %s", 
                    SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
        else if (where & SSL_CB_ALERT) {
            str = (where & SSL_CB_READ) ? "read" : "write";
            ssl_log(s, SSL_LOG_TRACE, "%s: Alert: %s:%s\n", SSL_LIBRARY_NAME,
                    SSL_alert_type_string_long(rc),
                    SSL_alert_desc_string_long(rc));
        }
        else if (where & SSL_CB_EXIT) {
            if (rc == 0)
                ssl_log(s, SSL_LOG_TRACE, "%s: Exit: failed in %s", 
                        SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
            else if (rc < 0)
                ssl_log(s, SSL_LOG_TRACE, "%s: Exit: error in %s", 
                        SSL_LIBRARY_NAME, SSL_state_string_long(ssl));
        }
    }

    /*
     * Because SSL renegotations can happen at any time (not only after
     * SSL_accept()), the best way to log the current connection details is
     * right after a finished handshake.
     */
    if (where & SSL_CB_HANDSHAKE_DONE) {
        ssl_log(s, SSL_LOG_INFO,
                "Connection: Client IP: %s, Protocol: %s, Cipher: %s (%s/%s bits)",
                ssl_var_lookup(NULL, s, c, NULL, "REMOTE_ADDR"),
                ssl_var_lookup(NULL, s, c, NULL, "SSL_PROTOCOL"),
                ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER"),
                ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER_USEKEYSIZE"),
                ssl_var_lookup(NULL, s, c, NULL, "SSL_CIPHER_ALGKEYSIZE"));
    }

    return;
}

