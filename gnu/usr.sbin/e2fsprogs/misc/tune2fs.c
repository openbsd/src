/*
 * tune2fs.c		- Change the file system parameters on
 *			  an unmounted second extended file system
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright 1995, 1996, 1997 by Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

/*
 * History:
 * 93/06/01	- Creation
 * 93/10/31	- Added the -c option to change the maximal mount counts
 * 93/12/14	- Added -l flag to list contents of superblock
 *                M.J.E. Mol (marcel@duteca.et.tudelft.nl)
 *                F.W. ten Wolde (franky@duteca.et.tudelft.nl)
 * 93/12/29	- Added the -e option to change errors behavior
 * 94/02/27	- Ported to use the ext2fs library
 * 94/03/06	- Added the checks interval from Uwe Ohse (uwe@tirka.gun.de)
 */

#include <fcntl.h>
#include <grp.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include <linux/ext2_fs.h>

#include "ext2fs/ext2fs.h"
#include "et/com_err.h"
#include "uuid/uuid.h"
#include "e2p/e2p.h"

#include "../version.h"

const char * program_name = "tune2fs";
char * device_name = NULL;
char * new_label = NULL;
char * new_last_mounted = NULL;
char * new_UUID = NULL;
int c_flag = 0;
int C_flag = 0;
int e_flag = 0;
int g_flag = 0;
int i_flag = 0;
int l_flag = 0;
int L_flag = 0;
int m_flag = 0;
int M_flag = 0;
int r_flag = 0;
int s_flag = -1;
int u_flag = 0;
int U_flag = 0;
int max_mount_count, mount_count;
unsigned long interval;
unsigned long reserved_ratio = 0;
unsigned long reserved_blocks = 0;
unsigned short errors;
unsigned long resgid = 0;
unsigned long resuid = 0;

#ifndef HAVE_STRCASECMP
static int strcasecmp (char *s1, char *s2)
{
	while (*s1 && *s2) {
		int ch1 = *s1++, ch2 = *s2++;
		if (isupper (ch1))
			ch1 = tolower (ch1);
		if (isupper (ch2))
			ch2 = tolower (ch2);
		if (ch1 != ch2)
			return ch1 - ch2;
	}
	return *s1 ? 1 : *s2 ? -1 : 0;
}
#endif

static volatile void usage (void)
{
	fprintf (stderr, "Usage: %s [-c max-mounts-count] [-e errors-behavior] "
		 "[-g group]\n"
		 "\t[-i interval[d|m|w]] [-l] [-s] [-m reserved-blocks-percent]\n"
		 "\t[-r reserved-blocks-count] [-u user] [-C mount-count]\n"
		 "\t[-L volume-label] [-M last-mounted-dir] [-U UUID] "
		 "device\n", program_name);
	exit (1);
}

void main (int argc, char ** argv)
{
	char c;
	char * tmp;
	errcode_t retval;
	ext2_filsys fs;
	struct ext2fs_sb *sb;
	struct group * gr;
	struct passwd * pw;
	int open_flag = 0;

	fprintf (stderr, "tune2fs %s, %s for EXT2 FS %s, %s\n",
		 E2FSPROGS_VERSION, E2FSPROGS_DATE,
		 EXT2FS_VERSION, EXT2FS_DATE);
	if (argc && *argv)
		program_name = *argv;
	initialize_ext2_error_table();
	while ((c = getopt (argc, argv, "c:e:g:i:lm:r:s:u:C:L:M:U:")) != EOF)
		switch (c)
		{
			case 'c':
				max_mount_count = strtoul (optarg, &tmp, 0);
				if (*tmp || max_mount_count > 16000)
				{
					com_err (program_name, 0,
						 "bad mounts count - %s",
						 optarg);
					usage ();
				}
				c_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'C':
				mount_count = strtoul (optarg, &tmp, 0);
				if (*tmp || mount_count > 16000)
				{
					com_err (program_name, 0,
						 "bad mounts count - %s",
						 optarg);
					usage ();
				}
				C_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'e':
				if (strcmp (optarg, "continue") == 0)
					errors = EXT2_ERRORS_CONTINUE;
				else if (strcmp (optarg, "remount-ro") == 0)
					errors = EXT2_ERRORS_RO;
				else if (strcmp (optarg, "panic") == 0)
					errors = EXT2_ERRORS_PANIC;
				else
				{
					com_err (program_name, 0,
						 "bad error behavior - %s",
						 optarg);
					usage ();
				}
				e_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'g':
				resgid = strtoul (optarg, &tmp, 0);
				if (*tmp)
				{
					gr = getgrnam (optarg);
					if (gr == NULL)
						tmp = optarg;
					else {
						resgid = gr->gr_gid;
						*tmp =0;
					}
				}
				if (*tmp)
				{
					com_err (program_name, 0,
						 "bad gid/group name - %s",
						 optarg);
					usage ();
				}
				g_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'i':
				interval = strtoul (optarg, &tmp, 0);
				switch (*tmp) {
				case 's':
					tmp++;
					break;
				case '\0':
				case 'd':
				case 'D': /* days */
					interval *= 86400;
					if (*tmp != '\0')
						tmp++;
					break;
				case 'm':
				case 'M': /* months! */
					interval *= 86400 * 30;
					tmp++;
					break;
				case 'w':
				case 'W': /* weeks */
					interval *= 86400 * 7;
					tmp++;
					break;
				}
				if (*tmp || interval > (365 * 86400))
				{
					com_err (program_name, 0,
						 "bad interval - %s", optarg);
					usage ();
				}
				i_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'l':
				l_flag = 1;
				break;
			case 'L':
				new_label = optarg;
				L_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'm':
				reserved_ratio = strtoul (optarg, &tmp, 0);
				if (*tmp || reserved_ratio > 50)
				{
					com_err (program_name, 0,
						 "bad reserved block ratio - %s",
						 optarg);
					usage ();
				}
				m_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'M':
				new_last_mounted = optarg;
				M_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'r':
				reserved_blocks = strtoul (optarg, &tmp, 0);
				if (*tmp)
				{
					com_err (program_name, 0,
						 "bad reserved blocks count - %s",
						 optarg);
					usage ();
				}
				r_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 's':
				s_flag = atoi(optarg);
				open_flag = EXT2_FLAG_RW;
				break;
			case 'u':
				resuid = strtoul (optarg, &tmp, 0);
				if (*tmp)
				{
					pw = getpwnam (optarg);
					if (pw == NULL)
						tmp = optarg;
					else {
						resuid = pw->pw_uid;
						*tmp = 0;
					}
				}
				if (*tmp)
				{
					com_err (program_name, 0,
						 "bad uid/user name - %s",
						 optarg);
					usage ();
				}
				u_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			case 'U':
				new_UUID = optarg;
				U_flag = 1;
				open_flag = EXT2_FLAG_RW;
				break;
			default:
				usage ();
		}
	if (optind < argc - 1 || optind == argc)
		usage ();
	if (!open_flag && !l_flag)
		usage();
	device_name = argv[optind];
	retval = ext2fs_open (device_name, open_flag, 0, 0,
			      unix_io_manager, &fs);
        if (retval)
	{
		com_err (program_name, retval, "while trying to open %s",
			 device_name);
		printf("Couldn't find valid filesystem superblock.\n");
		exit(1);
	}
	sb = (struct ext2fs_sb *) fs->super;

	if (c_flag) {
		fs->super->s_max_mnt_count = max_mount_count;
		ext2fs_mark_super_dirty(fs);
		printf ("Setting maximal mount count to %d\n",
			max_mount_count);
	}
	if (C_flag) {
		fs->super->s_mnt_count = mount_count;
		ext2fs_mark_super_dirty(fs);
		printf ("Setting current mount count to %d\n", mount_count);
	}
	if (e_flag) {
		fs->super->s_errors = errors;
		ext2fs_mark_super_dirty(fs);
		printf ("Setting error behavior to %d\n", errors);
	}
	if (g_flag)
#ifdef	EXT2_DEF_RESGID
	{
		fs->super->s_def_resgid = resgid;
		ext2fs_mark_super_dirty(fs);
		printf ("Setting reserved blocks gid to %lu\n", resgid);
	}
#else
		com_err (program_name, 0,
			 "The -g option is not supported by this version -- "
			 "Recompile with a newer kernel");
#endif
	if (i_flag)
	{
		fs->super->s_checkinterval = interval;
		ext2fs_mark_super_dirty(fs);
		printf ("Setting interval between check %lu seconds\n", interval);
	}
	if (m_flag)
	{
		fs->super->s_r_blocks_count = (fs->super->s_blocks_count / 100)
			* reserved_ratio;
		ext2fs_mark_super_dirty(fs);
		printf ("Setting reserved blocks percentage to %lu (%u blocks)\n",
			reserved_ratio, fs->super->s_r_blocks_count);
	}
	if (r_flag)
	{
		if (reserved_blocks >= fs->super->s_blocks_count)
		{
			com_err (program_name, 0,
				 "reserved blocks count is too big (%ul)",
				 reserved_blocks);
			exit (1);
		}
		fs->super->s_r_blocks_count = reserved_blocks;
		ext2fs_mark_super_dirty(fs);
		printf ("Setting reserved blocks count to %lu\n",
			reserved_blocks);
	}
	if (s_flag == 1) {
#ifdef EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER
		if (sb->s_feature_ro_compat &
		    EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)
			fprintf(stderr, "\nThe filesystem already "
				" has spare superblocks.\n");
		else {
			sb->s_feature_ro_compat |=
				EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
			fs->super->s_state &= ~EXT2_VALID_FS;
			ext2fs_mark_super_dirty(fs);
			printf("\nSparse superblock flag set.  "
			       "Please run e2fsck on the filesystem.\n");
		}
#else /* !EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER */
		com_err (program_name, 0,
			 "The -s option is not supported by this version -- "
			 "Recompile with a newer kernel");
#endif /* EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER */
	}
	if (s_flag == 0) {
#ifdef EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER
		if (!(sb->s_feature_ro_compat &
		      EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER))
			fprintf(stderr, "\nThe filesystem already "
				" does not support spare superblocks.\n");
		else {
			sb->s_feature_ro_compat &=
				~EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
			fs->super->s_state &= ~EXT2_VALID_FS;
			fs->flags |= EXT2_FLAG_MASTER_SB_ONLY;
			ext2fs_mark_super_dirty(fs);
			printf("\nSparse superblock flag cleared.  "
			       "Please run e2fsck on the filesystem.\n");
		}
#else /* !EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER */
		com_err (program_name, 0,
			 "The -s option is not supported by this version -- "
			 "Recompile with a newer kernel");
#endif /* EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER */
	}
	
	if (u_flag)
#ifdef	EXT2_DEF_RESUID
	{
		fs->super->s_def_resuid = resuid;
		ext2fs_mark_super_dirty(fs);
		printf ("Setting reserved blocks uid to %lu\n", resuid);
	}
#else
		com_err (program_name, 0,
			 "The -u option is not supported by this version -- "
			 "Recompile with a newer kernel");
#endif
	if (L_flag) {
		memset(sb->s_volume_name, 0, sizeof(sb->s_volume_name));
		strncpy(sb->s_volume_name, new_label,
			sizeof(sb->s_volume_name));
		ext2fs_mark_super_dirty(fs);
	}
	if (M_flag) {
		memset(sb->s_last_mounted, 0, sizeof(sb->s_last_mounted));
		strncpy(sb->s_last_mounted, new_last_mounted,
			sizeof(sb->s_last_mounted));
		ext2fs_mark_super_dirty(fs);
	}
	if (U_flag) {
		if (strcasecmp(new_UUID, "null") == 0) {
			uuid_clear(sb->s_uuid);
		} else if (strcasecmp(new_UUID, "random") == 0) {
			uuid_generate(sb->s_uuid);
		} else if (uuid_parse(new_UUID, sb->s_uuid)) {
			com_err(program_name, 0, "Invalid UUID format\n");
			exit(1);
		}
		ext2fs_mark_super_dirty(fs);
	}

	if (l_flag)
		list_super (fs->super);
	ext2fs_close (fs);
	exit (0);
}
