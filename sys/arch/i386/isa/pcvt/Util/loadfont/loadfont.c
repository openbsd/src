/*	$OpenBSD: loadfont.c,v 1.4 1999/01/13 07:26:06 niklas Exp $	*/

/*
 * Copyright (c) 1992, 1995 Hellmuth Michaelis
 *
 * Copyright (c) 1992, 1994 Brian Dunford-Shore 
 *
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
 *	This product includes software developed by
 *	Hellmuth Michaelis and Brian Dunford-Shore
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

static char *id =
	"@(#)loadfont.c, 3.31, Last Edit-Date: [Thu Aug 24 10:40:50 1995]";

/*---------------------------------------------------------------------------*
 *
 *	load a font into vga character font memory
 *
 *	-hm	removing explicit HGC support (same as MDA ..)
 *	-hm	new pcvt_ioctl.h SIZ_xxROWS
 *	-hm	add -d option
 *	-hm	patch from Joerg, -s scanlines option
 *
 *---------------------------------------------------------------------------*/
 
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <machine/pcvt_ioctl.h>

struct screeninfo screeninfo;

#define DEFAULTFD 0
int fd;

static int roundrows(int scrrow);
static int codetosize(int code);
static void setfont(int charset, int fontloaded, int charscan,
		    int scrscan, int scrrow);
static void loadfont(int fontset, int charscanlines,
		     unsigned char *font_table);
static void printvgafontattr(int charset);
static void printheader(void);
static void usage(void);

int
main(int argc, char **argv)
{
	extern int optind;
	extern int opterr;
	extern char *optarg;

	FILE *in;
	struct stat sbuf, *sbp;
	unsigned char *fonttab;
	int ret;
	int chr_height;
	int scr_scan;
	int scr_rows;
	int scan_lines = -1;
	int c;
	int chr_set = -1;
	char *filename = NULL;
	int fflag = -1;
	int info = -1;
	int dflag = 0;
	char *device = NULL;
	
	while( (c = getopt(argc, argv, "c:d:f:is:")) != -1)
	{
		switch(c)
		{
			case 'c':
				chr_set = atoi(optarg);
				break;
				
			case 'd':
				device = optarg;
				dflag = 1;
				break;

			case 'f':
				filename = optarg;
				fflag = 1;
				break;
				
			case 'i':
				info = 1;
				break;
				
			case 's':
				scan_lines = atoi(optarg);
				if(scan_lines == 0)
					usage();
				break;
			
			case '?':
			default:
				usage();
				break;
		}
	}
	
	if(chr_set == -1 || fflag == -1)
		info = 1;

	if(dflag)
	{
		if((fd = open(device, O_RDWR)) == -1)
		{
			char buffer[80];
			strcpy(buffer,"ERROR opening ");
			strcat(buffer,device);
			perror(buffer);
			exit(1);
		}
	}
	else
	{
		fd = DEFAULTFD;
	}

	if(ioctl(fd, VGAGETSCREEN, &screeninfo) == -1)
	{
		perror("ioctl VGAGETSCREEN failed");
		exit(1);
	}

	if(info == 1)
	{
		int i;
	
		switch(screeninfo.adaptor_type)
		{
		  case UNKNOWN_ADAPTOR:
		  case MDA_ADAPTOR:
		  case CGA_ADAPTOR:
		    fprintf(stderr,
			    "Adaptor does not support Downloadable Fonts!\n");
		    break;
		  case EGA_ADAPTOR:
		    printheader();
		    for(i = 0;i < 4;i++)
		    {
			printvgafontattr(i);
		    }
		    break;
		  case VGA_ADAPTOR:
		    printheader();		  
		    for(i = 0;i < 8;i++)
		    {
			printvgafontattr(i);
		    }
		}
		printf("\n");
		exit(0);
	}

	switch(screeninfo.adaptor_type)
		{
		case UNKNOWN_ADAPTOR:
		case MDA_ADAPTOR:
		case CGA_ADAPTOR:
			fprintf(stderr,
				"Adaptor does not support "
				"Downloadable Fonts!\n");
			exit(1);

		case EGA_ADAPTOR:
			if(scan_lines == -1) scan_lines = 350;
			else if(scan_lines != 350) {
				fprintf(stderr,
					"EGA adaptors can only operate with "
					"350 scan lines.\n");
				exit(1);
			}
			break;

		case VGA_ADAPTOR:
			if(scan_lines == -1) scan_lines = 400;
			else if(scan_lines != 400 && scan_lines != 480) {
				fprintf(stderr,
					"VGA adaptors can only operate with "
					"400/480 scan lines.\n");
				exit(1);
			}
			break;
		}

	if(chr_set < 0 || chr_set > 7)
		usage();

	sbp = &sbuf;
	
	if((in = fopen(filename, "r")) == NULL)
	{
		char buffer[80];
		sprintf(buffer, "cannot open file %s for reading", filename);
		perror(buffer);
		exit(1);
	}

	if((fstat(fileno(in), sbp)) != 0)
	{
		char buffer[80];
		sprintf(buffer, "cannot fstat file %s", filename);
		perror(buffer);
		exit(1);
	}
		
	chr_height = sbp->st_size / 256; /* 256 chars per font */
			
	if(chr_height * 256 != sbp->st_size ||
	   chr_height < 8 || chr_height > 20) {
		fprintf(stderr,
			"File is no valid font file, size = %ld.\n",
			(long)sbp->st_size);
		exit(1);
	}			

	scr_rows = codetosize(roundrows(scan_lines / chr_height));
	scr_scan = scr_rows * chr_height - 256 - 1;

	if((fonttab = (unsigned char *)malloc((size_t)sbp->st_size)) == NULL)
	{
		fprintf(stderr,"error, malloc failed\n");
		exit(1);
	}

	if((ret = fread(fonttab, sizeof(*fonttab), sbp->st_size, in)) !=
	   sbp->st_size)
	{
		fprintf(stderr,
			"error reading file %s, size = %ld, read = %d, "
			"errno %d\n",
			argv[1], (long)sbp->st_size, ret, errno);
		exit(1);
	}		

	loadfont(chr_set, chr_height, fonttab);
	setfont(chr_set, 1, chr_height - 1, scr_scan, scr_rows);

	exit(0);
}

static int
roundrows(int scrrow)
{
	if(scrrow >= 50) return SIZ_50ROWS;
	else if(scrrow >= 43) return SIZ_43ROWS;
	else if(scrrow >= 40) return SIZ_40ROWS;
	else if(scrrow >= 35) return SIZ_35ROWS;
	else if(scrrow >= 28) return SIZ_28ROWS;
	else return SIZ_25ROWS;
}

static int
codetosize(int code)
{
	static int sizetab[] = { 25, 28, 35, 40, 43, 50 };
	if(code < 0 || code >= sizeof sizetab / sizeof(int))
		return -1;
	return sizetab[code];
}

static void
setfont(int charset, int fontloaded, int charscan, int scrscan, int scrrow)
{
	struct vgafontattr vfattr;

	vfattr.character_set = charset;
	vfattr.font_loaded = fontloaded;
	vfattr.character_scanlines = charscan;
	vfattr.screen_scanlines = scrscan;
	vfattr.screen_size = scrrow;

	if(ioctl(fd, VGASETFONTATTR, &vfattr) == -1)
	{
		perror("loadfont - ioctl VGASETFONTATTR failed, error");
		exit(1);
	}
}

static void
loadfont(int fontset, int charscanlines, unsigned char *font_table)
{
	int i, j;
	struct vgaloadchar vlc;

	vlc.character_set = fontset;
	vlc.character_scanlines = charscanlines;

	for(i = 0; i < 256; i++)
	{
		vlc.character = i;
		for (j = 0; j < charscanlines; j++)
		{
			vlc.char_table[j] = font_table[j];
		}
		font_table += charscanlines;
		if(ioctl(fd, VGALOADCHAR, &vlc) == -1)
		{
			perror("loadfont - ioctl VGALOADCHAR failed, error");
			exit(1);
		}
	}
}

static void
printvgafontattr(int charset)
{
	struct vgafontattr vfattr;
	
	vfattr.character_set = charset;

	if(ioctl(fd, VGAGETFONTATTR, &vfattr) == -1)
	{
		perror("loadfont - ioctl VGAGETFONTATTR failed, error");
		exit(1);
	}
	printf(" %d  ",charset);
	if(vfattr.font_loaded)
	{

		printf("Loaded ");
		printf(" %2.2d       ", codetosize(vfattr.screen_size));
		printf(" %2.2d           ",
		       (((int)vfattr.character_scanlines) & 0x1f) + 1);
		printf(" %3.3d",
		       ((int)vfattr.screen_scanlines+0x101));
	}
	else
	{
		printf("Empty");
	}
	printf("\n");
}

static void
printheader(void)
{
	printf("\nEGA/VGA Charactersets Status Info:\n\n");
	printf("Set Status Lines CharScanLines ScreenScanLines\n");
	printf("--- ------ ----- ------------- ---------------\n");
}

static void
usage(void)
{
	fprintf(stderr,
         "\nloadfont - "
         "load a font into EGA/VGA font ram for the pcvt video driver\n");
	fprintf(stderr,
         "usage: loadfont -c <charset> -d <device> -f <filename>"
         " -i -s <scan_lines>\n");
	fprintf(stderr,
         "       -c <charset>    characterset to load (EGA 0..3, VGA 0..7)\n");
	fprintf(stderr,
         "       -d <device>     specify device\n");
	fprintf(stderr,
         "       -f <filename>   filename containing binary font data\n");
	fprintf(stderr,
         "       -i              print status and types of loaded fonts\n");
	fprintf(stderr,
         "       -s <scan_lines> number of scan lines on screen\n");
	exit(2);
}
