/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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

#include "appl_locl.h"
#include <sl.h>
#include "vos_local.h"

RCSID("$arla: vos_backup.c,v 1.5 2003/04/08 00:03:02 lha Exp $");

static char *vol;
static char *cell;
static int noauth;
static int localauth;
static int verbose;
static int helpflag;

static void
backup_volume (const char *volume,
	       const char *cell, arlalib_authflags_t auth)
{
    struct rx_connection *conn_vldb = NULL;
    struct rx_connection *conn_volser = NULL;
    char *newname = NULL;
    int32_t newVol;
    int32_t dbhost;
    int error;
    int ret,backexists=TRUE;
    nvldbentry the_vlentry;
    int32_t trans_id_rw, trans_id_bk;

    if (cell == NULL)
	cell = cell_getthiscell ();

    if(verbose)
	printf("Getting volume information for volume: %s... ", volume);

    error = get_vlentry (cell, NULL, volume, auth, &the_vlentry);
    if (error) {
        fprintf (stderr, "vos_backup: get_vlentry: %s\n",
               koerr_gettext(error));
	goto out;
    }

    if(verbose)
	printf("done.\n");

    conn_volser = arlalib_getconnbyaddr(cell,
					htonl(the_vlentry.serverNumber[0]),
					NULL,
					afsvolport,
					VOLSERVICE_ID,
					auth);
    if (conn_volser == NULL) {
        fprintf (stderr, "vos_backup: arlalib_getconnbyaddr: %s\n",
               koerr_gettext(error));
	goto out;
    }

    error = VOLSER_AFSVolTransCreate(conn_volser,
				     the_vlentry.volumeId[BACKVOL],
				     the_vlentry.serverPartition[0],
				     ITReadOnly,
				     &trans_id_bk);
    if (error) {
	backexists = FALSE;
    }

    if(trans_id_bk) {
	error = VOLSER_AFSVolEndTrans(conn_volser, trans_id_bk, &ret);
	if (error) {
	    fprintf (stderr, "backup_volume: VolTransCreate failed: %s\n",
		     koerr_gettext(error));
	    goto out;
	}
    }

    newVol = the_vlentry.volumeId[BACKVOL];

    error = VOLSER_AFSVolTransCreate(conn_volser,
				     the_vlentry.volumeId[0], /* XXX */
				     the_vlentry.serverPartition[0],
				     ITReadOnly,
				     &trans_id_rw);
    if (error) {
	fprintf (stderr, "backup_volume: VolTransCreate failed: %s\n",
		 koerr_gettext(error));
	goto out;
    }

    if (backexists) {
	if(verbose)
	    printf("Recloning volume %d... ", newVol);

	error = VOLSER_AFSVolReClone(conn_volser, trans_id_rw, newVol);
    } else {
	asprintf(&newname, "%s.backup", the_vlentry.name);
	if (newname == NULL) {
	    fprintf (stderr, "backup volume: asprintf failed: %s\n",
		     strerror(errno));
	    goto out;
	}

	if(verbose)
	    printf("Cloning to volume %s... ", newname);

	error = VOLSER_AFSVolClone(conn_volser,
				   trans_id_rw,
				   0,
				   BACKVOL,
				   newname,
				   &newVol);
    }

    if (error) {
	fprintf (stderr, "backup volume: AFSVolClone failed: %s\n",
		 koerr_gettext(error));
	goto trans_out;
    }

    if(verbose)
	printf("done.\n");

    error = arlalib_getsyncsite (cell, NULL, afsvldbport,
				 &dbhost, auth);
    if (error) {
	fprintf (stderr, "vos_createvolume: arla_getsyncsite: %s\n", 
		 koerr_gettext(error));
	goto trans_out;
    }

    conn_vldb = arlalib_getconnbyaddr(cell, dbhost, NULL,
				     afsvldbport,
				     VLDB_SERVICE_ID,
				     auth);
    if (conn_vldb == NULL) {
	fprintf (stderr,
		 "vos_createvolume: arlalib_getconnbyaddr: vldb-host: 0x%x\n",
	       dbhost);
	goto trans_out;
    }

    the_vlentry.volumeId[BACKVOL] = newVol;
    the_vlentry.flags |= VLF_BACKEXISTS;

    if(verbose)
	printf("Updating information in vldb... ");

    error = VL_ReplaceEntryN(conn_vldb,
			   the_vlentry.volumeId[RWVOL],
			   RWVOL,
			   &the_vlentry,
			   LOCKREL_OPCODE | LOCKREL_AFSID | LOCKREL_TIMESTAMP);
    if (error) {
	fprintf (stderr, "vos_createvolume: VL_UpdateEntry: %s\n", 
	       koerr_gettext (error));
    } else if (verbose)
	printf("done.\n");

    error = VOLSER_AFSVolTransCreate(conn_volser,
                                     the_vlentry.volumeId[BACKVOL], /* XXX */
                                     the_vlentry.serverPartition[0],
                                     ITOffline,
                                     &trans_id_bk);
    if (error) {
        fprintf (stderr, "vos_createvolume: VOLSER_AFSVolTransCreate: %s\n",
                 koerr_gettext(error));
        goto trans_out;
    }

    error = VOLSER_AFSVolSetFlags(conn_volser, trans_id_bk,0);
    if (error) {
        fprintf (stderr, "vos_createvolume: VOLSER_AFSVolSetFlags: %s\n",
                 koerr_gettext(error));
	goto trans_out_bk;
    }


trans_out_bk:
    ret = 0;

    error = VOLSER_AFSVolEndTrans(conn_volser, trans_id_bk, &ret);
    if (error)
	fprintf (stderr, "backup_Volume: VolEndTrans failed: %s\n",
		 koerr_gettext(error));

trans_out:
    ret = 0;

    error = VOLSER_AFSVolEndTrans(conn_volser, trans_id_rw, &ret);
    if (error)
	fprintf (stderr, "dump_volume: VolEndTrans failed: %s\n",
		 koerr_gettext(error));

out:
    if (conn_volser != NULL)
	arlalib_destroyconn (conn_volser);

    if (conn_vldb != NULL)
	arlalib_destroyconn (conn_vldb);

    if(newname != NULL)
	free(newname);

}


static struct agetargs backupargs[] = {
    {"id",	0, aarg_string,  &vol,  "id of volume", "volume",
     aarg_mandatory},
    {"cell",	0, aarg_string,  &cell, "what cell to use", NULL},
    {"noauth",	0, aarg_flag,    &noauth, "do not authenticate", NULL},
    {"localauth",0,aarg_flag,    &localauth, "localauth", NULL},
    {"verbose", 0, aarg_flag,	&verbose, "be verbose", NULL},
    {"help",	0, aarg_flag,    &helpflag, NULL, NULL},
    {NULL,      0, aarg_end,	NULL}
};

static void
backupusage(void)
{
    aarg_printusage(backupargs, "vos backup", "", AARG_AFSSTYLE);
}


int
vos_backup(int argc, char **argv)
{
    int optind = 0;

    noauth = localauth = verbose = 0;
    cell = vol = NULL;

    if (agetarg (backupargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	backupusage ();
	return 0;
    }

    if (helpflag) {
	backupusage ();
	return 0;
    }

    argc -= optind;
    argv += optind;

    backup_volume (vol, cell,
		   arlalib_getauthflag (noauth, localauth, 0, 0));
    return 0;
}

static int
backup_volume_wrap (void *data, struct vldbentry *e)
{
  if(e->volumeType == RWVOL) 
    backup_volume (e->name, cell, localauth);

  return 0;
  
}

static struct agetargs backupsysargs[] = {
    {"cell",	0, aarg_string,  &cell, "what cell to use", NULL},
    {"noauth",	0, aarg_flag,    &noauth, "do not authenticate", NULL},
    {"localauth",0,aarg_flag,    &localauth, "localauth", NULL},
    {"verbose", 0, aarg_flag,	&verbose, "be verbose", NULL},
    {"help",	0, aarg_flag,    &helpflag, NULL, NULL},
    {NULL,      0, aarg_end,	NULL}
};

static void
backupsysusage(void)
{
    aarg_printusage(backupsysargs, "vos backupsys", "", AARG_AFSSTYLE);
}

int
vos_backupsys(int argc, char **argv)
{
    int optind = 0;

    noauth = localauth = verbose = 0;
    cell = vol = NULL;

    if (agetarg (backupsysargs, argc, argv, &optind, AARG_AFSSTYLE)) {
	backupsysusage ();
	return 0;
    }

    if (helpflag) {
	backupsysusage ();
	return 0;
    }

    argc -= optind;
    argv += optind;

    return vos_listvldb_iter (NULL, cell, NULL, 
			      NULL,NULL,
			      arlalib_getauthflag (noauth, localauth, 0, 0),
			      backup_volume_wrap, NULL);
    return 0;    

}
