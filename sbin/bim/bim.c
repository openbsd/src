/*	$NetBSD: bim.c,v 1.4 1995/09/28 07:08:49 phil Exp $	*/

/* 
 * Copyright (c) 1994 Philip A. Nelson.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Philip A. Nelson.
 * 4. The name of Philip A. Nelson may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Boot Image Manager
 *
 *  (First copy called "hdsetup" and was written under Minix.)
 * 
 *   Phil Nelson
 *   Sept 30, 1990
 */

#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <a.out.h>
#include <string.h>
#include <ctype.h>

#define DKTYPENAMES
#include <sys/disklabel.h>
#include "images.h"

#define TRUE 1
#define FALSE 0
#define MAXARGCMDS 20

#define BLOCK_SIZE 1024

#define DEFAULT_DEVICE "/dev/sd0c"

/*  Global data....  */
char disk_info [BLOCK_SIZE];
int disk_fd;		/* The file descriptor for the disk. */

struct disklabel *dk_label = (struct disklabel *) &disk_info[LABELOFFSET];
struct imageinfo *im_table = 
    (struct imageinfo *) (&disk_info[LABELOFFSET] + sizeof(struct disklabel));

char *prog_name;
int  label_changed = FALSE;
int  images_changed = FALSE;
int  secsize;


/* Utility routines... */
/***********************/

void usage ()
{
  printf ("usage: %s [-c command [-c command ...]] [device]\n",prog_name);
  printf ("  Maximum of %d commands\n", MAXARGCMDS);
  exit (-2);
}

error (s1, s2)
char *s1, *s2;
{
  printf ("%s: %s%s\n",prog_name,s1,s2);
  exit (3);
}

void getlf ( inchar )
 char inchar; 
{
   while ( inchar != '\n')  inchar = getchar();
}

void getstr (str, size)
  char *str;
  int  size;
{
  char inchar;
  int  count;

  count = 0;
  inchar = getchar();
  while (count < size-1 && inchar != '\n') {
    *str++ = inchar;
    count++;
    inchar = getchar();
  }
  *str++ = 0;
  getlf (inchar);
}

/* Checksum a disk label */
unsigned short
dkcksum(lp)
        struct disklabel *lp;
{
        register unsigned short *start, *end, sum = 0;

        start = (unsigned short *)lp;
        end = (unsigned short*)&lp->d_partitions[lp->d_npartitions];
        while (start < end) sum ^= *start++;
        return sum;
}


void save_images ()
{
  int count;

  count = (int) lseek (disk_fd, (off_t) 0, SEEK_SET);
  if (count != 0)
    error ("lseek error in saving image info.","");
  count = write (disk_fd, disk_info, BLOCK_SIZE);
  if (count != BLOCK_SIZE)
    error ("write error in saveing image info.","");
  sync ();
}

/* Get a number using the prompt routine. */
GetInt (num, prompt_str)
     int *num;
     char *prompt_str;
{
  char answer[80];
  
  prompt (answer, 80, prompt_str);
  while (!Str2Int (answer, num))
    {
      printf ("Bad number.\n");
      prompt (answer, 80, prompt_str);
    }
}


/* This function will initialize the image information . */

void init_images (badmagic)
char badmagic;
{
    char answer[80];
    int  index;

    if (badmagic) {
	printf ("Image information has improper magic number.\n");
	while (TRUE) {
	    prompt (answer,3,"Do you want the images initialized? (y or n) ");
	    if (answer[0] == 'y') break;
	    if (answer[0] == 'n') error ("images not initialized.","");
	}
    }

    /* Initialize the image table. */
    im_table->ii_magic = IMAGE_MAGIC;
    im_table->ii_boot_partition = -1;
    for (index = 0; index < dk_label->d_npartitions; index++)
      if (dk_label->d_partitions[index].p_fstype == FS_BOOT)
        {
          im_table->ii_boot_partition = index;
	  break;
	}
    im_table->ii_boot_count = MAXIMAGES;
    im_table->ii_boot_used = 0;
    im_table->ii_boot_default = -1;
    images_changed = TRUE;
}


/* Print out the header and other information about the disk. */

int display_part(num, args, syntax)
     int num;
     char **args;
     char *syntax;
{
  int count;

  printf ("\nDisk: %s   Type: %s\n", dk_label->d_packname,
		dk_label->d_typename);
  printf ("Physical Sector Size = %d\n", dk_label->d_secsize);
  printf ("Disk Size = %ld\n", dk_label->d_secperunit);

  /* Disk Partitions. */
  printf (" partition         type  sector start  length in sectors\n");
  for (count = 0; count < dk_label->d_npartitions; count++) {
    if (dk_label->d_partitions[count].p_fstype != FS_UNUSED) {
      printf (" %5c  ", 'a'+count);
      printf ("%14s", fstypenames[dk_label->d_partitions[count].p_fstype]);
      printf ("%14ld  %17ld\n",
		dk_label->d_partitions[count].p_offset,
		dk_label->d_partitions[count].p_size);
    }
  }
  printf ("\n");

  return FALSE;
}

int display_image(num, args, syntax)
     int num;
     char **args;
     char *syntax;
{
  int count;

  /* Boot Images. */
  if (im_table->ii_boot_partition != -1)
    printf ("Boot partition = %c\n", 'a'+im_table->ii_boot_partition);
  if (im_table->ii_boot_default != -1)
    printf ("Default boot image  = %d\n", im_table->ii_boot_default);
  printf ("Boot Images: total of %d\n",im_table->ii_boot_count);
  printf ("  (image address and size in sectors.)\n");
  printf ("Image  address   size  load addr  run addr   name\n");
  for (count = 0; count < im_table->ii_boot_used; count++) {
    printf ("%5d %8lx %6lx  %9lx %9lx   %s\n", count,
		im_table->ii_images[count].boot_address/secsize,
		im_table->ii_images[count].boot_size/secsize,
		im_table->ii_images[count].boot_load_adr,
		im_table->ii_images[count].boot_run_adr,
		im_table->ii_images[count].boot_name );
  }
  printf ("\n");
  return FALSE;
}

int display_head(num, args, syntax)
     int num;
     char **args;
     char *syntax;
{
  printf ("\nDisk: %s   Type: %s\n", dk_label->d_packname,
		dk_label->d_typename);
  printf ("Physical Sector Size = %d\n", dk_label->d_secsize);
  printf ("Disk Size = %ld\n", dk_label->d_secperunit);

  return FALSE;
}


/* Utility routine for moving boot images.  These are byte addresses 
   relative to the start of the files.  */

int copy_bytes (from_fd, from_adr, to_fd, to_adr, number)
   int from_fd, from_adr, to_fd, to_adr, number;
{ 
  int count;
  int index;
  int index1;
  int left;
  int xfer_size;
  char buffer [BLOCK_SIZE];

  /* Check the parameters. */
  if (to_adr > from_adr && from_fd == to_fd)
    {
      printf ("There is a system error. (copy_bytes)\n");
      return 0;
    }

  /* Do the copy. */
  for (index = 0; index < number; index += BLOCK_SIZE)
    {
      count = lseek (from_fd, (off_t) (from_adr+index), SEEK_SET);
      if (count != from_adr+index)
        {
          printf ("Error in copying (seek from)\n");
	  return 0;
        }
      count = read (from_fd, buffer, BLOCK_SIZE);
      if (count != BLOCK_SIZE)
        {
          if (index != number-1 || count < 0)
	    {
              printf ("Error in copying (read from)\n");
	      return 0;
	    }
	  else
	    {
	      while (count < BLOCK_SIZE)
	        buffer[count++] = 0;
	    }
        }
      count = lseek (to_fd, (off_t) (to_adr+index), SEEK_SET);
      if (count != to_adr+index)
        {
          printf ("Error in copying (seek to)\n");
	  return 0;
        }
      count = write (to_fd, buffer, BLOCK_SIZE);
      if (count != BLOCK_SIZE)
        {
          printf ("Error in copying (write to)\n");
	  return 0;
        }
    }

  /* Success. */
  return 1;
}


/* Add a boot image. */
int
add_image (num, args, syntax)
    int num;
    char **args;
    char *syntax;
{
  struct exec im_exec;  /* Information about a new image. */
  int im_file;
  int im_size;	    /* Size of text and data in bytes. Rounded up to a full
			block in both text and data. */
  int which_image;  /* Which image is to be operated upon. */
  int count;        /* read/write counts. */
  int index;	    /* temporary variable for loops. */

  int part_size;    /* The total size of the boot partition (in bytes). */
  int total_size;   /* The total size of all images (in bytes). */
  int boot_start;   /* Byte address of start of boot partition. */
  int new_size;     /* The new total size of all images. */
  int im_addr;      /* Byte address of the new boot image. */
  unsigned int im_load_adr;  /* The memory load address. */
  unsigned int im_run_adr;   /* The memory run address. */
  char *nptr;	    /* Pointer for makeing name lower case. */

  /* Check argument numbers. */
  if (num != 2 && num !=3)
    {
      printf ("Syntax: %s\n", syntax);
      return FALSE;
    }

  /* Check for a boot partition. */
  if (im_table->ii_boot_partition == -1)
    {
      printf ("There is no boot partition.\n");
      return FALSE; 
    }

  /* Any free images? */
  which_image = im_table->ii_boot_used;
  if (which_image == im_table->ii_boot_count)
    {
      printf ("No more boot image slots available.\n");
      return FALSE;
    }

  /* Open the file. */
  im_file = open (args[1], O_RDONLY);
  if (im_file < 0)
    {
      printf ("Could not open %s\n", args[1]);
      return FALSE;
    }

  /* check the exec header. */
  count = read (im_file, (char *) &im_exec, sizeof(struct exec));
  if (count != sizeof(struct exec))
    {
      printf ("Read problems for file %s\n", args[1]);
      close (im_file);
      return FALSE;
    }

  if (N_GETMAGIC(im_exec) != ZMAGIC || N_GETMID(im_exec) != MID_MACHINE)
    {
      printf ("%s is not a a pc532 executable file.\n", args[1]);
      close (im_file);
      return FALSE;
    }

  if (im_exec.a_entry < 0x2000)
    {
       printf ("%s has a load address less than 0x2000.\n", args[1]);
       close (im_file);
       return;
    }
  im_load_adr = im_exec.a_entry - sizeof(im_exec); /* & ~(__LDPGSZ-1); */
  im_run_adr = im_exec.a_entry;

  if (im_load_adr > 0xFFFFFF)
    {
	im_load_adr = im_load_adr & 0xFFFFFF;
	im_run_adr  = im_run_adr  & 0xFFFFFF;
	printf ("%s has a load address greater than 0xFFFFFF.\n", args[1]);
	printf (
	  "using the address:\n\tload address = 0x%x\n\trun address = 0x%x\n",
	  im_load_adr, im_run_adr);
    }

  /* Check the sizes.  */
  boot_start = dk_label->d_partitions[im_table->ii_boot_partition].p_offset
  		* secsize;
  part_size = dk_label->d_partitions[im_table->ii_boot_partition].p_size
  		* secsize;
  total_size = 0;
  for (index = 0; index < im_table->ii_boot_used; index++)
    total_size = total_size + im_table->ii_images [index].boot_size;

  /* Calculate other things. */
  im_size = im_exec.a_text + im_exec.a_data;

  /* Final check. */
  new_size = total_size + im_size;
  if (new_size > part_size)
    {
      printf ("Image too big to fit in boot partition.\n");
      close(im_file);
    }

  /* Add the image. */
  im_addr = (total_size+secsize-1)/secsize * secsize;
  im_table->ii_images [which_image].boot_address = im_addr;
  im_table->ii_images [which_image].boot_size = im_size;
  im_table->ii_images [which_image].boot_load_adr = im_load_adr;
  im_table->ii_images [which_image].boot_run_adr = im_run_adr;
  if (num == 3)
    strncpy (im_table->ii_images [which_image].boot_name, args[2], 15);
  else
    strncpy (im_table->ii_images [which_image].boot_name, args[1], 15);
  if (copy_bytes (im_file,0,disk_fd,boot_start+im_addr,im_size))
    {
      im_table->ii_boot_used++;
      /* Make name lowercase and report on image. */
      for (nptr = im_table->ii_images[which_image].boot_name;
	   *nptr != 0;
	   nptr++)
	if (isupper(*nptr)) *nptr = tolower (*nptr);
      printf ("added image %d (%s).\n", which_image,
	       im_table->ii_images[which_image].boot_name);
      close (im_file);
    }
  else
    {
      printf ("Problems in installing image.\n");
      close (im_file);
      return FALSE;
    }

  /* Save the changes. */
  save_images ();    
  return FALSE;
}

/* Delete a boot image. */
int
delete_image (num, args, syntax)
    int num;
    char **args;
    char *syntax;
{
  int which_image;  /* Which image is to be operated upon. */
  int index;	    /* temporary variable for loops. */
  int boot_start;   /* Zone number of start of boot partition. */
  int del_size;     /* Size of the deleted image. */

  /* Check arguments. */
  if (num != 2)
    {
      printf ("Syntax: %s\n", syntax);
      return FALSE;
    }

  /* Find the image. */
  which_image = -1;
  for (index = which_image; index < im_table->ii_boot_used; index++)
    if (strcmp(im_table->ii_images[index].boot_name,args[1]) == 0)
      {
	which_image = index;
	break;
      }

  if (which_image == -1)
    if (!Str2Int(args[1],&which_image))
      {
        printf ("Syntax: %s\n", syntax);
        return FALSE;
      }

  if (which_image < 0 || which_image >= im_table->ii_boot_used)
    {
      printf ("Delete: No such image (%s)\n", args[1]);
      return FALSE;
    }

  /* Report on image we are deleteing. */
  printf ("deleting image %d (%s).\n", which_image,
	   im_table->ii_images[which_image].boot_name);

  /* Do the delete. */
  boot_start = dk_label->d_partitions[im_table->ii_boot_partition].p_offset
               * dk_label->d_secsize;
  del_size = im_table->ii_images[which_image].boot_size;
  for (index = which_image; index < im_table->ii_boot_used-1; index++)
    {
      copy_bytes (
	disk_fd, boot_start+im_table->ii_images[index+1].boot_address,
	disk_fd, boot_start+im_table->ii_images[index+1].boot_address-del_size,
	im_table->ii_images[index+1].boot_size);
      im_table->ii_images[index] = im_table->ii_images[index+1];
      im_table->ii_images[index].boot_address -= del_size;
    }
  im_table->ii_boot_used--;
  if (which_image == im_table->ii_boot_default)
    im_table->ii_boot_default = -1;
  else if (which_image < im_table->ii_boot_default)
    im_table->ii_boot_default--;

  /* Save the changes. */
  save_images ();    
  return FALSE;
}

/* Set the default boot image. */
int
set_default_image (num, args, syntax)
     int num;
     char **args;
     char *syntax;
{
  int which_image;

  if (num != 2 || !Str2Int(args[1],&which_image))
    {
      printf ("Syntax: %s\n", syntax);
      return FALSE;
    }

  if (which_image >= im_table->ii_boot_used)
    {
      printf ("No such image.\n");
      return FALSE;
    }

  im_table->ii_boot_default = which_image;
  images_changed = TRUE;
  return FALSE;
}

/* Initialize the disk or just the image portion. */

int
initialize (num, args, syntax)
     int num;
     char **args;
     char *syntax;
{
  /* Check the args */
  if ( num > 1)
    {
      printf ("Syntax: %s\n", syntax);
      return FALSE;
    }

  init_images (FALSE);
  return FALSE;
}


/* Write the disk header and exit. */

int write_exit (num, args, syntax)
     int num;
     char **args;
     char *syntax;
{
  if (images_changed) save_images ();
  
  return TRUE;
}


/* The main program! */
/*********************/

main (argc, argv)
int argc;
char *argv[];
{
    int count;		/* Used by reads. */
    char answer, *fname;
    int cmdscnt;	/* Number of argument line commands. */
    char *argcmds[MAXARGCMDS];
    extern int optind, opterr;
    extern char *optarg;
    char optchar;
    int index;

    /* Check the parameters.  */
    prog_name = argv[0];
    cmdscnt = 0;
    opterr = TRUE;
    fname = DEFAULT_DEVICE;
    while ((optchar = getopt (argc, argv, "c:")) != EOF)
      switch (optchar) {
	case 'c': if (cmdscnt == MAXARGCMDS) usage();
		  argcmds[cmdscnt++] = optarg;
		  break;
	case '?': usage ();
      }

    if (argc - optind > 1) usage();
    if (optind < argc) fname = argv[optind];

    disk_fd = open(fname, O_RDWR);
    if (disk_fd < 0) error("Could not open ", fname);

    /* Read the disk information block. */
    count = read (disk_fd, disk_info, BLOCK_SIZE);
    if (count != BLOCK_SIZE) error("Could not read info block on ", fname);
    
    /* Check for correct information and set up pointers. */
    if (dk_label->d_magic != DISKMAGIC) 
    	error ("Could not find a disk label on", fname);
    if (im_table->ii_magic != IMAGE_MAGIC)  init_images (TRUE);
    if (dkcksum (dk_label) != 0) 
       printf ("Warning: bad checksum in disk label.\n");

    /* initialize secsize. */
    secsize = dk_label->d_secsize;

    /* do the commands.... */
    if (cmdscnt > 0) 
     {
       /* Process the argv commands. */
       for (index = 0; index < cmdscnt; index++)
	 one_command (argcmds[index]);
     }
    else
     {
       /* Interactive command loop.  */
       display_part (0,NULL,NULL);
       display_image (0,NULL,NULL);
       command_loop ();
     }
}
