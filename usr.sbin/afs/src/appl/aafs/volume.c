/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <volumeserver.cs.h>
#include <vldb.cs.h>
#include <ko.h>
#include <ports.h>
#include <service.h>

#include <aafs/aafs_volume.h>
#include <aafs/aafs_conn.h>
#include <aafs/aafs_private.h>
#include <aafs/aafs_partition.h>

#include <roken.h>

struct aafs_volume {
    struct aafs_object obj;
    char name[VLDB_MAXNAMELEN];
    struct aafs_cell *cell;
    unsigned long flags;
#define AAFS_VOLUME_HAVE_VLDB_INFO	1
    nvldbentry vldb;
};

static void
volume_destruct(void *ptr, const char *name)
{
    struct aafs_volume *v = ptr;
    if (v->cell)
	aafs_cell_unref(v->cell);
}

int
aafs_volume_create(struct aafs_cell *cell,
		   const char *volumename, 
		   unsigned long flags,
		   struct aafs_volume **ret_volume)
{
    struct aafs_volume *v;

    *ret_volume = NULL;

    v = aafs_object_create(sizeof(*v), "volume", volume_destruct);

    v->cell = aafs_cell_ref(cell);

    strlcpy(v->name, volumename, sizeof(v->name));

    if ((flags & AAFS_VOL_CREATE_NO_CANONICALIZE) == 0)
	volname_canonicalize(v->name);

    v->flags = 0;

    *ret_volume = v;

    return 0;
}

int
aafs_volume_attach(struct aafs_cell *cell,
		   unsigned long flags,
		   nvldbentry *vldb,
		   struct aafs_volume **volume)
{
    int ret;
    struct aafs_volume *v;

    if ((ret = aafs_volume_create(cell, vldb->name, flags, volume)) != 0)
	return ret;
    v = *volume;
    v->flags = AAFS_VOLUME_HAVE_VLDB_INFO;
    memcpy(&v->vldb, vldb, sizeof(v->vldb));
    return 0;
}

void
aafs_volume_free(struct aafs_volume *v)
{
    aafs_object_unref(v, "volume");
}

char *
aafs_volume_name(struct aafs_volume *volume, char *volname, size_t sz)
{
    snprintf(volname, sz, "%s", volume->name);
    return volname;
}


int
aafs_volume_examine_nvldb(struct aafs_volume *volume,
			  unsigned long flags,
			  struct nvldbentry *nvldb)
{
    struct aafs_cell_db_ctx *ctx;
    struct rx_connection *conn;
    int oldservers = 0;
    int ret;

    if (volume->flags & AAFS_VOLUME_HAVE_VLDB_INFO) {
	memcpy(nvldb, &volume->vldb, sizeof(*nvldb));
	return 0;
    }

    
 retry:
    conn = aafs_cell_first_db(volume->cell,
			      htons(afsvldbport),
			      VLDB_SERVICE_ID,
			      &ctx);

    while(conn) {
	if (oldservers) {
	    vldbentry vldb;

	    ret = VL_GetEntryByName(conn, volume->name, &vldb);
	    if (ret == 0)
		vldb2vldbN(&vldb, nvldb);
	} else
	    ret = VL_GetEntryByNameN(conn, volume->name, nvldb);

	if (ret == RXGEN_OPCODE) {
	    oldservers = 1;
	    cell_db_free_context(ctx);
	    goto retry;
	}

	if (!aafs_cell_try_next_db(ret))
	    conn = NULL;
	else
	    conn = aafs_cell_next_db(ctx);
    }

    cell_db_free_context(ctx);

    if (ret == 0) {
	nvldb->name[VLDB_MAXNAMELEN-1] = '\0';
	memcpy(&volume->vldb, nvldb, sizeof(*nvldb));
	volume->flags |= AAFS_VOLUME_HAVE_VLDB_INFO;
    }

    return ret;
}

int
aafs_volume_print_nvldb(struct aafs_volume *v, FILE *f, unsigned long flags)
{
    struct nvldbentry *vldb = &v->vldb;
    char server_name[MAXHOSTNAMELEN];
    char part_name[17];
    int i;

    if ((v->flags & AAFS_VOLUME_HAVE_VLDB_INFO) == 0)
	return ENOENT;

    if ((flags & VOL_PRINT_VLDB_SKIP_NAME) == 0)
	fprintf(f, "%.*s\n", VLDB_MAXNAMELEN, vldb->name);

    printf("    ");
    if(vldb->flags & VLF_RWEXISTS)
	printf("RWrite: %u\t", vldb->volumeId[RWVOL]);
    if(vldb->flags & VLF_ROEXISTS)
	printf("ROnly: %u\t", vldb->volumeId[ROVOL]);
    if(vldb->flags & VLF_BACKEXISTS)
	printf("Backup: %u\t", vldb->volumeId[BACKVOL]);

    printf("\n    number of sites -> %d\n", vldb->nServers );
    
     for (i = 0; i < vldb->nServers; i++) {
	 struct aafs_server *s;
	 aafs_partition part;

	 printf("       ");
	 
	 aafs_server_create_by_long(v->cell, htonl(vldb->serverNumber[i]), &s);
	 aafs_server_get_name(s, server_name, sizeof(server_name));
	 aafs_server_free(s);
	 
	 part = aafs_partition_from_number(vldb->serverPartition[i]);
	 aafs_partition_name(part, part_name, sizeof(part_name));
	 printf("server %s partition %s %s Site",
		server_name, part_name,
		volumetype_from_serverflag(vldb->serverFlags[i]));

	 if (vldb->serverFlags[i] & VLSF_DONTUSE)
	     printf(" -- not replicated yet");

	 printf("\n");

     }
     
     if (vldb->flags & VLOP_ALLOPERS) {
	 char msg[100];

	 printf("Volume is currently LOCKED, reson: %s\n", 
		vol_getopname(vldb->flags, msg, sizeof(msg)));
     }


    return 0;
}

struct aafs_volume_info_entry {
    struct aafs_site *site;
    int status;
    xvolintInfo info;
};

struct aafs_volume_info {
    struct aafs_object obj;
    struct aafs_volume *volume;
    int num_info;
    struct aafs_volume_info_entry *info;
};

struct aafs_volume_info_ctx {
    struct aafs_object obj;
    struct aafs_volume_info *info;
    int current;
};

static void
info_destruct(void *ptr, const char *name)
{
    struct aafs_volume_info *info = ptr;
    int i;

    for (i = 0; i < info->num_info; i++)
	aafs_object_unref(info->info[i].site, "site");
    free(info->info);
    aafs_object_unref(info->volume, "volume");
}


int
aafs_volume_examine_info(struct aafs_volume *volume,
			 unsigned long flags,
			 struct aafs_volume_info **ret_info)
{
    struct aafs_volume_info *info;
    struct nvldbentry vldb;
    xvolEntries ie;
    struct rx_connection *conn;
    struct aafs_server *server;
    int ret, i, j, done;
    uint32_t serverflags, serverflagsmask;

    *ret_info = NULL;

    ret = aafs_volume_examine_nvldb(volume, 0, &vldb);
    if (ret)
	return ret;

    info = aafs_object_create(sizeof(*info), "volume-info", info_destruct);

    info->volume = aafs_object_ref(volume, "volume");
    info->num_info = 0;
    info->info = NULL;

    for (i = 0; i < vldb.nServers; i++) {
	uint32_t volid;

	conn = NULL;

	ie.val = NULL;

	serverflagsmask = 0;
	if (flags & VOL_EXA_VOLINFO_RW)
	    serverflagsmask |= VLSF_RWVOL;
	if (flags & VOL_EXA_VOLINFO_RO)
	    serverflagsmask |= VLSF_ROVOL;
	if (flags & VOL_EXA_VOLINFO_BU)
	    serverflagsmask |= VLSF_BACKVOL;

	if ((serverflagsmask & vldb.serverFlags[i]) == 0)
	    continue;

	ret = aafs_server_create_by_long(volume->cell, 
					 htonl(vldb.serverNumber[i]),
					 &server);
	if (ret)
	    continue;

	conn = aafs_conn_byserver(server, htons(afsvolport), VOLSER_SERVICE_ID);
	if (conn == NULL) {
	    aafs_object_unref(server, "server");
	    continue;
	}

	done = 1;
	serverflags = vldb.serverFlags[i]&(VLSF_RWVOL|VLSF_ROVOL|VLSF_BACKVOL);

	/*
	 * Half of this mumbojumbo is to support site's that
	 * RW/RO/BACK in the same entry. It seems like its not unsupported.
	 */

	do {
	    volid = 0;

	    /* 
	     * check if we have done this volume type, if not check it
	     * now and remove it from the set we are going to check
	     */

	    if (serverflags & VLSF_RWVOL) {
		volid = vldb.volumeId[RWVOL];
		serverflags &= ~VLSF_RWVOL;
		if (serverflags & VLSF_RWVOL)
		    done = 0;
	    } else if (vldb.serverFlags[i] & VLSF_ROVOL) {
		volid = vldb.volumeId[ROVOL];
		serverflags &= ~VLSF_ROVOL;
		if (serverflags & VLSF_ROVOL)
		    done = 0;
	    } else if (vldb.serverFlags[i] & VLSF_BACKVOL) {
		volid = vldb.volumeId[BACKVOL];
		serverflags &= ~VLSF_BACKVOL;
		if (serverflags & VLSF_BACKVOL)
		    done = 0;
	    }

	    if (volid == 0)
		break;
	    
	    ret = VOLSER_AFSVolXListOneVolume(conn, vldb.serverPartition[i],
					      volid, &ie);
	    if (ret)
		ie.len = 1;
	    
	    for (j = 0; j < ie.len; j++) {
		
		info->info = erealloc(info->info, 
				      (info->num_info + 1)
				      * sizeof(info->info[0]));
		if (ret == 0) {
		    memcpy(&info->info[info->num_info].info, 
			   &ie.val[j], sizeof(ie.val[0]));
		    info->info[info->num_info].status =
			AAFS_VOLUME_INFO_ENTRY_OK;
		} else {
		    memset(&info->info[info->num_info].info, 0, 
			   sizeof(info->info[info->num_info].info));
		    info->info[info->num_info].status =
			AAFS_VOLUME_INFO_ENTRY_DEAD;
		}
		
		aafs_site_create(volume->cell, server, vldb.serverPartition[i],
				 &info->info[info->num_info].site);
		info->num_info++;
	    }

	    if (ie.val)
		free(ie.val);
	} while (!done);
    }

    aafs_object_unref(server, "server");

    *ret_info = info;
    
    return 0;
}


static void
info_ctx_destruct(void *ptr, const char *name)
{
    struct aafs_volume_info_ctx *c = ptr;
    aafs_object_unref(c->info, "volume-info");
}


struct aafs_volume_info_entry *
aafs_volume_info_first(struct aafs_volume_info *list,
		       struct aafs_volume_info_ctx **ret_ctx)
{
    struct aafs_volume_info_ctx *c;

    *ret_ctx = NULL;

    c = aafs_object_create(sizeof(*c), "volume-info-ctx", info_ctx_destruct);
    c->current = -1;
    c->info = aafs_object_ref(list, "volume-info");

    *ret_ctx = c;
    return aafs_volume_info_next(c);
}

struct aafs_volume_info_entry *
aafs_volume_info_next(struct aafs_volume_info_ctx *c)
{
    c->current++;

    if (c->info->num_info <= c->current)
	return NULL;

    return &c->info->info[c->current];
}

int
aafs_volume_info_destroy_ctx(struct aafs_volume_info_ctx *ctx)
{
    aafs_object_unref(ctx, "volume-info-ctx");
    return 0;
}

struct aafs_site *
aafs_volume_info_get_site(struct aafs_volume_info_entry *e)
{
    return aafs_object_ref(e->site, "site");
}

int
aafs_volume_info_get_volinfo(struct aafs_volume_info_entry *e,
			     xvolintInfo *info)
{
    if (e->status == AAFS_VOLUME_INFO_ENTRY_OK)
	memcpy(info, &e->info, sizeof(*info));
    return e->status;
}


int
aafs_volume_status_print(FILE *f, int flags,
			 struct aafs_volume_info_entry *status)
{
    xvolintInfo *v = &status->info;
    char timestr[128];
    struct tm tm;

    printf("\n");
    printf("    Status      %s\t\tVolume type: %s\n", 
	   v->status == VOK ? "On-line" : "Busy",
	   volumetype_from_volsertype(v->type));
    printf("    Usage       %10d K\n", v->size);
    
    if (flags & AAFS_VOLUME_STATUS_PRINT_SUMMERY)
	return 0;

    if (v->status != VOK)
	return 0;
	
    printf("    MaxQuota    %10d K\n", v->maxquota);
    
    memset (&tm, 0, sizeof(tm));
    strftime (timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S %Z",
	      localtime_r((time_t*) &v->creationDate, &tm));
    printf("    Creation    %s\n", timestr);
    
    memset (&tm, 0, sizeof(tm));
    strftime (timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S %Z",
	      localtime_r((time_t*) &v->updateDate, &tm));
    printf("    Last Update %s\n", timestr);
    
    printf("    %d accesses in the past day (i.e., vnode references)\n",
	   v->dayUse);

    if (flags & AAFS_VOLUME_STATUS_PRINT_EXTENDED) {
	printf("    File count:\t%d\n\n", v->filecount);
	
	printf("                      Raw Read/Write Stats\n"
	       "          |-------------------------------------------|\n"
	       "          |    Same Network     |    Diff Network     |\n"
	       "          |----------|----------|----------|----------|\n"
	       "          |  Total   |   Auth   |   Total  |   Auth   |\n"
	       "          |----------|----------|----------|----------|\n");
	printf("Reads     |%9d |%9d |%9d |%9d |\n",
	       v->stat_reads[0],
	       v->stat_reads[1],
	       v->stat_reads[2],
	       v->stat_reads[3]);
	printf("Writes    |%9d |%9d |%9d |%9d |\n",
	       v->stat_writes[0],
	       v->stat_writes[1],
	       v->stat_writes[2],
	       v->stat_writes[3]);
	printf("          |-------------------------------------------|\n");
	printf("\n");
	printf("                   Writes Affecting Authorship\n"
	       "          |-------------------------------------------|\n"
	       "          |   File Authorship   | Directory Authorship|\n"
	       "          |----------|----------|----------|----------|\n"
	       "          |   Same   |   Diff   |    Same  |   Diff   |\n"
	       "          |----------|----------|----------|----------|\n");
	printf("0-60 sec  |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[0], v->stat_fileDiffAuthor[0],
	       v->stat_dirSameAuthor[0],  v->stat_dirDiffAuthor[0]);
	printf("1-10 min  |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[1], v->stat_fileDiffAuthor[1],
	       v->stat_dirSameAuthor[1],  v->stat_dirDiffAuthor[1]);
	printf("10min-1hr |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[2], v->stat_fileDiffAuthor[2],
	       v->stat_dirSameAuthor[2],  v->stat_dirDiffAuthor[2]);
	printf("1hr-1day  |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[3], v->stat_fileDiffAuthor[3],
	       v->stat_dirSameAuthor[3],  v->stat_dirDiffAuthor[3]);
	printf("1day-1wk  |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[4], v->stat_fileDiffAuthor[4],
	       v->stat_dirSameAuthor[4],  v->stat_dirDiffAuthor[4]);
	printf("> 1wk     |%9d |%9d |%9d |%9d |\n",
	       v->stat_fileSameAuthor[5], v->stat_fileDiffAuthor[5],
	       v->stat_dirSameAuthor[5],  v->stat_dirDiffAuthor[5]);
	printf("          |-------------------------------------------|\n");
    }
    return 0;
}

int
aafs_volume_status_have_type(struct aafs_volume *volume, 
			     int volume_type)
{
    return 0;
}

int
aafs_volume_sitelist(struct aafs_volume *volume,
		     struct aafs_site_list **sitelist)
{
    *sitelist = NULL;
    return 0;
}

/*
 * modification ops
 */

int
aafs_volume_add_readonly(struct aafs_volume *volume,
			 struct aafs_site *site)
{
    return 0;
}

int
aafs_volume_move_readwrite(struct aafs_volume *volume,
			   struct aafs_site *site,
			   unsigned long flags)
{
    return 0;
}


int
aafs_volume_release(struct aafs_volume *volume,
		    unsigned long flags)
{
    return 0;
}
