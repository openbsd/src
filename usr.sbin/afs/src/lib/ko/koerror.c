/*	$OpenBSD: koerror.c,v 1.1.1.1 1998/09/14 21:53:00 art Exp $	*/
/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: koerror.c,v 1.7 1998/04/03 03:36:32 assar Exp $");
#endif

#include <stdio.h>
#include <string.h>

#include <roken.h>

#include <vldb.h>
#include <volumeserver.h>
#ifdef KERBEROS
#include <kerberosIV/krb.h>
#include <des.h>
#include <rxkad.h>
#endif
#include <ko.h>


struct koerr {
    koerr_t code;
    const char *msg;
};

static struct koerr koerrmsg[] = {

    /* VL server errors */

    {VL_IDEXIST,        "Volume Id entry exists in vl database."},
    {VL_IO,             "I/O related error."},
    {VL_NAMEEXIST,      "Volume name entry exists in vl database."},
    {VL_CREATEFAIL,     "Internal creation failure."},
    {VL_NOENT,          "No such entry."},
    {VL_EMPTY,          "Vl database is empty."},
    {VL_ENTDELETED,     "Entry is deleted (soft delete)."},
    {VL_BADNAME,        "Volume name is illegal."},
    {VL_BADINDEX,       "Index is out of range."},
    {VL_BADVOLTYPE,     "Bad volume type."},
    {VL_BADPARTITION,   "Illegal server number (out of range)."},
    {VL_BADSERVER,      "Bad partition number."},
    {VL_REPSFULL,       "Run out of space for Replication sites."},
    {VL_NOREPSERVER,    "No such Replication server site exists."},
    {VL_DUPREPSERVER,   "Replication site alreay exists."},
    {VL_RWNOTFOUND,     "Parent R/W entry no found."},
    {VL_BADREFCOUNT,    "Illegal reference count numner."},
    {VL_SIZEEXCEEDED,   "Vl size for attributes exceeded."},
    {VL_BADENTRY,       "Bad incming vl entry."},
    {VL_BADVOLIDBUMP,   "Illegal max volid increment."},
    {VL_IDALREADHASED,  "RO/BACK id already hashed."},
    {VL_ENTRYLOCKED,    "Vl entry is already locked."},
    {VL_BADVOLOPER,     "Bad volume operation code."},
    {VL_BADRELLOCKTYPE, "Bad release lock type."},
    {VL_RERELEASE,      "Status report: last release was aborted."},
    {VL_BADSERVERFLAG,  "Invalid replication site server flag."},
    {VL_PERM,           "No permission access."},
    {VL_NOMEM,          "malloc(realloc) failed to alloc enough memory"},

    /* VOLSER errors */

    {VOLSERTRELE_ERROR,       "Internal error releasing transaction."},
    {VOLSERNO_OP,             "Unknown internal error."},
    {VOLSERREAD_DUMPERROR,    "Badly formatted dump."},
    {VOLSERDUMPERROR,         "Badly formatted dump(2)."},
    {VOLSERATTACH_ERROR,      "Could not attach volume."},
    {VOLSERILLEGAL_PARTITION, "Illegal partition."},
    {VOLSERDETACH_ERROR,      "Could not detach volume."},
    {VOLSERBAD_ACCESS,        "Insufficient privilege for volume operation."},
    {VOLSERVLDB_ERROR,        "Error from volume location database."},
    {VOLSERBADNAME,           "Bad volume name."},
    {VOLSERVOLMOVED,          "Volume moved."},
    {VOLSERBADOP,             "Illegal volume operation."},
    {VOLSERBADRELEASE,        "Volume release failed."},
    {VOLSERVOLBUSY,           "Volume still in use by volserver."},
    {VOLSERNO_MEMORY,         "Out of virtual memory."},
    {VOLSERNOVOL,	      "No such volume."},
    {VOLSERMULTIRWVOL,        "More then one read/write volume."},
    {VOLSERFAILEDOP,          "Failed volume server operation."},

#ifdef KERBEROS
    /* rxkad - XXX more sane messages */

    {RXKADINCONSISTENCY,      "rxkad - Inconsistency."},
    {RXKADPACKETSHORT,        "rxkad - Packet too short."},
    {RXKADLEVELFAIL,          "rxkad - Security level failed."},
    {RXKADTICKETLEN,          "rxkad - Invaild ticket length."},
    {RXKADOUTOFSEQUENCE,      "rxkad - Out of sequence."},
    {RXKADNOAUTH,             "rxkad - No authentication."},
    {RXKADBADKEY,             "rxkad - Bad key."},
    {RXKADBADTICKET,          "rxkad - Bad ticket."},
    {RXKADUNKNOWNKEY,         "rxkad - Unknown key."},
    {RXKADEXPIRED,            "rxkad - Ticket expired."},
    {RXKADSEALEDINCON,        "rxkad - Seal inconsistency."},
    {RXKADDATALEN,            "rxkad - Datalength error."},
    {RXKADILLEGALLEVEL,       "rxkad - Illegal level."},

#endif

    /* Not a known error */

    { 0L,              "Unknown error"}
};



const char *
koerr_gettext(koerr_t err) 
{
    struct koerr *koerror = koerrmsg;

    while (koerror->code != 0) {
	if (err == koerror->code)
	    break;
	++koerror;
    }

    if (koerror->code)
	return koerror->msg;
    else
	return strerror(err);
}
