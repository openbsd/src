/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

RCSID("$KTH: vldbserver.c,v 1.33 2001/01/01 20:42:58 lha Exp $");

static void make_vldb_from_vl(struct vldbentry *vldb_entry,
			      struct disk_vlentry *vl_entry);

static void
make_vldb_from_vl(struct vldbentry *vldb_entry, struct disk_vlentry *vl_entry)
{
    int i;

    strlcpy(vldb_entry->name, vl_entry->name, VLDB_MAXNAMELEN);
    vldb_entry->volumeType = vl_entry->volumeType;
    vldb_entry->nServers = 1; /* XXX What is this supposed to be? */

    for (i = 0; i < MAXNSERVERS; i++) {
	vldb_entry->serverNumber[i] = vl_entry->serverNumber[i];
	vldb_entry->serverPartition[i] = vl_entry->serverPartition[i];
	vldb_entry->serverFlags[i] = vl_entry->serverFlags[i];
    }

    for (i = 0; i < MAXTYPES; i++)
	vldb_entry->volumeId[i] = vl_entry->volumeId[i];

    vldb_entry->cloneId = vl_entry->cloneId;
    vldb_entry->flags = vl_entry->flags;
}

/*
 * The rpc - calls
 */

int
VL_CreateEntry(struct rx_call *call, 
	       const vldbentry *newentry) 
{
    struct disk_vlentry vl_entry;
    int32_t nServers;
    int i;

    if (!sec_is_superuser(call))
	return VL_PERM;

    memset (&vl_entry, 0, sizeof(vl_entry));

    vldb_debug ("VL_CreateEntry (name=%s, type=%d, ids=%d,%d,%d flags=%d)\n",
		newentry->name, newentry->volumeType,
		newentry->volumeId[RWVOL],
		newentry->volumeId[ROVOL],
		newentry->volumeId[BACKVOL],
		newentry->flags);

    strlcpy(vl_entry.name, newentry->name, VLDB_MAXNAMELEN);
    vl_entry.volumeType = newentry->volumeType;

    /* XXX All fields mustn't be set */
    nServers = newentry->nServers;
    if (nServers > MAXNSERVERS)
	nServers = MAXNSERVERS;
    for (i = nServers - 1 ; i >= 0 ; i--) {
	vl_entry.serverNumber[i] = newentry->serverNumber[i];
	vl_entry.serverPartition[i] = newentry->serverPartition[i];
	vl_entry.serverFlags[i] = newentry->serverFlags[i];
    }    

    for (i = 0; i < MAXTYPES; i++)
	vl_entry.volumeId[i] = newentry->volumeId[i];

    vl_entry.cloneId = newentry->cloneId;
    vl_entry.flags = newentry->flags;

    vldb_insert_entry(&vl_entry);
    return 0;
}

int 
VL_DeleteEntry(struct rx_call *call, 
	       const int32_t Volid, 
	       const int32_t voltype)
{
    vldb_debug ("VL_DeleteEntry\n") ;

    if (!sec_is_superuser(call))
	return VL_PERM;

    return VL_PERM ;
}

/*
 *
 */

int
VL_GetEntryByID(struct rx_call *call,
		const int32_t Volid, 
		const int32_t voltype, 
		vldbentry *entry) 
{
    struct disk_vlentry vl_entry;

    vldb_debug ("VL_GetEntryByID (Volid=%d,Voltype=%d)\n", 
		  Volid, voltype);
    
    if (vldb_get_first_id_entry(vldb_get_id_hash(Volid),
				voltype, &vl_entry) != 0)
	return VL_NOENT;

    while (1) {
	/* Return entry if match found */
	if (vl_entry.volumeId[voltype] == Volid) {
	    make_vldb_from_vl(entry, &vl_entry);
	    return 0;
	}
	
	if (vl_entry.nextIdHash[voltype] == 0)
	    break;
	
	vldb_read_entry(vl_entry.nextIdHash[voltype], &vl_entry);
    }
    return VL_NOENT;
}

/*
 *
 */

int
VL_GetEntryByName(struct rx_call *call, 
		  const char *volumename, 
		  vldbentry *entry) 
{
    struct disk_vlentry vl_entry;

    vldb_debug ("VL_GetEntryByName %s\n", volumename) ;

    if (isdigit(volumename[0])) {
	return VL_GetEntryByID(call, atol(volumename), 0 /* XXX */, entry);
    }

    if (vldb_get_first_name_entry(vldb_get_name_hash(volumename),
				  &vl_entry) == 0) {
	while (1) {
	    /* Return entry if match found */
	    if (strcmp(vl_entry.name, volumename) == 0) {
		make_vldb_from_vl(entry, &vl_entry);
		return 0;
	    }
	    
	    if (vl_entry.nextNameHash == 0)
		break;
	    
	    vldb_read_entry(vl_entry.nextNameHash, &vl_entry);
	}
    } 

    return VL_NOENT;
}

/*
 *
 */

int 
VL_GetNewVolumeId (struct rx_call *call, 
		   const int32_t bumpcount,
		   int32_t *newvolumid)
{
    vldb_debug ("VL_GetNewVolumeId(bumpcount=%d)\n", bumpcount) ;
    
    if (!sec_is_superuser(call))
	return VL_PERM;

    *newvolumid = vl_header.vital_header.MaxVolumeId;
    vldb_debug ("   returning low volume id = %d\n", *newvolumid);
    vl_header.vital_header.MaxVolumeId += bumpcount;
    vldb_write_header();

    return 0;
}

/*
 *
 */

int
VL_ReplaceEntry (struct rx_call *call, 
		 const int32_t Volid, 
		 const int32_t voltype,
		 const vldbentry *newentry,
		 const int32_t ReleaseType) 
{
    vldb_debug ("VL_ReplaceEntry\n") ;

    if (!sec_is_superuser(call))
	return VL_PERM;

    return VL_PERM ;
}

/*
 *
 */

int
VL_UpdateEntry (struct rx_call *call, 
		const int32_t Volid, 
		const int32_t voltype, 
		const VldbUpdateEntry *UpdateEntry,
		const int32_t ReleaseType)
{
    vldb_debug ("VL_UpdateEntry\n") ;

    if (!sec_is_superuser(call))
	return VL_PERM;

    return VL_PERM ;
}

/*
 *
 */

int 
VL_SetLock (struct rx_call *call, 
	    const int32_t Volid, 
	    const int32_t voltype,
	    const int32_t voloper) 
{
    vldb_debug ("VL_SetLock\n") ;

    if (!sec_is_superuser(call))
	return VL_PERM;

    return 0;
}

/*
 *
 */

int
VL_ReleaseLock (struct rx_call *call, 
		const int32_t volid,
		const int32_t voltype, 
		const int32_t ReleaseType) 
{
    vldb_debug ("VL_ReleaseLock\n") ;

    if (!sec_is_superuser(call))
	return VL_PERM;


    return 0;
}

/*
 *
 */

int
VL_ListEntry (struct rx_call *call, 
	      const int32_t previous_index, 
	      int32_t *count,
	      int32_t *next_index, 
	      vldbentry *entry) 
{
    vldb_debug ("VL_ListEntry\n") ;
    return VL_PERM ;
}

/*
 *
 */

int
VL_ListAttributes (struct rx_call *call, 
		   const VldbListByAttributes *attributes,
		   int32_t *nentries,
		   bulkentries *blkentries) 
{
    vldb_debug ("VL_ListAttributes\n") ;
    return VL_PERM ;
}

/*
 *
 */

int
VL_GetStats (struct rx_call *call, 
	     vldstats *stats,
	     vital_vlheader *vital_header) 
{
    vldb_debug ("VL_GetStats") ;
    return VL_PERM ;
}

/*
 *
 */

int 
VL_Probe(struct rx_call *call)
{
    vldb_debug ("VL_Probe\n") ;
    return 0;
}

/*
 *
 */

int
VL_CreateEntryN(struct rx_call *call,
		const nvldbentry *entry)
{
    int i;
    struct vldbentry vldb_entry;

    vldb_debug ("VL_CreateEntryN\n") ;

    if (!sec_is_superuser(call))
	return VL_PERM;

    memset (&vldb_entry, 0, sizeof (vldb_entry));

    strncpy(vldb_entry.name, entry->name, VLDB_MAXNAMELEN);
    vldb_entry.volumeType = RWVOL;
    vldb_entry.nServers = entry->nServers;

    for (i = 0; i < MAXNSERVERS; i++) {
	vldb_entry.serverNumber[i] = entry->serverNumber[i];
	vldb_entry.serverPartition[i] = entry->serverPartition[i];
	vldb_entry.serverFlags[i] = entry->serverFlags[i];
    }

    for (i = 0; i < MAXTYPES; i++)
	vldb_entry.volumeId[i] = entry->volumeId[i];

    vldb_entry.cloneId = entry->cloneId;
    vldb_entry.flags = entry->flags;

    return VL_CreateEntry(call, &vldb_entry);
}

/*
 *
 */

int
VL_GetEntryByIDN(struct rx_call *call,
		 const int32_t Volid,
		 const int32_t voltype,
		 nvldbentry *entry)
{
    struct vldbentry vldb_entry;
    int status, i;
    int32_t type = voltype;

    vldb_debug ("VL_GetEntryByIDN (Volid=%d,Voltype=%d)\n", Volid, type);

    memset (&vldb_entry, 0, sizeof(vldb_entry));

    if (type == -1)
	type = RWVOL;

    status = VL_GetEntryByID(call, Volid, type, &vldb_entry);

    if (status)
	return status;

    strlcpy(entry->name, vldb_entry.name, VLDB_MAXNAMELEN);
    entry->nServers = vldb_entry.nServers;
    for (i = 0; i < MAXNSERVERS; i++) {
	entry->serverNumber[i] = vldb_entry.serverNumber[i];
	entry->serverPartition[i] = vldb_entry.serverPartition[i];
	entry->serverFlags[i] = vldb_entry.serverFlags[i];
    }

    for (i = 0; i < MAXTYPES; i++)
	entry->volumeId[i] = vldb_entry.volumeId[i];

    entry->cloneId = vldb_entry.cloneId;
    entry->flags = vldb_entry.flags;

    return 0;
}

/*
 *
 */

int
VL_GetEntryByNameN(struct rx_call *call, 
		   const char *volumename, 
		   nvldbentry *entry) 
{
    struct vldbentry vldb_entry;
    int status, i;

    memset (&vldb_entry, 0, sizeof(vldb_entry));

    vldb_debug ("VL_GetEntryByNameN(volumename=%s)\n", volumename) ;
    status = VL_GetEntryByName(call, volumename, &vldb_entry);

    if (status)
	return status;

    memset (entry, 0, sizeof(*entry));
    strlcpy(entry->name, vldb_entry.name, VLDB_MAXNAMELEN);
    entry->nServers = vldb_entry.nServers;
    for (i = 0; i < MAXNSERVERS; i++) {
	entry->serverNumber[i] = vldb_entry.serverNumber[i];
	entry->serverPartition[i] = vldb_entry.serverPartition[i];
	entry->serverFlags[i] = vldb_entry.serverFlags[i];
    }

    for (i = 0; i < MAXTYPES; i++)
	entry->volumeId[i] = vldb_entry.volumeId[i];

    entry->cloneId = vldb_entry.cloneId;
    entry->flags = vldb_entry.flags;

    return 0;
}

/*
 *
 */

int
VL_GetEntryByNameU(struct rx_call *call, 
		   const char *volumename, 
		   uvldbentry *entry) 
{
    vldb_debug ("VL_GetEntryByNameU %s\n", volumename);
    memset(entry, 0, sizeof(*entry));
    
    return RXGEN_OPCODE;
}

/*
 *
 */

int
VL_ListAttributesN (struct rx_call *call, 
		   const VldbListByAttributes *attributes,
		   int32_t *nentries,
		   nbulkentries *blkentries) 
{
    vldb_debug ("VL_ListAttributesN\n");
    vldb_debug ("  attributes: Mask=(%d=", attributes->Mask);

    if (attributes->Mask & VLLIST_SERVER)
	vldb_debug  ("SERVER ");
    if (attributes->Mask & VLLIST_PARTITION)
	vldb_debug  ("PARTITION ");
    if (attributes->Mask & VLLIST_VOLUMETYPE)
	vldb_debug  ("VOLUMETYPE ");
    if (attributes->Mask & VLLIST_VOLUMEID)
	vldb_debug  ("VOLUMEID ");
    if (attributes->Mask & VLLIST_FLAG)
	vldb_debug  ("FLAG");

    vldb_debug (") server=%d partition=%d volumetype=%d volumeid=%d flag=%d\n",
	   attributes->server,
	   attributes->partition,
	   attributes->volumetype,
	   attributes->volumeid,
	   attributes->flag);

    *nentries = 1;

    blkentries->len = 0;
    blkentries->val = NULL;

    return VL_PERM;
}

/*
 *
 */

int
VL_ListAttributesU(struct rx_call *call, 
		   const VldbListByAttributes *attributes,
		   int32_t *nentries,
		   ubulkentries *blkentries) 
{
    vldb_debug ("VL_ListAttributesU\n") ;
    *nentries = 0;
    blkentries->len = 0;
    blkentries->val = NULL;
    return 0;
}

/*
 *
 */

int
VL_UpdateEntryByName(struct rx_call *call,
		     const char volname[65],
		     const struct VldbUpdateEntry *UpdateEntry,
		     const int32_t ReleaseType)
{
    vldb_debug ("VL_UpdateEntryByName (not implemented)\n");

    if (!sec_is_superuser(call))
	return VL_PERM;

    return VL_PERM;
}

/*
 *
 */

int
VL_GetAddrsU(struct rx_call *call,
	     const struct ListAddrByAttributes *inaddr,
	     struct afsUUID *uuid,
	     int32_t *uniq,
	     int32_t *nentries,
	     bulkaddrs *addrs)
{
    vldb_debug ("VL_GetAddrsU (not implemented)\n");
    return VL_PERM;
}

/*
 *
 */

int
VL_RegisterAddrs(struct rx_call *call,
		 const struct afsUUID *uid,
		 const int32_t spare,
		 const bulkaddrs *addrs)
{
    vldb_debug ("VL_RegistersAddrs (not implemented)\n");

    if (!sec_is_superuser(call))
	return VL_PERM;

    return 0;
}

/*
 *
 */

int
VL_CreateEntryU(struct rx_call *call,
		const struct uvldbentry *newentry)
{
    vldb_debug ("VL_CreateEntryU (not implemented)\n");

    if (!sec_is_superuser(call))
	return VL_PERM;

    return VL_PERM;
}

/*
 *
 */

int
VL_ReplaceEntryU(struct rx_call *call)
{
    vldb_debug ("VL_ReplaceEntryU (not implemented)\n");

    if (!sec_is_superuser(call))
	return VL_PERM;

    return VL_PERM;
}

/*
 *
 */

int
VL_ReplaceEntryN(struct rx_call *call,
		 const int32_t Volid,
		 const int32_t voltype,
		 const struct vldbentry *newentry,
		 const int32_t ReleaseType)
{
    vldb_debug ("VL_ReplaceEntryN (not implemented)\n");

    if (!sec_is_superuser(call))
	return VL_PERM;

    return VL_PERM;
}

/*
 *
 */

int
VL_ChangeAddrs(struct rx_call *call,
	       const int32_t old_ip,
	       const int32_t new_ip)
{
    vldb_debug ("VL_ChangeAddrs (not implemented)\n");

    if (!sec_is_superuser(call))
	return VL_PERM;

    return VL_PERM;
}

/*
 *
 */

int
VL_GetEntryByIDU(struct rx_call *call)
{
    vldb_debug ("VL_GetEntryByIDU (not implemented)\n");
    return VL_PERM;
}

/*
 *
 */

int
VL_ListEntryN(struct rx_call *call)
{
    vldb_debug ("VL_ListEntryN (not implemented)\n");
    return VL_PERM;
}

/*
 *
 */

int
VL_ListEntryU(struct rx_call *call)
{
    vldb_debug ("VL_ListEntryU (not implemented)\n");
    return VL_PERM;
}

/*
 *
 */

int
VL_GetAddrs(struct rx_call *call,
	    const int32_t handle,
	    const int32_t spare,
	    struct VL_Callback *spare3,
	    const int32_t *nentries,
	    bulkaddrs *blkaddr)
{
    vldb_debug ("VL_GetAddrs (not implemented)\n");
    return VL_PERM;
}

/*
 *
 */

int
VL_LinkedListN(struct rx_call *call)
{
    vldb_debug ("VL_LinkedListN (not implemented)\n");
    return VL_PERM;
}

/*
 *
 */

int
VL_LinkedListU(struct rx_call *call)
{
    vldb_debug ("VL_LinkedListU (not implemented)\n");
    return VL_PERM;
}


/*
 *
 */

static struct rx_service *vldbservice = NULL;
static struct rx_service *ubikservice = NULL;

static char *cell = NULL;
static char *realm = NULL;
static char *databasedir = NULL;
static char *srvtab_file = NULL;
static char *log_file = "syslog";
static int no_auth = 0;
static int do_create = 0;
static int vlsrv_debug = 0;

static struct agetargs args[] = {
    {"create",  0, aarg_flag,      &do_create, "create new database"},
    {"cell",	0, aarg_string,    &cell, "what cell to use"},
    {"realm",	0, aarg_string,	  &realm, "what realm to use"},
    {"prefix",'p', aarg_string,    &databasedir, "what dir to store the db"},
    {"noauth", 0,  aarg_flag,	  &no_auth, "disable authentication checks"},
    {"debug", 'd', aarg_flag,      &vlsrv_debug, "output debugging"},
    {"log",   'd', aarg_string,    &log_file, "log file"},
    {"srvtab",'s', aarg_string,    &srvtab_file, "what srvtab to use"},
    { NULL, 0, aarg_end, NULL }
};

static void
usage(void)
{
    aarg_printusage(args, NULL, "", AARG_AFSSTYLE);
}

int
main(int argc, char **argv) 
{
    Log_method *method;
    int optind = 0;
    int ret;
    
    set_progname (argv[0]);

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	usage ();
	return 1;
    }

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 1;
    }

    if (vlsrv_debug)
	vldb_setdebug (vlsrv_debug);

    if (no_auth)
	sec_disable_superuser_check ();

    if (do_create) {
	vldb_create (databasedir);
	return 0;
    }
	
    method = log_open (get_progname(), log_file);
    if (method == NULL)
	errx (1, "log_open failed");
    cell_init(0, method);
    ports_init();


    if (cell)
	cell_setthiscell (cell);

    network_kerberos_init (srvtab_file);
    
    vldb_init(databasedir);

    ret = network_init(htons(afsvldbport), "vl", VLDB_SERVICE_ID, 
		       VL_ExecuteRequest, &vldbservice, realm);
    if (ret)
	errx (1, "network_init failed with %d", ret);
    
    ret = network_init(htons(afsvldbport), "ubik", VOTE_SERVICE_ID, 
		       Ubik_ExecuteRequest, &ubikservice, realm);
    if (ret)
	errx (1, "network_init failed with %d", ret);

    printf("Milko vldbserver %s-%s started\n", PACKAGE, VERSION);

    rx_SetMaxProcs(vldbservice,5) ;
    rx_SetMaxProcs(ubikservice,5) ;
    rx_StartServer(1) ;

    abort() ; /* should not get here */
    return 0;
}
