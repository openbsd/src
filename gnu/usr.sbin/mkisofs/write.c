/*	$OpenBSD: write.c,v 1.2 1998/04/05 00:39:42 deraadt Exp $	*/
/*
 * Program write.c - dump memory  structures to  file for iso9660 filesystem.

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

static char rcsid[] ="$From: write.c,v 1.4 1997/04/10 03:33:25 eric Rel $";

#include <string.h>
#include <stdlib.h>
#include "mkisofs.h"
#include "iso9660.h"
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
 
#ifdef __svr4__
extern char * strdup(const char *);
#endif

#ifdef VMS
extern char * strdup(const char *);
#endif


/* Max number of sectors we will write at  one time */
#define NSECT 16

/* Counters for statistics */

static int table_size = 0;
static int total_dir_size = 0;
static int rockridge_size = 0;
static struct directory ** pathlist;
static next_path_index = 1;

/* Used to fill in some  of the information in the volume descriptor. */
static struct tm local;
static struct tm gmt;
 
/* Routines to actually write the disc.  We write sequentially so that
   we could write a tape, or write the disc directly */


#define FILL_SPACE(X)   memset(vol_desc.X, ' ', sizeof(vol_desc.X))

void FDECL2(set_721, char *, pnt, unsigned int, i)
{
     pnt[0] = i & 0xff;
     pnt[1] = (i >> 8) &  0xff;
}

void FDECL2(set_722, char *, pnt, unsigned int, i)
{
     pnt[0] = (i >> 8) &  0xff;
     pnt[1] = i & 0xff;
}

void FDECL2(set_723, char *, pnt, unsigned int, i)
{
     pnt[3] = pnt[0] = i & 0xff;
     pnt[2] = pnt[1] = (i >> 8) &  0xff;
}

void FDECL2(set_731, char *, pnt, unsigned int, i)
{
     pnt[0] = i & 0xff;
     pnt[1] = (i >> 8) &  0xff;
     pnt[2] = (i >> 16) &  0xff;
     pnt[3] = (i >> 24) &  0xff;
}

void FDECL2(set_732, char *, pnt, unsigned int, i)
{
     pnt[3] = i & 0xff;
     pnt[2] = (i >> 8) &  0xff;
     pnt[1] = (i >> 16) &  0xff;
     pnt[0] = (i >> 24) &  0xff;
}

int FDECL1(get_733, char *, p)
{
     return ((p[0] & 0xff)
	     | ((p[1] & 0xff) << 8)
	     | ((p[2] & 0xff) << 16)
	     | ((p[3] & 0xff) << 24));
}

void FDECL2(set_733, char *, pnt, unsigned int, i)
{
     pnt[7] = pnt[0] = i & 0xff;
     pnt[6] = pnt[1] = (i >> 8) &  0xff;
     pnt[5] = pnt[2] = (i >> 16) &  0xff;
     pnt[4] = pnt[3] = (i >> 24) &  0xff;
}

void FDECL4(xfwrite, void *, buffer, int, count, int, size, FILE *, file)
{
     while(count) 
     {
	  int got = fwrite(buffer,size,count,file);

	  if(got<=0) 
	  {
	       fprintf(stderr,"cannot fwrite %d*%d\n",size,count);
	       exit(1);
	  }
	  count-=got,*(char**)&buffer+=size*got;
     }
}

struct deferred_write
{
  struct deferred_write * next;
  char			* table;
  unsigned int		  extent;
  unsigned int		  size;
  char			* name;
};

static struct deferred_write * dw_head = NULL, * dw_tail = NULL;

static struct directory_entry * sort_dir;
static struct eltorito_boot_descriptor boot_desc;

unsigned int last_extent_written  =0;
static struct iso_primary_descriptor vol_desc;
static path_table_index;
static time_t begun;

/* We recursively walk through all of the directories and assign extent
   numbers to them.  We have already assigned extent numbers to everything that
   goes in front of them */

void FDECL1(assign_directory_addresses, struct directory *, node)
{
     int		dir_size;
     struct directory * dpnt;

     dpnt = node;
     
     while (dpnt)
     {
	  /*
	   * If we already have an extent for this (i.e. it came from
	   * a multisession disc), then don't reassign a new extent.
	   */
	  dpnt->path_index = next_path_index++;
	  if( dpnt->extent == 0 )
	  {
	       dpnt->extent = last_extent;
	       dir_size = (dpnt->size + (SECTOR_SIZE - 1)) >> 11;
	       
	       last_extent += dir_size;
	       
	       /* 
		* Leave room for the CE entries for this directory.  Keep them
		* close to the reference directory so that access will be 
		* quick. 
		*/
	       if(dpnt->ce_bytes)
	       {
		    last_extent += ROUND_UP(dpnt->ce_bytes) >> 11;
	       }
	  }

	  if(dpnt->subdir) 
	  {
	       assign_directory_addresses(dpnt->subdir);
	  }

	  dpnt = dpnt->next;
     }
}

static void FDECL3(write_one_file, char *, filename, 
		   unsigned int, size, FILE *, outfile)
{
     char		  buffer[SECTOR_SIZE * NSECT];
     FILE		* infile;
     int		  remain;
     int		  use;


     if ((infile = fopen(filename, "rb")) == NULL) 
     {
#if defined(sun) || defined(_AUX_SOURCE)
	  fprintf(stderr, "cannot open %s: (%d)\n", filename, errno);
#else
	  fprintf(stderr, "cannot open %s: %s\n", filename, strerror(errno));
#endif
	  exit(1);
     }
     remain = size;

     while(remain > 0)
     {
	  use =  (remain >  SECTOR_SIZE * NSECT - 1 ? NSECT*SECTOR_SIZE : remain);
	  use = ROUND_UP(use); /* Round up to nearest sector boundary */
	  memset(buffer, 0, use);
	  if (fread(buffer, 1, use, infile) == 0) 
	  {
	       fprintf(stderr,"cannot read from %s\n",filename); 
	       exit(1);
	  }
	  xfwrite(buffer, 1, use, outfile);
	  last_extent_written += use/SECTOR_SIZE;
#if 0
	  if((last_extent_written % 1000) < use/SECTOR_SIZE) 
	  {
	       fprintf(stderr,"%d..", last_extent_written);
	  }
#else
	  if((last_extent_written % 5000) < use/SECTOR_SIZE)
	  {
	       time_t now;
	       time_t the_end;
	       double frac;
	       
	       time(&now);
	       frac = last_extent_written / (double)last_extent;
	       the_end = begun + (now - begun) / frac;
	       fprintf(stderr, "%6.2f%% done, estimate finish %s",
		       frac * 100., ctime(&the_end));
	  }
#endif
	  remain -= use;
     }
     fclose(infile);
} /* write_one_file(... */

static void FDECL1(write_files, FILE *, outfile)
{
     struct deferred_write * dwpnt, *dwnext;
     dwpnt = dw_head;
     while(dwpnt)
     {
	  if(dwpnt->table) 
	  {
	       xfwrite(dwpnt->table,  1, ROUND_UP(dwpnt->size), outfile);
	       last_extent_written += ROUND_UP(dwpnt->size) / SECTOR_SIZE;
	       table_size += dwpnt->size;
/*		  fprintf(stderr,"Size %d ", dwpnt->size); */
	       free(dwpnt->table);
	  } 
	  else 
	  {

#ifdef VMS
	       vms_write_one_file(dwpnt->name, dwpnt->size, outfile);
#else
	       write_one_file(dwpnt->name, dwpnt->size, outfile);
#endif
	       free(dwpnt->name);
	  }

	  dwnext = dwpnt;
	  dwpnt = dwpnt->next;
	  free(dwnext);
     }
} /* write_files(... */

#if 0
static void dump_filelist()
{
     struct deferred_write * dwpnt;
     dwpnt = dw_head;
     while(dwpnt)
     {
	  fprintf(stderr, "File %s\n",dwpnt->name);
	  dwpnt = dwpnt->next;
     }
     fprintf(stderr,"\n");
}
#endif

int FDECL2(compare_dirs, const void *, rr, const void *, ll) 
{
     char * rpnt, *lpnt;
     struct directory_entry ** r, **l;
     
     r = (struct directory_entry **) rr;
     l = (struct directory_entry **) ll;
     rpnt = (*r)->isorec.name;
     lpnt = (*l)->isorec.name;
     
     /*
      *  Put the '.' and '..' entries on the head of the sorted list.
      *  For normal ASCII, this always happens to be the case, but out of
      *  band characters cause this not to be the case sometimes.
      */
     if( strcmp(rpnt, ".") == 0 ) return -1;
     if( strcmp(lpnt, ".") == 0 ) return  1;

     if( strcmp(rpnt, "..") == 0 ) return -1;
     if( strcmp(lpnt, "..") == 0 ) return  1;

     while(*rpnt && *lpnt) 
     {
	  if(*rpnt == ';' && *lpnt != ';') return -1;
	  if(*rpnt != ';' && *lpnt == ';') return 1;
	  
	  if(*rpnt == ';' && *lpnt == ';') return 0;
	  
	  if(*rpnt == '.' && *lpnt != '.') return -1;
	  if(*rpnt != '.' && *lpnt == '.') return 1;
	  
	  if((unsigned char)*rpnt < (unsigned char)*lpnt) return -1;
	  if((unsigned char)*rpnt > (unsigned char)*lpnt) return 1;
	  rpnt++;  lpnt++;
     }
     if(*rpnt) return 1;
     if(*lpnt) return -1;
     return 0;
}

void FDECL1(sort_directory, struct directory_entry **, sort_dir)
{
     int dcount = 0;
     int i, len;
     struct directory_entry * s_entry;
     struct directory_entry ** sortlist;
     
     s_entry = *sort_dir;
     while(s_entry)
     {
	  dcount++;
	  s_entry = s_entry->next;
     }

     /*
      * OK, now we know how many there are.  Build a vector for sorting. 
      */
     sortlist =   (struct directory_entry **) 
	  e_malloc(sizeof(struct directory_entry *) * dcount);

     dcount = 0;
     s_entry = *sort_dir;
     while(s_entry)
     {
	  sortlist[dcount] = s_entry;
	  len = s_entry->isorec.name_len[0];
	  s_entry->isorec.name[len] = 0;
	  dcount++;
	  s_entry = s_entry->next;
     }
  
     qsort(sortlist, dcount, sizeof(struct directory_entry *), 
	   (int (*)(const void *, const void *))compare_dirs);
     
     /* 
      * Now reassemble the linked list in the proper sorted order 
      */
     for(i=0; i<dcount-1; i++)
     {
	  sortlist[i]->next = sortlist[i+1];
     }

     sortlist[dcount-1]->next = NULL;
     *sort_dir = sortlist[0];
     
     free(sortlist);
     
}

void generate_root_record()
{
     time_t ctime;
     
     time (&ctime);

     local = *localtime(&ctime);
     gmt   = *gmtime(&ctime);
     
     root_record.length[0] = 1 + sizeof(struct iso_directory_record)
	  - sizeof(root_record.name);
     root_record.ext_attr_length[0] = 0;
     set_733((char *) root_record.extent, root->extent);
     set_733((char *) root_record.size, ROUND_UP(root->size));
     iso9660_date(root_record.date, ctime);
     root_record.flags[0] = 2;
     root_record.file_unit_size[0] = 0;
     root_record.interleave[0] = 0;
     set_723(root_record.volume_sequence_number, DEF_VSN);
     root_record.name_len[0] = 1;
}

static void FDECL1(assign_file_addresses, struct directory *, dpnt)
{
     struct directory * finddir;
     struct directory_entry * s_entry;
     struct file_hash *s_hash;
     struct deferred_write * dwpnt;
     char whole_path[1024];
     
     while (dpnt)
     {
	  s_entry = dpnt->contents;
	  for(s_entry = dpnt->contents; s_entry; s_entry = s_entry->next)
	  {
	       
	       /*
		* If we already have an  extent for this entry,
		* then don't assign a new one.  It must have come
		* from a previous session on the disc.  Note that
		* we don't end up scheduling the thing for writing
		* either.
		*/
	       if( isonum_733((unsigned char *) s_entry->isorec.extent) )
	       {
		    continue;
	       }
	       
	       /* 
		* This saves some space if there are symlinks present 
		*/
	       s_hash = find_hash(s_entry->dev, s_entry->inode);
	       if(s_hash)
	       {
		    if(verbose)
		    {
			 fprintf(stderr, "Cache hit for %s%s%s\n",s_entry->filedir->de_name, 
				 SPATH_SEPARATOR, s_entry->name);
		    }
		    set_733((char *) s_entry->isorec.extent, s_hash->starting_block);
		    set_733((char *) s_entry->isorec.size, s_hash->size);
		    continue;
	       }

	       /*
		* If this is for a directory that is not a . or a .. entry, 
		* then look up the information for the entry.  We have already
		* assigned extents for directories, so we just need to
		* fill in the blanks here.
		*/
	       if (strcmp(s_entry->name,".") && strcmp(s_entry->name,"..") && 
		   s_entry->isorec.flags[0] == 2)
	       {
		    finddir = dpnt->subdir;
		    while(1==1)
		    {
			 if(finddir->self == s_entry) break;
			 finddir = finddir->next;
			 if(!finddir) 
			 {
			      fprintf(stderr,"Fatal goof\n"); exit(1);
			 }
		    }
		    set_733((char *) s_entry->isorec.extent, finddir->extent);
		    s_entry->starting_block = finddir->extent;
		    s_entry->size = ROUND_UP(finddir->size);
		    total_dir_size += s_entry->size;
		    add_hash(s_entry);
		    set_733((char *) s_entry->isorec.size, ROUND_UP(finddir->size));
		    continue;
	       }


	       /*
		* If this is . or .., then look up the relevant info from the
		* tables.
		*/
	       if(strcmp(s_entry->name,".") == 0) 
	       {
		    set_733((char *) s_entry->isorec.extent, dpnt->extent);
		    
		    /* 
		     * Set these so that the hash table has the
		     * correct information
		     */
		    s_entry->starting_block = dpnt->extent;
		    s_entry->size = ROUND_UP(dpnt->size);
		    
		    add_hash(s_entry);
		    s_entry->starting_block = dpnt->extent;
		    set_733((char *) s_entry->isorec.size, ROUND_UP(dpnt->size));
		    continue;
	       }

	       if(strcmp(s_entry->name,"..") == 0) 
	       {
		    if(dpnt == root)
		    { 
			 total_dir_size += root->size;
		    }
		    set_733((char *) s_entry->isorec.extent, dpnt->parent->extent);
		    
		    /* 
		     * Set these so that the hash table has the
		     * correct information
		     */
		    s_entry->starting_block = dpnt->parent->extent;
		    s_entry->size = ROUND_UP(dpnt->parent->size);
		    
		    add_hash(s_entry);
		    s_entry->starting_block = dpnt->parent->extent;
		    set_733((char *) s_entry->isorec.size, ROUND_UP(dpnt->parent->size));
		    continue;
	       }

	       /* 
		* Some ordinary non-directory file.  Just schedule the
		* file to be written.  This is all quite
		* straightforward, just make a list and assign extents
		* as we go.  Once we get through writing all of the
		* directories, we should be ready write out these
		* files
		*/
	       if(s_entry->size) 
	       {
		    dwpnt = (struct deferred_write *) 
			 e_malloc(sizeof(struct deferred_write));
		    if(dw_tail)
		    {
			 dw_tail->next = dwpnt;
			 dw_tail = dwpnt;
		    } 
		    else 
		    {
			 dw_head = dwpnt;
			 dw_tail = dwpnt;
		    }
		    if(s_entry->inode  ==  TABLE_INODE) 
		    {
			 dwpnt->table = s_entry->table;
			 dwpnt->name = NULL;
			 sprintf(whole_path,"%s%sTRANS.TBL",
				 s_entry->filedir->whole_name, SPATH_SEPARATOR);
		    } 
		    else 
		    {
			 dwpnt->table = NULL;
			 strcpy(whole_path, s_entry->whole_name);
			 dwpnt->name = strdup(whole_path);
		    }
		    dwpnt->next = NULL;
		    dwpnt->size = s_entry->size;
		    dwpnt->extent = last_extent;
		    set_733((char *) s_entry->isorec.extent, last_extent);
		    s_entry->starting_block = last_extent;
		    add_hash(s_entry);
		    last_extent += ROUND_UP(s_entry->size) >> 11;
		    if(verbose)
		    {
			 fprintf(stderr,"%d %d %s\n", s_entry->starting_block,
				 last_extent-1, whole_path);
		    }
#ifdef DBG_ISO
		    if((ROUND_UP(s_entry->size) >> 11) > 500)
		    {
			 fprintf(stderr,"Warning: large file %s\n", whole_path);
			 fprintf(stderr,"Starting block is %d\n", s_entry->starting_block);
			 fprintf(stderr,"Reported file size is %d extents\n", s_entry->size);
			 
		    }
#endif
		    if(last_extent > (800000000 >> 11)) 
		    { 
			 /*
			  * More than 800Mb? Punt 
			  */
			 fprintf(stderr,"Extent overflow processing file %s\n", whole_path);
			 fprintf(stderr,"Starting block is %d\n", s_entry->starting_block);
			 fprintf(stderr,"Reported file size is %d extents\n", s_entry->size);
			 exit(1);
		    }
		    continue;
	       }

	       /*
		* This is for zero-length files.  If we leave the extent 0,
		* then we get screwed, because many readers simply drop files
		* that have an extent of zero.  Thus we leave the size 0,
		* and just assign the extent number.
		*/
	       set_733((char *) s_entry->isorec.extent, last_extent);
	  }
	  if(dpnt->subdir) 
	  {
	       assign_file_addresses(dpnt->subdir);
	  }
	  dpnt = dpnt->next;
     }
} /* assign_file_addresses(... */

void FDECL2(generate_one_directory, struct directory *, dpnt, FILE *, outfile)
{
     unsigned int			  ce_address = 0;
     char				* ce_buffer;
     unsigned int			  ce_index = 0;
     unsigned int			  ce_size;
     unsigned int			  dir_index;
     char				* directory_buffer;
     int				  new_reclen;
     struct directory_entry		* s_entry;
     struct directory_entry		* s_entry_d;
     unsigned int			  total_size;
     
     total_size = (dpnt->size + (SECTOR_SIZE - 1)) &  ~(SECTOR_SIZE - 1);
     directory_buffer = (char *) e_malloc(total_size);
     memset(directory_buffer, 0, total_size);
     dir_index = 0;
     
     ce_size = (dpnt->ce_bytes + (SECTOR_SIZE - 1)) &  ~(SECTOR_SIZE - 1);
     ce_buffer = NULL;
     
     if(ce_size) 
     {
	  ce_buffer = (char *) e_malloc(ce_size);
	  memset(ce_buffer, 0, ce_size);
	  
	  ce_index = 0;
	  
	  /*
	   * Absolute byte address of CE entries for this directory 
	   */
	  ce_address = last_extent_written + (total_size >> 11);
	  ce_address = ce_address << 11;
     }
     
     s_entry = dpnt->contents;
     while(s_entry) 
     {

	  /* 
	   * We do not allow directory entries to cross sector boundaries.  
	   * Simply pad, and then start the next entry at the next sector 
	   */
	  new_reclen = s_entry->isorec.length[0];
	  if( (dir_index & (SECTOR_SIZE - 1)) + new_reclen >= SECTOR_SIZE )
	  {
	       dir_index = (dir_index + (SECTOR_SIZE - 1)) & 
		    ~(SECTOR_SIZE - 1);
	  }

	  memcpy(directory_buffer + dir_index, &s_entry->isorec, 
		 sizeof(struct iso_directory_record) -
		 sizeof(s_entry->isorec.name) + s_entry->isorec.name_len[0]);
	  dir_index += sizeof(struct iso_directory_record) - 
	       sizeof (s_entry->isorec.name)+ s_entry->isorec.name_len[0];

	  /*
	   * Add the Rock Ridge attributes, if present 
	   */
	  if(s_entry->rr_attr_size)
	  {
	       if(dir_index & 1)
	       {
		    directory_buffer[dir_index++] = 0;
	       }

	       /* 
		* If the RR attributes were too long, then write the
		* CE records, as required.
		*/
	       if(s_entry->rr_attr_size != s_entry->total_rr_attr_size) 
	       {
		    unsigned char * pnt;
		    int len, nbytes;
		    
		    /* 
		     * Go through the entire record and fix up the CE entries
		     * so that the extent and offset are correct 
		     */
		    
		    pnt = s_entry->rr_attributes;
		    len = s_entry->total_rr_attr_size;
		    while(len > 3)
		    {
#ifdef DEBUG
			 if (!ce_size)
			 {
			      fprintf(stderr,"Warning: ce_index(%d) && ce_address(%d) not initialized\n",
				      ce_index, ce_address);
			 }
#endif
			 
			 if(pnt[0] == 'C' && pnt[1] == 'E') 
			 {
			      nbytes = get_733( (char *) pnt+20);
			      
			      if((ce_index & (SECTOR_SIZE - 1)) + nbytes >=
				 SECTOR_SIZE) 
			      {
				   ce_index = ROUND_UP(ce_index);
			      }
			      
			      set_733( (char *) pnt+4, 
				       (ce_address + ce_index) >> 11);
			      set_733( (char *) pnt+12, 
				       (ce_address + ce_index) & (SECTOR_SIZE - 1));
			      
			      
			      /* 
			       * Now store the block in the ce buffer 
			       */
			      memcpy(ce_buffer + ce_index, 
				     pnt + pnt[2], nbytes);
			      ce_index += nbytes;
			      if(ce_index & 1) 
			      {
				   ce_index++;
			      }
			 }
			 len -= pnt[2];
			 pnt += pnt[2];
		    }
		    
	       }

	       rockridge_size += s_entry->total_rr_attr_size;
	       memcpy(directory_buffer + dir_index, s_entry->rr_attributes, 
		      s_entry->rr_attr_size);
	       dir_index += s_entry->rr_attr_size;
	  }
	  if(dir_index & 1)
	  {
	       directory_buffer[dir_index++] = 0;
	  }
	  
	  s_entry_d = s_entry;
	  s_entry = s_entry->next;
	  
	  if (s_entry_d->rr_attributes) free(s_entry_d->rr_attributes);
	  if( s_entry_d->name != NULL )
	    {
	      free (s_entry_d->name);
	    }
	  if( s_entry_d->whole_name != NULL )
	    {
	      free (s_entry_d->whole_name);
	    }
	  free (s_entry_d);
     }
     sort_dir = NULL;

     if(dpnt->size != dir_index)
     {
	  fprintf(stderr,"Unexpected directory length %d %d %s\n",dpnt->size, 
	    dir_index, dpnt->de_name);
     }

     xfwrite(directory_buffer, 1, total_size, outfile);
     last_extent_written += total_size >> 11;
     free(directory_buffer);

     if(ce_size)
     {
	  if(ce_index != dpnt->ce_bytes)
	  {
	       fprintf(stderr,"Continuation entry record length mismatch (%d %d).\n",
		       ce_index, dpnt->ce_bytes);
	  }
	  xfwrite(ce_buffer, 1, ce_size, outfile);
	  last_extent_written += ce_size >> 11;
	  free(ce_buffer);
     }
     
} /* generate_one_directory(... */

static 
void FDECL1(build_pathlist, struct directory *, node)
{
     struct directory * dpnt;
     
     dpnt = node;
     
     while (dpnt)
     {
	  pathlist[dpnt->path_index] = dpnt;
	  if(dpnt->subdir) build_pathlist(dpnt->subdir);
	  dpnt = dpnt->next;
     }
} /* build_pathlist(... */

int FDECL2(compare_paths, void const *, r, void const *, l) 
{
  struct directory const *ll = *(struct directory * const *)l;
  struct directory const *rr = *(struct directory * const *)r;

  if (rr->parent->path_index < ll->parent->path_index)
  {
       return -1;
  }

  if (rr->parent->path_index > ll->parent->path_index) 
  {
       return 1;
  }

  return strcmp(rr->self->isorec.name, ll->self->isorec.name);
  
} /* compare_paths(... */

void generate_path_tables()
{
  struct directory_entry * de;
  struct directory	 * dpnt;
  int			   fix;
  int			   i;
  int			   j;
  int			   namelen;
  char			 * npnt;
  char			 * npnt1;
  int			   tablesize;

  /*
   * First allocate memory for the tables and initialize the memory 
   */
  tablesize = path_blocks << 11;
  path_table_m = (char *) e_malloc(tablesize);
  path_table_l = (char *) e_malloc(tablesize);
  memset(path_table_l, 0, tablesize);
  memset(path_table_m, 0, tablesize);

  /*
   * Now start filling in the path tables.  Start with root directory 
   */
  path_table_index = 0;
  pathlist = (struct directory **) e_malloc(sizeof(struct directory *) 
					    * next_path_index);
  memset(pathlist, 0, sizeof(struct directory *) * next_path_index);
  build_pathlist(root);

  do
  {
       fix = 0;
       qsort(&pathlist[1], next_path_index-1, sizeof(struct directory *), 
	     (int (*)(const void *, const void *))compare_paths);

       for(j=1; j<next_path_index; j++)
       {
	    if(pathlist[j]->path_index != j)
	    {
		 pathlist[j]->path_index = j;
		 fix++;
	    }
       }
  } while(fix);

  for(j=1; j<next_path_index; j++)
  {
       dpnt = pathlist[j];
       if(!dpnt)
       {
	    fprintf(stderr,"Entry %d not in path tables\n", j);
	    exit(1);
       }
       npnt = dpnt->de_name;
       
       /* 
	* So the root comes out OK 
	*/
       if( (*npnt == 0) || (dpnt == root) ) 
       {
	    npnt = ".";  
       }
       npnt1 = strrchr(npnt, PATH_SEPARATOR);
       if(npnt1) 
       { 
	    npnt = npnt1 + 1;
       }
       
       de = dpnt->self;
       if(!de) 
       {
	    fprintf(stderr,"Fatal goof\n"); 
	    exit(1);
       }
       
       
       namelen = de->isorec.name_len[0];
       
       path_table_l[path_table_index] = namelen;
       path_table_m[path_table_index] = namelen;
       path_table_index += 2;
       
       set_731(path_table_l + path_table_index, dpnt->extent); 
       set_732(path_table_m + path_table_index, dpnt->extent); 
       path_table_index += 4;
       
       set_721(path_table_l + path_table_index, 
	       dpnt->parent->path_index); 
       set_722(path_table_m + path_table_index, 
	       dpnt->parent->path_index); 
       path_table_index += 2;
       
       for(i =0; i<namelen; i++)
       {
	    path_table_l[path_table_index] = de->isorec.name[i];
	    path_table_m[path_table_index] = de->isorec.name[i];
	    path_table_index++;
       }
       if(path_table_index & 1) 
       {
	    path_table_index++;  /* For odd lengths we pad */
       }
  }
  
  free(pathlist);
  if(path_table_index != path_table_size)
  {
       fprintf(stderr,"Path table lengths do not match %d %d\n",
	       path_table_index,
	       path_table_size);
  }
} /* generate_path_tables(... */

void
FDECL3(memcpy_max, char *, to, char *, from, int, max)
{
  int n = strlen(from);
  if (n > max)
  {
       n = max;
  }
  memcpy(to, from, n);

} /* memcpy_max(... */

int FDECL1(iso_write, FILE *, outfile)
{
  char				buffer[2048];
  int				i;
  char				iso_time[17];
  int				should_write;

  time(&begun);
  assign_file_addresses(root);

  memset(buffer, 0, sizeof(buffer));

  /*
   * This will break  in the year  2000, I supose, but there is no good way
   * to get the top two digits of the year. 
   */
  sprintf(iso_time, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d00", 1900 + local.tm_year,
	  local.tm_mon+1, local.tm_mday,
	  local.tm_hour, local.tm_min, local.tm_sec);

  local.tm_min -= gmt.tm_min;
  local.tm_hour -= gmt.tm_hour;
  local.tm_yday -= gmt.tm_yday;
  iso_time[16] = (local.tm_min + 60*(local.tm_hour + 24*local.tm_yday));

  /*
   * First, we output 16 sectors of all zero 
   */

  for(i=0; i<16; i++)
    {
      xfwrite(buffer, 1, sizeof(buffer), outfile);
    }

  last_extent_written += 16;

  /*
   * Next we write out the primary descriptor for the disc 
   */
  memset(&vol_desc, 0, sizeof(vol_desc));
  vol_desc.type[0] = ISO_VD_PRIMARY;
  memcpy(vol_desc.id, ISO_STANDARD_ID, sizeof(ISO_STANDARD_ID));
  vol_desc.version[0] = 1;
  
  memset(vol_desc.system_id, ' ', sizeof(vol_desc.system_id));
  memcpy_max(vol_desc.system_id, system_id, strlen(system_id));
  
  memset(vol_desc.volume_id, ' ', sizeof(vol_desc.volume_id));
  memcpy_max(vol_desc.volume_id, volume_id, strlen(volume_id));
  
  should_write = last_extent - session_start;
  set_733((char *) vol_desc.volume_space_size, should_write);
  set_723(vol_desc.volume_set_size, 1);
  set_723(vol_desc.volume_sequence_number, DEF_VSN);
  set_723(vol_desc.logical_block_size, 2048);
  
  /*
   * The path tables are used by DOS based machines to cache directory
   * locations 
   */

  set_733((char *) vol_desc.path_table_size, path_table_size);
  set_731(vol_desc.type_l_path_table, path_table[0]);
  set_731(vol_desc.opt_type_l_path_table, path_table[1]);
  set_732(vol_desc.type_m_path_table, path_table[2]);
  set_732(vol_desc.opt_type_m_path_table, path_table[3]);

  /*
   * Now we copy the actual root directory record 
   */
  memcpy(vol_desc.root_directory_record, &root_record, 
	 sizeof(struct iso_directory_record) + 1);

  /*
   * The rest is just fluff.  It looks nice to fill in many of these fields,
   * though.
   */
  FILL_SPACE(volume_set_id);
  if(volset_id)  memcpy_max(vol_desc.volume_set_id,  volset_id, strlen(volset_id));

  FILL_SPACE(publisher_id);
  if(publisher)  memcpy_max(vol_desc.publisher_id,  publisher, strlen(publisher));

  FILL_SPACE(preparer_id);
  if(preparer)  memcpy_max(vol_desc.preparer_id,  preparer, strlen(preparer));

  FILL_SPACE(application_id);
  if(appid) memcpy_max(vol_desc.application_id, appid, strlen(appid));

  FILL_SPACE(copyright_file_id);
  if(copyright) memcpy_max(vol_desc.copyright_file_id, copyright, 
		       strlen(copyright));

  FILL_SPACE(abstract_file_id);
  if(abstract) memcpy_max(vol_desc.abstract_file_id, abstract, 
			  strlen(abstract));

  FILL_SPACE(bibliographic_file_id);
  if(biblio) memcpy_max(vol_desc.bibliographic_file_id, biblio, 
		       strlen(biblio));

  FILL_SPACE(creation_date);
  FILL_SPACE(modification_date);
  FILL_SPACE(expiration_date);
  FILL_SPACE(effective_date);
  vol_desc.file_structure_version[0] = 1;
  FILL_SPACE(application_data);

  memcpy(vol_desc.creation_date,  iso_time, 17);
  memcpy(vol_desc.modification_date,  iso_time, 17);
  memcpy(vol_desc.expiration_date, "0000000000000000", 17);
  memcpy(vol_desc.effective_date,  iso_time,  17);

  /*
   * if not a bootable cd do it the old way 
   */
  xfwrite(&vol_desc, 1, 2048, outfile);
  last_extent_written++;
  if (use_eltorito) 
    {
      /*
       * Next we write out the boot volume descriptor for the disc 
       */
      get_torito_desc(&boot_desc);
      xfwrite(&boot_desc, 1, 2048, outfile);
      last_extent_written ++;
    }
  
  /*
   * Now write the end volume descriptor.  Much simpler than the other one 
   */
  memset(&vol_desc, 0, sizeof(vol_desc));
  vol_desc.type[0] = ISO_VD_END;
  memcpy(vol_desc.id, ISO_STANDARD_ID, sizeof(ISO_STANDARD_ID));
  vol_desc.version[0] = 1;
  xfwrite(&vol_desc, 1, 2048, outfile);
  last_extent_written += 1;

  /*
   * Next we write the path tables 
   */
  xfwrite(path_table_l, 1, path_blocks << 11, outfile);
  xfwrite(path_table_m, 1, path_blocks << 11, outfile);
  last_extent_written += 2*path_blocks;
  free(path_table_l);
  free(path_table_m);
  path_table_l = NULL;
  path_table_m = NULL;

  /*
   * OK, all done with that crap.  Now write out the directories.
   * This is where the fur starts to fly, because we need to keep track of
   * each file as we find it and keep track of where we put it. 
   */

#ifdef DBG_ISO
  fprintf(stderr,"Total directory extents being written = %d\n", last_extent);
#endif
#if 0 
 generate_one_directory(root, outfile);
#endif
  generate_iso9660_directories(root, outfile);

  if(extension_record) 
    {
      xfwrite(extension_record, 1, SECTOR_SIZE, outfile);
      last_extent_written++;
    }

  /* 
   * Now write all of the files that we need. 
   */
  fprintf(stderr,"Total extents scheduled to be written = %d\n", 
	  last_extent - session_start);
  write_files(outfile);
  
  fprintf(stderr,"Total extents actually written = %d\n", 
	  last_extent_written - session_start);
  /* 
   * Hard links throw us off here 
   */
  if(should_write != last_extent - session_start)
    {
      fprintf(stderr,"Number of extents written not what was predicted.  Please fix.\n");
      fprintf(stderr,"Predicted = %d, written = %d\n", should_write, last_extent);
    }

  fprintf(stderr,"Total translation table size: %d\n", table_size);
  fprintf(stderr,"Total rockridge attributes bytes: %d\n", rockridge_size);
  fprintf(stderr,"Total directory bytes: %d\n", total_dir_size);
  fprintf(stderr,"Path table size(bytes): %d\n", path_table_size);

#ifdef DEBUG
  fprintf(stderr, "next extent, last_extent, last_extent_written %d %d %d\n",
	  next_extent, last_extent, last_extent_written);
#endif

  return 0;

} /* iso_write(... */
