/*
 * Copyright (c) 1999, 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$KTH: perf.c,v 1.12 2000/12/10 22:24:37 lha Exp $");

#include <sys/types.h>
#include <stdio.h>
#include <assert.h>
#include <err.h>

#include <service.h>

#include <cb.ss.h>
#include <fs.cs.h>

#include <des.h>
#include <krb.h>

#include <rx/rx.h>
#include <rx/rx_null.h>
#include <rx/rxgencon.h>
#include <rxkad/rxkad.h>

#include <atypes.h>

#include <arlalib.h>
#include <ports.h>
#include <service.h>

#include <agetarg.h>

static char *arg_cell = NULL;

/*
 * Each client need a callbackserver, here we go...
 */

int
RXAFSCB_Probe (struct rx_call *a_rxCallP)
{
    return 0;
}

int
RXAFSCB_InitCallBackState (struct rx_call *a_rxCallP)
{
    return 0;
}

int
RXAFSCB_CallBack (struct rx_call *a_rxCallP,
		  const AFSCBFids *a_fidArrayP,
		  const AFSCBs *a_callBackArrayP)
{
    return 0;
}


int
RXAFSCB_GetLock(struct rx_call *a_rxCallP,
		int32_t index,
		AFSDBLock *lock)
{
    return 1;
}

int
RXAFSCB_GetCE(struct rx_call *a_rxCallP,
	      int32_t index,
	      AFSDBCacheEntry *dbentry)
{
    return 1;
}

int
RXAFSCB_XStatsVersion(struct rx_call *a_rxCallP,
		      int32_t *version)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_GetXStats(struct rx_call *a_rxCallP,
		  int32_t client_version_num,
		  int32_t collection_number,
		  int32_t *server_version_number,
		  int32_t *time,
		  AFSCB_CollData *stats)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_InitCallBackState2(struct rx_call *a_rxCallP,
			   interfaceAddr *addr)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_WhoAreYou(struct rx_call *a_rxCallP,
		  interfaceAddr *addr)
{
    return RXGEN_OPCODE;
}

int
RXAFSCB_InitCallBackState3(struct rx_call *a_rxCallP,
			   const afsUUID *server_uuid)
{
    return 0;
}

int
RXAFSCB_ProbeUUID(struct rx_call *a_rxCallP,
		  const afsUUID *uuid)
{
    return RXGEN_OPCODE;
}

static void
cmcb_init (void)
{
    static struct rx_securityClass *nullSecObjP;
    static struct rx_securityClass *(securityObjects[1]);
    
    nullSecObjP = rxnull_NewClientSecurityObject ();
    if (nullSecObjP == NULL) {
	printf("Cannot create null security object.\n");
	return;
    }
    
    securityObjects[0] = nullSecObjP;
    
    if (rx_NewService (0, CM_SERVICE_ID, "cm", securityObjects,
		       sizeof(securityObjects) / sizeof(*securityObjects),
		       RXAFSCB_ExecuteRequest) == NULL ) {
	printf("Cannot install service.\n");
	return;
    }
    rx_StartServer (0);
}

/*
 *
 */

static struct timeval timer_start;
static struct timeval timer_stop;
static int timer_check = 0;

static void
start_timer (void)
{
    timer_check++;
    gettimeofday (&timer_start, NULL);
}

/*
 *
 */

static void
end_and_print_timer (char *str)
{
    long long start_l, stop_l;

    timer_check--; 
    assert (timer_check == 0);
    gettimeofday(&timer_stop, NULL);
    start_l = timer_start.tv_sec * 1000000 + timer_start.tv_usec;
    stop_l = timer_stop.tv_sec * 1000000 + timer_stop.tv_usec;
    printf("%s:\t%8llu msec\n", str, (stop_l-start_l)/1000);
}


static void
do_createfile (struct rx_connection *conn, const AFSFid *parent,
	       const char *name, AFSFid *child)
{
    static AFSStoreStatus InStatus = { 0,0,0,0,0 };
    AFSFetchStatus OutFidStatus, OutDirStatus;
    AFSCallBack CallBack;
    AFSVolSync a_volSyncP;
    int ret;
    
    ret = RXAFS_CreateFile (conn, parent, name, &InStatus,
			    child, &OutFidStatus, &OutDirStatus,
			    &CallBack, &a_volSyncP);
    if (ret)
	errx (1, "RXAFS_CreateFile returned %d", ret);
}

static void
do_createfilecontent (struct rx_connection *conn, const AFSFid *parent,
		      const char *name, AFSFid *child, int size,
		      int times, char *buffer)
{
    static AFSStoreStatus InStatus = { 0,0,0,0,0 };
    AFSFetchStatus OutFidStatus, OutDirStatus;
    AFSCallBack CallBack;
    AFSVolSync a_volSyncP;
    int ret;
    struct rx_call *call;
    int i;
    
    ret = RXAFS_CreateFile (conn, parent, name, &InStatus,
			    child, &OutFidStatus, &OutDirStatus,
			    &CallBack, &a_volSyncP);
    if (ret)
	errx (1, "RXAFS_CreateFile returned %d", ret);

    call = rx_NewCall (conn);

    if (call == NULL)
	errx (1, "rx_NewCall returned NULL");

    ret = StartRXAFS_StoreData (call, child, &InStatus,
				0, size*times, size*times);

    if (ret)
	errx (1, "StartRXAFS_StoreData returned %d", ret);

    for (i = 0; i < times; i++) {
	ret = rx_Write(call, buffer, size);
	if (ret != size)
	    errx (1, "rx_Write returned %d", ret);
    }

    ret = EndRXAFS_StoreData (call, &OutFidStatus, &a_volSyncP);

    if (ret)
	errx (1, "StartRXAFS_StoreData returned %d", ret);

    ret = rx_EndCall(call, ret);
    if (ret)
	errx (1, "rx_EndCall returned %d", ret);
}

static void
do_readfilecontent (struct rx_connection *conn, const AFSFid *fid,
		    int size,
		    int times, char *buffer)
{
    AFSFetchStatus OutFidStatus;
    AFSCallBack CallBack;
    AFSVolSync a_volSyncP;
    int ret;
    struct rx_call *call;
    int i;
    
    call = rx_NewCall (conn);

    if (call == NULL)
	errx (1, "rx_NewCall returned NULL");

    ret = StartRXAFS_FetchData (call, fid, 0, size*times);

    if (ret)
	errx (1, "StartRXAFS_FetchData returned %d", ret);

    for (i = 0; i < times; i++) {
	ret = rx_Read(call, buffer, size);
	if (ret != size)
	    errx (1, "rx_Read returned %d", ret);
    }

    ret = EndRXAFS_FetchData (call, &OutFidStatus, &CallBack,
			      &a_volSyncP);
    if (ret)
	errx (1, "StartRXAFS_FetchData returned %d", ret);

    ret = rx_EndCall(call, ret);
    if (ret)
	errx (1, "rx_EndCall returned %d", ret);
}

static void
do_gtod (struct rx_connection *conn)
{
    int ret;
    int32_t sec, usec;
    
    ret = RXAFS_GetTime (conn, &sec, &usec);
    if (ret)
	errx (1, "RXAFS_GetTime returned %d", ret);
}

static void
do_mkdir (struct rx_connection *conn, const AFSFid *parent,
	  const char *name, AFSFid *child)
{
    static AFSStoreStatus InStatus = { 0,0,0,0,0 };
    AFSFetchStatus OutFidStatus, OutDirStatus;
    AFSCallBack CallBack;
    AFSVolSync a_volSyncP;
    int ret;
    
    ret = RXAFS_MakeDir (conn, parent, name, &InStatus,
			 child, &OutFidStatus, &OutDirStatus,
			 &CallBack, &a_volSyncP);
    if (ret)
	errx (1, "RXAFS_MakeDir returned %d", ret);
}


static void
do_rmdir (struct rx_connection *conn, const AFSFid *parent,
	  const char *name)
{
    AFSFetchStatus a_newParentDirStatP;
    AFSVolSync a_volSyncP;
    int ret;

    ret = RXAFS_RemoveDir (conn, parent, name, &a_newParentDirStatP,
			   &a_volSyncP);
    if (ret)
	errx (1, "RXAFS_RemoveDir returned %d", ret);
}

static void
do_removefile (struct rx_connection *conn, const AFSFid *parent,
	       const char *name)
{
    AFSFetchStatus a_newParentDirStatP;
    AFSVolSync a_volSyncP;
    int ret;

    ret = RXAFS_RemoveFile (conn, parent, name, &a_newParentDirStatP,
			    &a_volSyncP);
    if (ret)
	errx (1, "RXAFS_RemoveDir returned %d", ret);
}

static void __attribute__((__unused__))
do_bulkstat (struct rx_connection *conn, AFSFid *fids,
	     int num)
{
    AFSCBFids FidsArray;
    AFSBulkStats StatArray;
    AFSCBs CBArray;
    AFSVolSync Sync;
    int ret;
    
    FidsArray.val = fids;
    FidsArray.len = num;

    ret = RXAFS_BulkStatus (conn, &FidsArray, &StatArray, &CBArray, &Sync);
    if (ret)
	errx (1, "RXAFS_BulkStatus faile with %d", ret);

    free (StatArray.val);
    free (CBArray.val);
}

/*
 *
 */

typedef void (*create_many_create)(struct rx_connection *conn,
				   const AFSFid *parent,
				   const char *name,
				   AFSFid *child);
typedef void (*create_many_remove)(struct rx_connection *conn,
				   const AFSFid *parent,
				   const char *name);

static void
create_many_entry (struct rx_connection *conn, AFSFid *parentfid, int num,
		   create_many_create create, 
		   create_many_remove remove,
		   const char *entry_name)
{
    char *dir, *stamp;
    AFSFid fid;
    AFSFid *files;
    int i;
    char filename[MAXPATHLEN];
    
    assert (num >= 0);
    
    files = emalloc (num * sizeof (fid));

    asprintf (&stamp, "creating  %d %ss", num, entry_name);

    asprintf (&dir, "create_many_files-%d-time-%d", num, (int)time(NULL));
    do_mkdir (conn, parentfid, dir, &fid);
    
    start_timer();
    for (i = 0; i < num; i++) {
	snprintf (filename, sizeof(filename), "%d", i);
	(create) (conn, &fid, filename, &files[i]);
    }
    end_and_print_timer (stamp);

    free (stamp);
    asprintf (&stamp, "removing  %d %ss", num, entry_name);

    start_timer();
    for (i = 0; i < num; i++) {
	snprintf (filename, sizeof(filename), "%d", i);
	(remove) (conn, &fid, filename);
    }
    end_and_print_timer (stamp);
    
    do_rmdir (conn, parentfid, dir);

    free (stamp);
    free (dir);
}

static void
create_and_read_content (struct rx_connection *conn, AFSFid *parentfid, int size)
{
    char *dir, *stamp;
    AFSFid fid;
    AFSFid file;
    char *buffer;
    
    asprintf (&stamp, "writing   %dM data", size);

    asprintf (&dir, "write_big_files-%d-time-%d", size, (int)time(NULL));
    do_mkdir (conn, parentfid, dir, &fid);

    buffer = malloc(1024*1024);
    memset(buffer, 'X', 1024*1024);
    
    start_timer();
    do_createfilecontent (conn, &fid, "foo", &file, 1024*1024,
			  size, buffer);
    end_and_print_timer (stamp);

    free (stamp);

    asprintf (&stamp, "reading   %dM data", size);
    start_timer();
    do_readfilecontent (conn, &file, 1024*1024, size, buffer);
    end_and_print_timer (stamp);

    free(stamp);

    free(buffer);

    asprintf (&stamp, "removing  %dM data", size);

    start_timer();
    do_removefile (conn, &fid, "foo");
    end_and_print_timer (stamp);
    
    do_rmdir (conn, parentfid, dir);

    free (stamp);
    free (dir);
}

static void
bench_gettime(struct rx_connection *conn, int times)
{
    char *stamp;
    int i;

    asprintf (&stamp, "gettime   %d times", times);
    
    start_timer();
    for (i = 0; i < times; i++)
	do_gtod (conn);
    end_and_print_timer (stamp);

    free (stamp);
}

/*
 *
 */

static int
do_bench (char *host, int volume, arlalib_authflags_t auth)
{
    struct rx_connection *conn;
    AFSFid root;

    conn = arlalib_getconnbyname (arg_cell, host, afsport,
				  FS_SERVICE_ID, auth);
    if (conn == NULL)
	errx (1, "arlalib_getconnbyname failed");


    root.Volume = volume;
    root.Vnode = 1;
    root.Unique = 1;

    bench_gettime(conn, 1);
    bench_gettime(conn, 100);
    bench_gettime(conn, 1000);
    bench_gettime(conn, 2000);
    create_and_read_content (conn, &root, 1);
    create_and_read_content (conn, &root, 2);
    create_and_read_content (conn, &root, 10);
    create_and_read_content (conn, &root, 20);
    create_and_read_content (conn, &root, 30);
    create_and_read_content (conn, &root, 40);
    create_and_read_content (conn, &root, 50);

    create_many_entry (conn, &root, 1000,do_createfile,do_removefile, "file");
    create_many_entry (conn, &root, 2000,do_createfile,do_removefile, "file");
    create_many_entry (conn, &root, 4000,do_createfile,do_removefile, "file");
    create_many_entry (conn, &root, 8000,do_createfile,do_removefile, "file");

    create_many_entry (conn, &root, 1000, do_mkdir, do_rmdir, "dir");
    create_many_entry (conn, &root, 2000, do_mkdir, do_rmdir, "dir");
    create_many_entry (conn, &root, 4000, do_mkdir, do_rmdir, "dir");
    create_many_entry (conn, &root, 8000, do_mkdir, do_rmdir, "dir");

    rx_DestroyConnection(conn);

    return 0;
}

static int
do_readbench(char *host, int volume, int vnode, int uniq, arlalib_authflags_t auth)
{
    char *stamp;
    char *buffer;
    AFSFid file;
    struct rx_connection *conn;

    conn = arlalib_getconnbyname (arg_cell, host, afsport,
				  FS_SERVICE_ID, auth);

    buffer = malloc(1024*1024);
    asprintf (&stamp, "reading   %dM data", 10);
    file.Vnode = vnode;
    file.Volume = volume;
    file.Unique = uniq;
    start_timer();
    do_readfilecontent (conn, &file, 1024*1024, 10, buffer);
    end_and_print_timer (stamp);

    free(stamp);
    free(buffer);
    rx_DestroyConnection(conn);
    return 0;
}

/*
 *
 */

static char *arg_host = NULL;
static int   arg_volume  = 0;
static int   arg_authlevel  = 2;
static int   arg_help  = 0;
static int   arg_vnode = 0;
static int   arg_uniq = 0;

struct agetargs args[] = {
    {"host",	0, aarg_string, &arg_host,
     "what host to use", "host", aarg_mandatory},
    {"volume",	0, aarg_integer, &arg_volume,
     "what volume to use", NULL, aarg_mandatory}, 
    {"authlevel",0, aarg_integer, &arg_authlevel,
     "authlevel", NULL, aarg_mandatory},
    {"help",	0, aarg_flag, &arg_help,
     "authlevel", NULL, aarg_mandatory},
    {"vnode", 0, aarg_integer, &arg_vnode,
     "what vnode to read from", NULL, aarg_optional},
    {"uniq", 0, aarg_integer, &arg_uniq,
     "what uniq to read from", NULL, aarg_optional},
    {"cell", 0, aarg_string, &arg_cell,
     "cell", NULL, aarg_optional},
   { NULL, 0}
};

static void
usage (int eval)
{
    aarg_printusage(args, "perf", NULL, AARG_AFSSTYLE);

    printf ("\nTips:\n"
	    " on errors use `fs strerror <errno>' to figure"
	    " out what was wrong\n");

    exit (eval);
}

    
int
main (int argc, char **argv)
{
    int ret, optind = 0;
    arlalib_authflags_t auth;
    char *auth_string;
 
    printf ("afs low-level performance tester\n");

    ret = rx_Init (htons(4712));
    if (ret)
	errx (1, "rx_Init returnd %d", ret);

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage (args, "", NULL, AARG_AFSSTYLE);
	return 0;
    }

    if (arg_help)
	usage (0);

    ports_init ();
    cmcb_init();

    switch (arg_authlevel) {
    case 1:
	auth = AUTHFLAGS_NOAUTH;
	auth_string = "clear";
	break;
    case 2:
	auth = AUTHFLAGS_TICKET;
	auth_string = "auth";
	rxkad_min_level = rxkad_auth;
	break;
    case 3:
	auth = AUTHFLAGS_TICKET;
	auth_string = "crypt";
	rxkad_min_level = rxkad_crypt;
	break;
    default:
	errx (1, "authlevel: only 1, 2 or 3 is valid (clear, auth, crypt)");
    }

    printf ("Perf\n");
    printf ("host: %s, volume: %d, auth: %s\n",
	    arg_host, arg_volume, auth_string);

    if (arg_vnode && arg_uniq)
	do_readbench(arg_host, arg_volume, arg_vnode, arg_uniq, auth);
    else
	do_bench (arg_host, arg_volume, auth);
    
    return 0;
}


