/* instbin.c - generated from instbin.conf by crunchgen 0.2 */
#define EXECNAME "instbin"
/*	$OpenBSD: instbin.c,v 1.1 2001/06/26 22:23:26 smurph Exp $	*/

/*
 * Copyright (c) 1994 University of Maryland
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of U.M. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  U.M. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * U.M. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL U.M.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: James da Silva, Systems Design and Analysis Group
 *			   Computer Science Department
 *			   University of Maryland at College Park
 */
/*
 * crunched_main.c - main program for crunched binaries, it branches to a 
 * 	particular subprogram based on the value of argv[0].  Also included
 *	is a little program invoked when the crunched binary is called via
 *	its EXECNAME.  This one prints out the list of compiled-in binaries,
 *	or calls one of them based on argv[1].   This allows the testing of
 *	the crunched binary without creating all the links.
 */
#include <stdio.h>
#include <string.h>

struct stub {
    char *name;
    int (*f)();
};

extern struct stub entry_points[];

int main(int argc, char **argv)
{
    char *slash, *basename;
    struct stub *ep;

    if(argv[0] == NULL || *argv[0] == '\0')
	crunched_usage();

    slash = strrchr(argv[0], '/');
    basename = slash? slash+1 : argv[0];

    for(ep=entry_points; ep->name != NULL; ep++)
	if(!strcmp(basename, ep->name)) break;

    if(ep->name)
	return ep->f(argc, argv);
    else {
	fprintf(stderr, "%s: %s not compiled in\n", EXECNAME, basename);
	crunched_usage();
    }
}


int crunched_main(int argc, char **argv)
{
    struct stub *ep;
    int columns, len;

    if(argc <= 1) 
	crunched_usage();

    return main(--argc, ++argv);
}


int crunched_usage()
{
    int columns, len;
    struct stub *ep;

    fprintf(stderr, "Usage: %s <prog> <args> ..., where <prog> is one of:\n",
	    EXECNAME);
    columns = 0;
    for(ep=entry_points; ep->name != NULL; ep++) {
	len = strlen(ep->name) + 1;
	if(columns+len < 80)
	    columns += len;
	else {
	    fprintf(stderr, "\n");
	    columns = len;
	}
	fprintf(stderr, " %s", ep->name);
    }
    fprintf(stderr, "\n");
    exit(1);
}

/* end of crunched_main.c */

extern int _crunched_dd_stub();
extern int _crunched_mount_cd9660_stub();
extern int _crunched_df_stub();
extern int _crunched_dhclient_stub();
extern int _crunched_mount_stub();
extern int _crunched_mount_ext2fs_stub();
extern int _crunched_sync_stub();
extern int _crunched_restore_stub();
extern int _crunched_newfs_msdos_stub();
extern int _crunched_stty_stub();
extern int _crunched_ln_stub();
extern int _crunched_disklabel_stub();
extern int _crunched_pax_stub();
extern int _crunched_ping_stub();
extern int _crunched_cat_stub();
extern int _crunched_ifconfig_stub();
extern int _crunched_ls_stub();
extern int _crunched_less_stub();
extern int _crunched_mount_nfs_stub();
extern int _crunched_fdisk_stub();
extern int _crunched_grep_stub();
extern int _crunched_umount_stub();
extern int _crunched_mount_msdos_stub();
extern int _crunched_rsh_stub();
extern int _crunched_fsck_stub();
extern int _crunched_scsi_stub();
extern int _crunched_mknod_stub();
extern int _crunched_route_stub();
extern int _crunched_ftp_stub();
extern int _crunched_mount_ffs_stub();
extern int _crunched_reboot_stub();
extern int _crunched_ed_stub();
extern int _crunched_cp_stub();
extern int _crunched_gzip_stub();
extern int _crunched_chmod_stub();
extern int _crunched_fsck_ffs_stub();
extern int _crunched_sort_stub();
extern int _crunched_init_stub();
extern int _crunched_newfs_stub();
extern int _crunched_mount_kernfs_stub();
extern int _crunched_tip_stub();
extern int _crunched_rm_stub();
extern int _crunched_mt_stub();
extern int _crunched_mkdir_stub();
extern int _crunched_sed_stub();
extern int _crunched_ksh_stub();
extern int _crunched_sleep_stub();
extern int _crunched_mv_stub();
extern int _crunched_expr_stub();
extern int _crunched_test_stub();
extern int _crunched_hostname_stub();
extern int _crunched_mg_stub();

struct stub entry_points[] = {
	{ "dd", _crunched_dd_stub },
	{ "mount_cd9660", _crunched_mount_cd9660_stub },
	{ "df", _crunched_df_stub },
	{ "dhclient", _crunched_dhclient_stub },
	{ "mount", _crunched_mount_stub },
	{ "mount_ext2fs", _crunched_mount_ext2fs_stub },
	{ "sync", _crunched_sync_stub },
	{ "restore", _crunched_restore_stub },
	{ "newfs_msdos", _crunched_newfs_msdos_stub },
	{ "stty", _crunched_stty_stub },
	{ "ln", _crunched_ln_stub },
	{ "disklabel", _crunched_disklabel_stub },
	{ "pax", _crunched_pax_stub },
	{ "tar", _crunched_pax_stub },
	{ "cpio", _crunched_pax_stub },
	{ "ping", _crunched_ping_stub },
	{ "cat", _crunched_cat_stub },
	{ "ifconfig", _crunched_ifconfig_stub },
	{ "ls", _crunched_ls_stub },
	{ "less", _crunched_less_stub },
	{ "more", _crunched_less_stub },
	{ "mount_nfs", _crunched_mount_nfs_stub },
	{ "fdisk", _crunched_fdisk_stub },
	{ "grep", _crunched_grep_stub },
	{ "fgrep", _crunched_grep_stub },
	{ "egrep", _crunched_grep_stub },
	{ "umount", _crunched_umount_stub },
	{ "mount_msdos", _crunched_mount_msdos_stub },
	{ "rsh", _crunched_rsh_stub },
	{ "fsck", _crunched_fsck_stub },
	{ "scsi", _crunched_scsi_stub },
	{ "mknod", _crunched_mknod_stub },
	{ "route", _crunched_route_stub },
	{ "ftp", _crunched_ftp_stub },
	{ "mount_ffs", _crunched_mount_ffs_stub },
	{ "reboot", _crunched_reboot_stub },
	{ "halt", _crunched_reboot_stub },
	{ "ed", _crunched_ed_stub },
	{ "cp", _crunched_cp_stub },
	{ "gzip", _crunched_gzip_stub },
	{ "gunzip", _crunched_gzip_stub },
	{ "gzcat", _crunched_gzip_stub },
	{ "chmod", _crunched_chmod_stub },
	{ "chgrp", _crunched_chmod_stub },
	{ "chown", _crunched_chmod_stub },
	{ "fsck_ffs", _crunched_fsck_ffs_stub },
	{ "sort", _crunched_sort_stub },
	{ "init", _crunched_init_stub },
	{ "newfs", _crunched_newfs_stub },
	{ "mount_kernfs", _crunched_mount_kernfs_stub },
	{ "tip", _crunched_tip_stub },
	{ "rm", _crunched_rm_stub },
	{ "mt", _crunched_mt_stub },
	{ "eject", _crunched_mt_stub },
	{ "mkdir", _crunched_mkdir_stub },
	{ "sed", _crunched_sed_stub },
	{ "ksh", _crunched_ksh_stub },
	{ "sh", _crunched_ksh_stub },
	{ "-sh", _crunched_ksh_stub },
	{ "sleep", _crunched_sleep_stub },
	{ "mv", _crunched_mv_stub },
	{ "expr", _crunched_expr_stub },
	{ "test", _crunched_test_stub },
	{ "[", _crunched_test_stub },
	{ "hostname", _crunched_hostname_stub },
	{ "mg", _crunched_mg_stub },
	{ EXECNAME, crunched_main },
	{ NULL, NULL }
};
