/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |
** | '_ ` _ \ / _ \ / _` |   / __/ __| |
** | | | | | | (_) | (_| |   \__ \__ \ | mod_ssl - Apache Interface to SSLeay
** |_| |_| |_|\___/ \__,_|___|___/___/_| http://www.engelschall.com/sw/mod_ssl/
**                      |_____|
**  ssl_engine_scache.c
**  Session Cache
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
                             /* ``Open-Source Software: generous
                                  programmers from around the world all
                                  join forces to help you shoot
                                  yourself in the foot for free.''
                                                 -- Unknown         */
#include "mod_ssl.h"


/*  _________________________________________________________________
**
**  Session Cache Support (Common)
**  _________________________________________________________________
*/

/*
 *  FIXME: There is no define in SSLeay, but SSLeay uses 1024*10,
 *  so 1024*20 should be ok.
 */
#define MAX_SESSION_DER 1024*20

void ssl_scache_init(server_rec *s, pool *p)
{
    SSLModConfigRec *mc = myModConfig();

    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_init(s, p);
    ssl_scache_expire(s);
    return;
}

void ssl_scache_store(server_rec *s, SSL_SESSION *pSession, int timeout)
{
    SSLModConfigRec *mc = myModConfig();
    ssl_scinfo_t SCI;
    UCHAR buf[MAX_SESSION_DER];
    UCHAR *b;

    /* add the key */
    SCI.ucaKey = pSession->session_id;
    SCI.nKey   = pSession->session_id_length;

    /* transform the session into a data stream */
    SCI.ucaData    = b = buf;
    SCI.nData      = i2d_SSL_SESSION(pSession, &b);
    SCI.tExpiresAt = timeout;

    /* and store it... */
    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_store(s, &SCI);

    return;
}

SSL_SESSION *ssl_scache_retrieve(server_rec *s, UCHAR *id, int idlen)
{
    SSLModConfigRec *mc = myModConfig();
    SSL_SESSION *pSession = NULL;
    ssl_scinfo_t SCI;
    time_t tNow;

    /* create cache query */
    SCI.ucaKey     = id;
    SCI.nKey       = idlen;
    SCI.ucaData    = NULL;
    SCI.nData      = 0;
    SCI.tExpiresAt = 0;

    /* perform cache query */
    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_retrieve(s, &SCI);

    /* return immediately if not found */
    if (SCI.ucaData == NULL)
        return NULL;

    /* check for expire time */
    tNow = time(NULL);
    if (SCI.tExpiresAt <= tNow) {
        if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
            ssl_scache_dbm_remove(s, &SCI);
        return NULL;
    }

    /* extract result and return it */
    pSession = d2i_SSL_SESSION(NULL, &SCI.ucaData, SCI.nData);
    return pSession;
}

void ssl_scache_remove(server_rec *s, SSL_SESSION *pSession)
{
    SSLModConfigRec *mc = myModConfig();
    ssl_scinfo_t SCI;

    /* create cache query */
    SCI.ucaKey     = pSession->session_id;
    SCI.nKey       = pSession->session_id_length;
    SCI.ucaData    = NULL;
    SCI.nData      = 0;
    SCI.tExpiresAt = 0;

    /* perform remove */
    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_remove(s, &SCI);

    return;
}

void ssl_scache_expire(server_rec *s)
{
    SSLModConfigRec *mc = myModConfig();

    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_expire(s);
    return;
}

char *ssl_scache_id2sz(UCHAR *id, int idlen)
{
    static char str[(SSL_MAX_SSL_SESSION_ID_LENGTH+1)*2];
    char *cp;
    int n;

    cp = str;
    for (n = 0; n < idlen && n < SSL_MAX_SSL_SESSION_ID_LENGTH; n++) {
        ap_snprintf(cp, sizeof(str)-(cp-str), "%02X", id[n]);
        cp += 2;
    }
    *cp = NUL;
    return str;
}


/*  _________________________________________________________________
**
**  Session Cache Support (DBM)
**  _________________________________________________________________
*/

void ssl_scache_dbm_init(server_rec *s, pool *p)
{
    SSLModConfigRec *mc = myModConfig();
    DBM *dbm;

    /*
     * for the DBM we need the data file
     */
    if (mc->szSessionCacheDataFile == NULL) {
        ssl_log(s, SSL_LOG_ERROR, "SSLSessionCache required");
        ssl_die();
    }

    /*
     * Open it once to create it and to make sure it
     * _can_ be created.
     */
    ssl_mutex_on();
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDWR|O_CREAT, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot create SSLSessionCache DBM file `%s'",
                mc->szSessionCacheDataFile);
        ssl_mutex_off();
        return;
    }
    ssl_dbm_close(dbm);

#ifndef WIN32
    /*
     * we have to make sure the Apache child processes
     * have access to the DBM file...
     */
    if (geteuid() == 0 /* is superuser */) {
        chown(mc->szSessionCacheDataFile,
              ap_user_id, -1 /* no gid change */);
        chown(ap_pstrcat(p, mc->szSessionCacheDataFile,
                         SSL_DBM_FILE_SUFFIX_DIR, NULL),
              ap_user_id, -1 /* no gid change */);
        chown(ap_pstrcat(p, mc->szSessionCacheDataFile,
                         SSL_DBM_FILE_SUFFIX_PAG, NULL),
              ap_user_id, -1 /* no gid change */);
    }
#endif
    ssl_mutex_off();

    return;
}

void ssl_scache_dbm_store(server_rec *s, ssl_scinfo_t *SCI)
{
    SSLModConfigRec *mc = myModConfig();
    DBM *dbm;
    datum dbmkey;
    datum dbmval;

    /* create DBM key */
    dbmkey.dptr  = SCI->ucaKey;
    dbmkey.dsize = SCI->nKey;

    /* create DBM value */
    dbmval.dsize = sizeof(time_t)+SCI->nData;
    dbmval.dptr  = (UCHAR *)malloc(dbmval.dsize);
    if (dbmval.dptr == NULL)
        return;
    memcpy(dbmval.dptr, &SCI->tExpiresAt, sizeof(time_t));
    memcpy((char *)dbmval.dptr+sizeof(time_t), SCI->ucaData, SCI->nData);

    /* and store it to the DBM file */
    ssl_mutex_on();
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDWR, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot open SSLSessionCache DBM file `%s' for writing (store)",
                mc->szSessionCacheDataFile);
        ssl_mutex_off();
        return;
    }
    ssl_dbm_store(dbm, dbmkey, dbmval, DBM_INSERT);
    ssl_dbm_close(dbm);
    ssl_mutex_off();

    /* free temporary buffers */
    free(dbmval.dptr);

    return;
}

void ssl_scache_dbm_retrieve(server_rec *s, ssl_scinfo_t *SCI)
{
    SSLModConfigRec *mc = myModConfig();
    DBM *dbm;
    datum dbmkey;
    datum dbmval;

    /* initialize result */
    SCI->ucaData    = NULL;
    SCI->nData      = 0;
    SCI->tExpiresAt = 0;

    /* create DBM key and values */
    dbmkey.dptr  = SCI->ucaKey;
    dbmkey.dsize = SCI->nKey;

    /* and fetch it from the DBM file */
    ssl_mutex_on();
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDONLY, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot open SSLSessionCache DBM file `%s' for reading (fetch)",
                mc->szSessionCacheDataFile);
        ssl_mutex_off();
        return;
    }
    dbmval = ssl_dbm_fetch(dbm, dbmkey);
    ssl_dbm_close(dbm);
    ssl_mutex_off();

    /* immediately return if not found */
    if (dbmval.dptr == NULL || dbmval.dsize < sizeof(time_t))
        return;

    /* copy over the information to the SCI */
    SCI->nData   = dbmval.dsize-sizeof(time_t);
    SCI->ucaData = (UCHAR *)malloc(SCI->nData);
    if (SCI->ucaData == NULL) {
        SCI->nData = 0;
        return;
    }
    memcpy(SCI->ucaData, (char *)dbmval.dptr+sizeof(time_t), SCI->nData);
    memcpy(&SCI->tExpiresAt, dbmval.dptr, sizeof(time_t));

    return;
}

void ssl_scache_dbm_remove(server_rec *s, ssl_scinfo_t *SCI)
{
    SSLModConfigRec *mc = myModConfig();
    DBM *dbm;
    datum dbmkey;

    /* create DBM key and values */
    dbmkey.dptr  = SCI->ucaKey;
    dbmkey.dsize = SCI->nKey;

    /* and delete it from the DBM file */
    ssl_mutex_on();
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDWR, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot open SSLSessionCache DBM file `%s' for writing (delete)",
                mc->szSessionCacheDataFile);
        ssl_mutex_off();
        return;
    }
    ssl_dbm_delete(dbm, dbmkey);
    ssl_dbm_close(dbm);
    ssl_mutex_off();

    return;
}

void ssl_scache_dbm_expire(server_rec *s)
{
    SSLModConfigRec *mc = myModConfig();
    static int nExpireCalls = 0;
    DBM *dbm;
    datum dbmkey;
    datum dbmval;
    time_t tNow;
    time_t tExpiresAt;

    /*
     * It's to expensive to expire allways,
     * so do it only from time to time...
     */
    if (nExpireCalls++ < 100)
        return;
    else
        nExpireCalls = 0;

    ssl_mutex_on();
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDWR, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot open SSLSessionCache DBM file `%s' for expiring",
                mc->szSessionCacheDataFile);
        ssl_mutex_off();
        return;
    }
    tNow = time(NULL);
    dbmkey = ssl_dbm_firstkey(dbm);
    for ( ; dbmkey.dptr != NULL; dbmkey = ssl_dbm_nextkey(dbm)) {
        dbmval = ssl_dbm_fetch(dbm, dbmkey);
        if (dbmval.dptr == NULL)
            continue;
        if (dbmval.dsize < sizeof(time_t)) {
            ssl_dbm_delete(dbm, dbmkey);
            continue;
        }
        memcpy(&tExpiresAt, dbmval.dptr, sizeof(time_t));
        if (tExpiresAt >= tNow)
            ssl_dbm_delete(dbm, dbmkey);
    }
    ssl_dbm_close(dbm);
    ssl_mutex_off();

    return;
}

