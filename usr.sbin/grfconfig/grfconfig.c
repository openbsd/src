/*	$NetBSD: grfconfig.c,v 1.3 1996/02/11 16:34:23 neil Exp $	*/

/*
 * Copyright (c) 1995 Ezra Story
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
 *      This product includes software developed by Ezra Story.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <amiga/dev/grfioctl.h>

extern char *optarg;
extern int optind;

/*
 * Dynamic mode loader for NetBSD/Amiga grf devices.
 */
int
main(ac, av)
	int     ac;
	char  **av;
{
	int     c, y, grffd;
	char    rawdata = 0;
    	char    oldmode = 1;
	char   *grfdevice = 0;
	char   *modefile = 0;
	char    buf[102];
	char    ystr[20];
	FILE   *fp;
	struct grfvideo_mode gv[1];

	while ((c = getopt(ac, av, "ro")) != -1) {
		switch (c) {
		case 'r':	/* raw output */
			rawdata = 1;
			break;
    	    	case 'o':
    	    	    	oldmode = 8;
    	    	    	break;
		default:
			printf("grfconfig [-r] device [file]\n");
			return (1);
		}
	}
	ac -= optind;
	av += optind;


	if (ac >= 1)
		grfdevice = av[0];
	else {
		printf("grfconfig: No grf device specified.\n");
		return (1);
	}

	if (ac >= 2)
		modefile = av[1];

	if ((grffd = open(grfdevice, O_RDWR)) < 0) {
		printf("grfconfig: can't open grf device.\n");
		return (1);
	}
	/* If a mode file is specificied, load it in, don't display any info. */

	if (modefile) {
		if (!(fp = fopen(modefile, "r"))) {
			printf("grfconfig: Cannot open mode definition file.\n");
			return (1);
		}
		while (fgets(buf, 300, fp)) {
			if (buf[0] == '#')
				continue;

			/* num clk wid hi dep hbs hss hse hbe ht vbs vss vse
			 * vbe vt */

			c = sscanf(buf, "%9s %d %hd %hd %hd %hd %hd %hd "
			    "%hd %hd %hd %hd %hd %hd %hd",
			    ystr,
			    &gv->pixel_clock,
			    &gv->disp_width,
			    &gv->disp_height,
			    &gv->depth,
			    &gv->hblank_start,
			    &gv->hsync_start,
			    &gv->hsync_stop,
			    &gv->hblank_stop,
			    &gv->htotal,
			    &gv->vblank_start,
			    &gv->vsync_start,
			    &gv->vsync_stop,
			    &gv->vblank_stop,
			    &gv->vtotal);
			if (c == 15) {
				if (y = atoi(ystr))
					gv->mode_num = y;
				else
					if (ystr[0] == 'c') {
						gv->mode_num = 255;
						gv->depth = 4;
					}
				gv->mode_descr[0] = 0;
				if (ioctl(grffd, GRFIOCSETMON, (char *) gv) < 0)
					printf("grfconfig: bad monitor "
					    "definition.\n");
			} else {
				printf("grfconfig: bad line in mode "
				    "definition file.\n");
			}
		}
		fclose(fp);
	} else {
		ioctl(grffd, GRFGETNUMVM, &y);
		y += 2;
		for (c = 1; c < y; c++) {
			c = gv->mode_num = (c != (y - 1)) ? c : 255;
			if (ioctl(grffd, GRFGETVMODE, gv) < 0)
				continue;
			if (rawdata) {
				if (c == 255)
					printf("c ");
				else
					printf("%d ", c);
				printf("%d %d %d %d %d %d %d "
				    "%d %d %d %d %d %d %d\n",
				    gv->pixel_clock,
				    gv->disp_width,
				    gv->disp_height,
				    gv->depth,
				    gv->hblank_start,
				    gv->hsync_start,
				    gv->hsync_stop,
				    gv->hblank_stop,
				    gv->htotal,
				    gv->vblank_start,
				    gv->vsync_start,
				    gv->vsync_stop,
				    gv->vblank_stop,
				    gv->vtotal
				    );
				continue;
			}
			if (c == 255)
				printf("Console: ");
			else
				printf("%2d: ", gv->mode_num);

			printf("%dx%d",
			    gv->disp_width,
			    gv->disp_height);

			if (c != 255)
				printf("x%d", gv->depth);
			else
				printf(" (%dx%d)",
				    gv->disp_width / 8,
				    gv->disp_height / gv->depth);

			printf("\t%d.%dkHz @ %dHz %s\n",
			    gv->pixel_clock / (gv->htotal * 1000 * oldmode),
			    (gv->pixel_clock / (gv->htotal * 100 * oldmode)) 
    	    	    	    	% 10,
			    gv->pixel_clock / (gv->htotal * gv->vtotal * 
    	    	    	    	oldmode),
			    gv->vblank_start + 100 < gv->disp_height ?
			    "I" :
			    (gv->vblank_start - 100) > gv->disp_height ?
			    "SD" :
			    "NI"
			    );
		}
	}

	close(grffd);
	return (0);
}
