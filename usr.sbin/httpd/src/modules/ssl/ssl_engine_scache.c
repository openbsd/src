/*                      _             _
**  _ __ ___   ___   __| |    ___ ___| |  mod_ssl
** | '_ ` _ \ / _ \ / _` |   / __/ __| |  Apache Interface to OpenSSL
** | | | | | | (_) | (_| |   \__ \__ \ |  www.modssl.org
** |_| |_| |_|\___/ \__,_|___|___/___/_|  ftp.modssl.org
**                      |_____|
**  ssl_engine_scache.c
**  Session Cache
*/

/* ====================================================================
 * Copyright (c) 1998-2000 Ralf S. Engelschall. All rights reserved.
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
 *     mod_ssl project (http://www.modssl.org/)."
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
 *     mod_ssl project (http://www.modssl.org/)."
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
 *  FIXME: There is no define in OpenSSL, but OpenSSL uses 1024*10,
 *  so 1024*20 should be ok.
 */
#define MAX_SESSION_DER 1024*20

void ssl_scache_init(server_rec *s, pool *p)
{
    SSLModConfigRec *mc = myModConfig();

    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_init(s, p);
    else if (mc->nSessionCacheMode == SSL_SCMODE_SHM)
        ssl_scache_shm_init(s, p);
    ssl_scache_expire(s, time(NULL));

#ifdef SSL_VENDOR
    ap_hook_use("ap::mod_ssl::vendor::scache_init",
                AP_HOOK_SIG3(void,ptr,ptr), AP_HOOK_ALL, s, p);
#endif

    return;
}

void ssl_scache_kill(server_rec *s)
{
    SSLModConfigRec *mc = myModConfig();

    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_kill(s);
    else if (mc->nSessionCacheMode == SSL_SCMODE_SHM)
        ssl_scache_shm_kill(s);

#ifdef SSL_VENDOR
    ap_hook_use("ap::mod_ssl::vendor::scache_kill",
                AP_HOOK_SIG1(void), AP_HOOK_ALL);
#endif
    return;
}

BOOL ssl_scache_store(server_rec *s, SSL_SESSION *pSession, int timeout)
{
    SSLModConfigRec *mc = myModConfig();
    ssl_scinfo_t SCI;
    UCHAR buf[MAX_SESSION_DER];
    UCHAR *b;
    BOOL rc = FALSE;

    /* add the key */
    SCI.ucaKey = pSession->session_id;
    SCI.nKey   = pSession->session_id_length;

    /* transform the session into a data stream */
    SCI.ucaData    = b = buf;
    SCI.nData      = i2d_SSL_SESSION(pSession, &b);
    SCI.tExpiresAt = timeout;

    /* and store it... */
    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        rc = ssl_scache_dbm_store(s, &SCI);
    else if (mc->nSessionCacheMode == SSL_SCMODE_SHM)
        rc = ssl_scache_shm_store(s, &SCI);

#ifdef SSL_VENDOR
    ap_hook_use("ap::mod_ssl::vendor::scache_store",
                AP_HOOK_SIG3(void,ptr,ptr), AP_HOOK_ALL, s, &SCI);
#endif
    
    /* allow the regular expiring to occur */
    ssl_scache_expire(s, time(NULL));

    return rc;
}

SSL_SESSION *ssl_scache_retrieve(server_rec *s, UCHAR *id, int idlen)
{
    SSLModConfigRec *mc = myModConfig();
    SSL_SESSION *pSession = NULL;
    ssl_scinfo_t SCI;
    time_t tNow;

    /* determine current time */
    tNow = time(NULL);

    /* allow the regular expiring to occur */
    ssl_scache_expire(s, tNow);

    /* create cache query */
    SCI.ucaKey     = id;
    SCI.nKey       = idlen;
    SCI.ucaData    = NULL;
    SCI.nData      = 0;
    SCI.tExpiresAt = 0;

    /* perform cache query */
    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_retrieve(s, &SCI);
    else if (mc->nSessionCacheMode == SSL_SCMODE_SHM)
        ssl_scache_shm_retrieve(s, &SCI);

#ifdef SSL_VENDOR
    ap_hook_use("ap::mod_ssl::vendor::scache_retrieve",
                AP_HOOK_SIG3(void,ptr,ptr), AP_HOOK_ALL, s, &SCI);
#endif

    /* return immediately if not found */
    if (SCI.ucaData == NULL)
        return NULL;

    /* check for expire time */
    if (SCI.tExpiresAt <= tNow) {
        if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
            ssl_scache_dbm_remove(s, &SCI);
        else if (mc->nSessionCacheMode == SSL_SCMODE_SHM)
            ssl_scache_shm_remove(s, &SCI);
#ifdef SSL_VENDOR
        ap_hook_use("ap::mod_ssl::vendor::scache_remove",
                    AP_HOOK_SIG3(void,ptr,ptr), AP_HOOK_ALL, s, &SCI);
#endif
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
    else if (mc->nSessionCacheMode == SSL_SCMODE_SHM)
        ssl_scache_shm_remove(s, &SCI);

#ifdef SSL_VENDOR
    ap_hook_use("ap::mod_ssl::vendor::scache_remove",
                AP_HOOK_SIG3(void,ptr,ptr), AP_HOOK_ALL, s, &SCI);
#endif

    return;
}

void ssl_scache_expire(server_rec *s, time_t now)
{
    SSLModConfigRec *mc = myModConfig();
    SSLSrvConfigRec *sc = mySrvConfig(s);
    static time_t last = 0;

    /*
     * make sure the expiration for still not-accessed session
     * cache entries is done only from time to time
     */
    if (now < last+sc->nSessionCacheTimeout)
        return;
    last = now;

    /*
     * Now perform the expiration 
     */
    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_expire(s, now);
    else if (mc->nSessionCacheMode == SSL_SCMODE_SHM)
        ssl_scache_shm_expire(s, now);

    return;
}

void ssl_scache_status(server_rec *s, pool *p, void (*func)(char *, void *), void *arg)
{
    SSLModConfigRec *mc = myModConfig();

    /* allow the regular expiring to occur */
    ssl_scache_expire(s, time(NULL));

    if (mc->nSessionCacheMode == SSL_SCMODE_DBM)
        ssl_scache_dbm_status(s, p, func, arg);
    else if (mc->nSessionCacheMode == SSL_SCMODE_SHM)
        ssl_scache_shm_status(s, p, func, arg);
    else
        func("N.A.", arg);
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
    ssl_mutex_on(s);
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDWR|O_CREAT, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot create SSLSessionCache DBM file `%s'",
                mc->szSessionCacheDataFile);
        ssl_mutex_off(s);
        return;
    }
    ssl_dbm_close(dbm);

#if !defined(OS2) && !defined(WIN32)
    /*
     * We have to make sure the Apache child processes
     * have access to the DBM file. But because there
     * are brain-dead platforms where we cannot exactly
     * determine the suffixes we try all possibilities.
     */
    if (geteuid() == 0 /* is superuser */) {
        chown(mc->szSessionCacheDataFile, ap_user_id, -1 /* no gid change */);
        if (chown(ap_pstrcat(p, mc->szSessionCacheDataFile, SSL_DBM_FILE_SUFFIX_DIR, NULL),
                  ap_user_id, -1) == -1) {
            if (chown(ap_pstrcat(p, mc->szSessionCacheDataFile, ".db", NULL),
                      ap_user_id, -1) == -1)
                chown(ap_pstrcat(p, mc->szSessionCacheDataFile, ".dir", NULL),
                      ap_user_id, -1);
        }
        if (chown(ap_pstrcat(p, mc->szSessionCacheDataFile, SSL_DBM_FILE_SUFFIX_PAG, NULL),
                  ap_user_id, -1) == -1) {
            if (chown(ap_pstrcat(p, mc->szSessionCacheDataFile, ".db", NULL),
                      ap_user_id, -1) == -1)
                chown(ap_pstrcat(p, mc->szSessionCacheDataFile, ".pag", NULL),
                      ap_user_id, -1);
        }
    }
#endif
    ssl_mutex_off(s);

    return;
}

void ssl_scache_dbm_kill(server_rec *s)
{
    SSLModConfigRec *mc = myModConfig();
    pool *p;

    if ((p = ap_make_sub_pool(NULL)) != NULL) {
        /* the correct way */
        unlink(ap_pstrcat(p, mc->szSessionCacheDataFile, SSL_DBM_FILE_SUFFIX_DIR, NULL));
        unlink(ap_pstrcat(p, mc->szSessionCacheDataFile, SSL_DBM_FILE_SUFFIX_PAG, NULL));
        /* the additional ways to be sure */
        unlink(ap_pstrcat(p, mc->szSessionCacheDataFile, ".dir", NULL));
        unlink(ap_pstrcat(p, mc->szSessionCacheDataFile, ".pag", NULL));
        unlink(ap_pstrcat(p, mc->szSessionCacheDataFile, ".db", NULL));
        unlink(mc->szSessionCacheDataFile);
        ap_destroy_pool(p);
    }
    return;
}

BOOL ssl_scache_dbm_store(server_rec *s, ssl_scinfo_t *SCI)
{
    SSLModConfigRec *mc = myModConfig();
    DBM *dbm;
    datum dbmkey;
    datum dbmval;

    /* be careful: do not try to store too much bytes in a DBM file! */
#ifdef SSL_USE_SDBM
    if ((SCI->nKey + SCI->nData) >= PAIRMAX)
        return FALSE;
#else
    if ((SCI->nKey + SCI->nData) >= 950 /* at least less than approx. 1KB */)
        return FALSE;
#endif

    /* create DBM key */
    dbmkey.dptr  = (char *)(SCI->ucaKey);
    dbmkey.dsize = SCI->nKey;

    /* create DBM value */
    dbmval.dsize = sizeof(time_t) + SCI->nData;
    dbmval.dptr  = (char *)malloc(dbmval.dsize);
    if (dbmval.dptr == NULL)
        return FALSE;
    memcpy((char *)dbmval.dptr, &SCI->tExpiresAt, sizeof(time_t));
    memcpy((char *)dbmval.dptr+sizeof(time_t), SCI->ucaData, SCI->nData);

    /* and store it to the DBM file */
    ssl_mutex_on(s);
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDWR, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot open SSLSessionCache DBM file `%s' for writing (store)",
                mc->szSessionCacheDataFile);
        ssl_mutex_off(s);
        free(dbmval.dptr);
        return FALSE;
    }
    if (ssl_dbm_store(dbm, dbmkey, dbmval, DBM_INSERT) < 0) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot store SSL session to DBM file `%s'",
                mc->szSessionCacheDataFile);
        ssl_dbm_close(dbm);
        ssl_mutex_off(s);
        free(dbmval.dptr);
        return FALSE;
    }
    ssl_dbm_close(dbm);
    ssl_mutex_off(s);

    /* free temporary buffers */
    free(dbmval.dptr);

    return TRUE;
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
    dbmkey.dptr  = (char *)(SCI->ucaKey);
    dbmkey.dsize = SCI->nKey;

    /* and fetch it from the DBM file */
    ssl_mutex_on(s);
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDONLY, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot open SSLSessionCache DBM file `%s' for reading (fetch)",
                mc->szSessionCacheDataFile);
        ssl_mutex_off(s);
        return;
    }
    dbmval = ssl_dbm_fetch(dbm, dbmkey);
    ssl_dbm_close(dbm);
    ssl_mutex_off(s);

    /* immediately return if not found */
    if (dbmval.dptr == NULL || dbmval.dsize <= sizeof(time_t))
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
    dbmkey.dptr  = (char *)(SCI->ucaKey);
    dbmkey.dsize = SCI->nKey;

    /* and delete it from the DBM file */
    ssl_mutex_on(s);
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDWR, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot open SSLSessionCache DBM file `%s' for writing (delete)",
                mc->szSessionCacheDataFile);
        ssl_mutex_off(s);
        return;
    }
    ssl_dbm_delete(dbm, dbmkey);
    ssl_dbm_close(dbm);
    ssl_mutex_off(s);

    return;
}

void ssl_scache_dbm_expire(server_rec *s, time_t tNow)
{
    SSLModConfigRec *mc = myModConfig();
    DBM *dbm;
    datum dbmkey;
    datum dbmval;
    pool *p;
    time_t tExpiresAt;
    int nElements = 0;
    int nDeleted = 0;
    int bDelete;
    datum *keylist;
    int keyidx;
    int i;

    /*
     * Here we have to be very carefully: Not all DBM libraries are
     * smart enough to allow one to iterate over the elements and at the
     * same time delete expired ones. Some of them get totally crazy
     * while others have no problems. So we have to do it the slower but
     * more safe way: we first iterate over all elements and remember
     * those which have to be expired. Then in a second pass we delete
     * all those expired elements. Additionally we reopen the DBM file
     * to be really safe in state.
     */

#define KEYMAX 1024

    ssl_mutex_on(s);
    for (;;) {
        /* allocate the key array in a memory sub pool */
        if ((p = ap_make_sub_pool(NULL)) == NULL)
            break;
        if ((keylist = ap_palloc(p, sizeof(dbmkey)*KEYMAX)) == NULL) {
            ap_destroy_pool(p);
            break;
        }

        /* pass 1: scan DBM database */
        keyidx = 0;
        if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                                O_RDWR, SSL_DBM_FILE_MODE)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                    "Cannot open SSLSessionCache DBM file `%s' for scanning",
                    mc->szSessionCacheDataFile);
            ap_destroy_pool(p);
            break;
        }
        dbmkey = ssl_dbm_firstkey(dbm);
        while (dbmkey.dptr != NULL) {
            nElements++;
            bDelete = FALSE;
            dbmval = ssl_dbm_fetch(dbm, dbmkey);
            if (dbmval.dsize <= sizeof(time_t) || dbmval.dptr == NULL)
                bDelete = TRUE;
            else {
                memcpy(&tExpiresAt, dbmval.dptr, sizeof(time_t));
                if (tExpiresAt <= tNow)
                    bDelete = TRUE;
            }
            if (bDelete) {
                if ((keylist[keyidx].dptr = ap_palloc(p, dbmkey.dsize)) != NULL) {
                    memcpy(keylist[keyidx].dptr, dbmkey.dptr, dbmkey.dsize);
                    keylist[keyidx].dsize = dbmkey.dsize;
                    keyidx++;
                    if (keyidx == KEYMAX)
                        break;
                }
            }
            dbmkey = ssl_dbm_nextkey(dbm);
        }
        ssl_dbm_close(dbm);

        /* pass 2: delete expired elements */
        if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                                O_RDWR, SSL_DBM_FILE_MODE)) == NULL) {
            ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                    "Cannot re-open SSLSessionCache DBM file `%s' for expiring",
                    mc->szSessionCacheDataFile);
            ap_destroy_pool(p);
            break;
        }
        for (i = 0; i < keyidx; i++) {
            ssl_dbm_delete(dbm, keylist[i]);
            nDeleted++;
        }
        ssl_dbm_close(dbm);

        /* destroy temporary pool */
        ap_destroy_pool(p);

        if (keyidx < KEYMAX)
            break;
    }
    ssl_mutex_off(s);

    ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache (DBM) Expiry: "
            "old: %d, new: %d, removed: %d", nElements, nElements-nDeleted, nDeleted);
    return;
}

void ssl_scache_dbm_status(server_rec *s, pool *p, void (*func)(char *, void *), void *arg)
{
    SSLModConfigRec *mc = myModConfig();
    DBM *dbm;
    datum dbmkey;
    datum dbmval;
    int nElem;
    int nSize;
    int nAverage;

    nElem = 0;
    nSize = 0;
    ssl_mutex_on(s);
    if ((dbm = ssl_dbm_open(mc->szSessionCacheDataFile,
                            O_RDONLY, SSL_DBM_FILE_MODE)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR|SSL_ADD_ERRNO,
                "Cannot open SSLSessionCache DBM file `%s' for status retrival",
                mc->szSessionCacheDataFile);
        ssl_mutex_off(s);
        return;
    }
    dbmkey = ssl_dbm_firstkey(dbm);
    for ( ; dbmkey.dptr != NULL; dbmkey = ssl_dbm_nextkey(dbm)) {
        dbmval = ssl_dbm_fetch(dbm, dbmkey);
        if (dbmval.dptr == NULL)
            continue;
        nElem += 1;
        nSize += dbmval.dsize;
    }
    ssl_dbm_close(dbm);
    ssl_mutex_off(s);
    if (nSize > 0 && nElem > 0)
        nAverage = nSize / nElem;
    else
        nAverage = 0;
    func(ap_psprintf(p, "cache type: <b>DBM</b>, maximum size: <b>unlimited</b><br>"), arg);
    func(ap_psprintf(p, "current sessions: <b>%d</b>, current size: <b>%d</b> bytes<br>", nElem, nSize), arg);
    func(ap_psprintf(p, "average session size: <b>%d</b> bytes<br>", nAverage), arg);
    return;
}

/*  _________________________________________________________________
**
**  Session Cache Support (SHM)
**  _________________________________________________________________
*/

/*
 *  Wrapper functions for table library which resemble malloc(3) & Co 
 *  but use the variants from the MM shared memory library.
 */

static void *ssl_scache_shm_malloc(size_t size)
{
    SSLModConfigRec *mc = myModConfig();
    return ap_mm_malloc(mc->pSessionCacheDataMM, size);
}

static void *ssl_scache_shm_calloc(size_t number, size_t size)
{
    SSLModConfigRec *mc = myModConfig();
    return ap_mm_calloc(mc->pSessionCacheDataMM, number, size);
}

static void *ssl_scache_shm_realloc(void *ptr, size_t size)
{
    SSLModConfigRec *mc = myModConfig();
    return ap_mm_realloc(mc->pSessionCacheDataMM, ptr, size);
}

static void ssl_scache_shm_free(void *ptr)
{
    SSLModConfigRec *mc = myModConfig();
    ap_mm_free(mc->pSessionCacheDataMM, ptr);
    return;
}

/*
 * Now the actual session cache implementation
 * based on a hash table inside a shared memory segment.
 */

void ssl_scache_shm_init(server_rec *s, pool *p)
{
    SSLModConfigRec *mc = myModConfig();
    AP_MM *mm;
    table_t *ta;
    int ta_errno;
    int avail;
    int n;

    /*
     * Create shared memory segment
     */
    if (mc->szSessionCacheDataFile == NULL) {
        ssl_log(s, SSL_LOG_ERROR, "SSLSessionCache required");
        ssl_die();
    }
    if ((mm = ap_mm_create(mc->nSessionCacheDataSize, 
                           mc->szSessionCacheDataFile)) == NULL) {
        ssl_log(s, SSL_LOG_ERROR, 
                "Cannot allocate shared memory: %s", ap_mm_error());
        ssl_die();
    }
    mc->pSessionCacheDataMM = mm;

    /* 
     * Make sure the childs have access to the underlaying files
     */
    ap_mm_permission(mm, SSL_MM_FILE_MODE, ap_user_id, -1);

    /*
     * Create hash table in shared memory segment
     */
    avail = ap_mm_available(mm);
    n = (avail/2) / 1024;
    n = n < 10 ? 10 : n;
    if ((ta = table_alloc(n, &ta_errno, 
                          ssl_scache_shm_malloc,  
                          ssl_scache_shm_calloc, 
                          ssl_scache_shm_realloc, 
                          ssl_scache_shm_free    )) == NULL) {
        ssl_log(s, SSL_LOG_ERROR,
                "Cannot allocate hash table in shared memory: %s",
                table_strerror(ta_errno));
        ssl_die();
    }
    table_attr(ta, TABLE_FLAG_AUTO_ADJUST|TABLE_FLAG_ADJUST_DOWN);
    table_set_data_alignment(ta, sizeof(char *));
    table_clear(ta);
    mc->tSessionCacheDataTable = ta;

    /*
     * Log the done work
     */
    ssl_log(s, SSL_LOG_INFO, 
            "Init: Created hash-table (%d buckets) "
            "in shared memory (%d bytes) for SSL session cache", n, avail);
    return;
}

void ssl_scache_shm_kill(server_rec *s)
{
    SSLModConfigRec *mc = myModConfig();

    if (mc->pSessionCacheDataMM != NULL) {
        ap_mm_destroy(mc->pSessionCacheDataMM);
        mc->pSessionCacheDataMM = NULL;
    }
    return;
}

BOOL ssl_scache_shm_store(server_rec *s, ssl_scinfo_t *SCI)
{
    SSLModConfigRec *mc = myModConfig();
    void *vp;

    ssl_mutex_on(s);
    if (table_insert_kd(mc->tSessionCacheDataTable, 
                        SCI->ucaKey, SCI->nKey, 
                        NULL, sizeof(time_t)+SCI->nData,
                        NULL, &vp, 1) != TABLE_ERROR_NONE) {
        ssl_mutex_off(s);
        return FALSE;
    }
    memcpy(vp, &SCI->tExpiresAt, sizeof(time_t));
    memcpy((char *)vp+sizeof(time_t), SCI->ucaData, SCI->nData);
    ssl_mutex_off(s);
    return TRUE;
}

void ssl_scache_shm_retrieve(server_rec *s, ssl_scinfo_t *SCI)
{
    SSLModConfigRec *mc = myModConfig();
    void *vp;
    int n;

    /* initialize result */
    SCI->ucaData    = NULL;
    SCI->nData      = 0;
    SCI->tExpiresAt = 0;

    /* lookup key in table */
    ssl_mutex_on(s);
    if (table_retrieve(mc->tSessionCacheDataTable,
                       SCI->ucaKey, SCI->nKey, 
                       &vp, &n) != TABLE_ERROR_NONE) {
        ssl_mutex_off(s);
        return;
    }

    /* copy over the information to the SCI */
    SCI->nData   = n-sizeof(time_t);
    SCI->ucaData = (UCHAR *)malloc(SCI->nData);
    if (SCI->ucaData == NULL) {
        SCI->nData = 0;
        ssl_mutex_off(s);
        return;
    }
    memcpy(&SCI->tExpiresAt, vp, sizeof(time_t));
    memcpy(SCI->ucaData, (char *)vp+sizeof(time_t), SCI->nData);
    ssl_mutex_off(s);

    return;
}

void ssl_scache_shm_remove(server_rec *s, ssl_scinfo_t *SCI)
{
    SSLModConfigRec *mc = myModConfig();

    /* remove value under key in table */
    ssl_mutex_on(s);
    table_delete(mc->tSessionCacheDataTable,
                 SCI->ucaKey, SCI->nKey, NULL, NULL);
    ssl_mutex_off(s);
    return;
}

void ssl_scache_shm_expire(server_rec *s, time_t tNow)
{
    SSLModConfigRec *mc = myModConfig();
    table_linear_t iterator;
    time_t tExpiresAt;
    void *vpKey;
    void *vpKeyThis;
    void *vpData;
    int nKey;
    int nKeyThis;
    int nData;
    int nElements = 0;
    int nDeleted = 0;
    int bDelete;
    int rc;

    ssl_mutex_on(s);
    if (table_first_r(mc->tSessionCacheDataTable, &iterator,
                      &vpKey, &nKey, &vpData, &nData) == TABLE_ERROR_NONE) {
        do {
            bDelete = FALSE;
            nElements++;
            if (nData < sizeof(time_t) || vpData == NULL)
                bDelete = TRUE;
            else {
                memcpy(&tExpiresAt, vpData, sizeof(time_t));
                if (tExpiresAt <= tNow)
                   bDelete = TRUE;
            }
            vpKeyThis = vpKey;
            nKeyThis  = nKey;
            rc = table_next_r(mc->tSessionCacheDataTable, &iterator,
                              &vpKey, &nKey, &vpData, &nData);
            if (bDelete) {
                table_delete(mc->tSessionCacheDataTable,
                             vpKeyThis, nKeyThis, NULL, NULL);
                nDeleted++;
            }
        } while (rc == TABLE_ERROR_NONE);
    }
    ssl_mutex_off(s);
    ssl_log(s, SSL_LOG_TRACE, "Inter-Process Session Cache (SHM) Expiry: "
            "old: %d, new: %d, removed: %d", nElements, nElements-nDeleted, nDeleted);
    return;
}

void ssl_scache_shm_status(server_rec *s, pool *p, void (*func)(char *, void *), void *arg)
{
    SSLModConfigRec *mc = myModConfig();
    void *vpKey;
    void *vpData;
    int nKey;
    int nData;
    int nElem;
    int nSize;
    int nAverage;

    nElem = 0;
    nSize = 0;
    ssl_mutex_on(s);
    if (table_first(mc->tSessionCacheDataTable,
                    &vpKey, &nKey, &vpData, &nData) == TABLE_ERROR_NONE) {
        do {
            if (vpKey == NULL || vpData == NULL)
                continue;
            nElem += 1;
            nSize += nData;
        } while (table_next(mc->tSessionCacheDataTable,
                            &vpKey, &nKey, &vpData, &nData) == TABLE_ERROR_NONE);
    }
    ssl_mutex_off(s);
    if (nSize > 0 && nElem > 0)
        nAverage = nSize / nElem;
    else
        nAverage = 0;
    func(ap_psprintf(p, "cache type: <b>SHM</b>, maximum size: <b>%d</b> bytes<br>", mc->nSessionCacheDataSize), arg);
    func(ap_psprintf(p, "current sessions: <b>%d</b>, current size: <b>%d</b> bytes<br>", nElem, nSize), arg);
    func(ap_psprintf(p, "average session size: <b>%d</b> bytes<br>", nAverage), arg);
    return;
}

