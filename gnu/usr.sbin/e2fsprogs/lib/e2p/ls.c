/*
 * ls.c			- List the contents of an ext2fs superblock
 *
 * Copyright (C) 1992, 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                                 Laboratoire MASI, Institut Blaise Pascal
 *                                 Universite Pierre et Marie Curie (Paris VI)
 *
 * Copyright (C) 1995, 1996, 1997  Theodore Ts'o <tytso@mit.edu>
 * 
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>

#include "e2p.h"

/*
 * The ext2fs library private definition of the ext2 superblock, so we
 * don't have to depend on the kernel's definition of the superblock,
 * which might not have the latest features.
 */
struct ext2fs_sb {
	__u32	s_inodes_count;		/* Inodes count */
	__u32	s_blocks_count;		/* Blocks count */
	__u32	s_r_blocks_count;	/* Reserved blocks count */
	__u32	s_free_blocks_count;	/* Free blocks count */
	__u32	s_free_inodes_count;	/* Free inodes count */
	__u32	s_first_data_block;	/* First Data Block */
	__u32	s_log_block_size;	/* Block size */
	__s32	s_log_frag_size;	/* Fragment size */
	__u32	s_blocks_per_group;	/* # Blocks per group */
	__u32	s_frags_per_group;	/* # Fragments per group */
	__u32	s_inodes_per_group;	/* # Inodes per group */
	__u32	s_mtime;		/* Mount time */
	__u32	s_wtime;		/* Write time */
	__u16	s_mnt_count;		/* Mount count */
	__s16	s_max_mnt_count;	/* Maximal mount count */
	__u16	s_magic;		/* Magic signature */
	__u16	s_state;		/* File system state */
	__u16	s_errors;		/* Behaviour when detecting errors */
	__u16	s_minor_rev_level; 	/* minor revision level */
	__u32	s_lastcheck;		/* time of last check */
	__u32	s_checkinterval;	/* max. time between checks */
	__u32	s_creator_os;		/* OS */
	__u32	s_rev_level;		/* Revision level */
	__u16	s_def_resuid;		/* Default uid for reserved blocks */
	__u16	s_def_resgid;		/* Default gid for reserved blocks */
	/*
	 * These fields are for EXT2_DYNAMIC_REV superblocks only.
	 *
	 * Note: the difference between the compatible feature set and
	 * the incompatible feature set is that if there is a bit set
	 * in the incompatible feature set that the kernel doesn't
	 * know about, it should refuse to mount the filesystem.
	 * 
	 * e2fsck's requirements are more strict; if it doesn't know
	 * about a feature in either the compatible or incompatible
	 * feature set, it must abort and not try to meddle with
	 * things it doesn't understand...
	 */
	__u32	s_first_ino; 		/* First non-reserved inode */
	__u16   s_inode_size; 		/* size of inode structure */
	__u16	s_block_group_nr; 	/* block group # of this superblock */
	__u32	s_feature_compat; 	/* compatible feature set */
	__u32	s_feature_incompat; 	/* incompatible feature set */
	__u32	s_feature_ro_compat; 	/* readonly-compatible feature set */
	__u8	s_uuid[16];		/* 128-bit uuid for volume */
	char	s_volume_name[16]; 	/* volume name */
	char	s_last_mounted[64]; 	/* directory where last mounted */
	__u32	s_reserved[206];	/* Padding to the end of the block */
};

#ifndef EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#endif

static void print_user (unsigned short uid)
{
	struct passwd *pw;

	printf ("%u ", uid);
	pw = getpwuid (uid);
	if (pw == NULL)
		printf ("(user unknown)\n");
	else
		printf ("(user %s)\n", pw->pw_name);
}

static void print_group (unsigned short gid)
{
	struct group *gr;

	printf ("%u ", gid);
	gr = getgrgid (gid);
	if (gr == NULL)
		printf ("(group unknown)\n");
	else
		printf ("(group %s)\n", gr->gr_name);
}

#define MONTH_INT (86400 * 30)
#define WEEK_INT (86400 * 7)
#define DAY_INT	(86400)
#define HOUR_INT (60 * 60)
#define MINUTE_INT (60)

static const char *interval_string(unsigned int secs)
{
	static char buf[256], tmp[80];
	int		hr, min, num;

	buf[0] = 0;

	if (secs == 0)
		return "<none>";

	if (secs >= MONTH_INT) {
		num = secs / MONTH_INT;
		secs -= num*MONTH_INT;
		sprintf(buf, "%d month%s", num, (num>1) ? "s" : "");
	}
	if (secs >= WEEK_INT) {
		num = secs / WEEK_INT;
		secs -= num*WEEK_INT;
		sprintf(tmp, "%s%d week%s", buf[0] ? ", " : "",
			num, (num>1) ? "s" : "");
		strcat(buf, tmp);
	}
	if (secs >= DAY_INT) {
		num = secs / DAY_INT;
		secs -= num*DAY_INT;
		sprintf(tmp, "%s%d day%s", buf[0] ? ", " : "",
			num, (num>1) ? "s" : "");
		strcat(buf, tmp);
	}
	if (secs > 0) {
		hr = secs / HOUR_INT;
		secs -= hr*HOUR_INT;
		min = secs / MINUTE_INT;
		secs -= min*MINUTE_INT;
		sprintf(tmp, "%s%d:%02d:%02d", buf[0] ? ", " : "",
			hr, min, secs);
		strcat(buf, tmp);
	}
	return buf;
}


#ifndef EXT2_INODE_SIZE
#define EXT2_INODE_SIZE(s) sizeof(struct ext2_inode)
#endif

#ifndef EXT2_GOOD_OLD_REV
#define EXT2_GOOD_OLD_REV 0
#endif

void list_super (struct ext2_super_block * s)
{
	int inode_blocks_per_group;
	struct ext2fs_sb *sb = (struct ext2fs_sb *) s;
	char buf[80];
	const char *os;
	time_t	tm;

	inode_blocks_per_group = (((s->s_inodes_per_group *
				    EXT2_INODE_SIZE(s)) +
				   EXT2_BLOCK_SIZE(s) - 1) /
				  EXT2_BLOCK_SIZE(s));
	if (sb->s_volume_name[0]) {
		memset(buf, 0, sizeof(buf));
		strncpy(buf, sb->s_volume_name, sizeof(sb->s_volume_name));
	} else
		strcpy(buf, "<none>");
	printf("Filesystem volume name:   %s\n", buf);
	if (sb->s_last_mounted[0]) {
		memset(buf, 0, sizeof(buf));
		strncpy(buf, sb->s_last_mounted, sizeof(sb->s_last_mounted));
	} else
		strcpy(buf, "<not available>");
	printf("Last mounted on:          %s\n", buf);
	if (!e2p_is_null_uuid(sb->s_uuid)) {
		e2p_uuid_to_str(sb->s_uuid, buf);
	} else
		strcpy(buf, "<none>");
	printf("Filesystem UUID:          %s\n", buf);
	printf ("Filesystem magic number:  0x%04X\n", s->s_magic);
	printf ("Filesystem revision #:    %d", s->s_rev_level);
	if (s->s_rev_level == EXT2_GOOD_OLD_REV) {
		printf(" (original)\n");
#ifdef EXT2_DYNAMIC_REV
	} else if (s->s_rev_level == EXT2_DYNAMIC_REV) {
		printf(" (dynamic)\n");
#endif
	} else
		printf("\n");
#ifdef EXT2_DYNAMIC_REV
	printf ("Filesystem features:      ");
	if (s->s_feature_ro_compat & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)
		printf("sparse_super");
	else
		printf("(none)");
	printf("\n");
#endif
	printf ("Filesystem state:        ");
	print_fs_state (stdout, s->s_state);
	printf ("\n");
	printf ("Errors behavior:          ");
	print_fs_errors (stdout, s->s_errors);
	printf ("\n");
	switch (s->s_creator_os) {
	    case EXT2_OS_LINUX: os = "Linux"; break;
	    case EXT2_OS_HURD:  os = "GNU"; break;
	    case EXT2_OS_MASIX: os = "Masix"; break;
	    default:		os = "unknown"; break;
	}
	printf("Filesystem OS type:       %s\n", os);
	printf("Inode count:              %u\n", s->s_inodes_count);
	printf("Block count:              %u\n", s->s_blocks_count);
	printf("Reserved block count:     %u\n", s->s_r_blocks_count);
	printf("Free blocks:              %u\n", s->s_free_blocks_count);
	printf("Free inodes:              %u\n", s->s_free_inodes_count);
	printf("First block:              %u\n", s->s_first_data_block);
	printf("Block size:               %u\n", EXT2_BLOCK_SIZE(s));
	printf("Fragment size:            %u\n", EXT2_FRAG_SIZE(s));
	printf("Blocks per group:         %u\n", s->s_blocks_per_group);
	printf("Fragments per group:      %u\n", s->s_frags_per_group);
	printf("Inodes per group:         %u\n", s->s_inodes_per_group);
	printf("Inode blocks per group:   %u\n", inode_blocks_per_group);
	tm = s->s_mtime;
	printf("Last mount time:          %s", ctime(&tm));
	tm = s->s_wtime;
	printf("Last write time:          %s", ctime(&tm));
	printf("Mount count:              %u\n", s->s_mnt_count);
	printf("Maximum mount count:      %d\n", s->s_max_mnt_count);
	tm = s->s_lastcheck;
	printf("Last checked:             %s", ctime(&tm));
	printf("Check interval:           %u (%s)\n", s->s_checkinterval,
	       interval_string(s->s_checkinterval));
	if (s->s_checkinterval)
	{
		time_t next;

		next = s->s_lastcheck + s->s_checkinterval;
		printf("Next check after:         %s", ctime(&next));
	}
#ifdef	EXT2_DEF_RESUID
	printf("Reserved blocks uid:      ");
	print_user(s->s_def_resuid);
	printf("Reserved blocks gid:      ");
	print_group(s->s_def_resgid);
#endif
#ifdef EXT2_DYNAMIC_REV
	if (s->s_rev_level >= EXT2_DYNAMIC_REV) {
		printf("First inode:              %d\n", s->s_first_ino);
		printf("Inode size:		  %d\n", s->s_inode_size);
	}
#endif
}




