/*	$OpenBSD: aptest.c,v 1.1 1996/03/05 11:25:46 mickey Exp $	*/
/* 
 * Copyright 1994  	Doug Anson, danson@lgc.com & David Holland, davidh@use.com
 *
 * Author: Doug Anson (danson@lgc.com)
 * Date  : 2/21/94
 * Modifed: David Holland (davidh@use.com)
 * Log:
 * 		DWH - Changed names/added comments	2/23/94
 * 		DWH - Removed annoying delays.		2/23/94
 * 
 * This program test the fb aperture driver by 'cheating'
 * it uses the aperture driver to access/read the main
 * system BIOS header
 * 
 * Copyright notice:
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Doug Anson, and David Holland be used in
 * advertising or publicity pertaining to distribution of the software 
 * Doug Anson, and David Holland make no * representations about the 
 * suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 *
 * Disclaimer:
 * DOUG ANSON, AND DAVID HOLLAND DISCLAIMS ALL WARRIENTS WITH REGARD TO THIS 
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, AND FITNESS, 
 * IN NO EVENT SHALL DOUG ANSON, OR DAVID HOLLAND BE LIABLE FOR ANY SPECIAL, 
 * INDIRECT, OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * USAGE OF THIS SOFTWARE.
 */

/*
 * linear framebuffer aperture driver test program
 */

/* 
 * Id
 */


#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#if !defined(sun)
extern void exit(int);
extern caddr_t mmap();
extern int close();
extern int munmap();
#endif

/* framebuffer access defines */
#define AP_DEV		"/dev/xf86"	/* framebuffer apperture device		*/
#define PADDR		0xa0000				/* offset from fbmem base     		*/
#define BUF_LENGTH  0x1000				/* length in bytes -- ignored 		*/

/* debug testing defines */
#define START_INDEX	0		/* display starting index(>=0)*/
#define STOP_INDEX	80		/* display stopping index	  */
#define INCR		1		/* display increment		  */

/* main program */
int main(int argc,char **argv)
{
	caddr_t	addr = (caddr_t)0;
	int		fb_dev;
	long	start = START_INDEX;
	long	stop = STOP_INDEX;
	int		i;

	/* open the framebuffer device */
	fb_dev = open (AP_DEV,O_RDWR);
	if (fb_dev < 0)
	{
		/* failed to open framebuffer driver */
		printf("ERROR: failed to open %s\n",AP_DEV);
		perror("ERROR: open()");
		exit(1);
	} 

	/* memory map the framebuffer */
	addr = (caddr_t)mmap((caddr_t)0,BUF_LENGTH,PROT_READ|PROT_WRITE,MAP_SHARED,
			             fb_dev,(off_t)PADDR);
	if (addr == (caddr_t)-1)
	{
		/* failed to memory map framebuffer driver */
		printf("ERROR: failed to mmap [0x%x ,size=%d bytes)\n",
			   PADDR,BUF_LENGTH);
		perror("ERROR: mmap()");
		close(fb_dev);
		exit(1);
	}
	else
	{
		/* frame buffer mapped */
		close(fb_dev);
		printf("NOTICE: BIOS mapped [0x%x ,size=%d) to addr=0x%x...\n",
			   PADDR,BUF_LENGTH,(int)addr);

		/* display the buffer */
    	for(i=start;i<stop;i=i+INCR)
			printf("%c",addr[i]);
        	/* printf("addr[%d]=%c\n",i,addr[i]);
			 */
		printf("\nDONE displaying memory contents (%d bytes)\n",stop);

		/* unmap and close */
		printf("UNMAPPING [0x%x ,size=%d) to addr=0x%x... and closing...",
               PADDR,BUF_LENGTH,(int)addr);
		munmap(addr,BUF_LENGTH);
		printf("DONE.\n");
		printf("Exiting successful...\n");
		exit(0);
	}
	return 1;
}
