/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
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

#include "fsrv_locl.h"

RCSID("$Id: fileserver.c,v 1.1 2000/09/11 14:41:13 art Exp $");

typedef enum { NO_SALVAGE, NORMAL_SALVAGE, SALVAGE_ALL } salvage_options_e;

/*
 * Local varibles
 */

static struct rx_service *fsservice;
static struct rx_service *volservice;

static char *cell = NULL;
static char *realm = NULL;
static char *srvtab_file = NULL;
static char *debug_levels = NULL;
static char *log_name = "syslog";
static int no_auth = 0;
static int force_salvage = 0;
static int salvage_options = NORMAL_SALVAGE;

/*
 *
 */

static void
sigusr1 (int foo)
{
    printf ("sigusr1\n");
    chdir ("/vicepa");
    exit (2); /* XXX profiler */
}

/*
 *
 */

static void
mark_clean (volume_handle *vol)
{
    vol->flags.offlinep	= FALSE;
    vol->flags.salvaged	= TRUE;
    vol->flags.attacherr	= FALSE;
}


static int
do_salvage (volume_handle *vol)
{
    int ret;

    ret = salvage_volume (vol);
    if (ret == 0) {
	mark_clean (vol);
    } else {
	vol->flags.offlinep	= TRUE;
	vol->flags.salvaged	= TRUE;
	vol->flags.attacherr	= TRUE;
    }
    return 0;
}

static int
salvage_and_attach (volume_handle *vol, void *arg)
{
    mlog_log (MDEBMISC, "salvaging and attatching to volume: %u", 
	      (u_int32_t)vol->vol);

    switch (salvage_options) {
    case NORMAL_SALVAGE:
	if (vol->flags.cleanp == FALSE)
	    do_salvage (vol);
	break;
    case NO_SALVAGE:
	mark_clean (vol);
	break;
    case SALVAGE_ALL:
	do_salvage (vol);
	break;
    default:
	abort();
    }
    return 0;
}


/*
 *
 */

static void
attach_volumes(void)
{
    mlog_log (MDEBMISC, "fileserver starting to attach to volumes");
    vld_iter_vol (salvage_and_attach, NULL);
    mlog_log (MDEBMISC, "fileserver done attaching to volumes");
}

/*
 * Main
 */

static struct getargs args[] = {
    {"cell",	0, arg_string,    &cell, "what cell to use"},
    {"realm",	0, arg_string,	  &realm, "what realm to use"},
    {"noauth",	0, arg_flag,	  &no_auth, "disable authentication checks"},
    {"debug",	0, arg_string,	  &debug_levels, "debug level"},
    {"log",	0, arg_string,	  &log_name, "log device name"},
    {"srvtab",'s', arg_string,    &srvtab_file, "what srvtab to use"},
    {"salvage", 0, arg_flag,      &force_salvage, "Force a salvage for all vols"},
    { NULL, 0, arg_end, NULL }
};

static void
usage(void)
{
    arg_printusage(args, "fileserver", "", ARG_GNUSTYLE);
}

int 
main(int argc, char **argv)
{
    int ret;
    int optind = 0;
    PROCESS pid;
    
    set_progname (argv[0]);

    if (getarg (args, argc, argv, &optind, ARG_GNUSTYLE)) {
	usage ();
	return 1;
    }

    argc -= optind;
    argv += optind;

    if (argc) {
	printf("unknown option %s\n", *argv);
	return 1;
    }

    printf ("fileserver booting\n");

    LWP_InitializeProcessSupport (LWP_NORMAL_PRIORITY, &pid);

    mlog_loginit (log_name, milko_deb_units, MDEFAULT_LOG);
    if (debug_levels)
	mlog_log_set_level (debug_levels);
    
    if (force_salvage)
      salvage_options = SALVAGE_ALL;

    ports_init();
    cell_init(0);
    ropa_init(400,1000);

    if (no_auth)
	sec_disable_superuser_check ();

    if (cell)
	cell_setthiscell (cell);

    network_kerberos_init (srvtab_file);
    
    ret = network_init(htons(afsport), "fs", FS_SERVICE_ID, 
		       RXAFS_ExecuteRequest, &fsservice, NULL);
    if (ret)
	errx (1, "network_init failed with %d", ret);

    fsservice->destroyConnProc = fs_connsec_destroyconn;
    
    ret = network_init(htons(afsvolport), "volser", VOLSER_SERVICE_ID, 
		       VOLSER_ExecuteRequest, &volservice, NULL);
    if (ret)
	errx (1, "network_init failed with %d", ret);

    signal (SIGUSR1, sigusr1);
    umask (S_IRWXG|S_IRWXO); /* 077 */

    vld_boot();
    vld_init();

    mnode_init (4711); /* XXX */
    attach_volumes();
    
    rx_SetMaxProcs(fsservice,5) ;
    rx_SetMaxProcs(volservice,5) ;

    printf ("fileserver started, serving data\n");

    rx_StartServer(1) ;

    abort() ;
    return 0;
}


