/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

#include "ko_locl.h"
#include <volumeserver.h>

RCSID("$arla: vlmisc.c,v 1.6 2002/05/25 19:12:31 lha Exp $");

/*
 * Convert old style vldbentry `old` to newer vldbNentry style `new'
 */

void
vldb2vldbN (const vldbentry *old, nvldbentry *new)
{
    int i;

    strlcpy (new->name, old->name, VLDB_MAXNAMELEN);
    if (old->nServers > MAXNSERVERS)
	new->nServers = MAXNSERVERS;
    else
	new->nServers = old->nServers;
    
    for (i = 0; i < new->nServers ; i++) {
	new->serverNumber[i]    = old->serverNumber[i];
	new->serverPartition[i] = old->serverPartition[i];
	new->serverFlags[i]     = old->serverFlags[i];
    }
    for (i = 0; i < MAXTYPES ; i++)
	new->volumeId[i] = old->volumeId[i];
    new->cloneId = old->cloneId;
    new->flags = old->flags;
}

void
volintInfo2xvolintInfo (const volintInfo *old, xvolintInfo *new)
{
    memset (new, 0, sizeof(*new));
    strlcpy (new->name, old->name, sizeof(new->name));
    new->volid		= old->volid;
    new->type		= old->type;
    new->backupID	= old->backupID;
    new->parentID	= old->parentID;
    new->cloneID	= old->cloneID;
    new->status		= old->status;
    new->copyDate	= old->copyDate;
    new->inUse		= old->inUse;
    new->creationDate	= old->creationDate;
    new->accessDate	= old->accessDate;
    new->updateDate	= old->updateDate;
    new->backupDate	= old->backupDate;
    new->dayUse		= old->dayUse;
    new->filecount	= old->filecount;
    new->maxquota	= old->maxquota;
    new->size		= old->size;
}

/*
 * Suffixes for volume names.
 */

static char *volsuffixes[] = {
"",				/* RWVOL */
".readonly",			/* ROVOL */
".backup"			/* BACKVOL */
};

/*
 * canonicalize `volname' and return type of volume
 */

int
volname_canonicalize (char *volname)
{
    int i;
    int type = RWVOL;

    for (i = 0; i < MAXTYPES; ++i) {
	char *p = strrchr (volname, '.');
	if (p != NULL && strcmp(p, volsuffixes[i]) == 0) {
	    *p = '\0';
	    type = i;
	    break;
	}
    }
    return type;
}

/*
 * make specific volume name
 */

size_t
volname_specific (const char *volname, int type, char *buf, size_t buf_sz)
{
    assert (type >= 0 && type < MAXTYPES);

    return snprintf (buf, buf_sz, "%s%s", volname, volsuffixes[type]);
}


/*
 * get volume suffix
 */

const char *
volname_suffix (int type)
{
    assert (type >= 0 && type < MAXTYPES);

    return volsuffixes[type];
}


char *
vol_getopname(int32_t op, char *str, size_t sz)
{
    op &= VLOP_ALLOPERS;

    switch(op) {
    case VLOP_MOVE:
	strlcpy(str, "move", sz);
	break;
    case VLOP_RELEASE:
	strlcpy(str, "release", sz);
	break;
    case VLOP_BACKUP:
	strlcpy(str, "backup", sz);
	break;
    case VLOP_DELETE:
	strlcpy(str, "delete", sz);
	break;
    case VLOP_DUMP:
	strlcpy(str, "dump", sz);
	break;
    default:
	snprintf(str, sz, "unknown flag %x\n", op);
	break;
    }
    return str;
}

/*
 * Print the first of RW, RO, or BACKUP that there's a clone of
 * according to serverFlag.
 */

const char *
volumetype_from_serverflag(int32_t flag)
{
    const char *str;

    if (flag & VLSF_RWVOL)
	str = "RW";
    else if (flag & VLSF_ROVOL)
	str = "RO";
    else if (flag & VLSF_ROVOL)
	str = "RO";
    else if (flag & VLSF_BACKVOL)
	str = "BACKUP";
    else
	str = "<unknown>";

    return str;
}

/*
 * Convert a volume `type' to a string.
 */

const char *
volumetype_from_volsertype(int32_t type)
{
    const char *str;

    switch (type) {
    case RWVOL:
	str = "RW";
	break;
    case ROVOL:
	str = "RO";
	break;
    case BACKVOL:
	str = "BK";
	break;
    default:
	str = "<unknown>";
    }
    return str;
}
