/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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
RCSID("$Id: koerror.c,v 1.3 2000/09/11 14:40:58 art Exp $");
#endif

#include <stdio.h>
#include <string.h>

#include <roken.h>

#include <vldb.h>
#include <volumeserver.h>
#include <pts.h>
#include <bos.h>
#include <ubik.h>
#include <rx/rx.h>
#include <rx/rxgencon.h>
#ifdef KERBEROS
#include <krb.h>
#include <des.h>
#include <rxkad.h>
#endif
#include <ko.h>
#include <fs_errors.h>

struct koerr {
    koerr_t code;
    const char *msg;
};

static struct koerr koerrmsg[] = {

    /* VL server errors */

    {VL_IDEXIST,        "VL - Volume Id entry exists in vl database."},
    {VL_IO,             "VL - I/O related error."},
    {VL_NAMEEXIST,      "VL - Volume name entry exists in vl database."},
    {VL_CREATEFAIL,     "VL - Internal creation failure."},
    {VL_NOENT,          "VL - No such entry."},
    {VL_EMPTY,          "VL - Vl database is empty."},
    {VL_ENTDELETED,     "VL - Entry is deleted (soft delete)."},
    {VL_BADNAME,        "VL - Volume name is illegal."},
    {VL_BADINDEX,       "VL - Index is out of range."},
    {VL_BADVOLTYPE,     "VL - Bad volume type."},
    {VL_BADPARTITION,   "VL - Illegal server number (out of range)."},
    {VL_BADSERVER,      "VL - Bad partition number."},
    {VL_REPSFULL,       "VL - Run out of space for Replication sites."},
    {VL_NOREPSERVER,    "VL - No such Replication server site exists."},
    {VL_DUPREPSERVER,   "VL - Replication site alreay exists."},
    {VL_RWNOTFOUND,     "VL - Parent R/W entry no found."},
    {VL_BADREFCOUNT,    "VL - Illegal reference count numner."},
    {VL_SIZEEXCEEDED,   "VL - Vl size for attributes exceeded."},
    {VL_BADENTRY,       "VL - Bad incming vl entry."},
    {VL_BADVOLIDBUMP,   "VL - Illegal max volid increment."},
    {VL_IDALREADHASED,  "VL - RO/BACK id already hashed."},
    {VL_ENTRYLOCKED,    "VL - Vl entry is already locked."},
    {VL_BADVOLOPER,     "VL - Bad volume operation code."},
    {VL_BADRELLOCKTYPE, "VL - Bad release lock type."},
    {VL_RERELEASE,      "VL - Status report: last release was aborted."},
    {VL_BADSERVERFLAG,  "VL - Invalid replication site server flag."},
    {VL_PERM,           "VL - No permission access."},
    {VL_NOMEM,          "VL - malloc(realloc) failed to alloc enough memory"},

    /* VOLSER errors */

    {VOLSERTRELE_ERROR,       "VOLSER - Internal error releasing "
                              "transaction."},
    {VOLSERNO_OP,             "VOLSER - Unknown internal error."},
    {VOLSERREAD_DUMPERROR,    "VOLSER - Badly formatted dump."},
    {VOLSERDUMPERROR,         "VOLSER - Badly formatted dump(2)."},
    {VOLSERATTACH_ERROR,      "VOLSER - Could not attach volume."},
    {VOLSERILLEGAL_PARTITION, "VOLSER - Illegal partition."},
    {VOLSERDETACH_ERROR,      "VOLSER - Could not detach volume."},
    {VOLSERBAD_ACCESS,        "VOLSER - Insufficient privilege for "
                              "volume operation."},
    {VOLSERVLDB_ERROR,        "VOLSER - Error from volume location database."},
    {VOLSERBADNAME,           "VOLSER - Bad volume name."},
    {VOLSERVOLMOVED,          "VOLSER - Volume moved."},
    {VOLSERBADOP,             "VOLSER - Illegal volume operation."},
    {VOLSERBADRELEASE,        "VOLSER - Volume release failed."},
    {VOLSERVOLBUSY,           "VOLSER - Volume still in use by volserver."},
    {VOLSERNO_MEMORY,         "VOLSER - Out of virtual memory."},
    {VOLSERNOVOL,	      "VOLSER - No such volume."},
    {VOLSERMULTIRWVOL,        "VOLSER - More then one read/write volume."},
    {VOLSERFAILEDOP,          "VOLSER - Failed volume server operation."},

    {PREXIST, 		      "PR - Entry exist."},
    {PRIDEXIST,		      "PR - Id exist."},
    {PRNOIDS,		      "PR - No Ids."},
    {PRDBFAIL,		      "PR - Protection-database failed."},
    {PRNOENT,		      "PR - No entry."},
    {PRPERM,		      "PR - Permission denied."},
    {PRNOTGROUP,	      "PR - Not a group."},
    {PRNOTUSER,	              "PR - Not a user."},
    {PRBADNAM,	              "PR - Bad name."},
    {PRBADARG,	              "PR - Bad argument."},
    {PRNOMORE,	              "PR - No more (?)."},
    {PRDBBAD,	              "PR - Protection-database went bad."},
    {PRGROUPEMPTY,	      "PR - Empty group."},
    {PRINCONSISTENT,	      "PR - Database inconsistency."},
    {PRBADDR,	              "PR - Bad address."},
    {PRTOOMANY,	              "PR - Too many."},

    {RXGEN_CC_MARSHAL,	      "rxgen - cc_marshal"},
    {RXGEN_CC_UNMARSHAL,      "rxgen - cc_unmarshal"},
    {RXGEN_SS_MARSHAL,	      "rxgen - ss_marshal"},
    {RXGEN_SS_UNMARSHAL,      "rxgen - ss_unmarshal"},
    {RXGEN_DECODE,	      "rxgen - decode"},
    {RXGEN_OPCODE,	      "rxgen - opcode"},
    {RXGEN_SS_XDRFREE,	      "rxgen - ss_xdrfree"},
    {RXGEN_CC_XDRFREE,	      "rxgen - cc_xdrfree"},

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

    {ARLA_VSALVAGE,           "arla-fs-error - salvaging"},
    {ARLA_VNOVNODE,           "arla-fs-error - no such vnode"},
    {ARLA_VNOVOL,             "arla-fs-error - no such volume"},
    {ARLA_VVOLEXISTS,         "arla-fs-error - volume already exists"},
    {ARLA_VNOSERVICE,         "arla-fs-error - no service"},
    {ARLA_VOFFLINE,           "arla-fs-error - volume offline"},
    {ARLA_VONLINE,            "arla-fs-error - volume online"},
    {ARLA_VDISKFULL,          "arla-fs-error - disk full"},
    {ARLA_VOVERQUOTA,         "arla-fs-error - quoua full"},
    {ARLA_VBUSY,              "arla-fs-error - busy volume"},
    {ARLA_VMOVED,             "arla-fs-error - moved volume"},
    {ARLA_VIO,                "arla-fs-error - I/O error"},
    {ARLA_VRESTARTING,        "arla-fs-error - restarting"},

    {BZNOTACTIVE, 	      "bos - Not active"},
    {BZNOENT,		      "bos - No entry"},
    {BZBUSY,		      "bos - Busy"},
    {BZNOCREATE,	      "bos - No able to create"},
    {BZDOM,		      "bos - Out of domain"},
    {BZACCESS,		      "bos - No access"},
    {BZSYNTAX,		      "bos - Syntax error"},
    {BZIO,		      "bos - I/O error"},
    {BZNET,		      "bos - Network error"},
    {BZBADTYPE,		      "bos - Bad type"},

    /* ubik errors */

    {UNOQUORUM,		      "no quorum elected"},
    {UNOTSYNC,		      "not synchronization site (should work on sync site)"},
    {UNHOSTS,		      "too many hosts"},
    {UIOERROR,		      "I/O error writing dbase or log"},
    {UINTERNAL,		      "mysterious internal error"},
    {USYNC,		      "major synchronization error"},
    {UNOENT,		      "file not found when processing dbase"},
    {UBADLOCK,		      "bad lock range size (must be 1)"},
    {UBADLOG,		      "read error reprocessing log"},
    {UBADHOST,		      "problems with host name"},
    {UBADTYPE,		      "bad operation for this transaction type"},
    {UTWOENDS,		      "two commits or aborts done to transaction"},
    {UDONE,		      "operation done after abort (or commmit)"},
    {UNOSERVERS,	      "no servers appear to be up"},
    {UEOF,		      "premature EOF"},
    {ULOGIO,		      "error writing log file"},
    {UBADFAM,		      "UBADFAM"},
    {UBADCELL,		      "UBADCELL"},
    {UBADSECGRP,	      "UBADSECGRP"},
    {UBADGROUP,	              "UBADGROUP"},
    {UBADUUID,	              "UBADUUID"},
    {UNOMEM,	              "UNOMEM"},
    {UNOTMEMBER,	      "UNOTMEMBER"},
    {UNBINDINGS,	      "UNBINDINGS"},
    {UBADPRINNAME,	      "UBADPRINNAME"},
    {UPIPE,	              "UPIPE"},
    {UDEADLOCK,	              "UDEADLOCK"},
    {UEXCEPTION,	      "UEXCEPTION"},
    {UTPQFAIL,	              "UTPQFAIL"},
    {USKEWED,	              "USKEWED"},
    {UNOLOCK,	              "UNOLOCK"},
    {UNOACCESS,	              "UNOACCESS"},
    {UNOSPC,	              "UNOSPC"},
    {UBADPATH,	              "UBADPATH"},
    {UBADF,	              "UBADF"},
    {UREINITIALIZE,	      "UREINITIALIZE"},

    /* Not a known error */

    { 0L,              "Unknown error"}
};



const char *
koerr_gettext(koerr_t err) 
{
    const char *ret = NULL;
    struct koerr *koerror = koerrmsg;

    while (koerror->code != 0) {
	if (err == koerror->code)
	    break;
	++koerror;
    }

    if (koerror->code == 0)
	ret = strerror(err);
    if (!ret)
	ret = koerror->msg;

    return ret;
}
