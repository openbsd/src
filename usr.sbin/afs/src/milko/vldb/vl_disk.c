/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

#include "vldb_locl.h"

RCSID("$arla: vl_disk.c,v 1.1 2001/10/03 00:47:54 map Exp $");

void
vldb_entry_to_disk(const struct vldbentry *entry,
		   struct disk_vlentry *diskentry)
{
    int i;
    int nservers;

    memset(diskentry, 0, sizeof(*diskentry));
    diskentry->volumeId[RWVOL] = entry->volumeId[RWVOL];
    diskentry->volumeId[ROVOL] = entry->volumeId[ROVOL];
    diskentry->volumeId[BACKVOL] = entry->volumeId[BACKVOL];
    diskentry->flags = entry->flags;
    diskentry->cloneId = entry->cloneId;
    diskentry->name = strdup(entry->name);

    nservers = min(entry->nServers, NMAXNSERVERS);

    diskentry->serverNumber.len = nservers;
    diskentry->serverPartition.len = nservers;
    diskentry->serverFlags.len = nservers;

    diskentry->serverNumber.val = malloc(sizeof(int32_t) * nservers);
    diskentry->serverPartition.val = malloc(sizeof(int32_t) * nservers);
    diskentry->serverFlags.val = malloc(sizeof(int32_t) * nservers);

    for (i = 0; i < nservers; i++) {
	diskentry->serverNumber.val[i] = entry->serverNumber[i];
	diskentry->serverPartition.val[i] = entry->serverPartition[i];
	diskentry->serverFlags.val[i] = entry->serverFlags[i];
    }
}

void
vldb_nentry_to_disk(const struct nvldbentry *entry,
		    struct disk_vlentry *diskentry)
{
    int i;
    int nservers;

    memset(diskentry, 0, sizeof(*diskentry));
    diskentry->volumeId[RWVOL] = entry->volumeId[RWVOL];
    diskentry->volumeId[ROVOL] = entry->volumeId[ROVOL];
    diskentry->volumeId[BACKVOL] = entry->volumeId[BACKVOL];
    diskentry->flags = entry->flags;
    diskentry->cloneId = entry->cloneId;
    diskentry->name = strdup(entry->name);

    nservers = min(entry->nServers, NMAXNSERVERS);

    diskentry->serverNumber.len = nservers;
    diskentry->serverPartition.len = nservers;
    diskentry->serverFlags.len = nservers;

    diskentry->serverNumber.val = malloc(sizeof(int32_t) * nservers);
    diskentry->serverPartition.val = malloc(sizeof(int32_t) * nservers);
    diskentry->serverFlags.val = malloc(sizeof(int32_t) * nservers);

    for (i = 0; i < nservers; i++) {
	diskentry->serverNumber.val[i] = entry->serverNumber[i];
	diskentry->serverPartition.val[i] = entry->serverPartition[i];
	diskentry->serverFlags.val[i] = entry->serverFlags[i];
    }
}


void
vldb_disk_to_entry(const struct disk_vlentry *diskentry,
		   struct vldbentry *entry)
{
    int i;

    memset(entry, 0, sizeof(*entry));
    entry->volumeId[RWVOL] = diskentry->volumeId[RWVOL];
    entry->volumeId[ROVOL] = diskentry->volumeId[ROVOL];
    entry->volumeId[BACKVOL] = diskentry->volumeId[BACKVOL];
    entry->flags = diskentry->flags;
    entry->cloneId = diskentry->cloneId;

    strncpy(entry->name, diskentry->name, VLDB_MAXNAMELEN);

    entry->nServers = min(MAXNSERVERS, diskentry->serverNumber.len);

    for (i = 0; i < entry->nServers; i++) {
	entry->serverNumber[i] = diskentry->serverNumber.val[i];
	entry->serverPartition[i] = diskentry->serverPartition.val[i];
	entry->serverFlags[i] = diskentry->serverFlags.val[i];
    }
}

void
vldb_disk_to_nentry(const struct disk_vlentry *diskentry,
		    struct nvldbentry *entry)
{
    int i;

    memset(entry, 0, sizeof(*entry));
    entry->volumeId[RWVOL] = diskentry->volumeId[RWVOL];
    entry->volumeId[ROVOL] = diskentry->volumeId[ROVOL];
    entry->volumeId[BACKVOL] = diskentry->volumeId[BACKVOL];
    entry->flags = diskentry->flags;
    entry->cloneId = diskentry->cloneId;

    strncpy(entry->name, diskentry->name, VLDB_MAXNAMELEN);

    entry->nServers = min(NMAXNSERVERS, diskentry->serverNumber.len);

    for (i = 0; i < entry->nServers; i++) {
	entry->serverNumber[i] = diskentry->serverNumber.val[i];
	entry->serverPartition[i] = diskentry->serverPartition.val[i];
	entry->serverFlags[i] = diskentry->serverFlags.val[i];
    }
}


void
vldb_free_diskentry(struct disk_vlentry *diskentry)
{
    free(diskentry->name);
    free(diskentry->serverNumber.val);
    free(diskentry->serverPartition.val);
    free(diskentry->serverFlags.val);
}
