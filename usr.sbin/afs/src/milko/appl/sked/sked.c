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

#include <config.h>
#include "roken.h"

RCSID("$arla: sked.c,v 1.33 2002/04/26 16:11:41 lha Exp $");

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>

#include <err.h>
#include <assert.h>

#include <sl.h>
#include <agetarg.h>

#include <atypes.h>
#include <rx/rx.h>
#include <fbuf.h>
#include <fs_def.h>
#include <pts.h>
#include <dpart.h>
#include <vstatus.h>
#include <voldb.h>
#include <vld.h>
#include <mnode.h>
#include <vldb.h>
#include <mdir.h>
#include <salvage.h>
#include <mlog.h>
#include <mdebug.h>

static int interactivep = 0;

#define INTER_RETURN(ret) if (interactivep) return 0; else exit (ret);

static int
create_volume (uint32_t part, uint32_t num, char *name)
{
    int ret;
    int32_t backstoretype = VLD_SVOL;
    struct dp_part *dp;


    ret = dp_create (part, &dp);
    if (ret)
	return ret;

    ret = vld_create_volume (dp, num, name, backstoretype, RWVOL, 0);
    if (ret)
	return ret;

    return 0;
}

/*
 *
 */

static int
show_volume (uint32_t part, uint32_t num)
{
    volume_handle *vh = NULL;
    int ret;
    struct dp_part *dp = NULL;

    ret = dp_create (part, &dp);
    if (ret) {
	printf ("show_volume: dp_create: %d\n", ret);
	goto out;
    }

    ret = vld_open_volume_by_num (dp, num, &vh);
    if (ret) {
	printf ("show_volume: vld_open_volume_ny_num: %d\n", ret);
	goto out;
    }
    
    ret = vld_info_uptodatep (vh);
    if (ret) {
	printf ("show_volume: vld_info_uptodatep: %d\n", ret);
	goto out;
    }

    printf ("volume-type:\t%s\n", vld_backstoretype_name(vh->type));
    printf ("------------------------\n");
    vol_pretty_print_info (stdout, &vh->info);
    
    ret = vld_db_uptodate (vh);
    if (ret) {
	printf ("show_volume: vld_db_uptodate: %d\n", ret);
	goto out;
    }

    printf ("voldb header:\n dir:\n");
    voldb_pretty_print_header(vh->db[0]);
    printf ("file:\n");
    voldb_pretty_print_header(vh->db[1]);

    printf ("info: ");
    vstatus_print_onode (stdout, &vh->sino);
    printf ("dir: ");
    vstatus_print_onode (stdout, &vh->dino);
    printf ("file: ");
    vstatus_print_onode (stdout, &vh->fino);

 out:
    if (vh)
	vld_free (vh);
    if (dp)
	dp_free (dp);

    INTER_RETURN(ret ? 1 : 0);
}

/*
 * 
 */

static void
showvols_cb (void *data, int fd)
{
    struct dp_part *dp = data;
    volume_handle *vh;
    int ret;

    assert (dp);

    ret = vld_open_volume_by_fd (dp, fd, &vh);
    if (ret) {
	printf ("showvols_cb: vld_open_volume_by_fd: %d\n", ret);
    }
    
    ret = vld_info_uptodatep (vh);
    if (ret) {
	printf ("showvols_cb: vld_info_uptodatep: %d\n", ret);
	vld_free (vh);
    }

    if (vh->vol != vh->info.volid)
	printf ("PANIC: vh->vol != vh->info.volid\n");
    
    printf ("%-32s %-4s %-4s %-8u %-8u %-8u %-8u\n",
	    vh->info.name, vld_backstoretype_name(vh->type), 
	    vol_voltype2name(vh->info.type), vh->vol,
	    vh->info.backupID, vh->info.parentID, 
	    vh->info.cloneID);

    vld_free (vh);
}

/*
 * List volumes on partition `part'.
 */

static int
show_vols (uint32_t part)
{
    struct dp_part *p;
    int ret;

    ret = dp_create (part, &p);
    if (ret)
	errx (1, "dp_create returned %d", ret);
    
    printf ("%-32s %-4s %-4s %-8s %-8s %-8s %-8s\n",
	    "Name", "Type", "Type", 
	    "VolID", "BackupID", "ParentID", "cloneID");
    ret = dp_findvol (p, showvols_cb, p);
    if (ret)
	errx (1, "dp_findvol returned %d", ret);

    dp_free (p);

    return 0;
}

/*
 * sl commands
 */

static int
volcreate_cmd (int argc, char **argv)
{
    uint32_t num, part;
    int ret;

    if (argc < 4 || argc > 5) {
	printf ("usage: volcreate part num name [partdir]\n");
	INTER_RETURN(1);
    }

    if (argc == 5)
	dpart_root = argv[4];

    ret = dp_parse (argv[1], &part);
    if (ret) {
	printf ("volcreate: `%s' is a invalid argument for partition\n", argv[1]);
	INTER_RETURN(1);
    }
	
    num = atoi (argv[2]);
    if (num == 0) {
	printf ("volcreate: `%s' is a invalid argument for volnum\n", argv[2]);
	INTER_RETURN(1);
    }

    ret = create_volume (part, num, argv[3]);
    if (ret) {
	printf ("volcreate: create_volume returned: %d\n", ret);
	INTER_RETURN(ret);
    }

    printf ("volume %u created successfully\n", num);

    return 0;
}

/*
 *
 */

static int
volshow_cmd (int argc, char **argv)
{
    uint32_t num, part;
    int ret;

    if (argc < 3 || argc > 4) {
	printf ("usage: volshow part num [partdir]\n");
	INTER_RETURN(1);
    }

    part = atoi (argv[1]);

    num = atoi (argv[2]);
    if (num == 0) {
	printf ("volcreate: `%s' is a invalid argument for volnum\n", argv[2]);
	INTER_RETURN(1);
    }

    dpart_root = argv[3];

    ret = show_volume (part, num);
    if (ret) {
	printf ("volshow: show_volume returned: %d\n", ret);
	INTER_RETURN(ret);
    }

    return 0;
}

/*
 *
 */

static int
vollist_cmd (int argc, char **argv)
{
    uint32_t part;
    int ret;

    if (argc != 2) {
	printf ("usage: vollist part\n");
	INTER_RETURN(1);
    }

    part = atoi (argv[1]);

    ret = show_vols (part);
    if (ret) {
	printf ("vollist: show_vols returned: %d\n", ret);
	INTER_RETURN(ret);
    }

    return 0;
}

/*
 *
 */

static int
volls_dir_cb (VenusFid *fid, const char *name, void *arg)
{
    printf ("%-60s %d.%d.%d\n",
	    name, 
	    fid->fid.Volume,
	    fid->fid.Vnode, 
	    fid->fid.Unique);
    return 0;
}

static int
volls_cmd (int argc, char **argv)
{
    uint32_t part;
    uint32_t vol;
    uint32_t vnode;
    struct dp_part *dp;
    int ret;
    
    if (argc < 4 || argc > 5) {
	printf ("volls part volume# vnode# [partdir]\n");
	INTER_RETURN(1);
    }

    part = atoi(argv[1]);
    vol = atoi(argv[2]);
    if (vol == 0) {
	printf ("erronous volume number\n");
	INTER_RETURN(1);
    }

    vnode = atoi(argv[3]);
    if (vnode == 0) {
	printf ("erronous vnode number\n");
	INTER_RETURN(1);
    }

    dpart_root = argv[4];

    ret = dp_create (part, &dp);
    if (ret) {
	printf ("volls: dp_create: %d\n", ret);
	INTER_RETURN(1);
    }

    {
	struct volume_handle *volh;
	struct mnode n;
	struct msec m;

	memset (&n, 0, sizeof(n));

	ret = vld_open_volume_by_num (dp, vol, &volh);
	if (ret)
	    errx (1, "volls: vld_open_volume_by_num: %d", ret);

	ret = vld_db_uptodate (volh);
	if (ret)
	    errx (1, "volls: vld_db_uptodate: %d", ret);

	n.fid.Vnode = vnode;
	m.flags = VOLOP_GETSTATUS;

	ret = vld_open_vnode (volh, &n, &m);
	if (ret)
	    errx (1, "volls: vld_open_vnode failed with %d", ret);

	printf ("mode=%o ino=%x dev=%x rdev=%x\n",
		n.sb.st_mode, (int) n.sb.st_ino,
		(int) n.sb.st_dev, (int) n.sb.st_rdev);
	
	printf ("size: %d\nparent fid: %d.%d\n",
		n.fs.Length, n.fs.ParentVnode, n.fs.ParentUnique);
	
	if (afs_dir_p (vnode)) {
	    VenusFid fid;
	    fid.Cell = 0;
	    fid.fid.Volume = volh->vol;
	    fid.fid.Vnode = vnode;
	    fid.fid.Unique = 0;

	    ret = mdir_readdir (&n, volls_dir_cb, NULL, fid);
	    if (ret)
		errx (1, "volls: mdir_readdir failed with %d", ret);

	} else {
	    printf ("this is a file.... XXX\n");
	}

	close (n.fd);

	vld_free(volh);

    }
    return 0;
}

/*
 *
 */

static int
volvnode_cmd (int argc, char **argv)
{
    uint32_t part;
    char *part_str = NULL;
    uint32_t vol;
    char *vol_str = NULL;
    struct dp_part *dp;
    int do_list = 0;
    int ret, optind = 0;

    struct agetargs args[] = {
	{"part",	0, aarg_string, NULL,
	 "what part to use", NULL, aarg_mandatory},
	{"vol",	        0, aarg_string, NULL,
	 "what vol to use", NULL, aarg_mandatory},
	{"list",      'l', aarg_flag, NULL,
	 "list vnodes" },
        { NULL, 0, aarg_end, NULL }
    }, *arg;

    arg = args;
    arg->value = &part_str;   arg++;
    arg->value = &vol_str;     arg++;
    arg->value = &do_list;   arg++;

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(args, "volvnode", NULL, AARG_AFSSTYLE);
	INTER_RETURN(1);
    }
    part = atoi(part_str);
    vol = atoi(vol_str);
    if (vol == 0) {
	printf ("erronous volume number\n");
	INTER_RETURN(1);
    }

    ret = dp_create (part, &dp);
    if (ret) {
	printf ("volls: dp_create: %d\n", ret);
	INTER_RETURN(1);
    }

    {
	struct volume_handle *volh;
	int i;
	uint32_t num, flags;


	ret = vld_open_volume_by_num (dp, vol, &volh);
	if (ret)
	    errx (1, "volvnode: vld_open_volume_by_num: %d", ret);

	/*
	 * print volume header
	 */

	ret = vld_info_uptodatep (volh);
	if (ret)
	    errx (1, "vld_info_uptodatep: %d", ret);
	
	vol_pretty_print_info (stdout, &volh->info);

	/*
	 * Print nodes
	 */

	ret = vld_db_uptodate (volh);
	if (ret)
	    errx (1, "vld_db_uptodate: %d", ret);

	/*
	 * Print dir nodes
	 */

	ret = voldb_header_info (volh->db[0], &num, &flags);
	if (ret)
	    errx (1, "voldb_header_info: %d", ret);

	printf ("---------------------------------\n");
	printf ("dir nodes contain:\n");
	printf (" num: %d\tflags 0x%x\n", num, flags);
	printf ("---------------------------------\n");

	if (do_list) {
	    for (i = 0; i < num; i++) {
		struct voldb_entry e;
		
		printf ("dnode #%d\n", i);
		
		ret = voldb_get_entry (volh->db[0], i, &e);
		if (ret)
		    errx (1, "voldb_get_dir (%d) returned: %d", i, ret);
		voldb_pretty_print_dir (&e.u.dir);
	    }
	}	    

	/*
	 * Print file nodes
	 */

	ret = voldb_header_info (volh->db[1], &num, &flags);
	if (ret)
	    errx (1, "voldb_header_info: %d", ret);

	printf ("---------------------------------\n");
	printf ("file nodes contain:\n");
	printf (" num: %d\tflags 0x%x\n", num, flags);
	printf ("---------------------------------\n");

	if (do_list) {
	    for (i = 0; i < num; i++) {
		struct voldb_entry e;
		
		printf ("fnode #%d\n", i);
		
		ret = voldb_get_entry (volh->db[1], i, &e);
		if (ret)
		    errx (1, "voldb_get_dir (%d) returned: %d", i, ret);
		voldb_pretty_print_file (&e.u.file);
	    }
	}

	vld_free(volh);

    }
    return 0;
}

/*
 *
 */

static int
fvolcreate_cmd (int argc, char **argv)
{
    uint32_t part;
    char *part_str = NULL;
    char *path_str = NULL;
    int vol_int = 0;
    struct dp_part *dp;
    int ret, optind = 0;

    struct agetargs args[] = {
	{"part",	0, aarg_string, NULL,
	 "what part to use", NULL, aarg_mandatory},
	{"vol",	        0, aarg_integer, NULL,
	 "what vol-number to use", NULL, aarg_mandatory},
	{"path",        0, aarg_string, NULL,
	 "what path to volume-ify", NULL, aarg_mandatory},
        { NULL, 0, aarg_end, NULL }
    }, *arg;

    arg = args;
    arg->value = &part_str;   arg++;
    arg->value = &vol_int;    arg++;
    arg->value = &path_str;   arg++;

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(args, "volvnode", NULL, AARG_AFSSTYLE);
	INTER_RETURN(1);
    }

    part = atoi(part_str);
    if (vol_int == 0) {
	printf ("erronous volume number\n");
	INTER_RETURN(1);
    }

    ret = dp_create (part, &dp);
    if (ret) {
	printf ("volls: dp_create: %d\n", ret);
	INTER_RETURN(1);
    }

    ret = vld_fvol_create_volume_ondisk (dp, vol_int, path_str);
    if (ret) {
	printf ("volls: fvol_create_volume_ondisk: %d\n", ret);
	INTER_RETURN(1);
    }

    return 0;
}

/*
 *
 */

static int
showvolname_cmd (int argc, char **argv)
{
    int ret;
    uint32_t num;
    uint32_t part;
    char name[MAXPATHLEN];

    if (argc != 2 && argc != 3)
	errx (1, "usage: showvolname [part] volnum");

    if (argc == 2) {
	num = atoi (argv[1]);
	if (num == 0)
	    errx (1, "showvolname: `%s' is a invalid argument", argv[1]);
	
	ret = vol_getname (num, name, sizeof (name));
	if (ret) {
	    printf ("vol_getname returned %d", ret);
	    INTER_RETURN(ret);
	}
	printf ("volume name is: %s\n", name);
    } else {
	part = atoi (argv[1]); /* XXX 0 is valid volume */

	num = atoi (argv[2]);
	if (num == 0)
	    errx (1, "showvolname: `%s' is a invalid argument", argv[2]);
	
	ret = vol_getfullname (part, num, name, sizeof (name));
	if (ret)
	    errx (1, "vol_getname returned %d", ret);
	printf ("volume full name is: %s\n", name);
    }

    return 0;
}

/*
 *
 */

static int
salvage_cmd (int argc, char **argv)
{
    char *part_str = NULL;
    int vol = 0;
    struct dp_part *dp;
    int ret, optind = 0;
	struct volume_handle *volh;

    struct agetargs args[] = {
	{"part",	0, aarg_string, NULL,
	 "what part to use", NULL, aarg_mandatory},
	{"vol",	        0, aarg_integer, NULL,
	 "what vol-number to use", NULL, aarg_mandatory},
	{"partdir",	0, aarg_string, NULL,
	 "where to find vicep*", NULL, aarg_optional},
        { NULL, 0, aarg_end, NULL }
    }, *arg;

    arg = args;
    arg->value = &part_str;   arg++;
    arg->value = &vol;    arg++;
    arg->value = &dpart_root;    arg++;

    if (agetarg (args, argc, argv, &optind, AARG_AFSSTYLE)) {
	aarg_printusage(args, "volvnode", NULL, AARG_AFSSTYLE);
	INTER_RETURN(1);
    }

    if (part_str == NULL || vol == 0) {
	warnx ("missing argument");
	INTER_RETURN(1);
    }
    
    dp = dp_getpart (part_str);
    if (dp == NULL) {
	warnx ("erronous volume number");
	INTER_RETURN(1);
    }

    ret = vld_open_volume_by_num (dp, vol, &volh);
    if (ret) {
	warnx ("salvage: vld_open_volume_by_num: %d", ret);
	INTER_RETURN(1);
    }

    ret = salvage_volume (volh);
    if (ret)
	warnx ("salvage returned %d", ret);
    else
	printf ("volume %s (%u) is ok\n", volh->info.name, vol);

    vld_free(volh);

    return 0;
}

/*
 *
 */

static int
version_cmd (int argc, char **argv)
{
    printf ("sked - maintaing volumes the hard way\n");

    printf ("Package: %s\n"
	    "Version: %s\n", PACKAGE, VERSION);
    return 0;
}


static int help_cmd (int argc, char **argv);
static int apropos_cmd (int argc, char **argv);

/*
 *
 */

static SL_cmd cmds[] = {
    {"apropos",	       apropos_cmd,     "apropos topic"},
    {"help",	       help_cmd,        "help text"},
    {"volcreate",      volcreate_cmd,	"create a volume"},
    {"volshow",        volshow_cmd,	"show a volume"},
    {"vollist",        vollist_cmd,	"list volumes on part"},
    {"volls",          volls_cmd,	"list files in volume,vnode pair"},
    {"volvnode",       volvnode_cmd,    "list all vnodes"},
    {"fvolcreate",     fvolcreate_cmd,  "create a fvolume"},
    {"name",           showvolname_cmd, "name of a volume"},
    {"salvage",        salvage_cmd,	"salvage a volume"},
    {"version",        version_cmd,     "show version of sked"},
    { NULL, NULL, NULL}
};

/*
 * Help command
 */

static int
help_cmd (int argc, char **argv)
{
    sl_help (cmds, argc, argv);
    return 0;
}

/*
 * Apropos command
 */

static int
apropos_cmd (int argc, char **argv)
{
    if (argc < 2)
	printf ("apropos <topic>\n");
    else
	sl_apropos (cmds, argv[1]);
    return 0;
}

/*
 * Main
 */

int
main (int argc, char **argv)
{
    int ret;
    Log_method *method;
    char *log_file = "/dev/stderr";
    
    set_progname (argv[0]);

    method = log_open (getprogname(), log_file);
    if (method == NULL)
	errx (1, "log_open failed");
    
   /*
    * We only boot, not init since we dont want to read in all volumes
    */
    vld_boot ();
    mnode_init (173);
    
    mlog_loginit (method, milko_deb_units, MDEFAULT_LOG /* MDEBALL */);

    /*
     * Command loop or if command, eval.
     */

    if (argc < 2) {
	interactivep = 1;
	sl_loop (cmds, "sked (cmd): ");
    } else {
	ret = sl_command(cmds, argc - 1, argv + 1);
	if (ret == -1)
	    printf ("%s: Unknown command\n", argv[1]); 
    }

    return 0;
}
