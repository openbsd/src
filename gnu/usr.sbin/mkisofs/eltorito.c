/*	$OpenBSD: eltorito.c,v 1.2 1998/04/05 00:39:31 deraadt Exp $	*/
/*
 * Program eltorito.c - Handle El Torito specific extensions to iso9660.
 * 

   Written by Michael Fulbright <msf@redhat.com> (1996).

   Copyright 1996 RedHat Software, Incorporated

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.   */


static char rcsid[] ="$From: eltorito.c,v 1.5 1997/03/08 17:27:01 eric Rel $";

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include "mkisofs.h"
#include "iso9660.h"

#undef MIN
#define MIN(a, b) (((a) < (b))? (a): (b))

static struct eltorito_validation_entry valid_desc;
static struct eltorito_defaultboot_entry default_desc;

/*
 * Check for presence of boot catalog. If it does not exist then make it 
 */
void FDECL1(init_boot_catalog, const char *, path)
{

    int		  bcat;
    char		* bootpath;                /* filename of boot catalog */
    char		* buf;
    struct stat	  statbuf;
    
    bootpath = (char *) e_malloc(strlen(boot_catalog)+strlen(path)+2);
    strcpy(bootpath, path);
    if (bootpath[strlen(bootpath)-1] != '/') 
    {
	strcat(bootpath,"/");
    }
    
    strcat(bootpath, boot_catalog);
    
    /*
     * check for the file existing 
     */
#ifdef DEBUG_TORITO
    printf("Looking for boot catalog file %s\n",bootpath);
#endif
    
    if (!stat_filter(bootpath, &statbuf)) 
    {
	/*
	 * make sure its big enough to hold what we want 
	 */
	if (statbuf.st_size == 2048) 
	{
	    /*
	     * printf("Boot catalog exists, so we do nothing\n"); 
	     */
	    free(bootpath);
	    return;
	}
	else 
	{
	    fprintf(stderr, "A boot catalog exists and appears corrupted.\n");
	    fprintf(stderr, "Please check the following file: %s.\n",bootpath);
	    fprintf(stderr, "This file must be removed before a bootable CD can be done.\n");
	    free(bootpath);
	    exit(1);
	}
    }
    
    /*
     * file does not exist, so we create it 
     * make it one CD sector long
     */
    bcat = open(bootpath, O_WRONLY | O_CREAT, S_IROTH | S_IRGRP | S_IRWXU );
    if (bcat == -1) 
    {
	fprintf(stderr, "Error creating boot catalog, exiting...\n");
	perror("");
	exit(1);
    }
    
    buf = (char *) e_malloc( 2048 );
    write(bcat, buf, 2048);
    close(bcat);
    free(bootpath);
} /* init_boot_catalog(... */

void FDECL1(get_torito_desc, struct eltorito_boot_descriptor *, boot_desc)
{
    int				bootcat;
    int				checksum;
    unsigned char		      * checksum_ptr;
    struct directory_entry      * de;
    struct directory_entry      * de2;
    int				i;
    int				nsectors;
    
    memset(boot_desc, 0, sizeof(*boot_desc));
    boot_desc->id[0] = 0;
    memcpy(boot_desc->id2, ISO_STANDARD_ID, sizeof(ISO_STANDARD_ID));
    boot_desc->version[0] = 1;
    
    memcpy(boot_desc->system_id, EL_TORITO_ID, sizeof(EL_TORITO_ID));
    
    /*
     * search from root of iso fs to find boot catalog 
     */
    de2 = search_tree_file(root, boot_catalog);
    if (!de2) 
    {
	fprintf(stderr,"Uh oh, I cant find the boot catalog!\n");
	exit(1);
    }
    
    set_731(boot_desc->bootcat_ptr,
	    (unsigned int) get_733(de2->isorec.extent));
    
    /* 
     * now adjust boot catalog
     * lets find boot image first 
     */
    de=search_tree_file(root, boot_image);
    if (!de) 
    {
	fprintf(stderr,"Uh oh, I cant find the boot image!\n");
	exit(1);
    } 
    
    /* 
     * we have the boot image, so write boot catalog information
     * Next we write out the primary descriptor for the disc 
     */
    memset(&valid_desc, 0, sizeof(valid_desc));
    valid_desc.headerid[0] = 1;
    valid_desc.arch[0] = EL_TORITO_ARCH_x86;
    
    /*
     * we'll shove start of publisher id into id field, may get truncated
     * but who really reads this stuff!
     */
    if (publisher)
        memcpy_max(valid_desc.id,  publisher, MIN(23, strlen(publisher)));    

    valid_desc.key1[0] = 0x55;
    valid_desc.key2[0] = 0xAA;
    
    /*
     * compute the checksum 
     */
    checksum=0;
    checksum_ptr = (unsigned char *) &valid_desc;
    for (i=0; i<sizeof(valid_desc); i+=2) 
    {
	/*
	 * skip adding in ckecksum word, since we dont have it yet! 
	 */
	if (i == 28)
	{
	    continue;
	}
	checksum += (unsigned int)checksum_ptr[i];
	checksum += ((unsigned int)checksum_ptr[i+1])*256;
    }
    
    /* 
     * now find out the real checksum 
     */
    checksum = -checksum;
    set_721(valid_desc.cksum, (unsigned int) checksum);
    
    /*
     * now make the initial/default entry for boot catalog 
     */
    memset(&default_desc, 0, sizeof(default_desc));
    default_desc.boot_id[0] = EL_TORITO_BOOTABLE;
    
    /*
     * use default BIOS loadpnt
     */ 
    set_721(default_desc.loadseg, 0);
    default_desc.arch[0] = EL_TORITO_ARCH_x86;
    
    /*
     * figure out size of boot image in sectors, for now hard code to
     * assume 512 bytes/sector on a bootable floppy
     */
    nsectors = ((de->size + 511) & ~(511))/512;
    printf("\nSize of boot image is %d sectors -> ", nsectors); 
    
    /*
     * choose size of emulated floppy based on boot image size 
     */
    if (nsectors == 2880 ) 
    {
	default_desc.boot_media[0] = EL_TORITO_MEDIA_144FLOP;
	printf("Emulating a 1.44 meg floppy\n");
    }
    else if (nsectors == 5760 ) 
    {
	default_desc.boot_media[0] = EL_TORITO_MEDIA_288FLOP;
	printf("Emulating a 2.88 meg floppy\n");
    }
    else if (nsectors == 2400 ) 
    {
	default_desc.boot_media[0] = EL_TORITO_MEDIA_12FLOP;
	printf("Emulating a 1.2 meg floppy\n");
    }
    else 
    {
	fprintf(stderr,"\nError - boot image is not the an allowable size.\n");
	exit(1);
    }
    
    
    /* 
     * FOR NOW LOAD 1 SECTOR, JUST LIKE FLOPPY BOOT!!! 
     */
    nsectors = 1;
    set_721(default_desc.nsect, (unsigned int) nsectors );
#ifdef DEBUG_TORITO
    printf("Extent of boot images is %d\n",get_733(de->isorec.extent));
#endif
    set_731(default_desc.bootoff, 
	    (unsigned int) get_733(de->isorec.extent));
    
    /*
     * now write it to disk 
     */
    bootcat = open(de2->whole_name, O_RDWR);
    if (bootcat == -1) 
    {
	fprintf(stderr,"Error opening boot catalog for update.\n");
	perror("");
	exit(1);
    }
    
    /* 
     * write out 
     */
    write(bootcat, &valid_desc, 32);
    write(bootcat, &default_desc, 32);
    close(bootcat);
} /* get_torito_desc(... */
