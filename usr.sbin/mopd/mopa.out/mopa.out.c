/*	$OpenBSD: mopa.out.c,v 1.10 2009/10/27 23:59:52 deraadt Exp $ */

/* mopa.out - Convert a Unix format kernel into something that
 * can be transferred via MOP.
 *
 * This code was written while referring to the NetBSD/vax boot
 * loader. Therefore anything that can be booted by the Vax
 * should be convertable with this program.
 *
 * If necessary, the a.out header is stripped, and the program
 * segments are padded out. The BSS segment is zero filled.
 * A header is prepended that looks like an IHD header. In 
 * particular the Unix mahine ID is placed where mopd expects
 * the image type to be (offset is IHD_W_ALIAS). If the machine
 * ID could be mistaken for a DEC image type, then the conversion 
 * is aborted. The original a.out header is copied into the front
 * of the header so that once we have detected the Unix machine
 * ID we can haul the load address and the xfer address out.
 */

/*
 * Copyright (c) 1996 Lloyd Parkes.  All rights reserved.
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
 *	This product includes software developed by Lloyd Parkes.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "os.h"
#include "common/common.h"
#include "common/mopdef.h"
#include "common/file.h"
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/exec_aout.h>
#endif
#if defined(__FreeBSD__)
#include <sys/imgact_aout.h>
#endif
#if defined(__bsdi__)
#include <a.out.h>
#define NOAOUT
#endif
#if !defined(MID_VAX)
#define MID_VAX 140
#endif

u_char header[512];		/* The VAX header we generate is 1 block. */
struct exec ex, ex_swap;

int
main (int argc, char **argv)
{
	FILE   *out;		/* A FILE because that is easier. */
	int	i;
	struct dllist dl;
	
#ifdef NOAOUT
	fprintf(stderr, "%s: has no function in OS/BSD\n", argv[0]);
	return(1);
#endif	

	if (argc != 3) {
		fprintf (stderr, "usage: %s infile outfile\n", argv[0]);
		return (1);
	}
	
	dl.ldfd = open (argv[1], O_RDONLY);
	if (dl.ldfd == -1) {
		perror (argv[1]);
		return (2);
	}
	
	GetFileInfo(dl.ldfd,
		    &dl.loadaddr,
		    &dl.xferaddr,
		    &dl.aout,
		    &dl.a_text,&dl.a_text_fill,
		    &dl.a_data,&dl.a_data_fill,
		    &dl.a_bss ,&dl.a_bss_fill, 0);

	if (dl.aout == -1) {
		fprintf(stderr,"%s: not an a.out file\n",argv[1]);
		return (3);
        }

	if (dl.aout != MID_VAX) {
		fprintf(stderr,"%s: file is not a VAX image (mid=%d)\n",
			argv[1],dl.aout);
		return (4);
	}

	i = dl.a_text + dl.a_text_fill + dl.a_data + dl.a_data_fill +
	    dl.a_bss  + dl.a_bss_fill;
	i = (i+1) / 512;

	dl.nloadaddr = dl.loadaddr;
	dl.lseek     = lseek(dl.ldfd,0L,SEEK_CUR);
	dl.a_lseek   = 0;
	dl.count     = 0;
	dl.dl_bsz    = 512;

	mopFilePutLX(header,IHD_W_SIZE,0xd4,2);   /* Offset to ISD section. */
	mopFilePutLX(header,IHD_W_ACTIVOFF,0x30,2);/* Offset to 1st section.*/
	mopFilePutLX(header,IHD_W_ALIAS,IHD_C_NATIVE,2);/* It's a VAX image.*/
	mopFilePutLX(header,IHD_B_HDRBLKCNT,1,1); /* Only one header block. */
	mopFilePutLX(header,0x30+IHA_L_TFRADR1,dl.xferaddr,4); /* Xfer Addr */
	mopFilePutLX(header,0xd4+ISD_W_PAGCNT,i,2);/* Imagesize in blks.*/
	
	out = fopen (argv[2], "w");
	if (!out) {
		perror (argv[2]);
		return (2);
	}
	
	/* Now we do the actual work. Write VAX MOP-image header */
	
	fwrite (header, sizeof (header), 1, out);

	fprintf (stderr, "copying %lu", dl.a_text);
	fprintf (stderr, "+%lu", dl.a_data);
	fprintf (stderr, "+%lu", dl.a_bss);
	fprintf (stderr, "->%lu", dl.xferaddr);
	fprintf (stderr, "\n");
	
	while ((i = mopFileRead(&dl,header)) > 0) {
		(void)fwrite(header, i, 1, out);
	}
	
	fclose (out);
	exit(0);
}
