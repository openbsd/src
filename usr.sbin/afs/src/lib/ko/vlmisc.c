/*	$OpenBSD: vlmisc.c,v 1.1 1999/04/30 01:59:11 art Exp $	*/
/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

#include "ko_locl.h"

RCSID("$KTH: vlmisc.c,v 1.1 1999/03/03 15:33:21 assar Exp $");

/*
 * Convert old style vldbentry `old` to newer vldbNentry style `new'
 */

void
vldb2vldbN (const vldbentry *old, nvldbentry *new)
{
    int i;

    strncpy (new->name, old->name, VLDB_MAXNAMELEN);
    new->name[VLDB_MAXNAMELEN-1] = '\0';
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
    strcpy (new->name, old->name);
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
