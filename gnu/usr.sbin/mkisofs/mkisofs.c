/*	$OpenBSD: mkisofs.c,v 1.2 1998/04/05 00:39:35 deraadt Exp $	*/
/*
 * Program mkisofs.c - generate iso9660 filesystem  based upon directory
 * tree on hard disk.

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

static char rcsid[] ="$From: mkisofs.c,v 1.9 1997/04/10 02:45:09 eric Rel $";

/* ADD_FILES changes made by Ross Biro biro@yggdrasil.com 2/23/95 */

#include <errno.h>
#include "mkisofs.h"
#include "config.h"

#ifdef linux
#include <getopt.h>
#endif

#include "iso9660.h"
#include <ctype.h>

#ifndef VMS
#include <time.h>
#else
#include <sys/time.h>
#include "vms.h"
#endif

#include <stdlib.h>
#include <sys/stat.h>

#ifndef VMS
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#include "exclude.h"

#ifdef __NetBSD__
#include <sys/time.h>
#include <sys/resource.h>
#endif

struct directory * root = NULL;

static char version_string[] = "mkisofs v1.11.2";

FILE * discimage;
unsigned int next_extent = 0;
unsigned int last_extent = 0;
unsigned int session_start = 0;
unsigned int path_table_size = 0;
unsigned int path_table[4] = {0,};
unsigned int path_blocks = 0;
struct iso_directory_record root_record;
char * extension_record = NULL;
int extension_record_extent = 0;
static  int extension_record_size = 0;

/* These variables are associated with command line options */
int use_eltorito = 0;
int use_RockRidge = 0;
int verbose = 0;
int all_files  = 0;
int follow_links = 0;
int rationalize = 0;
int generate_tables = 0;
char * preparer = PREPARER_DEFAULT;
char * publisher = PUBLISHER_DEFAULT;
char * appid = APPID_DEFAULT;
char * copyright = COPYRIGHT_DEFAULT;
char * biblio = BIBLIO_DEFAULT;
char * abstract = ABSTRACT_DEFAULT;
char * volset_id = VOLSET_ID_DEFAULT;
char * volume_id = VOLUME_ID_DEFAULT;
char * system_id = SYSTEM_ID_DEFAULT;
char * boot_catalog = BOOT_CATALOG_DEFAULT;
char * boot_image = BOOT_IMAGE_DEFAULT;

int omit_period = 0;             /* Violates iso9660, but these are a pain */
int transparent_compression = 0; /* So far only works with linux */
int omit_version_number = 0;     /* May violate iso9660, but noone uses vers*/
int RR_relocation_depth = 6;     /* Violates iso9660, but most systems work */
int full_iso9660_filenames = 0;  /* Used with Amiga.  Disc will not work with
				  DOS */
int allow_leading_dots = 0;	 /* DOS cannot read names with leading dots */

struct rcopts{
  char * tag;
  char ** variable;
};

struct rcopts rcopt[] = {
  {"PREP", &preparer},
  {"PUBL", &publisher},
  {"APPI", &appid},
  {"COPY", &copyright},
  {"BIBL", &biblio},
  {"ABST", &abstract},
  {"VOLS", &volset_id},
  {"VOLI", &volume_id},
  {"SYSI", &system_id},
  {NULL, NULL}
};

#if defined(ultrix) || defined(_AUX_SOURCE)
char *strdup(s)
char *s;{char *c;if(c=(char *)malloc(strlen(s)+1))strcpy(c,s);return c;}
#endif

void FDECL1(read_rcfile, char *, appname)
{
  FILE * rcfile;
  struct rcopts * rco;
  char * pnt, *pnt1;
  char linebuffer[256];
  static char rcfn[] = ".mkisofsrc";
  char filename[1000];
  int linum;

  strcpy(filename, rcfn);
  rcfile = fopen(filename, "r");
  if (!rcfile && errno != ENOENT)
    perror(filename);

  if (!rcfile)
    {
      pnt = getenv("MKISOFSRC");
      if (pnt && strlen(pnt) <= sizeof(filename))
	{
	  strcpy(filename, pnt);
	  rcfile = fopen(filename, "r");
	  if (!rcfile && errno != ENOENT)
	    perror(filename);
	}
    }

  if (!rcfile)
    {
      pnt = getenv("HOME");
      if (pnt && strlen(pnt) + strlen(rcfn) + 2 <= sizeof(filename))
	{
	  strcpy(filename, pnt);
	  strcat(filename, "/");
	  strcat(filename, rcfn);
	  rcfile = fopen(filename, "r");
	  if (!rcfile && errno != ENOENT)
	    perror(filename);
	}
    }
  if (!rcfile && strlen(appname)+sizeof(rcfn)+2 <= sizeof(filename))
    {
      strcpy(filename, appname);
      pnt = strrchr(filename, '/');
      if (pnt)
	{
	  strcpy(pnt + 1, rcfn);
	  rcfile = fopen(filename, "r");
	  if (!rcfile && errno != ENOENT)
	    perror(filename);
	}
    }
  if (!rcfile)
    return;
  fprintf(stderr, "Using \"%s\"\n", filename);
  /* OK, we got it.  Now read in the lines and parse them */
  linum = 0;
  while (fgets(linebuffer, sizeof(linebuffer), rcfile))
    {
      char *name;
      char *name_end;
      ++linum;
      /* skip any leading white space */
	pnt = linebuffer;
      while (*pnt == ' ' || *pnt == '\t')
	++pnt;
      /* If we are looking at a # character, this line is a comment. */
	if (*pnt == '#')
	  continue;
      /* The name should begin in the left margin.  Make sure it is in
	 upper case.  Stop when we see white space or a comment. */
	name = pnt;
      while (*pnt && isalpha(*pnt))
	{
	  if(islower(*pnt))
	    *pnt = toupper(*pnt);
	  pnt++;
	}
      if (name == pnt)
	{
	  fprintf(stderr, "%s:%d: name required\n", filename, linum);
	  continue;
	}
      name_end = pnt;
      /* Skip past white space after the name */
      while (*pnt == ' ' || *pnt == '\t')
	pnt++;
      /* silently ignore errors in the rc file. */
      if (*pnt != '=')
	{
	  fprintf(stderr, "%s:%d: equals sign required\n", filename, linum);
	  continue;
	}
      /* Skip pas the = sign, and any white space following it */
      pnt++; /* Skip past '=' sign */
      while (*pnt == ' ' || *pnt == '\t')
	pnt++;

      /* now it is safe to NUL terminate the name */

	*name_end = 0;

      /* Now get rid of trailing newline */

      pnt1 = pnt;
      while (*pnt1)
	{
	  if (*pnt1 == '\n')
	    {
	      *pnt1 = 0;
	      break;
	    }
	  pnt1++;
	};
      /* OK, now figure out which option we have */
      for(rco = rcopt; rco->tag; rco++) {
	if(strcmp(rco->tag, name) == 0)
	  {
	    *rco->variable = strdup(pnt);
	    break;
	  };
      }
      if (rco->tag == NULL)
	{
	  fprintf(stderr, "%s:%d: field name \"%s\" unknown\n", filename, linum,
		  name);
	}
     }
  if (ferror(rcfile))
    perror(filename);
  fclose(rcfile);
}

char * path_table_l = NULL;
char * path_table_m = NULL;
int goof = 0;

void usage(){
	fprintf(stderr,"Usage:\n");
	fprintf(stderr,
"mkisofs [-o outfile] [-R] [-V volid] [-v] [-a] \
[-T]\n [-l] [-d] [-V] [-D] [-L] [-p preparer]"
#ifdef ADD_FILES
"[-i file] \n"
#endif
"[-P publisher] [ -A app_id ] [-z] \n \
[-b boot_image_name] [-c boot_catalog-name] \
[-x path -x path ...] path\n");
	exit(1);
}


/* 
 * Fill in date in the iso9660 format 
 *
 * The standards  state that the timezone offset is in multiples of 15
 * minutes, and is what you add to GMT to get the localtime.  The U.S.
 * is always at a negative offset, from -5h to -8h (can vary a little
 * with DST,  I guess).  The Linux iso9660 filesystem has had the sign
 * of this wrong for ages (mkisofs had it wrong too for the longest time).
 */
int FDECL2(iso9660_date,char *, result, time_t, ctime){
  struct tm *local;
  local = localtime(&ctime);
  result[0] = local->tm_year;
  result[1] = local->tm_mon + 1;
  result[2] = local->tm_mday;
  result[3] = local->tm_hour;
  result[4] = local->tm_min;
  result[5] = local->tm_sec;

  /* 
   * Must recalculate proper timezone offset each time,
   * as some files use daylight savings time and some don't... 
   */
  result[6] = local->tm_yday;	/* save yday 'cause gmtime zaps it */
  local = gmtime(&ctime);
  local->tm_year -= result[0];
  local->tm_yday -= result[6];
  local->tm_hour -= result[3];
  local->tm_min -= result[4];
  if (local->tm_year < 0) 
    {
      local->tm_yday = -1;
    }
  else 
    {
      if (local->tm_year > 0) local->tm_yday = 1;
    }

  result[6] = -(local->tm_min + 60*(local->tm_hour + 24*local->tm_yday)) / 15;

  return 0;
}


extern char * cdwrite_data;

int FDECL2(main, int, argc, char **, argv){
  char * outfile;
  struct directory_entry de;
  unsigned long mem_start;
  struct stat statbuf;
  char * scan_tree;
  char * merge_image = NULL;
  struct iso_directory_record * mrootp = NULL;
  int c;
#ifdef ADD_FILES
  char *add_file_file = NULL;
#endif

  if (argc < 2)
    usage();

  /* Get the defaults from the .mkisofsrc file */
  read_rcfile(argv[0]);

  outfile = NULL;
  while ((c = getopt(argc, argv, "i:o:V:RrfvaTp:P:b:c:x:dDlLNzA:M:m:C:")) != EOF)
    switch (c)
      {
      case 'C':
	/*
	 * This is a temporary hack until cdwrite gets the proper hooks in
	 * it.
	 */
	cdwrite_data = optarg;
	break;
      case 'a':
	all_files++;
	break;
      case 'b':
	use_eltorito++;
	boot_image = optarg;  /* pathname of the boot image on cd */
	if (boot_image == NULL) {
	        fprintf(stderr,"Required boot image pathname missing\n");
		exit(1);
	}
	break;
      case 'c':
	use_eltorito++;
	boot_catalog = optarg;  /* pathname of the boot image on cd */
	if (boot_catalog == NULL) {
	        fprintf(stderr,"Required boot catalog pathname missing\n");
		exit(1);
	}
	break;
      case 'A':
	appid = optarg;
	if(strlen(appid) > 128) {
		fprintf(stderr,"Application-id string too long\n");
		exit(1);
	};
	break;
      case 'd':
	omit_period++;
	break;
      case 'D':
	RR_relocation_depth = 32767;
	break;
      case 'f':
	follow_links++;
	break;
      case 'i':
#ifdef ADD_FILES
	add_file_file = optarg;
	break;
#else
	usage();
	exit(1);
#endif
      case 'l':
	full_iso9660_filenames++;
	break;
      case 'L':
        allow_leading_dots++;
        break;
      case 'M':
	merge_image = optarg;
	break;
      case 'N':
	omit_version_number++;
	break;
      case 'o':
	outfile = optarg;
	break;
      case 'p':
	preparer = optarg;
	if(strlen(preparer) > 128) {
		fprintf(stderr,"Preparer string too long\n");
		exit(1);
	};
	break;
      case 'P':
	publisher = optarg;
	if(strlen(publisher) > 128) {
		fprintf(stderr,"Publisher string too long\n");
		exit(1);
	};
	break;
      case 'R':
	use_RockRidge++;
	break;
      case 'r':
	rationalize++;
	use_RockRidge++;
	break;
      case 'T':
	generate_tables++;
	break;
      case 'V':
	volume_id = optarg;
	break;
      case 'v':
	verbose++;
	break;
      case 'z':
#ifdef VMS
	fprintf(stderr,"Transparent compression not supported with VMS\n");
	exit(1);
#else
	transparent_compression++;
#endif
	break;
      case 'm':
        add_match(optarg);
	break;
      case 'x':
        exclude(optarg);
	break;
      default:
	usage();
	exit(1);
      }
#ifdef __NetBSD__
    {
	int resource;
    struct rlimit rlp;
	if (getrlimit(RLIMIT_DATA,&rlp) == -1) 
		perror("Warning: getrlimit");
	else {
		rlp.rlim_cur=33554432;
		if (setrlimit(RLIMIT_DATA,&rlp) == -1)
			perror("Warning: setrlimit");
		}
	}
#endif
#ifdef HAVE_SBRK
  mem_start = (unsigned long) sbrk(0);
#endif

  if(verbose) fprintf(stderr,"%s\n", version_string);

  if(     (cdwrite_data != NULL && merge_image == NULL)
       || (cdwrite_data == NULL && merge_image != NULL) )
    {
      fprintf(stderr,"Multisession usage bug - both -C and -M must be specified.\n");
      exit(0);
    }

  /*  The first step is to scan the directory tree, and take some notes */

  scan_tree = argv[optind];

#ifdef ADD_FILES
  if (add_file_file) {
    add_file(add_file_file);
  }
  add_file_list (argc, argv, optind+1);
#endif

  if(!scan_tree){
	  usage();
	  exit(1);
  };

#ifndef VMS
  if(scan_tree[strlen(scan_tree)-1] != '/') {
    scan_tree = (char *) e_malloc(strlen(argv[optind])+2);
    strcpy(scan_tree, argv[optind]);
    strcat(scan_tree, "/");
  };
#endif

  if(use_RockRidge){
#if 1
	extension_record = generate_rr_extension_record("RRIP_1991A",
				       "THE ROCK RIDGE INTERCHANGE PROTOCOL PROVIDES SUPPORT FOR POSIX FILE SYSTEM SEMANTICS",
				       "PLEASE CONTACT DISC PUBLISHER FOR SPECIFICATION SOURCE.  SEE PUBLISHER IDENTIFIER IN PRIMARY VOLUME DESCRIPTOR FOR CONTACT INFORMATION.", &extension_record_size);
#else
	extension_record = generate_rr_extension_record("IEEE_P1282",
				       "THE IEEE P1282 PROTOCOL PROVIDES SUPPORT FOR POSIX FILE SYSTEM SEMANTICS",
				       "PLEASE CONTACT THE IEEE STANDARDS DEPARTMENT, PISCATAWAY, NJ, USA FOR THE P1282 SPECIFICATION.", &extension_record_size);
#endif
  }

  /*
   * See if boot catalog file exists in root directory, if not
   * we will create it.
   */
  if (use_eltorito)
    init_boot_catalog(argv[optind]);

  /*
   * Find the device and inode number of the root directory.
   * Record this in the hash table so we don't scan it more than
   * once.
   */
  stat_filter(argv[optind], &statbuf);
  add_directory_hash(statbuf.st_dev, STAT_INODE(statbuf));

  memset(&de, 0, sizeof(de));

  de.filedir = root;  /* We need this to bootstrap */

  if( merge_image != NULL )
    {
      mrootp = merge_isofs(merge_image);
      if( mrootp == NULL )
	{
	  /*
	   * Complain and die.
	   */
	  fprintf(stderr,"Unable to open previous session image %s\n",
		  merge_image);
	  exit(1);
	}

      memcpy(&de.isorec.extent, mrootp->extent, 8);      
    }

  /*
   * Scan the actual directory (and any we find below it)
   * for files to write out to the output image.
   */
  if (!scan_directory_tree(argv[optind], &de, mrootp))
    {
      exit(1);
    }

  /*
   * Fix a couple of things in the root directory so that everything
   * is self consistent.
   */
  root->self = root->contents;  /* Fix this up so that the path tables get done right */

  if(reloc_dir) sort_n_finish(reloc_dir);

  if (goof) exit(1);
  
  /*
   * OK, ready to write the file.  Open it up, and generate the thing.
   */
  if (outfile){
	  discimage = fopen(outfile, "w");
	  if (!discimage){
		  fprintf(stderr,"Unable to open disc image file\n");
		  exit(1);

	  };
  } else
	  discimage =  stdout;

  /* Now assign addresses on the disc for the path table. */

  path_blocks = (path_table_size + (SECTOR_SIZE - 1)) >> 11;
  if (path_blocks & 1) path_blocks++;

  path_table[0] = session_start + 0x10 + 2 + (use_eltorito ? 1 : 0);
  path_table[1] = 0;
  path_table[2] = path_table[0] + path_blocks;
  path_table[3] = 0;

  last_extent += path_table[2] - session_start + path_blocks;  
		/* The next free block */

  /* The next step is to go through the directory tree and assign extent
     numbers for all of the directories */

  assign_directory_addresses(root);

  if(extension_record) {
	  struct directory_entry * s_entry;
	  extension_record_extent = last_extent++;
	  s_entry = root->contents;
	  set_733((char *) s_entry->rr_attributes + s_entry->rr_attr_size - 24,
		  extension_record_extent);
	  set_733((char *) s_entry->rr_attributes + s_entry->rr_attr_size - 8,
		  extension_record_size);
  };

  if (use_RockRidge && reloc_dir)
	  finish_cl_pl_entries();

  /* Now we generate the path tables that are used by DOS to improve directory
     access times. */
  generate_path_tables();

  /* Generate root record for volume descriptor. */
  generate_root_record();

  if (verbose)
    dump_tree(root);

  if( in_image != NULL )
    {
      fclose(in_image);
    }

  iso_write(discimage);

#ifdef HAVE_SBRK
  fprintf(stderr,"Max brk space used %x\n", 
	  (unsigned int)(((unsigned long)sbrk(0)) - mem_start));
#endif
  fprintf(stderr,"%d extents written (%d Mb)\n", last_extent, last_extent >> 9);
#ifdef VMS
  return 1;
#else
  return 0;
#endif
}

void *
FDECL1(e_malloc, size_t, size)
{
void* pt = 0;
	if( (size > 0) && ((pt=malloc(size))==NULL) ) {
		fprintf(stderr, "Not enough memory\n");
		exit (1);
		}
return pt;
}
