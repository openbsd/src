/*	$OpenBSD: mkisofs.h,v 1.2 1998/04/05 00:39:36 deraadt Exp $	*/
/*
 * Header file mkisofs.h - assorted structure definitions and typecasts.

   Written by Eric Youngdale (1993).

   Copyright 1993 Yggdrasil Computing, Incorporated

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * 	$From: mkisofs.h,v 1.4 1997/03/08 17:29:12 eric Rel $
 */

/* ADD_FILES changes made by Ross Biro biro@yggdrasil.com 2/23/95 */

#include <stdio.h>

/* This symbol is used to indicate that we do not have things like
   symlinks, devices, and so forth available.  Just files and dirs */

#ifdef VMS
#define NON_UNIXFS
#endif

#ifdef DJGPP
#define NON_UNIXFS
#endif

#ifdef VMS
#include <sys/dir.h>
#define dirent direct
#else
#include <dirent.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef linux
#include <sys/dir.h>
#endif

#ifdef ultrix
extern char *strdup();
#endif

#ifdef __STDC__
#define DECL(NAME,ARGS) NAME ARGS
#define FDECL1(NAME,TYPE0, ARG0) \
	NAME(TYPE0 ARG0)
#define FDECL2(NAME,TYPE0, ARG0,TYPE1, ARG1) \
	NAME(TYPE0 ARG0, TYPE1 ARG1)
#define FDECL3(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2) \
	NAME(TYPE0 ARG0, TYPE1 ARG1, TYPE2 ARG2)
#define FDECL4(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3) \
	NAME(TYPE0 ARG0, TYPE1 ARG1, TYPE2 ARG2, TYPE3 ARG3)
#define FDECL5(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3, TYPE4, ARG4) \
	NAME(TYPE0 ARG0, TYPE1 ARG1, TYPE2 ARG2, TYPE3 ARG3, TYPE4 ARG4)
#define FDECL6(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3, TYPE4, ARG4, TYPE5, ARG5) \
	NAME(TYPE0 ARG0, TYPE1 ARG1, TYPE2 ARG2, TYPE3 ARG3, TYPE4 ARG4, TYPE5 ARG5)
#else
#define DECL(NAME,ARGS) NAME()
#define FDECL1(NAME,TYPE0, ARG0) NAME(ARG0) TYPE0 ARG0;
#define FDECL2(NAME,TYPE0, ARG0,TYPE1, ARG1) NAME(ARG0, ARG1) TYPE0 ARG0; TYPE1 ARG1;
#define FDECL3(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2) \
	NAME(ARG0, ARG1, ARG2) TYPE0 ARG0; TYPE1 ARG1; TYPE2 ARG2;
#define FDECL4(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3) \
	NAME(ARG0, ARG1, ARG2, ARG3, ARG4) TYPE0 ARG0; TYPE1 ARG1; TYPE2 ARG2; TYPE3 ARG3;
#define FDECL5(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3, TYPE4, ARG4) \
	NAME(ARG0, ARG1, ARG2, ARG3, ARG4) TYPE0 ARG0; TYPE1 ARG1; TYPE2 ARG2; TYPE3 ARG3; TYPE4 ARG4;
#define FDECL6(NAME,TYPE0, ARG0,TYPE1, ARG1, TYPE2, ARG2, TYPE3, ARG3, TYPE4, ARG4, TYPE5, ARG5) \
	NAME(ARG0, ARG1, ARG2, ARG3, ARG4, ARG5) TYPE0 ARG0; TYPE1 ARG1; TYPE2 ARG2; TYPE3 ARG3; TYPE4 ARG4; TYPE5 ARG5;
#define const
#endif


#ifdef __svr4__
#include <stdlib.h>
#else
extern int optind;
extern char *optarg;
/* extern int getopt (int __argc, char **__argv, char *__optstring); */
#endif

#include "iso9660.h"
#include "defaults.h"

struct directory_entry{
  struct directory_entry * next;
  struct iso_directory_record isorec;
  unsigned int starting_block;
  unsigned int size;
  unsigned int priority;
  char * name;
  char * table;
  char * whole_name;
  struct directory * filedir;
  struct directory_entry * parent_rec;
  unsigned int de_flags;
  ino_t inode;  /* Used in the hash table */
  dev_t dev;  /* Used in the hash table */
  unsigned char * rr_attributes;
  unsigned int rr_attr_size;
  unsigned int total_rr_attr_size;
};

struct file_hash{
  struct file_hash * next;
  ino_t inode;  /* Used in the hash table */
  dev_t dev;  /* Used in the hash table */
  unsigned int starting_block;
  unsigned int size;
};
  
/* 
 * This structure describes one complete directory.  It has pointers
 * to other directories in the overall tree so that it is clear where
 * this directory lives in the tree, and it also must contain pointers
 * to the contents of the directory.  Note that subdirectories of this
 * directory exist twice in this stucture.  Once in the subdir chain,
 * and again in the contents chain.
 */
struct directory{
  struct directory * next;  /* Next directory at same level as this one */
  struct directory * subdir; /* First subdirectory in this directory */
  struct directory * parent;
  struct directory_entry * contents;
  struct directory_entry * self;
  char * whole_name;  /* Entire path */
  char * de_name;  /* Entire path */
  unsigned int ce_bytes;  /* Number of bytes of CE entries reqd for this dir */
  unsigned int depth;
  unsigned int size;
  unsigned int extent;
  unsigned short path_index;
};

struct deferred{
  struct deferred * next;
  unsigned int starting_block;
  char * name;
  struct directory * filedir;
  unsigned int flags;
};

extern int goof;
extern struct directory * root;
extern struct directory * reloc_dir;
extern unsigned int next_extent;
extern unsigned int last_extent;
extern unsigned int last_extent_written;
extern unsigned int session_start;
extern unsigned int path_table_size;
extern unsigned int path_table[4];
extern unsigned int path_blocks;
extern char * path_table_l;
extern char * path_table_m;
extern struct iso_directory_record root_record;

extern int use_eltorito;
extern int use_RockRidge;
extern int rationalize;
extern int follow_links;
extern int verbose;
extern int all_files;
extern int generate_tables;
extern int omit_period;
extern int omit_version_number;
extern int transparent_compression;
extern int RR_relocation_depth;
extern int full_iso9660_filenames;

/* tree.c */
extern int DECL(stat_filter, (char *, struct stat *));
extern void DECL(sort_n_finish,(struct directory *));
extern void finish_cl_pl_entries();
extern int DECL(scan_directory_tree,(char * path, 
				     struct directory_entry * self,
				     struct iso_directory_record *));
extern void DECL(generate_iso9660_directories,(struct directory *, FILE*));
extern void DECL(dump_tree,(struct directory * node));
extern struct directory_entry * DECL(search_tree_file, (struct 
				directory * node,char * filename));

/* eltorito.c */
extern void DECL(init_boot_catalog, (const char * path ));
extern void DECL(get_torito_desc, (struct eltorito_boot_descriptor * path ));

/* write.c */
extern void DECL(assign_directory_addresses,(struct directory * root));
extern int DECL(get_733,(char *));
extern int DECL(isonum_733,(unsigned char *));
extern void DECL(set_723,(char *, unsigned int));
extern void DECL(set_731,(char *, unsigned int));
extern void DECL(set_721,(char *, unsigned int));
extern void DECL(set_733,(char *, unsigned int));
extern void DECL(sort_directory,(struct directory_entry **));
extern void generate_root_record();
extern void DECL(generate_one_directory,(struct directory *, FILE*));
extern void generate_path_tables();
extern int DECL(iso_write,(FILE * outfile));
extern void DECL(memcpy_max, (char *, char *, int));


/* multi.c */

extern FILE * in_image;
extern struct iso_directory_record *
	DECL(merge_isofs,(char * path)); 

extern int DECL(free_mdinfo, (struct directory_entry **, int len));

extern struct directory_entry ** 
	DECL(read_merging_directory,(struct iso_directory_record *, int*));
extern void 
	DECL(merge_remaining_entries, (struct directory *, 
				       struct directory_entry **, int));

/* match.c */
extern int DECL(matches, (char *));
extern void DECL(add_match, (char *));

/* files.c */
struct dirent * DECL(readdir_add_files, (char **, char *, DIR *));

/* */

extern int DECL(iso9660_file_length,(const char* name, 
			       struct directory_entry * sresult, int flag));
extern int DECL(iso9660_date,(char *, time_t));
extern void DECL(add_hash,(struct directory_entry *));
extern struct file_hash * DECL(find_hash,(dev_t, ino_t));
extern void DECL(add_directory_hash,(dev_t, ino_t));
extern struct file_hash * DECL(find_directory_hash,(dev_t, ino_t));
extern void flush_file_hash();
extern int DECL(delete_file_hash,(struct directory_entry *));
extern struct directory_entry * DECL(find_file_hash,(char *));
extern void DECL(add_file_hash,(struct directory_entry *));
extern int DECL(generate_rock_ridge_attributes,(char *, char *,
					  struct directory_entry *, 
					  struct stat *, struct stat *,
					  int  deep_flag));
extern char * DECL(generate_rr_extension_record,(char * id,  char  * descriptor,
				    char * source, int  * size));

extern int    DECL(check_prev_session, (struct directory_entry **, int len,
				     struct directory_entry *,
				     struct stat *,
				     struct stat *,
				     struct directory_entry **));

#ifdef	USE_SCG
/* scsi.c */
extern	int	readsecs(int startsecno, void *buffer, int sectorcount);
extern	int	scsidev_open(char *path);
#endif

extern char * extension_record;
extern int extension_record_extent;
extern int n_data_extents;

/* These are a few goodies that can be specified on the command line, and  are
   filled into the root record */

extern char * preparer;
extern char * publisher;
extern char * copyright;
extern char * biblio;
extern char * abstract;
extern char * appid;
extern char * volset_id;
extern char * system_id;
extern char * volume_id;
extern char * boot_catalog;
extern char * boot_image;

extern void * DECL(e_malloc,(size_t));


#define SECTOR_SIZE (2048)
#define ROUND_UP(X)    ((X + (SECTOR_SIZE - 1)) & ~(SECTOR_SIZE - 1))

#define NEED_RE 1
#define NEED_PL  2
#define NEED_CL 4
#define NEED_CE 8
#define NEED_SP 16

#define PREV_SESS_DEV (sizeof(dev_t) >= 4 ? 0x7ffffffd : 0x7ffd)
#define TABLE_INODE (sizeof(ino_t) >= 4 ? 0x7ffffffe : 0x7ffe)
#define UNCACHED_INODE (sizeof(ino_t) >= 4 ? 0x7fffffff : 0x7fff)
#define UNCACHED_DEVICE (sizeof(dev_t) >= 4 ? 0x7fffffff : 0x7fff)

#ifdef VMS
#define STAT_INODE(X) (X.st_ino[0])
#define PATH_SEPARATOR ']'
#define SPATH_SEPARATOR ""
#else
#define STAT_INODE(X) (X.st_ino)
#define PATH_SEPARATOR '/'
#define SPATH_SEPARATOR "/"
#endif

/*
 * When using multi-session, indicates that we can reuse the
 * TRANS.TBL information for this directory entry.  If this flag
 * is set for all entries in a directory, it means we can just
 * reuse the TRANS.TBL and not generate a new one.
 */
#define SAFE_TO_REUSE_TABLE_ENTRY  1
/*
 * Volume sequence number to use in all of the iso directory records.
 */
#define DEF_VSN		1

/*
 * Make sure we have a definition for this.  If not, take a very conservative
 * guess.  From what I can tell SunOS is the only one with this trouble.
 */
#ifndef NAME_MAX
#ifdef FILENAME_MAX
#define NAME_MAX	FILENAME_MAX
#else
#define NAME_MAX	128
#endif
#endif
